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
 * FILE PURPOSE:     - Hold the clock portion / datalink portion of the atmel downloader.
 ******************************************************************************
 * FILE NAME:     atmel_datalink.c
 *
 * DESCRIPTION:   Holds the clocking and direction changing logic of the 
 *                atmel downloader.
 *                
 *
 *                Author: Matt Snoby, ARRIS Group, Inc.
 *
 *******************************************************************************/
#include <linux/kernel.h>	/* printk() */
#include <linux/slab.h>		/* kmalloc() */
#include <linux/fs.h>		/* everything... */
#include <linux/delay.h>

#include "atmel_phy.h"
#include "atmel_datalink.h"

/* private variables*/
static tTxDataSt gTxBitState = TX_IDLE;
static tRxDataSt gRxState =  RX_WAITING_FOR_START_BIT;
static char gTxBit=0;
static tClkSignalState curClockState = CLK_LOW;

/* global data*/
unsigned int gReceiveFrame =0; // this is the variable that will contain a completed frame.




char atmel_datalink_get_next_tx_bit(void)
{
    return gTxBit;
}

/**************************************************************************/
/*! \fn void atmel_datalink_set_next_tx_bit(char bit)
 **************************************************************************
 *  \brief Sets the next bit for transmit, must be injected in the correct order of clocking.
 *  \param[in] bit : 0 or 1 bit
 *  \return 0
 *  \note: You must call this function at the correct time! 
 **************************************************************************/

void atmel_datalink_set_next_tx_bit(char bit)
{

    if ((gTxBitState != TX_IDLE) )
    {
        if (gTxBitState != TX_COMPLETE)
        {
            /* this could be an error condition*/
        }
    }
 
    gTxBit = bit;
    gTxBitState = TX_PENDING;

}

/* we are in write mode and transitioning from low to high*/

/**************************************************************************/
/*! \fn void handle_write_mode_trans_tohigh (void)
 **************************************************************************
 *  \brief when in write mode handles the clock transition from low to high..
 *  \param[in] none
 *  \return 0
 *  \note:  
 **************************************************************************/
void handle_write_mode_trans_tohigh (void)
{
    if (gTxBitState == TX_DATA_LATCHED)
    {
        /* The data has been latched in and we have done a transition to high*/
        /* we can now consider the data transmitted                         */
        gTxBitState = TX_COMPLETE;
    }
}

/**************************************************************************/
/*! \fn void handle_write_mode_trans_tolow (void)
 **************************************************************************
 *  \brief when in write mode handles the clock transition from high to low
 *  \param[in] none
 *  \return 0
 *  \note: The clock is transitioning from high to low we now need to latch data 
 **************************************************************************/
void handle_write_mode_trans_tolow(void)
{
    char bit=0;
    bit =atmel_datalink_get_next_tx_bit() ;
    if (gTxBitState == TX_COMPLETE)
    {
		return;
    }
    if (gTxBitState != TX_PENDING)
    {
        return;
    }
    
    
    PDI_DATA_TX(bit);
    gTxBitState = TX_DATA_LATCHED;


}
/**************************************************************************/
/*! \fn void handle_read_mode_trans_high (void)
 **************************************************************************
 *  \brief when in reade mode handles the clock transition from low to high..
 *  \param[in] none
 *  \return 0
 *  \note:  This is where we sample the data.
 **************************************************************************/
void handle_read_mode_trans_high (void)
{
    static unsigned int bitCount=0;
    static unsigned int CurrentFrame=0;
    static bool bCollectingFrame = false;
    unsigned int bit=0;

    bit = PDI_DATA_READ_BIT();
    if (bCollectingFrame == false)
    {
        /* we are waiting on the startbit*/
        if (bit == 0)     /* the start bit is low*/
        {
            /* we can now start collecting bits for the frame*/
          //  printf("Found start bit\n");
            bCollectingFrame = true;
            gRxState =  RX_COLLECTING_BYTE;

            return;
        }
        else
        {
            return;  // just an idle bit
        }
    }
    /* 
        We are currently generating a frame that comes in LSB first. - we need to put that into
        MSB first.
    */

    bit = bit << bitCount;  // bitCount does two things, 1.) tells us what bit we are collecting
                            // and allows us to know how far to shift the bit 

    CurrentFrame = CurrentFrame | bit; // we are changing the frame from LSB to MSB first.  
    bitCount++;
    /* the complete frame size is 12 bits, but we threw away the start bit*/
    if (bitCount == 11)
    {
        /* this should be a complete frame with the two stop bits being the MSB*/
        if ((CurrentFrame & 0x600) == 0x600)
        {
            /* place this frame in the global variable as complete*/
			gReceiveFrame = CurrentFrame;

            /* reset flags for next frame*/
            bitCount=0;
            bCollectingFrame = false;
            gRxState = RX_COMPLETE ;

            CurrentFrame =0;
            /* maybe notify that a frame is ready?*/
            return;
        }

        /* something has gone wrong here.*/
        bitCount=0;
        bCollectingFrame = false;
        CurrentFrame =0;
    }
}



/**************************************************************************/
/*! \fn void handle_clk_lowToHigh_trans (void)
 **************************************************************************
 *  \brief Handles the data line when the clk transitions from low to high
 *  \param[in] none
 *  \return 0
 *  \note:  This is where we sample the data.
 **************************************************************************/
void handle_clk_lowToHigh_trans(void)
{
    if (PDI_GET_DATA_MODE() == DP_WRITEMODE )
    {
      handle_write_mode_trans_tohigh();
    }
    else
    {
        handle_read_mode_trans_high();
    }

}
/**************************************************************************/
/*! \fn void handle_clk_HighToLow_trans (void)
 **************************************************************************
 *  \brief Handles the data line when the clk transitions from high to low
 *  \param[in] none
 *  \return 0
 *  \note:  This is where we sample the data.
 **************************************************************************/
void handle_clk_HighToLow_trans(void)
{
    if (PDI_GET_DATA_MODE() == DP_WRITEMODE )
    {
      handle_write_mode_trans_tolow();
    }
    else
    {
		// this is where the sender will latch data in, nothing to do here.
    }


}
/* base line the clock and data lines*/
void atmel_datalink_init(void)
{
    curClockState = CLK_LOW;
    PDI_CLK_LOW();
    PDI_DATA_TX (0);
}

/**************************************************************************/
/*! \fn void atmel_datalink_clk (void)
 **************************************************************************
 *  \brief This is the heart of the system.  This clocks the clock line  and read / writes data.
 *  \param[in] none
 *  \return 0
 *  \note:  This handles the main clock and will read and write bits.
 **************************************************************************/
void atmel_datalink_clk(void)
{
    

    if (curClockState == CLK_LOW)
    {

        handle_clk_lowToHigh_trans();
        PDI_CLK_HI();
        curClockState = CLK_HIGH;
    }
    else
    {

        handle_clk_HighToLow_trans();
        PDI_CLK_LOW();
        curClockState = CLK_LOW;
    }
   udelay(USEC_DELAY_FOR_CLK);
    
}

/* This will do a complete clock cycle*/
void atmel_datalink_clk_idle(void)
{
    atmel_datalink_clk();
    atmel_datalink_clk();
}

/**************************************************************************/
/*! \fn void atmel_datalink_clk_and_send_data (void)
 **************************************************************************
 *  \brief after tx data has been submitted this gets called to send the data out.
 *  \param[in] none
 *  \return 0
 *  \note:  .
 **************************************************************************/
void atmel_datalink_clk_and_send_data(void)
{
    /* 
        slightly complex here we have to clock this bit out which means we have to make
        sure that the data has latched on a high to low transition and then a complete cycle 
        has take place.
    */
    while (gTxBitState != TX_COMPLETE)
    {
        atmel_datalink_clk();
    }
}

/**************************************************************************/
/*! \fn void atmel_datalink_change_mode (void)
 **************************************************************************
 *  \brief Changes between read and write states. Also handles setting up the clock
 *  \param[in] none
 *  \return 0
 *  \note:  
 **************************************************************************/
void atmel_datalink_change_mode(tDataModeState state)
{
    PDI_SET_DATA_MODE(state);

    if (DP_WRITEMODE == state)
    {
        /* need to change to write mode and put idle bits on the line
           idle bits can only be clocked in on a transition from high to low 
        */ 
        gTxBitState = TX_IDLE;
        atmel_datalink_set_next_tx_bit(1); // set the idle bit
        atmel_datalink_clk_and_send_data();// clock the data.
    }
    else
    {
        gRxState =  RX_WAITING_FOR_START_BIT;
        gReceiveFrame =0;
    }

}

/* returns what state the rx condition is in.*/
tRxDataSt atmel_datalink_receive_state(void)
{
    return gRxState;
}
/* Changes the receive state.  This is critical for receiving a frame.*/
void  atmel_datalink_set_receive_state(tRxDataSt rxState)
{
    gRxState = rxState;
   
}



