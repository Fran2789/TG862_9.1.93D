/*
 *
 * dbridge_db.h
 * Description:
 * Declaration of DOCSIS bridge multicast related functions and types.
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

#ifndef _DBRIDGE_MCAST_H_
#define _DBRIDGE_MCAST_H_


/**************************************************************************/
/*      INCLUDES                                                          */
/**************************************************************************/
#include <linux/list.h>
#include "dbridge_db.h"
/**************************************************************************/
/*  Various defines                                                       */
/**************************************************************************/

/**************************************************************************/
/*      INTERFACE  Defines and Structs                                    */
/**************************************************************************/


/*! \var typedef enum McastFilterType_e
    \brief enum defines the ...
*/
typedef enum McastFilterType_e
{
    MCAST_FILTER_NOT_ACTIVE,
	STATIC_FILTER, 			/* learn from config file */
    DYNAMIC_FILTER,         /* learn from SNMP or CPE */
    PROVISIONING_FILTER,    /* learn from PROVISIONING (IPV6) */
#ifdef DSG
    DSG_FILTER,
#endif
} McastFilterType_e;

/*! \var typedef struct DbridgeMcastMac DbridgeMcastMac_t
    \brief S.
*/
typedef struct DbridgeMcastMac 
{
    struct list_head    links;
	unsigned int		hwHashIndex;         /* the index in the HW hash table */
	unsigned int		refCount   ;         /* add ref count */
	McastFilterType_e	Type;               /* Mcast filter type */
    unsigned char		macAddress[MAC_ADDRESS_LEN];
 }DbridgeMcastMac_t;





/**************************************************************************/
/*      Default Values                                          */
/**************************************************************************/



/**************************************************************************/
/*      INTERFACE FUNCTIONS Prototypes:                                   */
/**************************************************************************/


/*! \fn int DbridgeDb_AddMcastMac(unsigned char* mac_address,McastFilterType_e filter_typ)
 *  \brief This function Add Mcast MAC address to Docsis bridge DB 
 *  \param[in] satic Mcast mac address.
 *  \param[in] type.
 *  \return OK or error status.
 */
int DbridgeDb_AddMcastMac(unsigned char* mac_address, McastFilterType_e filter_typ);

void DbridgeDb_DelMcastMac(unsigned char* mac_address);

void DbridgeDb_DelAllMcastMac(void);

DbridgeMcastMac_t* DbridgeDb_GetMcastByMac(unsigned char* mac_address);

#ifdef DSG
int DbridgeDb_AddDsgAddress(unsigned char* mac_address);
void DbridgeDb_DelDsgAddress(unsigned char* mac_address);
#endif //DSG
int dbrctl_read_mcastmac(char* page, char **start, off_t offset, int count,int *eof, void *data);


#endif

