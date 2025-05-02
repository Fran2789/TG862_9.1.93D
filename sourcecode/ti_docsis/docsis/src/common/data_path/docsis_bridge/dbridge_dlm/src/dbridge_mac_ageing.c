/*
 *
 * dbridge_mac_ageing.c
 * Description:
 * DOCSIS bridge MAC ageing implementation
 *
 * BSD LICENSE 
 *
 *  Copyright(c) 2012 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright 
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright 
 *     notice, this list of conditions and the following disclaimer in 
 *     the documentation and/or other materials provided with the 
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its 
 *     contributors may be used to endorse or promote products derived 
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR 
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY 
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#define _DBRIDGE_MAC_AGEING_C_

/*! \file dbridge_mac_ageing.c
    \brief the Docsis Bridge MAC ageing support
*/

/**************************************************************************/
/*      INCLUDES:                                                         */
/**************************************************************************/
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/list.h>
#include <linux/mm.h>
#include <linux/socket.h>
#include <linux/un.h>
#include <net/sock.h>
#include <net/inet_common.h>
#include <net/netlink.h>
#include <linux/netlink.h>
#include "dbridge_mac_ageing.h"
#include "dbridge_db.h"
#include "dbridge_common.h"

/**************************************************************************/
/*      EXTERNS Declaration:                                              */
/**************************************************************************/
 
/**************************************************************************/
/*      DEFINES:                                                          */
/**************************************************************************/

#define DBRIDGE_MAC_AGEING_DEBUG

#ifdef DBRIDGE_MAC_AGEING_DEBUG
/* note: prints function name for you */
#ifdef DBRIDGE_LOG
#  define DPRINTK(fmt, args...) \
        if(dbridgeDbgLevel & DBRIDGE_DBG_L2VPN_DS) printk(KERN_INFO "%s: " fmt, __FUNCTION__ , ## args)
#else
#  define DPRINTK(fmt, args...)
#endif
#else
#  define DPRINTK(fmt, args...)
#endif 

#define MAC_AGEING_STATE_NULL   0
#define MAC_AGEING_IDLE   1
#define MAC_AGEING_ACTIVE  2

#define MAC_AGEING_MAX_DEV  32
#define MAC_AGEING_TIMER_VALUE_DEFAULT  2
#define NL_MAC_LEARNING_CONTROL 102


typedef struct DbridgeMacAgeingState
{
    Uint32 state;
    struct timer_list mac_ageing_timer;
    Uint32 sys_if_Index;
         
}DbridgeMacAgeingState_t;

typedef struct DbridgeMacAgeingControlDb
{
    Bool mac_ageing_enable;
    Uint32 timer_value;
    DbridgeMacAgeingState_t devSm[MAC_AGEING_MAX_DEV];
         
}DbridgeMacAgeingControlDb_t;

/**************************************************************************/
/*      LOCAL DECLARATIONS:                                               */
/**************************************************************************/
static void DbridgeMacAgeing_timer_expired(unsigned long data); 
static Int32 DbridgeMacAgeing_get_dbIndex(Uint32 sysIndex);
static void DbridgeMacAgeing_notify_user_nl(Uint32 docsisIndex, DbridgeMacList_t *deletedMacList);

/**************************************************************************/
/*      LOCAL VARIABLES:                                                  */
/**************************************************************************/

DbridgeMacAgeingControlDb_t macAgeingDb;

/**************************************************************************/
/*      LOCAL FUNCTIONS Implementation:                                   */
/**************************************************************************/

/**************************************************************************/
/*! \fn static int DbridgeMacAgeing_timer_expired(unsigned long data)                                    
 **************************************************************************
 *  \brief mac ageing timer expires handler.
 *  \param[in] data - data attached to timer.
 *  \return none.
 **************************************************************************/
static void DbridgeMacAgeing_timer_expired(unsigned long data)
{
    Uint32 lockKey;
    DbridgeDb_FieldMask_e field_mask = (DBRALT_MASK_LEARNFROM | DBRALT_MASK_SRCDEV | DBRALT_MASK_DEVTYPE | DBRALT_MASK_ALL);
    struct net_device *dev = NULL;
    Uint32 sysIfIndex = macAgeingDb.devSm[data].sys_if_Index;
    Uint32 docsisIfIndex = 0;
    Char devName[IFNAMSIZ];
    Uint32 dbIndex = data;
    DbridgeMacList_t macList;

    dev = dev_get_by_index(&init_net, sysIfIndex);

    if (!dev)
    {
        printk (KERN_ERR"unable to find device\n");
        return;
    }

    memcpy(devName, dev->name, IFNAMSIZ);
    dev_put(dev);

    /************************************************************************/
    DBRALT_PROT_ON(lockKey);
    /************************************************************************/

    if (macAgeingDb.devSm[dbIndex].state != MAC_AGEING_ACTIVE)
    {
        DPRINTK(KERN_INFO"timer expired for %s not in active state\n",devName);
        /************************************************************************/
        DBRALT_PROT_OFF(lockKey);
        /************************************************************************/
        return;
    }

    /*set state to idle*/
    macAgeingDb.devSm[dbIndex].state = MAC_AGEING_IDLE;    

    memset(&macList, 0, sizeof(DbridgeMacList_t));
    /*delete mac addresses associated to dev name*/
    DbridgeDb_AltDelMac(NULL, CPE_DYNAMIC, NULL, devName, 0, DBR_CMCI_NET_DEV_TYPE, field_mask, &macList);        

    /*in case of deleting mac address*/
    if (macList.index)
    {
        if (DbridgeCommon_GetDocsisIfIndexBySysIndexCB)
        {
            DbridgeCommon_GetDocsisIfIndexBySysIndexCB(sysIfIndex, &docsisIfIndex);
            /*send notification to user*/
            DbridgeMacAgeing_notify_user_nl(docsisIfIndex, &macList);
        }
    }

    /************************************************************************/
    DBRALT_PROT_OFF(lockKey);
    /************************************************************************/
    return;
}

/**************************************************************************/
/*! \fn static Int32 DbridgeMacAgeing_get_dbIndex(Uint32 sysIndex)                                  
 **************************************************************************
 *  \brief get the DB index according to sys IF Index or a free place.
 *  \param[in] sysIndex - sys IF Index.
 *  \return DB index or error.
 **************************************************************************/
static Int32 DbridgeMacAgeing_get_dbIndex(Uint32 sysIndex)
{
    Uint32 dbIndex;

    for (dbIndex = 0; dbIndex < MAC_AGEING_MAX_DEV; dbIndex++)
    {
        if (macAgeingDb.devSm[dbIndex].state == MAC_AGEING_STATE_NULL)
        {
            macAgeingDb.devSm[dbIndex].sys_if_Index = sysIndex;
            return dbIndex;
        }
        else if (macAgeingDb.devSm[dbIndex].sys_if_Index == sysIndex)
        {
            return dbIndex;
        }
    }

    return -1;
}

/**************************************************************************/
/*! \fn static void DbridgeMacAgeing_notify_user_nl(Uint32 docsisIfIndex)                                 
 **************************************************************************
 *  \brief send message to user using NL socket.
 *  \param[in] docsisIfIndex - docsis IF Index.
 *  \return DB index or error.
 **************************************************************************/
static void DbridgeMacAgeing_notify_user_nl(Uint32 docsisIfIndex, DbridgeMacList_t *deletedMacList)
{
    struct sk_buff *skb;
    struct nlmsghdr *nlh;
    struct ti_docsis_nlmsg *msg;
    struct sock *nlsk = DbridgeDb_GetNlSocket();
    Int32 nl_pid = DbridgeDb_GetNlPid();
  
    if ((!nl_pid) || (!nlsk))
    {
        return;
    }

    skb = alloc_skb(NLMSG_SPACE(sizeof(struct ti_docsis_nlmsg)), GFP_KERNEL);
    if(!skb)
    {
        goto nlmsg_failure;
    }

    nlh = NLMSG_PUT(skb, nl_pid, 0, NETLINK_TI_DOCSIS, sizeof(struct ti_docsis_nlmsg));
    msg = NLMSG_DATA(nlh);
    msg->type = NL_MAC_LEARNING_CONTROL;
    if (deletedMacList)
    {
        msg->data.macData = *deletedMacList;
    }
    else
    {
        msg->data.macData.index = 0;
    }
    msg->data.macData.ifIndex = docsisIfIndex;
    NETLINK_CB(skb).pid = 0;
    NETLINK_CB(skb).dst_group = 0;

    netlink_unicast(nlsk, skb, nl_pid, 0);
    return;

nlmsg_failure:
    printk (KERN_ERR " DbridgeMacAgeing_notify_user_nl: skb_put failure\n");
}

/**************************************************************************/
/*      INTERFACE FUNCTIONS Implementation:                               */
/**************************************************************************/

/**************************************************************************/
/*! \fn int DbridgeMacAgeing_Init(void)                                     
 **************************************************************************
 *  \brief Docsis bridge MAC Ageing initialization.
 *  \param[in] no input.
 *  \param[out] no output.
 *  \return OK or error status.
 **************************************************************************/
int DbridgeMacAgeing_Init(void)
{
    Uint32 lockKey;

    /************************************************************************/
    DBRALT_PROT_ON(lockKey);
    /************************************************************************/

    memset(&macAgeingDb, 0, sizeof(DbridgeMacAgeingControlDb_t));
    macAgeingDb.timer_value = MAC_AGEING_TIMER_VALUE_DEFAULT;

    /************************************************************************/
    DBRALT_PROT_OFF(lockKey);
    /************************************************************************/
    DPRINTK(KERN_INFO"\nInit Dbridge MAC Ageing done\n");
    
	return 0;
}

/**************************************************************************/
/*! \fn int DbridgeMacAgeing_HandleEvent(Bool isOperUp, Char *name)                                    
 **************************************************************************
 *  \brief Docsis bridge MAC Ageing initialization.
 *  \param[in] no input.
 *  \param[out] no output.
 *  \return OK or error status.
 **************************************************************************/
int DbridgeMacAgeing_HandleEvent(Bool isOperUp, Uint32 sysIndex)
{
    struct timer_list *timer;
    Uint32 lockKey;
    Int32 dbIndex = DbridgeMacAgeing_get_dbIndex(sysIndex);
    Uint32 docsisIfIndex = 0;

    DPRINTK(KERN_INFO"Handle link up/down Event dbIndex %d sysIndex %d\n",dbIndex,sysIndex);

    if (dbIndex < 0)
    {
        printk(KERN_ERR "unable to handle MAC ageing event\n");
        return -1;
    }

    /************************************************************************/
    DBRALT_PROT_ON(lockKey);
    /************************************************************************/

    /*check if mac learning control enable*/
    if (macAgeingDb.mac_ageing_enable)
    {
        /*indication that operation status on*/
        if (isOperUp)
        {
            /*if timer of this device is active - stop timer and set to IDLE*/
            if (macAgeingDb.devSm[dbIndex].state == MAC_AGEING_ACTIVE)
            {
                DPRINTK(KERN_INFO"delete timer for DB index %d\n",dbIndex);
                timer = &(macAgeingDb.devSm[dbIndex].mac_ageing_timer);
                del_timer (timer);
                macAgeingDb.devSm[dbIndex].state = MAC_AGEING_IDLE;
            }

            if (DbridgeCommon_GetDocsisIfIndexBySysIndexCB)
            {
                DbridgeCommon_GetDocsisIfIndexBySysIndexCB(sysIndex, &docsisIfIndex);
                /*send notification to user*/
                DbridgeMacAgeing_notify_user_nl(docsisIfIndex, NULL);
            }
        }
        else /*indication that operation status off */
        {
            /* in case there is no active timer for this device */
            if (macAgeingDb.devSm[dbIndex].state != MAC_AGEING_ACTIVE)
            {
                macAgeingDb.devSm[dbIndex].state = MAC_AGEING_ACTIVE;
                
                /*check configured timer value*/
                if (macAgeingDb.timer_value)
                {
                    /* start timer */
                    DPRINTK(KERN_INFO"start timer for DB index %d\n",dbIndex);
                    timer = &(macAgeingDb.devSm[dbIndex].mac_ageing_timer);
                    init_timer(timer);
                    timer->data = dbIndex;
                    timer->function = DbridgeMacAgeing_timer_expired;
                    timer->expires = jiffies + macAgeingDb.timer_value * HZ;
                    add_timer(timer);                    
                }
                else
                {
                    DbridgeMacAgeing_timer_expired(dbIndex);                    
                }
            }
        }
    }

    /************************************************************************/
    DBRALT_PROT_OFF(lockKey);
    /************************************************************************/    
    return 0;
}

/**************************************************************************/
/*! \fn int DbridgeMacAgeing_SetStatusAndTimer(Bool macAgeingEnable, Uint32 timerValue)                                    
 **************************************************************************
 *  \brief set Docsis bridge MAC Ageing configuration.
 *  \param[in] macAgeingEnable - enable / disable status.
 *  \param[in] timerValue - holdoff timer value.
 *  \return OK or error status.
 **************************************************************************/
int DbridgeMacAgeing_SetStatusAndTimer(Bool macAgeingEnable, Uint32 timerValue)
{
    Uint32 lockKey;

    /************************************************************************/
    DBRALT_PROT_ON(lockKey);
    /************************************************************************/

    macAgeingDb.mac_ageing_enable = macAgeingEnable;
    macAgeingDb.timer_value = timerValue;

    /************************************************************************/
    DBRALT_PROT_OFF(lockKey);
    /************************************************************************/

    return 0;
}

