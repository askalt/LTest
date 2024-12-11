#include <libsyscall_intercept_hook_point.h>
#include <sys/syscall.h>
#include <syscall.h>
#include "stdio.h"

extern "C" bool __trap_syscall = 0;
extern "C" void CoroYield();

static int
hook(long syscall_number,
			long arg0, long arg1,
			long arg2, long arg3,
			long arg4, long arg5,
			long *result)
{
	if (syscall_number == SYS_futex && __trap_syscall) {
		fprintf(stderr, "child: futex(0x%lx, %d, %d)\n", (unsigned long)arg0, arg1, arg2);
        CoroYield();
		return 0;
	} else {
		/*
		 * Ignore any other syscalls
		 * i.e.: pass them on to the kernel
		 * as would normally happen.
		 */
		return 1;
	}
}

static __attribute__((constructor)) void
init(void)
{
	// Set up the callback function
	intercept_hook_point = hook;
}