#pragma once
#include <random>
#include <utility>
#include <vector>

#include "pick_strategy.h"

// Allows a random thread to work.
// Randoms new task.
template <typename TargetObj, typename StrategyVerifier>
struct RandomStrategy : PickStrategy<TargetObj, StrategyVerifier> {
  explicit RandomStrategy(size_t threads_count,
                          std::vector<TaskBuilder> constructors,
                          std::vector<int> weights)
      : PickStrategy<TargetObj, StrategyVerifier>{threads_count,
                                                  std::move(constructors)},
        weights{std::move(weights)} {}

  size_t Pick() override {
    pick_weights.clear();
    auto &threads = PickStrategy<TargetObj, StrategyVerifier>::threads;
    for (size_t i = 0; i < threads.size(); ++i) {
      if (!threads[i].empty() && threads[i].back()->IsParked()) {
        continue;
      }
      pick_weights.push_back(weights[i]);
    }
    assert(!pick_weights.empty() && "deadlock");
    auto thread_distribution =
        std::discrete_distribution<>(pick_weights.begin(), pick_weights.end());
    auto num =
        thread_distribution(PickStrategy<TargetObj, StrategyVerifier>::rng);
    for (size_t i = 0; i < threads.size(); ++i) {
      if (!threads[i].empty() && threads[i].back()->IsParked()) {
        continue;
      }
      if (num == 0) {
        return i;
      }
      num--;
    }
    assert(false && "oops");
  }

 private:
  std::vector<int> weights;
  std::vector<int> pick_weights;
};
