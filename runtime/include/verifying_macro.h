// Keeps as separated file because use in regression tests.
#pragma once
#include <cassert>
#include <iostream>
<<<<<<< HEAD:src/runtime/include/verifying_macro.h
=======
#include <memory>
#include <utility>
>>>>>>> 43c4393 (erase build logic from verify script):runtime/include/verifying_macro.h
#include <vector>

#include "lib.h"

namespace ltest {

extern std::vector<TaskBuilder> task_builders;

}  // namespace ltest

// Adds an attribute.
#define attr(attr) __attribute((__annotate__(#attr)))

// Tell that the function need to be converted to the coroutine.
#define non_atomic attr(ltest_nonatomic)

namespace ltest {

template <typename T>
std::string toString(const T &a);

template <typename tuple_t, size_t... index>
auto toStringListHelper(const tuple_t &t,
                        std::index_sequence<index...>) noexcept {
  return std::vector<std::string>{ltest::toString(std::get<index>(t))...};
}

template <typename tuple_t>
auto toStringList(const tuple_t &t) noexcept {
  typedef typename std::remove_reference<decltype(t)>::type tuple_type;
  constexpr auto s = std::tuple_size<tuple_type>::value;
  if constexpr (s == 0) {
    return std::vector<std::string>{};
  }
  return toStringListHelper<tuple_type>(t, std::make_index_sequence<s>{});
}

template <typename... Args>
auto toStringArgs(std::shared_ptr<void> args) {
  auto real_args = reinterpret_cast<std::tuple<Args...> *>(args.get());
  return toStringList(*real_args);
}

template <typename Ret, typename Target, typename... Args>
struct TargetMethod;

template <typename Target, typename... Args>
struct TargetMethod<int, Target, Args...> {
  using Method = std::function<int(Target *, Args...)>;
  TargetMethod(std::string_view method_name,
               std::function<std::tuple<Args...>(size_t)> gen, Method method) {
    auto builder =
        [gen = std::move(gen), method_name, method = std::move(method)](
            void *this_ptr, size_t thread_num) -> std::shared_ptr<CoroBase> {
      auto args = std::shared_ptr<void>(new std::tuple(gen(thread_num)));
      auto coro = Coro<Target, Args...>::New(
          method, this_ptr, args, &ltest::toStringArgs<Args...>, method_name);
      if (ltest::generators::generated_token) {
        coro->SetToken(ltest::generators::generated_token);
        ltest::generators::generated_token.reset();
      }
<<<<<<< HEAD:src/runtime/include/verifying_macro.h
      return coro;
=======
      return std::make_shared<TaskImplFromCoro>(coro);
>>>>>>> 43c4393 (erase build logic from verify script):runtime/include/verifying_macro.h
    };
    ltest::task_builders.push_back(builder);
  }
};

// Emulate that void f() returns 0.
template <typename Target, typename F, typename... Args>
struct Wrapper {
  F f;
  Wrapper(F f) : f(std::move(f)) {}
  int operator()(void *this_ptr, Args &&...args) {
    f(reinterpret_cast<Target *>(this_ptr), std::forward<Args>(args)...);
    return 0;
  }
};

<<<<<<< HEAD:src/runtime/include/verifying_macro.h
=======
// Checks does await_suspended returned false, if so calls the callback by hands
template <typename Target>
struct DualMethodWrapper {
  std::function<bool(Target *, std::coroutine_handle<>)> await_suspend;
  DualMethodWrapper(
      std::function<bool(Target *, std::coroutine_handle<>)> await_suspend)
      : await_suspend(std::move(await_suspend)) {}

  int operator()(void *this_ptr, std::coroutine_handle<> callback) {
    bool need_suspend =
        await_suspend(reinterpret_cast<Target *>(this_ptr), callback);
    if (!need_suspend) {
      callback();
    }
    return 0;
  }
};

struct CoroutineResponse {
  struct promise_type {
    CoroutineResponse get_return_object() {
      return {.h = std::coroutine_handle<promise_type>::from_promise(*this)};
    }
    std::suspend_never initial_suspend() { return {}; }
    std::suspend_never final_suspend() noexcept { return {}; }
    void unhandled_exception() {}
  };

  std::coroutine_handle<promise_type> h;
};

template <typename Awaitable>
CoroutineResponse StartCallback(
    std::shared_ptr<Awaitable> awaitable,
    std::shared_ptr<DualTaskImplFromCoro<Awaitable>> coro) {
  // TODO: Can I do it without co_awiat here?
  co_await std::suspend_always{};

  // Add FollowUpResonse event to the history
  coro->FinishTask(awaitable->await_resume());
}

template <typename Awaitable, typename Target, typename... Args>
struct TargetMethodDual {
  using Method = std::function<Awaitable(Target *, Args...)>;
  TargetMethodDual(
      std::string_view method_name,
      std::function<std::tuple<Args...>(size_t)> gen, Method method,
      std::function<bool(Awaitable *, std::coroutine_handle<>)> await_suspend) {
    auto dual_builder =
        [gen = std::move(gen), method_name, method = std::move(method),
         await_suspend = std::move(await_suspend)](
            void *this_ptr, size_t thread_num) -> std::variant<Task, DualTask> {
      // Create args for method that returns promise aka awaitable object
      auto args = std::shared_ptr<void>(new std::tuple(gen(thread_num)));
      auto typed_args = reinterpret_cast<std::tuple<Args...> *>(args.get());
      auto this_arg =
          std::tuple<Target *>{reinterpret_cast<Target *>(this_ptr)};

      // Call the method and get our awaitable object, now can call
      // await_suspend on the object
      std::shared_ptr<Awaitable> awaitable = std::make_shared<Awaitable>(
          std::apply(method, std::tuple_cat(this_arg, *typed_args)));

      // Create task for the await_suspend on the awaitable object
      std::shared_ptr<CoroBase> coro;
      // Create dual task from the task for the await_suspend
      std::shared_ptr<DualTaskImplFromCoro<Awaitable>> dual_coro =
          std::make_shared<DualTaskImplFromCoro<Awaitable>>(coro, awaitable);
      // Set callback that will set flag that follow-up is ready into dual task
      std::coroutine_handle<> callback =
          StartCallback<Awaitable>(awaitable, dual_coro).h;

      // Set task for the await_suspend on the awaitable object
      auto wrapper = DualMethodWrapper<Awaitable>{await_suspend};
      coro = Coro<Awaitable, std::coroutine_handle<>>::New(
          wrapper, awaitable.get(),
          std::shared_ptr<void>(new std::tuple(callback)), args,
          &ltest::toStringArgs<Args...>, method_name);
      // TODO: crutch, fix order and dependencies
      dual_coro->method = coro;

      if (ltest::generators::generated_token) {
        coro->SetToken(ltest::generators::generated_token);
        ltest::generators::generated_token.reset();
      }
      return dual_coro;
    };
    ltest::task_builders.push_back(
        TasksBuilder(std::string(method_name), dual_builder));
  }
};

>>>>>>> 43c4393 (erase build logic from verify script):runtime/include/verifying_macro.h
template <typename Target, typename... Args>
struct TargetMethod<void, Target, Args...> {
  using Method = std::function<void(Target *, Args...)>;

  TargetMethod(std::string_view method_name,
               std::function<std::tuple<Args...>(size_t)> gen, Method method) {
    auto builder =
        [gen = std::move(gen), method_name, method = std::move(method)](
            void *this_ptr, size_t thread_num) -> std::shared_ptr<CoroBase> {
      auto wrapper = Wrapper<Target, decltype(method), Args...>{method};
      auto args = std::shared_ptr<void>(new std::tuple(gen(thread_num)));
      auto coro = Coro<Target, Args...>::New(
          wrapper, this_ptr, args, &ltest::toStringArgs<Args...>, method_name);
      if (ltest::generators::generated_token) {
        coro->SetToken(ltest::generators::generated_token);
        ltest::generators::generated_token.reset();
      }
<<<<<<< HEAD:src/runtime/include/verifying_macro.h
      return coro;
=======
      return std::make_shared<TaskImplFromCoro>(coro);
>>>>>>> 43c4393 (erase build logic from verify script):runtime/include/verifying_macro.h
    };
    ltest::task_builders.push_back(builder);
  }
};

}  // namespace ltest

#define declare_task_name(symbol) const char *symbol##_task_name = #symbol

#define target_method(gen, ret, cls, symbol, ...)          \
  declare_task_name(symbol);                               \
  ltest::TargetMethod<ret, cls __VA_OPT__(, ) __VA_ARGS__> \
      symbol##_ltest_method_cls{symbol##_task_name, gen, &cls::symbol};
