/*
 *
 * dpp_counters_db.c
 * Description:
 * Docsis Packet Processor counters database implementation
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

#define _DPP_COUNTERS_DB_C_

/*! \file dpp_counters_db.c
    \brief Docsis Packet Processor counters data base support
*/

/**************************************************************************/
/*      INCLUDES:                                                         */
/**************************************************************************/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/ti_hil.h>
#include <linux/netdevice.h>
#include <linux/ti_pp_path.h>
#include "dpp_ctl.h"
#include "dpp_counters_db.h"
#include "dpp_counters_ctl.h"
#include "dfilter.h"
#include "qos_classifier.h"
#include "pal.h"

/**************************************************************************/
/*      EXTERNS Declaration:                                              */
/**************************************************************************/

#ifdef DPP_LOG
extern unsigned int dppDbgLevel;
#endif
 
/**************************************************************************/
/*      DEFINES:                                                          */
/**************************************************************************/

#define DPP_MAX_ALL_INTERFACE  32

/*! \var typedef struct DppPortEntry DppPortEntry_t
    \brief Structure defines the format of the Docsis packet processor ports table entry.
*/
typedef struct DppPortEntry
{
    Uint32 portDiscardsCounter;
    DppCountSessionEntry_t *activeSessionsList;

} DppPortEntry_t;


/*! \var typedef struct DppCountersDataBase DppCountersDataBase_t
    \brief Structure contains information about the Docsis Packet Processor counters Data Base.
*/
typedef struct DppCountersDataBase
{
    DppPortEntry_t dppPortsTable[DPP_MAX_ALL_INTERFACE];
#ifdef CONFIG_MACH_PUMA5
    DppCountSessionEntry_t dppSessionsTable[ TI_PP_MAX_ACCLERABLE_SESSIONS ];
#else
    DppCountSessionEntry_t dppSessionsTable[ AVALANCHE_PP_MAX_ACCELERATED_SESSIONS ];
#endif

} DppCountersDataBase_t;


/**************************************************************************/
/*      LOCAL DECLARATIONS:                                               */
/**************************************************************************/

/* Start session functions */
static int DppCountersDb_CreateSessionForward(Uint32 sessionHandle, 
                                              FilterLLC_T* ti_match_llc_filter, 
                                              FilterIP_T* ti_match_inbound_ip_filter,
                                              FilterIP_T* ti_match_outbound_ip_filter,
                                              QosClassifier_t* ti_match_qos_classifier,
											  FilterDSG_T* ti_match_dsg_filter);

static int DppCountersDb_CreateSessionDiscard(Uint32 sessionHandle, Uint32 ingressPortId, 
                                              FilterLLC_T* ti_match_llc_filter, 
                                              FilterIP_T* ti_match_inbound_ip_filter,
                                              FilterIP_T* ti_match_outbound_ip_filter,
                                              QosClassifier_t* ti_match_qos_classifier);

static int DppCountersDb_AddLlcFilterInfo(Uint32 sessionHandle, FilterLLC_T* ti_match_llc_filter);
static int DppCountersDb_AddInIpFilterInfo(Uint32 sessionHandle, FilterIP_T* ti_match_inbound_ip_filter);
static int DppCountersDb_AddOutIpFilterInfo(Uint32 sessionHandle, FilterIP_T* ti_match_outbound_ip_filter);
static int DppCountersDb_AddQosClassInfo(Uint32 sessionHandle, QosClassifier_t* ti_match_qos_classifier);
static int DppCountersDb_AddIngressPortInfo(Uint32 sessionHandle, Uint32 ingressPortId);
static int DppCountersDb_AddDsgFilterInfo(Uint32 sessionHandle, FilterDSG_T* ti_match_dsg_filter);



/* Delete session functions */
static int DppCountersDb_DeleteSessionForward(Uint32 sessionHandle, Uint32 sessionPacketsFw, Uint64 sessionOctetsFw);
static int DppCountersDb_DeleteSessionDiscard(Uint32 sessionHandle, Uint32 sessionPacketsFw);
static int DppCountersDb_DeleteLlcFilterInfo(Uint32 sessionHandle, Uint32 sessionPacketsFw);
static int DppCountersDb_DeleteInIpFilterInfo(Uint32 sessionHandle, Uint32 sessionPacketsFw);
static int DppCountersDb_DeleteOutIpFilterInfo(Uint32 sessionHandle, Uint32 sessionPacketsFw);
static int DppCountersDb_DeleteQosClassInfo(Uint32 sessionHandle, Uint32 sessionPacketsFw);
static int DppCountersDb_DeleteIngressPortInfo(Uint32 sessionHandle, Uint32 sessionPacketsFw);
static int DppCountersDb_DeleteDsgFilterInfo(Uint32 sessionHandle, Uint32 sessionPacketsFw, Uint64 sessionOctetsFw);
/**************************************************************************/
/*      LOCAL VARIABLES:                                                  */
/**************************************************************************/

/*  structure which keeps track of all the information for the DOCSIS Bridge MDF. */
static DppCountersDataBase_t dpp_counters_db;

/**************************************************************************/
/*      INTERFACE FUNCTIONS Implementation:                               */
/**************************************************************************/

/**************************************************************************/
/*! \fn int DppCountersDb_Init(void)                                     
 **************************************************************************
 *  \brief Docsis Packet Processor counters data base initialization.
 *  \param[in] no input.
 *  \param[out] no output.
 *  \return OK or error status.
 **************************************************************************/
int DppCountersDb_Init(void)
{
	memset((void *)&dpp_counters_db, 0, sizeof(DppCountersDataBase_t));
    printk(KERN_INFO"\nInit Docsis Packet Processor counters DB done.\n");
	return 0;
}

/**************************************************************************/
/*! \fn int DppCountersDb_CreateSession(Uint32 sessionHandle, Uint8 sessionType, 
                                        Uint32 ingressPortId, 
                                        void* ti_match_llc_filter, 
                                        void* ti_match_inbound_ip_filter,
                                        void* ti_match_outbound_ip_filter,
                                        void* ti_match_qos_classifier)                                
 **************************************************************************
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
 **************************************************************************/
int DppCountersDb_CreateSession(Uint32 sessionHandle, Uint8 sessionType, 
                                Uint32 ingressPortId, 
                                void* ti_match_llc_filter, 
                                void* ti_match_inbound_ip_filter,
                                void* ti_match_outbound_ip_filter,
                                void* ti_match_qos_classifier,
								void* ti_match_dsg_filter)

{
    int res = 0;
    Uint32 lockKey = 0;

    PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);

    dpp_counters_db.dppSessionsTable[sessionHandle].sessionHandle = sessionHandle;

    switch (sessionType)
    {
    case TI_DOCSIS_PP_SESSION_TYPE_FORWARDING:
        res = DppCountersDb_CreateSessionForward(sessionHandle, 
                                                 (FilterLLC_T*)ti_match_llc_filter, 
                                                 (FilterIP_T*)ti_match_inbound_ip_filter,
                                                 (FilterIP_T*)ti_match_outbound_ip_filter,
                                                 (QosClassifier_t*)ti_match_qos_classifier,
												 (FilterDSG_T*)ti_match_dsg_filter);
        break;

    case TI_DOCSIS_PP_SESSION_TYPE_DISCARDING:
        res = DppCountersDb_CreateSessionDiscard(sessionHandle, 
                                                 ingressPortId,
                                                 (FilterLLC_T*)ti_match_llc_filter, 
                                                 (FilterIP_T*)ti_match_inbound_ip_filter,
                                                 (FilterIP_T*)ti_match_outbound_ip_filter,
                                                 (QosClassifier_t*)ti_match_qos_classifier);
        break;

    default:
        printk(KERN_ERR"\n%s: Unknown session type %d\n", __FUNCTION__, sessionType);
        PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
        return -EINVAL;
        break;
    }

    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);

#ifdef DPP_LOG
    if(dppDbgLevel & DPP_DBG_COUNTERS)
    {
        printk(KERN_INFO"\n%s: DOCSIS PP create session %u done.\n", __FUNCTION__, sessionHandle);
    }
#endif

    return res;
}

/**************************************************************************/
/*! \fn int DppCountersDb_DeleteSession(Uint32 sessionHandle, Uint32 sessionPacketsFw)                                  
 **************************************************************************
 *  \brief DOCSIS Packet Processor counters delete session.
 *   Increments the relevant counter(s) fed by the session and removes all existing 
 *   associations between the deleted session and all the clients it feeds 
 *   (filter(s), classifier, ingress port). 
 *  \param[in] sessionHandle - PP session handle number.
 *  \param[in] sessionPacketsFw - PP session forwarded packets number.
 *  \param[out] no output.
 *  \return OK or error status.
 **************************************************************************/
int DppCountersDb_DeleteSession(Uint32 sessionHandle, Uint32 sessionPacketsFw, Uint64 sessionOctetsFw)
{
    int res = 0;
    Uint32 lockKey;

    PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);

    if (dpp_counters_db.dppSessionsTable[sessionHandle].sessionHandle !=  sessionHandle)
    {
        printk(KERN_ERR"\n%s: DB counters session handle %u != session handle %u.\n", 
               __FUNCTION__, dpp_counters_db.dppSessionsTable[sessionHandle].sessionHandle, sessionHandle);
        PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
        return -1;
    }

    switch (dpp_counters_db.dppSessionsTable[sessionHandle].sessionType)
    {
    case TI_DOCSIS_PP_SESSION_TYPE_FORWARDING:
        res = DppCountersDb_DeleteSessionForward(sessionHandle, sessionPacketsFw, sessionOctetsFw);
        break;

    case TI_DOCSIS_PP_SESSION_TYPE_DISCARDING:
        res = DppCountersDb_DeleteSessionDiscard(sessionHandle, sessionPacketsFw);
        break;

    default:
        printk("\n%s: Unknown session type %d\n", __FUNCTION__, 
               dpp_counters_db.dppSessionsTable[sessionHandle].sessionType);
        PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
        return -EINVAL;
        break;
    }

    memset(&(dpp_counters_db.dppSessionsTable[sessionHandle]), 0, sizeof(DppCountSessionEntry_t));

    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);

#ifdef DPP_LOG
    if(dppDbgLevel & DPP_DBG_COUNTERS)
    {
        printk(KERN_INFO"\n%s: DOCSIS PP delete session %u done.\n", __FUNCTION__, sessionHandle);
    }
#endif

    return res;
}

/**************************************************************************/
/*! \fn int DppCountersDb_GetIfCounters(DppIfCounters_t* dppIfCounters)                                     
 **************************************************************************
 *  \brief Get Docsis packet processor interface device counters according to interface name.
 *  \param[in,out] dppIfCounters - Docsis packet processor interface device counters structure.
 **************************************************************************/
int DppCountersDb_GetIfCounters(DppIfCounters_t* dppIfCounters)
{
    Uint32 lockKey;
    struct net_device *cur_dev;
    PPP_counters_t pppCtrs;
    Uint64 var64;

    PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);

    cur_dev = dev_get_by_name(&init_net,dppIfCounters->ifName);
         
    if (!cur_dev)
    {
        printk("\n%s: Failed to get device for interface name %s\n", __FUNCTION__, dppIfCounters->ifName);
        PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);        
        return -1;
    }

#ifdef DPP_LOG
    if(dppDbgLevel & DPP_DBG_COUNTERS)
    {
        //printk("\n%s: Get VPID statistics for interface name %s, pid_handle %u, vpid_handle %u\n", 
        //       __FUNCTION__, dppIfCounters->ifName, cur_dev->pid_handle, cur_dev->vpid_handle);
    }
#endif

    if ((cur_dev->pid_handle < 0) || (cur_dev->vpid_handle < 0))
    {
        /* Get PATH counters */
        if (PPP_ReadCounters(dppIfCounters->ifName, &pppCtrs) == 0)
        {
            /* Sum all rx bytes */
            var64 = pppCtrs.inCtrs.unicastCtr.octets + pppCtrs.inCtrs.multicastCtr.octets;
#ifdef CONFIG_MACH_PUMA5
            dppIfCounters->ifCounters.rx_byte_hi = (var64 >> 32) & 0xFFFFFFFF;
            dppIfCounters->ifCounters.rx_byte_lo = var64 & 0xFFFFFFFF;
#else
            dppIfCounters->ifCounters.rx_byte = var64;
#endif
            dppIfCounters->ifCounters.rx_unicast_pkt = pppCtrs.inCtrs.unicastCtr.packets;
            dppIfCounters->ifCounters.rx_multicast_pkt = pppCtrs.inCtrs.multicastCtr.packets;

            /* Sum all tx bytes */
            var64 = pppCtrs.outCtrs.unicastCtr.octets + pppCtrs.outCtrs.multicastCtr.octets;
#ifdef CONFIG_MACH_PUMA5
            dppIfCounters->ifCounters.tx_byte_hi = (var64 >> 32) & 0xFFFFFFFF;
            dppIfCounters->ifCounters.tx_byte_lo = var64 & 0xFFFFFFFF;
#else
            dppIfCounters->ifCounters.tx_byte = var64;
#endif
            dppIfCounters->ifCounters.tx_unicast_pkt = pppCtrs.outCtrs.unicastCtr.packets;
            dppIfCounters->ifCounters.tx_multicast_pkt = pppCtrs.outCtrs.multicastCtr.packets;
        }

        PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
        dev_put(cur_dev);
        return 0;
    }
         
    /* Get the VPID statistics */
#ifdef CONFIG_MACH_PUMA5
    if (ti_ppm_get_vpid_stats(cur_dev->vpid_handle, &(dppIfCounters->ifCounters)) < 0) 
#else
    if (avalanche_pp_get_stats_vpid(cur_dev->vpid_handle, &(dppIfCounters->ifCounters)) != PP_RC_SUCCESS)
#endif
    {
        printk("\n%s: Failed to get VPID statistics for interface name %s, vpid_handle %u\n", __FUNCTION__, dppIfCounters->ifName, cur_dev->vpid_handle);
        PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
        dev_put(cur_dev);
        return -1;
    }

    /* Get QoS related discards */
    if (dppIfCounters->includeQosDrops)
    {
        int cluster;
        int queue;

        for ( cluster=0; cluster < cur_dev->vpid_block.qos_clusters_count; cluster++)
        {
            for( queue=0;  queue < cur_dev->vpid_block.qos_cluster[cluster]->qos_q_cnt; queue++)
            {
#ifdef CONFIG_MACH_PUMA5
                TI_PP_QOS_QUEUE_STATS stats;
                ti_ppm_get_qos_q_stats( cur_dev->vpid_block.qos_cluster[cluster]->qos_q_cfg[queue].q_num, &stats );
                dppIfCounters->ifCounters.tx_discard += stats.drp_cnt;
#else
                AVALANCHE_PP_QOS_QUEUE_STATS_t stats;
                avalanche_pp_qos_get_queue_stats( cur_dev->vpid_block.qos_cluster[cluster]->qos_q_cfg[queue].q_num, &stats );
                dppIfCounters->ifCounters.tx_discard_pkt += stats.drp_cnt;
#endif
            }
        }
    }


    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
    dev_put(cur_dev);
#ifdef DPP_LOG
    if(dppDbgLevel & DPP_DBG_COUNTERS)
    {
        //printk(KERN_INFO"\n%s: Get Docsis PP interface device %s counters done.\n", 
        //       __FUNCTION__, dppIfCounters->ifName);
    }
#endif

	return 0;
}

/**************************************************************************/
/*! \fn int DppCountersDb_GetSfCounters(DppSfCounters_t* dppSfCounters)
 **************************************************************************
 *  \brief Get Docsis packet processor service flow counters according to the index.
 *  \param[in,out] dppSfCounters - Docsis packet processor service flow counters structure.
 **************************************************************************/
int DppCountersDb_GetSfCounters(DppSfCounters_t* dppSfCounters)
{
    Uint32 lockKey;
    struct net_device *cur_dev;

    PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);

    cur_dev = dev_get_by_name(&init_net, dppSfCounters->ifName);
    
    if (!cur_dev)
    {
        printk("\n%s: Failed to get device for interface name %s\n", __FUNCTION__, dppSfCounters->ifName);
        PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
        return -1;
    }

    dppSfCounters->sfCounters.drp_cnt   = 0;
    dppSfCounters->sfCounters.fwd_pkts  = 0;

    if ((8 > dppSfCounters->sfIndex) && (cur_dev->vpid_block.qos_clusters_count > dppSfCounters->sfIndex))
    /* Get QoS related statistics */
    {
        int queue;

        for( queue=0;  queue < cur_dev->vpid_block.qos_cluster[dppSfCounters->sfIndex]->qos_q_cnt; queue++)
        {
#ifdef CONFIG_MACH_PUMA5
            TI_PP_QOS_QUEUE_STATS   stats;
#else
            AVALANCHE_PP_QOS_QUEUE_STATS_t stats;
#endif

            if (dppSfCounters->clearFlag)
            {
#ifdef CONFIG_MACH_PUMA5
                ti_ppm_get_n_clear_qos_q_stats( cur_dev->vpid_block.qos_cluster[dppSfCounters->sfIndex]->qos_q_cfg[queue].q_num, &stats );
#else
                // TBD - no clear function...
                avalanche_pp_qos_get_queue_stats( cur_dev->vpid_block.qos_cluster[dppSfCounters->sfIndex]->qos_q_cfg[queue].q_num, &stats );
#endif
            }
            else
            {
#ifdef CONFIG_MACH_PUMA5
                ti_ppm_get_qos_q_stats( cur_dev->vpid_block.qos_cluster[dppSfCounters->sfIndex]->qos_q_cfg[queue].q_num, &stats );
#else
                avalanche_pp_qos_get_queue_stats( cur_dev->vpid_block.qos_cluster[dppSfCounters->sfIndex]->qos_q_cfg[queue].q_num, &stats );
#endif
            }
            dppSfCounters->sfCounters.drp_cnt   += stats.drp_cnt;
            dppSfCounters->sfCounters.fwd_pkts  += stats.fwd_pkts;
        }
    }

    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
    dev_put(cur_dev);

	return 0;
}

/**************************************************************************/
/*! \fn int DppCountersDb_GetTpPortCounters(DppTpPortCounters_t* dppTpPortCounters)                                     
 **************************************************************************
 *  \brief Get Docsis packet processor transparent port counters according to interface name.
 *  \param[in,out] dppTpPortCounters - Docsis packet processor transparent port counters structure.
 **************************************************************************/
int DppCountersDb_GetTpPortCounters(DppTpPortCounters_t* dppTpPortCounters)
{
    Uint32 lockKey;
    struct net_device *cur_dev = NULL;
    DppPortEntry_t* dppPortEntry = NULL;
    DppCountSessionEntry_t *currSessionEntry = NULL;
#ifdef CONFIG_MACH_PUMA5
    TI_PP_VPID_STATS ppVpidStats;
    TI_PP_SESSION_STATS sessionStats;
#else
    AVALANCHE_PP_VPID_STATS_t ppVpidStats;
    AVALANCHE_PP_SESSION_STATS_t sessionStats;
#endif
    
    Uint32 deadSessionsDiscards = 0;
    Uint32 currSessionsDiscards = 0;

    memset(&ppVpidStats, 0, sizeof(ppVpidStats));
    memset(&sessionStats, 0, sizeof(sessionStats));

    PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);

    cur_dev = dev_get_by_name(&init_net, dppTpPortCounters->ifName);
         
    if (!cur_dev)
    {
        printk("\n%s: Failed to get device for interface name %s\n", __FUNCTION__, dppTpPortCounters->ifName);
        PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
        return -1;
    }

#ifdef DPP_LOG
    if(dppDbgLevel & DPP_DBG_COUNTERS)
    {
        //printk("\n%s: Get VPID statistics for interface name %s, pid_handle %u, vpid_handle %u\n", 
        //       __FUNCTION__, dppTpPortCounters->ifName, cur_dev->pid_handle, cur_dev->vpid_handle);
    }
#endif

    if ((cur_dev->pid_handle < 0) || (cur_dev->vpid_handle < 0))
    {
        PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
        dev_put(cur_dev);
        return 0;
    }
         
    /* get the VPID statistics */
#ifdef CONFIG_MACH_PUMA5
    if (ti_ppm_get_vpid_stats(cur_dev->vpid_handle ,&ppVpidStats) < 0)
#else
    if (avalanche_pp_get_stats_vpid(cur_dev->vpid_handle, &ppVpidStats) != PP_RC_SUCCESS)
#endif    
    {
        printk("\n%s: Failed to get VPID statistics for interface name %s, vpid_handle %u\n", __FUNCTION__, dppTpPortCounters->ifName, cur_dev->vpid_handle);
        PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
        dev_put(cur_dev);
        return -1;
    }

    /* get in Frames */
    dppTpPortCounters->inFrames = ppVpidStats.rx_broadcast_pkt + 
                                  ppVpidStats.rx_multicast_pkt + 
                                  ppVpidStats.rx_unicast_pkt;

    /* get out Frames */
    dppTpPortCounters->outFrames = ppVpidStats.tx_broadcast_pkt + 
                                   ppVpidStats.tx_multicast_pkt + 
                                   ppVpidStats.tx_unicast_pkt;

    /* get discrad Frames of dead sessions */
    dppPortEntry = &(dpp_counters_db.dppPortsTable[cur_dev->ifindex]);
    deadSessionsDiscards = dppPortEntry->portDiscardsCounter;

#ifdef DPP_LOG
    if(dppDbgLevel & DPP_DBG_COUNTERS)
    {
        //printk(KERN_INFO"\n%s: port [%u]: discrad Frames of dead sessions = %u\n", 
        //       __FUNCTION__, cur_dev->ifindex, deadSessionsDiscards);
    }
#endif

    /* get discrad Frames of current sessions */
    if (dppPortEntry->activeSessionsList != NULL)
    {
        currSessionEntry = dppPortEntry->activeSessionsList;

        while (currSessionEntry != NULL)
        {
#ifdef CONFIG_MACH_PUMA5
            if (ti_ppm_get_session_stats(currSessionEntry->sessionHandle, &sessionStats) < 0)
#else
            if (avalanche_pp_get_stats_session(currSessionEntry->sessionHandle, &sessionStats) != PP_RC_SUCCESS)
#endif
            {
                printk(KERN_ERR"Failed to get session %d statistics for dev ifindex %d\n", currSessionEntry->sessionHandle, cur_dev->ifindex);
                PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
                dev_put(cur_dev);
                return -EINVAL;
            }

            currSessionsDiscards += sessionStats.packets_forwarded;
            currSessionEntry = currSessionEntry->ingressPortInfo.nextSession;
        }

        #ifdef DPP_LOG
        if(dppDbgLevel & DPP_DBG_COUNTERS)
        {
            printk(KERN_INFO"\n%s: port [%u]: discrad Frames of current sessions = %u\n", 
                   __FUNCTION__, cur_dev->ifindex, currSessionsDiscards);
        }
        #endif
    }

    dppTpPortCounters->discardFrames = deadSessionsDiscards + currSessionsDiscards;

    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
    dev_put(cur_dev);
#ifdef DPP_LOG
    if(dppDbgLevel & DPP_DBG_COUNTERS)
    {
        /*
        printk(KERN_INFO"\n%s: port [%u]: inFrames = %u\n", 
               __FUNCTION__, cur_dev->ifindex, dppTpPortCounters->inFrames);
        printk(KERN_INFO"\n%s: port [%u]: outFrames = %u\n", 
               __FUNCTION__, cur_dev->ifindex, dppTpPortCounters->outFrames);
        printk(KERN_INFO"\n%s: port [%u]: discrad Frames = %u\n", 
               __FUNCTION__, cur_dev->ifindex, dppTpPortCounters->discardFrames);
        printk(KERN_INFO"\n%s: Get Docsis PP transparent port device %s counters done.\n", 
               __FUNCTION__, dppTpPortCounters->ifName);*/
    }
#endif

	return 0;
}

/**************************************************************************/
/*! \fn int DppCountersDb_PrintIfCounters(void)                                 
 **************************************************************************
 *  \brief Print Docsis packet processor interface device counters.
 *  \param[in] no input.
 *  \param[out] no output.
 *  \return OK or error status.
 **************************************************************************/
int DppCountersDb_PrintIfCounters(void)
{
	Uint32 index = 0;
    struct net_device *cur_dev = NULL;
    DppIfCounters_t dppIfCounters[DPP_MAX_ALL_INTERFACE];
    memset(dppIfCounters, 0, sizeof(dppIfCounters));

	printk("\n---------------------------");
	printk("\n PP Interface dev counters ");
	printk("\n---------------------------");
#ifdef CONFIG_MACH_PUMA5
    printk("\nRx:\tIndex\tName\tUnicast\tBroadcast\tMulticast\tDiscard\tBytesHigh\tBytesLow");
    printk("\n---\t-----\t----\t-------\t---------\t---------\t-------\t---------\t--------");
#else
    printk("\nRx:\tIndex\tName\tUnicast\tBroadcast\tMulticast\tDiscard\tBytes");
    printk("\n---\t-----\t----\t-------\t---------\t---------\t-------\t-----");
#endif

    for (index = 0; index < DPP_MAX_ALL_INTERFACE; index++)
    {    
        cur_dev = dev_get_by_index(&init_net,index);

        if (cur_dev)
		{      
            memcpy(dppIfCounters[index].ifName, cur_dev->name, DPP_IF_MANE_MAX_LENGTH);

            if (!DppCountersDb_GetIfCounters(&dppIfCounters[index]))
            {
#ifdef CONFIG_MACH_PUMA5
                printk("\n\t%d\t%s\t%u\t%u\t\t%u\t\t%u\t%u\t\t%u", 
                       index, cur_dev->name,
                       dppIfCounters[index].ifCounters.rx_unicast_pkt, 
                       dppIfCounters[index].ifCounters.rx_broadcast_pkt, 
                       dppIfCounters[index].ifCounters.rx_multicast_pkt, 
                       dppIfCounters[index].ifCounters.rx_discard,
                       dppIfCounters[index].ifCounters.rx_byte_hi,
                       dppIfCounters[index].ifCounters.rx_byte_lo);
#else
                printk("\n\t%d\t%s\t%u\t%u\t\t%u\t\t%u\t%llu", 
                       index, cur_dev->name,
                       dppIfCounters[index].ifCounters.rx_unicast_pkt, 
                       dppIfCounters[index].ifCounters.rx_broadcast_pkt, 
                       dppIfCounters[index].ifCounters.rx_multicast_pkt, 
                       dppIfCounters[index].ifCounters.rx_discard_pkt,
                       dppIfCounters[index].ifCounters.rx_byte);
#endif
            }
            dev_put(cur_dev);
        }
    }

    printk("\n");
#ifdef CONFIG_MACH_PUMA5
    printk("\nTx:\tIndex\tName\tUnicast\tBroadcast\tMulticast\tDiscard\tErrors\tBytesHigh\tBytesLow");
    printk("\n---\t-----\t----\t-------\t---------\t---------\t-------\t------\t---------\t--------");
#else
    printk("\nTx:\tIndex\tName\tUnicast\tBroadcast\tMulticast\tDiscard\tErrors\tBytes");
    printk("\n---\t-----\t----\t-------\t---------\t---------\t-------\t------\t-----");
#endif

    for (index = 0; index < DPP_MAX_ALL_INTERFACE; index++)
    {    
        cur_dev = dev_get_by_index(&init_net, index);

        if (cur_dev)
		{      
#ifdef CONFIG_MACH_PUMA5
            printk("\n\t%d\t%s\t%u\t%u\t\t%u\t\t%u\t%u\t%u\t\t%u", 
                   index, cur_dev->name,
                   dppIfCounters[index].ifCounters.tx_unicast_pkt, 
                   dppIfCounters[index].ifCounters.tx_broadcast_pkt, 
                   dppIfCounters[index].ifCounters.tx_multicast_pkt, 
                   dppIfCounters[index].ifCounters.tx_discard,
                   dppIfCounters[index].ifCounters.tx_error,
                   dppIfCounters[index].ifCounters.tx_byte_hi,
                   dppIfCounters[index].ifCounters.tx_byte_lo);
#else
            printk("\n\t%d\t%s\t%u\t%u\t\t%u\t\t%u\t%u\t%llu", 
                   index, cur_dev->name,
                   dppIfCounters[index].ifCounters.tx_unicast_pkt, 
                   dppIfCounters[index].ifCounters.tx_broadcast_pkt, 
                   dppIfCounters[index].ifCounters.tx_multicast_pkt, 
                   dppIfCounters[index].ifCounters.tx_discard_pkt,
                   dppIfCounters[index].ifCounters.tx_error,
                   dppIfCounters[index].ifCounters.tx_byte);
#endif
            dev_put(cur_dev);
        }
    }
         
	printk("\n");
	printk("\n");
    return 0;
}

/**************************************************************************/
/*! \fn int DppCountersDb_PrintTpPortCounters(void)                                 
 **************************************************************************
 *  \brief Print Docsis packet processor transparent port counters.
 *  \param[in] no input.
 *  \param[out] no output.
 *  \return OK or error status.
 **************************************************************************/
int DppCountersDb_PrintTpPortCounters(void)
{
	Uint32 index = 0;
    struct net_device *cur_dev = NULL;
    DppTpPortCounters_t dppTpPortCounters;

	printk("\n--------------------------------------");
	printk("\n PP Transparent Ports frames counters ");
	printk("\n--------------------------------------");
	printk("\nIndex\tName\tIn\tOut\tDiscard");
	printk("\n-----\t----\t--\t---\t-------");

	for (index = 0; index < DPP_MAX_ALL_INTERFACE; index++)
	{
        cur_dev = dev_get_by_index(&init_net, index);

        if (cur_dev)
		{      
            memset(&dppTpPortCounters, 0, sizeof(dppTpPortCounters));
            memcpy(dppTpPortCounters.ifName, cur_dev->name, DPP_IF_MANE_MAX_LENGTH);

            if (!DppCountersDb_GetTpPortCounters(&dppTpPortCounters))
            {
                printk("\n%d\t%s\t%u\t%u\t%u", index, cur_dev->name, dppTpPortCounters.inFrames, 
                       dppTpPortCounters.outFrames, dppTpPortCounters.discardFrames);
            }
            dev_put(cur_dev);
		}
	}

	printk("\n");
	printk("\n");
	return 0;
}

/**************************************************************************/
/*      LOCAL FUNCTIONS:                                                  */
/**************************************************************************/


/**************************************************************************/
/*! \fn static int DppCountersDb_CreateSessionForward(Uint32 sessionHandle, 
                                                      void* ti_match_llc_filter, 
                                                      void* ti_match_inbound_ip_filter,
                                                      void* ti_match_outbound_ip_filter,
                                                      void* ti_match_qos_classifier)                                
 **************************************************************************
 *  \brief DOCSIS Packet Processor counters create forwaring session.
 *  \param[in] sessionHandle - PP session handle number.
 *  \param[in] ti_match_llc_filter - pointer to LLC filter.
 *  \param[in] ti_match_inbound_ip_filter - pointer to inbound IP filter.
 *  \param[in] ti_match_outbound_ip_filter - pointer to outbound IP filter.
 *  \param[in] ti_match_qos_classifier - pointer to QoS classifier.
 *  \param[out] no output.
 *  \return OK or error status.
 **************************************************************************/
static int DppCountersDb_CreateSessionForward(Uint32 sessionHandle, 
                                              FilterLLC_T* ti_match_llc_filter, 
                                              FilterIP_T* ti_match_inbound_ip_filter,
                                              FilterIP_T* ti_match_outbound_ip_filter,
                                              QosClassifier_t* ti_match_qos_classifier,
											  FilterDSG_T* ti_match_dsg_filter)
{   
    /* LLC filter Info */
    if (ti_match_llc_filter)
    {   
        DppCountersDb_AddLlcFilterInfo(sessionHandle, ti_match_llc_filter);
    }

    /* inbound IP filter Info */
    if (ti_match_inbound_ip_filter)
    {
        DppCountersDb_AddInIpFilterInfo(sessionHandle, ti_match_inbound_ip_filter);
    }

    /* outbound IP filter Info */
    if (ti_match_outbound_ip_filter)
    {
        DppCountersDb_AddOutIpFilterInfo(sessionHandle, ti_match_outbound_ip_filter);
    }

    /* QOS Classifier Info */
    if (ti_match_qos_classifier)
    {
        DppCountersDb_AddQosClassInfo(sessionHandle, ti_match_qos_classifier);
    }
	
	/* DSG Filter Info */
	if (ti_match_dsg_filter)
    {
        DppCountersDb_AddDsgFilterInfo(sessionHandle, ti_match_dsg_filter);
    }

    dpp_counters_db.dppSessionsTable[sessionHandle].sessionType = TI_DOCSIS_PP_SESSION_TYPE_FORWARDING;

#ifdef DPP_LOG
    if(dppDbgLevel & DPP_DBG_COUNTERS)
    {
        printk(KERN_INFO"\n%s: DOCSIS PP counters create forwaring session %u done.\n", 
               __FUNCTION__, sessionHandle);
    }
#endif

	return 0;
}

/**************************************************************************/
/*! \fn static int DppCountersDb_CreateSessionDiscard(Uint32 sessionHandle, Uint32 ingressPortId,
                                                      QosClassifier_t* ti_match_qos_classifier)                                   
 **************************************************************************
 *  \brief DOCSIS Packet Processor counters create discard session.
 *  \param[in] sessionHandle - PP session handle number.
 *  \param[in] ingressPortId - input device interface index.
 *  \param[in] ti_match_llc_filter - pointer to LLC filter.
 *  \param[in] ti_match_inbound_ip_filter - pointer to inbound IP filter.
 *  \param[in] ti_match_outbound_ip_filter - pointer to outbound IP filter.
 *  \param[in] ti_match_qos_classifier - pointer to QoS classifier.
 *  \param[out] no output.
 *  \return OK or error status.
 **************************************************************************/
static int DppCountersDb_CreateSessionDiscard(Uint32 sessionHandle, Uint32 ingressPortId, 
                                              FilterLLC_T* ti_match_llc_filter, 
                                              FilterIP_T* ti_match_inbound_ip_filter,
                                              FilterIP_T* ti_match_outbound_ip_filter,
                                              QosClassifier_t* ti_match_qos_classifier)
{
    /* Ingress port Info */
    DppCountersDb_AddIngressPortInfo(sessionHandle, ingressPortId);

    /* QOS Classifier Info */
    if (ti_match_qos_classifier)
    {
        DppCountersDb_AddQosClassInfo(sessionHandle, ti_match_qos_classifier);
    }

    /* LLC filter Info */
    if (ti_match_llc_filter)
    {   
        DppCountersDb_AddLlcFilterInfo(sessionHandle, ti_match_llc_filter);
    }

    /* inbound IP filter Info */
    if (ti_match_inbound_ip_filter)
    {
        DppCountersDb_AddInIpFilterInfo(sessionHandle, ti_match_inbound_ip_filter);
    }

    /* outbound IP filter Info */
    if (ti_match_outbound_ip_filter)
    {
        DppCountersDb_AddOutIpFilterInfo(sessionHandle, ti_match_outbound_ip_filter);
    }

    dpp_counters_db.dppSessionsTable[sessionHandle].sessionType = TI_DOCSIS_PP_SESSION_TYPE_DISCARDING;

#ifdef DPP_LOG
    if(dppDbgLevel & DPP_DBG_COUNTERS)
    {
        printk(KERN_INFO"\n%s: DOCSIS PP counters create discard session %u ingressPortId %u done.\n", 
               __FUNCTION__, sessionHandle, ingressPortId);
    }
#endif

	return 0;
}



/**************************************************************************/
/*! \fn static int DppCountersDb_AddDsgFilterInfo(Uint32 sessionHandle, FilterDSG_T* ti_match_dsg_filter)                                   
 **************************************************************************
 *  \brief Add matched DSG filter info to sessions table according to session handle.
 *  \param[in] sessionHandle - PP session handle number.
 *  \param[out] no output.
 *  \return OK or error status.
 **************************************************************************/
static int DppCountersDb_AddDsgFilterInfo(Uint32 sessionHandle, FilterDSG_T* ti_match_dsg_filter)
{
    dpp_counters_db.dppSessionsTable[sessionHandle].dsgFilterInfo.clientPtr = ti_match_dsg_filter;

    dpp_counters_db.dppSessionsTable[sessionHandle].dsgFilterInfo.nextSession = 
        ti_match_dsg_filter->DSGPpCounters.activeSessionsList;
    
    ti_match_dsg_filter->DSGPpCounters.activeSessionsList = &(dpp_counters_db.dppSessionsTable[sessionHandle]);

#ifdef DPP_LOG
    if(dppDbgLevel & DPP_DBG_COUNTERS)
    {
        printk(KERN_INFO"\n%s: Add matched DSG filter info DSGIndex %u of session %u done.\n", 
               __FUNCTION__, ti_match_dsg_filter->DSGIndex, sessionHandle);
    }
#endif

    return 0;
}





/**************************************************************************/
/*! \fn static int DppCountersDb_AddLlcFilterInfo(Uint32 sessionHandle, FilterLLC_T* ti_match_llc_filter)                                   
 **************************************************************************
 *  \brief Add matched LLC filter info to sessions table according to session handle.
 *  \param[in] sessionHandle - PP session handle number.
 *  \param[out] no output.
 *  \return OK or error status.
 **************************************************************************/
static int DppCountersDb_AddLlcFilterInfo(Uint32 sessionHandle, FilterLLC_T* ti_match_llc_filter)
{
    dpp_counters_db.dppSessionsTable[sessionHandle].llcFilterInfo.clientPtr = ti_match_llc_filter;

    dpp_counters_db.dppSessionsTable[sessionHandle].llcFilterInfo.nextSession = 
        ti_match_llc_filter->LLCPpCounters.activeSessionsList;
    
    ti_match_llc_filter->LLCPpCounters.activeSessionsList = &(dpp_counters_db.dppSessionsTable[sessionHandle]);

#ifdef DPP_LOG
    if(dppDbgLevel & DPP_DBG_COUNTERS)
    {
        printk(KERN_INFO"\n%s: Add matched LLC filter info LLCIndex %u of session %u done.\n", 
               __FUNCTION__, ti_match_llc_filter->LLCIndex, sessionHandle);
    }
#endif

    return 0;
}

/**************************************************************************/
/*! \fn static int DppCountersDb_AddInIpFilterInfo(Uint32 sessionHandle, FilterIP_T* ti_match_inbound_ip_filter)                              
 **************************************************************************
 *  \brief Add matched inbuond IP filter info to sessions table according to session handle.
 *  \param[in] sessionHandle - PP session handle number.
 *  \param[out] no output.
 *  \return OK or error status.
 **************************************************************************/
static int DppCountersDb_AddInIpFilterInfo(Uint32 sessionHandle, FilterIP_T* ti_match_inbound_ip_filter)
{
    dpp_counters_db.dppSessionsTable[sessionHandle].inIpFilterInfo.clientPtr = ti_match_inbound_ip_filter;

    dpp_counters_db.dppSessionsTable[sessionHandle].inIpFilterInfo.nextSession = 
        ti_match_inbound_ip_filter->IPInPpCounters.activeSessionsList;
    
    ti_match_inbound_ip_filter->IPInPpCounters.activeSessionsList = &(dpp_counters_db.dppSessionsTable[sessionHandle]);

#ifdef DPP_LOG
    if(dppDbgLevel & DPP_DBG_COUNTERS)
    {
        printk(KERN_INFO"\n%s: Add matched inbuond IP filter info IPIndex %u of session %u done.\n", 
               __FUNCTION__, ti_match_inbound_ip_filter->IPIndex, sessionHandle);
    }
#endif

    return 0;
}

/**************************************************************************/
/*! \fn static int DppCountersDb_AddOutIpFilterInfo(Uint32 sessionHandle, FilterIP_T* ti_match_outbound_ip_filter)
 **************************************************************************
 *  \brief Add matched outbound IP filter info to sessions table according to session handle.
 *  \param[in] sessionHandle - PP session handle number.
 *  \param[out] no output.
 *  \return OK or error status.
 **************************************************************************/
static int DppCountersDb_AddOutIpFilterInfo(Uint32 sessionHandle, FilterIP_T* ti_match_outbound_ip_filter)
{
    dpp_counters_db.dppSessionsTable[sessionHandle].outIpFilterInfo.clientPtr = ti_match_outbound_ip_filter;

    dpp_counters_db.dppSessionsTable[sessionHandle].outIpFilterInfo.nextSession = 
        ti_match_outbound_ip_filter->IPOutPpCounters.activeSessionsList;
    
    ti_match_outbound_ip_filter->IPOutPpCounters.activeSessionsList = &(dpp_counters_db.dppSessionsTable[sessionHandle]);

#ifdef DPP_LOG
    if(dppDbgLevel & DPP_DBG_COUNTERS)
    {
        printk(KERN_INFO"\n%s: Add matched outbound IP filter info IPIndex %u of session %u done.\n", 
               __FUNCTION__, ti_match_outbound_ip_filter->IPIndex, sessionHandle);
    }
#endif

    return 0;
}

/**************************************************************************/
/*! \fn static int DppCountersDb_AddQosClassInfo(Uint32 sessionHandle, QosClassifier_t* ti_match_qos_classifier)                                  
 **************************************************************************
 *  \brief Add matched classifier info to sessions table according to session handle.
 *  \param[in] sessionHandle - PP session handle number.
 *  \param[out] no output.
 *  \return OK or error status.
 **************************************************************************/
static int DppCountersDb_AddQosClassInfo(Uint32 sessionHandle, QosClassifier_t* ti_match_qos_classifier)
{
    dpp_counters_db.dppSessionsTable[sessionHandle].qosClassInfo.clientPtr = ti_match_qos_classifier;

    dpp_counters_db.dppSessionsTable[sessionHandle].qosClassInfo.nextSession = 
        ti_match_qos_classifier->ppCounters.activeSessionsList;

    ti_match_qos_classifier->ppCounters.activeSessionsList = &(dpp_counters_db.dppSessionsTable[sessionHandle]);

#ifdef DPP_LOG
    if(dppDbgLevel & DPP_DBG_COUNTERS)
    {
        printk(KERN_INFO"\n%s: Add matched classifier info classID %u sfID %u of session %u done.\n", 
               __FUNCTION__, ti_match_qos_classifier->classID, ti_match_qos_classifier->sfID, sessionHandle);
    }
#endif

    return 0;
}

/**************************************************************************/
/*! \fn static int DppCountersDb_AddIngressPortInfo(Uint32 sessionHandle, Uint32 ingressPortId)
 **************************************************************************
 *  \brief Add matched ingress port info to sessions table according to session handle.
 *  \param[in] sessionHandle - PP session handle number.
 *  \param[in] ingressPortId - input device interface index.
 *  \param[out] no output.
 *  \return OK or error status.
 **************************************************************************/
static int DppCountersDb_AddIngressPortInfo(Uint32 sessionHandle, Uint32 ingressPortId)
{
    dpp_counters_db.dppSessionsTable[sessionHandle].ingressPortInfo.clientPtr = 
        &(dpp_counters_db.dppPortsTable[ingressPortId]);

    dpp_counters_db.dppSessionsTable[sessionHandle].ingressPortInfo.nextSession = 
        dpp_counters_db.dppPortsTable[ingressPortId].activeSessionsList;
    
    dpp_counters_db.dppPortsTable[ingressPortId].activeSessionsList = 
        &(dpp_counters_db.dppSessionsTable[sessionHandle]);

#ifdef DPP_LOG
    if(dppDbgLevel & DPP_DBG_COUNTERS)
    {
        printk(KERN_INFO"\n%s: Add matched ingress port %u info of session %u done.\n", 
               __FUNCTION__, ingressPortId, sessionHandle);
    }
#endif

    return 0;
}

/**************************************************************************/
/*! \fn static int DppCountersDb_DeleteSessionForward(Uint32 sessionHandle, Uint32 sessionPacketsFw, Uint64 sessionOctetsFw))                               
 **************************************************************************
 *  \brief DOCSIS Packet Processor counters delete forwaring session.
 *  \param[in] sessionHandle - PP session handle number.
 *  \param[in] sessionPacketsFw - PP session forwarded packets number.
  *  \param[in] sessionOctetsFw - PP session forwarded bytes number.
 *  \param[out] no output.
 *  \return OK or error status.
 **************************************************************************/
static int DppCountersDb_DeleteSessionForward(Uint32 sessionHandle, Uint32 sessionPacketsFw, Uint64 sessionOctetsFw)
{
    if (dpp_counters_db.dppSessionsTable[sessionHandle].llcFilterInfo.clientPtr)
    {
        DppCountersDb_DeleteLlcFilterInfo(sessionHandle, sessionPacketsFw);
    }

    if (dpp_counters_db.dppSessionsTable[sessionHandle].inIpFilterInfo.clientPtr)
    {
        DppCountersDb_DeleteInIpFilterInfo(sessionHandle, sessionPacketsFw);
    }

    if (dpp_counters_db.dppSessionsTable[sessionHandle].outIpFilterInfo.clientPtr)
    {
        DppCountersDb_DeleteOutIpFilterInfo(sessionHandle, sessionPacketsFw);
    }

    if (dpp_counters_db.dppSessionsTable[sessionHandle].qosClassInfo.clientPtr)
    {
        DppCountersDb_DeleteQosClassInfo(sessionHandle, sessionPacketsFw);
    }
	if (dpp_counters_db.dppSessionsTable[sessionHandle].dsgFilterInfo.clientPtr)
    {
        DppCountersDb_DeleteDsgFilterInfo(sessionHandle, sessionPacketsFw, sessionOctetsFw);
    }

#ifdef DPP_LOG
    if(dppDbgLevel & DPP_DBG_COUNTERS)
    {
        printk(KERN_INFO"\n%s: DOCSIS PP counters delete forwaring session %u sessionPacketsFw %u done.\n", 
               __FUNCTION__, sessionHandle, sessionPacketsFw);
    }
#endif

    return 0;
}

/**************************************************************************/
/*! \fn static int DppCountersDb_DeleteSessionDiscard(Uint32 sessionHandle, Uint32 sessionPacketsFw)                                   
 **************************************************************************
 *  \brief DOCSIS Packet Processor counters delete discard session.
 *  \param[in] sessionHandle - PP session handle number.
 *  \param[in] sessionPacketsFw - PP session forwarded packets number.
 *  \param[out] no output.
 *  \return OK or error status.
 **************************************************************************/
static int DppCountersDb_DeleteSessionDiscard(Uint32 sessionHandle, Uint32 sessionPacketsFw)
{
    if (dpp_counters_db.dppSessionsTable[sessionHandle].ingressPortInfo.clientPtr)
    {
        DppCountersDb_DeleteIngressPortInfo(sessionHandle, sessionPacketsFw);
    }

    if (dpp_counters_db.dppSessionsTable[sessionHandle].qosClassInfo.clientPtr)
    {
        DppCountersDb_DeleteQosClassInfo(sessionHandle, sessionPacketsFw);
    }

    if (dpp_counters_db.dppSessionsTable[sessionHandle].llcFilterInfo.clientPtr)
    {
        DppCountersDb_DeleteLlcFilterInfo(sessionHandle, sessionPacketsFw);
    }

    if (dpp_counters_db.dppSessionsTable[sessionHandle].inIpFilterInfo.clientPtr)
    {
        DppCountersDb_DeleteInIpFilterInfo(sessionHandle, sessionPacketsFw);
    }

    if (dpp_counters_db.dppSessionsTable[sessionHandle].outIpFilterInfo.clientPtr)
    {
        DppCountersDb_DeleteOutIpFilterInfo(sessionHandle, sessionPacketsFw);
    }
   
#ifdef DPP_LOG
    if(dppDbgLevel & DPP_DBG_COUNTERS)
    {
        printk(KERN_INFO"\n%s: DOCSIS PP counters delete discard session %u sessionPacketsFw %u done.\n", 
               __FUNCTION__, sessionHandle, sessionPacketsFw);
    }
#endif

    return 0;
}

/**************************************************************************/
/*! \fn static int DppCountersDb_DeleteDsgFilterInfo(Uint32 sessionHandle, Uint32 sessionPacketsFw, Uint64 sessionOctetsFw)                                  
 **************************************************************************
 *  \brief Delete matched DSG filter info in sessions table according to session handle.
 *  \param[in] sessionHandle - PP session handle number.
 *  \param[in] sessionPacketsFw - PP session forwarded packets number.
 *  \param[in] sessionOctetsFw - PP session forwarded bytes number.
 *  \param[out] no output.
 *  \return OK or error status.
 **************************************************************************/
static int DppCountersDb_DeleteDsgFilterInfo(Uint32 sessionHandle, Uint32 sessionPacketsFw, Uint64 sessionOctetsFw)
{
	DppCountSessionEntry_t *currentSession = NULL;

    FilterDSG_T* ti_match_dsg_filter = 
        (FilterDSG_T*)(dpp_counters_db.dppSessionsTable[sessionHandle].dsgFilterInfo.clientPtr);

    DppCountSessionEntry_t *activeSessionsList = ti_match_dsg_filter->DSGPpCounters.activeSessionsList;

    if (activeSessionsList == NULL)
    {
        printk(KERN_ERR"\n%s: No active sessions for DSG filter DSGIndex = %u\n", 
               __FUNCTION__, ti_match_dsg_filter->DSGIndex);
        return -1;
    }

    /* add session's forwarded packets number to the filter counter  */
    ti_match_dsg_filter->DSGPpCounters.ppMatches += sessionPacketsFw;
	ti_match_dsg_filter->DSGPpCounters.ppBytes += sessionOctetsFw;

#ifdef DPP_LOG
    if(dppDbgLevel & DPP_DBG_COUNTERS)
    {
        printk(KERN_INFO"\n%s: ppMatches = %u\n", __FUNCTION__, 
               ti_match_dsg_filter->DSGPpCounters.ppMatches);
    }
#endif

    /* First session for this DSG filter */
    if (activeSessionsList->sessionHandle == sessionHandle)
    {
        ti_match_dsg_filter->DSGPpCounters.activeSessionsList = activeSessionsList->dsgFilterInfo.nextSession;
    }

    /* Not First session for this DSG filter */
    else
    {   
        currentSession = ti_match_dsg_filter->DSGPpCounters.activeSessionsList;

        while ((currentSession->dsgFilterInfo.nextSession != NULL) &&
               (currentSession->dsgFilterInfo.nextSession->sessionHandle != sessionHandle))
        {
            currentSession = currentSession->dsgFilterInfo.nextSession;
        }
    
        if (currentSession->dsgFilterInfo.nextSession == NULL)
        {
            printk(KERN_ERR"\n%s: session handle %u not found in DSG filter sessions list, DSGIndex = %u\n", 
                   __FUNCTION__, sessionHandle, ti_match_dsg_filter->DSGIndex);
            return -1;
        }
    
        currentSession->dsgFilterInfo.nextSession = 
            dpp_counters_db.dppSessionsTable[sessionHandle].dsgFilterInfo.nextSession;
    }
    
    dpp_counters_db.dppSessionsTable[sessionHandle].dsgFilterInfo.clientPtr = NULL;
    dpp_counters_db.dppSessionsTable[sessionHandle].dsgFilterInfo.nextSession = NULL;

#ifdef DPP_LOG
    if(dppDbgLevel & DPP_DBG_COUNTERS)
    {
        printk(KERN_INFO"\n%s: Delete matched DSG filter info DSGIndex %u of session %u done.\n", 
               __FUNCTION__, ti_match_dsg_filter->DSGIndex, sessionHandle);
    }
#endif

    return 0;

}

/**************************************************************************/
/*! \fn static int DppCountersDb_DeleteLlcFilterInfo(Uint32 sessionHandle, Uint32 sessionPacketsFw)                                  
 **************************************************************************
 *  \brief Delete matched LLC filter info in sessions table according to session handle.
 *  \param[in] sessionHandle - PP session handle number.
 *  \param[in] sessionPacketsFw - PP session forwarded packets number.
 *  \param[out] no output.
 *  \return OK or error status.
 **************************************************************************/
static int DppCountersDb_DeleteLlcFilterInfo(Uint32 sessionHandle, Uint32 sessionPacketsFw)
{
    DppCountSessionEntry_t *currentSession = NULL;

    FilterLLC_T* ti_match_llc_filter = 
        (FilterLLC_T*)(dpp_counters_db.dppSessionsTable[sessionHandle].llcFilterInfo.clientPtr);

    DppCountSessionEntry_t *activeSessionsList = ti_match_llc_filter->LLCPpCounters.activeSessionsList;

    if (activeSessionsList == NULL)
    {
        printk(KERN_ERR"\n%s: No active sessions for LLC filter LLCIndex = %u\n", 
               __FUNCTION__, ti_match_llc_filter->LLCIndex);
        return -1;
    }

    /* add session's forwarded packets number to the filter counter  */
    ti_match_llc_filter->LLCPpCounters.ppMatches += sessionPacketsFw;

#ifdef DPP_LOG
    if(dppDbgLevel & DPP_DBG_COUNTERS)
    {
        printk(KERN_INFO"\n%s: ppMatches = %u\n", __FUNCTION__, 
               ti_match_llc_filter->LLCPpCounters.ppMatches);
    }
#endif

    /* First session for this LLC filter */
    if (activeSessionsList->sessionHandle == sessionHandle)
    {
        ti_match_llc_filter->LLCPpCounters.activeSessionsList = activeSessionsList->llcFilterInfo.nextSession;
    }

    /* Not First session for this LLC filter */
    else
    {   
        currentSession = ti_match_llc_filter->LLCPpCounters.activeSessionsList;

        while ((currentSession->llcFilterInfo.nextSession != NULL) &&
               (currentSession->llcFilterInfo.nextSession->sessionHandle != sessionHandle))
        {
            currentSession = currentSession->llcFilterInfo.nextSession;
        }
    
        if (currentSession->llcFilterInfo.nextSession == NULL)
        {
            printk(KERN_ERR"\n%s: session handle %u not found in LLC filter sessions list, LLCIndex = %u\n", 
                   __FUNCTION__, sessionHandle, ti_match_llc_filter->LLCIndex);
            return -1;
        }
    
        currentSession->llcFilterInfo.nextSession = 
            dpp_counters_db.dppSessionsTable[sessionHandle].llcFilterInfo.nextSession;
    }
    
    dpp_counters_db.dppSessionsTable[sessionHandle].llcFilterInfo.clientPtr = NULL;
    dpp_counters_db.dppSessionsTable[sessionHandle].llcFilterInfo.nextSession = NULL;

#ifdef DPP_LOG
    if(dppDbgLevel & DPP_DBG_COUNTERS)
    {
        printk(KERN_INFO"\n%s: Delete matched LLC filter info LLCIndex %u of session %u done.\n", 
               __FUNCTION__, ti_match_llc_filter->LLCIndex, sessionHandle);
    }
#endif

    return 0;
}

/**************************************************************************/
/*! \fn static int DppCountersDb_DeleteInIpFilterInfo(Uint32 sessionHandle, Uint32 sessionPacketsFw)                                   
 **************************************************************************
 *  \brief Delete matched inbuond IP filter info in sessions table according to session handle.
 *  \param[in] sessionHandle - PP session handle number.
 *  \param[in] sessionPacketsFw - PP session forwarded packets number.
 *  \param[out] no output.
 *  \return OK or error status.
 **************************************************************************/
static int DppCountersDb_DeleteInIpFilterInfo(Uint32 sessionHandle, Uint32 sessionPacketsFw)
{
    DppCountSessionEntry_t *currentSession = NULL;

    FilterIP_T* ti_match_inbound_ip_filter = 
        (FilterIP_T*)(dpp_counters_db.dppSessionsTable[sessionHandle].inIpFilterInfo.clientPtr);

    DppCountSessionEntry_t *activeSessionsList = ti_match_inbound_ip_filter->IPInPpCounters.activeSessionsList;

    if (activeSessionsList == NULL)
    {
        printk(KERN_ERR"\n%s: No active sessions for inbuond IP filter IPIndex = %u\n", 
               __FUNCTION__, ti_match_inbound_ip_filter->IPIndex);
        return -1;
    }

    /* add session's forwarded packets number to the filter counter  */
    ti_match_inbound_ip_filter->IPInPpCounters.ppMatches += sessionPacketsFw;

#ifdef DPP_LOG
    if(dppDbgLevel & DPP_DBG_COUNTERS)
    {
        printk(KERN_INFO"\n%s: ppMatches = %u\n", __FUNCTION__, 
               ti_match_inbound_ip_filter->IPOutPpCounters.ppMatches);
    }
#endif

    /* First session for this IP filter */
    if (activeSessionsList->sessionHandle == sessionHandle)
    {
        ti_match_inbound_ip_filter->IPInPpCounters.activeSessionsList = 
            activeSessionsList->inIpFilterInfo.nextSession;
    }

    /* Not First session for this IP filter */
    else
    {
        currentSession = ti_match_inbound_ip_filter->IPInPpCounters.activeSessionsList;

        while ((currentSession->inIpFilterInfo.nextSession != NULL) &&
               (currentSession->inIpFilterInfo.nextSession->sessionHandle != sessionHandle))
        {
            currentSession = currentSession->inIpFilterInfo.nextSession;
        }
    
        if (currentSession->inIpFilterInfo.nextSession == NULL)
        {
            printk(KERN_ERR"\n%s: session handle %u not found in IP filter sessions list, IPIndex = %u\n", 
                   __FUNCTION__, sessionHandle, ti_match_inbound_ip_filter->IPIndex);
            return -1;
        }
    
        currentSession->inIpFilterInfo.nextSession = 
            dpp_counters_db.dppSessionsTable[sessionHandle].inIpFilterInfo.nextSession;
    }
    
    dpp_counters_db.dppSessionsTable[sessionHandle].inIpFilterInfo.clientPtr = NULL;
    dpp_counters_db.dppSessionsTable[sessionHandle].inIpFilterInfo.nextSession = NULL;

#ifdef DPP_LOG
    if(dppDbgLevel & DPP_DBG_COUNTERS)
    {
        printk(KERN_INFO"\n%s: Delete matched inbuond IP filter info IPIndex %u of session %u done.\n", 
               __FUNCTION__, ti_match_inbound_ip_filter->IPIndex, sessionHandle);
    }
#endif

    return 0;
}

/**************************************************************************/
/*! \fn static int DppCountersDb_DeleteOutIpFilterInfo(Uint32 sessionHandle, Uint32 sessionPacketsFw)                                   
 **************************************************************************
 *  \brief Delete matched outbound IP filter info in sessions table according to session handle.
 *  \param[in] sessionHandle - PP session handle number.
 *  \param[in] sessionPacketsFw - PP session forwarded packets number.
 *  \param[out] no output.
 *  \return OK or error status.
 **************************************************************************/
static int DppCountersDb_DeleteOutIpFilterInfo(Uint32 sessionHandle, Uint32 sessionPacketsFw)
{
    DppCountSessionEntry_t *currentSession = NULL;

    FilterIP_T* ti_match_outbound_ip_filter = 
        (FilterIP_T*)(dpp_counters_db.dppSessionsTable[sessionHandle].outIpFilterInfo.clientPtr);

    DppCountSessionEntry_t *activeSessionsList = ti_match_outbound_ip_filter->IPOutPpCounters.activeSessionsList;

    if (activeSessionsList == NULL)
    {
        printk(KERN_ERR"\n%s: No active sessions for outbound IP filter IPIndex = %u\n", 
               __FUNCTION__, ti_match_outbound_ip_filter->IPIndex);
        return -1;
    }

    /* add session's forwarded packets number to the filter counter  */
    ti_match_outbound_ip_filter->IPOutPpCounters.ppMatches += sessionPacketsFw;

#ifdef DPP_LOG
    if(dppDbgLevel & DPP_DBG_COUNTERS)
    {
        printk(KERN_INFO"\n%s: ppMatches = %u\n", __FUNCTION__, 
               ti_match_outbound_ip_filter->IPOutPpCounters.ppMatches);
    }
#endif

    /* First session for this IP filter */
    if (activeSessionsList->sessionHandle == sessionHandle)
    {
        ti_match_outbound_ip_filter->IPOutPpCounters.activeSessionsList = 
            activeSessionsList->outIpFilterInfo.nextSession;
    }

    /* Not First session for this IP filter */
    else
    {
        currentSession = ti_match_outbound_ip_filter->IPOutPpCounters.activeSessionsList;

        while ((currentSession->outIpFilterInfo.nextSession != NULL) &&
               (currentSession->outIpFilterInfo.nextSession->sessionHandle != sessionHandle))
        {
            currentSession = currentSession->outIpFilterInfo.nextSession;
        }
    
        if (currentSession->outIpFilterInfo.nextSession == NULL)
        {
            printk(KERN_ERR"\n%s: session handle %u not found in IP filter sessions list, IPIndex = %u\n", 
                   __FUNCTION__, sessionHandle, ti_match_outbound_ip_filter->IPIndex);
            return -1;
        }
    
        currentSession->outIpFilterInfo.nextSession = 
            dpp_counters_db.dppSessionsTable[sessionHandle].outIpFilterInfo.nextSession;
    }
    
    dpp_counters_db.dppSessionsTable[sessionHandle].outIpFilterInfo.clientPtr = NULL;
    dpp_counters_db.dppSessionsTable[sessionHandle].outIpFilterInfo.nextSession = NULL;

#ifdef DPP_LOG
    if(dppDbgLevel & DPP_DBG_COUNTERS)
    {
        printk(KERN_INFO"\n%s: Delete matched outbound IP filter info IPIndex %u of session %u done.\n", 
               __FUNCTION__, ti_match_outbound_ip_filter->IPIndex, sessionHandle);
    }
#endif

    return 0;
}

/**************************************************************************/
/*! \fn static int DppCountersDb_DeleteQosClassInfo(Uint32 sessionHandle, Uint32 sessionPacketsFw)                                   
 **************************************************************************
 *  \brief Delete matched classifier info in sessions table according to session handle.
 *  \param[in] sessionHandle - PP session handle number.
 *  \param[in] sessionPacketsFw - PP session forwarded packets number.
 *  \param[out] no output.
 *  \return OK or error status.
 **************************************************************************/
static int DppCountersDb_DeleteQosClassInfo(Uint32 sessionHandle, Uint32 sessionPacketsFw)
{
    DppCountSessionEntry_t *currentSession = NULL;

    QosClassifier_t* ti_match_qos_classifier = 
        (QosClassifier_t*)(dpp_counters_db.dppSessionsTable[sessionHandle].qosClassInfo.clientPtr);

    DppCountSessionEntry_t *activeSessionsList = ti_match_qos_classifier->ppCounters.activeSessionsList;

    if (activeSessionsList == NULL)
    {
        printk(KERN_ERR"\n%s: No active sessions for classifier classID = %u, SfID = %u\n", 
               __FUNCTION__, ti_match_qos_classifier->classID, ti_match_qos_classifier->sfID);
        return -1;
    }

    /* add session's forwarded packets number to the classifier counter  */
    ti_match_qos_classifier->ppCounters.ppPktCounter += sessionPacketsFw;

#ifdef DPP_LOG
    if(dppDbgLevel & DPP_DBG_COUNTERS)
    {
        printk(KERN_INFO"\n%s: ppPktCounter = %Lu\n", __FUNCTION__, 
               ti_match_qos_classifier->ppCounters.ppPktCounter);
    }
#endif

    /* First session for this classifier */
    if (activeSessionsList->sessionHandle == sessionHandle)
    {
        ti_match_qos_classifier->ppCounters.activeSessionsList = activeSessionsList->qosClassInfo.nextSession;
    }

    /* Not First session for this classifier */
    else
    {
        currentSession = ti_match_qos_classifier->ppCounters.activeSessionsList;

        while ((currentSession->qosClassInfo.nextSession != NULL) &&
               (currentSession->qosClassInfo.nextSession->sessionHandle != sessionHandle))
        {
            currentSession = currentSession->qosClassInfo.nextSession;
        }
    
        if (currentSession->qosClassInfo.nextSession == NULL)
        {
            printk(KERN_ERR"\n%s: session handle %u not found in classifier sessions list, classID = %u, SfID = %u\n", 
                   __FUNCTION__, sessionHandle, ti_match_qos_classifier->classID, ti_match_qos_classifier->sfID);
            return -1;
        }
    
        currentSession->qosClassInfo.nextSession = 
            dpp_counters_db.dppSessionsTable[sessionHandle].qosClassInfo.nextSession;
    }
    
    dpp_counters_db.dppSessionsTable[sessionHandle].qosClassInfo.clientPtr = NULL;
    dpp_counters_db.dppSessionsTable[sessionHandle].qosClassInfo.nextSession = NULL;

#ifdef DPP_LOG
    if(dppDbgLevel & DPP_DBG_COUNTERS)
    {
        printk(KERN_INFO"\n%s: Delete matched classifier info classID %u sfID %u of session %u done.\n", 
               __FUNCTION__, ti_match_qos_classifier->classID, ti_match_qos_classifier->sfID, sessionHandle);
    }
#endif

    return 0;
}

/**************************************************************************/
/*! \fn static int DppCountersDb_DeleteIngressPortInfo(Uint32 sessionHandle, Uint32 sessionPacketsFw)                                 
 **************************************************************************
 *  \brief Delete matched ingress port info in sessions table according to session handle.
 *  \param[in] sessionHandle - PP session handle number.
 *  \param[in] sessionPacketsFw - PP session forwarded packets number.
 *  \param[out] no output.
 *  \return OK or error status.
 **************************************************************************/
static int DppCountersDb_DeleteIngressPortInfo(Uint32 sessionHandle, Uint32 sessionPacketsFw)
{
    DppCountSessionEntry_t *currentSession = NULL;

    DppPortEntry_t* dppPortEntry = 
        (DppPortEntry_t*)(dpp_counters_db.dppSessionsTable[sessionHandle].ingressPortInfo.clientPtr);

    DppCountSessionEntry_t *activeSessionsList = dppPortEntry->activeSessionsList;

    if (activeSessionsList == NULL)
    {
        printk(KERN_ERR"\n%s: No active sessions for ingress port client\n", __FUNCTION__);
        return -1;
    }

    /* add session's forwarded packets number to the port discard counter  */
    dppPortEntry->portDiscardsCounter += sessionPacketsFw;

#ifdef DPP_LOG
    if(dppDbgLevel & DPP_DBG_COUNTERS)
    {
        printk(KERN_INFO"\n%s: portDiscardsCounter = %u\n", __FUNCTION__, dppPortEntry->portDiscardsCounter);
    }
#endif

    /* First session for this port */
    if (activeSessionsList->sessionHandle == sessionHandle)
    {
        dppPortEntry->activeSessionsList = activeSessionsList->ingressPortInfo.nextSession;
    }

    /* Not First session for this port */
    else
    {
        currentSession = dppPortEntry->activeSessionsList;

        while ((currentSession->ingressPortInfo.nextSession != NULL) &&
               (currentSession->ingressPortInfo.nextSession->sessionHandle != sessionHandle))
        {
            currentSession = currentSession->ingressPortInfo.nextSession;
        }
    
        if (currentSession->ingressPortInfo.nextSession == NULL)
        {
            printk(KERN_ERR"\n%s: session handle %u not found in port sessions list\n", 
                   __FUNCTION__, sessionHandle);
            return -1;
        }
    
        currentSession->ingressPortInfo.nextSession = 
            dpp_counters_db.dppSessionsTable[sessionHandle].ingressPortInfo.nextSession;
    }
    
    dpp_counters_db.dppSessionsTable[sessionHandle].ingressPortInfo.clientPtr = NULL;
    dpp_counters_db.dppSessionsTable[sessionHandle].ingressPortInfo.nextSession = NULL;

#ifdef DPP_LOG
    if(dppDbgLevel & DPP_DBG_COUNTERS)
    {
        printk(KERN_INFO"\n%s: Delete matched ingress port info of session %u done.\n", 
               __FUNCTION__, sessionHandle);
    }
#endif

    return 0;
}
