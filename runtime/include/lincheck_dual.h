#pragma once

#include <functional>
#include <iostream>
#include <numeric>

#include "lincheck.h"

// TODO: these code should be integrated with the lincheck checker, but
// unfortunately it's not obvious how to cache results of blocking calls(see
// docs for details), so now it's a separate struct
struct BlockingMethod {
  BlockingMethod() = default;
  // It's not possible to copy a coroutine state
  BlockingMethod(const BlockingMethod& o) = delete;
  BlockingMethod& operator=(const BlockingMethod& o) = delete;

  // Starts a new coroutine and the blocking method inside
  virtual void StartRequest() = 0;
  // Returns whether followup finished successfully
  virtual bool IsFinished() = 0;
  // Returns value that was returned by the blocking method
  virtual int GetResult() = 0;
  virtual ~BlockingMethod() = default;
};

// PromiseType have to be awaitable object
// BlockingMethodWrapper is a wrapper that wraps a coroutine promise(aka result
// of a blocking method)
template <class PromiseType>
struct BlockingMethodWrapper : BlockingMethod {
  BlockingMethodWrapper(const BlockingMethodWrapper& o) = delete;
  BlockingMethodWrapper& operator=(const BlockingMethodWrapper& o) = delete;

  explicit BlockingMethodWrapper(PromiseType promise)
      : promise(promise), coroutine_state(std::nullopt), result(std::nullopt) {}

  // starts a new coroutine and save the coroutine state
  void StartRequest() override {
    coroutine_state = std::move(StartRequestCoroutine());
  }

  bool IsFinished() override { return result.has_value(); }

  int GetResult() override {
    assert(result.has_value());
    return result.value();
  }

  // Can't copy the blocking method, because it's unclear what it means if we
  // can't copy the coroutines state
  BlockingMethodWrapper(BlockingMethodWrapper& other) = delete;

  ~BlockingMethodWrapper() override {
    if (coroutine_state.has_value() && coroutine_state.value().h) {
      coroutine_state->h.destroy();
    }
  }

 private:
  struct CoroutineResponse;

  // coroutine to await on the object inside
  CoroutineResponse StartRequestCoroutine() { result = co_await promise; }

  // have to have this boilerplate, it's required by the c++ standard
  struct CoroutineResponse {
    struct promise_type {
      CoroutineResponse get_return_object() {
        return {.h = std::coroutine_handle<promise_type>::from_promise(*this)};
      }
      std::suspend_never initial_suspend() { return {}; }
      // Without final suspend the destroy function isn't working well
      std::suspend_always final_suspend() noexcept { return {}; }
      void unhandled_exception() {}
    };

    std::coroutine_handle<promise_type> h;
  };

  PromiseType promise;
  std::optional<CoroutineResponse> coroutine_state;
  std::optional<int> result;
};

// This is the modified wg algorithm version, see docs for more details
template <class LinearSpecificationObject>
struct LinearizabilityDualChecker : ModelChecker {
  using BlockingMethodFactory = std::function<std::shared_ptr<BlockingMethod>(
      LinearSpecificationObject*, void* args)>;

  using NonBlockingMethod =
      std::function<int(LinearSpecificationObject*, void* args)>;

  using Method = std::variant<NonBlockingMethod, BlockingMethodFactory>;
  using MethodMap = std::map<MethodName, Method>;

  LinearizabilityDualChecker() = delete;

  LinearizabilityDualChecker(MethodMap specification_methods,
                             LinearSpecificationObject first_state);

  bool Check(const std::vector<HistoryEvent>& fixed_history) override;

 private:
  // ReproduceSeqHistory applies all events from the sequential history to the
  // first_state and returns obtained struct state and the stack of blocking
  // methods that were invoked
  std::pair<LinearSpecificationObject,
            std::map<size_t, std::shared_ptr<BlockingMethod>>>
  ReproduceSeqHistory(
      std::vector<std::pair<HistoryEvent, size_t>>& seq_history);

  std::map<MethodName, Method> specification_methods;
  LinearSpecificationObject first_state;
};

template <class LinearSpecificationObject>
LinearizabilityDualChecker<LinearSpecificationObject>::
    LinearizabilityDualChecker(
        LinearizabilityDualChecker::MethodMap specification_methods,
        LinearSpecificationObject first_state)
    : specification_methods(specification_methods), first_state(first_state) {
  if (!std::is_copy_constructible_v<LinearSpecificationObject>) {
    // TODO: should do it in the compile time
    throw std::invalid_argument(
        "LinearSpecificationObject type have to be is_copy_assignable_v");
  }
}

template <class LinearSpecificationObject>
bool LinearizabilityDualChecker<LinearSpecificationObject>::Check(
    const std::vector<HistoryEvent>& history) {
  // response -> invoke
  std::map<size_t, size_t> inv_res = get_inv_res_full_mapping(history);
  // follow_up_response -> request_invoke
  std::map<size_t, size_t> followup_request =
      get_followup_res_request_inv_mapping(history);
  // it can be a stack, but it's easier to use map: index -> method
  std::map<size_t, std::shared_ptr<BlockingMethod>> dual_requests;
  LinearSpecificationObject data_structure_state = first_state;
  // current history
  std::vector<std::pair<HistoryEvent, size_t>> seq_history;

  std::function<bool(const std::vector<HistoryEvent>&, std::vector<bool>&)>
      recursive_step;

  std::function<void(std::vector<size_t>&&, std::vector<bool>&)>
      fix_history_update_duals =
          [&](std::vector<size_t>&& indexes, std::vector<bool>& linearized) {
            for (auto index : indexes) {
              seq_history.pop_back();
              linearized[index] = false;
            }

            // have to reproduce the state from the start, because it's
            // impossible to copy a blocking method, so dual_requests might be
            // corrupted by the recursive_step call
            std::tie(data_structure_state, dual_requests) =
                ReproduceSeqHistory(seq_history);
          };

  recursive_step = [&](const std::vector<HistoryEvent>& history,
                       std::vector<bool>& linearized) -> bool {
    // Checks if the whole history is linearized
    if (std::reduce(linearized.begin(), linearized.end(), true,
                    std::bit_and<>())) {
      return true;
    }

    // walk all minimal operations
    for (size_t i = 0; i < history.size(); ++i) {
      // we could think that fixed_history doesn't contain events that already
      // have been linearized
      if (linearized[i]) {
        continue;
      }

      // all next operations are not minimal
      if (std::holds_alternative<Response>(history[i]) ||
          std::holds_alternative<RequestResponse>(history[i]) ||
          std::holds_alternative<FollowUpResponse>(history[i])) {
        break;
      }

      if (std::holds_alternative<Invoke>(history[i])) {
        // Nonblocking method case, same with the case in default recursive
        // checker
        Invoke minimal_op = std::get<Invoke>(history[i]);
        NonBlockingMethod method = std::get<NonBlockingMethod>(
            specification_methods.find(minimal_op.GetTask()->GetName())
                ->second);

        // Don't have to copy state here, because will have to reproduce it
        // anyway
        int res =
            method(&data_structure_state, minimal_op.GetTask()->GetArgs());

        linearized[i] = true;
        seq_history.emplace_back(history[i], i);
        std::vector<size_t> indexes_to_clear{i};

        // If invoke doesn't have a response we can't check the response
        if (inv_res.find(i) == inv_res.end()) {
          linearized[i] = true;
          seq_history.emplace_back(history[i], i);
          if (recursive_step(history, linearized)) {
            return true;
          }
          fix_history_update_duals({i}, linearized);
          continue;
        }

        // Have a response, try to linearize
        if (res == minimal_op.GetTask()->GetRetVal()) {
          linearized[i] = true;
          seq_history.emplace_back(history[i], i);
          assert(inv_res.find(i) != inv_res.end());
          linearized[inv_res[i]] = true;
          seq_history.emplace_back(history[inv_res[i]], inv_res[i]);

          if (recursive_step(history, linearized)) {
            return true;
          }

          // go back and reproduce state and duals
          fix_history_update_duals({i, inv_res[i]}, linearized);
        } else {
          // Anyway have applied method to the struct
          fix_history_update_duals(std::move(indexes_to_clear), linearized);
        }
      } else if (std::holds_alternative<RequestInvoke>(history[i])) {
        // Blocking method, try to linearize it, because we don't need to check
        // an answer in the request part answer have to be ready only when we
        // will be considering the corresponding follow-up part
        RequestInvoke minimal_op = std::get<RequestInvoke>(history[i]);
        assert(specification_methods.find(minimal_op.GetTask()->GetName()) !=
               specification_methods.end());
        BlockingMethodFactory mf = std::get<BlockingMethodFactory>(
            specification_methods.find(minimal_op.GetTask()->GetName())
                ->second);

        // Get out blocking method and save it to the map(stack)
        std::shared_ptr<BlockingMethod> method =
            mf(&data_structure_state, minimal_op.GetTask()->GetArgs());
        method->StartRequest();
        dual_requests[i] = method;

        linearized[i] = true;
        seq_history.emplace_back(history[i], i);
        if (inv_res.find(i) != inv_res.end()) {
          linearized[inv_res[i]] = true;
          seq_history.emplace_back(history[inv_res[i]], inv_res[i]);
        }

        if (recursive_step(history, linearized)) {
          return true;
        }

        // go back and reproduce state and duals
        std::vector<size_t> indexes_to_clear({i});
        if (inv_res.find(i) != inv_res.end()) {
          indexes_to_clear.push_back(inv_res[i]);
        }

        fix_history_update_duals(std::move(indexes_to_clear), linearized);
      } else if (std::holds_alternative<FollowUpInvoke>(history[i])) {
        // unfinished history, don't have to check the result
        if (inv_res.find(i) == inv_res.end()) {
          linearized[i] = true;
          seq_history.emplace_back(history[i], i);
          if (recursive_step(history, linearized)) {
            return true;
          }

          fix_history_update_duals({i}, linearized);
          continue;
        }

        // Blocking method follow-up section, have to check the return value
        // there
        FollowUpInvoke minimal_op = std::get<FollowUpInvoke>(history[i]);
        size_t followup_response_index = inv_res[i];
        size_t request_index = followup_request[followup_response_index];
        assert(dual_requests.find(request_index) != dual_requests.end());
        std::shared_ptr<BlockingMethod> method = dual_requests[request_index];

        // If the method doesn't ready just keep execution
        if (method->IsFinished() &&
            method->GetResult() == minimal_op.GetTask()->GetRetVal()) {
          linearized[i] = true;
          seq_history.emplace_back(history[i], i);
          linearized[inv_res[i]] = true;
          seq_history.emplace_back(history[inv_res[i]], inv_res[i]);

          if (recursive_step(history, linearized)) {
            return true;
          }

          fix_history_update_duals({i, inv_res[i]}, linearized);
        }
      }
    }

    return false;
  };

  std::vector<bool> linearized(history.size(), false);
  return recursive_step(history, linearized);
}

template <class LinearSpecificationObject>
std::pair<LinearSpecificationObject,
          std::map<size_t, std::shared_ptr<BlockingMethod>>>
LinearizabilityDualChecker<LinearSpecificationObject>::ReproduceSeqHistory(
    std::vector<std::pair<HistoryEvent, size_t>>& seq_history) {
  LinearSpecificationObject state = first_state;
  std::map<size_t, std::shared_ptr<BlockingMethod>> new_stack;

  for (auto event_pair : seq_history) {
    if (std::holds_alternative<Invoke>(event_pair.first)) {
      Invoke op = std::get<Invoke>(event_pair.first);

      assert(specification_methods.find(op.GetTask()->GetName()) !=
             specification_methods.end());
      NonBlockingMethod method = std::get<NonBlockingMethod>(
          specification_methods.find(op.GetTask()->GetName())->second);

      method(&state, op.GetTask()->GetArgs());
    } else if (std::holds_alternative<RequestInvoke>(event_pair.first)) {
      RequestInvoke op = std::get<RequestInvoke>(event_pair.first);

      BlockingMethodFactory mf = std::get<BlockingMethodFactory>(
          specification_methods.find(op.GetTask()->GetName())->second);
      std::shared_ptr<BlockingMethod> method =
          mf(&state, op.GetTask()->GetArgs());
      method->StartRequest();
      new_stack[event_pair.second] = method;
    }
  }

  return {state, new_stack};
}
