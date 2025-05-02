/*
  GPL LICENSE SUMMARY

  Copyright(c) 2008-2012 Intel Corporation.

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
  The full GNU General Public License is included in this distribution
  in the file called LICENSE.GPL.

  Contact Information:
    Intel Corporation
    2200 Mission College Blvd.
    Santa Clara, CA  97052
*/

/******************************************************************************
 * FILE PURPOSE:     - LED kernel Header
 ******************************************************************************
 * FILE NAME:     led_hal.h
 *
 * DESCRIPTION:   Header file defining HAL types and functions.
 *
 *
 *******************************************************************************/


#ifndef _LED_HAL_H_
#define _LED_HAL_H_

#include "_tistdtypes.h"
// ARRIS ADD
#ifndef __KERNEL__
#include "puma_autoconf.h"
#endif
// END ARRIS

#define LED_ARR_LEN(v)  (sizeof(v) / sizeof(v[0]))

#define MAX_STATES_PER_MOD 25


#define LED_HAL_BITMASK_WIDTH       (128)
#define LED_HAL_BITMASK_BYTE_WIDTH  (LED_HAL_BITMASK_WIDTH/8)
#define LED_HAL_BITMASK_REG_WIDTH   (LED_HAL_BITMASK_WIDTH/32)


// ARRIS CHANGE - Add/remove some LED entries
/*
 * NOTE: The LED defines that appear bellow, must match
 * the location of the modules inside the array named "modules"
*/
#if defined (CONFIG_MACH_PUMA5)
typedef enum
{
    PUMA_LED_ID_POWER,
    PUMA_LED_ID_DS,
    PUMA_LED_ID_DS_AMBER,
    PUMA_LED_ID_US,
    PUMA_LED_ID_US_AMBER,
    PUMA_LED_ID_ONLINE,
    PUMA_LED_ID_LINK,
    PUMA_LED_ID_LINK_AMBER,
    PUMA_LED_ID_LINK1,
    PUMA_LED_ID_LINK2,
    PUMA_LED_ID_LINK3,
    PUMA_LED_ID_LINK4,
    PUMA_LED_ID_LINE1,
    PUMA_LED_ID_LINE2,
    PUMA_LED_ID_LINE3,
    PUMA_LED_ID_LINE4,
    PUMA_LED_ID_WIFI,
    PUMA_LED_ID_COMCAST_DS,
    PUMA_LED_ID_BATTERY,
    PUMA_LED_ID_MOCA,

    PUMA_LED_ID_NUM_LEDS
}
PUMA_LED_ID_e;
#else
typedef enum
{
    PUMA_LED_ID_POWER,
/* ARRIS ADD BEGIN */
    PUMA_LED_ID_POWER_RD,
    PUMA_LED_ID_POWER_GR,
    PUMA_LED_ID_POWER_BL,
/* ARRIS ADD END */
    PUMA_LED_ID_DS,
/*ARRIS ADD BEGIN*/
    PUMA_LED_ID_DS_BLUE,
/*ARRIS ADD END*/
    PUMA_LED_ID_US,
/*ARRIS ADD BEGIN*/   
    PUMA_LED_ID_US_BLUE,
/*ARRIS ADD END*/
    PUMA_LED_ID_ONLINE,
    PUMA_LED_ID_LINK,
    PUMA_LED_ID_LINE1,
    PUMA_LED_ID_LINE2,
    PUMA_LED_ID_LINE3,
    PUMA_LED_ID_LINE4,
    PUMA_LED_ID_BATTERY,
    PUMA_LED_ID_MOCA,
/* ARRIS ADD BEGIN */
    PUMA_LED_ID_TEL_RD,
    PUMA_LED_ID_TEL_GR,
    PUMA_LED_ID_ONLINE_RD,
    PUMA_LED_ID_ONLINE_GR,
    PUMA_LED_ID_WIFI_RD,
    PUMA_LED_ID_WIFI_GR,
    PUMA_LED_ID_MOCA20,
    PUMA_LED_ID_DECT_PAGE,
/* ARRIS ADD END */

    PUMA_LED_ID_NUM_LEDS
}
PUMA_LED_ID_e;
#endif

/*! \var typedef enum ledState_e
    \brief LED states.
*/
typedef enum
{
    PUMA_LED_STATE_OFF,
    PUMA_LED_STATE_ON,
    PUMA_LED_STATE_FLASH,
/* ARRIS ADD BEGIN */
    PUMA_LED_STATE_FLASH_SLOW,
#if defined (CONFIG_MACH_PUMA5)
    PUMA_LED_STATE_FLASH_CT,	  // Puma5
    PUMA_LED_STATE_FLASH_SLOW_CT, // Puma5
#endif
/* ARRIS ADD END */
    PUMA_LED_STATE_NUM_STATES
}
PUMA_LED_STATE_e;





typedef enum
{
    LED_HAL_MODE_LED_OFF           ,
    LED_HAL_MODE_LED_ON            ,
    LED_HAL_MODE_LED_ONESHOT_BACK  ,
    LED_HAL_MODE_LED_ONESHOT_ON    ,
    LED_HAL_MODE_LED_FLASH         ,
    LED_HAL_MODE_LED_ONESHOT_OFF   ,
    LED_HAL_MODE_LED_FLASH_BACK    ,
}
STATE_CFG_MODE_T;

/* ARRIS ADD BEGIN - Moved here from puma6_board_led.c */
#define    NO_LED_BIT                  0xFFFFFFFF

#define    PUMA6_LED_GPIO_DS           (89)
#define    PUMA6_LED_GPIO_DS_BLUE      (11)    /*For SB6190*/
#define    PUMA6_LED_GPIO_POWER        (61)
#define    PUMA6_LED_GPIO_POWER_RD     (61)     

#define    PUMA6_LED_GPIO_POWER_GR     (65)     /* 2492-LGI ER+ */
#define    PUMA6_LED_GPIO_POWER_BL     (55)     /* 2492-LGI ER+ */
#define    PUMA6_LED_GPIO_WIFI_RD      (11)     /* 2492-LGI ER+ */
#define    PUMA6_LED_GPIO_WIFI_GR      (31)     /* 2492-LGI ER+ */

#if 0
#define    PUMA6_LED_GPIO_POWER_GR     (77)     /* 2492-LGI SR1/2 */
#define    PUMA6_LED_GPIO_POWER_BL     (53)     /* 2492-LGI SR1/2 */
#define    PUMA6_LED_GPIO_WIFI_RD      (31)     /* 2492-LGI SR1/2 */
#define    PUMA6_LED_GPIO_WIFI_GR      (11)     /* 2492-LGI SR1/2 */
#endif

#define    PUMA6_LED_GPIO_BATTERY      (53)     

#define    PUMA6_LED_GPIO_ONLINE       (56)
#define    PUMA6_LED_GPIO_LINK         (NO_LED_BIT) /*never used*/
#define    PUMA6_LED_GPIO_MOCA         (11)
#define    PUMA6_LED_GPIO_MOCA20       (11)
#define    PUMA6_LED_GPIO_US           (98)    /*For SB6190*/
#define    PUMA6_LED_GPIO_US_BLUE      (53)    /*For SB6190*/

#define    PUMA6_LED_GPIO_LINE1        (98)
#define    PUMA6_LED_GPIO_LINE2        (69)
#define    PUMA6_LED_GPIO_LINE3        (NO_LED_BIT) /*never used*/
#define    PUMA6_LED_GPIO_LINE4        (NO_LED_BIT) /*never used*/

#define    PUMA6_LED_GPIO_TEL_RD       (98)    
#define    PUMA6_LED_GPIO_TEL_GR       (69)    
#define    PUMA6_LED_GPIO_ONLINE_RD    (89)     
#define    PUMA6_LED_GPIO_ONLINE_GR    (56)
#define    PUMA6_LED_GPIO_DECT_PAGE    (77)
/* ARRIS ADD END */


/*---------------------------------------------------------------------------
 * Configuration Support.
 *-------------------------------------------------------------------------*/
typedef struct led_cfg
{
    Uint32 domain;
    Uint32 pos_map[ LED_HAL_BITMASK_REG_WIDTH ]; /* Indicates the position of LED bit; index 0 -> range of 0 - 31, index 1 -> range of 32 - 63 etc.*/
} LED_CFG_T;

typedef struct state_cfg
{
    Uint32          id;
    Uint32          mode;
    Uint32          param1;
    Uint32          param2;
    Uint32          led_val[ LED_HAL_BITMASK_REG_WIDTH ];
    LED_CFG_T       led_cfg;
} STATE_CFG_T;

typedef struct mod_cfg
{
    Int8            name[50];
    Uint32          instance;
    STATE_CFG_T     state_cfg;

} MOD_CFG_T;

typedef struct led_funcs
{
    Uint32 domain;
    Uint32 pos_map[ LED_HAL_BITMASK_REG_WIDTH ];  /* Indicates the position of LED bit; index 0 -> range of 0 - 31, index 1 -> range of 32 - 63 etc.*/
    Uint32 off_val[ LED_HAL_BITMASK_REG_WIDTH ]; /* Init values to be reflected on the LED(s). */
    Int32 (*outVal)(Uint32 led_val, Uint32 pos_map, Uint32 index);

} LED_FUNCS_T;

/*--------------------------------------------------------------------------
 * Public API(s)
 *------------------------------------------------------------------------*/
typedef void MOD_OBJ_HND;
typedef void LED_OBJ_HND;

MOD_OBJ_HND*    led_hal_register            (Int8 *name, Uint32 instance);
void            led_hal_unregister          (MOD_OBJ_HND *mod);

Int32           led_hal_action              (MOD_OBJ_HND *mod, Uint32 state_id);

LED_OBJ_HND*    led_hal_install_callbacks   (LED_FUNCS_T *led_funcs);
Int32           led_hal_uninstall_callbacks (LED_OBJ_HND *led);

Int32           led_hal_configure_mod       (MOD_CFG_T *cfg);

Int32           led_hal_dump_cfg_info       (Int8 *buf, Int32 size);

Int32           led_hal_init( void );
Int32           led_hal_exit( void );

#endif

