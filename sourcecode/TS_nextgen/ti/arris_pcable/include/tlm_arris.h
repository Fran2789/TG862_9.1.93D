/*

Copyright (c) 2008-2016 ARRIS Enterprises, Inc.

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
 * FILE PURPOSE:     - Battery charger manager include
 ******************************************************************************
 * FILE NAME:     tlm.h
 *
 * DESCRIPTION:   Included by both user and kernel space telemetry modules.
 *
 *                Author: Bill Mohr, ARRIS Group, Inc.
 *
 ******************************************************************************/

#ifndef TELEM_H
#define TELEM_H

#include "defs.h"

#ifndef TRUE
#define TRUE  1
#define FALSE 0
#endif

//  TLM_GPIO_PIN_DIRECTION_GPIOPDIRR &= ~io;   // direction = write (line goes low)
//  TLM_GPIO_PIN_DIRECTION_GPIOPDIRR |= io;    // direction = read (line goes high)
//  TLM_GPIO_DATA_OUT_GPIODOUTR |= io;         // line high
//  TLM_GPIO_DATA_OUT_GPIODOUTR &= ~io;        // line low

#define COUNTER_ADDR         IO_ADDRESS(0x08690040)
#define COUNTER_CONTROL      IO_ADDRESS(0x08690004)
#define COUNTER_CONTROL_MASK (1 << 31)

#define  SYS_CNT  *((uint32 *)(COUNTER_ADDR))

#if defined (CONFIG_MACH_PUMA5)
#define CHARGER_RESET      AVALANCHE_MAX_PRIMARY_GPIOS + 27  /* AGCS2/AUX_27   Pin AA14 */
#define CHARGER_DNLD       AVALANCHE_MAX_PRIMARY_GPIOS + 30  /* TAGCS2/AUX_30  Pin AB14 */
#define BATTERY_EPROM      AVALANCHE_MAX_PRIMARY_GPIOS + 29  /* AGCS4/AUX_29   Pin AA15 */
#else
#define CHARGER_RESET      72  //PUMA6_FWDNLD_N  -> SBWTCK -> CLOCK
#define CHARGER_DNLD       73  //PUMA6_BATCHG_RESET_N  -> SBWTDIO ->DATA
#define BATTERY_EPROM      58  //PUMA6_BATSN
#endif

#if defined (CONFIG_MACH_PUMA6)
         #define PAL_sysGpioCtrl(x,y,z)   PAL_sysGpioCtrl(x,z)
#endif


#define CHARGER_UART_TX                                  10  /* UART-B_TD */
#define CHARGER_UART_RX                                  11  /* UART-B_RD */
#define CHARGER_UART_CTS                                 12  /* UART-B_CTS */
#define CHARGER_UART_RTS                                 13  /* UART-B_RTS */


//#define CHARGER_UART_TX  (0x02 << 4) #define S3C64XX_GPB1_UART_TXD2 (0x02 << 4)
//#define CHARGER_UART_RX  (0x02 << 0) #define S3C64XX_GPB0_UART_RXD2 (0x02 << 0)

#define CHARGER_DNLD_RAW   (CHARGER_DNLD - AVALANCHE_MAX_PRIMARY_GPIOS)


#define TLM_USER_Q    0xE0  /* arbitrary number - assignment needs to be centralized */
#define TLM_SHM_ID    5555  /* arbitrary number - assignment needs to be centralized */


#define UART_OUT 0x01
#define UART_IN  0x02

#define TLM_MAJOR 111


extern uint32 CHRGR_debug;
extern uint16 TLM_maxBat;
extern void CHRGR_init (void);
extern void CHRGR_UPSreset (void);
extern void CHRGR_kill (void);

extern boolean TLM_dataValid (void);
extern boolean TLM_AC_ok (void);
extern boolean TLM_battery_missing (void);
extern boolean TLM_lowBat (void);
extern boolean TLM_testInProgress (void);
extern boolean TLM_battery_replace (void);
extern uint32  TLM_MtaRegCompleteGet (void);
extern void    TLM_MtaRegCompleteSet (uint32);
extern uint16 TLM_hexValGet(void);
extern char* TLM_batStatGet(void);
extern void TLM_getBatInitialTestString(char *batt_str);

int TLM_libInit (void);
void TLM_epromAscii (char *buff, uint16 choice);

// used by Logs & Alarm functionality

boolean TLM_AC_fail (uint8);
boolean TLM_battery_bad (uint8);
boolean TLM_alm_genFault(uint8);
boolean TLM_battery_depleted (uint8);
boolean TLM_output_offAsRequired (uint8);
boolean TLM_chargerFailed (uint8);
boolean TLM_ShutdownImminent (uint8);
boolean TLM_output_off (uint8);
boolean TLM_stateChanged (uint8);
boolean TLM_subChanged (uint8);
boolean TLM_dataShutdown (uint8);
boolean TLM_batteryChargerDisabled (uint8);
boolean TLM_downloadFailed (uint8);
boolean TLM_overTempShutdown (uint8);
boolean TLM_tempHigh (uint8);
boolean TLM_batShutdown (uint8);
void TLM_dataShutdownClr (void);
boolean TLM_chargerFailed (uint8 unused);
boolean TLM_battery_depleted(uint8);
boolean TLM_output_off(uint8 unused);
boolean TLM_output_offAsRequired (uint8 unused);
boolean TLM_isUpsAwaitingPower(uint8);
boolean TLM_ShutdownImminent (uint8 unused);
boolean TLM_isUpsShutdownPending(uint8);
boolean TLM_battery_missingTest(uint8);
boolean TLM_lowBatTest(uint8);

/****************************************************************************/
//
// The following functions constitute a read only API for obtaining
// raw data from charger located in shared memory.
//
// All return TRUE if success and FALSE if failure
//
/****************************************************************************/

boolean TLM_rd_status               (uint8  *pData);
boolean TLM_rd_rated_batt_capacity  (uint16 *pData);
boolean TLM_rd_tested_batt_capacity (uint16 *pData);
boolean TLM_rd_batt_state_of_charge (uint16 *pData);
boolean TLM_rd_batt_power           (uint16 *pData);
boolean TLM_rd_set_chg_state        (uint16 *pData);
boolean TLM_rd_typical_idle_power   (uint8  *pData);
boolean TLM_rd_bat_index_code       (uint8  *pData);
boolean TLM_rd_repl_batt_thresh     (uint16 *pData);
boolean TLM_rd_test_timer           (uint8  *pData);
boolean TLM_rd_info_status_reg      (uint8  *pData);
boolean TLM_rd_full_chg_time        (uint8  *pData);
boolean TLM_rd_low_batt_thresh      (uint16 *pData);
boolean TLM_rd_chg_failure_status   (uint8  *pData);
boolean TLM_rd_chg_reset_status     (uint8  *pData);
boolean TLM_rd_sec_batt_high_byte   (uint16 *pData);
boolean TLM_rd_sec_batt_low_byte    (uint16 *pData);
boolean TLM_rd_synch_timer          (uint8  *pData);
boolean TLM_rd_charger_temp         (uint8  *pData);
boolean TLM_rd_batt_voltage         (uint8  *pData);

// The following are factory related

boolean TLM_rd_fac_status           (uint8  *pData);
boolean TLM_rd_data_cnt_low         (uint8  *pData);
boolean TLM_rd_data_cnt_hi          (uint8  *pData);

/****************************************************************************/
//
// The following functions constitute an API used for writing commands and
// data to the charger.
//
/****************************************************************************/

void TLM_cmd_pause_test_timer (void);
void TLM_cmd_resume_test_timer (void);
void TLM_cmd_ff_test_timer (void);
void TLM_cmd_turn_off_ups (void);
void TLM_cmd_turn_on_ups (void);
void TLM_cmd_toggle_hi_temp (void);
void TLM_cmd_calibrate_timer (void);
void TLM_cmd_toggle_hi_temp_battery_shutdown (void);
void TLM_set_chg_state (uint16);
void TLM_set_repl_batt_thresh (uint16);
void TLM_set_full_chg_state (uint8);
void TLM_set_low_batt_thresh (uint16);
void TLM_set_batt_index_code (uint8);

// The following are factory related

void TLM_cmd_begin_testing (void);
void TLM_cmd_remove_dc (void);
void TLM_cmd_dump_data (void);


/****************************************************************************/


// Telemetry types for normal operation
//
// TLM_RD_TELEM must be the first entry
// "Request Data" types must come before "Command" & "Command with Data"
// Must be in same order as tlm_todo[] and TLM_dataDefs[]
//
enum
{
  TLM_RD_TELEM,                      // Request Data         Read UPS Status Telemetry
  TLM_RD_RATED_BATT_CAPACITY,        // Request Data         Read Rated Battery Capacity
  TLM_RD_TESTED_BATT_CAPACITY,       // Request Data         Read Tested Battery Capacity
  TLM_RD_BATT_STATE_OF_CHARGE,       // Request Data         Read Battery State of Charge
  TLM_RD_BATT_POWER,                 // Request Data         Read Battery Power
  TLM_RD_SET_CHG_STATE,              // Request Data         Read Set Charge State
  TLM_RD_TYPICAL_IDLE_POWER,         // Request Data         Read Typical Idle Power
  TLM_RD_REPL_BATT_THRESH,           // Request Data         Read Replace Battery Threshold
  TLM_RD_TEST_TIMER,                 // Request Data         Read Test Timer & Info Status
  TLM_RD_FULL_CHG_TIME,              // Request Data         Read Full-Charge Time
  TLM_RD_LOW_BATT_THRESH,            // Request Data         Read Low Battery Threshold
  TLM_RD_CHG_FAILURE_STATUS,         // Request Data         Read Charger Failure Status
  TLM_RD_SEC_BATT_HIGH_BYTE,         // Request Data         Read Seconds on Battery High-byte
  TLM_RD_SEC_BATT_LOW_BYTE,          // Request Data         Read Seconds on Battery Low-byte
  TLM_RD_SYNC_TIMER,                 // Request Data         Read Synchronization Timer
  TLM_RD_BATT_VOLTAGE,               // Request Data         Read Battery Voltage
  //
  TLM_CMD_PAUSE_TEST_TIMER,          // Command              Pause Test Timer
  TLM_CMD_RESUME_TEST_TIMER,         // Command              Resume Test Timer
  TLM_CMD_FF_TEST_TIMER,             // Command              Fast Forward Test Timer
  TLM_CMD_TURN_OFF_UPS,              // Command              Turn off UPS output
  TLM_CMD_TURN_ON_UPS,               // Command              Turn on UPS output
  TLM_CMD_TOGGLE_HI_TEMP_SHUT,       // Command              Toggle hi-temp shutdown
  TLM_CMD_TOGGLE_HI_TEMP_BATT_SHUT,  // Command              Toggle Thermal Discharge Feature
  TLM_CMD_CALIBRATE_TIMER,           // Command              Calibrate Timers
  //
  TLM_SET_CHG_STATE,                 // Command with Data    Set Charge State
  TLM_SET_REPL_BATT_THRESH,          // Command with Data    Set Replace Battery Threshold
  TLM_SET_FULL_CHG_STATE,            // Command with Data    Set Full-Charge State
  TLM_SET_LOW_BATT_THRESH,           // Command with Data    Set Low Battery Threshold
  TLM_SET_BATT_INDEX_CODE,           // Command with Data    Set Battery Index Code

  TLM_MAX_INDEX
};

// Telemetry types for factory test
// TLM_FT_REQ_STATUS must be first
// must be in same order as tlm_facTodo[]
//
enum
{
  TLM_FT_REQ_STATUS,           // Request Data         Read Test Status from UPS
  TLM_FT_REQ_CNT_HI,           // Request Data         Read High Byte of Data Count
  TLM_FT_REQ_CNT_LO,           // Request Data         Read Low  Byte of Data Count
  TLM_FT_BEGIN_TEST,           // Command              Begin Factory Test
  TLM_FT_REMOVE_DC,            // Command              Remove DC Power
  TLM_FT_DUMP_DATA             // Command Dump         Command to dump data to screen
};

// *** This may need to be changed when adding REQ_DATA type fields ***
#define TLM_MAX_REQ_DATA_INDEX   TLM_RD_BATT_VOLTAGE  /* max normal  index for REQ_DATA */
#define TLM_MAX_REQ_FAC_INDEX    TLM_FT_REQ_CNT_LO    /* max factory index for REQ_DATA */


/////////////////////////////////////////////////////////////////////////////
//
// Battery EPROM Reader
//

enum
{
  DATA_REV = 0,
  PCB_ASSY_PN,
  PCB_ASSY_RV,
  PCB_TEST,
  CELL_MAN,
  CELL_FACT,
  CELL_LOT,
  CELL_AH,
  PACK,
  PACK_TEST
};


typedef struct
{
  char data_rev  [1];  // ascii
  char pcb_assyPn[5];  // ascii
  char pcb_assyRv[1];  // ascii
  char pcb_test  [1];  // hex
  char cell_man  [2];  // ascii
  char cell_fact [1];  // ascii
  char cell_lot  [6];  // ascii
  char cell_ah   [2];  // ascii
  char pack      [11]; // ascii
  char pack_test [1];  // hex
  char checksum  [1];  // hex
} TLM_eprom_t;

//
/////////////////////////////////////////////////////////////////////////////

#define HW_MODEL_LENGTH 20
#define CHRGR_FW_REV_LEN 8
#define CHRGR_NAME_LEN 80
#define CMD_DUMP_SIZE (512)

enum
{
    NO_ACTION = 0,
    UP_ACTION,
    DN_ACTION
};


// Exists in shared memory for access by TLM API
typedef struct
{
    uint32 TLM_ConfigRunTime;
    uint32 TLM_ConfigReplaceBatTime;
    uint32 TLM_ConfigLowBattTime;
    uint32 TLM_ACfailDataShutdownEnable;
    uint32 TLM_ACfailDataShutdownTime;
    uint32 TLM_HiTempBatShutdownEnabled;
    uint32 TLM_OverTempAlarmThreshold;
    uint32 TLM_LogRepeatInterval;
    uint32 TLM_ticks;
    uint32 CHRGR_dead;
    uint32 CHRGR_downloadFailed;
    uint32 TLM_mtaRegComplete;
    uint32 TLM_libToDoFlag;
    uint32 TLM_upDnDelay;
    uint8  TLM_upDnAction;
    uint8  acOK;
    uint8  missing;
    uint8  lowBat;
    uint8  replace;
    uint8  index;
    uint8  TLM_telemetry;
    uint8  spare;
    uint16 upsVal;
    boolean TLM_finalChargeFlag;
    char   hwModel [HW_MODEL_LENGTH];
    char   chrgrFwRev [CHRGR_FW_REV_LEN];
    char   upsIdentName [CHRGR_NAME_LEN];
} TLM_derivedData;


// Used for tracking registration to determine when MTA registration is complete.
enum
{
    TLM_REG_START,
    TLM_DOCSIS_COMPLETE,
    TLM_PACM_COMPLETE
};


typedef struct
{
    unsigned int cmd;    // command
    void         *data;  // data provided with the command - depending upon command
} TLM_privIoctl;


// TLM message buffer for short ioctl exchanges
typedef struct
{
    uint8 data;
    uint8 spare;
} TLM_msg1_t;

typedef enum
{
    PLAT_702,
    PLAT_722,
    PLAT_802,
    PLAT_804,
    PLAT_822,
    PLAT_852,
    PLAT_862,
    PLAT_872,
    PLAT_1602,
    PLAT_1642,
    PLAT_1652,
    PLAT_1662,
    PLAT_1672,
    PLAT_1682,
    PLAT_2472,
    PLAT_2492,
    PLAT_2402,
    PLAT_3202,
    PLAT_UNKNOWN
    
}tPlatform;

typedef enum
{
    CHARGER_ZILOG,
    CHARGER_ATMEL,
    CHARGER_TI,
    CHARGER_UNKNOWN

} tCharger;

// TLM message buffer for ioctl exchanges with driver in kernel
typedef struct
{
    // The following are written by driver.
    uint8 result;
    uint8 testMode;   // Indicates mode for data - normal vs. factory test

    uint8 acOK;       // written by user app.  Tells Driver of AC failure.

    uint8 spare;

    uint8 data[(TLM_MAX_REQ_DATA_INDEX + 1) * 2];  // raw data from charger
} TLM_msg2_t;


// TLM message buffer for user space task level messaging
typedef struct
{
    uint8  msg_type;
    uint8  sub_type;
    uint8  data;
    uint8  dataB;
} TLM_msg3_t;


// TLM message buffer for ioctl exchanges of max. 4 bytes
typedef struct
{
    uint8 data[4];
} TLM_msg4_t;


// TLM message buffer for ioctl exchanges of EPROM data
typedef struct
{
    TLM_eprom_t epromPage;
    uint8 data_valid;     // set if last read was good
    uint8 spare;
} TLM_msg5_t;


// TLM message buffer for ioctl exchanges of max. 256 bytes
typedef struct
{
    uint8  data[CMD_DUMP_SIZE];
    uint16 result;
} TLM_msg6_t;


// TLM message buffer wrapper for user space task level messaging.
// This is needed to provide msgSndType field for compatibility with msgsnd().
typedef struct
{
    long msgSndType;
    TLM_msg3_t payload;
} TLM_msgsndBuf;


// event message types
enum
{
    TLM_timer_expiry_event,
    TLM_chrgr_download,
    TLM_chrgr_reset,
    TLM_chrgr_fwVerGet,
    TLM_charger_kill,
    TLM_write_cmd_or_data,
    TLM_fac0,
    TLM_fac2,
    TLM_fac3,
    TLM_fac4,
    TLM_dataDump,
    TLM_dataDumpCnt,
    TLM_readEprom,
    TLM_block,
    TLM_putStatus,
    TLM_rptTime,
    TLM_debug,
    TLM_querypowerconnected
};

// for UPS_CMD_OR_DATA type message
enum
{
    tlm_request = 0,
    tlm_data,
    tlm_battery
};


enum
{
    UPS_POLL = 1,
    UPS_DOWNLOAD,
    UPS_RESET,
    UPS_CHRGR_FW_VER,
    UPS_KILL,
    UPS_CMD_OR_DATA,
    UPS_FAC0,
    UPS_FAC4,
    UPS_DATA_DUMP,
    UPS_DATA_DUMP_CNT,
    UPS_READ_EPROM,
    UPS_DEBUG,
    UPS_INIT_STATUS,
    UPS_TEST_MODE,
    UPS_WAIT_POWER_EVENT
};


//
// Used for factory testing
//
#define CHARGER_BUSY_TESTING 0x00
#define REMOVE_AC_POWER      0x01
#define TEST_DATA_READY      0x02
#define TEST_FAIL_REMOVE_AC  0xff


/////////////////////////////////////////////////////////////////////////////
//
// For Support of Telemetry API Functions
//

boolean TLM_setMib (uint32,  char*, uint16, boolean);
void TLM_getMib    (uint32*, char*, uint16, boolean);
void TLM_getMibString (char*, uint16);
uint32 TLM_upsAlarmGet (char*, uint32, uint32*);
boolean TLM_upsAlarmIndexGet (uint32*, uint32, uint32*);

#define TEST_SCHEDULED 0
#define DISABLE_AUTO_TESTING 1
#define TEST_IN_PROGRESS 2
#define TEST_PENDING 3
#define UNAVAILABLE 4

// Represents the various MIBs (or potential MIBs) supported by TLM
enum
{
    UPS_upsBatteryStatus = 0,
    UPS_arrisMtaDevPwrSupplySecondsOnBattery,
    UPS_upsConfigLowBattTime,
    UPS_arrisMtaDevPwrSupplyConfigRunTime,
    UPS_arrisMtaDevPwrSupplyConfigReplaceBatTime,
    UPS_upsEstimatedChargeRemaining,
    UPS_arrisMtaDevPwrSupplyBatRatedMinutes,
    UPS_arrisMtaDevPwrSupplyBatAvailableMinutes,
    UPS_upsEstimatedMinutesRemaining,
    UPS_arrisMtaDevBatteryOperState,
    UPS_arrisMtaDevBatteryOperSubState,
    UPS_arrisMtaDevBatteryEprom,
    UPS_arrisMtaDevPwrSupplyBatteryTest,
    UPS_arrisMtaDevPwrSupplyBatteryTestTime,
    UPS_arrisMtaDevPwrSupplyFullChargeTime,
    UPS_arrisMtaDevPwrSupplyReadBatteryPwr,
    UPS_arrisMtaDevPwrSupplyTelemetryValues,
    UPS_arrisMtaDevPwrSupplyTemperature,
//  UPS_upsAlarmDescr,
    UPS_upsAlarmsPresent,
//  UPS_upsAlarmTime,
    UPS_upsOutputSource,
    UPS_upsSecondsOnBattery,
    UPS_arrisMtaDevPwrSupplyEnableDataShutdown,
    UPS_arrisMtaDevPwrSupplyDataShutdownTime,
    UPS_arrisMtaDevPwrSupplyHiTempBatteryShutdownControl,
    UPS_arrisMtaDevPwrSupplyOverTempAlarmControl,
    UPS_arrisMtaDevPwrSupplyOverTempAlarmThreshold,
    UPS_arrisMtaDevPwrSupplyHighestTemperature,
    UPS_arrisMtaDevPwrSupplyHighestTemperatureTime,
    UPS_arrisMtaDevPwrSupplyHighestTemperatureClear,
    UPS_arrisMtaDevBatteryChargerFWRev,
    UPS_arrisMtaDevPwrSupplyControlChargerReset,
    UPS_LogRepeatInterval,
    UPS_upsIdentManufacturer,
    UPS_upsIdentModel,
    UPS_upsIdentAgentSoftwareVersion,
    UPS_upsIdentName,
    UPS_upsIdentAttachedDevices,
    UPS_upsInputNumLines,
    UPS_upsOutputNumLines,
    UPS_upsShutdownType,
    UPS_upsShutdownAfterDelay,
    UPS_upsStartupAfterDelay,
    UPS_upsRebootWithDuration,
    UPS_arrisMtaDevBatteryLastStateChange,
    UPS_arrisMtaDevBatteryOrderingCode,
    UPS_arrisMtaDevEstimatedMinutesRemaining,
    UPS_arrisMtaDevEstimatedChargeRemaining,
    UPS_MAX               // Place all new definitions above this line
};


//
// End Support of Telemetry API Functions
//
/////////////////////////////////////////////////////////////////////////////
//
// For Support of Log & Alarm Capability
//

typedef boolean (*tlmTestFcn) (uint8);
typedef void    (*tlmClrFcn)  (void);

// Possible modes for each alarm to operate in
//
enum
{
  alm_off,
  alm_log,
  alm_repeatingLog,
  alm_alarm,
  alm_console,
  alm_ups
};

// Note: there are several arrays that must be kept in synch with this enum.
// See tlmLib.c
enum
{
  alm_ACfail,
  alm_batReplace,
  alm_batLow,
  alm_batDepleted,
  alm_UPSoff,
  alm_UPSoffasReqd,
  alm_genFault,
  alm_shutdownImminent,
  alm_batMissing,
  alm_overTempShutdown,
  alm_tempHigh,
  alm_chargerDisabled,
  alm_downloadFailed,
  alm_batMismatch,
  alm_stateChange,
  alm_subStateChange,
  alm_DataShutdown,
  alm_BatShutdown,
  alm_batBad,
  alm_waitPower,
  alm_shutdownPending,
  alm_Max
};

// The follow values are corresponded to mib arrisMtaDevPwrSupplyAlarm nodes
enum
{
  UPS_alm_acFail=1,
  UPS_alm_chargerOverTempShutdown,
  UPS_alm_chargerTemperatureHigh,
  UPS_alm_batteryChargerDisabled,
  UPS_alm_chargerDownloadFailed,
  UPS_alm_batteryMismatch,
  UPS_alm_BatteryBad,
  UPS_alm_LowBattery,
  UPS_alm_DepletedBattery,
  UPS_alm_OutputOff,
  UPS_alm_OutputOffAsRequested,
  UPS_alm_GeneralFault,
  UPS_alm_ShutdownImminent,
  UPS_alm_BatteryMissing,
  UPS_alm_AwaitingPower,
  UPS_alm_ShutdownPending,
  UPS_alm_Max
};
// Run time structure used to manage alarms.
// Note: keep the size of this structure evenly divisible by 4
//
typedef struct
{
  char almStr[80];               // string describing log/alarm
  unsigned long almTime;         // time at which alarm became active
  unsigned long almLastReported; // time at which alarm was last reported
  uint8 almActive;               // alarm status (active==raised inactive==lowered)
  uint8 almMode;                 // defines rules to use (ie. alarm vs. log)
  uint16 almIndex;               // index number for this TLM_alarm_t
  uint8  almUpsIndex;        // index for ups autonomous type
  uint8 spare[3];                // spare
  uint8 last;                    // flags last TLM_alarm_t in array
} TLM_alarm_t;

//
// End Support of Log & Alarm Capability
//
/////////////////////////////////////////////////////////////////////////////


#endif  /* ifndef TELEM_H */
