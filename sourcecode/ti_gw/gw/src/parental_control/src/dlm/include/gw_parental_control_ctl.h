/*
 *
 * gw_parental_control_ctl.h
 * Description:
 * GW parental control header file
 *
 * Copyright (C) 2008 Texas Instruments Incorporated - http://www.ti.com/ 
 * 
 * 
 *  Redistribution and use in source and binary forms, with or without 
 *  modification, are permitted provided that the following conditions 
 *  are met:
 *
 *    Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer.
 *
 *    Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the 
 *    documentation and/or other materials provided with the   
 *    distribution.
 *
 *    Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
*/

#ifndef _GW_PARENTAL_CONTROL_CTL_H_
#define _GW_PARENTAL_CONTROL_CTL_H_

/**************************************************************************/
/*      INCLUDES                                                          */
/**************************************************************************/
#include <_tistdtypes.h>

/**************************************************************************/
/*      INTERFACE  Defines and Structs                                    */
/**************************************************************************/

#define GW_PARENTAL_CONTROL_IOCTL_MAGIC			'f'
#define GW_PARENTAL_CONTROL_IOCTL_MAX_NUM          (ENUM_PARENTAL_CONTROL_IOCTL_NUM_OPS)

/*! \var typedef struct GwParentalControlIoctlData_t
    \brief Structure defines the format of the the GW parental control ioctl data structure. 
*/
typedef struct GwParentalControlIoctlData
{
	int subtype;		/* ioctl command subtype */
	int buf_len;		/* buffer length */
	Uint32 param1;		/* additional interger parameter*/
	void __user *buf;	/* data buffer   */

}GwParentalControlIoctlData_t;


/************************** Main Ioctl commands *************************/

/*! \var typedef enum GwParentalControlIoctlOperations_e
    \brief Enumerate GW parental control ioctl-s
*/
typedef enum
{
    ENUM_PARENTAL_CONTROL_IOCTL_SET_MAC,
    ENUM_PARENTAL_CONTROL_IOCTL_DEL_MAC,
    ENUM_PARENTAL_CONTROL_IOCTL_FLUSH_MAC,
    ENUM_PARENTAL_CONTROL_IOCTL_PRINT_DB,
    ENUM_PARENTAL_CONTROL_IOCTL_SET_ENABLE_STATUS,
    ENUM_PC_CPE_MAC_FILTERING_IOCTL_SET_MAC,
    ENUM_PC_CPE_MAC_FILTERING_IOCTL_DEL_MAC,
    ENUM_PC_CPE_MAC_FILTERING_IOCTL_FLUSH_MAC,
    ENUM_PC_CPE_MAC_FILTERING_IOCTL_PRINT_DB,
    ENUM_PARENTAL_CONTROL_IOCTL_NUM_OPS
} GwParentalControlIoctlOperations_e;

#define PARENTAL_CONTROL_IOCTL_SET_MAC           _IOW(GW_PARENTAL_CONTROL_IOCTL_MAGIC, ENUM_PARENTAL_CONTROL_IOCTL_SET_MAC,           GwParentalControlIoctlData_t)
#define PARENTAL_CONTROL_IOCTL_DEL_MAC           _IOW(GW_PARENTAL_CONTROL_IOCTL_MAGIC, ENUM_PARENTAL_CONTROL_IOCTL_DEL_MAC,           GwParentalControlIoctlData_t)
#define PARENTAL_CONTROL_IOCTL_PRINT_DB 		 _IOW(GW_PARENTAL_CONTROL_IOCTL_MAGIC, ENUM_PARENTAL_CONTROL_IOCTL_PRINT_DB,          GwParentalControlIoctlData_t)
#define PARENTAL_CONTROL_IOCTL_SET_ENABLE_STATUS _IOW(GW_PARENTAL_CONTROL_IOCTL_MAGIC, ENUM_PARENTAL_CONTROL_IOCTL_SET_ENABLE_STATUS, GwParentalControlIoctlData_t)
#define PARENTAL_CONTROL_IOCTL_FLUSH_MAC 		 _IOW(GW_PARENTAL_CONTROL_IOCTL_MAGIC, ENUM_PARENTAL_CONTROL_IOCTL_FLUSH_MAC,         GwParentalControlIoctlData_t)
#define PC_CPE_MAC_FILTERING_IOCTL_SET_MAC       _IOW(GW_PARENTAL_CONTROL_IOCTL_MAGIC, ENUM_PC_CPE_MAC_FILTERING_IOCTL_SET_MAC,       GwParentalControlIoctlData_t)
#define PC_CPE_MAC_FILTERING_IOCTL_DEL_MAC       _IOW(GW_PARENTAL_CONTROL_IOCTL_MAGIC, ENUM_PC_CPE_MAC_FILTERING_IOCTL_DEL_MAC,       GwParentalControlIoctlData_t)
#define PC_CPE_MAC_FILTERING_IOCTL_FLUSH_MAC     _IOW(GW_PARENTAL_CONTROL_IOCTL_MAGIC, ENUM_PC_CPE_MAC_FILTERING_IOCTL_FLUSH_MAC,     GwParentalControlIoctlData_t)
#define PC_CPE_MAC_FILTERING_IOCTL_PRINT_DB      _IOW(GW_PARENTAL_CONTROL_IOCTL_MAGIC, ENUM_PC_CPE_MAC_FILTERING_IOCTL_PRINT_DB,      GwParentalControlIoctlData_t)

#endif
