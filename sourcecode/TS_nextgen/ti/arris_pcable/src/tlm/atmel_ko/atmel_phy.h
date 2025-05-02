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
 * FILE NAME:     atmel_phy.h
 *
 * DESCRIPTION:   Functions that handle the physical interface for the 
 *                atmel firmware downloader.
 *                
 *
 *                Author: Matt Snoby, ARRIS Group, Inc.
 *
 *******************************************************************************/

#ifndef ATMEL_PHY_H
#define ATMEL_PHY_H


#ifdef __cplusplus
extern "C" {
#endif


typedef enum
{
    DP_READMODE,    /* THE PDI DATA line is in read mode*/
    DP_WRITEMODE    /* THE PDI DATA line is in write or tx mode*/
}tDataModeState;

typedef enum
{
    CLK_LOW,      /* THE Clock line is low*/
    CLK_HIGH      /* THE Clock line is high*/
}tClkSignalState;


/* Defines*/

#define PHY_PROP_DELAY_USEC (0)

#ifndef bool
    #define bool char
#endif

#ifndef false
    #define false 0
#endif

#ifndef true
    #define true 1
#endif


/* function prototypes.*/
char PDI_DATA_READ_BIT(void);
void PDI_SET_DATA_MODE(tDataModeState state);
tDataModeState PDI_GET_DATA_MODE(void);
unsigned int PDI_sysGpioControlPUPD(unsigned int bEnable, unsigned int bitMask);
void PDI_CLK_HI (void);
void PDI_CLK_LOW (void);
void PDI_DATA_TX (char bit);


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif




