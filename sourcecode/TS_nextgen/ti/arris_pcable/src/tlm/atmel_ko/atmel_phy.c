/*

Copyright (c) 2010-2016 ARRIS Enterprises, LLC

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
 * FILE PURPOSE:     - Handle physical layer access of the atmel downloader
 ******************************************************************************
 * FILE NAME:     atmel_phy.c
 *
 * DESCRIPTION:   Functions that handle the physical interface for the 
 *                atmel firmware downloader.
 *                
 *
 *                Author: Matt Snoby, ARRIS Group, Inc.
 *
 *******************************************************************************/

#include <linux/kernel.h>	/* printk() */
#include <linux/slab.h>		/* kmalloc() */
#include <linux/fs.h>		/* everything... */
#include <asm-arm/arch-avalanche/generic/pal.h>
#include <linux/delay.h>
#include "xnvm.h"
#include "atmel_phy.h"
#include "tlm_arris.h"


 /* private data */
static tDataModeState gDataMode = DP_WRITEMODE;





void PDI_CLK_HI (void)
{
    /* verified*/
    
    if (0!= PAL_sysGpioOutBit (CHARGER_RESET, 1))
    {
        printk("Error output %d\n", __LINE__);
    }
    udelay(PHY_PROP_DELAY_USEC);
}

void PDI_CLK_LOW (void)
{
     /* verified*/
  if (0!= PAL_sysGpioOutBit (CHARGER_RESET, 0))
  {
      printk("Error output %d\n", __LINE__);
  }

  udelay(PHY_PROP_DELAY_USEC);
}

/**************************************************************************/
/*! \fn void PDI_DATA_TX (char bit)
 **************************************************************************
 *  \brief Transmits the physical bit 0 or 1.
 *  \param[in] bit : send the physical bit zero or one
 *  \return 0
 *  \note: You can not just call this function, the clocking has to take place.
 **************************************************************************/
void PDI_DATA_TX (char bit)
{
    if (bit>1)
    {
        printk ("ERROR %s, %s\n",__FILE__,__FUNCTION__);
        return;
    }
    if (0!=  PAL_sysGpioOutBit (CHARGER_DNLD, bit))
    {
        printk("Error output %d\n", __LINE__);
    }
    udelay(PHY_PROP_DELAY_USEC);
}


/**************************************************************************/
/*! \fn char PDI_DATA_READ_BIT(void)
 **************************************************************************
 *  \brief Read a bit from the download pin.
 *  \param[in] none
 *  \return a 0 or a 1 depenant upon the signal level.
 *  \note: This function does not lock interrupts!!!!! 
 **************************************************************************/
char PDI_DATA_READ_BIT(void)
{
    char value;
    
    PAL_sysGpioCtrl (CHARGER_DNLD, GPIO_PIN, GPIO_INPUT_PIN);

    udelay(PHY_PROP_DELAY_USEC);
    value =  PAL_sysGpioInBit (CHARGER_DNLD);
    return value;
}


/**************************************************************************/
/*! \fn void PDI_SET_DATA_MODE(tDataModeState state)
 **************************************************************************
 *  \brief Controls the download pin to put it into input or output mode.
 *  \param[in] state : Place the download pin in input or output mode..
 *  \return 0
 *  \note: This function does not lock interrupts!!!!! 
 **************************************************************************/
void PDI_SET_DATA_MODE(tDataModeState state)
{
    static bool bcheck= DP_WRITEMODE;

    gDataMode = state;
    if (gDataMode == DP_READMODE)
    {
        // set to input
        PAL_sysGpioCtrl (CHARGER_DNLD, GPIO_PIN, GPIO_INPUT_PIN);
        bcheck =DP_READMODE; 
    }
    else
    {
        PAL_sysGpioCtrl (CHARGER_DNLD, GPIO_PIN, GPIO_OUTPUT_PIN);
    }
}

tDataModeState PDI_GET_DATA_MODE(void)
{
    return gDataMode ;
}

/**************************************************************************/
/*! \fn INT32 PDI_sysGpioControlPUPD(UINT32 bEnable, UINT32 bitMask)
 **************************************************************************
 *  \brief Control the internal pull up resistors in the PUMA.
 *  \param[in] bEnable : Enable or disable the pull up for the bitMask passed in.
 *  \param[in] bitMask : The specific bit that controls a specific pull up resistor
 *  \return 0
 *  \note: This function does not lock interrupts!!!!! 
 **************************************************************************/
unsigned int PDI_sysGpioControlPUPD(unsigned int bEnable, unsigned int bitMask)
{

#if defined (CONFIG_MACH_PUMA5)
    volatile UINT32 *pReg;
    UINT32 value =0;

    pReg = (UINT32*) (AVALANCHE_GPIO_PUPD_BANK_2);

    value = *pReg;


    if (bEnable == true)
    {
        value = value & (~bitMask);
    }
    else
    {
        value = value | bitMask;
    }

    *pReg = value;
#endif
    return 0;
}


