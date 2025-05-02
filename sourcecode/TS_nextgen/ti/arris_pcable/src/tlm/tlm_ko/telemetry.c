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
 * FILE NAME:     ups_manager.c                                                        
 *                                                                                 
 * DESCRIPTION:   Linux Battery Charger Telemetry Module
 *                Provides drivers in kernel space to obtain runtime battery
 *                related data from charger.
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


// externs
extern int charger;


#define BAT2_BIT 0x10

#define TLM_ROM_LEN   8 /* includes 8 bit family code, 48 bit id number & 8 bit crc */
#define TLM_PAGE_LEN  sizeof(TLM_eprom_t)
#define UART_MAX_TRIES 50



enum
{
   REQ_DATA,   // Request Data
   COMMAND,    // Command
   CMD_DATA,   // Command with Data
   CMD_DUMP    // Command to dump data to screen
};


// Determines whether to poll for 2nd battery information.
enum
{
   TLM_NEVER,
   TLM_ALWAYS,
   TLM_IF_2ND  // if unit can accomodate a 2nd battery
};




typedef void (timerFcn)(unsigned long);
 



// Defines an attribute to be polled periodically
// arrays are indexed by battery number
//
typedef struct
{
   uint8 index;       // cmd index
   uint8 code;        // hex value associated with this request (has meaning to UPS)
   uint8 data[2];     // data values to set or get at runtime - depending upon cmdType
   uint8 sem[2];      // used as a semaphore & todo flag (runtime use)
   uint8 dataValid[2];// data was successfully read on the last attempt

   uint8 cmdType:2;   // Request Data / Command / Command with Data
   uint8 last:1;      // designates entry as last in table
   uint8 facTest:1;   // factory test related.  This is only used as a double check.
   uint8 rdBat2:2;    // determines under what conditions to poll value for 2nd battery
} TLM_REQ_t;


// First entry must be the one that gets the general status
// Last entry must be flagged by setting "last" bit
// Must be in the order defined by enum
//
TLM_REQ_t tlm_todo[] = {
 { TLM_RD_TELEM,
                                0x07, {0,0}, {0,0}, {0,0}, REQ_DATA, 0, 0, TLM_IF_2ND },
 { TLM_RD_RATED_BATT_CAPACITY,
                                0x00, {0,0}, {0,0}, {0,0}, REQ_DATA, 0, 0, TLM_IF_2ND },
 { TLM_RD_TESTED_BATT_CAPACITY,
                                0x01, {0,0}, {0,0}, {0,0}, REQ_DATA, 0, 0, TLM_IF_2ND },
 { TLM_RD_BATT_STATE_OF_CHARGE,
                                0x02, {0,0}, {0,0}, {0,0}, REQ_DATA, 0, 0, TLM_IF_2ND },
 { TLM_RD_BATT_POWER,
                                0x03, {0,0}, {0,0}, {0,0}, REQ_DATA, 0, 0, TLM_ALWAYS },
 { TLM_RD_SET_CHG_STATE,
                                0x04, {0,0}, {0,0}, {0,0}, REQ_DATA, 0, 0, TLM_IF_2ND },
 { TLM_RD_TYPICAL_IDLE_POWER,
                                0x05, {0,0}, {0,0}, {0,0}, REQ_DATA, 0, 0, TLM_ALWAYS },
 { TLM_RD_REPL_BATT_THRESH,
                                0x06, {0,0}, {0,0}, {0,0}, REQ_DATA, 0, 0, TLM_IF_2ND },
 { TLM_RD_TEST_TIMER,
                                0x08, {0,0}, {0,0}, {0,0}, REQ_DATA, 0, 0, TLM_ALWAYS },
 { TLM_RD_FULL_CHG_TIME,
                                0x09, {0,0}, {0,0}, {0,0}, REQ_DATA, 0, 0, TLM_NEVER  },
 { TLM_RD_LOW_BATT_THRESH,
                                0x0a, {0,0}, {0,0}, {0,0}, REQ_DATA, 0, 0, TLM_ALWAYS },
 { TLM_RD_CHG_FAILURE_STATUS,
                                0x0b, {0,0}, {0,0}, {0,0}, REQ_DATA, 0, 0, TLM_ALWAYS },
 { TLM_RD_SEC_BATT_HIGH_BYTE,
                                0x0c, {0,0}, {0,0}, {0,0}, REQ_DATA, 0, 0, TLM_ALWAYS },
 { TLM_RD_SEC_BATT_LOW_BYTE,
                                0x0d, {0,0}, {0,0}, {0,0}, REQ_DATA, 0, 0, TLM_ALWAYS },
 { TLM_RD_SYNC_TIMER,
                                0x0e, {0,0}, {0,0}, {0,0}, REQ_DATA, 0, 0, TLM_ALWAYS },
 { TLM_RD_BATT_VOLTAGE,
                                0x0f, {0,0}, {0,0}, {0,0}, REQ_DATA, 0, 0, TLM_ALWAYS },
 { TLM_CMD_PAUSE_TEST_TIMER,
                                0x80, {0,0}, {0,0}, {0,0}, COMMAND,  0, 0, TLM_NEVER  },
 { TLM_CMD_RESUME_TEST_TIMER,
                                0x81, {0,0}, {0,0}, {0,0}, COMMAND,  0, 0, TLM_NEVER  },
 { TLM_CMD_FF_TEST_TIMER,
                                0x82, {0,0}, {0,0}, {0,0}, COMMAND,  0, 0, TLM_NEVER  },
 { TLM_CMD_TURN_OFF_UPS,
                                0x83, {0,0}, {0,0}, {0,0}, COMMAND,  0, 0, TLM_NEVER  },
 { TLM_CMD_TURN_ON_UPS,
                                0x84, {0,0}, {0,0}, {0,0}, COMMAND,  0, 0, TLM_NEVER  },
 { TLM_CMD_TOGGLE_HI_TEMP_SHUT,
                                0x85, {0,0}, {0,0}, {0,0}, COMMAND,  0, 0, TLM_NEVER  },
 { TLM_CMD_TOGGLE_HI_TEMP_BATT_SHUT,
                                0x85, {0,0}, {0,0}, {0,0}, COMMAND,  0, 0, TLM_NEVER  },
 { TLM_CMD_CALIBRATE_TIMER,
                                0x8f, {0,0}, {0,0}, {0,0}, COMMAND,  0, 0, TLM_NEVER  },
 { TLM_SET_CHG_STATE,
                                0xc4, {0,0}, {0,0}, {0,0}, CMD_DATA, 0, 0, TLM_IF_2ND },
 { TLM_SET_REPL_BATT_THRESH,
                                0xc6, {0,0}, {0,0}, {0,0}, CMD_DATA, 0, 0, TLM_IF_2ND },
 { TLM_SET_FULL_CHG_STATE,
                                0xc9, {0,0}, {0,0}, {0,0}, CMD_DATA, 0, 0, TLM_NEVER  },
 { TLM_SET_LOW_BATT_THRESH,
                                0xca, {0,0}, {0,0}, {0,0}, CMD_DATA, 0, 0, TLM_IF_2ND },
 { TLM_SET_BATT_INDEX_CODE,
                                0xd5, {0,0}, {0,0}, {0,0}, CMD_DATA, 0, 0, TLM_IF_2ND },
 { 0,                           0x00, {0,0}, {0,0}, {0,0}, REQ_DATA, 1, 0, TLM_NEVER  } };


// First entry must be the one that gets the general status
// Last entry must be flagged by setting "last" bit
// Must be in the order defined by enum
//
TLM_REQ_t tlm_facTodo[] = {
 { TLM_FT_REQ_STATUS, 0xf1, {0,0}, {0,0}, {0,0}, REQ_DATA, 0, 1, TLM_NEVER  },
 { TLM_FT_REQ_CNT_HI, 0xf2, {0,0}, {0,0}, {0,0}, REQ_DATA, 0, 1, TLM_NEVER  },
 { TLM_FT_REQ_CNT_LO, 0xf3, {0,0}, {0,0}, {0,0}, REQ_DATA, 0, 1, TLM_NEVER  },
 { TLM_FT_BEGIN_TEST, 0xf0, {0,0}, {0,0}, {0,0}, COMMAND,  0, 1, TLM_NEVER  },
 { TLM_FT_REMOVE_DC,  0xf5, {0,0}, {0,0}, {0,0}, COMMAND,  0, 1, TLM_NEVER  },
 { TLM_FT_DUMP_DATA,  0xf4, {0,0}, {0,0}, {0,0}, CMD_DUMP, 0, 1, TLM_NEVER  },
 { 0,                 0x00, {0,0}, {0,0}, {0,0}, REQ_DATA, 1, 1, TLM_NEVER  } };



boolean AC_ok;
boolean TLM_testMode;
uint16  TLM_maxBat;

static uint16 dataDumpCnt = 0;
uint32 TLM_uartMaxTries = 249; // try for 20s
uint8 TLM_chargerResets = 0;   // incremented when a charger has been reset
uint8 TLM_chargerPings = 0;    // incremented when a charger needs to be pinged
uint8 TLM_DebugVal = 0;

uint8 TLM_epromRom1 [TLM_ROM_LEN];


// prototypes
boolean TLM_pollUPS (uint16);
boolean TLM_uartOut (uint8 data);
boolean TLM_uartIn (uint8 *data);
boolean TLM_uart_byte (uint8 *data);
boolean TLM_uart_cmd (TLM_REQ_t*, uint16);
void    TLM_mk_req (uint8, uint8*, uint16);

void TLM_data2userMsg (TLM_msg2_t*);

TLM_REQ_t *get_pTlm_req (void);
uint16 get_tlm_max (void);
void TLM_invalidateCrc (uint8*);
void TLM_nullRom (uint8*);
//void TLM_communicationCheck (void);


uint32 TLM_configSclkHz = 112500000; //PUMA6_UART0_CLK_VAL

/* baud function copy from ti_psp_uboot/psp_uboot/src/u-boot-1.2.0/cpu/bf533/bf533_serial.h .c */

int baud_table[5] = {9600, 19200, 38400, 57600, 115200};

struct {
        unsigned char dl_high;
        unsigned char dl_low;
} hw_baud_table[5];

void calc_baud(void)
{
        unsigned char i;
        int     temp;

        for(i = 0; i < sizeof(baud_table)/sizeof(int); i++) {
                temp =  TLM_configSclkHz/(baud_table[i]*8);
                if ( temp && 0x1 == 1 ) {
                        temp++;
                }
                temp = temp/2;
                hw_baud_table[i].dl_high = (temp >> 8)& 0xFF;
                hw_baud_table[i].dl_low = (temp) & 0xFF;
        }
}


//  Send telemetry data once every polling period to user space
//  Note: currently supports only one battery
//
void TLM_data2userMsg (TLM_msg2_t *msg2)
{
    uint16 i, max;
    TLM_REQ_t *pTlm_req;

    pTlm_req = get_pTlm_req();

    max = get_tlm_max();

    for (i = 0; i <= max; i++)
    {
        msg2->data [(i*2)    ] = pTlm_req[i].data[0];
        msg2->data [(i*2) + 1] = pTlm_req[i].data[1];
    }
}


/****************************************************************************/
//
//  Function:       TLM_uartInit
//  Description:    Initialize UART0
//                  sets divider based on perifial clock of 100MHz
//                  to produce 19.2kb/s
//
/****************************************************************************/
void TLM_uartInit (void)
{
  uint8 uartBaudRateLo = 0x45;
  uint8 uartBaudRateHi = 0x01;

  #if defined (CONFIG_MACH_PUMA6)
  calc_baud();
  uartBaudRateLo = hw_baud_table[1].dl_low;
  uartBaudRateHi = hw_baud_table[1].dl_high;
  //printk("uartBaudRateLo = 0x%x, uartBaudRateHi = 0x%x\n", uartBaudRateLo, uartBaudRateHi);
  #endif

  *(volatile uint32 *)(AVALANCHE_UART0_REGS_BASE+0xc) = 0x83;  // enable access
  *(volatile uint32 *)(AVALANCHE_UART0_REGS_BASE+0x0) = uartBaudRateLo;
  *(volatile uint32 *)(AVALANCHE_UART0_REGS_BASE+0x4) = uartBaudRateHi;
  *(volatile uint32 *)(AVALANCHE_UART0_REGS_BASE+0xc) = 0x3;   // lock

  // set up for fifo mode
  *(volatile uint32 *)(AVALANCHE_UART0_REGS_BASE+0x8) = 0x1;

  msleep (50);

  printk ("TLM: Initializing UART\n");
}


/****************************************************************************/
//
//  Function:       ups_main
//  Description:    Initializes the telemetry task.
//
/****************************************************************************/
void TLM_init (void)
{
    // Note: Charger reset line is set to input because it must be monitored.  When
    // the UPS determines there are 10 seconds before low battery shutdown, UPS will
    // drive the line low.
    if (charger == CHARGER_ZILOG)
    {
        PAL_sysGpioCtrl (CHARGER_RESET, GPIO_PIN, GPIO_INPUT_PIN);
        PAL_sysGpioCtrl (CHARGER_DNLD,  GPIO_PIN, GPIO_INPUT_PIN);
    }

    #if defined (CONFIG_MACH_PUMA5)
    PAL_sysGpioCtrl (BATTERY_EPROM, GPIO_PIN, GPIO_INPUT_PIN); 
    #else
    PAL_sysGpioCtrl (BATTERY_EPROM, GPIO_PIN, GPIO_OUTPUT_PIN);
    #endif

    #if defined (CONFIG_MACH_PUMA5)
    // Note: the parameter "GPIO_OUTPUT_PIN" does not matter for the following
    PAL_sysGpioCtrl( CHARGER_UART_TX,  FUNCTIONAL_PIN, GPIO_OUTPUT_PIN); /* UART-B_TD */
    PAL_sysGpioCtrl( CHARGER_UART_RX,  FUNCTIONAL_PIN, GPIO_OUTPUT_PIN); /* UART-B_RD */
//  PAL_sysGpioCtrl( CHARGER_UART_CTS, FUNCTIONAL_PIN, GPIO_OUTPUT_PIN); /* UART-B_CTS */
//  PAL_sysGpioCtrl( CHARGER_UART_RTS, FUNCTIONAL_PIN, GPIO_OUTPUT_PIN); /* UART-B_RTS */
    #endif

#if 0
    #define CHRGR_PULLDOWN_REG 0xa8610a08
    #define CHRGR_PULLDOWN_VAL 0x01100000

    // disable pulldowns for firmware download & reset gpios
    uint32 *ip = (uint32 *)CHRGR_PULLDOWN_REG;
    *ip |= CHRGR_PULLDOWN_VAL;
#endif
    if (charger == CHARGER_ZILOG)
    {
        PAL_sysGpioOutBit (CHARGER_RESET, 1);  // charger reset line high
        PAL_sysGpioOutBit (CHARGER_DNLD,  1);  // FW download line high
    }

    #if defined (CONFIG_MACH_PUMA6)
    TLM_configSclkHz = PAL_sysClkcGetFreq(PAL_SYS_CLKC_UART0);
    #endif

    TLM_uartInit();

    TLM_maxBat = 1;  // change later if more than 1 battery is supported
    TLM_testMode = FALSE;
    AC_ok = TRUE;

    // Set direction to read so line goes high due to pull-up.  Set bit to low so that
    // whenever direction is set to write the line is driven low.
    #if defined (CONFIG_MACH_PUMA5)
    PAL_sysGpioCtrl (BATTERY_EPROM, GPIO_PIN, GPIO_INPUT_PIN);
    #else
    PAL_sysGpioCtrl (BATTERY_EPROM, GPIO_PIN, GPIO_OUTPUT_PIN);
    #endif
    PAL_sysGpioOutBit (BATTERY_EPROM,  0);                     // bit low

    TLM_invalidateCrc (TLM_epromRom1);

    if (charger == CHARGER_ATMEL) 
    {
        PAL_sysGpioOutBit (CHARGER_RESET, 1);
        PAL_sysGpioOutBit (CHARGER_DNLD, 1);
        PAL_sysGpioCtrl (CHARGER_DNLD, GPIO_PIN, GPIO_OUTPUT_PIN);
        PAL_sysGpioCtrl (CHARGER_RESET, GPIO_PIN, GPIO_OUTPUT_PIN);
    }
    else
    {
      /* initialize pins to input - floating */
      if ( charger == CHARGER_TI )
      {
        PAL_sysGpioCtrl (CHARGER_DNLD, GPIO_PIN, GPIO_INPUT_PIN);
        PAL_sysGpioCtrl (CHARGER_RESET, GPIO_PIN, GPIO_INPUT_PIN);
      }
    }
    return;
}




/////////////////////////////////////////////////////////////////////////////
//
// The following is used to communicate with the UPS via a UART
//


TLM_REQ_t *get_pTlm_req (void)
{
  return (TLM_testMode ? tlm_facTodo : tlm_todo);
}


uint16 get_tlm_max (void)
{
  return (TLM_testMode ? TLM_MAX_REQ_FAC_INDEX : TLM_MAX_REQ_DATA_INDEX);
}



/****************************************************************************/
//
// General purpose function for making Telemetry requests
//
// This schedules data writes and command execution only.  Reads are not
// performed here.
//
// Arguements: req       - request type (see enum in tlm.h)
//             pData     - pass data in/out (as required)
//
// Return Values:  none
//
/****************************************************************************/

uint32 TLM_timeout = 10;  // provides for debug tweaking - defaults to 10 * 10ms = 100ms


void TLM_mk_req (uint8 req, uint8 *pData, uint16 bat)
{
  TLM_REQ_t *pElem;

  if (req >= TLM_MAX_INDEX)           { return; }  // index out of range
  if (bat > 1)                        { return; }  // battery out of range

  pElem = get_pTlm_req() + req;  // point to the element in the array for this upsVal (req)

  if (TLM_testMode != pElem->facTest) { return; }  // be sure request is for current mode

  if (pElem->cmdType == REQ_DATA)     { return; }  // no reads using this fcn

  if (pElem->cmdType == CMD_DATA)  pElem->data[bat] = *pData;

  pElem->sem[bat] = 1;  // set the semaphore (actually todo flag)
}




/****************************************************************************/
//
// Walks the todo table and services any requests.
// The minimum action is to obtain current battery status.
//
// Function:  TLM_pollUPS periodically polls the Battery Charger for status info
// Function:  TLM_uart_cmd  writes/reads UART on a per command basis
// Function:  TLM_uart_byte writes/reads UART on a per byte basis
//
/****************************************************************************/


boolean TLM_pollUPS (uint16 batNum)
{
  TLM_REQ_t *pElem, *pTable;

  pTable = get_pTlm_req();

  for (pElem = pTable; !(pElem->last); pElem++)
  {
    // for 2nd battery not everything needs to be polled
    if (batNum && (pElem->rdBat2 == TLM_NEVER))  continue;

    // ADD for PROD00194102, do not send f1 f2 f3 any more after f2 f3 get valid data.
    if ((pElem->code == 0xf1)
       && (pElem->dataValid[batNum]) 
       && (pElem->data[batNum] == TEST_DATA_READY)
       && (pElem + 1)->dataValid[batNum] 
       && (pElem + 2)->dataValid[batNum])
    {
        pElem += 3;
        continue;
    }
    // ADD END

    // Invalidate previous data & assume read failure
    pElem->dataValid [batNum] = FALSE;

    // All REQ_DATAs get executed.
    // CMD_DATAs, CMD_DUMPs and COMMANDs are executed only if the sem bit is set.
    //
    if ((pElem->cmdType == REQ_DATA) || (pElem->sem [batNum]))
    {
      // the return is an error exit.  If sem is set, will cause a timeout.
      if (TLM_uart_cmd (pElem, batNum) == FALSE)  
      {
        return FALSE; // abort and try again in 4 seconds
      }
      pElem->sem [batNum] = 0;
    }

    // ADD for PROD00194102, do not send f2 f3 before charger is ready to transmit the data
    if ((pElem->code == 0xf1)
       && (pElem->dataValid[batNum]) 
       && (pElem->data[batNum] != TEST_DATA_READY))
    {
        pElem += 2;
        continue;
    }
    // ADD END
  }
  return TRUE;
}



// External interface fcn
//
void TLM_setDataDumpCnt (uint16 cnt)
{
  dataDumpCnt = cnt;
}


// ping the charger with a byte that should be reflected
//
boolean TLM_charger_ping(void)
{
  uint8 val = 0xbe;

  TLM_chargerPings++;

  if (TLM_uart_byte (&val)) { return TRUE;  }
  else                      { return FALSE; }
}


// Like TLM_uart_cmd() below but handles the special case of CMD_DUMP.
// We poll the UART repeatedly until all test results are obtained.
//
void TLM_uart_cmdDump (TLM_msg6_t *msg6)
{
  uint32 int_status;
  uint16 i;
  uint8 TLM_DebugValBackup;

  TLM_REQ_t *pReq = tlm_facTodo + TLM_FT_DUMP_DATA;

  if (!TLM_uartOut (pReq->code))
  {
    if (!TLM_uartOut (pReq->code))
    {
        msg6->result = 0;
        return;
    }
  }

  if ((dataDumpCnt == 0) || (dataDumpCnt > CMD_DUMP_SIZE))
  {
    printk ("TLM_uart_cmd: dataDumpCnt range pblm\n");
    msg6->result = 0;
    return;
  }

  //ADD for PROD00194102, disable the debug log before dump data, otherwise dump data will fail.
  //restore it after dump data complete, fac2/fac3 will send f5, need be printed. 
  TLM_DebugValBackup = TLM_DebugVal;
  TLM_DebugVal = 0;
  //ADD  END
 
  // We are blocking the OS for potentially ~70 ms.
  // Not a big problem because we are about to power down anyway.
  //
  PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &int_status);
  {
    for (i = 0; i < dataDumpCnt; i++)
    {
      while (!TLM_uartIn ((msg6->data)+i));  // note: no bailout
    }
  }
  PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, int_status);

  //ADD for PROD00194102, disable the debug log before dump data, otherwise dump data will fail.
  //restore it after dump data complete, fac2/fac3 will send f5, need be printed.
  TLM_DebugVal = TLM_DebugValBackup;
  //ADD  END
  
  msg6->result = dataDumpCnt;
}


// Sends one command.  Receives response as appropriate.
// Handles all command types.
// CMD_DUMP is an exception.  Sends one byte then receives many.
//
boolean TLM_uart_cmd (TLM_REQ_t *pReq, uint16 batNum)
{
  uint8 scratch, cmdCode;

  cmdCode = pReq->code;

  if (batNum)  cmdCode |= BAT2_BIT;

  scratch = cmdCode;

  if (!(TLM_uart_byte (&scratch)))  return FALSE;

  // For REQ_DATA the UPS responds with the requested byte of data
  if (pReq->cmdType == REQ_DATA)
  {
    pReq->data [batNum] = scratch;

    pReq->dataValid [batNum] = TRUE;

    return TRUE;
  }

  // For both COMMAND and CMD_DATA the UPS echos the command message when it is
  // ready to proceed
  if (scratch != cmdCode)
  {
    printk ("TLM: Charger returned wrong code: %x, index: %x\n",
                                           (uint32) scratch, (uint32) pReq->index);
    return FALSE;
  }
  if (pReq->cmdType == COMMAND)   return TRUE;


  // This is type CMD_DATA.  UPS echos the data byte as confirmation.
  //
  scratch = pReq->data [batNum];

  if (!(TLM_uart_byte (&scratch)))  return FALSE;

  if (scratch != pReq->data [batNum])
  {
    printk ("TLM: Charger returned wrong data: %x\n", (uint32) scratch);
    return FALSE;
  }
  return TRUE;
}

// Verify UART communication link is up
boolean TLM_check_uart(void)
{
  uint32    tryCount = 1000; // 10 seconds
  uint8     poll = tlm_facTodo[TLM_FT_REQ_STATUS].code;
  uint8     resp = 0xFF;
  boolean   success = FALSE;

  while(tryCount--)
  {
    if (TLM_uartOut(poll))
    {
      // fixed delay of 10ms
      mdelay(10);

      if (TLM_uartIn(&resp))
      {
        if (resp == 0)
        {
          // We're good
          success = TRUE;
          printk("TLM_check_uart: got response in %d tries\n", 1000-tryCount);
          break;
        }
      }
    }
  }

  return success;
}

// writes a byte then reads a byte
//
boolean TLM_uart_byte (uint8 *data)
{
  uint16 tryCount;
  uint16 readDelay = 5;

  for (tryCount = 0; ; tryCount++)
  {
    if (TLM_uartOut (*data))
    {
      break;
    }

    if (tryCount == UART_MAX_TRIES)
    {
      printk ("TLM: Failure to write to Charger\n");
      return FALSE;
    }

    if (!TLM_testMode)   // we can slow things down in application mode
    {
      msleep (500);
    }
  }

  // In factory test mode we get lots of data bytes back to back.  Sleeping normally does
  // not work because UART gets overrun.   In normal mode we get much less data so
  // the UART can do the buffering.  A non-zero ssleep works and is more efficient.
  //
  // Note: in normal mode readDelay progresses as follows (in ms):
  //      5, 10, 20, 40, 245(80)    Total Max Delay is roughly 20s  
  //
   
  if (TLM_testMode)
  {
    for (tryCount = 0; tryCount < 5000; tryCount++) // tries for at least 5 sec
    {
      if (TLM_uartIn (data))  { return TRUE; }

      msleep (1);
    }
  }
  else
  {
    for (tryCount = 0; tryCount < TLM_uartMaxTries; tryCount++)
    {
      msleep (readDelay);

      if (readDelay < 80)  readDelay *= 2;

      if (TLM_uartIn (data))  { return TRUE; }
    }
  }
  printk ("TLM: Battery Charger Rx data timeout error\n");
             
  return FALSE;
}



/*****************************************************************************/
//
// TLM_uartOut - output a character in polled mode.
//
// RETURNS: TRUE  if data has been transmitted
//          FALSE if the output buffer if full
//
/*****************************************************************************/

#define XMITTER_EMPTY 0x40


boolean TLM_uartOut (uint8 data)
{
  uint8 pollStatus = *(volatile uint32 *)(AVALANCHE_UART0_REGS_BASE+0x14);

  // if transmitter not ready to accept data
  //
  if (!(pollStatus & XMITTER_EMPTY))  // re-init UART and try again
  {
    TLM_uartInit();  // re-initialize UART

    pollStatus = *(volatile uint32 *)(AVALANCHE_UART0_REGS_BASE+0x14);
    if (!(pollStatus & XMITTER_EMPTY))    return FALSE;
  }

  if (TLM_DebugVal & UART_OUT) printk ("TLM: Sent %x to UART\n", data);

  *(volatile uint32 *)(AVALANCHE_UART0_REGS_BASE) = data;

  //clear any UART errors (Charger's response is not yet received)
  data = *(volatile uint32 *)(AVALANCHE_UART0_REGS_BASE);

  return TRUE;
}




/*****************************************************************************/
//
// TLM_uartIn - poll the device for input.
//
// RETURNS: TRUE  if data arrived
//          FALSE if the input buffer if empty or an error has occurred
//
/*****************************************************************************/

#define DATA_READY 0x01
#define DATA_ERROR 0x0e

boolean TLM_uartIn (uint8 *data)
{
  uint8 pollStatus = *(volatile uint32 *)(AVALANCHE_UART0_REGS_BASE+0x14);

  if (!(pollStatus & DATA_READY))   return FALSE;

  // got data
  //
  *data = *(volatile uint32 *)(AVALANCHE_UART0_REGS_BASE);

  if (pollStatus & DATA_ERROR)
  {
    if (TLM_DebugVal & UART_IN)
      { printk ("TLM: UART read error status: %02x\n", pollStatus); }
    return FALSE;
  }
  if (TLM_DebugVal & UART_IN) { printk ("TLM: Received %x from UART\n", *data); }

  return TRUE;
}



//
// End communications with UPS via UART
//
/////////////////////////////////////////////////////////////////////////////
//
// Battery EPROM Reader
//

// Counter frequency is 450 MHz.  Each count represents 2.222 ns, which equals 2222 ps.
#define PS_PER_COUNT 2222
uint32 psPerSysCount = 0;  // pico seconds per sysCount


#define RESET_TIME 500
#define RESET_READ_HOLDOFF 68

#define READ_START     2
#define READ_HOLDOFF  12
#define READ_END     107

#define WRITE1_START   2
#define WRITE1_END   119

#define WRITE0_START  61
#define WRITE0_END    60

#define READ_ROM      0x33
#define READ_MEM_PAGE 0xc3

#define SEARCH_ROM    0xf0
#define MATCH_ROM     0x55

#define EPROM_PAGES      4
#define EPROM_NEXT_PAGE  0x20


// Calculates delays used in reading battery eprom.
// Calculation is performed in advance because calc time can exceed the delay time.
// Delay is specified in uS and has a max of 4771708 us.
// Returns the required number of counts to wait.
//
uint32 us2counts (uint32 uS)
{
  // 1000000 ps in 1 us.   500000 is a calibration constant that was determined by experimentation
  uint32 delayInPs = (uS * 1000000) + 500000;

  return (delayInPs / PS_PER_COUNT);
}


#if defined (CONFIG_MACH_PUMA5)
// Test fcn to generate a timed pulse
//
void TLM_epromPulse (uint32 uS)
{
  uint32 int_status, *gpio_ctrl;

  // Set direction to read so line goes high due to pull-up.  Set bit to low so that
  // whenever direction is set to write the line is driven low.
  
  PAL_sysGpioCtrl (BATTERY_EPROM, GPIO_PIN, GPIO_INPUT_PIN);
  
  PAL_sysGpioOutBit (BATTERY_EPROM,  0);                     // bit low

  gpio_ctrl = (uint32*)(AVALANCHE_GPIO_DIR + AVALANCHE_AUX_GPIO_OFFSET);

  PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &int_status);

  // direction = write (line goes low)
  *gpio_ctrl &= ~(1 << (BATTERY_EPROM - AVALANCHE_MAX_PRIMARY_GPIOS));
  udelay(uS);

  // direction = read (line goes high)
  *gpio_ctrl |=  (1 << (BATTERY_EPROM - AVALANCHE_MAX_PRIMARY_GPIOS));

  PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, int_status) ;
}

#else
// Test fcn to generate a timed pulse
//
void TLM_epromPulse (uint32 uS)
{
  uint32 int_status;
  register uint32 delayInCounts, beginTime;

  delayInCounts = us2counts (uS);  // convert uS to number of counts to delay

  // Set direction to read so line goes high due to pull-up.  Set bit to low so that
  // whenever direction is set to write the line is driven low.
  PAL_sysGpioCtrl (BATTERY_EPROM, GPIO_PIN, GPIO_OUTPUT_PIN);
  PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &int_status);
  {
      beginTime = FREE_RUNNING_COUNTER_L_GET();
      PAL_sysGpioOutBit (BATTERY_EPROM,  0);
      while ((FREE_RUNNING_COUNTER_L_GET() - beginTime) <= delayInCounts) ;
      PAL_sysGpioOutBit (BATTERY_EPROM,  1);
  }
  PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, int_status);
}
#endif

#if defined (CONFIG_MACH_PUMA5)
// Writes a low for time t1 to the eprom.
// Reads status of line after time t2.  Status is returned as: 1 = TRUE, 0 = FALSE
// Normal GPIO functions are not called because they are too slow.
//
boolean TLM_epromWriteRead (uint32 t1, uint32 t2)
{
  uint32 int_status, result, *gpio_ctrl, *gpio_in;

  gpio_ctrl = (uint32*)(AVALANCHE_GPIO_DIR     + AVALANCHE_AUX_GPIO_OFFSET);
  gpio_in   = (uint32*)(AVALANCHE_GPIO_DATA_IN + AVALANCHE_AUX_GPIO_OFFSET);
  PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &int_status);

  // direction = write (line goes low)
  *gpio_ctrl &= ~(1 << (BATTERY_EPROM - AVALANCHE_MAX_PRIMARY_GPIOS));
  udelay(t1);

  // direction = read (line goes high)
  *gpio_ctrl |=  (1 << (BATTERY_EPROM - AVALANCHE_MAX_PRIMARY_GPIOS));
  udelay(t2);

  result = ((*gpio_in) & (1 << (BATTERY_EPROM - AVALANCHE_MAX_PRIMARY_GPIOS)));   // read
  udelay(READ_END);

  PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, int_status) ;

  return (result ? TRUE : FALSE);
}
#else
// Writes a low for time t1 to the eprom.
// Reads status of line after time t2.  Status is returned as: 1 = TRUE, 0 = FALSE
// Normal GPIO functions are not called because they are too slow.
//
boolean TLM_epromWriteRead (uint32 t1, uint32 t2)
{
  uint32 int_status, result, t2_delay, readEnd_delay;
  register uint32 delayInCounts, beginTime;

  delayInCounts = us2counts (t1);  // convert uS to number of counts to delay
  t2_delay      = us2counts (t2);  // pre-calculate the value and make it is readily available
  readEnd_delay = us2counts (READ_END);

  PAL_sysGpioCtrl (BATTERY_EPROM, GPIO_PIN, GPIO_OUTPUT_PIN);
  PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &int_status);
  {
      beginTime = FREE_RUNNING_COUNTER_L_GET();
      PAL_sysGpioOutBit (BATTERY_EPROM,  0);   // bit low
      while ((FREE_RUNNING_COUNTER_L_GET() - beginTime) <= delayInCounts) ;
      PAL_sysGpioOutBit (BATTERY_EPROM,  1);   // bit high

      delayInCounts = t2_delay;
      beginTime = FREE_RUNNING_COUNTER_L_GET();
      PAL_sysGpioCtrl (BATTERY_EPROM, GPIO_PIN, GPIO_INPUT_PIN);
      while ((FREE_RUNNING_COUNTER_L_GET() - beginTime) <= delayInCounts) ;
      result = PAL_sysGpioInBit(BATTERY_EPROM);

      delayInCounts = readEnd_delay;
      beginTime = FREE_RUNNING_COUNTER_L_GET();
      while ((FREE_RUNNING_COUNTER_L_GET() - beginTime) <= delayInCounts) ;
  }
  PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, int_status);
  PAL_sysGpioCtrl (BATTERY_EPROM, GPIO_PIN, GPIO_OUTPUT_PIN);

  return (result ? TRUE : FALSE);
}
#endif


// returns TRUE if reset went ok.  FALSE if problem.
//
boolean TLM_epromReset (void)
{
  boolean result;

  result = TLM_epromWriteRead (RESET_TIME, RESET_READ_HOLDOFF);

  msleep(10);

  return (result) ? 0 : 1;  // expected to read a 0 to confirm presence
}



// reads a bit from the eprom
//
boolean TLM_epromReadBit (void)
{
  boolean result;

  result = TLM_epromWriteRead (READ_START, READ_HOLDOFF);

  msleep(0);

  return (result);
}


#if defined (CONFIG_MACH_PUMA5)
// Writes a bit to the eprom
//
void TLM_epromWriteBit (boolean bit)
{
  uint32 int_status, *gpio_ctrl;
  uint32 t1, t2;

  if (bit)
  {
      t1 = WRITE1_START;
      t2 = WRITE1_END;
  }
  else
  {
      t1 = WRITE0_START;
      t2 = WRITE0_END;
  }
  gpio_ctrl = (uint32*)(AVALANCHE_GPIO_DIR + AVALANCHE_AUX_GPIO_OFFSET);
  PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &int_status);

  // direction = write (line goes low)
  *gpio_ctrl &= ~(1 << (BATTERY_EPROM - AVALANCHE_MAX_PRIMARY_GPIOS));
  udelay(t1);

  // direction = read (line goes high)
  *gpio_ctrl |=  (1 << (BATTERY_EPROM - AVALANCHE_MAX_PRIMARY_GPIOS));
  udelay(t2);

  PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, int_status) ;

  msleep(0);
}

#else
// Writes a bit to the eprom
//
void TLM_epromWriteBit (boolean bit)
{
  uint32 int_status, t1, t2, t2_delay;
  register uint32 delayInCounts, beginTime;

  if (bit)
  {
      t1 = WRITE1_START;
      t2 = WRITE1_END;
  }
  else
  {
      t1 = WRITE0_START;
      t2 = WRITE0_END;
  }

  delayInCounts = us2counts (t1);  // convert uS to number of counts to delay
  t2_delay      = us2counts (t2);  // pre-calculate the value and make it is readily available

  PAL_sysGpioCtrl (BATTERY_EPROM, GPIO_PIN, GPIO_OUTPUT_PIN);
  PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &int_status);
  {
      beginTime = FREE_RUNNING_COUNTER_L_GET();
      PAL_sysGpioOutBit (BATTERY_EPROM,  0);
      while ((FREE_RUNNING_COUNTER_L_GET() - beginTime) <= delayInCounts) ;
      PAL_sysGpioOutBit (BATTERY_EPROM,  1);

      delayInCounts = t2_delay;
      beginTime = FREE_RUNNING_COUNTER_L_GET();
      while ((FREE_RUNNING_COUNTER_L_GET() - beginTime) <= delayInCounts) ;
  }
  PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, int_status);

  msleep(0);
}
#endif

/////////////////////////



// Reads byte from eprom
//
uint8 TLM_epromReadByte (void)
{
  uint32 i;
  uint8 result = 0;

  for (i = 8; i; i--)
  {
    result >>= 1;

    if (TLM_epromReadBit())  result |= 0x80;
  }

  msleep(5);

  return result;
}



// Writes byte to eprom
//
void TLM_epromWriteByte (uint8 val)
{
  uint32 i;

  for (i = 8; i; i--)
  {
    TLM_epromWriteBit (val & 0x01);

    val >>= 1;
  }

  msleep(5);
}



uint8 TLM_calcCrc (uint8 data_byte, uint8 crc)
{
  uint8 bit_mask, temp_data;

  temp_data = data_byte;

  for (bit_mask = 0; bit_mask <= 7; bit_mask ++)
  {
    data_byte ^= crc;

    crc       >>= 1;
    temp_data >>= 1;

    if (data_byte & 0x01)  crc ^= 0x8C;

    data_byte = temp_data;
  }
  return crc;
}

/////////////////////////

#if 0
// Selects one eprom for reading based upon known rom info (family code,
// id number & crc)
//
boolean TLM_matchRom (char *cp)
{
  uint16 i;

  if (!TLM_epromReset())                      return FALSE;
  TLM_epromWriteByte (MATCH_ROM);

  for (i = 0; i < TLM_ROM_LEN; i++)           TLM_epromWriteByte (cp[i]);

  return TRUE;
}
#endif


// Reads ROM area of EPROM which contains unique S/N for that part.
// This cannot be used if there may be more than one battery present.
// If success, buffer contains ROM data and fcn returns TRUE
// IF failure, sets 1st byte of buffer to 0 and returns FALSE
//
boolean TLM_readRom (char *cp)
{
  uint16 i;

  uint8 crc = 0;

  if (TLM_epromReset())
  {
    TLM_epromWriteByte (READ_ROM);

    for (i = TLM_ROM_LEN-1; i; i--, cp++)
    {
      *cp = TLM_epromReadByte();

      crc = TLM_calcCrc (*cp, crc);
    }

    *cp = TLM_epromReadByte();

    if (*cp == crc)  return TRUE;
  }

  return FALSE;
}


#if 0

// In cases where there may be 2 batteries it is necessary to determine the
// 48 bit id number of each battery.  TLM_searchRom uses the search algorithm
// to determine these ids.  Call once with selector == 0 and once with
// selector == 1 to get info for both batteries.
//
// Returns TRUE collision occurred, FALSE if not
//
boolean TLM_searchRom (uint8 *scratch, boolean selector)
{
  uint16 i;
  uint8 bit, comp;
  boolean retVal = FALSE;

  TLM_nullRom (scratch);

  if (!TLM_epromReset())  return FALSE;

  TLM_epromWriteByte (SEARCH_ROM);

  for (i = 0; i < (TLM_ROM_LEN * 8); i++)  // read all bits including crc
  {
     bit  = TLM_epromReadBit();
     comp = TLM_epromReadBit();

     if (!(bit | comp))   // collision occurred - must arbitrate
     {
       bit = selector;
       retVal = TRUE;
     }

     TLM_epromWriteBit (bit);

     scratch [i/8] |= (bit << (i%8));
  }
  return retVal;
}

#endif


// Checks rom area CRC
// Returns TRUE if ok, FALSE if not
//
boolean TLM_romCrcIsOk (uint8 *cp)
{
  uint8 crc, i;

  for (crc = i = 0; i < (TLM_ROM_LEN - 1); i++)  // check crc
  {
    crc = TLM_calcCrc (cp[i], crc);

    if (cp [TLM_ROM_LEN - 1] == crc)  return TRUE;
  }

  return FALSE;
}



// invalidates CRC of rom area by filling rom area with 0xff
//
void TLM_invalidateCrc (uint8 *cp)
{
  uint16 i;

  for (i = 0; i < TLM_ROM_LEN; i++)    cp[i] = 0xff;
}




// Fills rom area with null
//
void TLM_nullRom (uint8 *cp)
{
  uint16 i;

  for (i = 0; i < TLM_ROM_LEN; i++)    cp[i] = 0;
}




// assumes that part has been reset and rom has already been read
//
boolean TLM_readMemPage (char *cp, uint8 pageLoByte)
{
  uint16 i;
  uint8 crc;

  boolean all_FF = TRUE; // all 0xff or all 0x00 is bad data
  boolean all_00 = TRUE;

  TLM_epromWriteByte (READ_MEM_PAGE);
  TLM_epromWriteByte (pageLoByte);      // address low  byte
  TLM_epromWriteByte (0x00);            // address high byte

  crc = TLM_calcCrc (READ_MEM_PAGE, 0);
  crc = TLM_calcCrc (pageLoByte, crc);  // address low  byte
  crc = TLM_calcCrc (0x00, crc);        // address high byte

  if (TLM_epromReadByte() != crc)    { return FALSE; }

  for (i = TLM_PAGE_LEN, crc = 0; i; i--, cp++)
  {
    *cp = TLM_epromReadByte();

    if (*cp != 0x00)
    {
        all_00 = FALSE;
    }
    if (*cp != 0xff)
    {
        all_FF = FALSE;
    }

    crc = TLM_calcCrc (*cp, crc);
  }

  if (all_00 || all_FF)              { return FALSE; }
  if (TLM_epromReadByte() == crc)    { return TRUE;  }

  return FALSE;
}



// Updates EPROM battery information
//
// returns TRUE if success or FALSE if failure
//
boolean TLM_epromUpdateInfo (TLM_eprom_t *TLM_epromPg)
{
  uint8 scratch1 [TLM_ROM_LEN], epromPage;

  // begin at address 0x0000 - higher byte is always 0x00
  uint8 loByte = 0x00;
  TLM_eprom_t TLM_epromTemp;

  TLM_invalidateCrc (scratch1);

  //if (!isInHighPwrMode())  return FALSE;  // only works when we run at full speed

  // The EPROM contains 4 possible pages of data.  
  // Only one of the four pages contains valid data.  Pages which failed
  // factory writes are all zeros, pages which have never been written to
  // contain all FFs.  Only the page which was successfully written to
  // contains valid data.  Get that page and fill up the TLM_eprom1 structure
  // with the valid EPROM data.
  // The pages are addressed as follows:
  //   Page 0:  0x0000 - 0x001F
  //   Page 1:  0x0020 - 0x003F
  //   Page 2:  0x0040 - 0x005F
  //   Page 3:  0x0060 - 0x007F

  for (epromPage = 0; epromPage < EPROM_PAGES; epromPage++, loByte += EPROM_NEXT_PAGE)
  {
    // re-initialize EPROM before reading each page
    //
    if (!TLM_readRom(scratch1))
    {
        printk ("TLM: Reading Battery EPROM Header Failed: %x %x %x %x %x %x %x %x\n",
                scratch1[0], scratch1[1], scratch1[2], scratch1[3], scratch1[4], scratch1[5], scratch1[6], scratch1[7]);
        return FALSE;
    }

    // Returns if battery info is already up to date - battery info is
    // updated whenever a new battery is inserted
    //
    if (!memcmp (scratch1, TLM_epromRom1, TLM_ROM_LEN-1))  return TRUE;

    if (TLM_readMemPage((char *)(&TLM_epromTemp), loByte))
    {
        /* MOD for PROD00196342 battery's Serial NumberPart Number are wrong sometimes
            only save to  TLM_epromPg when TLM_readMemPage succeed */
        memcpy (TLM_epromPg, &TLM_epromTemp, TLM_PAGE_LEN); 
        memcpy (TLM_epromRom1, scratch1, TLM_ROM_LEN);  // save new ROM info

        return TRUE;
    }
    msleep(10);  // pause for other processes
  }

  printk ("TLM: All 4 pages of Battery EPROM had invalid data\n");
  return FALSE;
}


//
// End Battery EPROM Reader
//
/////////////////////////////////////////////////////////////////////////////


// the purpose of this function is to recover from a communications failure.  a
// communications failure occurs when the Puma waits 20 seconds and the charger fails
// to respond.  when that occurs, this function is called.  
//void TLM_communicationCheck (void)
//{
//    if (CHRGR_dead)   return;  // if charger intentionally killed
//    if (TLM_testMode) return;  // factory test
//
//    // communication has failed.  attempt to ping the charger
//    if (TLM_charger_ping())
//    {
//        TLM_chargerResets = 0;
//        return;  // ping successful
//    }
//    if (!AC_ok)  return;  // resetting on battery will kill the device   
//
//    if (!TLM_chargerResets)  // try resetting charger if this has not already been tried
//    {
//        CHRGR_UPSreset();
//
//        TLM_chargerResets++;
//
//        printk ("TLM: Dead Charger - Resetting\n");  
//        ssleep(20);
//    }
//    else  // kill charger if reset failed
//    {
////    if(!NVM_GetByte(MFG_FACTORY_MODE)) // don't kill if factory mode enabled
//        {
//            CHRGR_kill();
//
//            printk ("TLM: UPS comms failed. ");  // intentially no \n. "Charger Killed\n" to be added.
//        }
//    }
//} 

