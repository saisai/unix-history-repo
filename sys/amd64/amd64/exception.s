/*-
 * Copyright (c) 1989, 1990 William F. Jolitz.
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <machine/asmacros.h>
#include <sys/mutex.h>
#include <machine/psl.h>
#include <machine/trap.h>

#include "assym.s"

#define	SEL_RPL_MASK	0x0003

	.text

/*****************************************************************************/
/* Trap handling                                                             */
/*****************************************************************************/
/*
 * Trap and fault vector routines.
 *
 * Most traps are 'trap gates', SDT_SYS386TGT.  A trap gate pushes state on
 * the stack that mostly looks like an interrupt, but does not disable 
 * interrupts.  A few of the traps we are use are interrupt gates, 
 * SDT_SYS386IGT, which are nearly the same thing except interrupts are
 * disabled on entry.
 *
 * The cpu will push a certain amount of state onto the kernel stack for
 * the current process.  The amount of state depends on the type of trap 
 * and whether the trap crossed rings or not.  See i386/include/frame.h.  
 * At the very least the current EFLAGS (status register, which includes 
 * the interrupt disable state prior to the trap), the code segment register,
 * and the return instruction pointer are pushed by the cpu.  The cpu 
 * will also push an 'error' code for certain traps.  We push a dummy 
 * error code for those traps where the cpu doesn't in order to maintain 
 * a consistent frame.  We also push a contrived 'trap number'.
 *
 * The cpu does not push the general registers, we must do that, and we 
 * must restore them prior to calling 'iret'.  The cpu adjusts the %cs and
 * %ss segment registers, but does not mess with %ds, %es, or %fs.  Thus we
 * must load them with appropriate values for supervisor mode operation.
 */
#define	IDTVEC(name)	ALIGN_TEXT; .globl __CONCAT(X,name); \
			.type __CONCAT(X,name),@function; __CONCAT(X,name):
#define	TRAP(a)		pushq $(a) ; jmp alltraps

MCOUNT_LABEL(user)
MCOUNT_LABEL(btrap)

IDTVEC(div)
	pushq $0; TRAP(T_DIVIDE)
IDTVEC(dbg)
	pushq $0; TRAP(T_TRCTRAP)
IDTVEC(nmi)
	pushq $0; TRAP(T_NMI)
IDTVEC(bpt)
	pushq $0; TRAP(T_BPTFLT)
IDTVEC(ofl)
	pushq $0; TRAP(T_OFLOW)
IDTVEC(bnd)
	pushq $0; TRAP(T_BOUND)
IDTVEC(ill)
	pushq $0; TRAP(T_PRIVINFLT)
IDTVEC(dna)
	pushq $0; TRAP(T_DNA)
IDTVEC(fpusegm)
	pushq $0; TRAP(T_FPOPFLT)
IDTVEC(tss)
	TRAP(T_TSSFLT)
IDTVEC(missing)
	TRAP(T_SEGNPFLT)
IDTVEC(stk)
	TRAP(T_STKFLT)
IDTVEC(prot)
	TRAP(T_PROTFLT)
IDTVEC(page)
	TRAP(T_PAGEFLT)
IDTVEC(mchk)
	pushq $0; TRAP(T_MCHK)
IDTVEC(rsvd)
	pushq $0; TRAP(T_RESERVED)
IDTVEC(fpu)
	pushq $0; TRAP(T_ARITHTRAP)
IDTVEC(align)
	TRAP(T_ALIGNFLT)
IDTVEC(xmm)
	pushq $0; TRAP(T_XMMFLT)
	
	/*
	 * alltraps entry point.  Interrupts are enabled if this was a trap
	 * gate (TGT), else disabled if this was an interrupt gate (IGT).
	 * Note that int0x80_syscall is a trap gate.  Only page faults
	 * use an interrupt gate.
	 */

	SUPERALIGN_TEXT
	.globl	alltraps
	.type	alltraps,@function
alltraps:
	subq	$TF_TRAPNO,%rsp		/* tf_err and tf_trapno already pushed */
	movq	%rdi,TF_RDI(%rsp)
	movq	%rsi,TF_RSI(%rsp)
	movq	%rdx,TF_RDX(%rsp)
	movq	%rcx,TF_RCX(%rsp)
	movq	%r8,TF_R8(%rsp)
	movq	%r9,TF_R9(%rsp)
	movq	%rax,TF_RAX(%rsp)
	movq	%rbx,TF_RBX(%rsp)
	movq	%rbp,TF_RBP(%rsp)
	movq	%r10,TF_R10(%rsp)
	movq	%r11,TF_R11(%rsp)
	movq	%r12,TF_R12(%rsp)
	movq	%r13,TF_R13(%rsp)
	movq	%r14,TF_R14(%rsp)
	movq	%r15,TF_R15(%rsp)
alltraps_with_regs_pushed:
	FAKE_MCOUNT(13*4(%rsp))
calltrap:
	FAKE_MCOUNT(btrap)		/* init "from" btrap -> calltrap */
	call	trap
	MEXITCOUNT
	jmp	doreti			/* Handle any pending ASTs */

/*
 * Call gate entry for FreeBSD ELF and Linux/NetBSD syscall (int 0x80)
 *
 * Even though the name says 'int0x80', this is actually a TGT (trap gate)
 * rather then an IGT (interrupt gate).  Thus interrupts are enabled on
 * entry just as they are for a normal syscall.
 *
 * This leaves a place to put eflags so that the call frame can be
 * converted to a trap frame. Note that the eflags is (semi-)bogusly
 * pushed into (what will be) tf_err and then copied later into the
 * final spot. It has to be done this way because esp can't be just
 * temporarily altered for the pushfl - an interrupt might come in
 * and clobber the saved cs/eip.
 */
	SUPERALIGN_TEXT
IDTVEC(int0x80_syscall)
	pushq	$2			/* sizeof "int 0x80" */
	subq	$TF_ERR,%rsp		/* skip over tf_trapno */
	movq	%rdi,TF_RDI(%rsp)
	movq	%rsi,TF_RSI(%rsp)
	movq	%rdx,TF_RDX(%rsp)
	movq	%rcx,TF_RCX(%rsp)
	movq	%r8,TF_R8(%rsp)
	movq	%r9,TF_R9(%rsp)
	movq	%rax,TF_RAX(%rsp)
	movq	%rbx,TF_RBX(%rsp)
	movq	%rbp,TF_RBP(%rsp)
	movq	%r10,TF_R10(%rsp)
	movq	%r11,TF_R11(%rsp)
	movq	%r12,TF_R12(%rsp)
	movq	%r13,TF_R13(%rsp)
	movq	%r14,TF_R14(%rsp)
	movq	%r15,TF_R15(%rsp)
	FAKE_MCOUNT(13*4(%rsp))
	call	syscall
	MEXITCOUNT
	jmp	doreti

/*
 * Fast syscall entry point.  We enter here with just our new %cs/%ss set,
 * and the new privilige level.  We are still running on the old user stack
 * pointer.  We have to juggle a few things around to find our stack etc.
 * swapgs gives us access to our PCPU space only.
 * XXX The PCPU stuff is stubbed out right now...
 */
IDTVEC(fast_syscall)
	#swapgs
	movq	%rsp,PCPU(SCRATCH_RSP)
	movq	common_tss+COMMON_TSS_RSP0,%rsp
	sti
	/* Now emulate a trapframe. Ugh. */
	subq	$TF_SIZE,%rsp
	movq	$KUDSEL,TF_SS(%rsp)
	/* defer TF_RSP till we have a spare register */
	movq	%r11,TF_RFLAGS(%rsp)
	movq	$KUCSEL,TF_CS(%rsp)
	movq	%rcx,TF_RIP(%rsp)	/* %rcx original value is in %r10 */
	movq	$2,TF_ERR(%rsp)
	movq	%rdi,TF_RDI(%rsp)	/* arg 1 */
	movq	%rsi,TF_RSI(%rsp)	/* arg 2 */
	movq	%rdx,TF_RDX(%rsp)	/* arg 3 */
	movq	%r10,TF_RCX(%rsp)	/* arg 4 */
	movq	%r8,TF_R8(%rsp)		/* arg 5 */
	movq	%r9,TF_R9(%rsp)		/* arg 6 */
	movq	%rax,TF_RAX(%rsp)	/* syscall number */
	movq	%rbx,TF_RBX(%rsp)	/* C preserved */
	movq	%rbp,TF_RBP(%rsp)	/* C preserved */
	movq	%r12,TF_R12(%rsp)	/* C preserved */
	movq	%r13,TF_R13(%rsp)	/* C preserved */
	movq	%r14,TF_R14(%rsp)	/* C preserved */
	movq	%r15,TF_R15(%rsp)	/* C preserved */
	movq	PCPU(SCRATCH_RSP),%r12	/* %r12 already saved */
	movq	%r12,TF_RSP(%rsp)	/* user stack pointer */
	call	syscall
	movq	PCPU(CURPCB),%rax
	testq	$PCB_FULLCTX,PCB_FLAGS(%rax)
	jne	3f
	/* simplified from doreti */
1:	/* Check for and handle AST's on return to userland */
	cli
	movq	PCPU(CURTHREAD),%rax
	testl	$TDF_ASTPENDING | TDF_NEEDRESCHED,TD_FLAGS(%rax)
	je	2f
	sti
	movq	%rsp, %rdi
	call	ast
	jmp	1b
2:	/* restore preserved registers */
	movq	TF_RDI(%rsp),%rdi	/* bonus; preserve arg 1 */
	movq	TF_RSI(%rsp),%rsi	/* bonus: preserve arg 2 */
	movq	TF_RDX(%rsp),%rdx	/* return value 2 */
	movq	TF_RAX(%rsp),%rax	/* return value 1 */
	movq	TF_RBX(%rsp),%rbx	/* C preserved */
	movq	TF_RBP(%rsp),%rbp	/* C preserved */
	movq	TF_R12(%rsp),%r12	/* C preserved */
	movq	TF_R13(%rsp),%r13	/* C preserved */
	movq	TF_R14(%rsp),%r14	/* C preserved */
	movq	TF_R15(%rsp),%r15	/* C preserved */
	movq	TF_RFLAGS(%rsp),%r11	/* original %rflags */
	movq	TF_RIP(%rsp),%rcx	/* original %rip */
	movq	TF_RSP(%rsp),%r9	/* user stack pointer */
	movq	%r9,%rsp		/* original %rsp */
	#swapgs
	sysretq
3:	/* Requested full context restore, use doreti for that */
	andq	$~PCB_FULLCTX,PCB_FLAGS(%rax)
	jmp	doreti

/*
 * Here for CYA insurance, in case a "syscall" instruction gets
 * issued from 32 bit compatability mode. MSR_CSTAR has to point
 * to *something* if EFER_SCE is enabled.
 */
IDTVEC(fast_syscall32)
	sysret

ENTRY(fork_trampoline)
	movq	%r12, %rdi		/* function */
	movq	%rbx, %rsi		/* arg1 */
	movq	%rsp, %rdx		/* trapframe pointer */
	call	fork_exit
	MEXITCOUNT
	jmp	doreti			/* Handle any ASTs */


/*
 * Include what was once config+isa-dependent code.
 * XXX it should be in a stand-alone file.  It's still icu-dependent and
 * belongs in i386/isa.
 */
#include "amd64/isa/vector.s"

	.data
	ALIGN_DATA

/*
 * void doreti(struct trapframe)
 *
 * Handle return from interrupts, traps and syscalls.
 */
	.text
	SUPERALIGN_TEXT
	.type	doreti,@function
doreti:
	FAKE_MCOUNT(bintr)		/* init "from" bintr -> doreti */
	/*
	 * Check if ASTs can be handled now.
	 */
	testb	$SEL_RPL_MASK,TF_CS(%rsp) /* are we returning to user mode? */
	jz	doreti_exit		/* can't handle ASTs now if not */

doreti_ast:
	/*
	 * Check for ASTs atomically with returning.  Disabling CPU
	 * interrupts provides sufficient locking evein the SMP case,
	 * since we will be informed of any new ASTs by an IPI.
	 */
	cli
	movq	PCPU(CURTHREAD),%rax
	testl	$TDF_ASTPENDING | TDF_NEEDRESCHED,TD_FLAGS(%rax)
	je	doreti_exit
	sti
	movq	%rsp, %rdi			/* pass a pointer to the trapframe */
	call	ast
	jmp	doreti_ast

	/*
	 * doreti_exit:	pop registers, iret.
	 *
	 *	The segment register pop is a special case, since it may
	 *	fault if (for example) a sigreturn specifies bad segment
	 *	registers.  The fault is handled in trap.c.
	 */
doreti_exit:
	MEXITCOUNT

	movq	TF_RDI(%rsp),%rdi
	movq	TF_RSI(%rsp),%rsi
	movq	TF_RDX(%rsp),%rdx
	movq	TF_RCX(%rsp),%rcx
	movq	TF_R8(%rsp),%r8
	movq	TF_R9(%rsp),%r9
	movq	TF_RAX(%rsp),%rax
	movq	TF_RBX(%rsp),%rbx
	movq	TF_RBP(%rsp),%rbp
	movq	TF_R10(%rsp),%r10
	movq	TF_R11(%rsp),%r11
	movq	TF_R12(%rsp),%r12
	movq	TF_R13(%rsp),%r13
	movq	TF_R14(%rsp),%r14
	movq	TF_R15(%rsp),%r15
	addq	$TF_RIP,%rsp		/* skip over tf_err, tf_trapno */
	.globl	doreti_iret
doreti_iret:
	iretq

  	/*
	 * doreti_iret_fault and friends.  Alternative return code for
	 * the case where we get a fault in the doreti_exit code
	 * above.  trap() (i386/i386/trap.c) catches this specific
	 * case, sends the process a signal and continues in the
	 * corresponding place in the code below.
	 */
	ALIGN_TEXT
	.globl	doreti_iret_fault
doreti_iret_fault:
	subq	$TF_RIP,%rsp		/* space including tf_err, tf_trapno */
	movq	%rdi,TF_RDI(%rsp)
	movq	%rsi,TF_RSI(%rsp)
	movq	%rdx,TF_RDX(%rsp)
	movq	%rcx,TF_RCX(%rsp)
	movq	%r8,TF_R8(%rsp)
	movq	%r9,TF_R9(%rsp)
	movq	%rax,TF_RAX(%rsp)
	movq	%rbx,TF_RBX(%rsp)
	movq	%rbp,TF_RBP(%rsp)
	movq	%r10,TF_R10(%rsp)
	movq	%r11,TF_R11(%rsp)
	movq	%r12,TF_R12(%rsp)
	movq	%r13,TF_R13(%rsp)
	movq	%r14,TF_R14(%rsp)
	movq	%r15,TF_R15(%rsp)
	movq	$T_PROTFLT,TF_TRAPNO(%rsp)
	movq	$0,TF_ERR(%rsp)	/* XXX should be the error code */
	jmp	alltraps_with_regs_pushed

#include "amd64/isa/icu_ipl.s"
