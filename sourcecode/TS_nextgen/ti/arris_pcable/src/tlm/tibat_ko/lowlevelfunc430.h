/*==========================================================================*\
|                                                                            |
| LowLevelFunc430.h                                                          |
|                                                                            |
| Low Level function prototypes, macros, and pin-to-signal assignments       |
| regarding to user's hardware                                               |
|----------------------------------------------------------------------------|
| Project:              MSP430 Replicator                                    |
| Developed using:      IAR Embedded Workbench 3.40B [Kickstart]             |
|             and:      Code Composer Eessentials 2.0                        |
|----------------------------------------------------------------------------|
| Author:               FRGR                                                 |
| Version:              1.9                                                  |
| Initial Version:      04-17-02                                             |
| Last Change:          05-24-12                                             |
|----------------------------------------------------------------------------|
| Version history:                                                           |
| 1.0 04/02 FRGR        Initial version.                                     |
| 1.1 04/02 FRGR        Included SPI mode to speed up shifting function by 2.|
|                       (JTAG control now on Port5)                          |
| 1.2 06/02 ALB2        Formatting changes, added comments. Removed code used|
|                       for debug purposes during development.               |
| 1.3 08/02 ALB2        Initial code release with Lit# SLAA149.              |
| 1.4 09/05 SUN1        Software delays redesigned to use TimerA harware;    |
|                       see MsDelay() routine. Added TA constant.            |
| 1.5 12/05 STO         Added RESET pin definition                           |
| 1.6 08/08 WLUT        Added DrvSignals and RlsSignals macros for clean     |
|                       JTAG init sequence.                                  |
| 1.7 05/09 GC (Elprotronic)  Added support for the new hardware - REP430F   |
| 1.8 07/09 FB          Added macros for Spy-Bi-Wire support                 |
| 1.9 05/12 RL          Updated commentaries                                 |
|----------------------------------------------------------------------------|
| Designed 2002 by Texas Instruments Germany                                 |
\*==========================================================================*/
/*
 * 
 * Copyright (C) 2013 Texas Instruments Incorporated - http://www.ti.com/ 
 * 
 *  Redistribution and use in source and binary forms, with or without 
 *  modification, are permitted provided that the following conditions 
 *  are met:
 *
 *    Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer.
 *
 *    Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the 
 *    documentation and/or other materials provided with the   
 *    distribution.
 *
 *    Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
*/

//! \file LowLevelFunc430.h
//! \brief Low Level function prototypes, macros, and pin-to-signal assignments regarding to user's hardware
//! \author Florian Berenbrinker
//! \author Gregory Czajkowski (Elprotronic)
//! \date 05/24/2011
//! \version 1.9

/****************************************************************************/
/* INCLUDES                                                                 */
/****************************************************************************/

#include "config430.h"        // High-level user input
#include <asm-arm/arch-avalanche/generic/pal.h>
#include "tlm_arris.h"

/****************************************************************************/
/* DEFINES & CONSTANTS                                                      */
/****************************************************************************/

//! \brief JTAG interface
#define JTAG_IF             1
//! \brief JTAG interface on a device that supports JTAG and SBW
#define SPYBIWIREJTAG_IF    2
//! \brief Spy-Bi-Wire interface
#define SPYBIWIRE_IF        3

#if( INTERFACE == SPYBIWIRE_IF )
#define SPYBIWIRE_MODE 
#endif

#ifndef __DATAFORMATS__
#define __DATAFORMATS__
#define F_BYTE                     8
#define F_WORD                     16
#define F_ADDR                     20
#define F_LONG                     32
#endif


// Constants for runoff status
//! \brief return 0 = error
#define STATUS_ERROR     0      // false
//! \brief return 1 = no error
#define STATUS_OK        1      // true

// ARRIS Modifed Start: 
#ifndef int32
#define int32 long
#endif
#ifndef NULL
#define NULL 0
#endif

#define GPIO_CLOCK      CHARGER_DNLD  //PUMA6_FWDNLD_N
#define GPIO_DATA       CHARGER_RESET  //PUMA6_BATCHG_RESET_N

#ifndef GPIO_PIN
#define GPIO_PIN 1
#endif

#ifndef GPIO_INPUT_PIN
#define GPIO_INPUT_PIN 1
#endif

#ifndef GPIO_OUTPUT_PIN
#define GPIO_OUTPUT_PIN 0
#endif

#define HIGH 1
#define LOW 0
//CLOCK
#define SBWTCK(x)  PAL_sysGpioOutBit(GPIO_CLOCK, x)  
//DATA
#define SBWTDIO(x)  PAL_sysGpioOutBit(GPIO_DATA, x)  

//DATA read
#define SBWTDORD_D  PAL_sysGpioInBit(GPIO_DATA)  

#define SBWTDIO_IN PAL_sysGpioCtrl (GPIO_DATA, GPIO_PIN, GPIO_INPUT_PIN)
#define SBWTDIO_OUT PAL_sysGpioCtrl (GPIO_DATA, GPIO_PIN, GPIO_OUTPUT_PIN)
#define SBWTCK_IN PAL_sysGpioCtrl (GPIO_CLOCK, GPIO_PIN, GPIO_INPUT_PIN)
#define SBWTCK_OUT PAL_sysGpioCtrl (GPIO_CLOCK, GPIO_PIN, GPIO_OUTPUT_PIN)

#define NANO_LOOP_DELAY (7)
#define SBW_DELAY {nano_delay(NANO_LOOP_DELAY);}
void nano_delay( unsigned int delay );


//! \brief SBW macro: clear TCLK signal
void ClrTCLK(void);
//! \brief SBW macro: set TCLK signal
void SetTCLK(void);

/*----------------------------------------------------------------------------
   Definition of global variables
*/
/****************************************************************************/
/* TYPEDEFS                                                                 */
/****************************************************************************/

#ifndef __BYTEWORD__
#define __BYTEWORD__
typedef unsigned short word;
typedef unsigned char byte;
#endif

/****************************************************************************/
/* FUNCTION PROTOTYPES                                                      */
/****************************************************************************/

void    FRAME_BITS(int32 Mode, int32 PCLK, int32 TDI, int32 *TDO);
void    FRAME_BITS_FCLK(uint32 cycles);
void    InitController(void);
void    InitTarget(void);
void    ReleaseTarget(void);
word    Shift_MSB(word Format, word Data);   // used for IR- -shift
word    Shift_LSB(word Format, word data);  // used for DR -- shift
void    EnterSBWmode(void);
void    ExitSBWmode(void);
void    ResetCharger(void);
void    RlsSignals( void );
/* FUNCTIONS moved from jtagfunc.h to keep TCLK_saved local to the lowlevelfunc430 module */
word DR_Shift16(word Data);
word IR_Shift(byte Instruction);

// End Arris Modified
