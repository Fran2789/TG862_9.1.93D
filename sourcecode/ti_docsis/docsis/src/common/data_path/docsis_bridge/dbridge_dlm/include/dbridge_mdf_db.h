/*
 *
 * dbridge_mdf_db.h
 * Description:
 * DOCSIS Bridge Multicast DSID Forwarding data base header file
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

#ifndef _DBRIDGE_MDF_DB_H_
#define _DBRIDGE_MDF_DB_H_

/**************************************************************************/
/*      INCLUDES                                                          */
/**************************************************************************/
#include <_tistdtypes.h>

/**************************************************************************/
/*  Various defines                                                       */
/**************************************************************************/

/* NOTICE: THIS MUST CORRELATE TO hal_db.h DOCSIS_MAX_DSID !!! */
#if defined (CONFIG_MACH_PUMA6)
#define DBR_MDF_MAX_DSIDS        (64)
#else
#define DBR_MDF_MAX_DSIDS        (32)
#endif

#define DBR_MDF_MAX_CPES         (64)
#if defined(MAC_ADDRESS_LEN) 
    #if (MAC_ADDRESS_LEN != 6)
        #error MAC_ADDRESS_LEN redefined with value != 6
    #endif
#else
    #define MAC_ADDRESS_LEN      (6)
#endif

#define DBR_MDF_DISABLE_MODE	 (0)	 
#define DBR_MDF_ENABLE_MODE      (1) 

/**************************************************************************/
/*      INTERFACE  Defines and Structs                                    */
/**************************************************************************/

/*! \var typedef struct DbridgeMcastClientMacAddr DbridgeMcastClientMacAddr_t
    \brief Structure defines the format of the Multicast Client MAC Address encodings parameters.
*/
typedef struct DbridgeMcastClientMacAddr
{
    Uint8 addrConfCode;                      /* indicates the client MAC Address confirmation/error code */
    Uint8 clientMacAddressAction;            /* informs the CM as to whether it is to add or delete the client MAC address. */
    Uint8 clientMacAddress[MAC_ADDRESS_LEN]; /* provides the CM with the source MAC address joining or 
                                                leaving the multicast group associated with the group flow label */
} DbridgeMcastClientMacAddr_t;


/*! \var typedef struct DbridgeMcastEncodings DbridgeMcastEncodings_t
    \brief Structure defines the format of the Multicast encodings parameters.
*/
typedef struct DbridgeMcastEncodings
{
    DbridgeMcastClientMacAddr_t clientMacAddrEncodings[DBR_MDF_MAX_CPES]; /* provides the CM with the client MAC address(es) 
                                                                             joining or leaving the multicast group */
    Uint8 clientMacAddrNum;  /* the number of current client MAC addresses in the array */
    Uint32 mcastDocsisCmim;  /* provides a bit mask representing the interfaces of the CM to which 
                                the CM is to forward multicast traffic associated with the DSID 
                                Each bit of CM interface mask corresponds to an interface, logical or physical */
    Bool gotMcastDocsisCmim; /* indicates if the Multicast CMIM value was received in the Multicast encodings */

    Uint32 mcastLinuxCmim;   /* CMIM interfaces indexed (in Linux Kernel numeration) bitmap */
                             /* a bit mask representing the Linux interfaces of the CM to which the
                                CM is to forward multicast traffic associated with the DSID*/

    Uint32 mcastFloodMap;    /* network device types bitmap for mapping the destanation index the packet is send to */
                             /* a bit mask representing the docsis interface type - CPE eSAFE CABLE WAN IP or LAN IP */

    Uint8 mcastConfCode;     /* indicates the confirmation/error code */
       

} DbridgeMcastEncodings_t;

/*! \var typedef union DbridgeGetAltMemberExist_t
    \brief Info for getting ALT member
*/
typedef union
{
    Bool found;
    Uint8 clientMacAddress[MAC_ADDRESS_LEN]; 
} DbridgeGetAltMemberExist_t;

/**************************************************************************/
/*      INTERFACE FUNCTIONS Prototypes:                                   */
/**************************************************************************/

/*! \fn int DbridgeMdfDb_Init(void)
 *  \brief Docsis bridge MDF data base initialization.
 *  \param[in] no input.
 *  \param[out] no output.
 *  \return OK or error status.
 */
int DbridgeMdfDb_Init(void);

/*! \fn int DbridgeMdfDb_ReInit(void)                                     
 *  \brief Docsis bridge MDF data base re-initialization.
 *  \param[in] no input.
 *  \param[out] no output.
 *  \return OK or error status.
 */
int DbridgeMdfDb_ReInit(void);

/*! \fn int DbridgeMdfDb_SetMdfMode(int subtype, Uint32 param1)                                     
 *  \brief Set Docsis Bridge MDF Mode.
 *  \param[in] subtype - command subtype ID.
 *  \param[in] param1 - generic integer parameter.
 *  \param[out] no output.
 *  \return OK or error status.
 */
int DbridgeMdfDb_SetMdfMode(int subtype, Uint32 param1);

/*! \fn int DbridgeMdfDb_SetPreRegDsid(int subtype, Uint32 param1)                                     
 *  \brief Set Multicast Pre-Registration DSID index.
 *  \param[in] subtype - command subtype ID.
 *  \param[in] param1 - generic integer parameter.
 *  \param[out] no output.
 *  \return OK or error status.
 */
int DbridgeMdfDb_SetPreRegDsid(int subtype, Uint32 param1);

/*! \fn int DbridgeMdfDb_SetMcastDsid(int subtype, Uint32 param1, void *buf)                                     
 *  \brief Set Multicast DSID Encodings.
 *  \param[in] subtype - command subtype ID.
 *  \param[in] param1 - generic integer parameter.
 *  \param[in/out] buf - input/output buffer.
 *  \return OK or error status.
 */
int DbridgeMdfDb_SetMcastDsid(int subtype, Uint32 param1, void *buf);

/*! \fn int DbridgeMdfDb_GetMcastDocsisClientCmim(int subtype, Uint32 param1, void *buf)                                    
 *  \brief Get Multicast Linux CMIM.
 *  \param[in] subtype - command subtype ID.
 *  \param[in] param1 - generic integer parameter.
 *  \param[in/out] buf - input/output buffer.
 *  \return OK or error status.
 */
int DbridgeMdfDb_GetMcastDocsisClientCmim(int subtype, Uint32 param1, void *buf);

/*! \fn int DbridgeMdfDb_GetMcastFloodMap(int subtype, Uint32 param1, void *buf)                                    
 *  \brief Get Multicast Flood Map.
 *  \param[in] subtype - command subtype ID.
 *  \param[in] param1 - generic integer parameter.
 *  \param[in/out] buf - input/output buffer.
 *  \return OK or error status.
 */
int DbridgeMdfDb_GetMcastFloodMap(int subtype, Uint32 param1, void *buf);

/*! \fn int DbridgeMdfDb_GetMcastDsidConfCode(int subtype, Uint32 param1, void *buf)
 *  \brief Get Multicast DSID confirmation/error codes.
 *  \param[in] subtype - command subtype ID.
 *  \param[in] param1 - generic integer parameter.
 *  \param[in/out] buf - input/output buffer.
 *  \return OK or error status.
 */
int DbridgeMdfDb_GetMcastDsidConfCode(int subtype, Uint32 param1, void *buf);

/*! \fn int DbridgeMdfDb_SetMcastDsidConfCode(int subtype, Uint32 param1)
 *  \brief Set Multicast DSID confirmation/error code.
 *  \param[in] subtype - command subtype ID.
 *  \param[in] param1 - generic integer parameter.
 *  \return OK or error status.
 */
int DbridgeMdfDb_SetMcastDsidConfCode(int subtype, Uint32 param1);

/*! \fn int DbridgeMdfDb_GetAltMemeberExist(int subtype, void *buf)
 *  \brief Does ALT member exist, key is MAC address
 *  \param[in] subtype - command subtype ID.
 *  \param[in/out] buf - input/output buffer.
 *  \return OK or error status.
 */
int DbridgeMdfDb_GetAltMemeberExist(int subtype, void *buf);

/*! \fn int DbridgeMdfDb_Print(void)                                     
 *  \brief Print Docsis bridge MDF data base.
 *  \param[in] no input.
 *  \param[out] no output.
 *  \return OK or error status.
 */
int DbridgeMdfDb_Print(void);

/*! \fn int DbridgeMdfDb_GetMdfMode(Uint32* mdfMode)                                     
 *  \brief Get Docsis Bridge MDF Mode.
 *  \param[in] no input.
 *  \param[out] mdfMode - the Docsis Bridge MDF Mode to be returned.
 *  \return OK or error status.
 */
int DbridgeMdfDb_GetMdfMode(Uint32* mdfMode);

/*! \fn int DbridgeMdfDb_GetMcastBitmaps(Uint32 dsidIndex, Uint32* mcastLinuxCmim, Uint32* mcastFloodMap)                                     
 *  \brief Get the Multicast bitmaps according to the dsid index.
 *  \param[in] dsidIndex - dsid index value.
 *  \param[out] mcastLinuxCmim - Multicast Linux CMIM value to be returned.
 *  \param[out] mcastFloodMap - Multicast Flood Map value to be returned.
 *  \return OK or error status.
 */
int DbridgeMdfDb_GetMcastBitmaps(Uint32 dsidIndex, unsigned long long* mcastLinuxCmim, Uint32* mcastFloodMap);

#endif

