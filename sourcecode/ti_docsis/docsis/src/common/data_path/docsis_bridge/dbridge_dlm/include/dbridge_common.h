/*
 *
 * dbridge_common.h
 * Description:
 * DOCSIS bridge Common defines header file
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

#ifndef _DBRIDGE_COMMON_H_
#define _DBRIDGE_COMMON_H_

/**************************************************************************/
/*      INCLUDES                                                          */
/**************************************************************************/
#include <_tistdtypes.h>

/**************************************************************************/
/*  Various defines                                                       */
/**************************************************************************/

extern int (*DbridgeCommon_GetSysIfIndexByDocsisCB)(Uint32 docsisIfIndex, Uint32 *sysIfIndex);
extern int (*DbridgeCommon_GetDocsisIfIndexBySysIndexCB)(Uint32 sysIfIndex, Uint32 *docsisIfIndex);

#define BIT_SHIFT(x)                        ( 1 << (x) )
#define BIT_SHIFT_64(x)                     ( 1LL << (x) )
#define BIT_SHIFT_INVERT(x)                 ( 0x80000000 >> (x) )
#define BIT_SHIFT_INVERT_64(x)              ( 0x8000000000000000 >> (x) )
#define SET_BIT_MASK(val, x)                ( (val) |=  BIT_SHIFT((x)) )
#define SET_BIT_MASK_64(val, x)             ( (val) |=  BIT_SHIFT_64((x)) )
#define UNSET_BIT_MASK(val, x)              ( (val) &= ~BIT_SHIFT((x)) )
#define UNSET_BIT_MASK_64(val, x)           ( (val) &= ~BIT_SHIFT_64((x)) )

/* Docsis CMIM to network device types macros */
#define DOCSIS_CMIM_IP_STACK                0x80000000
#define DOCSIS_CMIM_PRIMARY_CPE             0x40000000
#define DOCSIS_CMIM_CABLE                   0x20000000
#define DOCSIS_CMIM_CMCI                    0x07FF0000
#define DOCSIS_CMIM_ESAFE                   0x0000FFFF
#define IsCmciInterface(M)                  (((M) & DOCSIS_CMIM_CMCI) ? True : False)
#define IsCableInterface(M)                 (((M) & DOCSIS_CMIM_CABLE) ? True : False)
#define IsEsafeInterface(M)                 (((M) & DOCSIS_CMIM_ESAFE) ? True : False)
#define IsWanIpInterface(M)                 (((M) & DOCSIS_CMIM_IP_STACK) ? True : False)
#define IsPrimaryInterface(M)               (((M) & DOCSIS_CMIM_PRIMARY_CPE) ? True : False)

/* CNID (Protocol specific ti_meta_info) macros */
#define CNID_ENCRYPTED_BIT_FIELD_OFFSET     4

/* Used in the US direction to indicated that a packet was tested to be l2vpn related */
#define US_L2VPN_CHECKED_BIT_FIELD_OFFSET   4
#define CNID_L2VPN_RELATED_BIT_FIELD_OFFSET 6
#define CNID_MULTICAST_BIT_FIELD_OFFSET     7
#define CNID_L2VPN_RELATED_SAID_MASK        0x1F
#define CNID_ENCRYPTED                      (1 << CNID_ENCRYPTED_BIT_FIELD_OFFSET)
#define US_L2VPN_CHECKED                    (1 << US_L2VPN_CHECKED_BIT_FIELD_OFFSET)
#define CNID_L2VPN_RELATED                  (1 << CNID_L2VPN_RELATED_BIT_FIELD_OFFSET)
#define CNID_MULTICAST                      (1 << CNID_MULTICAST_BIT_FIELD_OFFSET)

/*! \def MAC_ISBROADCAST( pa )
 *  \brief if broadcast return true , else false
 */
#define MAC_ISBROADCAST( pa, hw ) ( ~0xFF ==( (~(pa)->hw[0]) | \
                                              (~(pa)->hw[1]) | \
                                              (~(pa)->hw[2]) | \
                                              (~(pa)->hw[3]) | \
                                              (~(pa)->hw[4]) | \
                                              (~(pa)->hw[5]) ))

/*! \def MAC_ISMULTICAST( pa )
 *  \brief if the multicast macaddress return true , else false
 */
#define MULTICAST_MASK 0x01    
#define MAC_ISMULTICAST( pa, field )  ( ((pa)->field[0]) & MULTICAST_MASK )

/**************************************************************************/
/*      INTERFACE  Defines and Structs                                    */
/**************************************************************************/


/**************************************************************************/
/*      INTERFACE FUNCTIONS Declaration:                                  */
/**************************************************************************/


/**************************************************************************/
/*! \fn int DbridgeCommon_TransDocsisCmimToLinuxCmim(Uint32 docsisCmim, Uint32* linuxCmim)
 **************************************************************************
 *  \brief Translate the Docsis CMIM to Linux CMIM.
 *  \param[in] docsisCmim - the Docsis CMIM value.
 *  \param[out] linuxCmim - the Linux CMIM value to be returned.
 *  \return OK or error status.
 **************************************************************************/
int DbridgeCommon_TransDocsisCmimToLinuxCmim(Uint32 docsisCmim, unsigned long long* linuxCmim);

/**************************************************************************/
/*! \fn int DbridgeCommon_TransLinuxCmimToDocsisCmim(Uint32 linuxCmim, Uint32* docsisCmim)
 **************************************************************************
 *  \brief Translate the Linux CMIM to Docsis CMIM.
 *  \param[in] linuxCmim - the linux CMIM value.
 *  \param[out] docsisCmim - the docsis CMIM value to be returned.
 *  \return OK or error status.
 **************************************************************************/
int DbridgeCommon_TransLinuxCmimToDocsisCmim(unsigned long long linuxCmim, Uint32* docsisCmim);

/**************************************************************************/
/*! \fn int DbridgeCommon_TransDocsisCmimToFloodMap(Uint32 docsisCmim, Uint32* floodMap)                                    
 **************************************************************************
 *  \brief Translate the Docsis CMIM to Flood Map (network device type numbering).
 *  \param[in] docsisCmim - the Docsis CMIM value.
 *  \param[out] floodMap - the Flood Map value to be returned.
 *  \param[out] no output.
 *  \return OK or error status.
 **************************************************************************/
int DbridgeCommon_TransDocsisCmimToFloodMap(Uint32 docsisCmim, Uint32* floodMap);


/**************************************************************************/
/*! \fn Bool DbridgeCommon_ReceiveFilterL2VPNorDUT(struct sk_buff *skb, unsigned int outDev)
 **************************************************************************
 *  \brief Filters L2VPN DS related or DUT traffic coming from CNI
 *  \param[in] skb - skb pointer
 *  \param[in] outDev - The output network device type (CMCI, ESAFE or WAN)
 *  \return True if need to drop the packet
 **************************************************************************/
Bool DbridgeCommon_ReceiveFilterL2VPNorDUT(struct sk_buff *skb, unsigned int outDev);

#endif

