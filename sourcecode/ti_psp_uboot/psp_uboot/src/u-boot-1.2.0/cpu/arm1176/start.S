/*
 *  armboot - Startup Code for OMP2420/ARM1136 CPU-core
 *
 *  Copyright (c) 2004    Texas Instruments <r-woodruff2@ti.com>
 *
 *  Copyright (c) 2001    Marius Gröger <mag@sysgo.de>
 *  Copyright (c) 2002    Alex Züpke <azu@sysgo.de>
 *  Copyright (c) 2002    Gary Jennejohn <gj@denx.de>
 *  Copyright (c) 2003    Richard Woodruff <r-woodruff2@ti.com>
 *  Copyright (c) 2003    Kshitij <kshitij@ti.com>
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.     See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

/* Copyright 2008, Texas Instruments Incorporated
 *
 * This program has been modified from its original operation by Texas Instruments
 * to do the following:
 * 1) Updated for Puma5 and Emulation Platforms.
 * 2) Handle watdog (cpu-external) reset
 * 3) arm1176 specific mods
 * 4) Exception handlers decared "global"
 *
 * Derived from cpu/arm1136/start.S
 * 
 * THIS MODIFIED SOFTWARE AND DOCUMENTATION ARE PROVIDED
 * "AS IS," AND TEXAS INSTRUMENTS MAKES NO REPRESENTATIONS
 * OR WARRENTIES, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
 * TO, WARRANTIES OF MERCHANTABILITY OR FITNESS FOR ANY
 * PARTICULAR PURPOSE OR THAT THE USE OF THE SOFTWARE OR
 * DOCUMENTATION WILL NOT INFRINGE ANY THIRD PARTY PATENTS,
 * COPYRIGHTS, TRADEMARKS OR OTHER RIGHTS.
 * See The GNU General Public License for more details.
 *
 * These changes are covered under version 2 of the GNU General Public License,
 * dated June 1991.
 */


/* 
 * Includes Intel Corporation's changes/modifications dated: 2011. 
 * Changed/modified portions - Copyright © 2011 , Intel Corporation.   
 */ 

#include <config.h>

.globl _start
_start: 
    b    reset
    ldr    pc, _undefined_instruction
    ldr    pc, _software_interrupt
    ldr    pc, _prefetch_abort
    ldr    pc, _data_abort
    ldr    pc, _not_used
    ldr    pc, _irq
    ldr    pc, _fiq

_undefined_instruction: .word undefined_instruction
_software_interrupt:    .word software_interrupt
_prefetch_abort:        .word prefetch_abort
_data_abort:            .word data_abort
_not_used:              .word not_used
_irq:                   .word irq
_fiq:                   .word fiq
_pad:                   .word 0x12345678 /* now 16*4=64 */

.global _end_vect
_end_vect:

    .balignl 16,0xdeadbeef
/*
 *************************************************************************
 *
 * Startup Code (reset vector)
 *
 * do important init only if we don't start from memory!
 * setup Memory and board specific bits prior to relocation.
 * relocate armboot to ram
 * setup stack
 *
 *************************************************************************
 */
#ifdef START_S_DEBUG
#define DEBUG_INT_OFFSEET 0x00001F00
#endif

#ifdef CONFIG_DYNAMIC_BASE_ADDRESS
#define  DYN_BASE_ADDR
#endif

_TEXT_BASE:
    .word    TEXT_BASE

/* Code start base address */
.globl _armboot_start
_armboot_start:
    .word _start

/*
 * These are defined in the board-specific linker script.
 */
.globl _bss_start
_bss_start:
    .word __bss_start

.globl _bss_end
_bss_end:
    .word _end

#ifdef DYN_BASE_ADDR
/*
 * Subtracting _start from them lets the linker put their
 * relative position in the executable instead of leaving
 * them null.
*/
.globl _bss_start_ofs
_bss_start_ofs:
    .word __bss_start - _start

.globl _bss_end_ofs
_bss_end_ofs:
       .word _end - _start
#endif

#ifdef CONFIG_USE_IRQ
/* IRQ stack memory (calculated at run-time) */
.globl IRQ_STACK_START
IRQ_STACK_START:
    .word    0x0badc0de

/* IRQ stack memory (calculated at run-time) */
.globl FIQ_STACK_START
FIQ_STACK_START:
    .word 0x0badc0de
#endif

/*
 * the actual reset code
 */

reset:
#ifdef START_S_DEBUG
        mov    r2, #DEBUG_INT_OFFSEET
        mov    r3, #0xFF
        str    r3, [r2]
#endif

/*
 * set the cpu to SVC32 mode
     */
    mrs    r0,cpsr
    bic    r0,r0,#0x1f
    orr    r0,r0,#0xd3
    msr    cpsr,r0

#ifndef CONFIG_SKIP_CPU_INIT
        /* mmu, cache stuff, board level init etc */
    bl  cpu_init_crit
#endif

#ifndef CONFIG_SKIP_RELOCATE_UBOOT
relocate:                   /* relocate U-Boot to RAM                */
    adr    r0, _start       /* r0 <- Load current position of code   */
    ldr    r1, _TEXT_BASE   /* r1 <- Store Base address              */
    cmp    r0, r1           /* test if we run from flash or RAM      */
    beq    stack_setup      /* don't reloc during debug              */

    ldr    r2, _armboot_start
    ldr    r3, _bss_start
    sub    r2, r3, r2       /* r2 <- size of armboot                 */
    add    r2, r0, r2       /* r2 <- source end address              */

copy_loop:
    ldmia    r0!, {r3-r10}  /* copy from source address [r0]         */
    stmia    r1!, {r3-r10}  /* copy to   target address [r1]         */
    cmp    r0, r2           /* until source end addreee [r2]         */
    ble    copy_loop
#endif    /* CONFIG_SKIP_RELOCATE_UBOOT */

#ifdef DYN_BASE_ADDR
.globl  relocate_code
relocate_code:
    adr     r7, _start  /* save addr of destination */

    ldr     r0, _TEXT_BASE          /* r0 <- Text base */
    sub     r9, r7, r0              /* r9 <- relocation offset */

    /*
    * fix .rel.dyn relocations
    */

    adr     r0, _start
    ldr     r10, _dynsym_start_ofs  /* r10 <- sym table ofs */
    add     r10, r10, r0            /* r10 <- sym table in Run-time */
    ldr     r2, _rel_dyn_start_ofs  /* r2 <- rel dyn start ofs */
    add     r2, r2, r0              /* r2 <- rel dyn start in Run-time */
    ldr     r3, _rel_dyn_end_ofs    /* r3 <- rel dyn end ofs */
    add     r3, r3, r0              /* r3 <- rel dyn end in Run-time */
    fixloop:
    ldr     r0, [r2]        /* r0 <- location to fix up, in Run-time */
    add     r0, r9          /* r0 <- location to fix up in RAM */
    ldr     r1, [r2, #4]
    and     r8, r1, #0xff
    cmp     r8, #23         /* relative fixup? */
    beq     fixrel
    cmp     r8, #2          /* absolute fixup? */
    beq     fixabs
    /* ignore unknown type of fixup */
    b       fixnext
    fixabs:
    /* absolute fix: set location to (offset) symbol value */
    mov     r1, r1, LSR #4          /* r1 <- symbol index in .dynsym */
    add     r1, r10, r1             /* r1 <- address of symbol in table */
    ldr     r1, [r1, #4]            /* r1 <- symbol value */
    add     r1, r9                  /* r1 <- relocated sym addr */
    b       fixnext
    fixrel:
    /* relative fix: increase location by offset */
    ldr     r1, [r0]
    add     r1, r1, r9
    fixnext:
    str     r1, [r0]
    add     r2, r2, #8      /* each rel.dyn entry is 8 bytes */
    cmp     r2, r3
    blo     fixloop
#endif

    /* Set up the stack                                               */
stack_setup:
#ifndef DYN_BASE_ADDR
    ldr    r0, _TEXT_BASE           /* upper 128 KiB: relocated uboot */
#else
    adr    r0, _start
#endif
    sub    r0, r0, #CFG_MALLOC_LEN    /* malloc area                  */
    sub    r0, r0, #CFG_GBL_DATA_SIZE /* bdinfo                       */
#ifdef CONFIG_USE_IRQ
    sub    r0, r0, #(CONFIG_STACKSIZE_IRQ+CONFIG_STACKSIZE_FIQ)
#endif
    sub    sp, r0, #12            /* leave 3 words for abort-stack    */

clear_bss:

#ifndef DYN_BASE_ADDR
    ldr    r0, _bss_start         /* find start of bss segment        */
    ldr    r1, _bss_end           /* stop here                        */
#else
    adr     r2, _start
    ldr     r0, _bss_start_ofs      /* find start of bss segment        */
    add     r0, r0, r2
    str     r0, _bss_start
    ldr     r1, _bss_end_ofs        /* stop here                        */
    add     r1, r1, r2
    str     r1, _bss_end
#endif

    mov    r2, #0x00000000        /* clear                            */
#if 0 /* BUG: This BSS clear is not working if the BSS size is zero !!  */
clbss_l:str    r2, [r0]           /* clear loop...                    */
    add    r0, r0, #4
    cmp    r0, r1
    bne    clbss_l
#endif

clbss_loop:                    
	cmp	r0, r1
	beq	clbss_end
    str	r2, [r0]		    
	add	r0, r0, #4
    b   clbss_loop

clbss_end:
#ifdef START_S_DEBUG
        mov    r2, #DEBUG_INT_OFFSEET
        mov    r3, #0xA1
        str    r3, [r2]
#endif

#ifdef DYN_BASE_ADDR
    ldr     r0, _start_armboot_ofs
    adr     r1, _start
    add     r0, r0, r1
    str     r0, _start_armboot
#endif
    ldr    pc, _start_armboot

_start_armboot: .word start_armboot

#ifdef DYN_BASE_ADDR
_start_armboot_ofs: .word start_armboot - _start

_rel_dyn_start_ofs:
    .word __rel_dyn_start - _start
_rel_dyn_end_ofs:
    .word __rel_dyn_end - _start
_dynsym_start_ofs:
    .word __dynsym_start - _start
#endif

/*
 *************************************************************************
 *
 * CPU_init_critical registers
 *
 * setup important registers
 * setup memory timing
 *
 *************************************************************************
 */
cpu_init_crit:
    /*
     * flush v4 I/D caches
     */
    mov    r0, #0
    mcr    p15, 0, r0, c7, c7, 0    /* flush v3/v4 cache */
    mcr    p15, 0, r0, c8, c7, 0    /* flush v4 TLB */

    /*
     * disable MMU stuff and caches
     */
    mrc    p15, 0, r0, c1, c0, 0
    bic    r0, r0, #0x00002300    @ clear bits 13, 9:8 (--V- --RS)
    bic    r0, r0, #0x00000007    @ clear bits 2:0 (-CAM)
    orr    r0, r0, #0x00000002    @ set bit 2 (A) Align
    orr    r0, r0, #0x00001000    @ set bit 12 (I) I-Cache
    mcr    p15, 0, r0, c1, c0, 0
      
    mov    r7, lr        /* persevere link reg across call */
        
#ifndef CONFIG_SKIP_LOWLEVEL_INIT 
    /*
     * Jump to board specific initialization... The Mask ROM will have already initialized
     * basic memory.  Go here to bump up clock rate and handle wake up conditions.
     */
    bl    lowlevel_init    /* go setup pll,mux,memory */

#else

    /*
     * Set Exception Vector Base Address
     */
#ifndef DYN_BASE_ADDR
    ldr    r1, _TEXT_BASE              /* Get vector base address */
#else
    adr    r1, _start                  /* Get vector base address */
#endif
    mcr    p15, 0, r1, c12, c0, 0      /* Write Vector Base Address Reg*/

    /*
     * Clear V (bit13) in System Control Register to select Normal 
     * exception vector location, the Vector Base Address Registers
     * determine the address range.
     */
    mrc    p15, 0, r0, c1, c0, 0   /* Read System Control Register */
    mov    r1, #1 
    bic    r0, r0, r1, LSL #13     /* Clear V (bit13)  */
    mcr    p15, 0, r0, c1, c0, 0   /* Write back updated value */

#ifdef  CONFIG_USE_IRQ
    /* 
     * enable IRQ and FIQ
     */
    mrs    r4,    CPSR
    bic    r4,    r4, #IRQ_MASK
    bic    r4,    r4, #FIQ_MASK
    msr    CPSR,    r4

#endif /*CONFIG_USE_IRQ*/
#endif /* CONFIG_SKIP_LOWLEVEL_INIT */

    mov    lr, r7        /* restore link */
    mov    pc, lr        /* back to my caller */

/*
 *************************************************************************
 *
 * Interrupt handling
 *
 *************************************************************************
 */
@
@ IRQ stack frame.
@
#define S_FRAME_SIZE    72

#define S_OLD_R0    68
#define S_PSR        64
#define S_PC        60
#define S_LR        56
#define S_SP        52

#define S_IP        48
#define S_FP        44
#define S_R10        40
#define S_R9        36
#define S_R8        32
#define S_R7        28
#define S_R6        24
#define S_R5        20
#define S_R4        16
#define S_R3        12
#define S_R2        8
#define S_R1        4
#define S_R0        0

#define MODE_SVC 0x13
#define I_BIT     0x80

/*
 * use bad_save_user_regs for abort/prefetch/undef/swi ...
 * use irq_save_user_regs / irq_restore_user_regs for IRQ/FIQ handling
 */

    .macro bad_save_user_regs
    sub    sp, sp, #S_FRAME_SIZE        @ carve out a frame on current user stack
    stmia  sp, {r0 - r12}               @ Save user registers (now in svc mode) r0-r12

    ldr    r2, _armboot_start
    sub    r2, r2, #(CFG_MALLOC_LEN)
    sub    r2, r2, #(CFG_GBL_DATA_SIZE+8) @ set base 2 words into abort stack
    ldmia  r2, {r2 - r3}                 @ get values for "aborted" pc and cpsr (into parm regs)
    add    r0, sp, #S_FRAME_SIZE         @ grab pointer to old stack

    add    r5, sp, #S_SP
    mov    r1, lr
    stmia  r5, {r0 - r3}                 @ save sp_SVC, lr_SVC, pc, cpsr
    mov    r0, sp                        @ save current stack into r0 (param register)
    .endm

    .macro irq_save_user_regs
    sub    sp, sp, #S_FRAME_SIZE
    stmia  sp, {r0 - r12}                @ Calling r0-r12
    add    r8, sp, #S_PC                 @ !!!! R8 NEEDS to be saved !!!! a reserved stack spot would be good.
    stmdb  r8, {sp, lr}^                 @ Calling SP, LR
    str    lr, [r8, #0]                  @ Save calling PC
    mrs    r6, spsr
    str    r6, [r8, #4]                  @ Save CPSR
    str    r0, [r8, #8]                  @ Save OLD_R0
    mov    r0, sp
    .endm

    .macro irq_restore_user_regs
    ldmia  sp, {r0 - lr}^                @ Calling r0 - lr
    mov    r0, r0
    ldr    lr, [sp, #S_PC]               @ Get PC
    add    sp, sp, #S_FRAME_SIZE
    subs   pc, lr, #4                    @ return & move spsr_svc into cpsr
    .endm

    .macro get_bad_stack
    ldr    r13, _armboot_start        @ setup our mode stack (enter in banked mode)
    sub    r13, r13, #(CFG_MALLOC_LEN)    @ move past malloc pool
    sub    r13, r13, #(CFG_GBL_DATA_SIZE+8) @ move to reserved a couple spots for abort stack

    str    lr, [r13]                @ save caller lr in position 0 of saved stack
    mrs    lr, spsr                 @ get the spsr
    str    lr, [r13, #4]            @ save spsr in position 1 of saved stack

    mov    r13, #MODE_SVC           @ prepare SVC-Mode
    @ msr  spsr_c, r13
    msr    spsr, r13                @ switch modes, make sure moves will execute
    mov    lr, pc                   @ capture return pc
    movs   pc, lr                   @ jump to next instruction & switch modes.
    .endm

    .macro get_bad_stack_swi
    sub    r13, r13, #4              @ space on current stack for scratch reg.
    str    r0, [r13]                 @ save R0's value.
    ldr    r0, _armboot_start        @ get data regions start
    sub    r0, r0, #(CFG_MALLOC_LEN)    @ move past malloc pool
    sub    r0, r0, #(CFG_GBL_DATA_SIZE+8)    @ move past gbl and a couple spots for abort stack
    str    lr, [r0]                 @ save caller lr in position 0 of saved stack
    mrs    r0, spsr                 @ get the spsr
    str    lr, [r0, #4]             @ save spsr in position 1 of saved stack
    ldr    r0, [r13]                @ restore r0
    add    r13, r13, #4             @ pop stack entry
    .endm

    .macro get_irq_stack            @ setup IRQ stack
    ldr    sp, IRQ_STACK_START
    .endm

    .macro get_fiq_stack            @ setup FIQ stack
    ldr    sp, FIQ_STACK_START
    .endm

/*
 * exception handlers
 */
.global undefined_instruction
    .align    5
undefined_instruction:
    get_bad_stack
    bad_save_user_regs
    bl    do_undefined_instruction

.global software_interrupt
    .align    5
software_interrupt:
#ifdef START_S_DEBUG
    /* TEMP DEBUG */
    mov    r2, #DEBUG_INT_OFFSEET
    mov    r3, #0xF2
    str    r3, [r2]
#endif
    get_bad_stack_swi
    bad_save_user_regs
    bl    do_software_interrupt

.global prefetch_abort
    .align    5
prefetch_abort:
#ifdef START_S_DEBUG
    /* TEMP DEBUG */
    mov    r2, #DEBUG_INT_OFFSEET
    mov    r3, #0xF3
    str    r3, [r2]
#endif
    get_bad_stack
    bad_save_user_regs
    bl    do_prefetch_abort

.global data_abort
    .align    5
data_abort:
#ifdef START_S_DEBUG
    /* TEMP DEBUG */
    mov    r2, #DEBUG_INT_OFFSEET
    mov    r3, #0xF4
    str    r3, [r2]
#endif
    get_bad_stack
    bad_save_user_regs
    bl    do_data_abort

.global not_used
    .align    5
not_used:
#ifdef START_S_DEBUG
    /* TEMP DEBUG */
    mov    r2, #DEBUG_INT_OFFSEET
    mov    r3, #0xF5
    str    r3, [r2]
#endif
    get_bad_stack
    bad_save_user_regs
    bl    do_not_used

#ifdef CONFIG_USE_IRQ
    .align    5
irq:
#ifdef START_S_DEBUG
    /* TEMP DEBUG */
    mov    r2, #DEBUG_INT_OFFSEET
    mov    r3, #0xF6
    str    r3, [r2]
#endif
    get_irq_stack
    irq_save_user_regs
    bl    do_irq
    irq_restore_user_regs

    .align    5
fiq:
#ifdef START_S_DEBUG
    /* TEMP DEBUG */
    mov    r2, #DEBUG_INT_OFFSEET
    mov    r3, #0xF7
    str    r3, [r2]

#endif
    get_fiq_stack
    /* someone ought to write a more effiction fiq_save_user_regs */
    irq_save_user_regs
    bl    do_fiq
    irq_restore_user_regs

#else
.global irq
    .align    5
irq:
#ifdef START_S_DEBUG
    /* TEMP DEBUG */
    mov    r2, #DEBUG_INT_OFFSEET
    mov    r3, #0xF8
    str    r3, [r2]
#endif
    get_bad_stack
    bad_save_user_regs
    bl    do_irq

.global fiq
    .align    5
fiq:
#ifdef START_S_DEBUG
    /* TEMP DEBUG */
    mov    r2, #DEBUG_INT_OFFSEET
    mov    r3, #0xF9
    str    r3, [r2]
#endif
    get_bad_stack
    bad_save_user_regs
    bl    do_fiq

#endif

        .align 5
.global arm1176_cache_flush
arm1176_cache_flush:
#ifdef START_S_DEBUG
    /* TEMP DEBUG */
    mov    r2, #DEBUG_INT_OFFSEET
    mov    r3, #0xFA
    str    r3, [r2]
#endif

    mcr  p15, 0, r1, c7, c5, 0    @ invalidate I cache
    mov  pc, lr                   @ back to caller


#if !defined CONFIG_TNETC550 && !defined(CONFIG_HARBORPARK)
    .align    5
.global reset_cpu
reset_cpu:
#ifdef START_S_DEBUG
    /* TEMP DEBUG */
    mov    r2, #DEBUG_INT_OFFSEET
    mov    r3, #0xFB
    str    r3, [r2]
#endif
        /* TODO : Put reset logic here */
_loop_forever:
    b    _loop_forever
#else
  
        /* Nothing done here: reset_cpu is handled SOC specific */
#endif /* CONFIG_TNETC550 */
        
        

