/*
 *
 * mac_mngt_msg.h 
 * Description:
 * Definitions for DOCSIS MAC management Messages
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


/*! \file mac_mngt_msg.h
    \brief definitions for DOCSIS MAC management Messages
*/

#ifndef _MAC_MGNT_MSG_H_
#define _MAC_MGNT_MSG_H_


/**************************************************************************/
/*      INCLUDES                                                          */
/**************************************************************************/

/**************************************************************************/
/*      INTERFACE  Defines and Structs                                    */
/**************************************************************************/

/*! \var typedef enum MngtMsgType MngtMsgType_e
    \brief enumarates the "type" value of DOCSIS MAC management packets.
*/
typedef enum MngtMsgType
{
    MAC_MNGT_MSG_TYPE_SYNC           =    1,       /* Timing Synchronization Message                */
    MAC_MNGT_MSG_TYPE_UCD2,            /* 2  */    /* Upstream Channel Descriptor (type 2) Message  */
    MAC_MNGT_MSG_TYPE_MAP,             /* 3  */    /* Upstream Bandwidth Allocation Message         */
    MAC_MNGT_MSG_TYPE_RNG_REQ,         /* 4  */    /* Ranging Request Message                       */
    MAC_MNGT_MSG_TYPE_RNG_RSP,         /* 5  */    /* Ranging Response Message                      */
    MAC_MNGT_MSG_TYPE_REG_REQ,         /* 6  */    /* Registration Request Message                  */
    MAC_MNGT_MSG_TYPE_REG_RSP,         /* 7  */    /* Registration Response Message                 */
    MAC_MNGT_MSG_TYPE_UCC_REQ,         /* 8  */    /* Upstream Channel Change Request Message       */
    MAC_MNGT_MSG_TYPE_UCC_RSP,         /* 9  */    /* Upstream Channel Change Response Message      */
    MAC_MNGT_MSG_TYPE_TRI_TCD,         /* 10 */    /* Telephony Channel Descriptor Message          */
    MAC_MNGT_MSG_TYPE_TRI_TSI,         /* 11 */    /* Termination System Information Message        */
    MAC_MNGT_MSG_TYPE_BPKM_REQ,        /* 12 */    /* Privacy Key Management Request Message        */
    MAC_MNGT_MSG_TYPE_BPKM_RSP,        /* 13 */    /* Privacy Key Management Response Message       */
    MAC_MNGT_MSG_TYPE_REG_ACK,         /* 14 */    /* Registration Acknowledge Message              */
    MAC_MNGT_MSG_TYPE_DSA_REQ,         /* 15 */    /* Dynamic Service Addition Request Message      */
    MAC_MNGT_MSG_TYPE_DSA_RSP,         /* 16 */    /* Dynamic Service Addition Response Message     */
    MAC_MNGT_MSG_TYPE_DSA_ACK,         /* 17 */    /* Dynamic Service Addition Acknowledge Message  */
    MAC_MNGT_MSG_TYPE_DSC_REQ,         /* 18 */    /* Dynamic Service Change Request Message        */
    MAC_MNGT_MSG_TYPE_DSC_RSP,         /* 19 */    /* Dynamic Service Change Response Message       */
    MAC_MNGT_MSG_TYPE_DSC_ACK,         /* 20 */    /* Dynamic Service Change Acknowledge Message    */
    MAC_MNGT_MSG_TYPE_DSD_REQ,         /* 21 */    /* Dynamic Service Deletion Request Message      */
    MAC_MNGT_MSG_TYPE_DSD_RSP,         /* 22 */    /* Dynamic Service Deletion Response Message     */
    MAC_MNGT_MSG_TYPE_DCC_REQ,         /* 23 */    /* Dynamic Channel Change Request Message        */
    MAC_MNGT_MSG_TYPE_DCC_RSP,         /* 24 */    /* Dynamic Channel Change Response Message       */
    MAC_MNGT_MSG_TYPE_DCC_ACK,         /* 25 */    /* Dynamic Channel Change Acknowledge Message    */
    MAC_MNGT_MSG_TYPE_DCI_REQ,         /* 26 */    /* Device Class Identification Request Message   */
    MAC_MNGT_MSG_TYPE_DCI_RSP,         /* 27 */    /* Device Class Identification Response Message  */
    MAC_MNGT_MSG_TYPE_UP_DIS,          /* 28 */    /* Upstream Transmitter Disable Message          */
    MAC_MNGT_MSG_TYPE_UCD29,           /* 29 */    /* Upstream Channel Descriptor (type 29) Message */
    MAC_MNGT_MSG_TYPE_INIT_RNG_REQ,    /* 30 */    /* Initial Ranging Request Message               */
    MAC_MNGT_MSG_TYPE_TST_REQ,         /* 31 */    /* Test Request Message                          */
    MAC_MNGT_MSG_TYPE_DCD,             /* 32 */    /* UDownstream Channel Descriptor Message        */
    MAC_MNGT_MSG_TYPE_MDD,             /* 33 */    /* MAC Domain Descriptor         Message         */
    MAC_MNGT_MSG_TYPE_B_INIT_RNG_REQ,  /* 34 */    /* Bonded Initial Ranging Request Message        */
    MAC_MNGT_MSG_TYPE_UCD35,           /* 35 */    /* Upstream Channel Descriptor (type 35) Message */
    MAC_MNGT_MSG_TYPE_DBC_REQ,         /* 36 */    /* Dynamic Bonding Change Request Message        */
    MAC_MNGT_MSG_TYPE_DBC_RSP,         /* 37 */    /* Dynamic Bonding Change Response Message       */
    MAC_MNGT_MSG_TYPE_DBC_ACK,         /* 38 */    /* Dynamic Bonding Change Acknowledge Message    */
    MAC_MNGT_MSG_TYPE_DPV_REQ,         /* 39 */    /* DOCSIS Path Verify Request Message            */
    MAC_MNGT_MSG_TYPE_DPV_RSP,         /* 40 */    /* DOCSIS Path Verify Response Message           */
    MAC_MNGT_MSG_TYPE_CM_STATUS,       /* 41 */    /* Status Report Message                         */
    MAC_MNGT_MSG_TYPE_CM_CTRL_REQ,     /* 42 */    /* CM Control Request Message                    */
    MAC_MNGT_MSG_TYPE_CM_CTRL_RSP,     /* 43 */    /* CM Control Response Message                   */
    MAC_MNGT_MSG_TYPE_REG_REQ_MP,      /* 44 */    /* Multipart Registration Request Message        */
    MAC_MNGT_MSG_TYPE_REG_RSP_MP,      /* 45 */    /* Multipart Registration Response Message       */
    MAC_MNGT_MSG_TYPE_EM_REQ,          /* 46 */    /* Energy Management Request Message             */
    MAC_MNGT_MSG_TYPE_EM_RSP,          /* 47 */    /* Energy Management Response Message       */
    MAC_MNGT_MSG_TYPE_CM_STATUS_ACK,   /* 48 */    /* CM Status Report Ack Massage                  */
    DOCSIS_IGMP_JOIN = 100,            /* 100 */   /* Internal IGMP Join message                    */
    DOCSIS_IGMP_LEAVE = 101,           /* 101 */   /* Internal IGMP Leave message                   */    
    DOCSIS_MAC_LEARNING_CONTROL = 102, /* 102 */   /* Internal mac learning control message         */    
} MngtMsgType_e;

#pragma pack(1)
struct CM_mng_t {
    unsigned char da[6];
    unsigned char sa[6];
    unsigned short len;
    unsigned char dsap;
    unsigned char ssap;
    unsigned char control;
    unsigned char version;
    unsigned char type;
    unsigned char reserved;
    unsigned char data[];
};
#pragma pack()


/**************************************************************************/
/*      EXTERN definition block                                           */
/* #ifndef _MDL_C                                                         */
/* #define EXTERN                                                         */
/* #else                                                                  */
/* #define EXTERN extern                                                  */
/* #endif                                                                 */
/**************************************************************************/

/**************************************************************************/
/*      INTERFACE VARIABLES (prefix with EXTERN)                          */
/**************************************************************************/

/**************************************************************************/
/*      INTERFACE FUNCTIONS Prototypes:                                   */
/**************************************************************************/

#endif

