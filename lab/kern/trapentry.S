/* See COPYRIGHT for copyright information. */

#include <inc/mmu.h>
#include <inc/memlayout.h>
#include <inc/trap.h>

#include <kern/picirq.h>


###################################################################
# exceptions/interrupts
###################################################################

#if NO_LAB3A_CH_OPT
#error "Deprecated code, consider using Optimized code for Lab 3a challenge"

/* The TRAPHANDLER macro defines a globally-visible function for handling
 * a trap.  It pushes a trap number onto the stack, then jumps to _alltraps.
 * Use TRAPHANDLER for traps where the CPU automatically pushes an error code.
 */ 
#define TRAPHANDLER(name, num)						\
	.globl name;		/* define global symbol for 'name' */	\
	.type name, @function;	/* symbol type is function */		\
	.align 2;		/* align function definition */		\
	name:			/* function starts here */		\
	pushl $(num);							\
	jmp _alltraps

/* Use TRAPHANDLER_NOEC for traps where the CPU doesn't push an error code.
 * It pushes a 0 in place of the error code, so the trap frame has the same
 * format in either case.
 */
#define TRAPHANDLER_NOEC(name, num)					\
	.globl name;							\
	.type name, @function;						\
	.align 2;							\
	name:								\
	pushl $0;							\
	pushl $(num);							\
	jmp _alltraps

#else	/* !NO_LAB3A_CH_OPT */

#define GATE_NULL \
	.word	0, 0; \
	.byte	0, 0, 0, 0
#define GATE(type, sel, off, dpl) \
	.word	((off) & 0xffff), ((sel) & 0xffff); \
	.byte	0, (0x80 | (((dpl) & 0x3) << 5) | ((type) & 0xf)), \
			(((off) >> 16) & 0xff), (((off) >> 24) & 0xff)

.macro GATE_OFFSET_STUB name, num
.text
1:
	leal	\name, %edx
	leal	idt, %eax
	addl	$(SZ_GATEDESC*(\num)), %eax
	movw	%dx, (%eax)
	sarl	$(16), %edx
	addl	$(6), %eax
	movw	%dx, (%eax)
	jmp		1f
.endm

.macro TRAPHANDLER name, num, nametype, dpl
.text
.type \name, @function
.align 2
\name:
	pushl	$(\num)
	jmp		_alltraps

GATE_OFFSET_STUB name=\name,num=\num

.data
GATE(\nametype, GD_KT, 0x0, \dpl)
.endm

.macro TRAPHANDLER_NOEC name, num, nametype, dpl
.text
.type \name, @function
.align 2
\name:
	pushl	$0
	pushl	$(\num)
	jmp		_alltraps

GATE_OFFSET_STUB name=\name,num=\num

.data
GATE(\nametype, GD_KT, 0x0, \dpl)
.endm

#endif	/* !NO_LAB3A_CH_OPT */

.text

/*
 * Lab 3: Your code here for generating entry points for the different traps.
 */
#if NO_LAB3A_CH_OPT

TRAPHANDLER_NOEC(th_divide, T_DIVIDE)
TRAPHANDLER_NOEC(th_debug, T_DEBUG)
TRAPHANDLER_NOEC(th_nmi, T_NMI)
TRAPHANDLER_NOEC(th_brkpt, T_BRKPT)
TRAPHANDLER_NOEC(th_oflow, T_OFLOW)
TRAPHANDLER_NOEC(th_bound, T_BOUND)
TRAPHANDLER_NOEC(th_illop, T_ILLOP)
TRAPHANDLER_NOEC(th_device, T_DEVICE)
TRAPHANDLER(th_dblflt, T_DBLFLT)
/*
TRAPHANDLER_NOEC(th_coproc, T_COPROC)
 */
TRAPHANDLER(th_tss, T_TSS)
TRAPHANDLER(th_segnp, T_SEGNP)
TRAPHANDLER(th_stack, T_STACK)
TRAPHANDLER(th_gpflt, T_GPFLT)
TRAPHANDLER(th_pgflt, T_PGFLT)
/*
TRAPHANDLER_NOEC(th_res, T_RES)
 */
TRAPHANDLER_NOEC(th_fperr, T_FPERR)
TRAPHANDLER(th_align, T_ALIGN)
TRAPHANDLER_NOEC(th_mchk, T_MCHK)
TRAPHANDLER_NOEC(th_simderr, T_SIMDERR)
TRAPHANDLER_NOEC(th_syscall, T_SYSCALL)
/*
TRAPHANDLER_NOEC(th_default, T_DEFAULT)
 */

#else	/* !NO_LAB3A_CH_OPT */

.text

.globl setup_idt
.type setup_idt, @function
.align 2
setup_idt:
	jmp		1f

.data

.globl	idt
.type	idt, @object
.size	idt, SZ_GATEDESC_TABLE
.p2align 2				# force 4 byte alignment
idt:

/*
 * BEGIN:	~~~~~~~~~~ IDT Entries population ~~~~~~~~~~
 * NOTE: The TRAPHANDLER* macros must be used in *order* here, with proper
 * zero-padding wherever applicable.
 */
/*
 * First 20 Trap Vectors
 */
TRAPHANDLER_NOEC name=th_divide,num=T_DIVIDE,nametype=STS_IG32,dpl=0
TRAPHANDLER_NOEC name=th_debug,num=T_DEBUG,nametype=STS_IG32,dpl=0
TRAPHANDLER_NOEC name=th_nmi,num=T_NMI,nametype=STS_IG32,dpl=0
TRAPHANDLER_NOEC name=th_brkpt,num=T_BRKPT,nametype=STS_IG32,dpl=3
TRAPHANDLER_NOEC name=th_oflow,num=T_OFLOW,nametype=STS_IG32,dpl=0
TRAPHANDLER_NOEC name=th_bound,num=T_BOUND,nametype=STS_IG32,dpl=0
TRAPHANDLER_NOEC name=th_illop,num=T_ILLOP,nametype=STS_IG32,dpl=0
TRAPHANDLER_NOEC name=th_device,num=T_DEVICE,nametype=STS_IG32,dpl=0
TRAPHANDLER name=th_dblflt,num=T_DBLFLT,nametype=STS_IG32,dpl=0
GATE_NULL	/* T_COPROC */
TRAPHANDLER name=th_tss,num=T_TSS,nametype=STS_IG32,dpl=0
TRAPHANDLER name=th_segnp,num=T_SEGNP,nametype=STS_IG32,dpl=0
TRAPHANDLER name=th_stack,num=T_STACK,nametype=STS_IG32,dpl=0
TRAPHANDLER name=th_gpflt,num=T_GPFLT,nametype=STS_IG32,dpl=0
TRAPHANDLER name=th_pgflt,num=T_PGFLT,nametype=STS_IG32,dpl=0
GATE_NULL	/* T_RES */
TRAPHANDLER_NOEC name=th_fperr,num=T_FPERR,nametype=STS_IG32,dpl=0
TRAPHANDLER name=th_align,num=T_ALIGN,nametype=STS_IG32,dpl=0
TRAPHANDLER_NOEC name=th_mchk,num=T_MCHK,nametype=STS_IG32,dpl=0
TRAPHANDLER_NOEC name=th_simderr,num=T_SIMDERR,nametype=STS_IG32,dpl=0
/*
 * Next 12: NULL for now
 */
.data
	.zero SZ_GATEDESC*12
/*
 * Next 16: (External interrupts) IRQs 0-15
 */
TRAPHANDLER_NOEC name=th_irq_0,num=IRQ_OFFSET+0x0,nametype=STS_IG32,dpl=0
TRAPHANDLER_NOEC name=th_irq_1,num=IRQ_OFFSET+0x1,nametype=STS_IG32,dpl=0
TRAPHANDLER_NOEC name=th_irq_2,num=IRQ_OFFSET+0x2,nametype=STS_IG32,dpl=0
TRAPHANDLER_NOEC name=th_irq_3,num=IRQ_OFFSET+0x3,nametype=STS_IG32,dpl=0
TRAPHANDLER_NOEC name=th_irq_4,num=IRQ_OFFSET+0x4,nametype=STS_IG32,dpl=0
TRAPHANDLER_NOEC name=th_irq_5,num=IRQ_OFFSET+0x5,nametype=STS_IG32,dpl=0
TRAPHANDLER_NOEC name=th_irq_6,num=IRQ_OFFSET+0x6,nametype=STS_IG32,dpl=0
TRAPHANDLER_NOEC name=th_irq_7,num=IRQ_OFFSET+0x7,nametype=STS_IG32,dpl=0
TRAPHANDLER_NOEC name=th_irq_8,num=IRQ_OFFSET+0x8,nametype=STS_IG32,dpl=0
TRAPHANDLER_NOEC name=th_irq_9,num=IRQ_OFFSET+0x9,nametype=STS_IG32,dpl=0
TRAPHANDLER_NOEC name=th_irq_a,num=IRQ_OFFSET+0xa,nametype=STS_IG32,dpl=0
TRAPHANDLER_NOEC name=th_irq_b,num=IRQ_OFFSET+0xb,nametype=STS_IG32,dpl=0
TRAPHANDLER_NOEC name=th_irq_c,num=IRQ_OFFSET+0xc,nametype=STS_IG32,dpl=0
TRAPHANDLER_NOEC name=th_irq_d,num=IRQ_OFFSET+0xd,nametype=STS_IG32,dpl=0
TRAPHANDLER_NOEC name=th_irq_e,num=IRQ_OFFSET+0xe,nametype=STS_IG32,dpl=0
TRAPHANDLER_NOEC name=th_irq_f,num=IRQ_OFFSET+0xf,nametype=STS_IG32,dpl=0
/*
 * 49th: Arbitrarily chosen System call
 */
TRAPHANDLER_NOEC name=th_syscall,num=T_SYSCALL,nametype=STS_IG32,dpl=3
/*
 * Rest all NULL for now
 */
.data
	.zero SZ_GATEDESC*(NO_GATEENTRIES-1-T_SYSCALL)
/*
 * END:		~~~~~~~~~~ IDT Entries population ~~~~~~~~~~
 */

.purgem TRAPHANDLER_NOEC
.purgem TRAPHANDLER
.purgem GATE_OFFSET_STUB

.data
.globl	idt_pd
.type	idt_pd, @object
.size	idt_pd, SZ_PSEUDODESC
.p2align 2				# force 4 byte alignment
idt_pd:
	.word	(SZ_GATEDESC_TABLE - 1)
	.long	idt

.text
1:
	ret

#endif	/* !NO_LAB3A_CH_OPT */



/*
 * Lab 3: Your code here for _alltraps
 */
_alltraps:
	pushl	%ds
	pushl	%es
	pushal
	xorl	%eax, %eax		# Zero out 'eax'
	movl	$(GD_KD), %eax
	movw	%ax, %ds
	movw	%ax, %es
	pushl	%esp
	call	trap
	popl	%esp
	popal
	popl	%es
	popl	%ds
	addl	$4, %esp		# Pop off Trap No.
	addl	$4, %esp		# Pop off Error Code
	iret

#if NO_FAST_SYSCALL
//#warning "Not using any FAST system call, consider using partially."

#else	/* !NO_FAST_SYSCALL */

.globl sysenter_handler
.type sysenter_handler, @function
.align 2
sysenter_handler:
	pushl	%ebp			# Return esp
	pushl	%esi			# Return eip
	pushl	$0				# dummy arg5, not passed
	pushl	%edi			# arg4
	pushl	%ebx			# arg3
	pushl	%ecx			# arg2
	pushl	%edx			# arg1
	pushl	%eax			# syscall no.
	call	syscall
	# return value of 'syscall' should be in 'eax' by now
	addl	$24, %esp		# Pop off syscall no., arg1, arg2, arg3, arg4, arg5
	popl	%edx			# Pop off Return eip
	popl	%ecx			# Pop off Return esp
	sysexit

#endif	/* !NO_FAST_SYSCALL */

