/*
 *
 * dbridge_igmp.h
 * Description:
 * Declaration of DOCSIS bridge IGMP related functions and types.
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

#ifndef _DBRIDGE_IGMP_H_
#define _DBRIDGE_IGMP_H_

#include <linux/igmp.h>

#include "dbridge_db.h"

/* group states */
#define IDLE_MEMBER_STATE               0        /* Non-Member     */
#define JOINED_MEMBER_STATE             1        /* Joined-Member  */
#define JOIN_IN_PROCESS_STATE           2        /* Wait-to-Join   */
#define LEAVE_STATE                     0
#define JOIN_STATE                      1

#define IGMP_HFC_INT_INDEX              0
#define IGMP_CMCI_INT_INDEX             1

#define NUM_OF_MULTICAST_INTERFACES     2

struct igmp_cfg
{
    int enabled;
    int igmp_mqi_timeout;
    int igmp_qri_timeout;
    int igmp_m1_max_timeout;
    int igmp_docsis_robustness;
    /* Callback function to send IGMP packet via DOCSIS bridge. Interface specfies 
     * whether packet has to be sent to CMCI or CABLE interface
     */
    int (*ti_igmp_send_packet)(struct sk_buff *skb, unsigned long long device_map, unsigned long device_type);
    /* Callback function to notify DOCSIS of IGMP JOIN/LEAVE messages. 
     * MAC address is source mac of JOIN/LEAVE message. 
     * Message type specifies JOIN(1)/LEAVE(0).
     */
    int (*ti_igmp_message_processed)(char *mac_address, int msg_type);
};

struct igmp_stats
{
   unsigned long  queryInterval;
   unsigned long  version;
   unsigned long  querier;
   unsigned long  queryMaxResTime;
   unsigned long  version1QuerierTimer;
   unsigned long  wrongVerQueries;
   unsigned long  joins;
   unsigned long  groups;
   unsigned long  robustness;
   unsigned long  lastMemQueryIntrvl;
   unsigned long  proxyIfIndex;
   unsigned long  querierUpTime;
   unsigned long  querierExpiryTime;
};

struct igmp_cache_entry 
{
    /* Links to the next and prev cache entry */ 
    struct list_head    links;
    unsigned char       state;
    unsigned char       blocked;
    unsigned char       static_address;
    unsigned long       group_address;
    unsigned long       lastReporter;
    unsigned char       MRselfSentFlag;
    unsigned long       M1ExpireTime;
    unsigned long       M2ExpireTime;
    unsigned long       UpTime;
    /* Store the packet */
    struct sk_buff      *skb;
    /* Devices map from where JOIN request was received */
    unsigned long long  input_dev;
	unsigned char	    mac_address[ETH_ALEN];	/* source MAC address */
    unsigned long long  cpe_map;      
};

int ti_igmp_init (struct igmp_cfg *cfg);
int ti_igmp_start (void);
int ti_igmp_stop (void);
int ti_igmp_packet_handler(struct igmphdr *igmp_hdr, struct sk_buff *skb, unsigned int device_type, unsigned int cpe_num);
unsigned long long ti_get_igmp_devices(unsigned long ip_addr);
int dbrctl_read_igmp_dbg(char* page, char **start, off_t offset, int count,int *eof, void *data);
int dbrctl_read_igmp_if_mib(char* page, char **start, off_t offset, int count,int *eof, void *data);
int dbrctl_read_igmp_cache_mib(char* page, char **start, off_t offset, int count,int *eof, void *data);

#endif /* _DBRIDGE_IGMP_H_ */
