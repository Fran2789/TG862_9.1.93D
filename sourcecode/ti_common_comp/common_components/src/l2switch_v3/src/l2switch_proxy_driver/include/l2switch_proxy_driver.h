/*
 * GPL LICENSE SUMMARY
 *
 *  Copyright(c) 2014 Intel Corporation.
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
/*! \file l2switch_proxy_driver.h
    \brief declaration of functions and types used in the l2switch_proxy_driver 
*/

#ifndef _L2SWITCH_PROXY_DRIVER_H
#define _L2SWITCH_PROXY_DRIVER_H

/**************************************************************************/
/*      INCLUDES:                                                         */
/**************************************************************************/

/**************************************************************************/
/*      INTERFACE  DEFINES AND STRUCTS                                    */
/**************************************************************************/
#define L2SWITCH_PROXY_DEV_NAME   "/dev/l2switch_proxy_driver"
#define PROXY_PROC_NAME           "proxyStats"

typedef Uint8   proxy_psm_param_t;


#define L2SWITCH_PROXY_DRIVER_ID   57

#define L2SWITCH_PROXY_SETUP                     _IO(L2SWITCH_PROXY_DRIVER_ID,  1)
#define L2SWITCH_PROXY_PSM                       _IOWR(L2SWITCH_PROXY_DRIVER_ID, 2, proxy_psm_param_t)
#endif 