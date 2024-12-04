#include "pretty_print.h"
#include "scheduler.h"

#define USER_REGS_STRUCT_IP(regss)           (regss.rip)
#define USER_REGS_STRUCT_SP(regss)           (regss.rsp)
#define USER_REGS_STRUCT_SC_NO(regss)        ((const int)(regss.orig_rax))
#define USER_REGS_STRUCT_SC_RTNVAL(regss)    (regss.rax)
#define USER_REGS_STRUCT_SC_ARG0(regss)      (regss.rdi)
#define USER_REGS_STRUCT_SC_ARG1(regss)      (regss.rsi)
#define USER_REGS_STRUCT_SC_ARG2(regss)      (regss.rdx)
#define USER_REGS_STRUCT_SC_ARG3(regss)      (regss.r10)
#define USER_REGS_STRUCT_SC_ARG4(regss)      (regss.r8)
#define USER_REGS_STRUCT_SC_ARG5(regss)      (regss.r9)
#define PTRACE_TRAP_INDICATOR_BIT (1 << 7)

// extern bool __trap_syscall;

namespace ltest {
    extern "C" int TrapRun(std::unique_ptr<Scheduler> &&scheduler, PrettyPrinter& pretty_printer);
}