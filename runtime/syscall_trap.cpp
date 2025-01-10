#include "syscall_trap.h"

#include <asm/unistd_64.h>
#include <elf.h>
#include <sys/ptrace.h>
#include <sys/reg.h>
#include <sys/uio.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <unistd.h>

bool __trap_syscall = 0;

ltest::SyscallTrapGuard::SyscallTrapGuard() { __trap_syscall = true; }

ltest::SyscallTrapGuard::~SyscallTrapGuard() { __trap_syscall = false; }
