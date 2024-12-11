#pragma once
#include "pretty_print.h"
#include "scheduler.h"

namespace ltest {

struct SyscallTrapGuard {
  SyscallTrapGuard();
  ~SyscallTrapGuard();
};

int TrapRun(std::unique_ptr<Scheduler>&& scheduler,
            PrettyPrinter& pretty_printer);
}  // namespace ltest