/*

Copyright (c) 2015-2016 ARRIS Enterprises, Inc.

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
 * FILE PURPOSE: Holds paths to the actual charger images.
 ******************************************************************************
 * FILE NAME:     battery.h
 *
 * DESCRIPTION:   Holds interface and path names for accessing the Atmel and
 *                TI charger devices.
 *                
 *******************************************************************************/

#ifndef BATTERY_H
#define BATTERY_H
#include <linux/ioctl.h>

#ifdef __cplusplus
extern "C" {
#endif


#ifndef bool
    #define bool int
#endif

#ifndef false
    #define false 0
#endif

#ifndef true
    #define true 1
#endif


#define ATMEL_APP_TM802G_FW "/lib/modules/2.6.39.3/drivers/tlm/TM802G_AP.bin"
#define ATMEL_APP_TM804G_FW "/lib/modules/2.6.39.3/drivers/tlm/TM804G_AP.bin"
#define ATMEL_APP_TM822G_FW "/lib/modules/2.6.39.3/drivers/tlm/TM822G_AP.bin"
#define ATMEL_APP_TG852G_FW "/lib/modules/2.6.39.3/drivers/tlm/TG852G_AP.bin"
#define ATMEL_APP_TG862G_FW "/lib/modules/2.6.39.3/drivers/tlm/TG862G_AP.bin"
#define ATMEL_APP_DTG872G_FW "/lib/modules/2.6.39.3/drivers/tlm/DTG872G_AP.bin"
#define ATMEL_APP_TM1602G_FW "/lib/modules/2.6.39.3/drivers/tlm/TM1602G_AP.bin"
#define ATMEL_APP_TG1642G_FW "/lib/modules/2.6.39.3/drivers/tlm/TG1642G_AP.bin"
#define ATMEL_APP_TG1662G_FW "/lib/modules/2.6.39.3/drivers/tlm/TG1662G_AP.bin"
#define ATMEL_APP_TG1672G_FW "/lib/modules/2.6.39.3/drivers/tlm/TG1672G_AP.bin"
#define ATMEL_APP_TG1682G_FW "/lib/modules/2.6.39.3/drivers/tlm/TG1682G_AP.bin"
#define ATMEL_APP_TG2472G_FW "/lib/modules/2.6.39.3/drivers/tlm/TG2472G_AP.bin"
#define ATMEL_APP_TG2492G_FW "/lib/modules/2.6.39.3/drivers/tlm/TG1692G_AP.bin"
#define ATMEL_APP_MG2402G_FW "/lib/modules/2.6.39.3/drivers/tlm/MG2402G_AP.bin"

#define ATMEL_FACTORY_TM802G_FW "/lib/modules/2.6.39.3/drivers/tlm/TM802G_ST.bin"
#define ATMEL_FACTORY_TM804G_FW "/lib/modules/2.6.39.3/drivers/tlm/TM804G_ST.bin"
#define ATMEL_FACTORY_TM822G_FW "/lib/modules/2.6.39.3/drivers/tlm/TM822G_ST.bin"
#define ATMEL_FACTORY_TG852G_FW "/lib/modules/2.6.39.3/drivers/tlm/TG852G_ST.bin"
#define ATMEL_FACTORY_TG862G_FW "/lib/modules/2.6.39.3/drivers/tlm/TG862G_ST.bin"
#define ATMEL_FACTORY_DTG872G_FW "/lib/modules/2.6.39.3/drivers/tlm/DTG872G_ST.bin"
#define ATMEL_FACTORY_TM1602G_FW "/lib/modules/2.6.39.3/drivers/tlm/TM1602G_ST.bin"
#define ATMEL_FACTORY_TG1642G_FW "/lib/modules/2.6.39.3/drivers/tlm/TG1642G_ST.bin"
#define ATMEL_FACTORY_TG1662G_FW "/lib/modules/2.6.39.3/drivers/tlm/TG1662G_ST.bin"
#define ATMEL_FACTORY_TG1672G_FW "/lib/modules/2.6.39.3/drivers/tlm/TG1672G_ST.bin"
#define ATMEL_FACTORY_TG1682G_FW "/lib/modules/2.6.39.3/drivers/tlm/TG1682G_ST.bin"
#define ATMEL_FACTORY_TG2472G_FW "/lib/modules/2.6.39.3/drivers/tlm/TG2472G_ST.bin"
#define ATMEL_FACTORY_TG2492G_FW "/lib/modules/2.6.39.3/drivers/tlm/TG1692G_ST.bin"
#define ATMEL_FACTORY_MG2402G_FW "/lib/modules/2.6.39.3/drivers/tlm/MG2402G_ST.bin"

#define LOCATION_OF_ATMEL_KERNEL_MODULE   "/lib/modules/2.6.39.3/drivers/tlm/atmel.ko"
#define LOCATION_OF_TIBAT_KERNEL_MODULE   "/lib/modules/2.6.39.3/drivers/tlm/tibat.ko"

/* TM3202G */
#define TIBAT_APP_TM3202G_FW "/lib/modules/2.6.39.3/drivers/tlm/TM3202G_AP_TI.bin"
#define TIBAT_FACTORY_TM3202G_FW "/lib/modules/2.6.39.3/drivers/tlm/TM3202G_ST_TI.bin"

/* TG1682 - cost reduction supports TI battery charger */
#define TIBAT_APP_TG1682G_FW "/lib/modules/2.6.39.3/drivers/tlm/TG1682G_AP_TI.bin"
#define TIBAT_FACTORY_TG1682G_FW "/lib/modules/2.6.39.3/drivers/tlm/TG1682G_ST_TI.bin"

/* TG1652  */
#define TIBAT_APP_TG1652G_FW "/lib/modules/2.6.39.3/drivers/tlm/TG1652G_AP_TI.bin"
#define TIBAT_FACTORY_TG1652G_FW "/lib/modules/2.6.39.3/drivers/tlm/TG1652G_ST_TI.bin"

#define ATMEL_APP_MAJOR_NUMBER            112
#define TI_APP_MAJOR_NUMBER               112

#define ATMEL_APP_DEVICE_NAME             "/dev/atmel"
#define TI_APP_DEVICE_NAME                "/dev/tibat"

/* CRC INFORMATION NEEDS TO BE UPDATED EVERY TIME A
   NEW LOAD IS PROVIDED BY HENRY SULLY */
/* TM3202G */
#define TIBAT_APP_3202G_REV0102AU_CRC 	  (0x8FBF)
#define TIBAT_ST_3202G_REV0003AU_CRC 	  (0xF5C6)
#define TIBAT_APP_CRC_3202G  	          TIBAT_APP_3202G_REV0102AU_CRC
#define TIBAT_ST_CRC_3202G                TIBAT_ST_3202G_REV0003AU_CRC

/* TG1682G */
#define TIBAT_APP_1682G_REV0102AV_CRC 	  (0xFB08)
#define TIBAT_ST_1682G_REV0003AV_CRC 	  (0xF49C)
#define TIBAT_APP_CRC_1682G               TIBAT_APP_1682G_REV0102AV_CRC
#define TIBAT_ST_CRC_1682G  		  TIBAT_ST_1682G_REV0003AV_CRC

/* TG1652G */
#define TIBAT_APP_1652G_REV0102AW_CRC     (0x80C6)
#define TIBAT_ST_1652G_REV0003AW_CRC      (0xBCFF)
#define TIBAT_APP_CRC_1652G               TIBAT_APP_1652G_REV0102AW_CRC
#define TIBAT_ST_CRC_1652G                TIBAT_ST_1652G_REV0003AW_CRC

#define ATMEL_SIZE_OF_FLASH 	          (0x5000)
#define TIBAT_SIZE_OF_FLASH               (0x4000)

#define LOCATION_OF_TI_APP_VERSION 	  (0xC4E0)

#define TIBAT_PAGE_SIZE 		  (1024)
#define ATMEL_PAGE_SIZE 		  (256)

#define BATTERY_APP_VERSION_LENGTH        (10)
#define TIBAT_SIZE_OF_LOAD 	          (15360)

#define LOCATION_OF_TI_BINARY_VERSION     (0xE0)
#define LOCATION_OF_ATMEL_BINARY_VERSION  (0x1C0)
#define MAIN_START_ADDRESS 		  (0xC000)

#define BATTERY_IOC_MAGIC                 'g'
#define BATTERY_IOCTDEBUG                 _IO(BATTERY_IOC_MAGIC,0)
#define BATTERY_IOCTWRITEFUSE             _IO(BATTERY_IOC_MAGIC,0)
#define BATTERY_IOCTERASEAPPANDBOOT       _IO(BATTERY_IOC_MAGIC,1)
#define BATTERY_IOCTCRC                   _IO(BATTERY_IOC_MAGIC,2)

//#define BATTERY_DEBUG 1  // comment out to enable debug ioctl

int  battery_load_kernel_module(tCharger charger);
int  battery_free_kernel_module(tCharger charger);
int  battery_load_firmware_binary(const char *pBinary, tCharger charger);
int  battery_retrieve_firmware_revision_from_binary(const char *pBinaryPath, char *pRetStr, int length, tCharger charger);
bool atmel_verify_binary(const char *pBinary, const char *pDeviceName);
bool tibat_verify_binary(unsigned int crcInfo );



#ifdef __cplusplus
} /* extern "C" */
#endif

#endif

