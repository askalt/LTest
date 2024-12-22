#pragma once
#include "scheduler.h"

namespace ltest {
struct DefaultStrategyVerifier {
  inline bool Verify(NextTask task) { return true; }

  inline void OnFinished(ChosenTask task) {}
};
}  // namespace ltest
