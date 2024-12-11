#include "syscall_trap.h"

#include <asm/unistd_64.h>
#include <elf.h>
#include <sys/ptrace.h>
#include <sys/reg.h>
#include <sys/uio.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <unistd.h>

#include "verifying.h"

extern "C" bool __trap_syscall = 0;

ltest::SyscallTrapGuard::SyscallTrapGuard() { __trap_syscall = true; }

ltest::SyscallTrapGuard::~SyscallTrapGuard() { __trap_syscall = false; }

int ltest::TrapRun(std::unique_ptr<Scheduler> &&scheduler,
                   PrettyPrinter &pretty_printer) {
  auto guard = SyscallTrapGuard{};
  auto res = Run(std::move(scheduler), pretty_printer);
  return res;
}