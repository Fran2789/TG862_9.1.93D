/*

Copyright (c) 2008-2016 ARRIS Enterprises, LLC

All rights reserved.

Redistribution and use in source and binary forms, with or without 
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, 
   this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice, 
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.
3. The name of the author may not be used to endorse or promote products 
   derived from this software without specific prior written permission.


Alternatively, this software may be distributed under the terms of the
GNU General Public License ("GPL") version 2 as published by the Free
Software Foundation. 
 

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED 
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. 
IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; 
OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR 
OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF 
ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

GPLv2 license:

This program is free software; you can redistribute it and/or modify it 
under the terms of the GNU General Public License as published by the 
Free Software Foundation; either version 2 of the License, or 
(at your option) any later version.

This program is distributed in the hope that it will be useful, but 
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY 
or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License 
for more details.

You should have received a copy of the GNU General Public License along with 
this program; if not, write to the Free Software Foundation, Inc., 
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

*/

/******************************************************************************
 * FILE PURPOSE:     - Battery Charger Telemetry
 ******************************************************************************
 * FILE NAME:     charger.c
 *
 * DESCRIPTION:   Linux Battery Charger Telemetry Module
 *                Provides drivers in kernel space to download FW to charger.
 *
 *                Author: Bill Mohr, ARRIS Group, Inc.
 *
 *******************************************************************************/

#define __KERNEL__ 1
#define __LINUX_ARM_ARCH__ 6

/* Start - Header Includes */

#include <asm/uaccess.h>
#include <asm-arm/arch-avalanche/generic/avalanche_intc.h>
#include "tlm_arris.h"

/* End - Header Includes */


// prototypes

boolean CHRGR_Gen4_txrx (boolean, uint8*, uint32);
uint8 CHRGR_ascii2hex (uint8*);
uint8 CHRGR_ascii2dec (uint8*);
boolean CHRGR_dnld (uint8*);


// externs
extern char CHRGR_702load[], CHRGR_702loadFactory[];
extern char CHRGR_722load[], CHRGR_722loadFactory[];
extern char CHRGR_802load[], CHRGR_802loadFactory[];
extern char CHRGR_804load[], CHRGR_804loadFactory[];
extern char CHRGR_822load[], CHRGR_822loadFactory[];
extern char CHRGR_852load[], CHRGR_852loadFactory[];
extern char CHRGR_862load[], CHRGR_862loadFactory[];
extern char CHRGR_872load[], CHRGR_872loadFactory[];
extern char CHRGR_1602load[], CHRGR_1602loadFactory[];
extern char CHRGR_1642load[], CHRGR_1642loadFactory[];
extern char CHRGR_1662load[], CHRGR_1662loadFactory[];
extern char CHRGR_1672load[], CHRGR_1672loadFactory[];
extern char CHRGR_1682load[], CHRGR_1682loadFactory[];
// extern char CHRGR_2472load[], CHRGR_2472loadFactory[];  // per Henry Sully, 2472 is Atmel only.  No Mot. Srec. needed.
extern char CHRGR_2492load[], CHRGR_2492loadFactory[];
extern char CHRGR_2402load[], CHRGR_2402loadFactory[];

extern TLM_derivedData *derivedData;
extern int charger;

static uint16 *crctable;


#define CHRGR_RD 1
#define CHRGR_WR 0

// the following value is processor speed dependant (but need not be too accurate)
#define START_BIT_TIMEOUT 11000000


uint32 CHRGR_debug = 0;
uint32 CHRGR_debug_delay = 52;  // 50ms

uint8 CHRGR_seq_baud[]    = {0x80};
uint8 CHRGR_seq_cksum[]   = {0x0e};
uint8 CHRGR_seq_freq[]    = {0x08, 0x0f, 0xfa, 0x02, 0x15, 0x9a};
uint8 CHRGR_seq_unlock1[] = {0x08, 0x0f, 0xf8, 0x01, 0x73};
uint8 CHRGR_seq_unlock2[] = {0x08, 0x0f, 0xf8, 0x01, 0x8c};
uint8 CHRGR_seq_eraseSel[]= {0x08, 0x0f, 0xf9, 0x01, 0x00};  // last byte specifies page
uint8 CHRGR_seq_erase[]   = {0x08, 0x0f, 0xf8, 0x01, 0x95};
uint8 CHRGR_seq_poll[]    = {0x09, 0x0f, 0xf8, 0x01};
uint8 CHRGR_seq_rdFlash[] = {0x0b, 0x00, 0x00, 0x00, 0x80};  // 0x80 says read 128 bytes

void CHRGR_fwVerGet (TLM_msg4_t* msg4)
{
    char *cp;
    uint8 buf[2], plat;

    plat = msg4->data[0];

    msg4->data[0] = msg4->data[1] = 0;

    switch(plat)
    {
        case PLAT_702:
            printk ("TLM: Get version for 702\n");
            cp = CHRGR_702load;
            break;
        case PLAT_722:
            printk ("TLM: Get version for 722\n");
            cp = CHRGR_722load;
            break;
        case PLAT_802:
            printk ("TLM: Get version for 802\n");
            cp = CHRGR_802load;
            break;
        case PLAT_804:
            printk ("TLM: Get version for 804\n");
            cp = CHRGR_804load;
            break;
        case PLAT_822:
            printk ("TLM: Get version for 822\n");
            cp = CHRGR_822load;
            break;
        case PLAT_852:
            printk ("TLM: Get version for 852\n");
            cp = CHRGR_852load;
            break;
        case PLAT_862:
            printk ("TLM: Get version for 862\n");
            cp = CHRGR_862load;
            break;
        case PLAT_872:
            printk ("TLM: Get version for 872\n");
            cp = CHRGR_872load;
            break;
        case PLAT_1602:
            printk ("TLM: Get version for 1602\n");
            cp = CHRGR_1602load;
            break;
        case PLAT_1642:
            printk ("TLM: Get version for 1642\n");
            cp = CHRGR_1642load;
            break;
        case PLAT_1662:
            printk ("TLM: Get version for 1662\n");
            cp = CHRGR_1662load;
            break;
        case PLAT_1672:
            printk ("TLM: Get version for 1672\n");
            cp = CHRGR_1672load;
            break;
        case PLAT_1682:
            printk ("TLM: Get version for 1682\n");
            cp = CHRGR_1682load;
            break;
//      case PLAT_2472:  // per Henry Sully, 2472 is Atmel only.  No Mot. Srec. needed.
//          printk ("TLM: Get version for 2472\n");
//          cp = CHRGR_2472load;
//          break;
        case PLAT_2492:
            printk ("TLM: Get version for 2492\n");
            cp = CHRGR_1682load;
            break;
        case PLAT_2402:
            printk ("TLM: Get version for 2402\n");
            cp = CHRGR_2402load;
            break;

        default:
            printk ("TLM: CHRGR_fwVerGet - unrecognized model\n");
            return;
    }

    do  // look for record for address 003e
    {
        if ( !(cp = strstr (cp, "S1")) )
        {
            printk ("Charger: No version number found in UPS code\n");
            return;
        }
        cp += 4;  // point to address in S1 record
    }
    while ( memcmp (cp, "003E", 4) );

    cp += 12;  // point to major revision
    buf[0] = CHRGR_ascii2hex (cp);            // ie. "30" becomes 0x30
    cp += 2;
    buf[1] = CHRGR_ascii2hex (cp);            // ie. "31" becomes 0x31

  // major revision
    msg4->data[0] = CHRGR_ascii2dec (buf);  // ie. {0x30, 0x31} becomes 0x01

    cp += 2;   // point to decimal

    if ( CHRGR_ascii2hex (cp) != '.' )   // sanity check
    {
        printk ("Charger: No decimal found in UPS version number\n");
        return;
    }

    cp += 2;  // point to minor revision
    buf[0] = CHRGR_ascii2hex (cp);
    cp += 2;
    buf[1] = CHRGR_ascii2hex (cp);

    msg4->data[1] = CHRGR_ascii2dec (buf);  // minor revision
}


// Find next S record.
// Return pntr to S record type or 0 if null terminator found
//
char *CHRGR_nextSrec (char* pSrec)
{
    for ( ; *pSrec; pSrec++ )
    {
        if ( pSrec[0] == 'S' )
        {
            return(pSrec + 1);
        }
    }
    return((char*) 0);
}



// convert 2 ascii characters to one hex byte.  Returns hex byte.
//
uint8 CHRGR_ascii2hex (uint8* pAscii)
{
    char c[3];

    memcpy (c, pAscii, 2);

    c[2] = 0;

    return((char) simple_strtol (c, 0, 16));
}

// convert 2 ascii characters to one decimal byte.  Returns hex byte.
//
uint8 CHRGR_ascii2dec (uint8* pAscii)
{
    char c[3];

    memcpy (c, pAscii, 2);

    c[2] = 0;

    return((char) simple_strtol (c, 0, 10));
}

// convert S1 record to hex
// wBuf points to destination for hex.   pSrec points to S1 record.
//
// returns length of record to be sent to battery charger (S1 checksum is discarded)
//
uint8 CHRGR_sRec2hex (uint8 *wBuf, uint8 *pSrec)
{
    uint8 len, numData;

  // subtract 1 for S1 record checksum and 2 for address to end up with number of data bytes
  //
    len = numData = CHRGR_ascii2hex (pSrec) - 3;
    pSrec += 2;

  // battery charger memory address for this record (16 bits)
  //
    *wBuf++ = CHRGR_ascii2hex (pSrec);
    pSrec += 2;
    *wBuf++ = CHRGR_ascii2hex (pSrec);
    pSrec += 2;

  // data length to be read by charger (16 bits)
  //
    *wBuf++ = 0;
    *wBuf++ = len;

    for ( ; numData; numData-- )
    {
        *wBuf++ = CHRGR_ascii2hex (pSrec);

        pSrec += 2;
    }

    return(len + 4); // add 2 for 16 bit address and 2 for 16 bit data byte count
}

void CHRGR_resetHi (void)
{
    if ( charger == CHARGER_ZILOG )
    {
        PAL_sysGpioCtrl (CHARGER_RESET, GPIO_PIN, GPIO_INPUT_PIN);
    }
}

void CHRGR_resetLo (void)
{
    if ( charger == CHARGER_ZILOG )
    {
        PAL_sysGpioOutBit (CHARGER_RESET, 0);
        PAL_sysGpioCtrl   (CHARGER_RESET, GPIO_PIN, GPIO_OUTPUT_PIN);
    }
}

void CHRGR_fwdnldHi (void)
{
    if ( charger == CHARGER_ZILOG )
    {
        PAL_sysGpioCtrl (CHARGER_DNLD, GPIO_PIN, GPIO_INPUT_PIN);
    }
}

void CHRGR_fwdnldLo (void)
{
    if ( charger == CHARGER_ZILOG )
    {
        PAL_sysGpioOutBit (CHARGER_DNLD, 0);
        PAL_sysGpioCtrl   (CHARGER_DNLD, GPIO_PIN, GPIO_OUTPUT_PIN);
    }
}

// place the charger into reset
void CHRGR_kill (void)
{
    CHRGR_resetLo();   // hold charger in reset
}



// Reset UPS (for initialization)
//
void CHRGR_UPSreset (void)
{
    CHRGR_resetLo();
    udelay(100);
    CHRGR_resetHi();
}



void CHRGR_debugDisplay (uint8 *data, uint32 len)
{
    uint32 i;
    uint8 *pBuf, buf[70];

    pBuf = buf;
    pBuf += sprintf (pBuf, "Output: ");

    for ( i = 1 ; i <= len; i++, data++ )
    {
        pBuf += sprintf (pBuf, "%02x ", (uint32) *data); 

        if ( !(i % 16) )
        {
            sprintf (pBuf, "\n");
            printk (buf);
            pBuf = buf;
            pBuf += sprintf (pBuf, "        ");
        }
    }
    if ( pBuf != buf )
    {
        sprintf (pBuf, "\n");
        printk (buf);
    }
    msleep(CHRGR_debug_delay);       // give time for printing to screen
}



// Performs crc on block of data
//
uint16 crc_ccitt (uint16 crc, uint8 *data, uint16 size)
{
    crc = ~crc;

    if ( CHRGR_debug )  CHRGR_debugDisplay (data, (uint16) 128);

    for ( ; size; size-- )
    {
        crc = (crc >> 8) ^ crctable[(*data++ ^ crc) & 0xff];
    }

    return ~crc;
}


#define CRC_CCITT_POLY 0x8408

void gen_crctable (uint16 *table)
{
    int i, j;
    uint16 crc;

    for ( i = 0; i < 256; i++ )
    {
        crc = i;

        for ( j = 0; j < 8; j++ )
        {
            if ( crc & 0x01 )
            {
                crc >>= 1;  crc ^= CRC_CCITT_POLY;
            }
            else
            {
                crc >>= 1;
            }
        }
        table[i] = crc;
    }
}



// Main download function.
//
// Interrupt locking is performed here and in CHRGR_Gen4_txrx.
// CHRGR_Gen4_txrx locks interrupts whenever writing.
//
boolean CHRGR_dnld (uint8 *CHRGR_code)
{
    uint32 int_status, debug_status, i;
    uint16 chrgrCksum, chargerFlashSize;
    uint8 len, cBuf[140], *pSrec, page, endPage;
    boolean result;
    uint16 srecCksum = 0;
    boolean resultOk = TRUE;

    // place charger into debug mode
    {
        PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &int_status);
        CHRGR_resetLo();
        CHRGR_fwdnldLo();

        udelay(100);

        CHRGR_resetHi();
        PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, int_status);

        msleep(10);
        CHRGR_fwdnldHi();

        udelay(100);
    }

    // establish baud rate
    CHRGR_Gen4_txrx (CHRGR_WR, CHRGR_seq_baud,    (uint32) 1);
    CHRGR_Gen4_txrx (CHRGR_WR, CHRGR_seq_freq,    (uint32) 6);

    endPage = 0x0e;   // erase pages 0 - 0x0e
    for ( page = 0; page <= endPage; page++ )
    {
        CHRGR_seq_eraseSel [4] = page;
        CHRGR_Gen4_txrx (CHRGR_WR, CHRGR_seq_eraseSel,(uint32) 5);

        // unlock flash control register
        CHRGR_Gen4_txrx (CHRGR_WR, CHRGR_seq_unlock1, (uint32) 5);
        CHRGR_Gen4_txrx (CHRGR_WR, CHRGR_seq_unlock2, (uint32) 5);

        CHRGR_Gen4_txrx (CHRGR_WR, CHRGR_seq_eraseSel,(uint32) 5);
        CHRGR_Gen4_txrx (CHRGR_WR, CHRGR_seq_erase,   (uint32) 5);


    // Poll until finished.
    // Interrupts must be locked for each poll.  This results in nesting
    // of locks during writes.  (not a problem)
    // Debug printing is inhibited while interrupts are locked.
    //
        for ( i = 0, cBuf[0] = 1; ; i++ )
        {
            msleep(5);  // give battery charger a chance to get something done

            PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &int_status);
            debug_status = CHRGR_debug; // no debug printing
            CHRGR_debug = 0;
            {
                CHRGR_Gen4_txrx (CHRGR_WR, CHRGR_seq_poll,  (uint32) 4);
                result = CHRGR_Gen4_txrx (CHRGR_RD, cBuf, (uint32) 1);
            }
            CHRGR_debug = debug_status;
            PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, int_status);

      // should be ~1.5 second bailout
      //
            if ( cBuf[0] == 0 )
            {
                break;   // success
            }
            else if ( !result )
            {
                printk ("Charger: flash erase failed - no response from charger\n");

                resultOk = FALSE;
            }
            else if ( i == 300 )
            {
                printk ("Charger: flash erase failed - charger took too long\n");

                resultOk = FALSE;
            }
            else if ( cBuf[0] == 0xff )
            {
                printk ("Charger: flash erase failed - charger not in debug mode\n");

                resultOk = FALSE;
            }
            else if ( (cBuf[0] < 0x10) || (cBuf[0] > 0x17) )
            {
                printk ("Charger: flash erase failed - charger returned %x\n", cBuf[0]);

                resultOk = FALSE;
            }

            if ( resultOk == FALSE )
            {
                printk ("Charger: failed flash erase\n");

                return FALSE;
            }
        }
    }

    if ( CHRGR_debug ) // debug only
    {
        printk ("Battery charger confirmed flash erase i=%d\n", i);
        msleep(CHRGR_debug_delay);       // give time for printing to screen
    }

    if ( CHRGR_code == NULL )
    {
        // we were only supposed to erase, not reprogram 
        return TRUE;
    }

    // unlock flash control register
    CHRGR_Gen4_txrx (CHRGR_WR, CHRGR_seq_unlock1, (uint32) 5);
    CHRGR_Gen4_txrx (CHRGR_WR, CHRGR_seq_unlock2, (uint32) 5);

    // download S records
    //
    for ( (pSrec = CHRGR_code); (pSrec = CHRGR_nextSrec (pSrec)); )
    {
        switch ( *pSrec ) // S record type
        {
        case '1':
            pSrec++;  // pSrec now points to length field of S1 record

            cBuf[0] = 0x0a;  // code to program flash record

            len = CHRGR_sRec2hex (cBuf+1, pSrec);

            CHRGR_Gen4_txrx (CHRGR_WR, cBuf, (uint32) (len + 1)); // +1 for 0x0a code

            // -2 for 16 bit added length.  *2 for ascii->hex conversion
            pSrec += (2 * (len - 2));

            break;

        case 'A':   // proprietary S record type that contains 16 bit checksum
            pSrec++;  // pSrec now points to 1st byte of cksum

            srecCksum = 0xffff;  // last 2 bytes will show that cksum was received

            ((uint8 *) (&srecCksum))[0] = CHRGR_ascii2hex (pSrec);
            pSrec += 2;
            ((uint8 *) (&srecCksum))[1] = CHRGR_ascii2hex (pSrec);

            break;

        case '0':  // charger FW version info
        default:
            break;
        }
    }

    chrgrCksum = 0;

    if ( !(crctable = (uint16 *) kmalloc(sizeof(uint16) * 256, GFP_KERNEL)) )
    {
        printk ("CHRGR_dnld: kmalloc failed\n");
        return FALSE;
    }
    gen_crctable (crctable);

    chargerFlashSize = 7680;

    // Read 128 bytes at a time & perform checksum on a total of:
    // TM502P - 7680 bytes    TM502G - 4096 bytes
    for ( i = 0; i < chargerFlashSize; i += 128 )
    {
        // Read a chunk of data
        PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &int_status);
        debug_status = CHRGR_debug; // no debug printing
        CHRGR_debug = 0;
        {
            CHRGR_seq_rdFlash [1] = (uint8) (i >> 8);  // upper byte starting address
            CHRGR_seq_rdFlash [2] = (uint8) i;         // lower byte starting address
            CHRGR_Gen4_txrx (CHRGR_WR, CHRGR_seq_rdFlash,  (uint32) 5);

            result = CHRGR_Gen4_txrx (CHRGR_RD, cBuf, (uint32) 128);
        }
        CHRGR_debug = debug_status;
        PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, int_status);

        // Calculate checksum on the data
        if ( result )
        {
            chrgrCksum = crc_ccitt (chrgrCksum, cBuf, (uint16) 128);

            if ( CHRGR_debug ) 
            {    
               printk ("Checksum: %04x\n", chrgrCksum);
            }

            msleep(25);
        }
        else
        {
            printk ("Charger: checksum error\n");
            kfree (crctable);
            return FALSE;
        }
    }
    kfree (crctable);


    if ( CHRGR_debug )
    {
        uint16 localSum;

        PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &int_status);
        debug_status = CHRGR_debug; // no debug printing
        CHRGR_debug = 0;

        CHRGR_Gen4_txrx (CHRGR_WR, CHRGR_seq_cksum,  (uint32) 1);  
        result = CHRGR_Gen4_txrx (CHRGR_RD, (uint8 *)(&localSum), (uint32) 2);

        CHRGR_debug = debug_status;
        PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, int_status);

        if ( result ) 
        {
            printk ("Charger Calculated Checksum: %04x\n", localSum);
        }
        else 
        {
            printk ("Failed to read Charger Calculated Checksum\n");
        }
    }


    PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &int_status);
    {
        CHRGR_UPSreset();   // exit debug mode

        // set to input so that we can monitor charger for eminent shutdown
        if ( charger == CHARGER_ZILOG )
        {
            PAL_sysGpioCtrl (CHARGER_RESET, GPIO_PIN, GPIO_INPUT_PIN);
        }
        // set to input
        if ( charger == CHARGER_ZILOG )
        {
            PAL_sysGpioCtrl (CHARGER_DNLD, GPIO_PIN, GPIO_INPUT_PIN);
        }
    }
    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, int_status);

    // validate checksum
    //
    if ( chrgrCksum == srecCksum )
    {
        return TRUE;
    }
    else
    {
        printk ("Checksum Mismatch - Charger: %04x  Srec: %04x\n", chrgrCksum, srecCksum);
        return FALSE;
    }
}



// for designer and factory testing
//
void CHRGR_debug1(void)
{
    CHRGR_debug = 1;
}
void CHRGR_debug0(void)
{
    CHRGR_debug = 0;
}
boolean CHRGR_erase(void)
{
    return(CHRGR_dnld (NULL));
}

boolean CHRGR_download (uint8 plat)
{
    if ( plat == PLAT_702 )
    {
        printk ("TLM: Downloading for 702\n");
        return(CHRGR_dnld (CHRGR_702load));
    }
    if ( plat == PLAT_722 )
    {
        printk ("TLM: Downloading for 722\n");
        return(CHRGR_dnld (CHRGR_722load));
    }
    if ( plat == PLAT_802 )
    {
        printk ("TLM: Downloading for 802\n");
        return(CHRGR_dnld (CHRGR_802load));
    }
    if ( plat == PLAT_804 )
    {
        printk ("TLM: Downloading for 804\n");
        return(CHRGR_dnld (CHRGR_804load));
    }
    if ( plat == PLAT_822 )
    {
        printk ("TLM: Downloading for 822\n");
        return(CHRGR_dnld (CHRGR_822load));
    }
    if ( plat == PLAT_852 )
    {
        printk ("TLM: Downloading for 852\n");
        return(CHRGR_dnld (CHRGR_852load));
    }
    if ( plat == PLAT_862 )
    {
        printk ("TLM: Downloading for 862\n");
        return(CHRGR_dnld (CHRGR_862load));
    }
    if ( plat == PLAT_872 )
    {
        printk ("TLM: Downloading for 872\n");
        return(CHRGR_dnld (CHRGR_872load));
    }
    if ( plat == PLAT_1602 )
    {
        printk ("TLM: Downloading for 1602\n");
        return(CHRGR_dnld (CHRGR_1602load));
    }
    if ( plat == PLAT_1642 )
    {
        printk ("TLM: Downloading for 1642\n");
        return(CHRGR_dnld (CHRGR_1642load));
    }
    if ( plat == PLAT_1662 )
    {
        printk ("TLM: Downloading for 1662\n");
        return(CHRGR_dnld (CHRGR_1662load));
    }
    if ( plat == PLAT_1672 )
    {
        printk ("TLM: Downloading for 1672\n");
        return(CHRGR_dnld (CHRGR_1672load));
    }
    if ( plat == PLAT_1682 )
    {
        printk ("TLM: Downloading for 1682\n");
        return(CHRGR_dnld (CHRGR_1682load));
    }
//  if ( plat == PLAT_2472 )   // per Henry Sully, 2472 is Atmel only.  No Mot. Srec. needed.
//  {
//      printk ("TLM: Downloading for 2472\n");
//      return(CHRGR_dnld (CHRGR_2472load));
//  }
    if ( plat == PLAT_2492 )
    {
        printk ("TLM: Downloading for 2492\n");
        return(CHRGR_dnld (CHRGR_1682load));
    }
    if ( plat == PLAT_2402 )
    {
        printk ("TLM: Downloading for 2402\n");
        return(CHRGR_dnld (CHRGR_2402load));
    }

    printk ("TLM: Unrecognized model for download\n");

    return 0;
}

boolean CHRGR_downloadFactory (uint8 plat)
{
    if ( plat == PLAT_702 )
    {
        printk ("TLM: Downloading 702 Test\n");
        return(CHRGR_dnld (CHRGR_702loadFactory));
    }
    if ( plat == PLAT_722 )
    {
        printk ("TLM: Downloading 722 Test\n");
        return(CHRGR_dnld (CHRGR_722loadFactory));
    }
    if ( plat == PLAT_802 )
    {
        printk ("TLM: Downloading 802 Test\n");
        return(CHRGR_dnld (CHRGR_802loadFactory));
    }
    if ( plat == PLAT_804 )
    {
        printk ("TLM: Downloading 804 Test\n");
        return(CHRGR_dnld (CHRGR_804loadFactory));
    }
    if ( plat == PLAT_822 )
    {
        printk ("TLM: Downloading 822 Test\n");
        return(CHRGR_dnld (CHRGR_822loadFactory));
    }
    if ( plat == PLAT_852 )
    {
        printk ("TLM: Downloading 852 Test\n");
        return(CHRGR_dnld (CHRGR_852loadFactory));
    }
    if ( plat == PLAT_862 )
    {
        printk ("TLM: Downloading 862 Test\n");
        return(CHRGR_dnld (CHRGR_862loadFactory));
    }
    if ( plat == PLAT_872 )
    {
        printk ("TLM: Downloading 872 Test\n");
        return(CHRGR_dnld (CHRGR_872loadFactory));
    }
    if ( plat == PLAT_1602 )
    {
        printk ("TLM: Downloading 1602 Test\n");
        return(CHRGR_dnld (CHRGR_1602loadFactory));
    }
    if ( plat == PLAT_1642 )
    {
        printk ("TLM: Downloading 1642 Test\n");
        return(CHRGR_dnld (CHRGR_1642loadFactory));
    }
    if ( plat == PLAT_1662 )
    {
        printk ("TLM: Downloading 1662 Test\n");
        return(CHRGR_dnld (CHRGR_1662loadFactory));
    }
    if ( plat == PLAT_1672 )
    {
        printk ("TLM: Downloading 1672 Test\n");
        return(CHRGR_dnld (CHRGR_1672loadFactory));
    }
    if ( plat == PLAT_1682 )
    {
        printk ("TLM: Downloading 1682 Test\n");
        return(CHRGR_dnld (CHRGR_1682loadFactory));
    }
//  if ( plat == PLAT_2472 )    // per Henry Sully, 2472 is Atmel only.  No Mot. Srec. needed.
//  {
//      printk ("TLM: Downloading 2472 Test\n");
//      return(CHRGR_dnld (CHRGR_2472loadFactory));
//  }
    if ( plat == PLAT_2492 )
    {
        printk ("TLM: Downloading 2492 Test\n");
        return(CHRGR_dnld (CHRGR_1682loadFactory));
    }
    if ( plat == PLAT_2402 )
    {
        printk ("TLM: Downloading 2402 Test\n");
        return(CHRGR_dnld (CHRGR_2402loadFactory));
    } 

    printk ("TLM: Unrecognized model for test download\n");

    return 0;
}

// Read/Write designated number of bytes from/to the charger via GPIO.
//
// Reads & writes are performed via the same function to ensure consistent
// timing (baud rate) for both read and write.
// ie. write goes through all the steps of read (in addition to write)
//     read goes through all the steps of write (in addition to read)
//
// Interrupts are locked here when writing.  When reading, interrupt locking is
// performed at a higher level.  (It reduces code to do it this way.)
//
// Returns: TRUE  = success
//          FALSE = failure
//
boolean CHRGR_Gen4_txrx (boolean mode, uint8 *data, uint32 len)
{
    uint32 i, reg, numBits, int_status, outData, inData;

    // to silence warnings from compiler & coverity (code inspection tool)
    int_status = outData = inData = 0;
    if ( charger != CHARGER_ZILOG )
    {
        printk(" ERROR %s : %s should never be here\n", __FILE__, __FUNCTION__);
        return TRUE;
    }

    if ( mode == CHRGR_WR )
    {
        if ( CHRGR_debug )    CHRGR_debugDisplay (data, len);

        PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &int_status);
    }

    for ( ; len; len--, data++ )
    {
        if ( mode == CHRGR_RD )
        {
            numBits = 9;  // read 9 bits (1 start, 8 data, 0 stop)

            // set to input
            PAL_sysGpioCtrl (CHARGER_DNLD, GPIO_PIN, GPIO_INPUT_PIN);

            // Look for start bit.  Bail if timeout (~2 second)
            for ( i = 0; PAL_sysGpioInBit (CHARGER_DNLD); i++ )
            {
                if ( i >= START_BIT_TIMEOUT )
                {
                    return FALSE;
                }
            }
        }
        else  // writeMode
        {
            numBits = 10;  // write 10 bits (1 start, 8 data, 1 stop)

            // Adds start bit, stop bit and preshifts 1 to the left.
            // Start bit is low, stop bit is high.  LSB is xmitted first.
            outData = ((uint32) *data << 2) | 0xc00;

            // set to output high
            //
            PAL_sysGpioOutBit (CHARGER_DNLD, 1);
            PAL_sysGpioCtrl   (CHARGER_DNLD, GPIO_PIN, GPIO_OUTPUT_PIN);
        }

        // The following is a critical section.  It needs to be consistent in execution time
        // between writes and reads.  This means no branches.   ie. no if statements
        //
        for ( i = 0; i < numBits; i++ )
        {
          // Read Mode:  Split read delay so that reads sample in the middle of the bit
          // Write Mode: Xmit 1/2 bit.  This completes the last bit xmitted.  If this is
          //    is the 1st bit of the byte, it will be high.  This completes the stop
          //    bit from the last byte xmitted.  If this is the 1st byte xmitted, the
          //    xmission is simply delayed by 1/2 bit time.  There is a Zilog requirement
          //    to only xmit 1/2 stop bit when we expect Zilog to xmit next.  If we
          //    xmit next this delay completes the stop bit from the previous xmission.
          //    If Zilog xmits next we release the line after 1/2 stop bit.
            udelay(5);

            // Read Section
            {
                reg = PAL_sysGpioInBit (CHARGER_DNLD);

                inData |= (reg << 31);    // or it into bit 31

                inData >>= 1;
            }

            // Write Section
            {
                outData >>= 1;

                // Warning:  do not use PAL_sysGpioOutBit() here - contains different execution
                // paths (depending upon whether a 1 or 0 is to be output)
                #if defined (CONFIG_MACH_PUMA5)
                PAL_sysGpioOutValue ((outData & 0x01) << CHARGER_DNLD_RAW, 1 << CHARGER_DNLD_RAW, 1);
                #endif
            }

            udelay(5); // half bit of delay
        }
        // end of critical section

        if ( mode == CHRGR_RD )
        {
            *data = (uint8) (inData >> 23);

            // Ensure we do not mistake the end of this bit as start of next byte.
            // Note: this assumes an inter byte time of at least 1/2 bit time
            udelay(5);
        }
    }

    // Release the transmitter (set to input).  Line will float high.
    //
    if ( mode == CHRGR_WR )
    {
        PAL_sysGpioCtrl (CHARGER_DNLD,  GPIO_PIN, GPIO_INPUT_PIN);

        PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, int_status);
    }

    return TRUE;
}


//
// End Charger Flash Download
//
/////////////////////////////////////////////////////////////////////////////


