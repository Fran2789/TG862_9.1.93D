/*
 *
 * dpp_counters_db.h
 * Description:
 * DOCSIS Packet Processor counters data base header file
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

#ifndef _DPP_COUNTERS_DB_H_
#define _DPP_COUNTERS_DB_H_

/**************************************************************************/
/*      INCLUDES                                                          */
/**************************************************************************/
#include <_tistdtypes.h>
#ifdef CONFIG_MACH_PUMA5
#include <linux/ti_ppm.h>
#else
#include <asm-arm/arch-avalanche/generic/avalanche_pp_api.h>
#endif

/**************************************************************************/
/*  Various defines                                                       */
/**************************************************************************/

#define DPP_IF_MANE_MAX_LENGTH  16

/**************************************************************************/
/*      INTERFACE  Defines and Structs                                    */
/**************************************************************************/

/*! \var typedef struct DppIfCounters DppIfCounters_t
    \brief Structure defines the format of the Docsis packet processor counters per interface device.
*/
typedef struct DppIfCounters
{
    Char    ifName[DPP_IF_MANE_MAX_LENGTH];
    Bool    includeQosDrops;
#ifdef CONFIG_MACH_PUMA5
    TI_PP_VPID_STATS ifCounters;
#else
    AVALANCHE_PP_VPID_STATS_t ifCounters;
#endif
} DppIfCounters_t;

/*! \var typedef struct DppSfCounters DppSfCounters_t
    \brief Structure defines the format of the Docsis packet processor counters per service flow.
*/
typedef struct DppSfCounters
{
    Uint32  sfIndex;
    Uint32  clearFlag;
    Uint8   ifName[DPP_IF_MANE_MAX_LENGTH];
#ifdef CONFIG_MACH_PUMA5
    TI_PP_QOS_QUEUE_STATS sfCounters;
#else 
    AVALANCHE_PP_QOS_QUEUE_STATS_t sfCounters;
#endif

} DppSfCounters_t;

/*! \var typedef struct DppTpPortCounters DppTpPortCounters_t
    \brief Structure defines the format of the Docsis packet processor counters per transparent port.
*/
typedef struct DppTpPortCounters
{
    Uint8 ifName[DPP_IF_MANE_MAX_LENGTH]; 
    Uint32 inFrames;
    Uint32 outFrames;
    Uint32 discardFrames;

} DppTpPortCounters_t;


/*! \var typedef struct DppFilterCounters DppFilterCounters_t
    \brief Structure defines the format of the Docsis packet processor counters per filter.
*/
typedef struct DppFilterCounters
{
   Uint64 ppBytes;
   Uint32 ppMatches;
   struct DppCountSessionEntry *activeSessionsList;

} DppFilterCounters_t;


/*! \var typedef struct DppQosClassifierCounters DppQosClassifierCounters_t
    \brief Structure defines the format of the Docsis packet processor counters per classifier.
*/
typedef struct DppQosClassifierCounters
{
    Uint64 ppPktCounter;
    struct DppCountSessionEntry *activeSessionsList;

} DppQosClassifierCounters_t;


/*! \var typedef struct DppSessionClientInfo DppSessionClientInfo_t
    \brief Structure defines the format of the Docsis packet processor session client information entry.
*/
typedef struct DppSessionClientInfo
{
    struct DppCountSessionEntry *nextSession;
    void* clientPtr;

} DppSessionClientInfo_t;


/*! \var typedef struct DppCountSessionEntry DppCountSessionEntry_t
    \brief Structure defines the format of the Docsis packet processor counters sessions table entry.
*/
typedef struct DppCountSessionEntry
{
    Uint32 sessionHandle;  
    Uint8 sessionType;  
    DppSessionClientInfo_t ingressPortInfo;
    DppSessionClientInfo_t llcFilterInfo;
    DppSessionClientInfo_t inIpFilterInfo;
    DppSessionClientInfo_t outIpFilterInfo;
	DppSessionClientInfo_t dsgFilterInfo;
    DppSessionClientInfo_t qosClassInfo;

} DppCountSessionEntry_t;


/**************************************************************************/
/*      INTERFACE FUNCTIONS Prototypes:                                   */
/**************************************************************************/

/*! \fn int DppCountersDb_Init(void)
 *  \brief Docsis Packet Processor counters data base initialization.
 *  \param[in] no input.
 *  \param[out] no output.
 *  \return OK or error status.
 */
int DppCountersDb_Init(void);

/*! \fn int DppCountersDb_CreateSession(Uint32 sessionHandle, Uint8 sessionType, 
                                        Uint32 ingressPortId, 
                                        void* ti_match_llc_filter, 
                                        void* ti_match_inbound_ip_filter,
                                        void* ti_match_outbound_ip_filter,
                                        void* ti_match_qos_classifier)                                
 *  \brief DOCSIS Packet Processor counters create session.
 *   Updates Docsis PP data structures to reflect the necessary 
 *   associations between the new session and all the clients it feeds 
 *  (filter(s), classifier, ingress port).
 *  \param[in] sessionHandle - PP session handle number.
 *  \param[in] sessionType - PP session type.
 *  \param[in] ingressPortId - input device interface index.
 *  \param[in] ti_match_llc_filter - pointer to LLC filter.
 *  \param[in] ti_match_inbound_ip_filter - pointer to inbound IP filter.
 *  \param[in] ti_match_outbound_ip_filter - pointer to outbound IP filter.
 *  \param[in] ti_match_qos_classifier - pointer to QoS classifier.
 *  \param[out] no output.
 *  \return OK or error status.
 */
int DppCountersDb_CreateSession(Uint32 sessionHandle, Uint8 sessionType, 
                                Uint32 ingressPortId, 
                                void* ti_match_llc_filter, 
                                void* ti_match_inbound_ip_filter,
                                void* ti_match_outbound_ip_filter,
                                void* ti_match_qos_classifier,
								void* ti_match_dsg_filter);

/*! \fn int DppCountersDb_DeleteSession(Uint32 sessionHandle, , Uint64 sessionOctetsFw)                                  
 *  \brief DOCSIS Packet Processor counters delete session.
 *   Increments the relevant counter(s) fed by the session and removes all existing 
 *   associations between the deleted session and all the clients it feeds 
 *   (filter(s), classifier, ingress port). 
 *  \param[in] sessionHandle - PP session handle number.
 *  \param[in] sessionPacketsFw - PP session forwarded packets number.
 *  \param[in] sessionOctetsFw - PP session forwarded bytes number.
 *  \param[out] no output.
 *  \return OK or error status.
 */
int DppCountersDb_DeleteSession(Uint32 sessionHandle, Uint32 sessionPacketsFw, Uint64 sessionOctetsFw);

/*! \fn int DppCountersDb_GetIfCounters(DppIfCounters_t* dppIfCounters)                                   
 *  \brief Get Docsis packet processor interface device counters according to interface name.
 *  \param[in,out] dppIfCounters - Docsis packet processor interface device counters structure.
 *  \return OK or error status.
 */
int DppCountersDb_GetIfCounters(DppIfCounters_t* dppIfCounters);

/*! \fn int DppCountersDb_GetTpPortCounters(DppTpPortCounters_t* dppTpPortCounters)                                  
 *  \brief Get Docsis packet processor transparent port counters according to interface name.
 *  \param[in,out] dppTpPortCounters - Docsis packet processor transparent port counters structure.
 *  \return OK or error status.
 */
int DppCountersDb_GetTpPortCounters(DppTpPortCounters_t* dppTpPortCounters);

/*! \fn int DppCountersDb_GetSfCounters(DppSfCounters_t* dppSfCounters)                                   
 *  \brief Get Docsis packet processor service flow counters according to the index.
 *  \param[in,out] dppSfCounters - Docsis packet processor service flow counters structure.
 *  \return OK or error status.
 */
int DppCountersDb_GetSfCounters(DppSfCounters_t* dppSfCounters);

/*! \fn int DppCountersDb_PrintIfCounters(void)                                     
 *  \brief Print Docsis packet processor interface device counters.
 *  \param[in] no input.
 *  \param[out] no output.
 *  \return OK or error status.
 */
int DppCountersDb_PrintIfCounters(void);

/*! \fn int DppCountersDb_PrintTpPortCounters(void)                                     
 *  \brief Print Docsis packet processor transparent port counters.
 *  \param[in] no input.
 *  \param[out] no output.
 *  \return OK or error status.
 */
int DppCountersDb_PrintTpPortCounters(void);


#endif

