/*==========================================================================*\
|                                                                            |
| JTAGfunc430.h                                                              |
|                                                                            |
| JTAG Function Prototypes and Definitions                                   |
|----------------------------------------------------------------------------|
| Project:              JTAG Functions                                       |
| Developed using:      IAR Embedded Workbench 3.40B [Kickstart]             |
|             and:      Code Composer Eessentials 2.0                        |
|----------------------------------------------------------------------------|
| Author:               STO                                                  |
| Version:              1.5                                                  |
| Initial Version:      04-17-02                                             |
| Last Change:          03-14-13                                             |
|----------------------------------------------------------------------------|
| Version history:                                                           |
| 1.0 04/02 FRGR        Initial version.                                     |
| 1.1 06/02 ALB2        Formatting changes, added comments.                  |
| 1.2 08/02 ALB2        Initial code release with Lit# SLAA149.              |
| 1.3 01/06 STO         Minor cosmetic changes                               |
| 1.4 04/07 WLUT        WriteFLASHallSections changed due to function spec   |
| 1.5 03/13 RL/MD       Added unlock function for Info A                     |
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

//! \file JTAGfunc430.h
//! \brief JTAG Function Prototypes and Definitions
//! \author Berenbrinker Florian
//! \date 03/14/2013
//! \version 1.5

/****************************************************************************/
/* INCLUDES                                                                 */
/****************************************************************************/

#include "config430.h"        // High-level user input

/****************************************************************************/
/* Global types                                                             */
/****************************************************************************/

#ifndef __BYTEWORD__
#define __BYTEWORD__
typedef unsigned short  word;
typedef unsigned char byte;
#endif

/****************************************************************************/
/* Define section for constants                                             */
/****************************************************************************/

//----------------------------------------------------------------------------
// Constants for the JTAG instruction register (IR) require LSB first.
// The MSB has been interchanged with LSB due to use of the same shifting
// function as used for the JTAG data register (DR) which requires MSB 
// first.
//----------------------------------------------------------------------------

// Instructions for the JTAG control signal register
//! \brief Set the JTAG control signal register
//#define IR_CNTRL_SIG_16BIT         0xC8   // original value: 0x13
#define IR_CNTRL_SIG_16BIT     0x13

//! \brief Read out the JTAG control signal register
//#define IR_CNTRL_SIG_CAPTURE       0x28   // original value: 0x14
#define IR_CNTRL_SIG_CAPTURE    0x14

//! \brief Release the CPU from JTAG control
//#define IR_CNTRL_SIG_RELEASE       0xA8   // original value: 0x15
#define IR_CNTRL_SIG_RELEASE       0x15

// Instructions for the JTAG fuse
//! \brief Prepare for JTAG fuse blow
#define IR_PREPARE_BLOW            0x44   // original value: 0x22
//! \brief Perform JTAG fuse blow
#define IR_EX_BLOW                 0x24   // original value: 0x24

// Instructions for the JTAG data register
//! \brief Set the MSP430 MDB to a specific 16-bit value with the next 
//! 16-bit data access 
//#define IR_DATA_16BIT              0x82   // original value: 0x41
#define IR_DATA_16BIT          0x41

//! \brief Set the MSP430 MDB to a specific 16-bit value (RAM only)
//#define IR_DATA_QUICK              0xC2   // original value: 0x43
#define IR_DATA_QUICK              0x43   // original value: 0x43


// Instructions for the JTAG PSA mode
//! \brief Switch JTAG data register to PSA mode
//#define IR_DATA_PSA                0x22   // original value: 0x44
#define IR_DATA_PSA                0x44

//! \brief Shift out the PSA pattern generated by IR_DATA_PSA
//#define IR_SHIFT_OUT_PSA           0x62   // original value: 0x46
#define IR_SHIFT_OUT_PSA           0x46   // original value: 0x46

// Instructions for the JTAG address register
//! \brief Set the MSP430 MAB to a specific 16-bit value
//! \details Use the 20-bit macro for 430X and 430Xv2 architectures
//#define IR_ADDR_16BIT              0xC1   // original value: 0x83
#define IR_ADDR_16BIT            0x83

//! \brief Read out the MAB data on the next 16/20-bit data access
//#define IR_ADDR_CAPTURE            0x21   // original value: 0x84
#define IR_ADDR_CAPTURE            0x84

//! \brief Set the MSP430 MDB with a specific 16-bit value and write
//! it to the memory address which is currently on the MAB
//#define IR_DATA_TO_ADDR            0xA1   // original value: 0x85
#define IR_DATA_TO_ADDR            0x85   // original value: 0x85

//! \brief Bypass instruction - TDI input is shifted to TDO as an output
#define IR_BYPASS                  0xFF   // original value: 0xFF

// JTAG identification values for Flash-based MSP430 devices
//! \brief JTAG identification value for MSP430 architecture devices
#define JTAG_ID                    0x89

// Constants for data formats, dedicated addresses
#define F_BYTE                     8
#define F_WORD                     16
#define V_RESET                    0xFFFE

// Constants for VPP connection at Blow-Fuse
//! \brief Fuse blow voltage is supplied via the TDI pin
#define VPP_ON_TDI                 0
//! \brief Fuse blow voltage is supplied via the TEST pin
#define VPP_ON_TEST                1

/****************************************************************************/
/* Function prototypes                                                      */
/****************************************************************************/

// Low level JTAG functions
word DR_Shift16(word Data);
word IR_Shift(byte Instruction);
void ResetTAP(void);
word ExecutePOR(void);
word SetInstrFetch(void);
void SetPC(word Addr);
void HaltCPU(void);
void ReleaseCPU(void);

// High level JTAG functions
word GetDevice(void);
void ReleaseDevice(void);
void WriteMem(word Format, word Addr, word Data);
word WriteFLASH(word StartAddr, word Length, word *DataArray);
word WriteFLASHallSections(const unsigned int *data, const unsigned long *address, const unsigned long *length_of_sections, const unsigned long sections);
int ReadFLASH(word startAddr, word endAddr, unsigned char *dataArray);
word ReadMem(word Format, word Addr);
word EraseFLASH(word EraseMode, word EraseAddr);
/* unused functions */
#if 0
void ReadMemQuick(word StartAddr, word Length, word *dataArray);
void WriteMemQuick(word StartAddr, word Length, word *DataArray);
word EraseCheck(word StartAddr, word Length);
word VerifyMem(word StartAddr, word Length, word *DataArray);
void UnlockInfoA(void);
#endif

/****************************************************************************/
/* VARIABLES                                                                */
/****************************************************************************/

#define ERASE_SGMT    0xA502
#define ERASE_MAIN    0xA504
#define ERASE_MASS    0xA506

/****************************************************************************/
/*                         END OF SOURCE FILE                               */
/****************************************************************************/
