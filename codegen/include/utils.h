#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/Cloning.h"

using namespace llvm;

namespace utils {

// Clone the function with new name and return type.
Function *CloneFuncChangeRetType(Function *f, Type *new_ret_type,
                                 const Twine &new_name) {
  std::vector<Type *> arg_types;
  for (const auto &I : f->args()) {
    arg_types.push_back(I.getType());
  }

  auto ftype = FunctionType::get(new_ret_type, arg_types,
                                 f->getFunctionType()->isVarArg());

  Function *newF = Function::Create(
      ftype, f->getLinkage(), f->getAddressSpace(), new_name, f->getParent());

  Function::arg_iterator DestI = newF->arg_begin();
  ValueToValueMapTy vmap{};
  for (const auto &I : f->args()) {
    DestI->setName(I.getName());
    vmap[&I] = &*DestI++;
  }

  SmallVector<ReturnInst *, 8> Returns;
  CloneFunctionInto(newF, f, vmap, CloneFunctionChangeType::LocalChangesOnly,
                    Returns, "", nullptr);
  return newF;
}

// Generate malloc callee.
// declare ptr @malloc(i32)
FunctionCallee GenMallocCallee(Module &M) {
  auto &ctx = M.getContext();
  auto ptr_type = PointerType::get(ctx, 0);
  auto int_type = Type::getInt32Ty(ctx);
  std::vector<Type *> args = {int_type};
  return M.getOrInsertFunction("malloc",
                               FunctionType::get(ptr_type, args, false));
}

// Generate free callee.
// declare void @free(ptr)
FunctionCallee GenFreeCallee(Module &M) {
  auto &ctx = M.getContext();
  auto ptr_type = PointerType::get(ctx, 0);
  auto void_type = Type::getVoidTy(ctx);
  std::vector<Type *> args = {ptr_type};
  return M.getOrInsertFunction("free",
                               FunctionType::get(void_type, args, false));
}

struct SeqGenerator {
  int num{};
  int next() { return num++; }
};

}  // namespace utils