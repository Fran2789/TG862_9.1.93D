/*
 *
 * dbridge_hal_filter.c
 * Description:
 * DOCSIS bridge IGMP implementation
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


#include <linux/bitops.h>
#include <linux/cpu.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/igmp.h>
#include <linux/list.h>
#include <linux/random.h>
#include <linux/ip.h>
#include <linux/mm.h>
#include <linux/socket.h>
#include <linux/un.h>
#include <net/sock.h>
#include <net/inet_common.h>
#include <net/netlink.h>
#include <linux/netlink.h>

#include "dbridge_common.h"
#include "dbridge_db.h"
#include "dbridge_igmp.h"
#include "dbridge_mcast.h"
#include "dbridge_main.h"
#include "pal.h"

#undef CONFIG_TI_L2_IGMP_DEBUG

#define FALSE       0
#define TRUE        1
#define IGMP_INIT   1
#define IGMP_START  2
#define IGMP_STOP   3


typedef struct TI_IGMP_MCB
{
    /* IGMP State machine - INIT, START, STOP */
    int state;

    /* Configuration received from DOCSIS */
    struct igmp_cfg     cfg;

    /* Statistics */
    struct igmp_stats   igmp_stats[NUM_OF_MULTICAST_INTERFACES];

    /* Total number of groups registered via JOIN */
    int                 number_of_groups;
    
    /* Time when last last was recived. Required to recompute MQI */
    unsigned long       last_MQ_time;
    unsigned long       start_timer;

    /* 1 sec IGMP Timer */
    struct timer_list   igmp_timer;
}TI_IGMP_MCB;

TI_IGMP_MCB  ti_igmp_mcb;

LIST_HEAD (igmp_cache);

/* well known  IGMP mac address */
static unsigned char mcatWkIgmpMac[ETH_ALEN]={0x01,0x00,0x5e,0x00,0x00,0x01};

#define NL_IGMP_JOIN 100
#define NL_IGMP_LEAVE 101


void notify_user_nl(unsigned long addr, int joining)
{
    struct sk_buff *skb;
    struct nlmsghdr *nlh;
    struct ti_docsis_nlmsg *msg;
    struct sock *nlsk = DbridgeDb_GetNlSocket();
    int nl_pid = DbridgeDb_GetNlPid();

    
    if (!nl_pid) return;
    skb = alloc_skb(NLMSG_SPACE(sizeof(struct ti_docsis_nlmsg)), GFP_KERNEL);
    if(!skb)
        goto nlmsg_failure;
    nlh = NLMSG_PUT(skb, nl_pid, 0, NETLINK_TI_DOCSIS, sizeof(struct ti_docsis_nlmsg));
    msg = NLMSG_DATA(nlh);
    msg->type = joining ? NL_IGMP_JOIN : NL_IGMP_LEAVE;
    msg->data.addr = addr;
    NETLINK_CB(skb).pid = 0;
    NETLINK_CB(skb).dst_group = 0;
    netlink_unicast(nlsk, skb, nl_pid, 0);
    return;

nlmsg_failure:
    printk (KERN_ERR " notify_user_nl: skb_put failure\n");
}

static void nl_data_ready( struct sk_buff *skb)
{
    struct nlmsghdr *nlh;

        nlh = nlmsg_hdr(skb);
    DbridgeDb_SetNlPid(nlh->nlmsg_pid);
}

static void init_user_nl(void)
{
    struct sock *nlsk;

    nlsk = netlink_kernel_create(&init_net,NETLINK_TI_DOCSIS, 0, nl_data_ready, NULL, THIS_MODULE);
    DbridgeDb_SetNlSocket(nlsk);
}

static void initialize_igmp_configuration(struct igmp_cfg *cfg)
{
    memcpy ((void *)&ti_igmp_mcb.cfg, (void *)cfg, sizeof (struct igmp_cfg));

    
    ti_igmp_mcb.igmp_stats[IGMP_HFC_INT_INDEX].queryInterval =0;
    ti_igmp_mcb.igmp_stats[IGMP_HFC_INT_INDEX].version = 2;
    ti_igmp_mcb.igmp_stats[IGMP_HFC_INT_INDEX].querier =0 ;
    ti_igmp_mcb.igmp_stats[IGMP_HFC_INT_INDEX].queryMaxResTime =0;
    ti_igmp_mcb.igmp_stats[IGMP_HFC_INT_INDEX].querierUpTime=0;
    ti_igmp_mcb.igmp_stats[IGMP_HFC_INT_INDEX].querierExpiryTime =0;
    ti_igmp_mcb.igmp_stats[IGMP_HFC_INT_INDEX].version1QuerierTimer =0;
    ti_igmp_mcb.igmp_stats[IGMP_HFC_INT_INDEX].wrongVerQueries =0;
    ti_igmp_mcb.igmp_stats[IGMP_HFC_INT_INDEX].joins=0;
    ti_igmp_mcb.igmp_stats[IGMP_HFC_INT_INDEX].proxyIfIndex=0;
    ti_igmp_mcb.igmp_stats[IGMP_HFC_INT_INDEX].groups =0;
    ti_igmp_mcb.igmp_stats[IGMP_HFC_INT_INDEX].robustness=0;
    ti_igmp_mcb.igmp_stats[IGMP_HFC_INT_INDEX].lastMemQueryIntrvl =0;


    ti_igmp_mcb.igmp_stats[IGMP_CMCI_INT_INDEX].queryInterval = (unsigned long)ti_igmp_mcb.cfg.igmp_mqi_timeout;
    ti_igmp_mcb.igmp_stats[IGMP_CMCI_INT_INDEX].version = 2;
    ti_igmp_mcb.igmp_stats[IGMP_CMCI_INT_INDEX].querier =0 ;
    ti_igmp_mcb.igmp_stats[IGMP_CMCI_INT_INDEX].queryMaxResTime =0;
    ti_igmp_mcb.igmp_stats[IGMP_CMCI_INT_INDEX].querierUpTime=0 ;
    ti_igmp_mcb.igmp_stats[IGMP_CMCI_INT_INDEX].querierExpiryTime =0;
    ti_igmp_mcb.igmp_stats[IGMP_CMCI_INT_INDEX].version1QuerierTimer =0;
    ti_igmp_mcb.igmp_stats[IGMP_CMCI_INT_INDEX].wrongVerQueries =0;
    ti_igmp_mcb.igmp_stats[IGMP_CMCI_INT_INDEX].joins=0;
    ti_igmp_mcb.igmp_stats[IGMP_CMCI_INT_INDEX].proxyIfIndex=2;
    ti_igmp_mcb.igmp_stats[IGMP_CMCI_INT_INDEX].groups =0;
    ti_igmp_mcb.igmp_stats[IGMP_CMCI_INT_INDEX].robustness=0;
    ti_igmp_mcb.igmp_stats[IGMP_CMCI_INT_INDEX].lastMemQueryIntrvl =0;

    
    
    return;
}

/**************************************************************************
 * FUNCTION NAME : mark_device_in_group
 **************************************************************************
 * DESCRIPTION   :
 *  Delete IGMP Cache entry
 *  
 * RETURNS       :
 * None
 *
 **************************************************************************/
static void mark_device_in_group(struct igmp_cache_entry* group_info, 
                            unsigned long dev, unsigned char flag)
{
    if (flag)
        SET_BIT_MASK_64(group_info->input_dev, dev);
    else
        UNSET_BIT_MASK_64(group_info->input_dev, dev); 
    return;
}


/**************************************************************************
 * FUNCTION NAME : mark_cpe_map_in_group
 **************************************************************************
 * DESCRIPTION   :
 *  Delete IGMP Cache entry
 *  
 * RETURNS       :
 * None
 *
 **************************************************************************/
static void mark_cpe_map_in_group(struct igmp_cache_entry* group_info, 
                            unsigned long cpe_num , unsigned char flag)
{
    if (flag)
        SET_BIT_MASK_64(group_info->cpe_map, cpe_num);
    else
        UNSET_BIT_MASK_64(group_info->cpe_map, cpe_num);
    return;
}


/**************************************************************************
 * FUNCTION NAME : create_igmp_cache_entry
 **************************************************************************
 * DESCRIPTION   :
 *  Create IGMP Cache entry
 *  
 * RETURNS       :
 *  NULL  -   Error
 *  Pointer to the cache entry
 *  
 **************************************************************************/
static struct igmp_cache_entry* create_igmp_cache_entry(unsigned long ip_addr)
{
    struct igmp_cache_entry   *group_info;
    struct list_head *ptr;
    struct igmp_cache_entry *entry;
 
    /* Allocate memory for Cache. */
    group_info = (struct igmp_cache_entry *) kmalloc(sizeof(struct igmp_cache_entry), GFP_KERNEL);
    if (group_info == NULL)
    {
       printk (KERN_ERR " Failed to allocate memory for IGMP cache\n");
        return NULL;
    }
 
    group_info->state = IDLE_MEMBER_STATE;
    group_info->blocked = FALSE;
    group_info->static_address = FALSE;
    group_info->M1ExpireTime = 0;
    group_info->M2ExpireTime = 0;
    group_info->skb = NULL;
    group_info->input_dev = 0;
    group_info->group_address = ip_addr;
    group_info->cpe_map = 0;
    group_info->UpTime = ti_igmp_mcb.start_timer;
    ti_igmp_mcb.igmp_stats[IGMP_CMCI_INT_INDEX].joins++;
    ti_igmp_mcb.number_of_groups++;

    /* Add the new entry sorted */
    list_for_each(ptr, &igmp_cache) 
    {
        entry = list_entry(ptr, struct igmp_cache_entry, links);
        if (entry->group_address > group_info->group_address) 
        {
            list_add_tail((struct list_head *)&group_info->links, ptr);
            return group_info;
        }
    }

    /* Add the new entry to global IGMP Cache list */
    list_add_tail((struct list_head *)&group_info->links, &igmp_cache);
    return group_info;
}


/**************************************************************************
 * FUNCTION NAME : delete_igmp_cache_entry
 **************************************************************************
 * DESCRIPTION   :
 *  Delete IGMP Cache entry
 *  
 * RETURNS       :
 *  None
 *
 **************************************************************************/
static void delete_igmp_cache_entry(struct igmp_cache_entry* group_info)
{
    /* Remove the cache entry from the global IGMP cache list. */
    list_del((struct list_head *)&group_info->links);
    ti_igmp_mcb.number_of_groups--;

    /* Free allocated memory */
    kfree (group_info);
}

/**************************************************************************
 * FUNCTION NAME : flush_igmp_cache_entry
 **************************************************************************
 * DESCRIPTION   :
 *  Flush IGMP Cache
 *  
 * RETURNS       :
 *  None
 *
 **************************************************************************/
static void flush_igmp_cache_entry(void)
{
    struct igmp_cache_entry *group_info;

    while (!list_empty(&igmp_cache))    
    {
        group_info = (struct igmp_cache_entry *)list_entry(igmp_cache.next,
                 struct igmp_cache_entry, links);

        /* Free any skb stored */
        if (group_info->skb)
            kfree_skb(group_info->skb);

        /* Remove the cache entry from the global IGMP cache list. */
        list_del((struct list_head *)&group_info->links);

        /* Free allocated memory */
        kfree (group_info);
    }
    ti_igmp_mcb.number_of_groups = 0;
}

/**************************************************************************
 * FUNCTION NAME : get_igmp_cache_entry
 **************************************************************************
 * DESCRIPTION   :
 *  Get IGMP Cache entry
 *  
 * RETURNS       :
 *  NULL  -   Error
 *  Pointer to the cache entry
 *
 * 
 **************************************************************************/
static struct igmp_cache_entry* get_igmp_cache_entry(unsigned long ip_addr)
{
    struct list_head        *ptr_temp;
    struct igmp_cache_entry *group_info;

    list_for_each(ptr_temp, &igmp_cache)
    {
        /* Get the WAN Bridge Information and check for the name. */
        group_info = (struct igmp_cache_entry *)ptr_temp;
        if (group_info->group_address == ip_addr)
            return group_info;
    }
    return NULL;
}

/**************************************************************************
 * FUNCTION NAME : ti_get_igmp_devices
 **************************************************************************
 * DESCRIPTION   :
 *  Called by external entity to get devices on which IGMP JOIN was received
 *  
 * RETURNS       :
 *  0   -   If no IP address match
 *  Device map - Indicate what netdevices IGMP JOIN was received on
 *
 **************************************************************************/
unsigned long long ti_get_igmp_devices(unsigned long ip_addr)
{
    struct list_head        *ptr_temp;
    struct igmp_cache_entry *group_info;
    unsigned int lockKey;

    /************************************************************************/
    PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);
    /************************************************************************/

    if (ti_igmp_mcb.state != IGMP_START)
    {
        /************************************************************************/
        PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
        /************************************************************************/
        return 0;
    }

    list_for_each(ptr_temp, &igmp_cache)
    {
        /* Get the WAN Bridge Information and check for the name. */
        group_info = (struct igmp_cache_entry *)ptr_temp;
        if (group_info->group_address == ip_addr)
        {
            /************************************************************************/
            PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
            /************************************************************************/
            return group_info->input_dev;
        }
    }

    /************************************************************************/
    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
    /************************************************************************/
    return 0;
}

/**************************************************************************
 * FUNCTION NAME : igmp_updatelastReporter
 **************************************************************************
 * DESCRIPTION   :
 *  Start M1 timer 
 *  
 * RETURNS       :
 *  None
 *  
 **************************************************************************/
static void igmp_updatelastReporter(struct igmp_cache_entry *group_info,struct sk_buff *skb)
{
    struct iphdr    *iph = (struct iphdr *) ((skb_mac_header(skb)) + ETH_HLEN);
    group_info->lastReporter = iph->saddr;

}

/**************************************************************************
 * FUNCTION NAME : igmp_updatetQueryInfo
 **************************************************************************
 * DESCRIPTION   :
 *  Start M1 timer 
 *  
 * RETURNS       :
 *  None
 *  
 **************************************************************************/
 void igmp_updatetQueryInfo(struct sk_buff *skb)
 {
     struct iphdr    *iph = (struct iphdr *) ((skb_mac_header(skb)) + ETH_HLEN);
     unsigned int   lockKey;

     /************************************************************************/
     /*   Protect the stats DataBase entry                                   */
     /************************************************************************/
     PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);
     /************************************************************************/
     
     if(ti_igmp_mcb.igmp_stats[IGMP_HFC_INT_INDEX].querier == 0)
     {
         ti_igmp_mcb.igmp_stats[IGMP_HFC_INT_INDEX].querier = iph->saddr;
         ti_igmp_mcb.igmp_stats[IGMP_HFC_INT_INDEX].querierUpTime = ti_igmp_mcb.start_timer;
         ti_igmp_mcb.igmp_stats[IGMP_CMCI_INT_INDEX].querier = iph->saddr;
     }

     /************************************************************************/
     PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
     /************************************************************************/
     return;
 }

/**************************************************************************
 * FUNCTION NAME : dbrctl_read_igmpdbg
 **************************************************************************
 * DESCRIPTION   :
 *  Display IGMP Cache entries
 *  
 * RETURNS       :
 *  None
 *
 **************************************************************************/
int dbrctl_read_igmp_dbg(char* page, char **start, off_t offset, int count,int *eof, void *data)
{
    
    off_t pos = 0;
    off_t begin = 0;
    int len = 0;
    struct list_head        *ptr_temp;
    struct igmp_cache_entry *group_info;
    unsigned int lockKey;

    /************************************************************************/
    PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);
    /************************************************************************/


    len+= sprintf(page+len,"start_timer        %lu \n", ti_igmp_mcb.start_timer);
    len+= sprintf(page+len, "IGMP Cache                     \n");
    len+= sprintf(page+len, "----------                     \n");
    len+= sprintf(page+len, "IGMP Total Groups              %d\n", ti_igmp_mcb.number_of_groups);

    list_for_each(ptr_temp, &igmp_cache)
    {
        /* Get the WAN Bridge Information and check for the name. */
        group_info = (struct igmp_cache_entry *)ptr_temp;
        len+= sprintf(page+len,"IGMP Group address         %lu.%lu.%lu.%lu \n",
                      (group_info->group_address >>24)&0xff, (group_info->group_address >>16)&0xff,
                      (group_info->group_address >> 8)&0xff, (group_info->group_address)   &0xff );
        len+= sprintf(page+len,"IGMP Device map            %llX \n", group_info->input_dev);
        len+= sprintf(page+len,"IGMP SKB                   0x%p \n", group_info->skb);
        len+= sprintf(page+len,"IGMP Group M1 Timer        %lu \n", group_info->M1ExpireTime);
        len+= sprintf(page+len,"IGMP Group M2 Timer        %lu \n", group_info->M2ExpireTime);
        len+= sprintf(page+len,"IGMP last                  %lu.%lu.%lu.%lu \n",
        (group_info->lastReporter >>24)&0xff, (group_info->lastReporter >>16)&0xff,
        (group_info->lastReporter >> 8)&0xff, (group_info->lastReporter)   &0xff );
        len+= sprintf(page+len,"IGMP Up time        %lu   \n", (ti_igmp_mcb.start_timer - group_info->UpTime));
        len+= sprintf(page+len,"Source MAC Address         0x%02x-0x%02x-0x%02x-0x%02x-0x%02x-0x%02x\n", 
        group_info->mac_address[0], group_info->mac_address[1], group_info->mac_address[2],
        group_info->mac_address[3], group_info->mac_address[4], group_info->mac_address[5]);
        len+= sprintf(page+len,"IGMP CPE map        %llx \n", group_info->cpe_map);
        
        pos = begin + len;
        if (pos < offset)
        {
            len = 0;
            begin = pos;
        }
        if (pos > offset + count)
            goto done;
    }


    len+= sprintf(page+len, "\n");

    done:

    *start = page + (offset - begin);
    len -= (offset - begin);

    if (len > count)
        len = count;
    if (len < 0)
        len = 0;

    /************************************************************************/
    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey); 
    /************************************************************************/
    return len;

}


/**************************************************************************
 * FUNCTION NAME : dbrctl_read_igmp_if_mib
 **************************************************************************
 * DESCRIPTION   :
 *  Display IGMP Cache entries
 *  
 * RETURNS       :
 *  None
 *
 **************************************************************************/
int dbrctl_read_igmp_if_mib(char* page, char **start, off_t offset, int count,int *eof, void *data)
{
    
    unsigned int len = 0;
    unsigned long up_time=0;
    unsigned int  lockKey;


    len+= sprintf(page+len, "IGMP Interfase\n");
    len+= sprintf(page+len, "--------------\n");

    len+= sprintf(page+len, "\t1\t2\t3\t4\t5\t6\t7\t8\t9\t10\t11\t12\t13\t14\n");
    
    /************************************************************************/
    PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);
    /************************************************************************/

    if(ti_igmp_mcb.igmp_stats[IGMP_HFC_INT_INDEX].querierUpTime)
        up_time = (ti_igmp_mcb.start_timer - ti_igmp_mcb.igmp_stats[IGMP_HFC_INT_INDEX].querierUpTime);
     
    len+= sprintf(page+len, "HFC\t%lu\t%d\t%lu\t%lu\t%lu\t%lu\t%lu\t%lu\t%lu\t%lu\t%lu\t%d\t%lu\t%lu\n",
    ti_igmp_mcb.igmp_stats[IGMP_HFC_INT_INDEX].queryInterval,
    (ti_igmp_mcb.state == IGMP_START),
    ti_igmp_mcb.igmp_stats[IGMP_HFC_INT_INDEX].version,
    ti_igmp_mcb.igmp_stats[IGMP_HFC_INT_INDEX].querier,
    ti_igmp_mcb.igmp_stats[IGMP_HFC_INT_INDEX].queryMaxResTime,
    up_time,
    ti_igmp_mcb.igmp_stats[IGMP_HFC_INT_INDEX].querierExpiryTime,
    ti_igmp_mcb.igmp_stats[IGMP_HFC_INT_INDEX].version1QuerierTimer,
    ti_igmp_mcb.igmp_stats[IGMP_HFC_INT_INDEX].wrongVerQueries,
    ti_igmp_mcb.igmp_stats[IGMP_HFC_INT_INDEX].joins,
    ti_igmp_mcb.igmp_stats[IGMP_HFC_INT_INDEX].proxyIfIndex,
    0,
    ti_igmp_mcb.igmp_stats[IGMP_HFC_INT_INDEX].robustness,
    ti_igmp_mcb.igmp_stats[IGMP_HFC_INT_INDEX].lastMemQueryIntrvl);
       

    if(ti_igmp_mcb.igmp_stats[IGMP_CMCI_INT_INDEX].querierUpTime)
        up_time = (ti_igmp_mcb.start_timer - ti_igmp_mcb.igmp_stats[IGMP_CMCI_INT_INDEX].querierUpTime);
    else
        up_time = 0;

    len+= sprintf(page+len, "CMCI\t%lu\t%d\t%lu\t%lu\t%lu\t%lu\t%lu\t%lu\t%lu\t%lu\t%lu\t%d\t%lu\t%lu\n",
    ti_igmp_mcb.igmp_stats[IGMP_CMCI_INT_INDEX].queryInterval,
    (ti_igmp_mcb.state == IGMP_START),
    ti_igmp_mcb.igmp_stats[IGMP_CMCI_INT_INDEX].version,
    ti_igmp_mcb.igmp_stats[IGMP_CMCI_INT_INDEX].querier,
    ti_igmp_mcb.igmp_stats[IGMP_CMCI_INT_INDEX].queryMaxResTime,
    up_time,
    ti_igmp_mcb.igmp_stats[IGMP_CMCI_INT_INDEX].querierExpiryTime,
    ti_igmp_mcb.igmp_stats[IGMP_CMCI_INT_INDEX].version1QuerierTimer,
    ti_igmp_mcb.igmp_stats[IGMP_CMCI_INT_INDEX].wrongVerQueries,
    ti_igmp_mcb.igmp_stats[IGMP_CMCI_INT_INDEX].joins,
    ti_igmp_mcb.igmp_stats[IGMP_CMCI_INT_INDEX].proxyIfIndex,
    ti_igmp_mcb.number_of_groups,
    ti_igmp_mcb.igmp_stats[IGMP_CMCI_INT_INDEX].robustness,
    ti_igmp_mcb.igmp_stats[IGMP_CMCI_INT_INDEX].lastMemQueryIntrvl);
    
    /************************************************************************/
    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
    /************************************************************************/

    
    len+= sprintf(page+len, "1)\tQueryInterval\n");
    len+= sprintf(page+len, "2)\tState\n");
    len+= sprintf(page+len, "3)\tVersion\n");
    len+= sprintf(page+len, "4)\tQuerier\n");
    len+= sprintf(page+len, "5)\tQueryMaxResTime\n");
    len+= sprintf(page+len, "6)\tQuerierUpTime\n");
    len+= sprintf(page+len, "7)\tQuerierExpiryTime\n");
    len+= sprintf(page+len, "8)\tVersion1QuerierTimer\n");
    len+= sprintf(page+len, "9)\tWrongVerQueries\n");
    len+= sprintf(page+len, "10)\tJoins\n");
    len+= sprintf(page+len, "11)\tProxyIfIndex\n");
    len+= sprintf(page+len, "12)\tGroups\n");
    len+= sprintf(page+len, "13)\tRobustness\n");
    len+= sprintf(page+len, "14)\tLastMemQueryIntrvl\n");
    

    *eof = 1;
    return len;
}
    
/**************************************************************************
 * FUNCTION NAME : dbrctl_read_igmp_cache_mib
 **************************************************************************
 * DESCRIPTION   :
 *  Display IGMP Cache entries
 *  
 * RETURNS       :
 *  None
 *
 **************************************************************************/
int dbrctl_read_igmp_cache_mib(char* page, char **start, off_t offset, int count,int *eof, void *data)
{
    
    off_t pos = 0;
    off_t begin = 0;
    int len = 0;
    unsigned int grop_index = 1;
    struct list_head        *ptr_temp;
    struct igmp_cache_entry *group_info;
    unsigned int  lockKey;


    len+= sprintf(page+len, "IGMP Cache Mib\n");
    len+= sprintf(page+len, "--------------\n");
    len+= sprintf(page+len, "IGMP Total Groups\t%d\n", ti_igmp_mcb.number_of_groups);
    len+= sprintf(page+len,"\t1\t2\t3\t4\t5\t6\t7\t8\n");

    /************************************************************************/
    PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);
    /************************************************************************/

    list_for_each(ptr_temp, &igmp_cache)
    {
        group_info = (struct igmp_cache_entry *)ptr_temp;
        
        len+= sprintf(page+len,"->\t%lu\t%u\t%u\t%lu\t%lu\t%lu\t%u\t%u\n",
        group_info->group_address, 1, 2, group_info->lastReporter,
        (ti_igmp_mcb.start_timer - group_info->UpTime),group_info->M2ExpireTime,1 , 0);

        grop_index++;
        pos = begin + len;
        if (pos < offset)
        {
            len = 0;
            begin = pos;
        }
        if (pos > offset + count)
        {
            /************************************************************************/
            PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
            /************************************************************************/
            goto done;
        }
    }

    /************************************************************************/
    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
    /************************************************************************/
    
    len+= sprintf(page+len, "1)\tAddress\n");
    len+= sprintf(page+len, "2)\tIfIndex\n");
    len+= sprintf(page+len, "3)\tSelf\n");
    len+= sprintf(page+len, "4)\tlastReporter\n");
    len+= sprintf(page+len, "5)\tUpTime\n");
    len+= sprintf(page+len, "6)\tExpireTime\n");
    len+= sprintf(page+len, "7)\tStatus\n");
    len+= sprintf(page+len, "8)\tVersioin1HostTimer\n");
    len+= sprintf(page+len, "\n");

    done:

    *start = page + (offset - begin);
    len -= (offset - begin);

    if (len > count)
        len = count;
    if (len < 0)
        len = 0;

    return len;

}

/**************************************************************************
 * FUNCTION NAME : igmp_start_M1_timer
 **************************************************************************
 * DESCRIPTION   :
 *  Start M1 timer 
 *  
 * RETURNS       :
 *  None
 *  
 **************************************************************************/
static void igmp_start_M1_timer(struct igmp_cache_entry *group_info)
{
    unsigned long random_number;
   
    /* M1 timer is set to a random value between 0~3 seconds */
  
    get_random_bytes(&random_number, 4); 
    
    group_info->M1ExpireTime = (random_number % (ti_igmp_mcb.cfg.igmp_m1_max_timeout + 1));

    if (group_info->M1ExpireTime == 0)
        group_info->M1ExpireTime++;
}

/**************************************************************************
 * FUNCTION NAME : igmp_start_M2_timer
 **************************************************************************
 * DESCRIPTION   :
 *  Start M2 timer 
 *  
 * RETURNS       :
 *  None
 *  
 **************************************************************************/
static void igmp_start_M2_timer(struct igmp_cache_entry *group_info, unsigned char group_flag)
{
    unsigned long queryMaxResTime = ti_igmp_mcb.igmp_stats[IGMP_CMCI_INT_INDEX].queryMaxResTime;
    unsigned long queryInterval = ti_igmp_mcb.igmp_stats[IGMP_CMCI_INT_INDEX].queryInterval;

    /* Handle IGMPv1 query */
    if (queryMaxResTime == 0 ) 
        queryMaxResTime = ti_igmp_mcb.cfg.igmp_qri_timeout;

    if (group_flag)
    {
        /* If group specific query, restart the M2 timer */
        group_info->M2ExpireTime = (queryMaxResTime / 10) * ti_igmp_mcb.cfg.igmp_docsis_robustness;
    }
    else 
    {
        /* M2 timer = 2 * MQI + MRI .  */
        group_info->M2ExpireTime = (queryInterval * ti_igmp_mcb.cfg.igmp_docsis_robustness) 
                                        + (queryMaxResTime / 10);
    }
    if (group_info->M2ExpireTime == 0)
        group_info->M2ExpireTime++;
}

/**************************************************************************
 * FUNCTION NAME : igmp_handle_M2_timeout
 **************************************************************************
 * DESCRIPTION   :
 *  Handles M2 timer timeout
 *  
 * RETURNS       :
 *  None
 *
 **************************************************************************/
static void igmp_handle_M2_timeout(struct igmp_cache_entry *group_info)
{
    if (group_info == NULL)
    {
        printk (KERN_ERR" M2 Timeout on non-existent group\n");
        return;
    }
    else
    {
        /* Notify external entity a LEAVE message or timeout was processed */
        ti_igmp_mcb.cfg.ti_igmp_message_processed(group_info->mac_address, LEAVE_STATE);
        notify_user_nl(group_info->group_address, 0);
    }
    return;
}

/**************************************************************************
 * FUNCTION NAME : ti_igmp_timer_expired
 **************************************************************************
 * DESCRIPTION   :
 *  TI Layer2 IGMP timer function
 *  
 * RETURNS       :
 *  None 
 *  
 **************************************************************************/
static void ti_igmp_timer_expired(unsigned long __data)
{
    struct timer_list       *timer;
    struct igmp_cache_entry *group_info;
    unsigned long           value = 1, counter;
    struct list_head        *ptr_temp;
    unsigned int lockKey;

    /************************************************************************/
    PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);
    /************************************************************************/

    if (ti_igmp_mcb.state != IGMP_START)
    {
        /************************************************************************/
        PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
        /************************************************************************/
        return;
    }
    
    counter = ti_igmp_mcb.number_of_groups;
    ptr_temp = igmp_cache.next;

    /* count the start time in seconds */
    ti_igmp_mcb.start_timer++;
    
    while (counter-- > 0)
    {
        group_info = (struct igmp_cache_entry *) 
                        list_entry(ptr_temp, struct igmp_cache_entry, links);

        ptr_temp = ptr_temp->next;

        if (group_info->M2ExpireTime) 
        {
            if (--group_info->M2ExpireTime == 0) 
            {
                struct sk_buff *skbToBeReleased = NULL;

                igmp_handle_M2_timeout(group_info);

                skbToBeReleased = group_info->skb;

                delete_igmp_cache_entry(group_info);

                /************************************************************************/
                PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
                /************************************************************************/

                /* Free any skb stored */
                if (skbToBeReleased)
                {
                    kfree_skb (skbToBeReleased);
                }

                /************************************************************************/
                /*   Protect the group_info DataBase entry again                        */
                /************************************************************************/
                PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);
                /************************************************************************/
                continue;               
            }
        }

        if (group_info->M1ExpireTime) 
        {
            if (--group_info->M1ExpireTime == 0) 
            {
                /************************/
                /*  Handle M1 timeout   */
                /************************/
                if (group_info->state == JOIN_IN_PROCESS_STATE)
                {
                    /* Change state to JOINED */
                    group_info->state = JOINED_MEMBER_STATE;
                    
                    /* Send IGMP MR to Cable Interface */
                    if (group_info->skb)
                    {
                        struct sk_buff *skbToBeSent = group_info->skb;
                        unsigned long long tmpInputDev = group_info->input_dev;

                        group_info->skb = NULL;
                        /* Last MR was sent by us */
                        group_info->MRselfSentFlag = TRUE;

                        /************************************************************************/
                        PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
                        /************************************************************************/

                        ti_igmp_mcb.cfg.ti_igmp_send_packet(skbToBeSent, tmpInputDev,
                            DBR_CABLE_NET_DEV_TYPE);

                        /************************************************************************/
                        /*   Protect the group_info DataBase entry again                        */
                        /************************************************************************/
                        PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);
                        /************************************************************************/
                        
                    }
                }
            }
        }
    } 

    /* Reinitialize IGMP timer. Expiration time 1 second */
    timer = &ti_igmp_mcb.igmp_timer;
    init_timer(timer);
    timer->data = (unsigned long) &value;
    timer->function = ti_igmp_timer_expired;
    timer->expires = jiffies + 1 * HZ;
    add_timer(timer);

    /************************************************************************/
    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
    /************************************************************************/

    return;
}

/**************************************************************************
 * FUNCTION NAME : process_igmp_membership_report
 **************************************************************************
 * DESCRIPTION   :
 *  Process IGMP membership Report
 *  
 * RETURNS       :
 *  None
 *
 * NOTES         :
 *  In the case of error; the packet memory is freed.
 **************************************************************************/
static void process_igmp_membership_report(struct igmphdr *igmp_hdr, 
                struct sk_buff *skb, unsigned int device_type, unsigned int cpe_num)
{
    struct igmp_cache_entry *group_info = NULL;
    int                     dev_index = 0;
    struct ethhdr           *eth_header;
    unsigned int            lockKey;
    struct sk_buff          *skbToBeReleased = NULL;

    if ( skb->mac_header == NULL)
    {
        /* No Ethernet header. Drop the packet */
        printk (KERN_ERR" No Ethernet header. Dropping packet\n");
        kfree_skb(skb);
        return;
    }

    /* Get the Ethernet header */    
    eth_header = eth_hdr(skb);

#ifdef CONFIG_TI_L2_IGMP_DEBUG
    printk ("DEBUG: Processing IGMP MR for address (%4X)......\n", igmp_hdr->group);
#endif
    
    if (skb->ti_docsis_input_dev != NULL) 
    {
        dev_index = skb->ti_docsis_input_dev->ifindex;
    }
    else if (skb->dev != NULL)
    {
        dev_index = skb->dev->ifindex;
    }
    else
    {
        printk (KERN_DEBUG" IGMP MR for group address(%4X) doesnot have input_dev or dev set. Dropping packet\n",
                igmp_hdr->group);
        kfree_skb(skb);
        return;
    }

    /************************************************************************/
    /*   Protect the group_info DataBase entry                              */
    /************************************************************************/
    PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);
    /************************************************************************/

    group_info = get_igmp_cache_entry(igmp_hdr->group);

    if (device_type == DBR_CABLE_NET_DEV_TYPE)
    {
        /* Membership request received from RF */
        if (group_info != NULL)
        {
            if (group_info->state == JOIN_IN_PROCESS_STATE)
            {
                /* Cancel M1 timer */
                group_info->M1ExpireTime = 0;
                /* Not sending previous MR received from CPE to RF. Free the skb */
                skbToBeReleased = group_info->skb;
                group_info->skb = NULL;
            }
            /* Clear flag to notify that the last membership report was not sent by us. */
            group_info->MRselfSentFlag = FALSE;
            /* Update lastReporter fot igmp mibs */
            igmp_updatelastReporter(group_info,skb);

            /* Change state to Joined */
            group_info->state = JOINED_MEMBER_STATE;
#ifdef CONFIG_TI_L2_IGMP_DEBUG
            printk ("DEBUG: IGMP MR received from RFI i/f for group address(%4X). Dropping packet\n",
                igmp_hdr->group);
#endif

        }
        else
        {
#ifdef CONFIG_TI_L2_IGMP_DEBUG
            printk ("DEBUG: IGMP MR received from RFI i/f for non-existent group address(%4X). Dropping packet\n",
                igmp_hdr->group);
#endif
        }
        /************************************************************************/
        PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
        /************************************************************************/

        dbridgeAddDiscards(skb);
        kfree_skb (skb);

        if (NULL != skbToBeReleased)
        {
            kfree_skb( skbToBeReleased );
        }
    }
    else
    {
        /* Membership request received from LAN */
        if( group_info == NULL )
        {
            /* New membership join */ 
            if ((group_info = create_igmp_cache_entry(igmp_hdr->group)) != NULL)
            {
                group_info->state = JOIN_IN_PROCESS_STATE;
                group_info->skb = skb;
                /* Update lastReporter fot igmp mibs */
                igmp_updatelastReporter(group_info,skb);

#ifdef CONFIG_TI_L2_IGMP_DEBUG
                printk ("DEBUG: IGMP Cache Entry created (0x%p) Source MAC 0x%02x-0x%02x-0x%02x-0x%02x-0x%02x-0x%02x\n", 
                    group_info, eth_header->h_source[0], eth_header->h_source[1], 
                    eth_header->h_source[2], eth_header->h_source[3], 
                    eth_header->h_source[4], eth_header->h_source[5]);
#endif
                memcpy ((void *)&group_info->mac_address[0],  (void *)&eth_header->h_dest[0], ETH_ALEN);
                 
                /* set the cpe bit map */
                mark_cpe_map_in_group(group_info, cpe_num-1, TRUE);            
                /* Start M1 and M2 timer */
                igmp_start_M1_timer(group_info);
                igmp_start_M2_timer(group_info, FALSE);
                
                /* Set the input device map to store incoming interface */
                if (dev_index > 0)
                {
                    mark_device_in_group(group_info, dev_index - 1, TRUE);
                }
                else
                {
                    printk (KERN_WARNING" IGMP Member JOIN received for device index (%d) not found\n", 
                        dev_index);
                }


                /* Notify external entity a JOIN message was processed */
                ti_igmp_mcb.cfg.ti_igmp_message_processed(group_info->mac_address, JOIN_STATE);
                notify_user_nl(group_info->group_address, 1);

                /************************************************************************/
                PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
                /************************************************************************/
            }
            else
            {
                /************************************************************************/
                PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
                /************************************************************************/

                printk (KERN_ERR" Adding group address (%4X) failed. Dropping packet\n", igmp_hdr->group);
                dbridgeAddDiscards(skb);
                kfree_skb(skb); 
            }
        }
        else
        {
            memcpy ((void *)&group_info->mac_address[0],  (void *)&eth_header->h_dest[0], ETH_ALEN);
            /* set the cpe bit map */
            mark_cpe_map_in_group(group_info, cpe_num-1, TRUE);            
            /* Update lastReporter fot igmp mibs */
            igmp_updatelastReporter(group_info,skb);

            /* Existing member. Process only if Group address is not static */
            if (group_info->static_address == FALSE)
            {
                if (group_info->state == JOINED_MEMBER_STATE) 
                {
                    /* Store the skb to send a report to RF after random delay */
                    group_info->state = JOIN_IN_PROCESS_STATE;
                    igmp_start_M1_timer(group_info);
                    if (group_info->skb == NULL)
                    {
                        group_info->skb = skb;
                    }
                    else
                    {
                        printk ("Why am I here??");
                    }
                }
                else
                {
                    /************************************************************************/
                    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
                    /************************************************************************/

#ifdef CONFIG_TI_L2_IGMP_DEBUG
                    printk ("DEBUG: IGMP MR received in Joining state from (%4X). Dropping packet\n",
                        igmp_hdr->group);
#endif
                    dbridgeAddDiscards(skb);
                    kfree_skb(skb);
                    return; 
                }

                group_info->M2ExpireTime = 0;
                igmp_start_M2_timer(group_info, FALSE);

                if (dev_index > 0)                
                {
                    mark_device_in_group(group_info, dev_index - 1, TRUE);
                }
                else
                {
                    printk (KERN_WARNING" IGMP Member JOIN received for device index (%d) not found\n", 
                        dev_index);
                }
                /************************************************************************/
                PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
                /************************************************************************/
            }
            else
            {
                /************************************************************************/
                PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
                /************************************************************************/
                printk (KERN_WARNING" IGMP MR received for static MAC group address(%4X). Dropping packet\n",
                    igmp_hdr->group);
                dbridgeAddDiscards(skb);
                kfree_skb (skb);
            }
        }
    }
}

/**************************************************************************
 * FUNCTION NAME : process_igmp_leave_group
 **************************************************************************
 * DESCRIPTION   :
 *  Process IGMP Leave message
 *  
 * RETURNS       :
 *  None 
 *  
 * NOTES         :
 *  In the case of error; the packet memory is freed.
 **************************************************************************/
static void process_igmp_leave_group(struct igmphdr *igmp_hdr, 
            struct sk_buff *skb, unsigned int device_type, unsigned int cpe_num)
{
    struct igmp_cache_entry *group_info = NULL;
    int                     dev_index = 0;
    unsigned int            lockKey;
    struct sk_buff          *skbToBeReleased = NULL;

    if (skb->ti_docsis_input_dev != NULL)
    {
        dev_index = skb->ti_docsis_input_dev->ifindex;
    }
    else if (skb->dev != NULL)
    {
        dev_index = skb->dev->ifindex;
    }
    else
    {
        printk (KERN_WARNING" IGMP MR for group address(%4X) doesnot have input_dev or dev set. Dropping packet\n",
         igmp_hdr->group);
        kfree_skb(skb);
        return;
    }


    if (DBR_CABLE_NET_DEV_TYPE == device_type)
    {
        printk (KERN_WARNING" IGMP MR for group address(%4X) doesnot have input_dev or dev set. Dropping packet\n",
                igmp_hdr->group);
        dbridgeAddDiscards(skb);
        kfree_skb(skb);
        return;
    }

#ifdef CONFIG_TI_L2_IGMP_DEBUG
    printk ("DEBUG: Processing IGMP Leave for address (%4X)......\n", igmp_hdr->group);
#endif

    /************************************************************************/
    /*   Protect the group_info DataBase entry                              */
    /************************************************************************/
    PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);
    /************************************************************************/

    if ((group_info = get_igmp_cache_entry(igmp_hdr->group)) != NULL)
    {
        unsigned long long tmpInputDev = group_info->input_dev;

        // ARRIS ADD - This causes a memory leak since group_info->skb is the ONLY thing tracking this
        // skb in the system.  Once all members have left the group we'll free it below and NULL it.
        #if 0
        /* skb is sent for forwarding. Do not free */
        group_info->skb = NULL;
        #endif
        // END ARRIS ADD

            /* unset the cpe bit map */
        mark_cpe_map_in_group(group_info, cpe_num-1, FALSE);           

        /* Reset the input device map to store incoming interface */
        if (dev_index > 0)
        {
            mark_device_in_group(group_info, dev_index - 1, FALSE);
        }
        else
        {
            printk (KERN_WARNING" IGMP Leave received for device index (%d) not found\n", dev_index);
        }

        /* If last member in group - Delete timers, group information from cache */
        if (group_info->cpe_map == 0)
        {
            igmp_handle_M2_timeout(group_info);

            /* Free any skb stored */
            if (group_info->skb)
            {
                skbToBeReleased = group_info->skb;
                // ARRIS ADD - NULL out skb since it's about to be freed
                group_info->skb = NULL;
                // END ARRIS ADD
            }

            delete_igmp_cache_entry(group_info);
        }
        
        /************************************************************************/
        PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
        /************************************************************************/

        /* Send IGMP MR to Cable Interface */
        ti_igmp_mcb.cfg.ti_igmp_send_packet(skb, tmpInputDev, DBR_CABLE_NET_DEV_TYPE);

        if (NULL != skbToBeReleased)
        {
            kfree_skb(skbToBeReleased);
        }

    } 
    else
    {
        /************************************************************************/
        PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
        /************************************************************************/

#ifdef CONFIG_TI_L2_IGMP_DEBUG
        printk ("DEBUG: IGMP Leave message received for non-existent group address(%4X). Dropping packet\n",
            igmp_hdr->group);
#endif
        dbridgeAddDiscards(skb);
        kfree_skb (skb);
    }
}



/**************************************************************************
 * FUNCTION NAME : process_igmp_membership_query
 **************************************************************************
 * DESCRIPTION   :
 *  Process IGMP membership Query
 *  
 * RETURNS       :
 *  None
 * 
 * NOTES         :
 *  In the case of error; the packet memory is freed.
 **************************************************************************/
static void process_igmp_membership_query(struct igmphdr *igmp_hdr, 
                struct sk_buff *skb, unsigned int device_type)
{
    struct igmp_cache_entry *group_info = NULL;
    unsigned long long device_map = 0;
    unsigned long           query_delta;
    unsigned int            lockKey;
    //, curr_time;

#ifdef CONFIG_TI_L2_IGMP_DEBUG
    printk ("DEBUG: Processing IGMP Query......\n");
#endif

    if (device_type == DBR_CABLE_NET_DEV_TYPE)
    {
        if (igmp_hdr->group == 0)
        {
            /************************************************************************/
            /*   Protect the stats DataBase entry                                   */
            /************************************************************************/
            PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);
            /************************************************************************/
 
            /* All host Membership Query received */
            /* Get time difference from previous query */

            query_delta = ti_igmp_mcb.start_timer -  ti_igmp_mcb.last_MQ_time;
            /* Update last MQ time to current time */
            ti_igmp_mcb.last_MQ_time = ti_igmp_mcb.start_timer;

            ti_igmp_mcb.igmp_stats[IGMP_CMCI_INT_INDEX].queryMaxResTime = igmp_hdr->code;
            
            ti_igmp_mcb.igmp_stats[IGMP_CMCI_INT_INDEX].queryInterval = (unsigned long)
                max ((unsigned long)ti_igmp_mcb.cfg.igmp_mqi_timeout, (unsigned long) query_delta);

            /* IGMP v1 query received */
            if (igmp_hdr->code == 0)
                ti_igmp_mcb.igmp_stats[IGMP_HFC_INT_INDEX].wrongVerQueries++;
            
            device_map  = DbridgeDB_GetCpeInterfaceMask();
            /************************************************************************/
            PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
            /************************************************************************/
        }
        else
        {
            /************************************************************************/
            /*   Protect the group_info DataBase entry                              */
            /************************************************************************/
            PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);
            /************************************************************************/

            /* Group Specific Query received */
            if ((group_info = get_igmp_cache_entry(igmp_hdr->group)) != NULL)
            {
                group_info->M2ExpireTime = 0;
                igmp_start_M2_timer(group_info, TRUE);
                ti_igmp_mcb.igmp_stats[IGMP_CMCI_INT_INDEX].lastMemQueryIntrvl = igmp_hdr->code;
                ti_igmp_mcb.igmp_stats[IGMP_CMCI_INT_INDEX].queryMaxResTime = igmp_hdr->code;
                device_map  = group_info->input_dev;
                /************************************************************************/
                PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
                /************************************************************************/
            }
            else
            {
                /************************************************************************/
                PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
                /************************************************************************/
            #ifdef CONFIG_TI_L2_IGMP_DEBUG
                printk ("DEBUG: IGMP MQ received for non-existing group. Dropping packet\n");
            #endif

                dbridgeAddDiscards(skb);
                kfree_skb(skb);
                return;
            }
        }

        igmp_updatetQueryInfo(skb);
        /* Forward the MQ packet to LAN interface */
        ti_igmp_mcb.cfg.ti_igmp_send_packet(skb, device_map, (unsigned long)(DBR_CMCI_NET_DEV_TYPE));
    }
    else
    {
#ifdef CONFIG_TI_L2_IGMP_DEBUG
        printk ("DEBUG: IGMP MQ received from non-RF interface. Dropping packet\n");
#endif
        dbridgeAddDiscards(skb);
        kfree_skb(skb);
        return;        
    }
    return;
}

/**************************************************************************
 * FUNCTION NAME : ti_igmp_packet_handler
 **************************************************************************
 * DESCRIPTION   :
 *  TI IGMP Management Packet handler function
 *  
 * RETURNS       :
 *  <0  -   Error
 *   0  -   igmp version is igmpv2/igmpv1
 *   1  -   igmp version is igmpv3
 *
 * NOTES         :
 *  The packet owned by IGMP. It will be storedfor future use or 
 *  freed in case of error.
 **************************************************************************/
int ti_igmp_packet_handler(struct igmphdr *igmp_hdr, struct sk_buff *skb, unsigned int device_type, unsigned int cpe_num)
{

    if (ti_igmp_mcb.state != IGMP_START)
    {
        dbridgeAddDiscards(skb);
        kfree_skb(skb);
        return 0;
    }

#ifdef CONFIG_TI_L2_IGMP_DEBUG
    printk ("Debug: IGMP Type 0x%X - Group = %4X\n", igmp_hdr->type, igmp_hdr->group);
#endif

    switch (igmp_hdr->type)
    {
        case IGMP_HOST_MEMBERSHIP_REPORT:
        case IGMPV2_HOST_MEMBERSHIP_REPORT:
            process_igmp_membership_report(igmp_hdr, skb, device_type, cpe_num);

        break;
        case IGMP_HOST_LEAVE_MESSAGE:  
            process_igmp_leave_group(igmp_hdr, skb, device_type, cpe_num);
        break;
        case IGMP_HOST_MEMBERSHIP_QUERY:
            process_igmp_membership_query(igmp_hdr, skb, device_type);
            break;
        default:
        return 1;
    }

    return 0;
}

/**************************************************************************
 * FUNCTION NAME : ti_igmp_start
 **************************************************************************
 * DESCRIPTION   :
 *  TI Layer2 IGMP start function
 *  
 * RETURNS       :
 *  <0  -   Error
 *   0  -   Success
 *
 **************************************************************************/
int ti_igmp_start (void)
{
    struct timer_list   *timer;
    unsigned long       value = 1;
    unsigned int lockKey;

    /************************************************************************/
    PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);
    /************************************************************************/

    if (ti_igmp_mcb.state == IGMP_START)
    { // ARRIS ADD re-enable interrupts
        /************************************************************************/
        PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
        /************************************************************************/
        return 0;
    } // END ARRIS

#ifdef CONFIG_TI_L2_IGMP_DEBUG
    printk ("DEBUG: TI Layer 2 IGMP Starting\n");
#endif
    /* Initialize IGMP timer. Expiration time 1 second */
    timer = &ti_igmp_mcb.igmp_timer;
    init_timer(timer);
    timer->data = (unsigned long) &value;
    timer->function = ti_igmp_timer_expired;
    timer->expires = jiffies + 1 * HZ;
    add_timer(timer);
    ti_igmp_mcb.state = IGMP_START;
    
    /* open well known  IGMP mac address filter (224.0.0.1) */
    DbridgeDb_AddMcastMac(mcatWkIgmpMac,DYNAMIC_FILTER);

    /************************************************************************/
    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
    /************************************************************************/

    return 0;
}

/**************************************************************************
 * FUNCTION NAME : ti_igmp_stop
 **************************************************************************
 * DESCRIPTION   :
 *  TI Layer2 IGMP stop function
 *  
 * RETURNS       :
 *  <0  -   Error
 *   0  -   Success
 *
 **************************************************************************/
int ti_igmp_stop (void)
{
    struct timer_list   *timer;
    unsigned int lockKey;

    /************************************************************************/
    PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);
    /************************************************************************/

    if (ti_igmp_mcb.state != IGMP_START)
    {
        /************************************************************************/
        PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
        /************************************************************************/
        return 0; /* ARRIS CHANGE - Fix unreachable code */
    }
#ifdef CONFIG_TI_L2_IGMP_DEBUG
    printk ("DEBUG: TI Layer 2 IGMP Stoping\n");
#endif

    /* Close well known  IGMP mac address filter (224.0.0.1)*/
    DbridgeDb_DelMcastMac(mcatWkIgmpMac);

    ti_igmp_mcb.state = IGMP_STOP;
    ti_igmp_mcb.start_timer=0;
    timer = &ti_igmp_mcb.igmp_timer;
    del_timer (timer);

    /* Flush IGMP cache */
    flush_igmp_cache_entry();
    /************************************************************************/
    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
    /************************************************************************/

    return 0;
}


/**************************************************************************
 * FUNCTION NAME : ti_igmp_init
 **************************************************************************
 * DESCRIPTION   :
 *  TI Layer2 IGMP init function
 *  
 * RETURNS       :
 *  <0  -   Error
 *   0  -   Success
 *
 **************************************************************************/
int ti_igmp_init (struct igmp_cfg *cfg)
{
#ifdef CONFIG_TI_L2_IGMP_DEBUG
    printk ("DEBUG: TI Layer 2 IGMP Initializing\n");
#endif
    /* Initialize the Global IGMP Master Control Block */
    memset ((void *)&ti_igmp_mcb, 0, sizeof(ti_igmp_mcb));

    /* Initialize IGMP configuration */
    initialize_igmp_configuration(cfg);
    ti_igmp_mcb.state = IGMP_INIT;
    init_user_nl();

    return 0;
}

