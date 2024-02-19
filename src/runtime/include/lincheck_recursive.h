#pragma once

#include <functional>

#include "lincheck.h"

// This is the simplest wgl version, it doesn't contain any optimizations, it's
// slow but useful for stress tests of other implementations
template <class LinearSpecificationObject,
          class SpecificationObjectHash = std::hash<LinearSpecificationObject>,
          class SpecificationObjectEqual =
              std::equal_to<LinearSpecificationObject>>
struct LinearizabilityCheckerRecursive : ModelChecker {
  LinearizabilityCheckerRecursive() = delete;

  LinearizabilityCheckerRecursive(
      std::map<MethodName, std::function<int(LinearSpecificationObject*)>>
          specification_methods,
      LinearSpecificationObject first_state);

  bool Check(
      const std::vector<std::variant<StackfulTaskInvoke, StackfulTaskResponse>>&
          history) override;

 private:
  std::map<MethodName, std::function<int(LinearSpecificationObject*)>>
      specification_methods;
  LinearSpecificationObject first_state;
};

template <class LinearSpecificationObject, class SpecificationObjectHash,
          class SpecificationObjectEqual>
LinearizabilityCheckerRecursive<LinearSpecificationObject,
                                SpecificationObjectHash,
                                SpecificationObjectEqual>::
    LinearizabilityCheckerRecursive(
        std::map<MethodName, std::function<int(LinearSpecificationObject*)>>
            specification_methods,
        LinearSpecificationObject first_state)
    : specification_methods(specification_methods), first_state(first_state) {
  if (!std::is_copy_assignable_v<LinearSpecificationObject>) {
    // TODO: should do it in the compile time
    throw std::invalid_argument(
        "LinearSpecificationObject type have to be is_copy_assignable_v");
  }
}

template <class LinearSpecificationObject, class SpecificationObjectHash,
          class SpecificationObjectEqual>
bool LinearizabilityCheckerRecursive<LinearSpecificationObject,
                                     SpecificationObjectHash,
                                     SpecificationObjectEqual>::
    Check(const std::vector<
          std::variant<StackfulTaskInvoke, StackfulTaskResponse>>& history) {
  std::map<size_t, size_t> inv_res = get_inv_res_mapping(history);

  std::function<bool(const std::vector<
    std::variant<StackfulTaskInvoke, StackfulTaskResponse>>&, std::vector<bool>&, LinearSpecificationObject)> recursive_step;

  recursive_step =
      [&](
          const std::vector<
              std::variant<StackfulTaskInvoke, StackfulTaskResponse>>& history,
          std::vector<bool>& linearized,
          LinearSpecificationObject data_structure_state) -> bool {
    // walk all minimal operations
    for (size_t i = 0; i < history.size(); ++i) {
      // we could think that history doesn't contain events that already have
      // been linearized
      if (linearized[i]) {
        continue;
      }

      // all next operations are not minimal
      if (history[i].index() == 1) {
        break;
      }

      StackfulTaskInvoke minimal_op = std::get<StackfulTaskInvoke>(history[i]);
      auto method =
          specification_methods.find(minimal_op.task.GetName())->second;

      LinearSpecificationObject data_structure_state_copy =
          data_structure_state;
      // state is already have been copied, because it's the argument of the
      // lambda
      int res = method(&data_structure_state_copy);
      if (res == minimal_op.task.GetRetVal()) {
        linearized[i] = true;
        assert(inv_res.find(i) != inv_res.end());
        linearized[inv_res[i]] = true;

        if (recursive_step(history, linearized, data_structure_state_copy)) {
          return true;
        }
      }
    }

    return false;
  };

  std::vector<bool> linearized(history.size(), false);
  return recursive_step(history, linearized, first_state);
}
