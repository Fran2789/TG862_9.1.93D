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

/** \file   puma6_gpio_ctrl.c
 *  \brief  GPIO config control APIs. 
 *          
 *  \author     Amihay Tabul
 *
 *  \version    0.1     Created
 */

#include "puma6_gpio_ctrl.h"
#include "arm_atom_mbx.h"
#include "docsis_ip_boot_params.h"
#include "puma6_hw_mutex.h"
#include "puma6_aep.h"
#include <common.h>
#include <command.h>

/*******************************************************************************************
    GPIO Unit Register Map
Offset      Symbol              Register Name/Function                          Default
00h         GPIO_OUT_0          GPIO 31:0 pin data output register.             0000_0000
04h         GPIO_OUT_EN_0       GPIO 31:0 pin output driving register.          0000_0000
08h         GPIO_INPUT_0        GPIO 31:0 pin status input level register.      XXXX_XXXX
0Ch         GPIO_INT_STAT_0     GPIO 31:0 interrupt status register.            0000_0000
10h         GPIO_INT_EN_0       GPIO 31:0 Interrupt Enable Register             0000_0000
14h         GPIO_IN_MODE_LE_0   GPIO 31:0 interrupt Level/Edge                  0000_0000
18h         GPIO_in_MODE_RF_0   GPIO 31:0 Interrupt Rise/Fall                   0000_0000
1Ch         GPIO_MUX_CNTL       GPIO Mux Control                                0000_0000
20h         GPIO_OUT_1          GPIO 63:32 pin data output register.            0000_0000
24h         GPIO_OUT_EN_1       GPIO 63:32 pin output driving register.         0000_0000
28h         GPIO_INPUT_1        GPIO 63:32 pin status input level register.     XXXX_XXXX
2Ch         GPIO_INT_STAT_1     GPIO 63:32 interrupt status register.           0000_0000
30h         GPIO_INT_EN_1       GPIO 63:32 Interrupt Enable Register            0000_0000
34h         GPIO_IN_MODE_LE_1   GPIO 63:32 interrupt Level/Edge                 0000_0000
38h         GPIO_in_MODE_RF_1   GPIO 63:32 Interrupt Rise/Fall                  0000_0000
3Ch            Reserved         See Note                                        
40h         GPIO_OUT_2          GPIO 95:64 pin data output register.            0000_0000
44h         GPIO_OUT_EN_2       GPIO 95:64 pin output driving register.         0000_0000
48h         GPIO_INPUT_2        GPIO 95:64 pin status input level register.     XXXX_XXXX
4Ch         GPIO_INT_STAT_2     GPIO 95:64 interrupt status register.           0000_0000
50h         GPIO_INT_EN_2       GPIO 95:64 Interrupt Enable Register            0000_0000
54h         GPIO_IN_MODE_LE_2   GPIO 95:64 interrupt Level/Edge                 0000_0000
58h         GPIO_in_MODE_RF_2   GPIO 95:64 Interrupt Rise/Fall                  0000_0000
54h            Reserved         See Note                                        
60h         GPIO_OUT_3          GPIO 127:96 pin data output register.           0000_0000
64h         GPIO_OUT_EN_3       GPIO 127:96 pin output driving register.        0000_0000
68h         GPIO_INPUT_3        GPIO 127:96 pin status input level register.    XXXX_XXXX
6Ch         GPIO_INT_STAT_3     GPIO 127:96 interrupt status register.          0000_0000
70h         GPIO_INT_EN_3       GPIO 127:96 Interrupt Enable Register           0000_0000
74h         GPIO_IN_MODE_LE_3   GPIO 127:96 interrupt Level/Edge                0000_0000
78h         GPIO_in_MODE_RF_3   GPIO 127:96 Interrupt Rise/Fall                 0000_0000
7Ch            Reserved         See Note    
80h         GPIO_CLEAR_0        GPIO 31:0 pin data output register clear    
84h         GPIO_SET_0          GPIO 31:0 pin data output register set  
88h         GPIO_POLARITY_0     GPIO 31:0 pin data input polarity invert    
8Ch            Reserved         See Note    
90h         GPIO_CLEAR_1        GPIO 63:32 pin data output register clear   
94h         GPIO_SET_1          GPIO 63:32 pin data output register set 
98h         GPIO_POLARITY_1     GPIO 63:32 pin data input polarity invert   
9Ch            Reserved         See Note    
A0h         GPIO_CLEAR_2        GPIO 95:64 pin data output register clear   
A4h         GPIO_SET_2          GPIO 95:64 pin data output register set 
A8h         GPIO_POLARITY_2     GPIO 95:64 pin data input polarity invert   
ACh            Reserved         See Note    
B0h         GPIO_CLEAR_3        GPIO 127:96 pin data output register clear  
B4h         GPIO_SET_3          GPIO 127:96 pin data output register set    
B8h         GPIO_POLARITY_3     GPIO 127:96 pin data input polarity invert  
BCh         GPIO_GROUP_3        GPIO 127:112 interrupt group select 
C0h to FFh     Reserved         See Note    
***********************************************************************************************/

/****************************************************************************/
/****************************************************************************/
/*                          GPIO Defines                                    */
/****************************************************************************/
/****************************************************************************/
#define PUMA6_MAX_GPIOS                             (128)
#define AVALANCHE_GPIO_BASE                         (0x0FFE0400)

/*
    PUMA6_GPIO_OUT:
    The output data register controls the logical levels on the pins that are configured as GPIO Outputs.
    These registers may be written or read.
    Address Offsets: 00h, 20h, 40h, 60h
*/
#define PUMA6_GPIO_OUT_REG                              (AVALANCHE_GPIO_BASE + 0x0)
/*
    PUMA6_GPIO_OUT_EN:
    GPIO Direction Control 
        0:  Input mode
        1:  Output mode
    Address Offsets: 04h, 24h, 44h, 64h
*/
#define PUMA6_GPIO_OUT_EN_REG                           (AVALANCHE_GPIO_BASE + 0x4)
/*
    PUMA6_GPIO_INPUT:
    These registers may be only read and reflect the logical levels on the external pins.
    Logical low signal is read as '0', and logical high level is read as '1'.
    Address Offsets: 08h, 28h, 48h, 68h
*/
#define PUMA6_GPIO_INPUT_REG                            (AVALANCHE_GPIO_BASE + 0x8)
/*
    PUMA6_GPIO_INT_STATUS:
    These registers may be read and written and represent an active interrupt status when set.
    Writing a '1' to the bit causes it to be reset. If there is pending event a cleared event may not
    be visible in a subsequent register read. 
    Address Offsets: 0Ch, 2Ch, 4Ch, 6Ch
*/
#define PUMA6_GPIO_INT_STATUS_REG                       (AVALANCHE_GPIO_BASE + 0xC)
/*
    PUMA6_GPIO_INT_EN:
    These registers may be read and written and control the operation of the pins assigned for interrupt request functions.
    All interrupt requests are directly routed into the interrupt controller. It is recommended to set up the Mode registers,
    GPIO_TYP0 and GPIO_TYP1, prior to enabling the interrupt. If a GPIO pin is a shared function and those shared signals are active,
    enabling the interrupt may cause unexpected interrupts. Interrupts are detected based on the requirements of the Mode registers
    and the state of the pin is not required to persist beyond those conditions; however, the interrupt remains asserted until the interrupt
    status bit is cleared.

    GPIO Interrupt Enable Control for pins GPIO0-GPIO31, When set to "1", the corresponding pin may generate interrupt request
    to the main interrupt controller. When cleared, the interrupt requests will not be generated.
    Note:  if a GPIO pin is configured as an output, the corresponding GPIO_INT_EN bit in GPIO_INT register must be cleared.
    Otherwise, you may see an interrupt get "looped back" to the internal logic.
    Address Offsets: 10h, 30h, 50h, 70h
*/
#define PUMA6_GPIO_INT_EN_REG                           (AVALANCHE_GPIO_BASE + 0x10)
/*
    PUMA6_GPIO_INT_MODELE:
    GPIO Interrupt Control, Bits in this register allow the selection of a level or an edge trigger for the interrupt. 
    0 - Level detection
    1 - Edge detection
    Address Offset: 14h, 34h, 54h, 74h 
*/
#define PUMA6_GPIO_INT_MODELE_REG                       (AVALANCHE_GPIO_BASE + 0x14)
/*
    PUMA6_GPIO_INT_MODERF:
    GPIO Interrupt Control, Bits in this register allow the selection of either rising or falling edge (or level) modes
    0 - rising edge
    1 - falling edge
    Address Offset: 18h, 38h, 58h, 78h 
*/
#define PUMA6_GPIO_INT_MODERF_REG                       (AVALANCHE_GPIO_BASE + 0x18)
/*
    PUMA6_GPIO_MUXCNTL:
    This register may be read and written and controls the state of port output enables for Smart Card 0 and 1, UART 1 and 2 
    (UART 0 does not share a pin), and the GBE_LINK output pins.  It also provides the PWM trigger source selection.
    See PUB PWM document for more information about the trigger inputs.  Also note that the shared GPIO/PWM signals have the 
    trigger swapped so that an adjacent pwm may act as a trigger or allow an external trigger input.
    For example shared pin GPIO_55_PWM0 may use GPIO_52_PWM3 as a trigger or as an external trigger input.
    Address Offset: 1Ch 
*/
#define PUMA6_GPIO_MUXCNTL_REG                          (AVALANCHE_GPIO_BASE + 0x1C)


/*
    PUMA6_GPIO_CLEAR:
    Writing a '1' to a bit in this register has the effect of clearing the corresponding bit in the GPIO output data register.
    These bits are write only. 
    Address Offsets: 80h, 90h, A0h, B0h
*/
#define PUMA6_GPIO_CLEAR_REG                            (AVALANCHE_GPIO_BASE + 0x80)
/*
    PUMA6_GPIO_SET:
    Writing a '1' to a bit in this register has the effect of setting the corresponding bit in the GPIO output data register. 
    These bits are write only. 
    Address Offsets: 84h, 94h, A4h, B4h
*/
#define PUMA6_GPIO_SET_REG                              (AVALANCHE_GPIO_BASE + 0x84)
/*
    PUMA6_GPIO_POLARITY:
    Writing a '1' to a bit in this register has the effect of inverting the corresponding data input bit. 
    Writing a '0' does not invert the corresponding data input. These bits are read/write.
    Address Offsets: 88h, 98h, A8h, B8h
*/
#define PUMA6_GPIO_POLARITY_REG                         (AVALANCHE_GPIO_BASE + 0x88)
/*
    PUMA6_GPIO_GROUP:
    Controls the routing of the GPIO interrupts to either gpio_int (legacy interrupt), gpio_irq_a, or gpio_irq_b.
    These bits are read/write.
    Address Offset: BCh
*/
#define PUMA6_GPIO_GROUP_REG                            (AVALANCHE_GPIO_BASE + 0xBC)

/****************************************************************************/
/****************************************************************************/
/*                          GPIO Helper macros                              */
/****************************************************************************/
/****************************************************************************/
/* Helper function for 32 bit endian swapping */
#define ENDIAN_SWAP32(x) ({ __typeof__ (x) val = (x); \
                               (((val & 0x000000FF)<<24) | ((val & 0x0000FF00)<<8) | ((val & 0x00FF0000)>>8)  | ((val & 0xFF000000)>>24)); \
                          })

/* Macro to read GPIO register from base address + offset */
#define PUMA6_GPIO_REG_GET(reg)                   g_gpio_reg_get(reg)
#define PUMA6_GPIO_REG_SET(reg, val)              g_gpio_reg_set(reg,val)

/* This Macro will help to calc the GPIO reg for the 0x20 spacing , we have 4 groups of Regs to hand 128 bits of GPIOs */
#define PUMA6_GPIO_REG_CALC_20(gpio_inx,reg)      (((int)((gpio_inx)/32))*0x20 + (reg)) 
/* This Macro will help to calc the GPIO reg for the 0x10 spacing , we have 4 groups of Regs to hand 128 bits of GPIOs */
#define PUMA6_GPIO_REG_CALC_10(gpio_inx,reg)      (((int)((gpio_inx)/32))*0x10 + (reg)) 

#define PUMA6_GPIO_REG_ADDR(gpio_inx,reg) \
    (((reg) > PUMA6_GPIO_MUXCNTL_REG)  ?  \
        /* The spacing is 0x10 between the 4 Regs */  \
        PUMA6_GPIO_REG_CALC_10((gpio_inx),(reg)) \
      : \
        /* The spacing is 0x20 between the 4 Regs */  \
        PUMA6_GPIO_REG_CALC_20((gpio_inx),(reg)) \
     )
#define BIT(i)    ((1 << (i)))

#define HW_VALUE_LED_OFF    (1)
#define HW_VALUE_LED_ON     (0)

/* To open debug prints use this macro - for printf to work make sure the init_baudrate, serial_init and console_init_f */     
/* To open debug/info prints use this macro */     
//#define GPIO_DEBUG_OUTPUT_ON
//#define GPIO_INFO_OUTPUT_ON
//
/* To open 'led' command in uboot shell */
// ARRIS CHANGE - don't enable uboot led command
//#define GPIO_LED_CMD

/* Debug */
#ifdef  GPIO_DEBUG_OUTPUT_ON
    #define GPIO_DEBUG_OUTPUT(fmt, args...) printf("Puma6-Uboot GPIO Debug %s: " fmt, __FUNCTION__ , ## args)
#else
    #define GPIO_DEBUG_OUTPUT(fmt, args...)
#endif

/* Info */
#ifdef  GPIO_INFO_OUTPUT_ON
    #define GPIO_INFO_OUTPUT(fmt, args...) printf("Puma6-Uboot GPIO Info %s: " fmt, __FUNCTION__ , ## args)
#else   
    #define GPIO_INFO_OUTPUT(fmt, args...)
#endif  



typedef unsigned int (*gpio_reg_get_t) (unsigned int addr);
typedef void         (*gpio_reg_set_t) (unsigned int addr, unsigned int data);


static unsigned int gpio_reg_get_default (unsigned int addr);
static void gpio_reg_set_default (unsigned int addr,unsigned int data);

static gpio_reg_get_t g_gpio_reg_get = gpio_reg_get_default;
static gpio_reg_set_t g_gpio_reg_set = gpio_reg_set_default;

#ifdef GPIO_LED_CMD
static unsigned int gpio_power = 0;
static unsigned int gpio_ds = 0;
static unsigned int gpio_us = 0;
static unsigned int gpio_online = 0;
static unsigned int gpio_link = 0;
static unsigned int gpio_line1 = 0;
static unsigned int gpio_line2 = 0;
#endif
 
/****************************************************************************/
/****************************************************************************/
/*                                GPIO     API                              */
/****************************************************************************/
/****************************************************************************/

/*
    Important Note for Puma6 SoC:
    - The CEFDK will be responsible for configuring the ATOM GPIOs.
    - The U-Boot will be responsible for configuring the ARM11 GPIOs.
    - After ARM11 is out of reset, the ATOM can only use the following GPIO regs:
        GPIO_CLEARn—GPIO Data Output Registers Clear (GPIOs 0-127).
        GPIO_SETn—GPIO Data Output Registers Set (GPIOs 0-127).
    - In ARM11, only U-Boot will use all the GPIO regs to configure the ARM11 GPIOs.
    - ATOM and ARM11 kernels will only use GPIO_CLEARn and GPIO_SETn regs.
*/ 

static unsigned int gpio_reg_get_default (unsigned int addr)
{
    printf("ERROR:  GPIO driver is not initialized correctly\n");
    return 0;
}

static void gpio_reg_set_default (unsigned int addr,unsigned int data)
{
    printf("ERROR:  GPIO driver is not initialized correctly\n");
    return ;
}

static unsigned int gpio_reg_get_direct (unsigned int addr)
{
    return (*((volatile unsigned int *)(addr)));
}

static void gpio_reg_set_direct (unsigned int addr,unsigned int data)
{
    ((*((volatile unsigned int *)(addr))) = (data));
    return ;
}
static unsigned int gpio_reg_get_aep (unsigned int addr)
{
    unsigned int value = 0;
    if (aep_ipc_gpio_read(addr-AVALANCHE_GPIO_BASE,&value) == AEP_STATUS_OK)
    {
        printf("[AEP-GPIO] Read  0x%.8X, get 0x%.8X\n",addr,value); 
        return value;
    }

    /* Error */
    printf("ERROR: Read from AEP address=%x failed\n",addr);
    return 0;
}

static void gpio_reg_set_aep (unsigned int addr,unsigned int data)
{
    printf("[AEP-GPIO] Write 0x%.8X, set 0x%.8X\n",addr,data);
    if (aep_ipc_gpio_write(addr-AVALANCHE_GPIO_BASE, data, 0xFFFFFFFF) == AEP_STATUS_OK)
        return;

    /* Error */
    printf("ERROR: Write to AEP address=%x, value=%x failed\n",addr,data);
    return;
}


/*! \fn int puma6_gpioInit(unsigned int puma6_boardtype_id)
    \brief This API initializes all Puma6 GPIO that need to be in output mode and also set the default value.
    \param puma6_boardtype - puma6 boardtype.
    \return Returns (0) on success and (-1) on failure
*/
int puma6_gpioInit(unsigned int puma6_boardtype_id)
{

#ifndef GPIO_LED_CMD
    unsigned int gpio_power = 0;
    unsigned int gpio_ds = 0;
    unsigned int gpio_us = 0;
    unsigned int gpio_online = 0;
    unsigned int gpio_link = 0;
    unsigned int gpio_line1 = 0;
    unsigned int gpio_line2 = 0;
#endif

    // printf("[AEP-GPIO] puma6_gpioInit: - started (id=%d)\n",puma6_boardtype_id); 
    if (aep_is_active() == AEP_NOT_ACTIVE)
    {
        /* AEP is not active - use direct registers access */
        g_gpio_reg_set = gpio_reg_set_direct;
        g_gpio_reg_get = gpio_reg_get_direct;
    }
    else
    {
        /* Open All GPIO - Debug Only */
		//printf("[AEP-GPIO] puma6_gpioInit: - Open all GPIOs access\n"); 
        //aep_ipc_set_gpio_access(0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF);

        /* AEP is active - use aep registers access */
        g_gpio_reg_set = gpio_reg_set_aep;
        g_gpio_reg_get = gpio_reg_get_aep;
    }

    GPIO_INFO_OUTPUT("init LED GPIO for puma6_boardtype_id=%d \n",puma6_boardtype_id);

    gpio_line1 = PUMA6_LED_GPIO_LINE1;
    gpio_line2 = PUMA6_LED_GPIO_LINE2; 
    // ARRIS REMOVE (we don't have US LED) gpio_us = PUMA6_LED_GPIO_US;

    switch(puma6_boardtype_id)
    {
        case PUMA6_HP_BOARD_ID:
        case PUMA6_HP_MG_BOARD_ID:
        {   /* GPIO for HP and HP-MG */
            gpio_power  = PUMA6_LED_GPIO_POWER;  
            gpio_ds     = PUMA6_LED_GPIO_DS;        
            gpio_online = PUMA6_LED_GPIO_ONLINE;
// ARRIS REMOVE            gpio_link   = PUMA6_LED_GPIO_LINK;    
            GPIO_INFO_OUTPUT("init LED GPIO for HP and HP-MG boards\n");
            /* PUMA6_ADI_RESET is needed for HP and HP-MG only, as only them have MoCA */
            puma6_gpioCtrl(PUMA6_ADI_RESET,  GPIO_PIN, GPIO_OUTPUT_PIN);
            break;
        }
        case PUMA6_FM_BOARD_ID:
        case PUMA6_CI_BOARD_ID:
        {   /* true for FM and CI board type */
            gpio_power  = PUMA6_LED_GPIO_POWER_FM_CI;   
            gpio_ds     = PUMA6_LED_GPIO_DS_FM_CI;         
            gpio_online = PUMA6_LED_GPIO_ONLINE_FM_CI; 
            gpio_link   = PUMA6_LED_GPIO_LINK_FM_CI;     
            GPIO_INFO_OUTPUT("init LED GPIO for FM and CI boards\n");
            /* ATOM MUX Reg */
            /* Need to update the ARM11 uboot to configure the ATOM GPIO mux reg: */
            /* - For HP we will not do anything the ATOM done the write of 0x88. */
            /* - For FM and CI the ARM11 will write 0x89.  */
            //GPIO_INFO_OUTPUT("Setting GPIO Mux Reg to 0x89 for FM and CI boards (old value is 0x%x)\n",PUMA6_GPIO_REG_GET(PUMA6_GPIO_MUXCNTL_REG));
            // ARRIS NOTE: Intel commented out the following because it does not work.
            // Endian disagreement between atom and ARM means they should set it to 0x89000000
            //PUMA6_GPIO_REG_SET(PUMA6_GPIO_MUXCNTL_REG, (0x89));  
            break;
        }
        case PUMA6_GS_BOARD_ID:
        {   /* true for GS board type */
            gpio_power  = PUMA6_LED_GPIO_POWER_GS;   
            gpio_ds     = PUMA6_LED_GPIO_DS_GS;         
            gpio_online = PUMA6_LED_GPIO_ONLINE_GS; 
            gpio_link   = PUMA6_LED_GPIO_LINK_GS;     

            GPIO_INFO_OUTPUT("init LED GPIO for GS board\n");
            break;
        }
        case PUMA6_BL_BOARD_ID:
        {   /* true for BL board type */
            gpio_power  = PUMA6_LED_GPIO_POWER_GS;
            gpio_ds     = PUMA6_LED_GPIO_DS_GS;
            gpio_online = PUMA6_LED_GPIO_ONLINE_GS;
            gpio_link   = PUMA6_LED_GPIO_LINK_BL;
            gpio_line1 = PUMA6_LED_GPIO_LINE1_BL;
            gpio_line2 = PUMA6_LED_GPIO_LINE2_BL;
            gpio_us = PUMA6_LED_GPIO_US_BL; 

            GPIO_INFO_OUTPUT("init LED GPIO for BL board\n");
            break;
        }
        case PUMA6_UNKNOWN_BOARD_ID:
        default:
        {
            GPIO_INFO_OUTPUT("init LED ERROR  puma6_boardtype = PUMA6_UNKNOWN_BOARD_ID !!! \n");
            return (-1);
        }
    }

    /* Set all LED GPIO to be GPIO output */
    // ARRIS REMOVE puma6_gpioCtrl(gpio_us,    GPIO_PIN, GPIO_OUTPUT_PIN);
    puma6_gpioCtrl(gpio_line2, GPIO_PIN, GPIO_OUTPUT_PIN);
    puma6_gpioCtrl(gpio_line1, GPIO_PIN, GPIO_OUTPUT_PIN);
    puma6_gpioCtrl(gpio_power,           GPIO_PIN, GPIO_OUTPUT_PIN);  
    puma6_gpioCtrl(gpio_ds,              GPIO_PIN, GPIO_OUTPUT_PIN);     
    puma6_gpioCtrl(gpio_online,GPIO_PIN, GPIO_OUTPUT_PIN); 
    // ARRIS REMOVE puma6_gpioCtrl(gpio_link,  GPIO_PIN, GPIO_OUTPUT_PIN); 


    /* ARRIS ADD */
    puma6_gpioCtrl(PUMA6_LED_GPIO_POWER_RD,  GPIO_PIN, GPIO_OUTPUT_PIN);
    puma6_gpioCtrl(PUMA6_LED_GPIO_POWER_GR,  GPIO_PIN, GPIO_OUTPUT_PIN);
    puma6_gpioCtrl(PUMA6_LED_GPIO_POWER_BL,  GPIO_PIN, GPIO_OUTPUT_PIN);

    puma6_gpioCtrl(PUMA6_LED_GPIO_ONLINE_OLD,  GPIO_PIN, GPIO_OUTPUT_PIN);
    puma6_gpioCtrl(PUMA6_LED_GPIO_STANDBY, GPIO_PIN, GPIO_OUTPUT_PIN);
    puma6_gpioCtrl(PUMA6_LED_GPIO_DECT_PAGE, GPIO_PIN, GPIO_OUTPUT_PIN);
    /* END ARRIS */

    /* Set LEDs to OFF */
    // ARRIS REMOVE puma6_gpioOutBit(gpio_us,    HW_VALUE_LED_OFF);
    puma6_gpioOutBit(gpio_line2 ,HW_VALUE_LED_OFF);
    puma6_gpioOutBit(gpio_line1 ,HW_VALUE_LED_OFF);
    puma6_gpioOutBit(gpio_ds,    HW_VALUE_LED_OFF);
    puma6_gpioOutBit(gpio_online,HW_VALUE_LED_OFF);
    // ARRIS REMOVE puma6_gpioOutBit(gpio_link,  HW_VALUE_LED_OFF);
    /* ARRIS ADD */
    puma6_gpioOutBit(PUMA6_LED_GPIO_ONLINE_OLD,  HW_VALUE_LED_OFF);
    puma6_gpioOutBit(PUMA6_LED_GPIO_STANDBY, HW_VALUE_LED_OFF);
    puma6_gpioOutBit(PUMA6_LED_GPIO_DECT_PAGE, HW_VALUE_LED_OFF);
    /* END ARRIS */

    if (PUMA6_LED_GPIO_BATTERY != NO_LED_BIT)
    {
       puma6_gpioCtrl(PUMA6_LED_GPIO_BATTERY, GPIO_PIN, GPIO_OUTPUT_PIN);
       puma6_gpioOutBit(PUMA6_LED_GPIO_BATTERY,HW_VALUE_LED_OFF);
       GPIO_INFO_OUTPUT("set LED BATTERY gpio_pin=%d to output value %d \n",PUMA6_LED_GPIO_BATTERY, HW_VALUE_LED_OFF);
    }
    
    /* Set LED power LED to ON */
    puma6_gpioOutBit(gpio_power,          HW_VALUE_LED_ON);

    /* ARRIS ADD BEGIN */
    puma6_gpioOutBit(PUMA6_LED_GPIO_POWER_RD,          HW_VALUE_LED_ON);
    puma6_gpioOutBit(PUMA6_LED_GPIO_POWER_BL,          HW_VALUE_LED_ON);
    puma6_gpioOutBit(PUMA6_LED_GPIO_POWER_GR,          HW_VALUE_LED_ON);

    GPIO_INFO_OUTPUT("set LED POWER_RD gpio_pin=%d to output value %d \n", PUMA6_LED_GPIO_POWER_RD, HW_VALUE_LED_ON);
    GPIO_INFO_OUTPUT("set LED POWER_GR gpio_pin=%d to output value %d \n", PUMA6_LED_GPIO_POWER_GR, HW_VALUE_LED_ON);
    GPIO_INFO_OUTPUT("set LED POWER_BL gpio_pin=%d to output value %d \n", PUMA6_LED_GPIO_POWER_BL, HW_VALUE_LED_ON);
    /* ARRIS ADD END */

    // ARRIS REMOVE GPIO_INFO_OUTPUT("set LED US gpio_pin=%d to output value %d \n",    gpio_us,    HW_VALUE_LED_OFF);
    GPIO_INFO_OUTPUT("set LED LINE1 gpio_pin=%d to output value %d \n", gpio_line1, HW_VALUE_LED_OFF);
    GPIO_INFO_OUTPUT("set LED LINE2 gpio_pin=%d to output value %d \n", gpio_line2, HW_VALUE_LED_OFF);
    GPIO_INFO_OUTPUT("set LED POWER gpio_pin=%d to output value %d \n", gpio_power, HW_VALUE_LED_ON);
    GPIO_INFO_OUTPUT("set LED ONLINE gpio_pin=%d to output value %d \n",gpio_online,HW_VALUE_LED_OFF);
    // ARRIS REMOVE GPIO_INFO_OUTPUT("set LED LINK gpio_pin=%d to output value %d \n",  gpio_link,  HW_VALUE_LED_OFF);
    GPIO_INFO_OUTPUT("set LED DS gpio_pin=%d to output value %d \n",    gpio_ds,    HW_VALUE_LED_OFF);
    /* ARRIS ADD */
    GPIO_INFO_OUTPUT("set LED ONLINE_OLD gpio_pin=%d to output value %d \n",PUMA6_LED_GPIO_ONLINE_OLD,  HW_VALUE_LED_OFF);
    GPIO_INFO_OUTPUT("set LED STANDBY gpio_pin=%d to output value %d \n",   PUMA6_LED_GPIO_STANDBY,     HW_VALUE_LED_OFF);
    GPIO_INFO_OUTPUT("set LED DECT PAGE gpio_pin=%d to output value %d \n", PUMA6_LED_GPIO_DECT_PAGE,   HW_VALUE_LED_OFF);
    /* END ARRIS */
    
#if 0 // ARRIS ADD
    /* For GS board this GPIO aren't in use */
    if ( (puma6_boardtype_id != PUMA6_GS_BOARD_ID) && (puma6_boardtype_id != PUMA6_BL_BOARD_ID) )
    {
        puma6_gpioCtrl(PUMA6_LED_GPIO_MOCA,  GPIO_PIN, GPIO_OUTPUT_PIN);
        puma6_gpioCtrl(PUMA6_LED_GPIO_LINE3, GPIO_PIN, GPIO_OUTPUT_PIN);
        puma6_gpioCtrl(PUMA6_LED_GPIO_LINE4, GPIO_PIN, GPIO_OUTPUT_PIN);

        /* Set LEDs to OFF */
        puma6_gpioOutBit(PUMA6_LED_GPIO_MOCA, HW_VALUE_LED_OFF);
        puma6_gpioOutBit(PUMA6_LED_GPIO_LINE3,HW_VALUE_LED_OFF);
        puma6_gpioOutBit(PUMA6_LED_GPIO_LINE4,HW_VALUE_LED_OFF);

        GPIO_INFO_OUTPUT("set LED MOCA gpio_pin=%d to output value %d \n", PUMA6_LED_GPIO_MOCA,  HW_VALUE_LED_OFF);
        GPIO_INFO_OUTPUT("set LED LINE3 gpio_pin=%d to output value %d \n",PUMA6_LED_GPIO_LINE3, HW_VALUE_LED_OFF);
        GPIO_INFO_OUTPUT("set LED LINE4 gpio_pin=%d to output value %d \n",PUMA6_LED_GPIO_LINE4, HW_VALUE_LED_OFF);
    }    
#endif // ARRIS ADD
    
    /* Init other GPIOs */
    /* ARRIS ADD */
#if defined(CONFIG_SYSTEM_MOCA)
    // only configure this as output on 16xx.  2402 uses this pin for cablecard 
        puma6_gpioCtrl(  PUMA6_LED_GPIO_MOCA,  GPIO_PIN, GPIO_OUTPUT_PIN);
        puma6_gpioOutBit(PUMA6_LED_GPIO_MOCA,  HW_VALUE_LED_OFF);
        GPIO_INFO_OUTPUT("set LED MOCA gpio_pin=%d to output value %d \n", PUMA6_LED_GPIO_MOCA,  HW_VALUE_LED_OFF);
#endif
    //To support boot up from battery, the PUMA6_BATCHG_RESET_N must be set high before set gpio output. 
    puma6_gpioOutBit(PUMA6_BATCHG_RESET_N, 1); 
    puma6_gpioCtrl(  PUMA6_BATCHG_RESET_N,  GPIO_PIN, GPIO_OUTPUT_PIN);
    
    puma6_gpioCtrl(PUMA6_TUNER_RESET_GPIO,  GPIO_PIN, GPIO_OUTPUT_PIN); /* Set Tuner Reset GPIO */
    puma6_gpioCtrl(PUMA6_DAC_PSM_GPIO,  GPIO_PIN, GPIO_OUTPUT_PIN); 
    puma6_gpioCtrl(PUMA6_MOCA20_RESET_N, GPIO_PIN, GPIO_OUTPUT_PIN);
    puma6_gpioCtrl(PUMA6_MOCA_MDIO, GPIO_PIN, GPIO_OUTPUT_PIN);
    puma6_gpioCtrl(PUMA6_MOCA_MDC, GPIO_PIN, GPIO_OUTPUT_PIN);
    puma6_gpioCtrl(PUMA6_FWDNLD_N, GPIO_PIN, GPIO_OUTPUT_PIN);
    puma6_gpioCtrl(PUMA6_BATSN, GPIO_PIN, GPIO_OUTPUT_PIN);
    puma6_gpioCtrl(PUMA6_DECT_RESET_N, GPIO_PIN, GPIO_OUTPUT_PIN);
    puma6_gpioCtrl(PUMA6_DECT_INT_N, GPIO_PIN, GPIO_OUTPUT_PIN);
    puma6_gpioOutBit(PUMA6_TUNER_RESET_GPIO,    0); /* reset tuner */
    puma6_gpioOutBit(PUMA6_DAC_PSM_GPIO,        0); /* allow Tx */
    puma6_gpioOutBit(PUMA6_MOCA20_RESET_N, 1); /* switch out of reset */
    puma6_gpioOutBit(PUMA6_MOCA_MDIO, 1); 
    puma6_gpioOutBit(PUMA6_MOCA_MDC, 1); 
    puma6_gpioOutBit(PUMA6_FWDNLD_N, 1); 
    puma6_gpioOutBit(PUMA6_BATSN, 1); 
    puma6_gpioOutBit(PUMA6_DECT_RESET_N, 0); /* reset DECT */ 
    puma6_gpioOutBit(PUMA6_DECT_INT_N, 1); 

    GPIO_INFO_OUTPUT("set TUNER reset gpio_pin=%d to output value %d \n",PUMA6_TUNER_RESET_GPIO, 0);
	GPIO_INFO_OUTPUT("set DAC_PSM gpio_pin=%d to output value %d \n",PUMA6_DAC_PSM_GPIO, 1);
    GPIO_INFO_OUTPUT("set MOCA20 reset gpio_pin=%d to output value %d \n",PUMA6_MOCA20_RESET_N, 1);
    GPIO_INFO_OUTPUT("set MOCA MDIO gpio_pin=%d to output value %d \n",PUMA6_MOCA_MDIO, 1);
    GPIO_INFO_OUTPUT("set MOCA MDC gpio_pin=%d to output value %d \n",PUMA6_MOCA_MDC, 1);
    GPIO_INFO_OUTPUT("set FWDNLD gpio_pin=%d to output value %d \n",PUMA6_FWDNLD_N, 1);
    GPIO_INFO_OUTPUT("set BATCHG reset gpio_pin=%d to output value %d \n",PUMA6_BATCHG_RESET_N, 1);
    GPIO_INFO_OUTPUT("set BATSN gpio_pin=%d to output value %d \n",PUMA6_BATSN, 1);
    GPIO_INFO_OUTPUT("set DECT reset gpio_pin=%d to output value %d \n",PUMA6_DECT_RESET_N, 0);
    GPIO_INFO_OUTPUT("set DECT interrupt gpio_pin=%d to output value %d \n",PUMA6_DECT_INT_N, 1);
    /* END ARRIS ADD */

    /* Init ATOM GPIOs */
    //puma6_gpioCtrl(99,  GPIO_PIN, GPIO_OUTPUT_PIN); 
    //puma6_gpioOutBit(99,0); /* GPIO 99 will output '0' */
    //GPIO_INFO_OUTPUT("Setting GPIO 99 to output 0\n");

    //puma6_gpioCtrl(100,  GPIO_PIN, GPIO_OUTPUT_PIN); 
    //puma6_gpioOutBit(100,1); /* GPIO 99 will output '1' */
    //GPIO_INFO_OUTPUT("Setting GPIO 100 to output 1\n");

    /* Init ATOM Interrupt GPIOs */
    //PUMA6_GPIO_REG_SET( 0x50 /* GPIO_INT_EN_2 */, 0x01600000 ); /* set GPIO 85, 86 and 88 to interrupt enable */
    //GPIO_INFO_OUTPUT("Setting GPIO Reg 0x50 to 0x01600000 - interrupt enable to GPIO 85 86 88 \n");
    //PUMA6_GPIO_REG_SET( 0x58 /* GPIO_in_MODE_RF_2 */, 0x01600000 ); /* set GPIO 85, 86 and 88 to interrupt low voltage interrupt active */
    //GPIO_INFO_OUTPUT("Setting GPIO Reg 0x58 to 0x01600000 - low voltage interrupt active to GPIO 85 86 88 \n");

    /* Update the ARM11 MBOX that the ATOM and ARM GPIO init is done. */
    PUMA6_ARM11_MBOX_UPDATE_EVENT(ARM11_EVENT_GPIO_INIT_EXIT);

    return(0);
}

/*! \fn int puma6_gpioCtrl(unsigned int gpio_pin,
                    PumaGpioTypes_t pin_mode,
                    PumaGpioTypes_t pin_direction)
    \brief This API initializes the mode of a specific GPIO pin
    \param gpio_pin GPIO pin number (auxiliary GPIOs start after primary GPIOs)
    \param pin_mode GPIO pin mode (GPIO_PIN, FUNCTIONAL_PIN)
    \param pin_direction Direction of the gpio pin in GPIO_PIN mode
    \return Returns (0) on success and (-1) on failure
*/
int puma6_gpioCtrl(unsigned int gpio_pin, PumaGpioTypes_t pin_mode, PumaGpioTypes_t pin_direction)
{
    unsigned int gpio_out_en_reg;

    GPIO_DEBUG_OUTPUT("gpio_pin=%d pin_mode=%d pin_direction=%d\n",gpio_pin,pin_mode,pin_direction);

    if ( gpio_pin >= PUMA6_MAX_GPIOS )
    {
        return(-1);
    }

    gpio_out_en_reg = PUMA6_GPIO_REG_ADDR(gpio_pin, PUMA6_GPIO_OUT_EN_REG);

    GPIO_DEBUG_OUTPUT("gpio_out_en_reg = 0x%x gpio_out_en_reg val before = 0x%x \n",gpio_out_en_reg,(PUMA6_GPIO_REG_GET(gpio_out_en_reg)));

    if (hw_mutex_lock(HW_MUTEX_GPIO) == 0)
    {
        printf("Error: GPIO fail to lock hw-mutex.\n");
        return(-1);
    }

    if (( pin_mode == GPIO_PIN) && (pin_direction == GPIO_OUTPUT_PIN) )
    {
        PUMA6_GPIO_REG_SET(gpio_out_en_reg, (PUMA6_GPIO_REG_GET(gpio_out_en_reg) | ENDIAN_SWAP32(BIT(gpio_pin%32))));
    }
    else /* FUNCTIONAL or GPIO_INPUT_PIN */
    {
        PUMA6_GPIO_REG_SET(gpio_out_en_reg, (PUMA6_GPIO_REG_GET(gpio_out_en_reg) & ENDIAN_SWAP32(~BIT(gpio_pin%32))));
    }

    hw_mutex_unlock(HW_MUTEX_GPIO);
    GPIO_DEBUG_OUTPUT("gpio_out_en_reg = 0x%x gpio_out_en_reg val after = 0x%x \n",gpio_out_en_reg, (PUMA6_GPIO_REG_GET(gpio_out_en_reg)));

    return(0);
}


/*! \fn int puma6_gpioOutBit(unsigned int gpio_pin, unsigned int value) 
    \brief This API outputs the specified value on the gpio pin
    \param gpio_pin GPIO pin number (auxiliary GPIOs start after primary GPIOs)
    \param value 0/1 (TRUE/FALSE)
    \return Returns (0) on success and (-1) on failure
*/
int puma6_gpioOutBit(unsigned int gpio_pin, unsigned int value)
{
    unsigned int gpio_out_reg;

    GPIO_DEBUG_OUTPUT("gpio_pin=%d value=%d \n",gpio_pin,value);

    if ( gpio_pin >= PUMA6_MAX_GPIOS )
    {
        return(-1);
    }

    if ( value ) /* SET Reg */
    {
        gpio_out_reg = PUMA6_GPIO_REG_ADDR(gpio_pin, PUMA6_GPIO_SET_REG);
    }
    else /* CLEAR Reg */
    {
        gpio_out_reg = PUMA6_GPIO_REG_ADDR(gpio_pin, PUMA6_GPIO_CLEAR_REG);
    }

    GPIO_DEBUG_OUTPUT("gpio_out_reg = 0x%x \n",gpio_out_reg);

    PUMA6_GPIO_REG_SET(gpio_out_reg, ENDIAN_SWAP32(BIT(gpio_pin%32)));

    return(0);
}

/*! \fn int PAL_sysGpioInBit(unsigned int gpio_pin)
    \brief This API reads the specified gpio_pin and returns the value
    \param gpio_pin GPIO pin number (auxiliary GPIOs start after primary GPIOs)
    \return Returns gpio status 0/1 (TRUE/FALSE), on failure returns (-1)
*/
int puma6_gpioInBit(unsigned int gpio_pin)
{
    unsigned int gpio_in_reg;
    unsigned int ret_val = 0;

    GPIO_DEBUG_OUTPUT("gpio_pin=%d \n",gpio_pin);

    if ( gpio_pin >= PUMA6_MAX_GPIOS )
    {
        return(-1);
    }

    gpio_in_reg = PUMA6_GPIO_REG_ADDR(gpio_pin,PUMA6_GPIO_INPUT_REG);

    GPIO_DEBUG_OUTPUT("gpio_in_reg = 0x%x  gpio_in_reg val = 0x%x \n",gpio_in_reg, PUMA6_GPIO_REG_GET(gpio_in_reg));

    if ( ((PUMA6_GPIO_REG_GET(gpio_in_reg)) & (ENDIAN_SWAP32(BIT(gpio_pin%32)))) )
    {
        ret_val = 1;
    }

    return(ret_val);
}

#ifdef GPIO_LED_CMD
int do_led_test (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
    int i=0;
    int led_on_off = -1;
    int led_all = 1;
    int led_pin[20] = {0};

    /* Parse Command Line   */
    /* -------------------- */
    for (i=1;i<argc-1;i++)
    {
        led_pin[i] = (int)simple_strtoul(argv[i], NULL, 10);
        if (led_pin[i] == 0)
        {
             printf("Error: illigal LED number\n");
             return 1;
        }
        if (led_pin[i] > 13)
        {
             printf("Error: illigal LED number\n");
             return 1;
        }

        led_all = 0;
    }

    if (strcmp(argv[i], "on") == 0)
    {
        led_on_off = HW_VALUE_LED_ON;
        
    }

    if (strcmp(argv[i], "off") == 0)
    {
        led_on_off = HW_VALUE_LED_OFF;
        
    }

    if (led_on_off == -1)
    {
        printf("Error: must specify 'on' or 'off'\n");
        return 1;
    }

    if (led_all == 1)
    {
        printf("Set All LEDs to %s\n",(led_on_off == HW_VALUE_LED_OFF)?"OFF":"ON");
// ARRIS REMOVE        puma6_gpioOutBit(PUMA6_LED_GPIO_US,   led_on_off);
        puma6_gpioOutBit(PUMA6_LED_GPIO_LINE2,led_on_off);
// ARRIS REMOVE        puma6_gpioOutBit(PUMA6_LED_GPIO_LINE4,led_on_off);
// ARRIS REMOVE        puma6_gpioOutBit(PUMA6_LED_GPIO_LINE3,led_on_off);
        puma6_gpioOutBit(PUMA6_LED_GPIO_LINE1,led_on_off);
        puma6_gpioOutBit(gpio_ds,             led_on_off);
        puma6_gpioOutBit(gpio_online,         led_on_off);
// ARRIS REMOVE        puma6_gpioOutBit(gpio_link,           led_on_off);
        puma6_gpioOutBit(PUMA6_LED_GPIO_BATTERY,led_on_off);
        puma6_gpioOutBit(PUMA6_LED_GPIO_MOCA, led_on_off);
        
        return 0;
    }

    for (i=0;i<20;i++)
    {
        switch(led_pin[i])
        {
        case 0:
            break;
        case 1:
            puma6_gpioOutBit(gpio_power,led_on_off);
            break;
        case 2:
            puma6_gpioOutBit(gpio_ds,led_on_off);
            break;
        case 3:
// ARRIS REMOVE            puma6_gpioOutBit(PUMA6_LED_GPIO_US,led_on_off);
            break;
        case 4:
            puma6_gpioOutBit(gpio_online,led_on_off);
            break;
        case 5:
// ARRIS REMOVE            puma6_gpioOutBit(gpio_link,led_on_off);
            break;
        case 6:
            puma6_gpioOutBit(PUMA6_LED_GPIO_LINE1,led_on_off);
            break;
        case 7:
            puma6_gpioOutBit(PUMA6_LED_GPIO_LINE2,led_on_off);
            break;
        case 8:
// ARRIS REMOVE            puma6_gpioOutBit(PUMA6_LED_GPIO_LINE3,led_on_off);
            break;
        case 9:
// ARRIS REMOVE            puma6_gpioOutBit(PUMA6_LED_GPIO_LINE4,led_on_off);
            break;
        case 10:
            puma6_gpioOutBit(PUMA6_LED_GPIO_BATTERY,led_on_off);
            break;
        case 11:
            puma6_gpioOutBit(PUMA6_LED_GPIO_MOCA, led_on_off);
            break;
        default:
            printf("Error: LED %d, is not defined.\n",led_pin[i]);

        }
    }

    return 0;
}

U_BOOT_CMD(
    led, CFG_MAXARGS, 0, do_led_test,
    "led\t - Set On/Off all LEDs\n",
    " - No help available.\n"
);
#endif

