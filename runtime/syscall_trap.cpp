#include "syscall_trap.h"
#include "lib.h"
#include "verifying.h"

#include <asm/unistd_64.h>
#include <stdio.h>
#include <string.h>
#include <sys/ptrace.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <sys/reg.h>
#include <unistd.h>
#include <elf.h>
#include <sys/uio.h>

// bool __trap_syscall = false; 

static int set_bp_and_wait_for_trap(
    pid_t next_bp_tid,
    int *exit_status) { /* NOTEs: 'bp' = breakpoint; Reports only 'trap events'
                           which are due to termination or stops caused by
                           syscall's */

  for (int pending_signal = 0;;) {
    if (-1 != next_bp_tid) { 
      ptrace(PTRACE_SYSCALL, next_bp_tid, 0, pending_signal);
    }

    /* Reset signal (after it has been delivered) */
    pending_signal = 0;

    int trapped_tracee_status;
    const pid_t trapped_tracee_tid =
        waitpid(-1, &trapped_tracee_status, __WALL);

    if (WIFSTOPPED(trapped_tracee_status)) {
      siginfo_t si;

      next_bp_tid = trapped_tracee_tid;
      const int stopsig = WSTOPSIG(trapped_tracee_status);

      if ((SIGTRAP | PTRACE_TRAP_INDICATOR_BIT) == stopsig) {
        return trapped_tracee_tid; /* >>>   Tracee was stopped (indicated by
                                      positive returned tid; only possible stop
                                      reason here: due to syscall breakpoint) */

      } else if (SIGTRAP == stopsig) {
        continue;
      } else if (ptrace(PTRACE_GETSIGINFO, trapped_tracee_tid, 0, &si) < 0) {
        continue;
      } else {
        fprintf(stderr,
                "\n+++ [%d] received (not delivered yet) signal \"%s\" +++\n",
                trapped_tracee_tid, strsignal(stopsig));
        pending_signal = stopsig;
      }

    } else {
      if (WIFEXITED(trapped_tracee_status)) {
        *exit_status = WEXITSTATUS(trapped_tracee_status);

      } else if (WIFSIGNALED(trapped_tracee_status)) {
        *exit_status = WTERMSIG(trapped_tracee_status);
      }

      return -(trapped_tracee_tid); /* >>>   Tracee has terminated (indicated by
                                       negative returned tid; possible stop
                                       reasons: see above) */
    }
  }
}

inline int ptrace_get_regs_content(pid_t tid, struct user_regs_struct *regs) {

    ptrace(PTRACE_GETREGS, tid, 0, &regs);

    return 0;
}

extern "C" long handled_futex(uint32_t *uaddr, int futex_op, uint32_t val,
                    const struct timespec *timeout,
                    uint32_t *uaddr2, uint32_t val3) {
label: 
  fprintf(stderr, "child: %p\n", &&label);
  fprintf(stderr, "child: futex(0x%lx, %d, %d)\n", (unsigned long)uaddr, futex_op, val);
  CoroYield();
  return 0;
                    }

// int ltest::TrapRun(std::unique_ptr<Scheduler> &&scheduler, PrettyPrinter &pretty_printer) {
//     pid_t tracee_pid = fork();
//     if (tracee_pid) {
//       int status = 0;
//       struct user_regs_struct regs;
//       long syscall_nr;
//       waitpid(tracee_pid, &status, 0);
//       fprintf(stderr, "Started\n");
//       ptrace(PTRACE_SETOPTIONS, tracee_pid, 0, PTRACE_O_TRACESYSGOOD);
//       int tracee_exit_status = -1;
//       for (pid_t trapped_tracee_sttid = tracee_pid; ;) {
//         // fprintf(stderr, "Catching call...\n");
//         trapped_tracee_sttid =
//             set_bp_and_wait_for_trap(trapped_tracee_sttid, &tracee_exit_status);
//         // fprintf(stderr, "Caught\n");
//         if (0 > trapped_tracee_sttid) {
//           fprintf(stderr, "\n+++ [%d] terminated w/ %d +++\n",
//                   -(trapped_tracee_sttid), tracee_exit_status);
//           if (-(tracee_pid) == trapped_tracee_sttid) {
//             break;
//           }      /* -> Thread group leader exited -> Stop tracing */
//           else { /* -> LWP in thread group exited */
//             trapped_tracee_sttid = -1; /* NOTE: `-1` = tracee has exited
//                                           (pertinent for `wait_for_trap`) */
//             continue;
//           }
//           /*   -> Thread stopped (i.e., hit breakpoint) */
//         } else {
//           syscall_nr = ptrace(PTRACE_PEEKUSER, trapped_tracee_sttid, sizeof(long)*ORIG_RAX);
//           if (syscall_nr == __NR_futex) {
//             __trap_syscall = ptrace(PTRACE_PEEKDATA, trapped_tracee_sttid, (void*)&__trap_syscall);
//             if (__trap_syscall) {
//               ptrace(PTRACE_GETREGS, trapped_tracee_sttid, 0, &regs);
//               fprintf(stderr, "parent: futex(0x%lx, %d, %d) at %08lx\n", (unsigned long)(USER_REGS_STRUCT_SC_ARG0(regs)), USER_REGS_STRUCT_SC_ARG1(regs), USER_REGS_STRUCT_SC_ARG2(regs),   regs.rip);
//               regs.rsp -= sizeof(long);
//               // Копируем слово в память дочернего процесса
//               ptrace(PTRACE_POKEDATA, trapped_tracee_sttid, (void*)regs.rsp, regs.rip + 2);
//               // Устанавливаем RIP по адресу handled_futex
//               regs.rip = (unsigned long) handled_futex;
//               // regs.rdi += 1000;
//               fprintf(stderr, "rsp: %8llx, rip: %08llx\n", regs.rsp, regs.rip);
//               ptrace(PTRACE_SETREGS, trapped_tracee_sttid, 0, &regs);
//             }
//           }
//         }
//       }
//       return tracee_exit_status;
//     } else {
//       ptrace(PTRACE_TRACEME, 0, 0, 0);
//       kill(getpid(), SIGSTOP);
//       handled_futex(0, 0, 0, 0, 0, 0);
//       __trap_syscall = true;
//       auto res = Run(std::move(scheduler), pretty_printer);
//       __trap_syscall = false;
//       return res;
//     }
// }

int ltest::TrapRun(std::unique_ptr<Scheduler> &&scheduler, PrettyPrinter &pretty_printer) {
  auto res = Run(std::move(scheduler), pretty_printer);
  return res;
}