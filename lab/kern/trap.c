#include <inc/mmu.h>
#include <inc/x86.h>
#include <inc/assert.h>
#include <inc/string.h>
#include <inc/pincl.h>

#include <kern/pmap.h>
#include <kern/trap.h>
#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/env.h>
#include <kern/syscall.h>
#include <kern/sched.h>
#include <kern/kclock.h>
#include <kern/picirq.h>
#include <kern/time.h>
#include <kern/e100.h>

static struct Taskstate ts;

/* Interrupt descriptor table.  (Must be built at run time because
 * shifted function addresses can't be represented in relocation records.)
 */
#if NO_LAB3A_CH_OPT
struct Gatedesc idt[256] = { { 0 } };
struct Pseudodesc idt_pd = {
	sizeof(idt) - 1, (uint32_t) idt
};
#else	/* !NO_LAB3A_CH_OPT */
extern void setup_idt();
extern struct Pseudodesc idt_pd;
#endif	/* !NO_LAB3A_CH_OPT */


static const char *trapname(int trapno)
{
	static const char * const excnames[] = {
		"Divide error",
		"Debug",
		"Non-Maskable Interrupt",
		"Breakpoint",
		"Overflow",
		"BOUND Range Exceeded",
		"Invalid Opcode",
		"Device Not Available",
		"Double Fault",
		"Coprocessor Segment Overrun",
		"Invalid TSS",
		"Segment Not Present",
		"Stack Fault",
		"General Protection",
		"Page Fault",
		"(unknown trap)",
		"x87 FPU Floating-Point Error",
		"Alignment Check",
		"Machine-Check",
		"SIMD Floating-Point Exception"
	};

	if (trapno < sizeof(excnames)/sizeof(excnames[0]))
		return excnames[trapno];
	if (trapno == T_SYSCALL)
		return "System call";
	if (trapno >= IRQ_OFFSET && trapno < IRQ_OFFSET + 16)
		return "Hardware Interrupt";
	return "(unknown trap)";
}

void
print_idt(struct Gatedesc *idt_head)
{
	int i;
	struct Gatedesc* ide;

	cprintf("IDT at %p\n", idt_head);
	for (i = 0; i < NO_GATEENTRIES; ++i) {
		ide = &idt_head[i];
		cprintf("IDE: [%p], %3d = %x, %u, %u, %x, %u, %u, %u, %04x, %04x\n",
				ide, i, ide->gd_ss, ide->gd_args, ide->gd_rsv1, ide->gd_type,
				ide->gd_s, ide->gd_dpl, ide->gd_p,
				ide->gd_off_31_16, ide->gd_off_15_0);
	}
}

#if NO_LAB3A_CH_OPT
static void
setup_idt()
{
	extern void th_divide();
	extern void th_debug();
	extern void th_nmi();
	extern void th_brkpt();
	extern void th_oflow();
	extern void th_bound();
	extern void th_illop();
	extern void th_device();
	extern void th_dblflt();
	//extern void th_coproc();
	extern void th_tss();
	extern void th_segnp();
	extern void th_stack();
	extern void th_gpflt();
	extern void th_pgflt();
	//extern void th_res();
	extern void th_fperr();
	extern void th_align();
	extern void th_mchk();
	extern void th_simderr();
	extern void th_syscall();
	//extern void th_default();

	clog("th_divide = %p", th_divide);
	clog("th_syscall = %p", th_syscall);
	SETGATE(idt[T_DIVIDE],	1, GD_KT, th_divide,	0);
	SETGATE(idt[T_DEBUG],	1, GD_KT, th_debug,		0);
	SETGATE(idt[T_NMI],		0, GD_KT, th_nmi,		0);
	SETGATE(idt[T_BRKPT],	1, GD_KT, th_brkpt,		3);
	SETGATE(idt[T_OFLOW],	1, GD_KT, th_oflow,		0);
	SETGATE(idt[T_BOUND],	1, GD_KT, th_bound,		0);
	SETGATE(idt[T_ILLOP],	1, GD_KT, th_illop,		0);
	SETGATE(idt[T_DEVICE],	1, GD_KT, th_device,	0);
	SETGATE(idt[T_DBLFLT],	1, GD_KT, th_dblflt,	0);
//	SETGATE(idt[T_COPROC],	1, GD_KT, th_coproc,	0);
	SETGATE(idt[T_TSS],		1, GD_KT, th_tss,		0);
	SETGATE(idt[T_SEGNP],	1, GD_KT, th_segnp,		0);
	SETGATE(idt[T_STACK],	1, GD_KT, th_stack,		0);
	SETGATE(idt[T_GPFLT],	1, GD_KT, th_gpflt,		0);
	SETGATE(idt[T_PGFLT],	1, GD_KT, th_pgflt,		0);
//	SETGATE(idt[T_RES],		1, GD_KT, th_res,		0);
	SETGATE(idt[T_FPERR],	1, GD_KT, th_fperr,		0);
	SETGATE(idt[T_ALIGN],	1, GD_KT, th_align,		0);
	SETGATE(idt[T_MCHK],	1, GD_KT, th_mchk,		0);
	SETGATE(idt[T_SIMDERR],	1, GD_KT, th_simderr,	0);
	SETGATE(idt[T_SYSCALL],	1, GD_KT, th_syscall,	3);
//	SETGATE(idt[T_DEFAULT],	1, GD_KT, th_default,	0);
}
#endif	/* NO_LAB3A_CH_OPT */

void
idt_init(void)
{
	extern struct Segdesc gdt[];
	
	// LAB 3: Your code here.
	assert(sizeof(struct Gatedesc) == SZ_GATEDESC);
	assert(sizeof(struct Pseudodesc) == SZ_PSEUDODESC);
	assert(sizeof(struct Trapframe) == SIZEOF_STRUCT_TRAPFRAME);
	clog("idt_pd = %u, %p", idt_pd.pd_lim, idt_pd.pd_base);
	//print_idt(idt);
	setup_idt();
	//print_idt(idt);

	// Setup a TSS so that we get the right stack
	// when we trap to the kernel.
	ts.ts_esp0 = KSTACKTOP;
	ts.ts_ss0 = GD_KD;

	// Initialize the TSS field of the gdt.
	gdt[GD_TSS >> 3] = SEG16(STS_T32A, (uint32_t) (&ts),
					sizeof(struct Taskstate), 0);
	gdt[GD_TSS >> 3].sd_s = 0;

	// Load the TSS
	ltr(GD_TSS);

	// Load the IDT
	asm volatile("lidt idt_pd");
}

void
print_trapframe(struct Trapframe *tf)
{
	cprintf("TRAP frame at %p\n", tf);
	print_regs(&tf->tf_regs);
	cprintf("  es   0x----%04x\n", tf->tf_es);
	cprintf("  ds   0x----%04x\n", tf->tf_ds);
	cprintf("  trap 0x%08x %s\n", tf->tf_trapno, trapname(tf->tf_trapno));
	cprintf("  err  0x%08x\n", tf->tf_err);
	cprintf("  eip  0x%08x\n", tf->tf_eip);
	cprintf("  cs   0x----%04x\n", tf->tf_cs);
	cprintf("  flag 0x%08x\n", tf->tf_eflags);
	cprintf("  esp  0x%08x\n", tf->tf_esp);
	cprintf("  ss   0x----%04x\n", tf->tf_ss);
}

void
print_regs(struct PushRegs *regs)
{
	cprintf("  edi  0x%08x\n", regs->reg_edi);
	cprintf("  esi  0x%08x\n", regs->reg_esi);
	cprintf("  ebp  0x%08x\n", regs->reg_ebp);
	cprintf("  oesp 0x%08x\n", regs->reg_oesp);
	cprintf("  ebx  0x%08x\n", regs->reg_ebx);
	cprintf("  edx  0x%08x\n", regs->reg_edx);
	cprintf("  ecx  0x%08x\n", regs->reg_ecx);
	cprintf("  eax  0x%08x\n", regs->reg_eax);
}

static void
trap_dispatch(struct Trapframe *tf)
{
	// Handle processor exceptions.
	// LAB 3: Your code here.
	int32_t sc_ret;

	assert(tf != NULL);
	switch (tf->tf_trapno) {
		case T_PGFLT:
			page_fault_handler(tf);
			return;
		case T_BRKPT:
		case T_DEBUG:
			/* break into the kernel monitor */
			while (1) {
				monitor(tf);
			}
			return;
		case T_SYSCALL:
			sc_ret = syscall(tf->tf_regs.reg_eax, tf->tf_regs.reg_edx,
					tf->tf_regs.reg_ecx, tf->tf_regs.reg_ebx,
					tf->tf_regs.reg_edi, tf->tf_regs.reg_esi);
			tf->tf_regs.reg_eax = sc_ret;
			return;

		default:
			break;
	}
	
	// Handle clock interrupts.
	// LAB 4: Your code here.

	// Add time tick increment to clock interrupts.
	// LAB 6: Your code here.
	switch (tf->tf_trapno) {
		case (IRQ_OFFSET + IRQ_TIMER):
			time_tick();
			sched_yield();
			return;
		case (IRQ_OFFSET + IRQ_KBD):
			//cprintf("Keyboard interrupt on irq %d\n", IRQ_KBD);
			kbd_intr();
			return;
		case (IRQ_OFFSET + IRQ_SERIAL):
			//cprintf("Serial interrupt on irq %d\n", IRQ_SERIAL);
			serial_intr();
			return;

		default:
			break;
	}

	if (tf->tf_trapno == (IRQ_OFFSET + e100_irq_line)) {
		cprintf("E100 interrupt on irq %d\n", e100_irq_line);
		return;
	}

	// Handle spurious interrupts
	// The hardware sometimes raises these because of noise on the
	// IRQ line or other reasons. We don't care.
	if (tf->tf_trapno == IRQ_OFFSET + IRQ_SPURIOUS) {
		cprintf("Spurious interrupt on irq 7\n");
		print_trapframe(tf);
		return;
	}


	// Handle keyboard and serial interrupts.
	// LAB 7: Your code here.

	// Unexpected trap: The user process or the kernel has a bug.
	print_trapframe(tf);
	if (tf->tf_cs == GD_KT)
		panic("unhandled trap in kernel");
	else {
		env_destroy(curenv);
		return;
	}
}

void
trap(struct Trapframe *tf)
{
	// The environment may have set DF and some versions
	// of GCC rely on DF being clear
	asm volatile("cld" ::: "cc");

	// Check that interrupts are disabled.  If this assertion
	// fails, DO NOT be tempted to fix it by inserting a "cli" in
	// the interrupt path.
	assert(!(read_eflags() & FL_IF));

	if ((tf->tf_cs & 3) == 3) {
		// Trapped from user mode.
		// Copy trap frame (which is currently on the stack)
		// into 'curenv->env_tf', so that running the environment
		// will restart at the trap point.
		assert(curenv);
		curenv->env_tf = *tf;
		// The trapframe on the stack should be ignored from here on.
		tf = &curenv->env_tf;
	}
	
	// Dispatch based on what type of trap occurred
	trap_dispatch(tf);

	// If we made it to this point, then no other environment was
	// scheduled, so we should return to the current environment
	// if doing so makes sense.
	if (curenv && curenv->env_status == ENV_RUNNABLE)
		env_run(curenv);
	else
		sched_yield();
}


void
page_fault_handler(struct Trapframe *tf)
{
	uint32_t fault_va;
	void *va;
	int rc;
	struct UTrapframe utf;

	// Read processor's CR2 register to find the faulting address
	fault_va = rcr2();

	// Handle kernel-mode page faults.
	
	// LAB 3: Your code here.
	if ((tf->tf_cs & 0x3) == RPL_K) {
		panic("Oops! Page fault in kernel: %p", fault_va);
	}

	// We've already handled kernel-mode exceptions, so if we get here,
	// the page fault happened in user mode.

	// Call the environment's page fault upcall, if one exists.  Set up a
	// page fault stack frame on the user exception stack (below
	// UXSTACKTOP), then branch to curenv->env_pgfault_upcall.
	//
	// The page fault upcall might cause another page fault, in which case
	// we branch to the page fault upcall recursively, pushing another
	// page fault stack frame on top of the user exception stack.
	//
	// The trap handler needs one word of scratch space at the top of the
	// trap-time stack in order to return.  In the non-recursive case, we
	// don't have to worry about this because the top of the regular user
	// stack is free.  In the recursive case, this means we have to leave
	// an extra word between the current top of the exception stack and
	// the new stack frame because the exception stack _is_ the trap-time
	// stack.
	//
	// If there's no page fault upcall, the environment didn't allocate a
	// page for its exception stack or can't write to it, or the exception
	// stack overflows, then destroy the environment that caused the fault.
	// Note that the grade script assumes you will first check for the page
	// fault upcall and print the "user fault va" message below if there is
	// none.  The remaining three checks can be combined into a single test.
	//
	// Hints:
	//   user_mem_assert() and env_run() are useful here.
	//   To change what the user environment runs, modify 'curenv->env_tf'
	//   (the 'tf' variable points at 'curenv->env_tf').

	// LAB 4: Your code here.
	if (curenv->env_pgfault_upcall != NULL) {
		utf.utf_fault_va = fault_va;
		utf.utf_err = tf->tf_err;
		utf.utf_regs = tf->tf_regs;
		utf.utf_eip = tf->tf_eip;
		utf.utf_eflags = tf->tf_eflags;
		utf.utf_esp = tf->tf_esp;

		// NOTE: since PF happened in user mode, CR3 should already have the
		// corres. env's PD loaded, hence no need to do explicit lcr3().
		if ( ((UXSTACKTOP - PGSIZE) <= tf->tf_esp)
				&& (tf->tf_esp < UXSTACKTOP) ) {
			va = (void *) (tf->tf_esp - sizeof(utf) - 4);
			//clog("wp1: va = %p, fault_va = %p, err = 0x%x, esp = %p, eip = %p",
			//		va, fault_va, tf->tf_err, tf->tf_esp, tf->tf_eip);
			user_mem_assert(curenv, va, sizeof(utf) + 4, PTE_W | PTE_U);

			//lcr3(curenv->env_cr3);
			memset(va + sizeof(utf), 0, 4);
		}
		else {
			va = (void *) (UXSTACKTOP - sizeof(utf));
			//clog("wp2: va = %p, fault_va = %p, err = 0x%x, esp = %p, eip = %p",
			//		va, fault_va, tf->tf_err, tf->tf_esp, tf->tf_eip);
			user_mem_assert(curenv, va, sizeof(utf), PTE_W | PTE_U);

			//lcr3(curenv->env_cr3);
		}
		memmove(va, &utf, sizeof(utf));

		tf->tf_esp = (uintptr_t) va;
		tf->tf_eip = (uintptr_t) curenv->env_pgfault_upcall;
		env_run(curenv);
	}

	// Destroy the environment that caused the fault.
	cprintf("[%08x] user fault va %08x ip %08x err %08x\n",
		curenv->env_id, fault_va, tf->tf_eip, tf->tf_err);
	print_trapframe(tf);
	//mon_backtrace(0, 0, tf);
	env_destroy(curenv);
}

