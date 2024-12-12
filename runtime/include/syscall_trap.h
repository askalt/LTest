#pragma once

namespace ltest {

struct SyscallTrapGuard {
  SyscallTrapGuard();
  ~SyscallTrapGuard();
};

}  // namespace ltest