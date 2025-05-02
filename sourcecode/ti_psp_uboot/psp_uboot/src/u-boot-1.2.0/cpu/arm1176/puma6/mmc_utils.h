 /*
 * GPL LICENSE SUMMARY
 *
 *  Copyright(c) 2012 Intel Corporation.
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
 *
 *  You should have received a copy of the GNU General Public License 
 *  along with this program; if not, write to the Free Software 
 *  Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *  The full GNU General Public License is included in this distribution 
 *  in the file called LICENSE.GPL.
 *
 *  Contact Information:
 *  Intel Corporation
 *  2200 Mission College Blvd.
 *  Santa Clara, CA  97052
 */

#ifndef __MMC_UTILS_H_
#define __MMC_UTILS_H_


#define STATUS_OK       		0
#define STATUS_NOK      		1
#define VERBOSE_ENABLE  		1
#define VERBOSE_DISABLE 		0

/* Functions to read and write from/to mmc*/
int mmc_read_buff(unsigned int addr, unsigned int* buff, unsigned int size);
int flash_write_mmc(unsigned int src, unsigned int dest, unsigned int length, int verbose);


#endif
