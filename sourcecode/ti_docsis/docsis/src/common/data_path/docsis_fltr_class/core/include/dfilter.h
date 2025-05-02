/*
 *
 * dfilter.h
 * Description:
 * Protocol filters header file
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

#ifndef _FILTER_H_
#define _FILTER_H_

/**************************************************************************/
/*      INCLUDES                                                          */
/**************************************************************************/
#include "dpp_counters_db.h"

/**************************************************************************/
/*      INTERFACE  Defines and Structs                                    */
/**************************************************************************/
/* Spanning Tree Protocol filtering */
typedef enum StpControl_e
{
	FLTR_STP_ON = 1,       /* filter out STP frames */
	FLTR_NO_STP_DISCARD,   /* discard STP frames */
	FLTR_NO_STP_FORWARD    /* forward STP frames */
}StpControl_t;

/**************************************************************************/
/* LLC and Ip filtering tables                                            */
/**************************************************************************/
/*LLC filter structure (RFC-4639)*/
typedef struct FilterLLCStruct{
   Uint32 LLCIndex;
   Uint32 LLCIfIndex;
   Uint32 LLCProtocolType;
   Uint32 LLCProtocol;
   Uint32 LLCMatches;
   DppFilterCounters_t LLCPpCounters;
} FilterLLC_T;

/*IP filter structure (RFC-4639)*/
typedef struct FilterIPStruct{
   Uint32 IPIndex;
   Uint32 IPControl;  	 /* Accept discard or Policy(treated as accept) */
   Uint32 IPIfIndex;   	 /* Interface index */
   Uint32 IPDirection; 	 /* In, Out or both */
   Uint32 IPBroadcast; 	 /* Truth value - apply to braodcast */
   Uint32 IPSaddr;
   Uint32 IPSmask;
   Uint32 IPDaddr;
   Uint32 IPDmask;
   Uint32 IPProtocol;
   Uint32 IPSourcePortLow;
   Uint32 IPSourcePortHigh;
   Uint32 IPDestPortLow;
   Uint32 IPDestPortHigh;
   Uint32 IPMatches;   	/* Number of matching packets */
   Uint8  IPTos;       	/* Type of Service - inside IP Header */
   Uint8  IPTosMask;
   Uint16 padding;		/*padding for the purpose of alignment*/
   Uint32 IPContinue;  	/* If True - continue looking for other matches in table */
   Uint32 IPPolicyId;
   /*Parameters computed dynamically not from RFC*/
   unsigned long long IPIfIndexMask; /* Interface index mask (1<< ifindex-1)*/
   Uint32 IPSAddrMask; 	 /* src address & mask - computed dynamically */
   Uint32 IPDAddrMask;  /* dest address & mask - computed dynamically */
   DppFilterCounters_t IPInPpCounters;
   DppFilterCounters_t IPOutPpCounters;
 } FilterIP_T;

//#ifdef DSG
#define MACCMP(a1, a2) \
   ( (*((unsigned short *)((a1) + 4))) == (*((unsigned short *)((a2) + 4))) && \
     (*((unsigned short *)((a1) + 2))) == (*((unsigned short *)((a2) + 2))) && \
     (*((unsigned short *)((a1)    ))) == (*((unsigned short *)((a2)    ))) )
#define IP_SRC_ADDRESS_FIELD        12
#define IP_DEST_ADDRESS_FIELD        16
#pragma pack(1)
typedef struct { /* Mac addresses + Type/Length */
    unsigned long   MacDestAddr4Bytes; /* first 4 bytes (MSB) */
    unsigned short  MacDestAddr2Bytes; /* the other 2 bytes (LSB) */
    unsigned long   MacSrcAddr4Bytes;  /* first 4 bytes (MSB) */
    unsigned short  MacSrcAddr2Bytes;  /* the other 2 bytes (LSB) */
    unsigned short Type_Length; /* Type or Length (Ethernet 2 or 802.3) */
} DSG_DB_Layer2_header;
#pragma pack()
#pragma pack(1)
typedef struct { /* Mac addresses + Type/Length */
    unsigned long   MacDestAddr4Bytes; /* first 4 bytes (MSB) */
    unsigned short  MacDestAddr2Bytes; /* the other 2 bytes (LSB) */
    unsigned long   MacSrcAddr4Bytes;  /* first 4 bytes (MSB) */
    unsigned short  MacSrcAddr2Bytes;  /* the other 2 bytes (LSB) */
    unsigned short  TagFrameType;      /* protocol type */
    unsigned short  Prio_VlanId;       /* prio + vlan id */
    unsigned short Type_Length;        /* Type or Length (Ethernet 2 or 802.3) */
} DSG_DB_IEEE_header;
#pragma pack()
#pragma pack(1)
typedef struct { /* LLC format */
    unsigned long   pkts;
    unsigned long   octets;
} DSGMatch;
#pragma pack()
typedef struct { /* LLC format */
    unsigned char   Dsap;
    unsigned char   Reserved[5];
    unsigned short  Type;
} DSG_DB_Layer2_LLC_header;
typedef struct FilterDSGStruct{
   Uint32 DSGIndex;
   long DSGPriority;
   Uint32 DSGSrcIpAddr;
   Uint32 DSGSrcIpMask;
   Uint32 DSGDestIpAddr;
   Uint32 DSGDestPortStart;
   Uint32 DSGDestPortEnd;
   DSGMatch DSGMatches;       /* Number of matching packets */
   Uint8   DSGMacAddress[6];
   DppFilterCounters_t DSGPpCounters;
 } FilterDSG_T;
//#endif
/* Protocol filter result */
typedef enum FilterAction_e
{
	DFLTR_DISCARD,
	DFLTR_ACCEPT,
	DFLTR_UNKNOWN,
}DFltrAction_t;

/* Filtered packet direction */
typedef enum FilterPktDirect_e
{
	DFLTR_FROM_CABLE,
	DFLTR_TO_CABLE,
#ifdef DSG
    DFLTR_TO_DSG,
#endif
}DFltrPktDirect_t;

/*All interface types*/
#define DFLTR_IFACE_ALL		 0 
#define DFLTR_IFACE_CUSTSIDE_ETH   1    
/**************************************************************************/
/*      INTERFACE  Function prototypes                                    */
/**************************************************************************/
int StpControl_Set(Uint32 param1);
int DFilter_SetLLCData(int subtype, Uint32 param1, void *buf);
int DFilter_SetIpData(int subtype, Uint32 param1,  void *buf);
int DFilter_GetLLCData(int subtype, Uint32 param1, void *buf);
int DFilter_GetIpData(int subtype, Uint32 param1, void *buf);
int DFilters_Init(void);
int DFilters_ReInit(void);
int DFilters_Cleanup(void);
int DFilters_Start(void);
int DFilters_Stop(void);
void DFilter_UpdateIpFilterCmciMask(unsigned long long sysCmciIfBitmap);      //ARRIS ADD
void DFilter_UpdateCustSideSysIfIndex(Uint32 sysIfIndex);                     //ARRIS ADD  
#endif
