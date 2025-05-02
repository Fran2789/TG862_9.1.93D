/*==========================================================================*\
|                                                                            |
| LowLevelFunc430.c                                                          |
|                                                                            |
| Low Level Functions regarding user's hardware                              |
|----------------------------------------------------------------------------|
| Project:              MSP430 Replicator                                    |
| Developed using:      IAR Embedded Workbench 3.40B [Kickstart]             |
|             and:      Code Composer Eessentials 2.0                        |
|----------------------------------------------------------------------------|
| Author:               FRGR                                                 |
| Version:              1.6                                                  |
| Initial Version:      04-17-02                                             |
| Last Change:          05-24-12                                             |
|----------------------------------------------------------------------------|
| Version history:                                                           |
| 1.0 04/02 FRGR        Initial version.                                     |
| 1.1 04/02 FRGR        Included SPI mode to speed up shifting function by 2.|
| 1.2 06/02 ALB2        Formatting changes, added comments.                  |
| 1.3 08/02 ALB2        Initial code release with Lit# SLAA149.              |
| 1.4 09/05 SUN1        Software delays redesigned to use TimerA harware;    |
|                       see MsDelay() routine. Added TA setup                |
| 1.5 12/05 STO         Adapted for 2xx devices with SpyBiWire using 4JTAG   |
| 1.6 08/08 WLUT        Cleaned up InitTarget() for JTAG init sequence.      |
| 1.7 05/09 GC (Elprotronic)  Added support for the new hardware - REP430F   |
| 1.8 07/09 FB          Added support for Spy-Bi-Wire and function           |
|                        configure_IO_SBW( void )                            |
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

//! \file LowLevelFunc430.c
//! \brief Low Level Functions regarding user's Hardware
//! \author Wolfgang Lutsch
//! \date 09/16/2009
//! \version 1.9

/****************************************************************************/
/* INCLUDES                                                                 */
/****************************************************************************/

#include "lowlevelfunc430.h"
#include "jtagfunc430.h"
#include "tlm_arris.h"

/****************************************************************************/
/* GLOBAL VARIABLES                                                         */
/****************************************************************************/

//! \brief Holds the last value of TCLK before entering a JTAG sequence
static int32 TCLK_saved = HIGH; // Per 3.19.1.2.3.1.4, initial state of TCLK is HIGH

/****************************************************************************/
/* FUNCTIONS                                                                */
/****************************************************************************/

/* delay function - nop execution is in the nanoseconds range.
   clock and data delays have been tweaked using a scope */
void nano_delay( unsigned int delay)
{
    UINT32 ix;
    for(ix=0;ix<delay;ix++)                                                                                   // 7
    {
        asm("nop");
    }
}

void FRAME_BITS(int32 Mode, int32 PCLK, int32 TDI, int32 *TDO)
{
   unsigned long flags;
   int32 tdo = 0;;
   TCLK_saved = TDI; // save the TCLK 

   SBWTDIO_OUT;      // Set DATA = Output
   
   /* disable interrupts */
   local_irq_save(flags);

   SBWTCK(1);        // 1. Set CLOCK High
   SBW_DELAY;        // 2. Delay T1
   SBW_DELAY;        // 
   SBW_DELAY;        // 

   SBWTDIO(Mode);    // 3. Set Data = Mode
   SBW_DELAY;        // Delay TCLOCK high
   SBW_DELAY;        // Delay TCLOCK high
   SBWTCK(0);        // 6. Set CLOCK low
   SBW_DELAY;        // Delay TCLOCK high

   SBWTDIO(PCLK);    //Set DATA = PCLK
   SBW_DELAY;        // Delay T1
   SBW_DELAY;        // Delay PCLK stable
   SBWTCK(1);        //Set CLOCK High, this is the start of the TDI slot
   SBW_DELAY;        // Delay T1
   SBW_DELAY;        // Delay TCLOCK low
   SBW_DELAY;        // 

   SBWTDIO(TDI);     //Set DATA = Data-in/TCLK
   SBW_DELAY;        // Delay TDI
   SBW_DELAY;        // Delay TDI
   SBW_DELAY;        // 
   SBWTCK(0);        //Set CLOCK Low
   SBW_DELAY;        // Delay TCLOCK low
   SBW_DELAY;        // Delay TCLOCK 2nd delay of cycle
   SBW_DELAY;        // Delay TCLOCK 3rd delay of cycle

   SBWTCK(1);        //Set CLOCK High, this is the start of the TDO slot
   SBW_DELAY;        // Delay TCLOCK low
   SBWTDIO_IN;       // Set Data = Input
   SBW_DELAY;        // 
   SBW_DELAY;        // Delay T2

   SBWTCK(0);        // Set CLOCK low
   SBW_DELAY;        // Delay T5
   SBW_DELAY;        // Delay TCLOCK low
   SBW_DELAY;        // Delay T2
   tdo = SBWTDORD_D;  // Read/Latch DATA
   SBWTCK(1);        //Set CLOCK High, this is the end of the TDO Slot and the frame
   SBW_DELAY;        // 
   SBW_DELAY;        //
   SBWTDIO_OUT;      // Set DATA = Output

   /* re-enable interrupts */
   local_irq_restore(flags);

   /* Copy read bit if needed */
   if(TDO)
   {
       *TDO = tdo;
   }
}

/* Special FRAME to be used during FLASH erase */
void FRAME_BITS_FCLK(uint32 cycles)
{
   unsigned int tdo = 0;
   unsigned int ix;
   unsigned long flags;

   SBWTDIO_OUT;      // Set DATA = Output

   /* disable interrupts */
   local_irq_save(flags);

   SBWTCK(1);        // 1 Set CLOCK High
   SBW_DELAY;        // 2 Delay T1
   SBW_DELAY;        // 
   SBW_DELAY;        // 

   SBWTDIO(0);       // 3 Set Mode to 0 always
   SBW_DELAY;        // 4 Delay TCLOCK high
   SBW_DELAY;        // 5 Delay T2 
   SBWTCK(0);        // 6 Set CLOCK low
   SBW_DELAY;        // 7 Delay T3

   SBWTDIO(TCLK_saved); // 8 Set DATA = PCLK
   SBW_DELAY;        // 9 Delay TCLOCK low
   SBW_DELAY;        // 10 Delay T4
   SBWTCK(1);        // 11 Set CLOCK High, this is the start of the TDI slot
   SBW_DELAY;        // 12 Delay T1

   for(ix=0;ix<cycles;ix++)
   {
       SBWTDIO(1);       // 13a Set to 1 always
       nano_delay(110);
       SBWTDIO(0);       // 13c Set to 0 always
       nano_delay(180);
   }
   TCLK_saved = 0; // save the last TCLK as LOW

   SBW_DELAY;        // 14 Delay TCLOCK high
   SBW_DELAY;        // 15 Delay T2
   SBWTCK(0);        // 16 Set CLOCK Low
   SBW_DELAY;        // 17 Delay TCLOCK low
   SBW_DELAY;        //    Delay TCLOCK 2nd delay of cycle
   SBW_DELAY;        //    Delay TCLOCK 3rd delay of cycle

   SBWTCK(1);        // 18 Set CLOCK High, this is the start of the TDO slot
   SBW_DELAY;        // 19 Delay T1
   SBWTDIO_IN;       // 20 Set Data = Input
   SBW_DELAY;        // 21 Delay TCLOCK high
   SBW_DELAY;        // 22 Delay T2

   SBWTCK(0);        // 23 Set CLOCK low
   SBW_DELAY;        // 24 Delay T5
   tdo = SBWTDORD_D; // 25 Read/Latch DATA, discard...
   SBW_DELAY;        // 26 Delay TCLOCK low
   SBW_DELAY;        // 27 Delay T6
   SBWTCK(1);        // 28 Set CLOCK High, this is the end of the TDO Slot and the frame
   SBW_DELAY;        // 
   SBW_DELAY;        // 
   SBWTDIO_OUT;      // Set DATA = Output

   /* re-enable interrupts */
   local_irq_restore(flags);

   return;

}

//----------------------------------------------------------------------------
//! \brief Clear TCLK in Spy-Bi-Wire mode
//! \details enters with TCLK_saved and exits with TCLK = 0
void ClrTCLK(void)
{
    FRAME_BITS(LOW, TCLK_saved, LOW, NULL);
}

//----------------------------------------------------------------------------
//! \brief Set TCLK in Spy-Bi-Wire mode
//! \details enters with TCLK_saved and exits with TCLK = 1
void SetTCLK(void)
{
    FRAME_BITS(LOW, TCLK_saved, HIGH, NULL);
}

//----------------------------------------------------------------------------
//! \brief Shift a value into TDI (MSB first) and simultaneously shift out a 
//! value from TDO (MSB first).
//! \param[in] Format (number of bits shifted, 8 (F_BYTE), 16 (F_WORD), 
//! 20 (F_ADDR) or 32 (F_LONG))
//! \param[in] Data (data to be shifted into TDI)
//! \return unsigned long (scanned TDO value)
word Shift_MSB(word Format, word Data)
{
   word TDOword = 0x0000;
   word MSB = 0x0000;
   word i;
   int32 TDO;
	
   //Data = 0x2C01 = 1011 0000 0000 0001
   (Format == F_WORD) ? (MSB = 0x8000) : (MSB = 0x80); // 1000 0000 0000 0000
   for (i = Format; i > 0; i--)
   {
        if (i == 1)                     // last bit requires TMS=1; TDO one bit before TDI
        {
           // The LSB
            FRAME_BITS(HIGH, TCLK_saved, (Data & MSB), &TDO);
        }
        else
        {
            FRAME_BITS(LOW, TCLK_saved, (Data & MSB), &TDO);
        }
        
        /* shift output data */
        Data <<= 1;

        if (TDO)
            TDOword++;
        if (i > 1)
            TDOword <<= 1;               // TDO could be any port pin
   }
    FRAME_BITS(HIGH, TCLK_saved, TCLK_saved, NULL); // update IR
    FRAME_BITS(LOW, TCLK_saved, TCLK_saved, NULL); //Run-Test/Idle

   return(TDOword);
}

word Shift_LSB(word Format, word Data)
{
    word TDOword = 0x0000;
    word LSB = 0x0001;
    word i;
    int32 TDO;

    for (i = Format; i > 0; i--)
    {
       if (i == 1)    // last bit requires TMS=1; TDO one bit before TDI
       {  // the MSB
           FRAME_BITS(HIGH, TCLK_saved, (Data & LSB), &TDO);
       }
       else
       {
           // bits from LSB 0 to LSB 6
           FRAME_BITS(LOW, TCLK_saved, (Data & LSB), &TDO);
       }
       Data >>= 1;
       if (TDO) // read in TDO Slot
       TDOword++;
       if (i > 1)
       TDOword <<= 1;    // TDO could be any port pin
    }

    FRAME_BITS(HIGH, TCLK_saved, TCLK_saved, NULL); // update IR
    FRAME_BITS(LOW, TCLK_saved, TCLK_saved, NULL); //Run-Test/Idle
    if(TDOword != 0x89)
    {
        printk(KERN_WARNING "IR_SHIFT did not return 0x89 (0x%x), data = 0x%x\n",TDOword,Data);
    }
    return(TDOword);
}

void ResetCharger(void)
{
    unsigned long flags;

    SBWTDIO_OUT;      // Set DATA = Output

    printk("Resetting Charger\n");

    /* disable interrupts */
    local_irq_save(flags);

    SBWTCK(0);    // Pull CLOCK Low
    udelay(2);
    SBWTDIO(0);   // Pull DATA Low
    udelay(8);
    SBWTDIO(1);
    udelay(8);
    SBWTDIO_IN;   // Set Data as INPUT

    /* re-enable interrupts */
    local_irq_restore(flags);
}

//----------------------------------------------------------------------------
//! \brief Set up I/O pins for JTAG communication
void ExitSBWmode(void)
{
    unsigned long flags;

    SBWTDIO_OUT;      // Set DATA = Output

    /* disable interrupts */
    local_irq_save(flags);

    SBWTCK(0);    // Pull CLOCK Low
    udelay(120);
    SBWTCK_IN;    // Set CLOCK = input
    SBWTDIO(0);   // Pull DATA Low
    udelay(7);
    SBWTDIO(1);
    udelay(7);
    SBWTDIO(0);   // Pull DATA Low
    udelay(7);
    SBWTDIO(1);
    SBWTDIO_IN;   // Set Data as INPUT

    /* re-enable interrupts */
    local_irq_restore(flags);

    return;
}

//----------------------------------------------------------------------------
//! \brief Set up I/O pins for JTAG communication
void EnterSBWmode(void)
{
    unsigned long flags;

    /* initialize clock and data GPIOs to be output
       only the DATA GPIO will be switched between INPUT/OUTPUT while in
       SBWMODE.  CLK should stay as output until we exit mode */
    SBWTDIO_OUT;      // Set DATA = Output
    SBWTCK_OUT;       // Set CLK = Output

    /* disable interrupts */
    local_irq_save(flags);

    SBWTDIO(1);  // Pull DATA High
    udelay(1);
    SBWTCK(1);   // Pull CLOCK High
    /* HIGH CLK DELAY */
    udelay(2);

    SBWTCK(0);   // Pull CLOCK Low
    udelay(2);
    /* LOW CLK DELAY */

    SBWTCK(1);   // Pull CLOCK High
    udelay(2);

    /* re-enable interrupts */
    local_irq_restore(flags);

    return;
}

//----------------------------------------------------------------------------
//! \brief Release I/O pins
void RlsSignals(void)
{
    unsigned long flags;

    SBWTDIO_OUT;      // Set DATA = Output

    /* disable interrupts */
    local_irq_save(flags);

    SBWTCK(0);   // Set CLOCK low
    SBWTDIO(0);  // Set DATA low
    /* LOW CLK DELAY */
    udelay(2);

    /* re-enable interrupts */
    local_irq_restore(flags);

    return;
}

/* FUNCTIONS moved from jtagfunc.c to keep TCLK_saved local to this module */
//----------------------------------------------------------------------------
//! \brief Function for shifting a given 16-bit word into the JTAG data
//! register through TDI.
//! \param[in] word data (16-bit data, MSB first)
//! \return word (value is shifted out via TDO simultaneously)
word DR_Shift16(word data)
{
    // JTAG FSM state = Run-Test/Idle
    FRAME_BITS(HIGH, TCLK_saved, TCLK_saved, NULL);
    // JTAG FSM state = Select DR-Scan
    FRAME_BITS(LOW,  TCLK_saved, TCLK_saved, NULL);
    // JTAG FSM state = Capture-DR
    FRAME_BITS(LOW,  TCLK_saved, TCLK_saved, NULL);
    // JTAG FSM state = Shift-DR, Shift in TDI (16-bit)
    return(Shift_MSB(F_WORD, data));
    // JTAG FSM state = Run-Test/Idle

}

//----------------------------------------------------------------------------
//! \brief Function for shifting a new instruction into the JTAG instruction
//! register through TDI (MSB first, but with interchanged MSB - LSB, to
//! simply use the same shifting function, Shift(), as used in DR_Shift16).
//! \param[in] byte Instruction (8bit JTAG instruction, MSB first)
//! \return word TDOword (value shifted out from TDO = JTAG ID)
word IR_Shift(byte instruction)
{

    // JTAG FSM state = Run-Test/Idle
    FRAME_BITS(HIGH, TCLK_saved, TCLK_saved, NULL);
    // JTAG FSM state = Select DR-Scan
    
    FRAME_BITS(HIGH, TCLK_saved, HIGH, NULL);

    // JTAG FSM state = Select IR-Scan
    FRAME_BITS(LOW, TCLK_saved, HIGH, NULL);
    // JTAG FSM state = Capture-IR
    FRAME_BITS(LOW, TCLK_saved, HIGH, NULL);
    //TMSL_TDIH();
    // JTAG FSM state = Shift-IR, Shift in TDI (8-bit)    
    return(Shift_LSB(F_BYTE, instruction));
    // JTAG FSM state = Run-Test/Idle
}

/****************************************************************************/
/*                         END OF SOURCE FILE                               */
/****************************************************************************/
