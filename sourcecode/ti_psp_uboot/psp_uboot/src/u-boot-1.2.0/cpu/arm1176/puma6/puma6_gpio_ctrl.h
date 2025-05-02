/*
 *  puma6_gpio_ctrl
 *
 *  GPL LICENSE SUMMARY
 *
 *  Copyright(c) 2012-2014 Intel Corporation. All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of version 2 of the GNU General Public License as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *  The full GNU General Public License is included in this distribution
 *  in the file called LICENSE.GPL.
 *
 *  Contact Information:
 *    Intel Corporation
 *    2200 Mission College Blvd.
 *    Santa Clara, CA  97052
 *
 * The file contains the main data structure and API definitions for U-Boot GPIO driver
 *
 */

/** \file   puma6_gpio_ctrl.h
 *  \brief  GPIO config control APIs. 
 *          
 *  \author     Amihay Tabul
 *
 *  \version    0.1     Created
 */

#ifndef _PUMA6_GPIO_CTRL_H_
#define _PUMA6_GPIO_CTRL_H_


/*****************************************************************************
 * GPIO Control
 *****************************************************************************/
#define NO_LED_BIT          (0xFFFFFFFF)

/* Puma6 list of all GPIOs that are used for LED (in intel boards) */
/* GPIO numbers for Intel HarborPark and HarborPark-MG boards */
// ARRIS change the LED GPIO definitions for our boards
#define    PUMA6_LED_GPIO_DS       (89)    
#define    PUMA6_LED_GPIO_POWER    (61)   
#define    PUMA6_LED_GPIO_POWER_RD (61)   
#define    PUMA6_LED_GPIO_POWER_GR (65)   
#define    PUMA6_LED_GPIO_POWER_BL (55)   

#ifdef CONFIG_TI_BBU
#define    PUMA6_LED_GPIO_BATTERY  (53)     
#else
#define    PUMA6_LED_GPIO_BATTERY  (53) 
#endif
#define    PUMA6_LED_GPIO_ONLINE_OLD (60) /* ARRIS */
#define    PUMA6_LED_GPIO_ONLINE   (56) /* ARRIS */
//#define    PUMA6_LED_GPIO_LINK     (59)
#define    PUMA6_LED_GPIO_MOCA     (11)  
//#define    PUMA6_LED_GPIO_US       (61)  
#define    PUMA6_LED_GPIO_LINE2    (69)  
#define    PUMA6_LED_GPIO_LINE1    (98)  
#define    PUMA6_LED_GPIO_STANDBY  (44)
#define    PUMA6_LED_GPIO_DECT_PAGE (77)
// END ARRIS
/* GPIO numbers for Intel Falconmine and Catisland boards - only the diffrances from HarborPark are listed */
#define    PUMA6_LED_GPIO_POWER_FM_CI  (52)   /* GPIO number 52 is used as Power LED in FM and CI boards */       
#define    PUMA6_LED_GPIO_DS_FM_CI     (54)     
#define    PUMA6_LED_GPIO_ONLINE_FM_CI (55)
#define    PUMA6_LED_GPIO_LINK_FM_CI   (56)  

/* GPIO numbers for Intel Golden Springs board - only the diffrences from HarborPark are listed */
#define    PUMA6_LED_GPIO_POWER_GS     (65)        
#define    PUMA6_LED_GPIO_DS_GS        (64)     
#define    PUMA6_LED_GPIO_ONLINE_GS    (44)
#define    PUMA6_LED_GPIO_LINK_GS      (45)  

/* GPIO numbers for Intel Baker Lake board - only the diffrences from Golden Springs are listed */
#define    PUMA6_LED_GPIO_MOCA_BL      (69)
#define    PUMA6_LED_GPIO_LINE1_BL     (74)
#define    PUMA6_LED_GPIO_LINE2_BL     (53)
#define    PUMA6_LED_GPIO_US_BL        (64) /* BL: US share same GPIO as DS (64) */
#define    PUMA6_LED_GPIO_LINK_BL      (NO_LED_BIT)


/* Puma6 list of all GPIOs (that are not LED) that are used in intel boards */
#define    PUMA6_TUNER_RESET_GPIO      (97)
#define    PUMA6_ADI_RESET	       (43)  /* ARRIS MOD */
// ARRIS ADD
#define    PUMA6_MOCA20_RESET_N        (43)
#define    PUMA6_MOCA_MDIO             (25)
#define    PUMA6_MOCA_MDC              (24)
#define    PUMA6_FWDNLD_N              (73)
#define    PUMA6_BATCHG_RESET_N        (72)
#define    PUMA6_BATSN                 (58)
#define    PUMA6_DECT_RESET_N          (78)
#define    PUMA6_DECT_PAGE_N           (90) /* no special handling needed */
#define    PUMA6_DECT_INT_N            (102)
#define    PUMA6_DATA_PWR_N            (10)
#define    PUMA6_DAC_PSM_GPIO          (101)
// END ARRIS

typedef enum PumaGpioTypes
{
    FUNCTIONAL_PIN  = 0,
    GPIO_PIN        = 1,
    GPIO_OUTPUT_PIN = 0,
    GPIO_INPUT_PIN  = 1,
} PumaGpioTypes_t;

int puma6_gpioInit(unsigned int puma6_boardtype_id);
int puma6_gpioInBit(unsigned int gpio_pin);
int puma6_gpioOutBit(unsigned int gpio_pin, unsigned int value);
int puma6_gpioCtrl(unsigned int gpio_pin, PumaGpioTypes_t pin_mode, PumaGpioTypes_t pin_direction);

#endif /* _PUMA6_GPIO_CTRL_H_ */
