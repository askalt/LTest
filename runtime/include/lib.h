#pragma once
#include <signal.h>
#include <valgrind/memcheck.h>

#include <cassert>
#include <coroutine>
#include <csetjmp>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#define panic() assert(false)

// Coroutine stack size.
const int STACK_SIZE = 1024 * 1024;

struct CoroBase;

// Current executing coroutine.
extern std::shared_ptr<CoroBase> this_coro;

// Current resumer context.
extern std::jmp_buf sched_ctx;

// Current starter context.
extern std::jmp_buf start_point;

void CoroBody(int signum);

// Runtime token.
// Target method could use token generator.
struct Token {
  // Parks the task. Yields.
  void Park();
  // Unpark the task parked by token.
  void Unpark();

 private:
  // Resets the token.
  void Reset();
  // If token is parked.
  bool parked{};

  friend class CoroBase;
};

<<<<<<< HEAD:src/runtime/include/lib.h
=======
// TaskAbstract represents task for non-dual methods
struct TaskAbstract {
  TaskAbstract(const TaskAbstract&) = delete;
  TaskAbstract(TaskAbstract&&) = delete;
  TaskAbstract& operator=(TaskAbstract&&) = delete;

  // Restart task with the new state
  virtual std::shared_ptr<TaskAbstract> Restart(void* this_ptr) = 0;

  // Resume the method to the next yield.
  virtual void Resume() = 0;

  // Check if the method is returned.
  virtual bool IsReturned() const = 0;

  // Returns return value of the method.
  virtual int GetRetVal() const = 0;

  // Returns the name of the method.
  virtual std::string GetName() const = 0;

  // Returns the args as strings.
  virtual std::vector<std::string> GetStrArgs() const = 0;

  // Returns raw pointer to the tuple arguments.
  virtual void* GetArgs() const = 0;

  // Returns whether this thread is waiting
  virtual bool IsSuspended() const = 0;

  // Continue execution of the task until the method
  // will be finished. This method is required to avoid
  // memory leaks
  virtual void Terminate() = 0;

  // Sets the token. Use token for blocking algorithms
  virtual void SetToken(std::shared_ptr<Token>) = 0;

  virtual ~TaskAbstract() {};

 protected:
  // Need this constructor for tests
  TaskAbstract() {};
};

// DualTaskAbstract represents task for dual methods
struct DualTaskAbstract {
  DualTaskAbstract(const DualTaskAbstract&) = delete;
  DualTaskAbstract(DualTaskAbstract&&) = delete;
  DualTaskAbstract& operator=(DualTaskAbstract&&) = delete;

  // Restart task with the new state
  virtual std::shared_ptr<DualTaskAbstract> Restart(void* this_ptr) = 0;

  // Resume the Request section of the dual method to the next yield.
  // IsRequestFinished have to be false
  virtual void ResumeRequest() = 0;

  // Returns whether the request section of dual method finished
  virtual bool IsRequestFinished() const = 0;

  // Provides an opportunity to set the callback. Callback will be called
  // when the followUp section will be finished. Can be used to add events
  // to a history
  virtual void SetFollowUpTerminateCallback(std::function<void()>) = 0;

  // Returns whether the follow up section of dual method finished
  virtual bool IsFollowUpFinished() const = 0;

  // Returns return value of the method.
  // IsFollowUpFinished have to be true
  virtual int GetRetVal() const = 0;

  // Returns the name of the method.
  virtual std::string GetName() const = 0;

  // Returns the args as strings.
  virtual std::vector<std::string> GetStrArgs() const = 0;

  // Returns raw pointer to the tuple arguments.
  virtual void* GetArgs() const = 0;

  // Continue execution of the task until the method
  // will be finished. This method is required to avoid
  // memory leaks
  virtual void Terminate() = 0;

  virtual ~DualTaskAbstract() {};

 protected:
  // Need this constructor for tests
  DualTaskAbstract() {};
};

using Task = std::shared_ptr<TaskAbstract>;
using DualTask = std::shared_ptr<DualTaskAbstract>;

// (this_ptr, thread_num) -> Task | DualTask
struct TasksBuilder {
  using BuilderFunc =
      std::function<std::variant<Task, DualTask>(void*, size_t)>;
  TasksBuilder(std::string name, BuilderFunc func)
      : name(name), builder_func(func) {}

  const std::string& GetName() const { return name; }

  std::variant<Task, DualTask> Build(void* this_ptr, size_t thread_id) {
    return builder_func(this_ptr, thread_id);
  }

 private:
  std::string name;
  BuilderFunc builder_func;
};

void Terminate(std::variant<Task, DualTask>);

>>>>>>> 43c4393 (erase build logic from verify script):runtime/include/lib.h
extern "C" void CoroYield();

struct CoroBase : public std::enable_shared_from_this<CoroBase> {
  CoroBase(const CoroBase&) = delete;
  CoroBase(CoroBase&&) = delete;
  CoroBase& operator=(CoroBase&&) = delete;

  // Restart the coroutine from the beginning passing this_ptr as this.
  // Returns restarted coroutine.
  virtual std::shared_ptr<CoroBase> Restart(void* this_ptr) = 0;

  // Resume the coroutine to the next yield.
  void Resume();

  // Check if the coroutine is returned.
  bool IsReturned() const;

  // Returns return value of the coroutine.
  int GetRetVal() const;

  // Returns the name of the coroutine.
  std::string_view GetName() const;

  // Returns the args as strings.
  virtual std::vector<std::string> GetStrArgs() const = 0;

  // Returns raw pointer to the tuple arguments.
  virtual void* GetArgs() const = 0;

  // Returns new pointer to the coroutine.
  // https://en.cppreference.com/w/cpp/memory/enable_shared_from_this
  std::shared_ptr<CoroBase> GetPtr();

  // Terminate the coroutine.
  void Terminate();

  // Sets the token.
  void SetToken(std::shared_ptr<Token>);

  // Checks if the coroutine is parked.
  bool IsParked() const;

  virtual ~CoroBase();

 protected:
  CoroBase() = default;

  virtual int Run() = 0;

  friend void CoroBody(int);
  friend void ::CoroYield();

  template <typename Target, typename... Args>
  friend class Coro;

  // Return value.
  int ret{};
  // Is coroutine returned.
  bool is_returned{};
  // Stack.
  std::unique_ptr<char[]> stack{};
  // Last remembered context.
  std::jmp_buf ctx{};
  // Valgrind stack id.
  unsigned val_stack_id;
  // Name.
  std::string_view name;
  // Token.
  std::shared_ptr<Token> token{};
};

template <typename Target, typename... Args>
struct Coro final : public CoroBase {
  // CoroF is a target class method.
  using CoroF = std::function<int(Target*, Args...)>;
  // ArgsToStringF converts arguments to the strings for pretty printing.
  using ArgsToStringsF =
      std::function<std::vector<std::string>(std::shared_ptr<void>)>;

  // unsafe: caller must ensure that this_ptr points to Target.
  std::shared_ptr<CoroBase> Restart(void* this_ptr) override {
    /**
     *  The task must be returned if we want to restart it.
     *   We can't just Terminate() it because it is the runtime responsibility
     * to decide, in which order the tasks should be terminated.
     *
     */
    assert(IsReturned());
    auto coro = New(func, this_ptr, args, args_to_strings, name);
    if (token != nullptr) {
      coro->token = std::move(token);
    }
    return coro;
  }

  // unsafe: caller must ensure that this_ptr points to Target.
  static std::shared_ptr<CoroBase> New(CoroF func, void* this_ptr,
                                       std::shared_ptr<void> args,
                                       ArgsToStringsF args_to_strings,
                                       std::string_view name) {
    auto c = std::make_shared<Coro>();
    c->func = std::move(func);
    c->args = std::move(args);
    c->name = name;
    c->args_to_strings = std::move(args_to_strings);
    c->this_ptr = this_ptr;
    c->stack = std::unique_ptr<char[]>(new char[STACK_SIZE]);
    c->val_stack_id =
        VALGRIND_STACK_REGISTER(c->stack.get(), c->stack.get() + STACK_SIZE);
    sigset_t news, olds, suss;
    sigemptyset(&news);
    sigaddset(&news, SIGUSR2);
    if (sigprocmask(SIG_BLOCK, &news, &olds) != 0) {
      panic();
    }

    /**
     * New handler should jump onto a new stack and remember
     * that position. Afterwards the stack is disabled and
     * becomes dedicated to that single coroutine.
     */
    struct sigaction newsa, oldsa;
    newsa.sa_handler = CoroBody;
    newsa.sa_flags = SA_ONSTACK;
    sigemptyset(&newsa.sa_mask);
    if (sigaction(SIGUSR2, &newsa, &oldsa) != 0) {
      panic();
    }

    stack_t oldst, newst;
    newst.ss_sp = c->stack.get();
    newst.ss_size = STACK_SIZE;
    newst.ss_flags = 0;
    if (sigaltstack(&newst, &oldst) != 0) {
      panic();
    }
    /* Jump onto the stack and remember its position. */
    auto old_this = this_coro;
    this_coro = c->GetPtr();
    sigemptyset(&suss);
    if (sigsetjmp(start_point, 1) == 0) {
      raise(SIGUSR2);
      while (this_coro != nullptr) {
        sigsuspend(&suss);
      }
    }
    this_coro = old_this;

    /**
     * Return the old stack, unblock SIGUSR2. In other words,
     * rollback all global changes. The newly created stack
     * now is remembered only by the new coroutine, and can be
     * used by it only.
     */
    if (sigaltstack(NULL, &newst) != 0) {
      panic();
    }
    newst.ss_flags = SS_DISABLE;
    if (sigaltstack(&newst, NULL) != 0) {
      panic();
    }
    if ((oldst.ss_flags & SS_DISABLE) == 0 && sigaltstack(&oldst, NULL) != 0) {
      panic();
    }
    if (sigaction(SIGUSR2, &oldsa, NULL) != 0) {
      panic();
    }
    if (sigprocmask(SIG_SETMASK, &olds, NULL) != 0) {
      panic();
    }
    return c;
  }

  int Run() override {
    auto real_args = reinterpret_cast<std::tuple<Args...>*>(args.get());
    auto this_arg = std::tuple<Target*>{reinterpret_cast<Target*>(this_ptr)};
    return std::apply(func, std::tuple_cat(this_arg, *real_args));
  }

  std::vector<std::string> GetStrArgs() const override {
    assert(args_to_strings != nullptr);
    return args_to_strings(args);
  }

  void* GetArgs() const override { return args.get(); }

  ~Coro() { VALGRIND_STACK_DEREGISTER(val_stack_id); }

 private:
  // Function to execute.
  CoroF func;
  // Pointer to the arguments, points to the std::tuple<Args...>.
  std::shared_ptr<void> args;
  // Function that can make strings from args for pretty printing.
  std::function<std::vector<std::string>(std::shared_ptr<void>)>
      args_to_strings;
  // Raw pointer to the target class object.
  void* this_ptr;
};

<<<<<<< HEAD:src/runtime/include/lib.h
// (this_ptr, thread_num) -> Task
using TaskBuilder = std::function<std::shared_ptr<CoroBase>(void*, size_t)>;
=======
// Awaitable is a type of an awaitable object whose await_suspende method
// is presented in this DualTaskImplFromCoro
// DualTaskImplFromCoro owns the awaitable object, so the type is required
// for safe delete
template <class Awaitable>
struct DualTaskImplFromCoro : DualTaskAbstract {
  DualTaskImplFromCoro(const DualTaskImplFromCoro& other) = default;

  DualTaskImplFromCoro(std::shared_ptr<CoroBase> method,
                       std::shared_ptr<Awaitable> awaitable_object)
      : method(method),
        callback([]() {}),
        is_follow_up_finished(false),
        return_value({}),
        awaitable_object(awaitable_object) {}

  // Не будет работать, перезапустится только функция для промиса, но промис уже
  // сломан
  virtual std::shared_ptr<DualTaskAbstract> Restart(void* this_ptr) override {
    return std::make_shared<DualTaskImplFromCoro>(
        method->Restart(this_ptr),
        std::shared_ptr<Awaitable>{(Awaitable*)this_ptr});
  }

  virtual void ResumeRequest() override {
    assert(!method->IsReturned());
    method->Resume();
  }

  virtual bool IsRequestFinished() const override {
    return method->IsReturned();
  }

  virtual void SetFollowUpTerminateCallback(std::function<void()> c) override {
    callback = c;
  };

  virtual bool IsFollowUpFinished() const override {
    return is_follow_up_finished;
  }

  virtual int GetRetVal() const override {
    assert(return_value.has_value());
    return return_value.value();
  }

  virtual std::string GetName() const override { return method->GetName(); }

  virtual std::vector<std::string> GetStrArgs() const override {
    return method->GetStrArgs();
  }

  virtual void* GetArgs() const override { return method->GetArgs(); }

  virtual void Terminate() override { method->Terminate(); }

  void FinishTask(int result) {
    callback();
    is_follow_up_finished = true;
    return_value = result;
  }

  // TODO: it's a crutch, should be private
  //  This field represents await_suspend method with yields
  std::shared_ptr<CoroBase> method;

 private:
  // Callback that will be called when the follow-up section for this task
  // will be finished
  std::function<void()> callback;
  bool is_follow_up_finished;
  std::optional<int> return_value;
  std::shared_ptr<Awaitable> awaitable_object;
};

struct TaskImplFromCoro : TaskAbstract {
  TaskImplFromCoro(std::shared_ptr<CoroBase> method) : method(method) {}

  virtual std::shared_ptr<TaskAbstract> Restart(void* this_ptr) override {
    return std::make_shared<TaskImplFromCoro>(method->Restart(this_ptr));
  };

  virtual void Resume() override { method->Resume(); }

  virtual bool IsReturned() const override { return method->IsReturned(); }

  virtual int GetRetVal() const override {
    assert(method->IsReturned());
    return method->GetRetVal();
  }

  virtual std::string GetName() const override { return method->GetName(); }

  virtual std::vector<std::string> GetStrArgs() const override {
    return method->GetStrArgs();
  }

  virtual void* GetArgs() const override { return method->GetArgs(); }

  virtual bool IsSuspended() const override { return method->IsParked(); }

  virtual void Terminate() override { method->Terminate(); }

  virtual void SetToken(std::shared_ptr<Token> token) override {
    method->SetToken(token);
  }

 private:
  std::shared_ptr<CoroBase> method;
};
>>>>>>> 43c4393 (erase build logic from verify script):runtime/include/lib.h
