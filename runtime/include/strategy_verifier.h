#pragma once
#include "scheduler.h"

struct DefaultStrategyVerifier {
  inline bool Verify(NextTask task) { return true; }

  inline void OnFinished(ChosenTask task) {}
};
