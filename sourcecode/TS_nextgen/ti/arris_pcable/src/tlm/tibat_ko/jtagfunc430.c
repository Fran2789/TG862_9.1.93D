/*==========================================================================*\
|                                                                            |
| JTAGfunc430.c                                                              |
|                                                                            |
| JTAG Control Sequences for Erasing / Programming / Fuse Burning            |
|----------------------------------------------------------------------------|
| Project:              JTAG Functions                                       |
| Developed using:      IAR Embedded Workbench 3.40B [Kickstart]             |
|             and:      Code Composer Eessentials 2.0                        |
|----------------------------------------------------------------------------|
| Author:               RL                                                   |
| Version:              2.5                                                  |
| Initial Version:      04-17-02                                             |
| Last Change:          05-02-14                                             |
|----------------------------------------------------------------------------|
| Version history:                                                           |
| 1.0 04/02 FRGR        Initial version.                                     |
| 1.1 04/02 ALB2        Formatting changes, added comments.                  |
| 1.2 08/02 ALB2        Initial code release with Lit# SLAA149.              |
| 1.3 09/05 JDI         'ResetTAP': added SetTDI for fuse check              |
|                       search for F2xxx to find respective modifications in |
|                       'SetPC', 'HaltCPU', 'VerifyPSA', 'EraseFLASH'        |
|                       'WriteFLASH'                                         |
|           SUN1        Software delays redesigned to use TimerA harware;    |
|                       see MsDelay() routine.                               |
| 1.4 01/06 STO         Added entry sequence for SpyBiWire devices           |
|                       Minor cosmetic changes                               |
| 1.5 03/06 STO         BlowFuse() make correct fuse check after blowing.    |
| 1.6 07/06 STO         Loop in WriteFLASH() changed.                        |
| 1.7 04/06 WLUT        'VerifyPSA', 'ReadMemQuick' changed to TCLK high.    |
|                       JTAG4 sequence changed according to figure 10 in     |
|                       Lit# SLAA149.                                        |
|                       WriteFLASHallSections changed due to spec of srec_cat|
|                       Renamed 'ExecutePUC' to 'ExecutePOR'.                |
| 1.8 01/08 WLUT        Added usDelay(5) in ResetTAP() to ensure at least    |
|                       5us low phase of TMS during JTAG fuse check.         |
| 1.9 08/08 WLUT        Added StartJtag() and StopJtag() for a clean init    |
|                       sequence.                                            |
| 2.0 07/09 FB          Added support for Spy-Bi-Wire and new replecator     |
| 2.1 06/12 RL/GC (Elprotronic) Updated commentaries/Added Fuse Blow via SBW |
| 2.2 03/13 RL/MD       Added unlock function for Info A                     |
| 2.3 07/13 RL          Fixed unlock function for Info A                     |
| 2.4 02/14 RL          Fixed flow in SetPC()                                |
| 2.5 04/02 RL          Updated SBW entry sequence to support i2040          |
|----------------------------------------------------------------------------|
| Designed 2002 by Texas Instruments Germany                                 |
\*==========================================================================*/
/*
 * 
 * Copyright (C) 2014 Texas Instruments Incorporated - http://www.ti.com/ 
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

//! \file JTAGfunc430.c
//! \brief JTAG Control Sequences for Erasing / Programming / Fuse Burning
//! \author Robert Lessmeier
//! \date 05/02/2014
//! \version 2.5

// NOTE ARRIS has made many changes in this file, basically remove all the TI implementations. 

/****************************************************************************/
/* INCLUDES                                                                 */
/****************************************************************************/

#include "jtagfunc430.h"
#include "lowlevelfunc430.h"
#include <linux/delay.h>


//! \brief Holds the Flash InfoA Lock/Unlock Key, default = locked
static unsigned short SegmentInfoAKey = 0xA500;
/****************************************************************************/
/* Low level routines for accessing the target device via JTAG:             */
/****************************************************************************/

//----------------------------------------------------------------------------
//! \brief Reset target JTAG interface and perform fuse-HW check.
void ResetTAP(void)
{
    word i;

    // Reset JTAG FSM
    for (i = 6; i > 0; i--)
    {
        FRAME_BITS(HIGH, HIGH, HIGH, 0);
    }
    // JTAG FSM is in Test-Logic-Reset now             
    FRAME_BITS(LOW, HIGH, HIGH, 0);
    // JTAG FSM is in Run-Test/IDLE

    // Perform fuse check
    FRAME_BITS(HIGH, HIGH, HIGH, 0);
    FRAME_BITS(LOW, HIGH, HIGH, 0);
    FRAME_BITS(HIGH, HIGH, HIGH, 0);
    FRAME_BITS(LOW, HIGH, HIGH, 0);
    FRAME_BITS(HIGH, HIGH, HIGH, 0);
    // In every TDI slot a TCK for the JTAG machine is generated.
    // Thus we need to get TAP in Run/Test Idle state back again.
    FRAME_BITS(HIGH, HIGH, HIGH, 0);
    FRAME_BITS(LOW, HIGH, HIGH, 0);               // now in Run/Test Idle
}

//----------------------------------------------------------------------------
//! \brief Function to execute a Power-On Reset (POR) using JTAG CNTRL SIG 
//! register
//! \return word (STATUS_OK if target is in Full-Emulation-State afterwards,
//! STATUS_ERROR otherwise)
word ExecutePOR(void)
{
    word JtagVersion;

    // Perform Reset
    IR_Shift(IR_CNTRL_SIG_16BIT);
    DR_Shift16(0x2C01);                 // Apply Reset
    DR_Shift16(0x2401);                 // Remove Reset
    ClrTCLK();                          // TCLK 0
    SetTCLK();                          // TCLK 1
    ClrTCLK();                          // TCLK 0
    SetTCLK();                          // TCLK 1
    ClrTCLK();                          // TCLK 0
    JtagVersion = IR_Shift(IR_ADDR_CAPTURE); // read JTAG ID, checked at function end
    SetTCLK();

    // CPU is now reset and registers are in power up state.
    // PC=data that is currently stored in 0xFFFE (reset vector)
    WriteMem(F_WORD, 0x0120, 0x5A80);   // Disable Watchdog on target device

    if (JtagVersion != JTAG_ID)
    {
        return(STATUS_ERROR);
    }
    return(STATUS_OK);
}

//----------------------------------------------------------------------------
//! \brief Function to set target CPU JTAG FSM into the instruction fetch state
//! \return word (STATUS_OK if instr. fetch was set, STATUS_ERROR otherwise)
word SetInstrFetch(void)
{
    word i;

    IR_Shift(IR_CNTRL_SIG_CAPTURE);

    // Wait until CPU is in instr. fetch state, timeout after limited attempts
    for (i = MAX_ENTRY_TRY; i > 0; i--)
    {
        if (DR_Shift16(0x0000) & 0x0080)
        {
            return(STATUS_OK);
        }
        ClrTCLK(); // Set Low
        SetTCLK(); // Set High
    }
    return(STATUS_ERROR);
}

//----------------------------------------------------------------------------
//! \brief Load a given address into the target CPU's program counter (PC).
//! \param[in] word Addr (destination address)
void SetPC(word Addr)
{
    SetInstrFetch();              // Set CPU into instruction fetch mode, TCLK=1

    // Load PC with address
    IR_Shift(IR_CNTRL_SIG_16BIT); // Write to JTAG control status register
    DR_Shift16(0x3401);           // Release low byte from JTAG control. CPU has control of RW & BYTE.
    IR_Shift(IR_DATA_16BIT);
    DR_Shift16(0x4030);           // "mov #addr,PC" instruction to load PC
    ClrTCLK();                    // Set high
    SetTCLK();                    // Set Low
    DR_Shift16(Addr);             // "mov #addr,PC" instruction, value Addr to write in PC
    ClrTCLK();
    SetTCLK();
    IR_Shift(IR_ADDR_CAPTURE);    // PC is written on the next clock
    ClrTCLK();                    // TCLK = 0; Now the PC should be on Addr
    IR_Shift(IR_CNTRL_SIG_16BIT); // Write to JTAG control status register
    DR_Shift16(0x2401);           // JTAG has control of RW & BYTE.
    SetTCLK();                    // Leave with TCLK high
}

//----------------------------------------------------------------------------
//! \brief Function to set the CPU into a controlled stop state
void HaltCPU(void)
{
    if( SetInstrFetch() == STATUS_ERROR)              // Set CPU into instruction fetch mode
    {
        printk("SetInstrFetch failed\n");
    }

    IR_Shift(IR_DATA_16BIT);
    DR_Shift16(0x3FFF);           // Send JMP $ instruction
    ClrTCLK();
    IR_Shift(IR_CNTRL_SIG_16BIT);
    DR_Shift16(0x2409);           // Set JTAG_HALT bit to halt CPU
    SetTCLK();
}

//----------------------------------------------------------------------------
//! \brief Function to release the target CPU from the controlled stop state
void ReleaseCPU(void)
{
    ClrTCLK();                    // Set TCLK low
    IR_Shift(IR_CNTRL_SIG_16BIT);
    DR_Shift16(0x2401);           // Clear the HALT_JTAG bit, set CPU to read and run
    IR_Shift(IR_ADDR_CAPTURE); 
    SetTCLK();
}

/* unused function */
#if 0
//----------------------------------------------------------------------------
//! \brief This function compares the computed PSA (Pseudo Signature Analysis)
//! value to the PSA value shifted out from the target device.
//! It is used for very fast data block write or erasure verification.
//! \param[in] unsigned long StartAddr (Start address of data block to be checked)
//! \param[in] unsigned long Length (Number of words within data block)
//! \param[in] word *DataArray (Pointer to array with the data, 0 for Erase Check)
//! \return word (STATUS_OK if comparison was successful, STATUS_ERROR otherwise)
static word VerifyPSA(word StartAddr, word Length, word *DataArray)
{
// TODO: TBD
    return STATUS_OK;
}
#endif

/****************************************************************************/
/* High level routines for accessing the target device via JTAG:            */
/*                                                                          */
/* From the following, the user is relieved from coding anything.           */
/* To provide better understanding and clearness, some functionality is     */
/* coded generously. (Code and speed optimization enhancements may          */
/* be desired)                                                              */
/****************************************************************************/

//----------------------------------------------------------------------------
//! \brief Function to take target device under JTAG control. Disables the 
//! target watchdog. Sets the global DEVICE variable as read from the target 
//! device.
//! \return word (STATUS_ERROR if fuse is blown, incorrect JTAG ID or
//! synchronizing time-out; STATUS_OK otherwise)
word GetDevice(void)
{
    word i;
    for (i = 0; i < MAX_ENTRY_TRY; i++)
    {
        ResetTAP();
        IR_Shift(IR_CNTRL_SIG_16BIT);
        DR_Shift16(0x2401);                  // Set device into JTAG mode + read
        if (IR_Shift(IR_CNTRL_SIG_CAPTURE) == JTAG_ID)
        {
            /* TODO - what to do in this case? */
           break;
        }
    }
    if(i >= MAX_ENTRY_TRY)
    {
        return(STATUS_ERROR);
    }

    // Wait until CPU is synchronized, timeout after a limited # of attempts
    for (i = 50; i > 0; i--)
    {
        if (DR_Shift16(0x0000) & 0x0200)
        {
            word DeviceId;
            DeviceId = ReadMem(F_WORD, 0x0FF0);// Get target device type
                                               //(bytes are interchanged)
            DeviceId = (DeviceId << 8) + (DeviceId >> 8); // swop bytes
            /* ARRIS does not use Device APIs */
            //printk ("DeviceId = 0x%x\n",DeviceId);
            //Set Device index, which is used by functions in Device.c
            //SetDevice(DeviceId);
            break;
        }
        else
        {
            if (i == 1)
            {
                return(STATUS_ERROR);      // Timeout reached, return false
            }
        }
    }

    if (!ExecutePOR())                     // Perform PUC, Includes
    {
        printk (KERN_ERR "ExecutePOR failed\n");
        return(STATUS_ERROR);              // target Watchdog disable.
    }
    return(STATUS_OK);
}

//----------------------------------------------------------------------------
//! \brief Function to release the target device from JTAG control
//! \param[in] word Addr (0xFFFE: Perform Reset, means Load Reset Vector into 
//! PC, otherwise: Load Addr into PC)
void ReleaseDevice(void)
{
    ExitSBWmode();
}

//----------------------------------------------------------------------------
//! \brief This function writes one byte/word at a given address ( <0xA00)
//! \param[in] word Format (F_BYTE or F_WORD)
//! \param[in] word Addr (Address of data to be written)
//! \param[in] word Data (shifted data)
void WriteMem(word Format, word Addr, word Data)
{
    HaltCPU();                  // Halt the CPU

    ClrTCLK();
    IR_Shift(IR_CNTRL_SIG_16BIT);
    if  (Format == F_WORD)
    {
        DR_Shift16(0x2408);     // Set word write
    }
    else
    {
        DR_Shift16(0x2418);     // Set byte write
    }
    IR_Shift(IR_ADDR_16BIT);    // Set an address
    DR_Shift16(Addr);           // Set addr 0x120: watchdog control register address 
    IR_Shift(IR_DATA_TO_ADDR);  // Set data to write to the address
    DR_Shift16(Data);           // Shift in 16 bits data
    SetTCLK();

    ReleaseCPU();
}

/* unused function */
#if 0
//----------------------------------------------------------------------------
//! \brief This function writes an array of words into the target memory.
//! \param[in] word StartAddr (Start address of target memory)
//! \param[in] word Length (Number of words to be programmed)
//! \param[in] word *DataArray (Pointer to array with the data)
void WriteMemQuick(word StartAddr, word Length, word *DataArray)
{
    word i;

    // Initialize writing:
    SetPC((word)(StartAddr-4));
    HaltCPU();

    ClrTCLK();
    IR_Shift(IR_CNTRL_SIG_16BIT);
    DR_Shift16(0x2408);             // Set RW to write
    IR_Shift(IR_DATA_QUICK);
    for (i = 0; i < Length; i++)
    {
        DR_Shift16(DataArray[i]);   // Shift in the write data
        SetTCLK();
        ClrTCLK();                  // Increment PC by 2
    }
    ReleaseCPU();
}
#endif 
//----------------------------------------------------------------------------
//! \brief This function programs/verifies an array of words into the FLASH
//! memory by using the FLASH controller.
//! \param[in] word StartAddr (Start address of FLASH memory)
//! \param[in] word Length (Number of words to be programmed)
//! \param[in] word *DataArray (Pointer to array with the data)
word WriteFLASH(word StartAddr, word Length, word *DataArray)
{
    word i;                     // Loop counter
    word addr = StartAddr;      // Address counter
    word FCTL3_val = SegmentInfoAKey;   // SegmentInfoAKey holds Lock-Key for Info
                                        // Seg. A 

    // The start addr should not be lower than 0xc400.
    if (addr < 0xC400)
   	{
        return(STATUS_ERROR);
    }
    HaltCPU();                    // 1

    ClrTCLK();                    // 2
    IR_Shift(IR_CNTRL_SIG_16BIT); // 3
    DR_Shift16(0x2408);           // 4 Set RW to write
    IR_Shift(IR_ADDR_16BIT);      // 5
    DR_Shift16(0x0128);           // 6 FCTL1 register
    IR_Shift(IR_DATA_TO_ADDR);    // 7
    DR_Shift16(0xA540);           // 8 Enable FLASH write
    SetTCLK();                    // 9

    ClrTCLK();                    // 10
    IR_Shift(IR_ADDR_16BIT);      // 11
    DR_Shift16(0x012A);           // 12 FCTL2 register
    IR_Shift(IR_DATA_TO_ADDR);    // 13
    DR_Shift16(0xA540);           // 14 Select MCLK as source, DIV=1
    SetTCLK();                    // 15

    ClrTCLK();                    // 16
    IR_Shift(IR_ADDR_16BIT);      // 17 
    DR_Shift16(0x012C);           // 18 FCTL3 register
    IR_Shift(IR_DATA_TO_ADDR);    // 19 
    DR_Shift16(FCTL3_val);        // 20 Clear FCTL3; F2xxx: Unlock Info-Seg.
                                  // A by toggling LOCKA-Bit if required,
    SetTCLK();                    // 21

    ClrTCLK();                    // 22
    IR_Shift(IR_CNTRL_SIG_16BIT); // 23

    for (i = 0; i < Length; i++, addr += 2)
    {
        //printk ("Writing 0x%x to 0x%x\n",DataArray[i],addr);
        DR_Shift16(0x2408);             // 24 Set RW to write
        IR_Shift(IR_ADDR_16BIT);        // 25 
        DR_Shift16(addr);               // 26 Set address
        IR_Shift(IR_DATA_TO_ADDR);      // 27
        DR_Shift16(DataArray[i]);       // 28 Set data
        SetTCLK();                      // 29 
        ClrTCLK();                      // 30 
        IR_Shift(IR_CNTRL_SIG_16BIT);   // 31
        DR_Shift16(0x2409);             // 32 Set RW to read

        FRAME_BITS_FCLK(25);            // 33-36
                                        // F2xxx: 29 are ok
    }                                   // 37 - loop

    IR_Shift(IR_CNTRL_SIG_16BIT);       // 38
    DR_Shift16(0x2408);                 // 39 Set RW to write
    IR_Shift(IR_ADDR_16BIT);            // 40
    DR_Shift16(0x0128);                 // 41 FCTL1 register
    IR_Shift(IR_DATA_TO_ADDR);          // 42 
    DR_Shift16(0xA500);                 // 43 Disable FLASH write
    SetTCLK();                          // 44

    // set LOCK-Bits again
    ClrTCLK();                          // 45
    IR_Shift(IR_ADDR_16BIT);            // 46 
    DR_Shift16(0x012C);                 // 47 FCTL3 address
    IR_Shift(IR_DATA_TO_ADDR);          // 48
    DR_Shift16(0xA510);                 // 49 Disable FLASH Write Acess. LOCK = 1
    SetTCLK();                          // 50

    ReleaseCPU();
    return(STATUS_OK);
}

/* unused function */
#if 0
//----------------------------------------------------------------------------
//! \brief This function programs/verifies a set of data arrays of words into a FLASH
//! memory by using the "WriteFLASH()" function. It conforms with the
//! "CodeArray" structure convention of file "Target_Code_(IDE).s43" or "Target_Code.h".
//! \param[in] const unsigned int  *DataArray (Pointer to array with the data)
//! \param[in] const unsigned long *address (Pointer to array with the startaddresses)
//! \param[in] const unsigned long *length_of_sections (Pointer to array with the number of words counting from startaddress)
//! \param[in] const unsigned long sections (Number of sections in code file)
//! \return word (STATUS_OK if verification was successful,
//! STATUS_ERROR otherwise)
word WriteFLASHallSections(const unsigned int *data, const unsigned long *address, const unsigned long *length_of_sections, const unsigned long sections)
{
    int i, init = 1;

    for(i = 0; i < sections; i++)
    {
        // Write/Verify(PSA) one FLASH section
        WriteFLASH(address[i], length_of_sections[i], (word*)&data[init-1]);
        if (!VerifyMem(address[i], length_of_sections[i], (word*)&data[init-1]))
        {
            return(STATUS_ERROR);
        }
        init += length_of_sections[i];      
    }
    
    return(STATUS_OK);
}
#endif 

int ReadFLASH(word startAddr, word endAddr, unsigned char *dataArray)
{
    word readOut_le;
    unsigned int addr;

    if(startAddr > endAddr) 
    {
        return -1;
    }

    HaltCPU();

    ClrTCLK();
    for (addr=startAddr; addr<=endAddr; addr+=2)
    {
        IR_Shift(IR_CNTRL_SIG_16BIT);
        DR_Shift16(0x2409);         // Set word read
        IR_Shift(IR_ADDR_16BIT);
        DR_Shift16((word)addr);      // Set address to read
        IR_Shift(IR_DATA_TO_ADDR);
        SetTCLK();    
        ClrTCLK();   
        readOut_le = DR_Shift16(0x0000);   // Shift out 16 bits, little Endian
        *dataArray =  (unsigned char)readOut_le & 0x00FF; // The MSB
        dataArray++;
        *dataArray = (unsigned char)((readOut_le & 0xFF00) >> 8);  //The LSB
        dataArray++;
	}
    ReleaseCPU();
	
    return 0;
}

//----------------------------------------------------------------------------
//! \brief This function reads one byte/word from a given address in memory
//! \param[in] word Format (F_BYTE or F_WORD)
//! \param[in] word Addr (address of memory)
//! \return word (content of the addressed memory location)
word ReadMem(word Format, word Addr)
{
    word TDOword;

    HaltCPU();

    ClrTCLK();
    IR_Shift(IR_CNTRL_SIG_16BIT);
    if  (Format == F_WORD)
    {
        DR_Shift16(0x2409);         // Set word read
    }
    else
    {
        DR_Shift16(0x2419);         // Set byte read
    }
    IR_Shift(IR_ADDR_16BIT);
    DR_Shift16(Addr);               // Set address to read
    IR_Shift(IR_DATA_TO_ADDR);
    SetTCLK();

    ClrTCLK();
    TDOword = DR_Shift16(0x0000);   // Shift out 16 bits, little Endian

    ReleaseCPU();
    return(Format == F_WORD ? TDOword : TDOword & 0x00FF);
}

/* unused function */
#if 0
//----------------------------------------------------------------------------
//! \brief This function reads an array of words from the memory.
//! \param[in] word StartAddr (Start address of memory to be read)
//! \param[in] word Length (Number of words to be read)
//! \param[out] word *DataArray (Pointer to array for the data)
void ReadMemQuick(word StartAddr, word Length, word *DataArray)
{
    word i;

    // Initialize reading:
    SetPC(StartAddr-4);
    HaltCPU();

    ClrTCLK();
    IR_Shift(IR_CNTRL_SIG_16BIT);
    DR_Shift16(0x2409);                    // Set RW to read
    IR_Shift(IR_DATA_QUICK);

    for (i = 0; i < Length; i++)
    {
        SetTCLK();
        DataArray[i] = DR_Shift16(0x0000); // Shift out the data
                                           // from the target.
        ClrTCLK();
    }
    ReleaseCPU();
}
#endif

//----------------------------------------------------------------------------
//! \brief This function performs a mass erase (with and w/o info memory) or a
//! segment erase of a FLASH module specified by the given mode and address.
//! Large memory devices get additional mass erase operations to meet the spec.
//! (Could be extended with erase check via PSA)
//! \param[in] word Mode (could be ERASE_MASS or ERASE_MAIN or ERASE_SGMT)
//! \param[in] word Addr (any address within the selected segment)
word EraseFLASH(word EraseMode, word EraseAddr)
{
    //word StrobeAmount = 4820;       // default for Segment Erase
    word StrobeAmount = 9628;       // default for Segment Erase
    word i, loopcount = 1;          // erase cycle repeating for Mass Erase
    word FCTL3_val = SegmentInfoAKey;    // SegmentInfoAKey holds Lock-Key for Info
                                         // Seg. A
    if ((EraseMode == ERASE_MASS) || (EraseMode == ERASE_MAIN))
    {
        loopcount = 15;             // additional cycles for erase.
    }

    // The start addr should not be lower than 0xc400.
    if (EraseAddr < 0xC400)
   	{
        return(STATUS_ERROR);
    }

    HaltCPU();                        // 1

    for (i = loopcount; i > 0; i--)
    {
        ClrTCLK();                    // 2   
        IR_Shift(IR_CNTRL_SIG_16BIT); // 3 
        DR_Shift16(0x2408);           // 4 set RW to write
        IR_Shift(IR_ADDR_16BIT);      // 5
        DR_Shift16(0x0128);           // 6 FCTL1 address
        IR_Shift(IR_DATA_TO_ADDR);    // 7
        DR_Shift16(EraseMode);        // 8 Enable erase mode
        SetTCLK();                    // 9
                                      
        ClrTCLK();                    // 10
        IR_Shift(IR_ADDR_16BIT);      // 11 
        DR_Shift16(0x012A);           // 12 FCTL2 address
        IR_Shift(IR_DATA_TO_ADDR);    // 13
        DR_Shift16(0xA540);           // 14 MCLK is source, DIV=1
        SetTCLK();                    // 15
                                      
        ClrTCLK();                    // 16
        IR_Shift(IR_ADDR_16BIT);      // 17
        DR_Shift16(0x012C);           // 18 FCTL3 address
        IR_Shift(IR_DATA_TO_ADDR);    // 19 
        DR_Shift16(FCTL3_val);        // 20 Clear FCTL3; F2xxx: Unlock Info-Seg. A by toggling LOCKA-Bit if required,
        SetTCLK();                    // 21

        ClrTCLK();                    // 22 
        IR_Shift(IR_ADDR_16BIT);      // 23 
        DR_Shift16(EraseAddr);        // 24 Set erase address
        IR_Shift(IR_DATA_TO_ADDR);    // 25
        DR_Shift16(0x55AA);           // 26 Dummy write to start erase
        SetTCLK();                    // 27 

        ClrTCLK();                    // 28
        IR_Shift(IR_CNTRL_SIG_16BIT); // 29
        DR_Shift16(0x2409);           // 30 Set RW to read
        FRAME_BITS_FCLK(StrobeAmount);// 31 - 34
        IR_Shift(IR_CNTRL_SIG_16BIT); // 35
        DR_Shift16(0x2408);           // 36 Set RW to write
        IR_Shift(IR_ADDR_16BIT);      // 37
        DR_Shift16(0x0128);           // 38 FCTL1 address
        IR_Shift(IR_DATA_TO_ADDR);    // 39
        DR_Shift16(0xA500);           // 40 Disable erase
        SetTCLK();                    // 41
    }
    // set LOCK-Bits again
    ClrTCLK();                         // 42 
    IR_Shift(IR_ADDR_16BIT);           // 43 
    DR_Shift16(0x012C);                // 44 FCTL3 address
    IR_Shift(IR_DATA_TO_ADDR);         // 45
    DR_Shift16(0xA510);                // 46 Disable FLASH Write acess, LOCK = 1
    SetTCLK();                         // 47

    ReleaseCPU();                      // 48
    return(STATUS_OK);
}

/* Comment out unused functions */
#if 0
//----------------------------------------------------------------------------
//! \brief This function performs an Erase Check over the given memory range
//! \param[in] word StartAddr (Start address of memory to be checked)
//! \param[in] word Length (Number of words to be checked)
//! \return word (STATUS_OK if erase check was successful, STATUS_ERROR 
//! otherwise)
word EraseCheck(word StartAddr, word Length)
{
    return (VerifyPSA(StartAddr, Length, 0));
}

//----------------------------------------------------------------------------
//! \brief This function performs a Verification over the given memory range
//! \param[in] word StartAddr (Start address of memory to be verified)
//! \param[in] word Length (Number of words to be verified)
//! \param[in] word *DataArray (Pointer to array with the data)
//! \return word (STATUS_OK if verification was successful, STATUS_ERROR
//! otherwise)
word VerifyMem(word StartAddr, word Length, word *DataArray)
{
    return (VerifyPSA(StartAddr, Length, DataArray));
}

//------------------------------------------------------------------------
//! \brief This Function unlocks segment A of the InfoMemory (Flash)
void UnlockInfoA(void)
{
    SegmentInfoAKey = 0xA540;
}
#endif

/****************************************************************************/
/*                         END OF SOURCE FILE                               */
/****************************************************************************/
