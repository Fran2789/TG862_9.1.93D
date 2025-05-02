/*
 *
 * qos_classifier.h
 * Description:
 * QOS Classifiers header file
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

#ifndef _QOS_CLASS_H_
#define _QOS_CLASS_H_

/**************************************************************************/
/*      INCLUDES                                                          */
/**************************************************************************/
#include <_tistdtypes.h>
#include "sys_ptypes.h"
#include "dpp_counters_db.h"

/**************************************************************************/
/*      INTERFACE  Defines and Structs                                    */
/**************************************************************************/

/* Classifier specified parameters BitMaps */
#define CL_BIT_PRIORITY            0x80000000
#define CL_BIT_ACT_STATE           0x40000000
#define CL_BIT_IP_TOS              0x20000000
#define CL_BIT_IP_PROTOCOL         0x10000000
#define CL_BIT_IP_SOUR_ADDR        0x08000000
#define CL_BIT_IP_SOUR_MASK        0x04000000
#define CL_BIT_IP_DEST_ADDR        0x02000000
#define CL_BIT_IP_DEST_MASK        0x01000000
#define CL_BIT_SOUR_PORT_START     0x00800000
#define CL_BIT_SOUR_PORT_END       0x00400000
#define CL_BIT_DEST_PORT_START     0x00200000
#define CL_BIT_DEST_PORT_END       0x00100000
#define CL_BIT_DEST_MAC            0x00080000
#define CL_BIT_SOUR_MAC            0x00040000
#define CL_BIT_ETHER_TYPE          0x00020000
#define CL_BIT_USER_PRI            0x00010000
#define CL_BIT_VLAN_ID             0x00008000
#define CL_BIT_FLOW_LABEL          0x00004000
#define CL_BIT_CM_INTERFACE_MASK   0x00002000

#define CL_BIT_ICMP46_TYPE         0x00000200

/* ICMP Type limits */
#define CL_ICMP_TYPE_MIN 0
#define CL_ICMP_TYPE_MAX 255

/*! \var    typedef struct QosClassIpv4_t
    \brief  QOS Classifier IPv4 matching criterias
*/
typedef struct {
	Uint32  srcAddr;
	Uint32  srcMask;
	Uint32  dstAddr;
	Uint32  dstMask;
	Uint16  protocol;
	Uint8  	tosLow;
	Uint8  	tosHigh;
	Uint8  	tosMask;
}QosClassIpv4_t;

/*! \var    typedef struct QosClassIpv6_t
    \brief  QOS Classifier IPv6 matching criterias
*/
typedef struct {
	Uint32  flowLabel;
	Uint32 	srcAddr[4];
	Uint32 	dstAddr[4];
	Uint32 	srcMask[4];
	Uint32 	dstMask[4];
	Uint16 	nextHdr;
	Uint8	tcLow;
	Uint8	tcHigh;
	Uint8	tcMask;
}QosClassIpv6_t;

/*! \var    typedef struct QosClassTcpUdp_t
    \brief  QOS Classifier TCP/UDP matching criterias
*/
typedef struct {
	Uint16  srcPortStart;
	Uint16  srcPortEnd;
	Uint16  dstPortStart;
	Uint16  dstPortEnd;
}QosClassTcpUdp_t;

/*! \var    typedef struct QosClassEthLlc_t
    \brief  QOS Classifier Ethernet LLC matching criterias
*/
typedef struct {
	Uint32 dstMacAddr4bytes; /* first 4 bytes (MSB) */
	Uint32 dstMacMask4bytes; /* first 4 bytes (MSB) */
	Uint16 dstMacAddr2bytes; /* the other 2 bytes (LSB) */
	Uint16 dstMacMask2bytes; /* the other 2 bytes (LSB) */
	Uint32 srcMacAddr4bytes; /* first 4 bytes (MSB) */
	Uint16 srcMacAddr2bytes; /* the other 2 bytes (LSB) */
	Uint16 enetProtocol;
	Uint8  enetProtocolType;
}QosClassEthLlc_t;

/*! \var    typedef struct QosClassEthLlc_t
    \brief  QOS Classifier Ethernet LLC matching criterias
*/
typedef struct {
	Uint16  vlanId;
	Uint8  	prioLow;
	Uint8   prioHigh;
}QosClassVlan_t;

/*! \var    typedef struct QosClassifier_t
    \brief  QOS Classifier structure
*/
typedef struct {
        Uint16 classID;
		Uint32 sfID;
        Int16  sfIndex; /*internal index of SF in the table */
		Int16  phsIndex;
		Uint8  priority;
		Uint64 pktCounter;

		/*Which classification criterias are used in this classifier*/
		Uint8   isLlcCriterion;
		Uint8   isVlanCriterion;
		Uint8   isIpv4Criterion;
		Uint8   isIpv6Criterion;
		Uint8   isTcpUdpCriterion;
		Uint8	isCmimCriterion;

		/* Bitmap of parameters which are relevant */
        Uint32   clParamsBitmap;

        /* Classifier parameters */
		union{
			QosClassIpv4_t v4;
			QosClassIpv6_t v6;
		}ip;
		QosClassTcpUdp_t udpTcp;
		QosClassEthLlc_t llc;
		QosClassVlan_t	 vlan;
		unsigned long long cmim;
		DppQosClassifierCounters_t ppCounters;;

        Uint8   Icmp46TypeLow;
        Uint8   Icmp46TypeHigh;

} QosClassifier_t;

/**************************************************************************/
/*      INTERFACE  Function prototypes                                    */
/**************************************************************************/
void QosClass_Init(void);
int QosClass_Start(void);
int QosClass_Stop(void);
int QosClass_Set(int subtype, Uint32 param1, Uint32 param2, void *buf);
int QosClass_Get(int subtype, Uint32 param1, Uint32 param2, void *buf);
int QosClass_Cleanup(void);

#endif
