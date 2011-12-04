/* See COPYRIGHT for copyright information. */

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/assert.h>
#include <inc/x86.h>
#include <inc/pincl.h>

#include <kern/monitor.h>
#include <kern/console.h>
#include <kern/pmap.h>
#include <kern/kclock.h>
#include <kern/env.h>
#include <kern/trap.h>
#include <kern/sched.h>
#include <kern/picirq.h>
#include <kern/time.h>
#include <kern/pci.h>


static void
setup_msr()
{
	extern void sysenter_handler();
	uint32_t lo, hi;

#if !(NO_FAST_SYSCALL)
	wrmsr(SYSENTER_CS_MSR, GD_KT, 0x0);
	wrmsr(SYSENTER_EIP_MSR, (uint32_t) sysenter_handler,
			0x0);
	wrmsr(SYSENTER_ESP_MSR, KSTACKTOP, 0x0);

	lo = hi = 0x0;
	rdmsr(SYSENTER_CS_MSR, &lo, &hi);
	clog("SYSENTER_CS_MSR = 0x %x %x", hi, lo);
	lo = hi = 0x0;
	rdmsr(SYSENTER_EIP_MSR, &lo, &hi);
	clog("sysenter_handler = %p, SYSENTER_EIP_MSR = 0x %x %x",
			sysenter_handler, hi, lo);
	lo = hi = 0x0;
	rdmsr(SYSENTER_ESP_MSR, &lo, &hi);
	clog("SYSENTER_ESP_MSR = 0x %x %x", hi, lo);
#endif
}

void
i386_init(void)
{
	extern char edata[], end[];

	// Before doing anything else, complete the ELF loading process.
	// Clear the uninitialized global data (BSS) section of our program.
	// This ensures that all static/global variables start out zero.
	memset(edata, 0, end - edata);

	// Initialize the console.
	// Can't call cprintf until after we do this!
	cons_init();

	cprintf("6828 decimal is %o octal!\n", 6828);

	// Lab 2 memory management initialization functions
	i386_detect_memory();
	i386_vm_init();

	// Lab 3 user environment initialization functions
	setup_msr();
	env_init();
	idt_init();

	// Lab 4 multitasking initialization functions
	pic_init();
	kclock_init();

	time_init();
	pci_init();

	// Should always have an idle process as first one.
	ENV_CREATE(user_idle);

	// Start fs.
	ENV_CREATE(fs_fs);

#if !defined(TEST_NO_NS)
	// Start ns.
	ENV_CREATE(net_ns);
#endif

#if defined(TEST)
	// Don't touch -- used by grading script!
	ENV_CREATE2(TEST, TESTSIZE);
#else
	// Touch all you want.
#if LAB4A_SCHED_PRI
	int i;
	for (i = 1; i < 6; ++i) {
		ENV_CREATE(user_yield);
	}
#else
	//ENV_CREATE(user_primes);
	//ENV_CREATE(user_dumbfork);
	//ENV_CREATE(user_forktree);
	//ENV_CREATE(user_pingpong);
	// ENV_CREATE(user_writemotd);
	// ENV_CREATE(user_testfile);
	// ENV_CREATE(user_icode);
	// ENV_CREATE(user_testtime);
#if defined(TEST_NO_NS)
	// ENV_CREATE(net_testoutput);
	ENV_CREATE(net_testinput);
#else
	ENV_CREATE(user_echosrv);
	ENV_CREATE(user_httpd);
#endif
	//ENV_CREATE(user_testpteshare);
	//ENV_CREATE(user_testfdsharing);
	//ENV_CREATE(user_testshell);
	ENV_CREATE(user_icode);
#endif
#endif // TEST*

	// Should not be necessary - drains keyboard because interrupt has given up.
	kbd_intr();

	// Schedule and run the first user environment!
	sched_yield();
}


/*
 * Variable panicstr contains argument to first call to panic; used as flag
 * to indicate that the kernel has already called panic.
 */
static const char *panicstr;

/*
 * Panic is called on unresolvable fatal errors.
 * It prints "panic: mesg", and then enters the kernel monitor.
 */
void
_panic(const char *file, int line, const char *fmt,...)
{
	va_list ap;

	if (panicstr)
		goto dead;
	panicstr = fmt;

	// Be extra sure that the machine is in as reasonable state
	__asm __volatile("cli; cld");

	va_start(ap, fmt);
	cprintf("kernel panic at %s:%d: ", file, line);
	vcprintf(fmt, ap);
	cprintf("\n");
	va_end(ap);

dead:
	/* break into the kernel monitor */
	while (1)
		monitor(NULL);
}

/* like panic, but don't */
void
_warn(const char *file, int line, const char *fmt,...)
{
	va_list ap;

	va_start(ap, fmt);
	cprintf("kernel warning at %s:%d: ", file, line);
	vcprintf(fmt, ap);
	cprintf("\n");
	va_end(ap);
}
