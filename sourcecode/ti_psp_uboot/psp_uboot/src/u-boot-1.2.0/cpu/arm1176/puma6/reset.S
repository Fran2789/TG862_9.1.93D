/*
 * reset.S
 * Description:
 * Handle reset of CPU using watchdog reset.
 *
 *
 * Copyright (C) 2008, Texas Instruments, Incorporated
 *
 *  This program is free software; you can distribute it and/or modify it
 *  under the terms of the GNU General Public License (Version 2) as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *  for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
 */

/* 
 * Includes Intel Corporation's changes/modifications dated: 2011. 
 * Changed/modified portions - Copyright © 2011 , Intel Corporation.   
 */ 


#include <config.h>
#include <puma6.h>

	.align	5
.global reset_cpu
reset_cpu:
    /*-----------------------------------------------------------------
     * Set Watchdog Timer 
     *-----------------------------------------------------------------*/
    /* Unlock CHANGE register */
    ldr r1, =REG_WT_CHNG_LOCK
    ldr r0, =WT_CHNG_UNLOCK_WORD1
    str r0, [r1]
    /* Wait for UNLOCK stage 1  */
    mov r2, #0x20 
WaitChangeUnlock1Loop:
    ldr r0, [r1]
    cmp r0, #1
    beq ChangeUnlocked1
    sub r2, r2, #0x1
    cmp r2, #0x0
    bne WaitChangeUnlock1Loop    
ChangeUnlocked1:    
    cmp r2, #0x0
    beq ExitReset
    ldr r0, =WT_CHNG_UNLOCK_WORD2
    str r0, [r1]
    /* Wait for UNLOCK  */
    mov r2, #0x20 
WaitChangeUnlockLoop:
    ldr r0, [r1]
    cmp r0, #3
    beq ChangeUnlocked
    sub r2, r2, #0x1
    cmp r2, #0x0
    bne WaitChangeUnlockLoop    
    
ChangeUnlocked:    
    cmp r2, #0x0
    beq ExitReset
    ldr r1, =REG_WT_CHNG
    mov r0, #1
    str r0, [r1]

    /* Unlock DISABLE register */
    ldr r1, =REG_WT_DISABLE_LOCK
    ldr r0, =WT_DISABLE_UNLOCK_WORD1
    str r0, [r1]
    /* Wait for UNLOCK stage 1  */
    mov r2, #0x20 
WaitDisableUnlock1Loop:
    ldr r0, [r1]
    cmp r0, #1
    beq DisableUnlocked1
    sub r2, r2, #0x1
    cmp r2, #0x0
    bne WaitDisableUnlock1Loop    
DisableUnlocked1:    
    cmp r2, #0x0
    beq ExitReset
    ldr r0, =WT_DISABLE_UNLOCK_WORD2
    str r0, [r1]
    /* Wait for UNLOCK stage 2 */
    mov r2, #0x20 
WaitDisableUnlock2Loop:
    ldr r0, [r1]
    cmp r0, #2
    beq DisableUnlocked2
    sub r2, r2, #0x1
    cmp r2, #0x0
    bne WaitDisableUnlock2Loop    
DisableUnlocked2:
    cmp r2, #0x0
    beq ExitReset
    ldr r0, =WT_DISABLE_UNLOCK_WORD3
    str r0, [r1]
    /* Wait for UNLOCa */
    mov r2, #0x20 
WaitDisableUnlockLoop:
    ldr r0, [r1]
    cmp r0, #3
    beq DisableUnlocked
    sub r2, r2, #0x1
    cmp r2, #0x0
    bne WaitDisableUnlockLoop    

DisableUnlocked:    
    cmp r2, #0x0
    beq ExitReset
    ldr r1, =REG_WT_DISABLE
    mov r0, #1
    str r0, [r1]
    
ExitReset:
    bx lr

        

