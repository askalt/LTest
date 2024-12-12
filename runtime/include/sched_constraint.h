#pragma once
#include "scheduler.h"

namespace ltest {
struct DefaultSchedConstraint {
  inline bool Validate(NextTask task) { return true; }

  inline void OnFinished(ChosenTask& task) {}
};
}  // namespace ltest
