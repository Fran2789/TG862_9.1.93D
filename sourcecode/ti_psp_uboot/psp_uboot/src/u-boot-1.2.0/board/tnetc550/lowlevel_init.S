/*
 * Board specific setup info
 *
 * (C) Copyright 2003-2004
 *
 * Texas Instruments, <www.ti.com>
 * Kshitij Gupta <Kshitij@ti.com>
 *
 * Modified for OMAP 1610 H2 board by Nishant Kamat, Jan 2004
 *
 * Modified for OMAP730 P2 Board by Dave Peverley, MPC-Data Limited
 * (http://www.mpc-data.co.uk)
 *
 * TODO : Tidy up and change to use system register defines
 *        from omap730.h where possible.
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
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
 * 1) MMSPI configuration
 * 2) Setup PLL
 * 3) Bring EMIF out of reset
 * 4) Relocate or setup arm vector base as per configuration
 * 5) Emulation platforms support.
 *
 * Derived from board/omap730p2/lowlevel_init.S
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

#include <config.h>
#include <version.h>

#ifndef CONFIG_PUMA5_VOLCANO_EMU
#include <puma5.h>
#endif

/* Define the following for enabling DFT module (for final testing on ATE) */
#undef DO_EMIF_VRST

_TEXT_BASE:
	.word	TEXT_BASE	/* sdram load addr from config.mk */

.globl lowlevel_init
lowlevel_init:
	/* Save callers address in r11 - r11 must never be modified */
	mov r11, lr

#ifndef CONFIG_PUMA5_VOLCANO_EMU
    
    /* Unlock CFG MMR region */
    ldr r1, =PUMA5_BOOTCFG_KICK_0
    ldr r0, =PUMA5_BOOTCFG_KICK_0_VAL
    str r0, [r1]
    ldr r1, =PUMA5_BOOTCFG_KICK_1
    ldr r0, =PUMA5_BOOTCFG_KICK_1_VAL
    str r0, [r1]

    /* Set MPEG In in PinMux0 */ 
    ldr r1, =REG_PIN_MUX0
    ldr r0, [r1]
    orr r0, #(1<<MPEG_IN_EN_BIT)
    str r0, [r1]

/* NOTE :
	The reset value of MMSPI Reg is set for NORMAL READ and when we boost
	the PLL's much earlier than flash driver init (which sets MMSPI Reg to
	FAST read mode),the access to flash becomes inconsistent. To aviod this 
    setting the MM-SPI register for FAST READ mode even before boosting the 
    PLL's 
*/
#ifndef CFG_NO_FLASH
    /* Set FASTREAD in  MMSPI  Reg0 */ 
    ldr r1, =REG_MM_SPI_SETUP0
    ldr r0, =MM_SPI_SETUP0_FR_VAL
    str r0, [r1]

    /* Set FASTREAD in MMSPI  Reg1 */
    ldr r1, =REG_MM_SPI_SETUP1
    ldr r0, =MM_SPI_SETUP1_FR_VAL
    str r0, [r1]
#endif   

#ifndef CFG_EXTERNAL_CLK
current0:
        adr r0, current0
        ldr r1, =current0
        cmp r0, r1
        beq skipemif 
#else
        b skipemif        
#endif

    /*  
     *  Apply EMIF reset before touching any PLL
     */
    ldr     r0, =REG_PSCMDCTL_DDREMIF
    ldr     r1, =0x100
    str     r1, [r0]

    /* Enable the Power Domain Transition Command */
    ldr     r0, =REG_PSCPTCMD
    mov     r1, #1
    str     r1, [r0]

    /* Check for Transition Complete(PTSTAT) */
    ldr     r0, =REG_PSCPTSTAT
CheckDDREMIFStateTrans1:
    ldr     r1, [r0]
    tst     r1, #0x1
    bne     CheckDDREMIFStateTrans1

#ifdef CONFIG_TI_PSPUBOOT_DDR_ASYNC_MODE
    /* Set ASYNC mode for EMIF Controller and DDR PHY  */
    ldr r1, =REG_DDR_CR
    ldr r0, [r1]
    orr r0, r0, #EMIF_DDR_DUAL_MODE
    str r0, [r1]
#endif

#ifndef CFG_EXTERNAL_CLK
    /*- ----------------------------------------------------------------
     * Setup ARM PLL 
     *-----------------------------------------------------------------*/
    
    /* Bypass : PLLEN=0 */
    ldr r1, =REG_ARMPLL_PLLCTL
    ldr r0, [r1]
    ldr r2, =PLL_BYPASS_MASK_N
    and r0, r0, r2
    str r0, [r1]

    /* Wait for few cycles (ideally 4 cycles of the slowest of the clocks
     * CLKIN/OSCIN, pll_clkout, to make sure the PLLEN mux switches properly to
     * the bypass clock.
     */
    mov r2, #0x20 
WaitARMPLLBypassLoop:
    sub r2, r2, #0x1
    cmp r2, #0x0
    bne WaitARMPLLBypassLoop    
                           
    /* PLLRST=0 */
    ldr r2, =PLL_RESET_MASK_N
    and r0, r0, r2
    str r0, [r1]

    /* Set PREDIV */
    ldr r1, =REG_ARMPLL_PREDIV
    ldr r0, [r1]
    ldr r2, =PLL_PREDIV_MASK_N
    and r0, r0, r2
    mov r2, #1 
    mov r2, r2, LSL #15
#ifdef CONFIG_TI_PSPUBOOT_DDR_ASYNC_MODE
    orr r2, r2, #ARMPLL_PREDIV
#endif    
    orr r0, r0, r2
    str r0, [r1]

    /* Program PLLM */
    ldr r1, =REG_ARMPLL_PLLM
    mov r0, #ARMPLL_PLLM
    str r0, [r1]

    /* 
     * 1) Program PLLDIVn (0-6) : 
     *          SYSCLK0 => /1 (ARM11)
     *          SYSCLK1 => /4 (VLYNQ)
     *          SYSCLK2 => /2 (FULL)
     *          SYSCLK3 => /4 (HALF)
     *          SYSCLK4 => /1 (DDR)
     *          SYSCLK5 => /2 (Packet Processor/Voice DSP)
     *          SYSCLK6 => /8 (Serial Flash)
     * 2) Set OSCDIV1
     * 3) GO
     */
    ldr r1, =REG_ARMPLL_PLLDIV1
    ldr r0, [r1]
    ldr r2, =PLL_DIV_MASK_N
    and r0, r0, r2
    mov r2, #1 
    mov r2, r2, LSL #15
    orr r2, r2, #ARMPLL_PLLDIV1
    orr r0, r0, r2
    str r0, [r1]
   
    ldr r1, =REG_ARMPLL_PLLDIV2
    ldr r0, [r1]
    ldr r2, =PLL_DIV_MASK_N
    and r0, r0, r2
    mov r2, #1 
    mov r2, r2, LSL #15
    orr r2, r2, #ARMPLL_PLLDIV2
    orr r0, r0, r2
    str r0, [r1]
   
    ldr r1, =REG_ARMPLL_PLLDIV3
    ldr r0, [r1]
    ldr r2, =PLL_DIV_MASK_N
    and r0, r0, r2
    mov r2, #1 
    mov r2, r2, LSL #15
    orr r2, r2, #ARMPLL_PLLDIV3
    orr r0, r0, r2
    str r0, [r1]
   
    ldr r1, =REG_ARMPLL_PLLDIV4
    ldr r0, [r1]
    ldr r2, =PLL_DIV_MASK_N
    and r0, r0, r2
    mov r2, #1 
    mov r2, r2, LSL #15
    orr r2, r2, #ARMPLL_PLLDIV4
    orr r0, r0, r2
    str r0, [r1]
    
    ldr r1, =REG_ARMPLL_PLLDIV5
    ldr r0, [r1]
    ldr r2, =PLL_DIV_MASK_N
    and r0, r0, r2
    mov r2, #1 
    mov r2, r2, LSL #15
    orr r2, r2, #ARMPLL_PLLDIV5
    orr r0, r0, r2
    str r0, [r1]
    
    ldr r1, =REG_ARMPLL_PLLDIV6
    ldr r0, [r1]
    ldr r2, =PLL_DIV_MASK_N
    and r0, r0, r2
    mov r2, #1 
    mov r2, r2, LSL #15
    orr r2, r2, #ARMPLL_PLLDIV6
    orr r0, r0, r2
    str r0, [r1]

    ldr r1, =REG_ARMPLL_PLLDIV7
    ldr r0, [r1]
    ldr r2, =PLL_DIV_MASK_N
    and r0, r0, r2
    mov r2, #1 
    mov r2, r2, LSL #15
    orr r2, r2, #ARMPLL_PLLDIV7
    orr r0, r0, r2
    str r0, [r1]

    /* Program the GOSET bit to take new divier values */
    ldr r0, =REG_ARMPLL_PLLCMD
    ldr r1, [r0] 
    orr r1, r1, #0x1 
    str r1, [r0]

    /* Wait for Done */
    ldr r1, =REG_ARMPLL_PLLSTAT
doneLoop: 
    ldr r0, [r1]
    and r0, r0, #0x1
    cmp r0, #0x0
    bne doneLoop

    /* Wait for PLL to Reset Properly */
    mov r0, #500 
ResetPLLLoop:
    sub r0, r0, #0x1
    cmp r0, #0x0
    bne ResetPLLLoop
    
    /* Deassert PLL Reset PLLRST=0 */
    ldr r1, =REG_ARMPLL_PLLCTL
    ldr r0, [r1]
    ldr r2, =PLL_UNRESET_MASK
    orr r0, r0, r2
    str r0, [r1]
    
    /* Wait for PLL to lock */
    mov r2, #2000
PLLLockLoop:
    sub r2, r2, #0x1
    cmp r2, #0x0
    bne PLLLockLoop

    /* Enable the PLL */
    ldr r2, =PLL_ENABLE_MASK
    orr r0, r0, r2 
    str r0, [r1]

#if (CONFIG_COMMANDS & CFG_CMD_NET) 
    /* XXX Disable CLK_OUT0 clock to avoid possible high jitter. Use external
     * PHY clock instead.
     */
#if 0    
     ldr r1, =REG_PUDCR0
     ldr r0, [r1]
     mov r2, #CLK_OUT0_DISABLE_MASK
     orr r0, r0, r2
     str r0, [r1]
#else
    /* Powerdown the CLO OUT0 interface */
     ldr r1, =REG_IO_PDCR
     ldr r0, [r1]
     mov r2, #CLK_OUT0_PWRDN
     orr r0, r0, r2
     str r0, [r1]
#endif
    
    /*-------------------------------------
     * Set GMII PLL
     * !@@ TODO: Assuming PLL bypass mode, in reset and directly setting MUL. Check
     * if OK
     * 
     * -----------------------------------/
    /* Bypass PLLEN */
#if 0
    ldr r1, =REG_GMIIPLL_PLLCTL
    ldr r0, [r1]
    ldr r2, =GMIIPLL_BYPASS_MASK_N
    and r0, r0, r2
    str r0, [r1]
    /* Wait for few cycles (ideally 4 cycles of the slowest of the clocks
     * CLKIN/OSCIN, pll_clkout, to make sure the PLLEN mux switches properly to
     * the bypass clock.
     */
    mov r2, #0x20 
WaitGMIIPLLBypassLoop:
    sub r2, r2, #0x1
    cmp r2, #0x0
    bne WaitGMIIPLLBypassLoop    
                           
    /* PLLRST */
    ldr r2, =GMIIPLL_RESET_MASK_N
    and r0, r0, r2
    str r0, [r1]
#else
    /* PLLRST */
    ldr r1, =REG_GMIIPLL_PLLCTL
    mov r2, #0
    str r0, [r1]
#endif

    ldr r1, =REG_GMIIPLL_PLLMULT
    mov r0, #GMIIPLL_PLLMULT
    str r0, [r1]

    ldr r1, =REG_GMIIPLL_PLLDIV
    mov r0, #GMIIPLL_PLLDIV
    mov r2, #1
    mov r2, r2, LSL #15
    orr r0, r0, r2
    str r0, [r1]
    
    /* Wait for PLL to Reset Properly */
    mov r0, #500 
ResetGMIIPLLLoop:
    sub r0, r0, #0x1
    cmp r0, #0x0
    bne ResetGMIIPLLLoop
    
    /* Deassert PLL Reset */
    ldr r1, =REG_GMIIPLL_PLLCTL
    ldr r0, [r1]
    ldr r2, =GMIIPLL_UNRESET_MASK
    orr r0, r0, r2
    str r0, [r1]
    
    /* Wait for PLL to lock */
    mov r2, #2000
GMIIPLLLockLoop:
    sub r2, r2, #0x1
    cmp r2, #0x0
    bne GMIIPLLLockLoop

    /* Disable bypass */
    ldr r2, =GMIIPLL_UNBYPASS_MASK
    orr r0, r0, r2
    str r0, [r1]

    /* Enable the PLL */
    ldr r2, =GMIIPLL_ENABLE_MASK_N
    and r0, r0, r2 
    str r0, [r1]

#endif /* CFG_CMD_NET */
#endif /* !CFG_EXTERNAL_CLK */
#endif /* !CONFIG_PUMA5_VOLCANO_EMU */

        /*-------------------------------------------------------------------
         * Memory system init (EMIF) 
         *-----------------------------------------------------------------*/
        
	/* Skip over EMIF initialization if running from SDRAM. */
#ifndef CFG_EXTERNAL_CLK
current:
        adr r0, current
        ldr r1, =current
        cmp r0, r1
        beq skipemif 
#else
        b skipemif        
#endif

#ifndef CONFIG_PUMA5_VOLCANO_EMU
    /*  
     *  Bring DDR PHY out of reset 
     */

    ldr     r0, =REG_PSCMDCTL_DDRPHY
    ldr     r1, [r0]
    ldr     r2, =0xffffffe0
    and     r1, r1, r2
    mov     r2, #1
    mov     r2, r2, LSL #8
    orr     r2, r2, #0x03
    orr     r1, r1, r2
    str     r1, [r0]

    /* Enable the Power Domain Transition Command */
    ldr     r0, =REG_PSCPTCMD
    mov     r1, #1
    str     r1, [r0]

    /* Check for Transition Complete(PTSTAT) */
    ldr     r0, =REG_PSCPTSTAT
CheckDDREMIFStateTrans3:
    ldr     r1, [r0]
    tst     r1, #0x1
    bne     CheckDDREMIFStateTrans3

    /*  
     *  Bring DDR EMIF out of reset 
     */
    ldr     r0, =REG_PSCMDCTL_DDREMIF
    ldr     r1, [r0]
    ldr     r2, =0xffffffe0
    and     r1, r1, r2
    mov     r2, #1
    mov     r2, r2, LSL #8
    orr     r2, r2, #0x03
    orr     r1, r1, r2
    str     r1, [r0]

#ifdef DO_EMIF_VRST /* For DFT module */
    ldr     r0, =REG_PSCMDCTL_EMIFVRST
    ldr     r1, [r0]
    ldr     r2, =0xffffffe0
    and     r1, r1, r2
    mov     r2, #1
    mov     r2, r2, LSL #8
    orr     r2, r2, #0x03
    orr     r1, r1, r2
    str     r1, [r0]
#endif

    /* Enable the Power Domain Transition (GO) Command */
    ldr     r0, =REG_PSCPTCMD
    mov     r1, #1
    str     r1, [r0]
    
    /* Check for Transition Complete(PTSTAT) */
    ldr     r0, =REG_PSCPTSTAT
CheckDDREMIFStateTrans4:
    ldr     r1, [r0]
    tst     r1, #0x1
    bne     CheckDDREMIFStateTrans4

#endif /* !CONFIG_PUMA5_VOLCANO_EMU */  
   
#ifdef CONFIG_PUMA5_VOLCANO_EMU
        adr     r0, s_initemif
        bl      DispStr  
#endif
        BL InitEmif
       
#ifdef CONFIG_PUMA5_VOLCANO_EMU
        adr     r0, s_emifdone
        bl      DispStr  
#endif

skipemif:

    bl GetSdramSize
    ldr r3, =PHYS_SDRAM_1
    str r0, [r3]

#ifdef CONFIG_RELOCATE_VEC_BASE
        bl SetVectorBase
#else
        /* TODO: copy vector table to SRAM (except for first 4 bytes) */
#endif
                  
#ifdef  CONFIG_USE_IRQ
	/* enable IRQ and FIQ */

	mrs	r4,	CPSR
	bic	r4,	r4, #IRQ_MASK
	bic	r4,	r4, #FIQ_MASK
	msr	CPSR,	r4
#endif

	/* back to arch calling code */
	mov	pc,	r11

	/* the literal pools origin */
	.ltorg

/* misc values */
.equ	IRQ_MASK,		0x80           /* IRQ mask value */
.equ	FIQ_MASK,		0x40           /* FIQ mask value */

/* Boost the SPI clock to 80MHz for capable Serial Flash devices */
.global BoostSpiClock
BoostSpiClock:
    ldr r1, =REG_ARMPLL_PLLDIV7
    ldr r0, [r1]
    ldr r2, =PLL_DIV_MASK_N
    and r0, r0, r2
    mov r2, #1 
    mov r2, r2, LSL #15
    orr r2, r2, #ARMPLL_PLLDIV7_BOOST
    orr r0, r0, r2
    str r0, [r1]

    /* Program the GOSET bit to take new divier values */
    ldr r0, =REG_ARMPLL_PLLCMD
    ldr r1, [r0] 
    orr r1, r1, #0x1 
    str r1, [r0]

    /* Wait for Done */
    ldr r1, =REG_ARMPLL_PLLSTAT
boostDoneLoop: 
    ldr r0, [r1]
    and r0, r0, #0x1
    cmp r0, #0x0
    bne boostDoneLoop
    bx  lr


#ifdef CONFIG_RELOCATE_VEC_BASE
.global SetVectorBase
SetVectorBase:
#ifdef CONFIG_SKIP_RELOCATE_UBOOT
        ldr     r1, =_start
        ldr     r0, =SetVectorBase
        sub     r0, r0, r1
        sub     r0, pc, r0
        sub     r1, r0, #20             /* 12 + 8 */
#else
        /* NOTE : We are changing the vector address to point to RAM but the
         * actual copy of vector table will happen later (in start.S). This is
         * even more risky when CONFIG_USE_IRQ is defined. Problems could be
         * avoided by enabling interrupts in board_init or respected peripheral
         * inits, if required.
         */

        ldr    r1, _TEXT_BASE                      /* Reclocated  vector table */
#endif
         mcr    p15, 0, r1, c12, c0, 0          /* Write Secure or Non-Secure Vector Base Address Register (c12) */

    /*
     * Clear V (bit13) in System Control Register to select Normal exception
     * vector location (the Vector Base Address Register determines the address
     * range).
     */
    
    mrc    p15, 0, r0, c1, c0, 0
    /* Read System Control Register */
    mov    r1, #1 
    bic    r0, r0, r1, LSL #13
    /* Clear V (bit13)  * */
    mcr    p15, 0, r0, c1, c0, 0
    /* Write updated value back */
    mov    pc,lr
                                                                         
#endif /* CONFIG_RELOCATE_VEC_BASE */


#ifdef CONFIG_PUMA5_VOLCANO_EMU
s_initemif:    .asciz   "initemif"
s_emifdone:    .asciz   "emifdone"
#endif

.align	4
.global DispStr

#ifdef CONFIG_PUMA5_VOLCANO_EMU
/******************************************************
 Routine: DispStr
  Description: Display specified string on ASCII display
  
  void DispStr(char *val)

  Destroyes:
    r1, r2, r3, r12 
  
*******************************************************/
DispStr:
    ldr     r2, ascii_start
    ldr     ip, ascii_end
    mov     r1, #32         
next_char:
    ldrb    r3, [r0]
    cmp     r3, #0
    strneb  r3, [r2]
#if 0
    streqb  r1, [r2]
#else
    bxeq    lr 
#endif
    addne   r0, r0, #1
    cmp     r2, ip
    add     r2, r2, #8
    bne     next_char
    bx      lr

ascii_start:        .word   (ASCII_DISP_BASE)        
ascii_end:          .word   ((ASCII_DISP_BASE)*(7*ASCII_DISP_OFFSET))

#else /* !CONFIG_PUMA5_VOLCANO_EMU */
/* Do nothing at this time */
DispStr:
    bx      lr
#endif /* CONFIG_PUMA5_VOLCANO_EMU */


