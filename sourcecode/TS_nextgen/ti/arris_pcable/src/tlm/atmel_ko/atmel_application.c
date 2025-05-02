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
 * FILE PURPOSE:     - High level access to the atmel nvm part.
 ******************************************************************************
 * FILE NAME:     atmel_application.c
 *
 * DESCRIPTION:   High level abstraction to plug into the xnvm.c reference code from atmel.
 *                
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

#include "atmel_phy.h"
#include "atmel_datalink.h"
#include "atmel_transport.h"
#include "atmel_phy.h"
#include "tlm_arris.h"




/* set the GPIO lines all to low*/
void reset_gpio_lines(void)
{
    PAL_sysGpioCtrl (CHARGER_RESET, GPIO_PIN, GPIO_OUTPUT_PIN);

    atmel_datalink_change_mode(DP_WRITEMODE);

    atmel_datalink_init();

    PDI_DATA_TX (0);
    PDI_CLK_LOW ();
    udelay(20);

}

/* initialize the interface so that the atmel chip knows to go into PDI mode.*/
/**************************************************************************/
/*! \fn  void hal_pdi_init(void)
 **************************************************************************
 *  \brief This function will hold the dta line high for 110 us then clock in to 
    \brief place the atmel part into PDI mode.
 *  \param[in] None
 *  \return [out] none
 *  \note:  
 **************************************************************************/
void hal_pdi_init(void)
{
    int count=0;

    PDI_DATA_TX (1);
// Changing the delay from 110 us to 50 us.
// Atmel specification document mentions that PDI_CLK cycle must start no later than 100 us after taking the PDI_DATA line to high.
// With the rev E part this was causing itermittent failure and puma was waiting forever for the nvm busy bit to be set untill the watchdog timer
// comes in and reset the unit.
// After long discussion with Atmel we have choosen 50 us delay.
    udelay(50);
    for (count=0; count< 40; count++)
    {
        /* this will keep the Data line high for 60 clocks or 120 transitions.*/
        atmel_datalink_clk_idle();
    }

}

/**************************************************************************/
/*! \fn  void hal_pdi_releaseprocessor(void)
 **************************************************************************
 *  \brief releases the atmel part to start to run.
 *  \param[in] None
 *  \return [out] none
 *  \note:  
 **************************************************************************/
void hal_pdi_releaseprocessor(void)
{
     /* set the GPIO lines to HIGH so that the processor will start running*/

    PAL_sysGpioCtrl (CHARGER_DNLD, GPIO_PIN, GPIO_OUTPUT_PIN);
    PAL_sysGpioCtrl (CHARGER_RESET, GPIO_PIN, GPIO_OUTPUT_PIN);
    PAL_sysGpioOutBit (CHARGER_DNLD, 1);

    PAL_sysGpioOutBit (CHARGER_RESET, 0);
    msleep(10);
    PAL_sysGpioOutBit (CHARGER_RESET, 1);

}

void hal_pdi_deinit( void )
{

}

/* basically just sends idle bits so that the interface stays active.*/
void hal_pdi_idle(void)
{
    int i;
    if (DP_WRITEMODE != PDI_GET_DATA_MODE())
    {
        atmel_datalink_change_mode(DP_WRITEMODE);

        for (i=0;i<128; i++)
        {
            atmel_datalink_set_next_tx_bit(1); // set the idle bit
            atmel_datalink_clk_and_send_data();// clock the data.
        }

    }

}


/**************************************************************************/
/*! \fn  bool hal_pdi_write( const uint8_t *data, uint16_t length )
 **************************************************************************
 *  \brief implements the write command for the atmel reference code.
 *  \param[in] *data - pointer to buffer to write
 *  \param [in] length - length of the buffer to transmit
 *  \return [out] bool - success or failure.
 *  \note:  
 **************************************************************************/
bool hal_pdi_write( const uint8_t *data, uint16_t length )
{
    int count;
    int i =0;
    

    if (DP_WRITEMODE != PDI_GET_DATA_MODE())
    {
        atmel_datalink_change_mode(DP_WRITEMODE);

        for (i=0;i<128; i++)
        {
            atmel_datalink_set_next_tx_bit(1); // set the idle bit
            atmel_datalink_clk_and_send_data();// clock the data.
        }

    }

    for (count=0; count< length; count++)
    {

        atmel_transport_tx_byte(*data);
        data++;

        /* sending idle bits between each byte sent.*/
        for (i=0;i<12; i++)
        {
            atmel_datalink_set_next_tx_bit(1); // set the idle bit
            atmel_datalink_clk_and_send_data();// clock the data.
        }
    }

    return true;
}
/**************************************************************************/
/*! \fn  uint16_t hal_pdi_read( uint8_t *data, uint16_t length )
 **************************************************************************
 *  \brief implements the read command necessary for the atmel reference code.
 *  \param[in] *data - pointer to buffer to hold the read data
 *  \param [in] length - length of the buffer
 *  \return [out] bool - success or failure.
 *  \note:  
 **************************************************************************/
uint16_t hal_pdi_read( uint8_t *data, uint16_t length )
{

    int count;
    int origLeng = length;
    memset(data, 0, length);

    for (count=0; count<length; count++)
    {
        *data = atmel_transport_read_byte();
         data++;
    }

    if (count == 0)
    {
        printk(" count = 0, original length =%d\n", origLeng);
    }
    return count;

}



