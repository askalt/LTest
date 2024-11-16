#include <set>
#include <utility>
#include <vector>

#include "llvm/Demangle/Demangle.h"
#include "llvm/IR/Mangler.h"
#include "llvm/IR/NoFolder.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Pass.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Transforms/Coroutines/CoroCleanup.h"
#include "llvm/Transforms/Coroutines/CoroEarly.h"
#include "llvm/Transforms/Coroutines/CoroElide.h"
#include "llvm/Transforms/Coroutines/CoroSplit.h"
#include "utils.h"

using namespace llvm;

using Builder = IRBuilder<NoFolder>;
using FunIndex = std::set<std::pair<StringRef, StringRef>>;

// Attributes.
const StringRef nonatomic_attr = "ltest_nonatomic";
const StringRef nonatomic_manual_attr = "ltest_nonatomic_manual";
const StringRef gen_attr = "ltest_gen";
const StringRef target_attr_prefix = "ltesttarget_";

const StringRef coro_suf = "_coro";
const StringRef task_builder_suf = "_task_builder";

template <typename T>
void Assert(bool cond, const T &obj) {
  if (!cond) {
    errs() << "Assertion failed: \n";
    errs() << obj << "\n";
    assert(cond);
  }
}

void Assert(bool cond) { assert(cond); }

Value *GenerateCall(Builder *builder, Function *fun) {
  if (fun->arg_size() == 0) {
    return builder->CreateCall(fun, {});
  }
  Assert(fun->arg_size() == 1, fun);
  auto arg = fun->arg_begin();
  Assert(arg->hasStructRetAttr(), fun);
  auto alloca = builder->CreateAlloca(arg->getType(), nullptr);
  return builder->CreateCall(fun, {alloca});
}

Twine ToCoro(const StringRef name,
             const std::map<StringRef, StringRef> &unmangled_name) {
  if (auto it = unmangled_name.find(name); it != unmangled_name.end()) {
    // For root tasks (with ltesttarget attr) we generate coroutines with
    // unmangled names, because we need to call this tasks from cpp code. See
    // runtime/verifying.h for the details.
    return it->second + coro_suf;
  }
  return name + coro_suf;
}

FunIndex CreateFunIndex(const Module &M) {
  FunIndex index{};
  for (auto it = M.global_begin(); it != M.global_end(); ++it) {
    if (it->getName() != "llvm.global.annotations") {
      continue;
    }
    auto *CA = dyn_cast<ConstantArray>(it->getOperand(0));
    for (auto o_it = CA->op_begin(); o_it != CA->op_end(); ++o_it) {
      auto *CS = dyn_cast<ConstantStruct>(o_it->get());
      auto *fun = dyn_cast<Function>(CS->getOperand(0));
      auto *AnnotationGL = dyn_cast<GlobalVariable>(CS->getOperand(1));
      auto annotation =
          dyn_cast<ConstantDataArray>(AnnotationGL->getInitializer())
              ->getAsCString();
      index.insert({annotation, fun->getName()});
    }
  }
  return index;
}

bool HasAttribute(const FunIndex &index, const StringRef name,
                  const StringRef attr) {
  return index.find({attr, name}) != index.end();
}

// Generates coro clones for functions in the module.
struct CoroGenerator final {
  CoroGenerator(Module &M) : M(M) {
    auto &ctx = M.getContext();
    i1_t = Type::getInt1Ty(ctx);
    i8_t = Type::getInt8Ty(ctx);
    i32_t = Type::getInt32Ty(ctx);
    ptr_t = PointerType::get(ctx, 0);
    void_t = Type::getVoidTy(ctx);
    token_t = Type::getTokenTy(ctx);
    // This signature must be as in runtime declaration.
    // TODO: validate this by some way.
    promise_t = StructType::create("CoroPromise", i32_t, i32_t, ptr_t);
    promise_ptr_t = PointerType::get(promise_t, 0);

    // Names clashes?
    set_child_hdl = Function::Create(
        FunctionType::get(void_t, {promise_ptr_t, ptr_t}, false),
        Function::ExternalLinkage, "set_child_hdl", M);

    set_ret_val = Function::Create(
        FunctionType::get(void_t, {promise_ptr_t, i32_t}, false),
        Function::ExternalLinkage, "set_ret_val", M);

    get_ret_val =
        Function::Create(FunctionType::get(i32_t, {promise_ptr_t}, false),
                         Function::ExternalLinkage, "get_ret_val", M);
    init_promise =
        Function::Create(FunctionType::get(void_t, {ptr_t}, false),
                         Function::ExternalLinkage, "init_promise", M);
    get_promise =
        Function::Create(FunctionType::get(promise_ptr_t, {ptr_t}, false),
                         Function::ExternalLinkage, "get_promise", M);
  }

  void Run(const FunIndex &index) {
    std::map<StringRef, StringRef> unmangled_name;
    for (const auto &[attr, symbol] : index) {
      if (attr.starts_with(target_attr_prefix)) {
        auto real_name = attr.slice(target_attr_prefix.size(), attr.size());
        unmangled_name[symbol] = real_name;
      }
    }

    for (auto &F : M) {
      if (IsCoroTarget(F.getName(), index)) {
        // Generate coroutine.
        GenCoroFunc(&F, index, unmangled_name);
      }
    }
  }

 private:
  Module &M;
  Type *void_t;
  Type *token_t;

  StructType *promise_t;

  IntegerType *i1_t;
  IntegerType *i8_t;
  IntegerType *i32_t;

  PointerType *ptr_t;
  PointerType *promise_ptr_t;

  Function *set_child_hdl;
  Function *set_ret_val;
  Function *get_ret_val;
  Function *get_promise;
  Function *init_promise;

  bool IsCoroTarget(const StringRef fun_name, const FunIndex &index) {
    return HasAttribute(index, fun_name, nonatomic_attr) ||
           HasAttribute(index, fun_name, nonatomic_manual_attr);
  }

  bool NeedInterrupt(Instruction *insn, bool only_manual_suspends,
                     const FunIndex &index) {
    bool is_manual_suspend = false;
    bool is_required_suspend_call = false;

    if (auto call = dyn_cast<CallInst>(insn)) {
      auto called = call->getCalledFunction();
      if (called && called->hasName()) {
        auto called_name = called->getName();
        is_manual_suspend |= called_name == "CoroYield";
        is_manual_suspend |=
            HasAttribute(index, called_name, nonatomic_manual_attr);

        is_required_suspend_call = IsCoroTarget(called_name, index);
      }
    }
    if (only_manual_suspends) {
      return is_manual_suspend;
    }

    if (is_manual_suspend || isa<LoadInst>(insn) || isa<StoreInst>(insn) ||
        is_required_suspend_call || isa<AtomicRMWInst>(insn) /*||
        isa<InvokeInst>(insn)*/) {
      return true;
    }
    return false;
  }

  Function *GenCoroFunc(Function *F, const FunIndex &index,
                        const std::map<StringRef, StringRef> &unmangled_name) {
    auto name = F->getName();
    if (F->empty()) {
      errs() << "Skip generation for " << name << ": it's empty\n";
      return nullptr;
    }

    auto coro_name = ToCoro(name, unmangled_name);
    auto func = M.getFunction(coro_name.str());
    if (func != nullptr && !func->empty()) {
      // Was generated later or defined in user code.
      errs() << coro_name << " is generated already" << "\n";
      return func;
    }
    if (func == nullptr) {
      // Not target method. Just a regular method that could be non atomic too.
      func = utils::CloneFuncChangeRetType(F, ptr_t, coro_name);
    } else {
      errs() << "See target method: " << func->getName() << "\n";
      // It's target method as it's coro signature is declared in module
      // already. So clone the body from non coroutine.
      Function::arg_iterator DestI = func->arg_begin();
      ValueToValueMapTy vmap{};
      for (const auto &I : F->args()) {
        DestI->setName(I.getName());
        vmap[&I] = &*DestI++;
      }
      SmallVector<ReturnInst *, 8> Returns;
      CloneFunctionInto(func, F, vmap,
                        CloneFunctionChangeType::LocalChangesOnly, Returns, "",
                        nullptr);
    }
    errs() << "Gen " << coro_name << " for " << name << "\n";
    bool only_manual_suspends =
        HasAttribute(index, name, nonatomic_manual_attr);
    errs() << "Only manual suspends = " << only_manual_suspends << "\n";
    auto old_ret_t = F->getReturnType();
    assert(func != nullptr);
    auto generated = RawGenCoroFunc(func, old_ret_t, index, unmangled_name,
                                    only_manual_suspends);

    // errs() << "GENERATED:\n";
    // errs() << *generated << "\n";
    // errs() << "==========================================\n\n";

    // errs() << "INITIAL METHOD:\n";
    // errs() << *F << "\n";
    // errs() << "==========================================\n\n";

    return generated;
  }

  // TODO: rewrite it using one pass.
  Function *RawGenCoroFunc(Function *F, Type *old_ret_t,
                           const FunIndex &index,
                           const std::map<StringRef, StringRef> &umangled_name,
                           bool only_manual_suspends) {
    auto int_gen = utils::SeqGenerator{};
    auto &ctx = M.getContext();

    auto i32_0 = ConstantInt::get(i32_t, 0);
    auto ptr_null = ConstantPointerNull::get(ptr_t);
    auto token_none = ConstantTokenNone::get(ctx);
    auto i1_false = ConstantInt::get(i1_t, false);

    assert(ptr_t == F->getReturnType() && "F must have ptr return type");
    F->setPresplitCoroutine();

    Builder Builder(&*F->begin());

#ifdef REPLACE_CORO_INVOKES
    // Generate fictive normal_dest blocks for invoke instructions.
    // Because, we must suspend function after invoke.
    for (auto b_it = (*F).begin(); b_it != (*F).end(); ++b_it) {
      auto terminator = b_it->getTerminator();
      if (auto invoke = dyn_cast<InvokeInst>(terminator)) {
        auto normal_dest_new =
            BasicBlock::Create(ctx, "coro_normal_dest", F, &*std::next(b_it));
        Builder.SetInsertPoint(normal_dest_new);
        Builder.CreateBr(invoke->getNormalDest());
        invoke->setNormalDest(normal_dest_new);
      }
    }
#endif

    // * Add suspension points after suitable operations.
    for (auto b_it = (*F).begin(); b_it != (*F).end();) {
      auto cb = b_it;
      if (cb->getName().starts_with("coro_normal_dest")) {
        ++b_it;
        continue;
      }

      std::optional<BasicBlock *> new_entry_block;

      auto current_block = BasicBlock::Create(
          ctx, "execution." + std::to_string(int_gen.next()));
      for (auto insn_it = cb->begin(); insn_it != cb->end();) {
        if (std::next(insn_it) == cb->end()) {
          // Leave terminate instruction in this block.
          break;
        }
        auto start = insn_it;
        while (std::next(insn_it) != cb->end() &&
               !NeedInterrupt(&*insn_it, only_manual_suspends, index)) {
          ++insn_it;
        }
        if (std::next(insn_it) == cb->end()) {
          // This is the last segment, leave them in this block.
          break;
        }
        auto new_block = BasicBlock::Create(
            ctx, "execution." + std::to_string(int_gen.next()), F, &*b_it);
        if (!new_entry_block.has_value()) {
          new_entry_block = new_block;
        }
        b_it = cb->getIterator();
        auto iter = insn_it++;

        // Move instructions segment to the new block.
        while (true) {
          std::optional<decltype(iter)> prev;
          if (iter != start) {
            prev = std::prev(iter);
          }
          iter->removeFromParent();
          iter->insertInto(new_block, new_block->begin());
          if (!prev.has_value()) break;
          iter = prev.value();
        }
        Builder.SetInsertPoint(new_block);
        Builder.CreateIntrinsic(i8_t, Intrinsic::coro_suspend,
                                {token_none, i1_false});
      }
      b_it++;
      cb->setName("terminator." + std::to_string(int_gen.next()));
      if (new_entry_block.has_value()) {
        // Replace all uses of the terminate block with new entry block.
        cb->replaceAllUsesWith(new_entry_block.value());
        // But, phi nodes still must refer to terminate block,
        // because they are real successors.
        cb->replaceSuccessorsPhiUsesWith(new_entry_block.value(), &*cb);
      }
      // TODO: remove after DEBUG
      // int_gen.num += 100;
    }

    auto &first_real_block = F->front();
    auto suspend = BasicBlock::Create(ctx, "suspend", F, &F->front());
    auto cleanup = BasicBlock::Create(ctx, "cleanup", F, suspend);
    auto init = BasicBlock::Create(ctx, "init", F, cleanup);

    // Init:
    Builder.SetInsertPoint(init);
    auto promise = Builder.CreateAlloca(promise_t, nullptr, "promise");

    Builder.CreateCall(init_promise, {promise});
    auto id = Builder.CreateIntrinsic(token_t, Intrinsic::coro_id,
                                      {i32_0, promise, ptr_null, ptr_null},
                                      nullptr, "id");
    auto size = Builder.CreateIntrinsic(i32_t, Intrinsic::coro_size, {},
                                        nullptr, "size");
    auto alloc =
        Builder.CreateCall(utils::GenMallocCallee(M), {size}, "hdl_alloc");
    auto hdl = Builder.CreateIntrinsic(ptr_t, Intrinsic::coro_begin,
                                       {id, alloc}, nullptr, "hdl");
    auto suspend_0 = Builder.CreateIntrinsic(i8_t, Intrinsic::coro_suspend,
                                             {token_none, i1_false});
    auto switch_instr = Builder.CreateSwitch(suspend_0, suspend, 2);
    switch_instr->addCase(ConstantInt::get(i8_t, 0), &first_real_block);
    switch_instr->addCase(ConstantInt::get(i8_t, 1), cleanup);

    // Cleanup:
    Builder.SetInsertPoint(cleanup);
    auto mem = Builder.CreateIntrinsic(ptr_t, Intrinsic::coro_free, {id, hdl});
    Builder.CreateCall(utils::GenFreeCallee(M), {mem});
    Builder.CreateBr(suspend);

    // Suspend:
    Builder.SetInsertPoint(suspend);
    auto unused = Builder.CreateIntrinsic(i1_t, Intrinsic::coro_end,
                                          {hdl, i1_false, token_none});
    Builder.CreateRet(hdl);

    // * Replace ret instructions with result memorization.
    // We memorize return value in promise.
    // * Create terminate instructions in execution blocks.
    // * Replace function calls with coroutine clone calls.
    for (auto &b : *F) {
      auto b_name = b.getName();

      if (!b_name.starts_with("execution") && !b_name.starts_with("init") &&
          !b_name.starts_with("cleanup") && !b_name.starts_with("suspend") &&
          !b_name.starts_with("coro_normal_dest")) {
        auto instr = &*b.rbegin();
        // Check if terminate instruction is `ret`.
        if (auto ret = dyn_cast<ReturnInst>(instr)) {
          Builder.SetInsertPoint(ret);
          if (old_ret_t == i32_t) {
            Builder.CreateCall(set_ret_val, {promise, ret->getOperand(0)});
          } else {
            // If function returns not int we set i32 as ret value.
            // TODO: Take hash if we can.
            Builder.CreateCall(set_ret_val, {promise, i32_0});
          }
          // We need to suspend after `ret` because
          // return value must be accessed before coroutine destruction.
          auto suspend_res = Builder.CreateIntrinsic(
              i8_t, Intrinsic::coro_suspend, {token_none, i1_false});
          auto _switch = Builder.CreateSwitch(suspend_res, suspend, 2);
          _switch->addCase(ConstantInt::get(i8_t, 0), cleanup);
          _switch->addCase(ConstantInt::get(i8_t, 1), cleanup);
          instr->eraseFromParent();
        }

#ifdef REPLACE_CORO_INVOKES
        // Check if terminate instruction is `invoke`.
        // We must replace raw invokes with coroutine invokes.
        if (auto invoke = dyn_cast<InvokeInst>(instr)) {
          auto f = invoke->getCalledFunction();
          if (!f || !f->hasName()) {
            continue;
          }
          auto f_name = f->getName();
          if (IsCoroTarget(f_name, index)) {
            auto coro_f = GenCoroFunc(f, index, umangled_name);
            if (coro_f != nullptr) {
              errs() << "Replace " << f_name << " invoke\n";

              //   ...
              //    %hdl = invoke ptr call_coro() to normal_dest unwind ...
              Builder.SetInsertPoint(invoke);
              std::vector<Value *> args(invoke->arg_size());
              for (int i = 0; i < invoke->arg_size(); ++i) {
                args[i] = invoke->getArgOperand(i);
              }

              // Generated by the our pass normal dest.
              auto generated_normal_dest = invoke->getNormalDest();
              auto hdl = Builder.CreateInvoke(coro_f, generated_normal_dest,
                                              invoke->getUnwindDest(), args);
              // generated_normal_dest:
              //   call void set_child_hdl(...)
              //   %suspend = @llvm.coro.suspend()
              //   switch
              //
              auto br =
                  dyn_cast<BranchInst>(generated_normal_dest->getTerminator());
              auto normal_dest = br->getSuccessor(0);
              Builder.SetInsertPoint(&*generated_normal_dest->begin());
              Builder.CreateCall(set_child_hdl, {promise, hdl});

              // normal_dest:
              //   %as_promise = call Promise* @get_promise(hdl)
              //   %ret_val    = call i32 @get_ret(as_promise)
              //   operate with %ret_val
              //   ...
              Builder.SetInsertPoint(&*normal_dest->begin());
              auto as_promise = Builder.CreateCall(get_promise, {hdl});
              auto ret_val = Builder.CreateCall(get_ret_val, {as_promise});
              for (auto &use : invoke->uses()) {
                User *user = use.getUser();
                user->setOperand(use.getOperandNo(), ret_val);
              }
              invoke->eraseFromParent();
            }
          }
        }
#endif
      }

#ifdef REPLACE_CORO_INVOKES
      if (b_name.starts_with("coro_normal_dest")) {
        // Replace br with suspension.
        auto term = b.getTerminator();
        auto br = dyn_cast<BranchInst>(term);
        Builder.SetInsertPoint(&b);
        auto suspend_res = Builder.CreateIntrinsic(
            i8_t, Intrinsic::coro_suspend, {token_none, i1_false});
        auto _switch = Builder.CreateSwitch(suspend_res, suspend, 2);
        _switch->addCase(ConstantInt::get(i8_t, 0), br->getSuccessor(0));
        _switch->addCase(ConstantInt::get(i8_t, 1), cleanup);
        br->eraseFromParent();
      }
#endif

      if (!b_name.starts_with("execution")) {
        continue;
      }

      auto suspend_res = &*b.rbegin();
      Builder.SetInsertPoint(&b);
      auto _switch = Builder.CreateSwitch(suspend_res, suspend, 2);

      auto next_block = &*std::next(b.getIterator());
      _switch->addCase(ConstantInt::get(i8_t, 0), next_block);
      _switch->addCase(ConstantInt::get(i8_t, 1), cleanup);

      auto insn = &*std::prev(suspend_res->getIterator());

      // Suspension point is generated after each call, so we
      // can think that call is last instruction in itself execution block.
      if (auto call = dyn_cast<CallInst>(insn)) {
        // Replace calls with coroutines clone calls.
        // Maybe need to generate coro clones in process.
        auto f = call->getCalledFunction();
        if (!f || !f->hasName()) {
          continue;
        }
        auto f_name = f->getName();
        if (IsCoroTarget(f_name, index)) {
          auto coro_f = GenCoroFunc(f, index, umangled_name);
          if (coro_f == nullptr) {
            // We can't generate this call because
            // can't generate coro clone for callee.
            continue;
          }
          // TODO: remove after debug.
          errs() << "Replace " << f_name << " call\n";
          Assert(NeedInterrupt(insn, only_manual_suspends, index));

          Builder.SetInsertPoint(call);
          std::vector<Value *> args(call->arg_size());
          for (int i = 0; i < call->arg_size(); ++i) {
            args[i] = call->getArgOperand(i);
          }
          auto hdl = Builder.CreateCall(coro_f, args);

          Builder.SetInsertPoint(call);
          Builder.CreateCall(set_child_hdl, {promise, hdl});
          // %hdl = ptr call @call_coro()
          // call void set_child_hdl(...)
          // %suspend = @llvm.coro.suspend()
          // switch
          //
          // In the next block:
          // %as_promise = call Promise* @get_promise(hdl)
          // %ret_val    = call i32 @get_ret(as_promise)
          // operate with %ret_val

          Builder.SetInsertPoint(&*next_block->begin());
          auto as_promise = Builder.CreateCall(get_promise, {hdl});
          auto ret_val = Builder.CreateCall(get_ret_val, {as_promise});
          for (auto &use : call->uses()) {
            User *user = use.getUser();
            user->setOperand(use.getOperandNo(), ret_val);
          }
          call->eraseFromParent();
        }
      }
    }

    return F;
  }
};

namespace {

struct CoroGenPass : public PassInfoMixin<CoroGenPass> {
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM) {
    errs() << M << "\n";
    auto fun_index = CreateFunIndex(M);

    CoroGenerator gen{M};
    gen.Run(fun_index);

    return PreservedAnalyses::none();
  };
};

}  // namespace

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return {.APIVersion = LLVM_PLUGIN_API_VERSION,
          .PluginName = "coro_gen",
          .PluginVersion = "v0.1",
          .RegisterPassBuilderCallbacks = [](PassBuilder &PB) {
            PB.registerPipelineStartEPCallback(
                [](ModulePassManager &MPM, OptimizationLevel Level) {
                  MPM.addPass(CoroGenPass());
                });
          }};
}
