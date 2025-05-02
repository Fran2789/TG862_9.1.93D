/*
 *
 * dbridge_mdf_ctl.h
 * Description:
 * DOCSIS Bridge Multicast DSID Forwarding control header file
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

#ifndef _DBRIDGE_MDF_CTL_H_
#define _DBRIDGE_MDF_CTL_H_

/**************************************************************************/
/*      INCLUDES                                                          */
/**************************************************************************/
#include <_tistdtypes.h>

/**************************************************************************/
/*      INTERFACE  Defines and Structs                                    */
/**************************************************************************/

#define DBR_MDF_IOCTL_MAGIC			'f'
#define DBR_MDF_IOCTL_MAX_NUM       (ENUM_MDF_IOCTL_NUM_OPS)

/*! \var typedef struct DbridgeMdfIoctlData DbridgeMdfIoctlData_t
    \brief Structure defines the format of the the Multicast DSID Forwarding ioctl data structure. 
*/
typedef struct DbridgeMdfIoctlData
{
	int subtype;		/* ioctl command subtype */
	int buf_len;		/* buffer length */
	Uint32 param1;		/* additional interger parameter*/
	void   *buf;	/* data buffer   */

}DbridgeMdfIoctlData_t;


/************************** Main Ioctl commands *************************/

/*! \var typedef enum DbridgeMdfIoctlOerations_e
    \brief Enumerate Dbridge MDF ioctl-s
*/
typedef enum
{
    ENUM_MDF_IOCTL_S_MODE,
    ENUM_MDF_IOCTL_S_PREREGDSID,
    ENUM_MDF_IOCTL_S_MCASTDSID,
    ENUM_MDF_IOCTL_G_MCASTDSIDFWDCMIM,
    ENUM_MDF_IOCTL_G_MCASTDSIDFLOODMAP,
    ENUM_MDF_IOCTL_G_MCASTDSIDCONF,
    ENUM_MDF_IOCTL_G_ALTGETMEMBEREXIST,
    ENUM_MDF_IOCTL_S_MCASTDSIDCONF,
    ENUM_MDF_IOCTL_S_PRINTDBRIDGEMDFDB,
    ENUM_MDF_IOCTL_NUM_OPS
} DbridgeMdfIoctlOperations_e;

/* MDF Mode ioctl */
#define MDF_IOCTL_S_MODE                _IOW(DBR_MDF_IOCTL_MAGIC, ENUM_MDF_IOCTL_S_MODE, DbridgeMdfIoctlData_t)

/* Pre-Registration DSID ioctl */
#define MDF_IOCTL_S_PREREGDSID          _IOW(DBR_MDF_IOCTL_MAGIC, ENUM_MDF_IOCTL_S_PREREGDSID, DbridgeMdfIoctlData_t)

/* Multicast DSID ioctls */
#define MDF_IOCTL_S_MCASTDSID           _IOW(DBR_MDF_IOCTL_MAGIC, ENUM_MDF_IOCTL_S_MCASTDSID, DbridgeMdfIoctlData_t)
#define MDF_IOCTL_G_MCASTDSIDFWDCMIM	_IOR(DBR_MDF_IOCTL_MAGIC, ENUM_MDF_IOCTL_G_MCASTDSIDFWDCMIM, DbridgeMdfIoctlData_t)
#define MDF_IOCTL_G_MCASTDSIDFLOODMAP	_IOR(DBR_MDF_IOCTL_MAGIC, ENUM_MDF_IOCTL_G_MCASTDSIDFLOODMAP, DbridgeMdfIoctlData_t)
#define MDF_IOCTL_G_MCASTDSIDCONF       _IOR(DBR_MDF_IOCTL_MAGIC, ENUM_MDF_IOCTL_G_MCASTDSIDCONF, DbridgeMdfIoctlData_t)
#define MDF_IOCTL_G_ALTGETMEMBEREXIST   _IOR(DBR_MDF_IOCTL_MAGIC, ENUM_MDF_IOCTL_G_ALTGETMEMBEREXIST, DbridgeMdfIoctlData_t)
#define MDF_IOCTL_S_MCASTDSIDCONF       _IOW(DBR_MDF_IOCTL_MAGIC, ENUM_MDF_IOCTL_S_MCASTDSIDCONF, DbridgeMdfIoctlData_t)

/* Multicast print ioctl */
#define MDF_IOCTL_S_PRINTDBRIDGEMDFDB   _IOW(DBR_MDF_IOCTL_MAGIC, ENUM_MDF_IOCTL_S_PRINTDBRIDGEMDFDB, DbridgeMdfIoctlData_t)

/* Set ioctl subtypes */
#define MDF_IOCTL_S_PREREGDSID_SUBTYPE_ADD    0  /* add Pre-Registration DSID */
#define MDF_IOCTL_S_PREREGDSID_SUBTYPE_DEL	  1  /* delete Pre-Registration DSID */

#define MDF_IOCTL_S_MCASTDSID_SUBTYPE_ADD     0  /* add Multicast DSID */
#define MDF_IOCTL_S_MCASTDSID_SUBTYPE_DEL     1  /* delete Multicast DSID */
#define MDF_IOCTL_S_MCASTDSID_SUBTYPE_UPD     2  /* update Multicast DSID */

/**************************************************************************/
/*      INTERFACE FUNCTIONS Prototypes:                                   */
/**************************************************************************/

/*! \fn int DbridgeMdfCtl_Init(void)
 *  \brief DOCSIS Bridge MDF Ctl initialization.
 *  \param[in] no input.
 *  \param[out] no output.
 *  \return 0 or error code.
 */
int DbridgeMdfCtl_Init(void);

/*! \fn int DbridgeMdfCtl_Exit(void)
 *  \brief DOCSIS Bridge MDF Ctl exit.
 *  \param[in] no input.
 *  \param[out] no output.
 *  \return 0 or error code.
 */
int DbridgeMdfCtl_Exit(void);

#endif
