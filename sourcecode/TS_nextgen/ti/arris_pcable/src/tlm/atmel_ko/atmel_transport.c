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
 * FILE PURPOSE:     - definitions for the atmel download.
 ******************************************************************************
 * FILE NAME:     atmel_transport.c
 *
 * DESCRIPTION:   Functions that handle the transport layer interface for the 
 *                atmel firmware downloader.
 *                
 *
 *                Author: Matt Snoby, ARRIS Group, Inc.
 *
 *******************************************************************************/
#include "stdio.h"
#include "atmel_phy.h"
#include "atmel_datalink.h"



/**************************************************************************/
/*! \fn  char atmel_transport_generate_even_parity_bit( unsigned char value )
 **************************************************************************
 *  \brief The byte to be transmitted is examined and an even parity bit is generated.
 *  \param[in] value to be transmitted.
 *  \return [out] 0 or 1 which will be the even parity bit.
 *  \note:  
 **************************************************************************/
char atmel_transport_generate_even_parity_bit( unsigned char value )
{
    unsigned int v = value;       // word value to compute the parity of
	bool parity = false;          // parity will be the parity of v

	while (v)
	{
		parity = !parity;
		v = v & (v - 1);
	}
	return parity;
}

/**************************************************************************/
/*! \fn  unsigned int atmel_transport_generate_frame( unsigned char data )
 **************************************************************************
 *  \brief Generates the 12 bit frame  to be sent to the atmel interface..
 *  \param[in] value to be transmitted.
 *  \return [out] 12 bit frame.
 *  \note: 1 start bit, data, parity bit, and two stop bits. 
 **************************************************************************/
unsigned int atmel_transport_generate_frame( unsigned char data )
{
    unsigned int frame =0;
    unsigned int temp =0;
    const char stopbits=6; // 11x binary  //111x
    char EvenParityBit =0;

    // calculate the parity bit
    EvenParityBit = atmel_transport_generate_even_parity_bit(data);
    frame = stopbits | EvenParityBit;

    /*
    stop 2  = 1
    stop 1  = 1
    Parity  = x
    bit 7    msb
    ...
    bit 0    lsb
    start   =0
    */

    /* now shift the stop bits to the 12th location.*/
    frame = frame << 9;

    /* shift the data we want sent 1 to the left to make room for the start bit.*/
    temp = data << 1; 

    /* now insert the shifted data and start bits into the frame*/
    frame = frame | temp; 

    return frame;
}

/**************************************************************************/
/*! \fn  void atmel_transport_tx_byte(unsigned int byte)
 **************************************************************************
 *  \brief Transmits a byte to the atmel nvm controller
 *  \param[in] byte to be transmitted.
 *  \return [out] none
 *  \note:  
 **************************************************************************/
void atmel_transport_tx_byte(unsigned int byte)
{
    int count =0;
    char bit =0;
    unsigned int frame;


    frame = atmel_transport_generate_frame(byte);

    /* shift and send data*/
    for (count =0; count<12;count++)
    {
        bit = (frame >> count) & 0x01;
        atmel_datalink_set_next_tx_bit(bit);
        atmel_datalink_clk_and_send_data();
    }
}

/**************************************************************************/
/*! \fn  void atmel_transport_idle_clock(void)
 **************************************************************************
 *  \brief in between data and reading, we need to send idle bits on the wire.
 *  \param[in] None
 *  \return [out] none
 *  \note:  
 **************************************************************************/
void atmel_transport_idle_clock(void)
{
    switch (PDI_GET_DATA_MODE())
    {
        case DP_READMODE:
            atmel_datalink_clk_idle();
            break;
        case DP_WRITEMODE:
            atmel_datalink_set_next_tx_bit(1); // idle
            atmel_datalink_clk_and_send_data();
            break;
    }

}


/**************************************************************************/
/*! \fn  char atmel_transport_read_byte(void)
 **************************************************************************
 *  \brief This will read a byte from the atmel interface
 *  \param[in] None
 *  \return [out] byte that was read.
 *  \note:  
 **************************************************************************/
char atmel_transport_read_byte(void)
{
    unsigned char value=0;

    if (DP_READMODE != PDI_GET_DATA_MODE())
    {
        atmel_datalink_change_mode(DP_READMODE);
    }

    while(1)
    {
        if (atmel_datalink_receive_state() == RX_COMPLETE)
        {
            break;
        }
        atmel_transport_idle_clock();
    }

    /* now gReceiveFrame Holds the frame  // remove the stop bits and parity bits and return*/

    value = gReceiveFrame & 0xFF;

    // reset our variable to wait for the next byte on the wire.
    atmel_datalink_set_receive_state(RX_WAITING_FOR_START_BIT);
     
    return value;

}

