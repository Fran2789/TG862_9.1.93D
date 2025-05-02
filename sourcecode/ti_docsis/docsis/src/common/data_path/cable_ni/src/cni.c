/*
 *
 * cni.c
 * Description:
 * DOCSIS datapath driver implementation
 *
 *
 * GPL LICENSE SUMMARY
 *
 *  Copyright(c) 2008-2014 Intel Corporation.
 *
 *  This program is free software; you can redistribute it and/or modify 
 *  it under the terms of version 2 of the GNU General Public License as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful, but 
 *  WITHOUT ANY WARRANTY; without even the implied warranty of 
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU 
 *  General Public License for more details.
 *
 *
 *  You should have received a copy of the GNU General Public License 
 *  along with this program; if not, write to the Free Software 
 *  Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *  The full GNU General Public License is included in this distribution 
 *  in the file called LICENSE.GPL.
 *
 *  Contact Information:
 *  Intel Corporation
 *  2200 Mission College Blvd.
 *  Santa Clara, CA  97052
 */

#define DRV_NAME    "Cable Modem Network Interface driver"
#define DRV_VERSION    "0.0.2"

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/mii.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/irq.h>
#include <linux/proc_fs.h>
#include <linux/inet_lro.h>
#include <linux/ethtool.h>

// ARRIS ADD
// LRO PATCH
#ifdef CONFIG_INET_LRO
#include <linux/inet_lro.h>
#endif
// SOCKET FILTER
#include <linux/tcp.h>
#include <linux/ip.h>
#include <linux/in.h>

#include "cni.h"                // ARRIS ADD : Private IOCTL definitions

#include <arris/arris_mod_api.h>  /* ARRIS ADD : new kernel hooks API */
// END ARRIS

#include "pal.h"
#include "pal_cppi41.h"

extern int DBridge_receive (struct sk_buff *skb);

/* Enable only ONE of the following */
/* Determines the context used for received packets */
//#define CNI_USE_TASKLET
//#define CNI_USE_NAPI
#define CNI_USE_WORKQ

//#define CNI_USE_TASKLET_HIGH
#define CNI_USE_NAPI_HIGH
//#define CNI_USE_WORKQ_HIGH


/* Enable only ONE of the following */
/* Determines the method used to receive a packet */
/* NETIF - The packet is transferred to the networking infrastructure, is visible using "tcpdump -i cni0" */
/* DBRIDGE - The packet is transferred directly to the Dbridge module, is NOT visible using "tcpdump -i cni0" */
//#define CNI_RX_NETIF
#define CN_RX_DBRIDGE

#if defined(CNI_USE_WORKQ) || defined(CNI_USE_WORKQ_HIGH)
#include <linux/workqueue.h>
#include <linux/sched.h>

typedef struct
{
    struct work_struct work;
    struct net_device *dev;
} cni_rx_work_t;
#endif

#ifdef CONFIG_MACH_PUMA6
#include <puma6_cppi.h>
#else
#include <puma5_cppi.h>
#endif

#ifdef CONFIG_TI_PACKET_PROCESSOR /* For PID base config */
#ifdef CONFIG_MACH_PUMA6
#include <puma6_pp.h>
#include "avalanche_pp_api.h"
#else
#include <puma5_pp.h>
#endif
#endif

/*
 * Related session router definitions
 * ----------------------------------
 */

#ifdef CONFIG_TI_PACKET_PROCESSOR
static int cni_pp_set_pid_discard (struct net_device *dev);
static int cni_pp_set_pid_active  (struct net_device *dev);
#endif

/* Cable network interface driver */

extern struct net_device * wanDev;
extern struct net_device * mtaDev;

#define CNID_LOW_PRIORITY           0
#define CNID_HIGH_PRIORITY          1

/* ARRIS ADD START - FROM SERCOMM */
#ifndef CONFIG_VENDOR_COMCAST
#ifdef INCLUDE_SERCOMM_GW
int (*sc_modify_dest_mac_hook)(struct sk_buff *skb) = NULL; 
int (*sc_modify_src_mac_hook)(struct sk_buff *skb) = NULL; 
EXPORT_SYMBOL(sc_modify_src_mac_hook);
EXPORT_SYMBOL(sc_modify_dest_mac_hook);
#endif /* INCLUDE_SERCOMM_GW */
#endif /* Comcast */
ARRISMOD_DEFINE( int, arris_rip_check_src_mac_hook, struct sk_buff* );
ARRISMOD_DEFINE( int, arris_rip_check_dst_mac_hook, struct sk_buff* );
/* ARRIS ADD END  */

#define CNID_INTD_HOST_NUM          0
#define CPPI4_BD_LENGTH_FOR_CACHE   64

#define CNID_TX_SERVICE_MAX         32
#define CNID_RX_SERVICE_MAX         64
/* ARRIS MOD : increase the number of packets that can be processed on the CNI interface in a single NAPI POLL */
/* Normally, this number is very low (10 packets or so), as the PP will kick in, and we stop seeing packets on 
   this interface.  During speedtest, the PP is never involved, and we easily hit 150+ consecutive packets in a poll */
#define CIND_RX_SERVICE_MAX         192
/* END ARRIS MOD */

#define CNID_ACC_PAGE_NUM_ENTRY     (32)
#define CNID_ACC_NUM_PAGE           (2)
#define CNID_ACC_ENTRY_TYPE         (PAL_CPPI41_ACC_ENTRY_TYPE_D)
/* Byte size of page = number of entries per page * size of each entry */
#define CNID_ACC_PAGE_BYTE_SZ       (CNID_ACC_PAGE_NUM_ENTRY * ((CNID_ACC_ENTRY_TYPE + 1) * sizeof(unsigned int*)))
/* byte size of list = byte size of page * number of pages * */
#define CNID_ACC_LIST_BYTE_SZ       (CNID_ACC_PAGE_BYTE_SZ * CNID_ACC_NUM_PAGE)


#define CNID_QM_DESC_SIZE_CODE      10 /* ((sizeof(Cppi4HostDesc) - 24)/4) */

/* define to enable copious debugging info */
//#define CNID_DEBUG

#ifdef CNID_DEBUG
/* note: prints function name for you */
#  define DPRINTK(fmt, args...) printk("%s: " fmt, __FUNCTION__ , ## args)
#else
#  define DPRINTK(fmt, args...)
#endif

#ifdef CONFIG_MACH_PUMA6
#define QOS_MAX_US_SF_BE_NUMBER             (8) /* Intended to be 16 ... */ /*max number of supported upstream best effort SF*/
#else
#define QOS_MAX_US_SF_BE_NUMBER             (8) /*max number of supported upstream best effort SF*/
#endif

/// *** MTA_LIMIT_HIGH_PRIORITY_RATE ***
#if defined(CNI_USE_TASKLET_HIGH) || defined(CNI_USE_NAPI_HIGH)
/* If neither CNI_USE_TASKLET_HIGH or CNI_USE_NAPI_HIGH are used there's no need for this feature - 
   mta0 traffic will not get prioritized anyways. */

#ifdef CNI_USE_WORKQ
/* please note that enabling this feature is only viable when using CNI_USE_WORKQ
   if not (Rx low using TASKLET or NAPI) redirecting the traffic won't help and it'll actually
   hinder performance by adding more overhead to packet handling when the solution will do absolutly nothing. */

#if defined(CONFIG_MACH_PUMA5) || defined(CONFIG_MACH_PUMA6) /* FW support implemented for PUMA5 and PUMA6 */

#define MTA_LIMIT_HIGH_PRIORITY_RATE /* comment to turn off feature */

/* This Feature addresses the issue of possible flooding of the mta0 traffic.
   This is critical since PP will send mta0 traffic to high priority queue thus possibly denying CPU from system
   critical processes and resulting in a CM reboot.

   Inorder to counter it we will set a limit on the fastest mta0 traffic allowed. This is integrated with PP fw,
   if mta0 packets are arriving too fast we will notify PP to not send mta0 traffic through the high priority queue.
   Then we will moniter the diverted mta0 traffic, if drops below our threshold we will re-enable the high priority queue. 
   We check for rate violation by setting a check point on mta0 packet count. When this count is reached we will calculate
   the time span of said packet count. If it's less than a threshold we set, we consider this a rate violation. 

   Please Note: Packet Count and Min Time parameters determining highest allowed rate are also changeable via kernel shell.
   
   * To view values currently set use:
        cat /sys/module/avalanche_cnid/parameters/<param>
   * To change values use:
        echo <desired value> > /sys/module/avalanche_cnid/parameters/<param>

        where <param> = variable name [so mtaPacketCheckMinTimeInJiffies or mtaPacketCountCheck]

   Please Note(2): one can read isMtaRateHigh and mtaHighQueueToggleCounter from kernel shell for debugging or statitics.

   * To view values use [these are read-only]:
        cat /sys/module/avalanche_cnid/parameters/<param>
*/

#endif
#endif
#endif
/// ***********************************

#ifdef MTA_LIMIT_HIGH_PRIORITY_RATE /* this is only definable under certain conditions [check the #define for more details]. */
#define MTA_PACKET_COUNT_CHECK 1024 /* initial value for reset turnaround for mta0 packet counter. When this amount of packets arrives
                                       we will check for rate violation. */

#define MTA_PACKET_CHECK_MIN_TIME_MSEC 1000 /* initial minimum time interval (in msec) allowed for the defined packet count to arrive in.
                                               If below this value that means then packets are arriving too fast, possibly being an attack. */

/* Both above defines are initial values since we copy them to static variables which are later used [so they can be changed via kernel shell]. */

//#define MTA_LIMIT_HIGH_PRIORITY_RATE_DBG /* for mta0 rate limiting related debug prints */

#ifdef MTA_LIMIT_HIGH_PRIORITY_RATE_DBG
/* note: prints feature Header and '\n' is included inside */
#  define DPRINTK_MTA(fmt, args...) printk("mta0 rate limit: " fmt "\n" , ## args)
#else
#  define DPRINTK_MTA(fmt, args...)
#endif
#endif

MODULE_AUTHOR ("Texas Instruments, Inc");
MODULE_DESCRIPTION (DRV_NAME);
MODULE_LICENSE("GPL");
MODULE_VERSION(DRV_VERSION);


/************************************************************************/
/*     CNI driver private data                                          */
/************************************************************************/
typedef struct cnid_private_t {
    // ARRIS ADD : LRO PATCH
#ifdef CONFIG_INET_LRO
#define CNI_MAX_LRO_ENTRIES_LOW  16
    struct net_lro_mgr      lro_mgr_low; /* This entry must be first */
    struct net_lro_desc     lro_arr_low[CNI_MAX_LRO_ENTRIES_LOW];

#define CNI_MAX_LRO_ENTRIES_HIGH  8    
    struct net_lro_mgr      lro_mgr_high; /* This entry must be first */
    struct net_lro_desc     lro_arr_high[CNI_MAX_LRO_ENTRIES_HIGH];
#endif
    // END ARRIS

    struct net_device       *netdev;

#if defined(CNI_USE_NAPI)
    struct napi_struct      napi;
#elif defined(CNI_USE_TASKLET)
    struct tasklet_struct   rx_tasklet;         /* RX completion processing tasklet */
#elif defined(CNI_USE_WORKQ)
    struct workqueue_struct *cni_rx_wq;
    cni_rx_work_t           cni_rx_work;
#endif

#if defined(CNI_USE_NAPI_HIGH)
    struct napi_struct      napi_high;
#elif defined(CNI_USE_TASKLET_HIGH)
    struct tasklet_struct   rx_tasklet_high;
#elif defined(CNI_USE_WORKQ_HIGH)
    struct workqueue_struct *cni_rx_wq_high;
    cni_rx_work_t           cni_rx_work_high;
#endif

    PAL_Handle              pal_hnd;        /* The handle to PAL layer */

#ifdef CONFIG_MACH_PUMA5
    PAL_Cppi4AccChHnd       tx_acc_hnd;     /* The Tx accumulator channel handle */
    Ptr                     tx_list_base;
    Cppi4HostDescLinux**    tx_list;

    struct tasklet_struct   tx_tasklet;     /* Tx completion processing tasklet */
#endif

    PAL_Cppi4QueueHnd       tx_queue            [PAL_CPPI41_SR_DOCSIS_TX_DATA_Q_COUNT];

    PAL_Cppi4QueueHnd       tx_qos_queue        [PAL_CPPI41_SR_HOST_TO_PP_Q_COUNT];
    PAL_Cppi4QueueHnd       tx_qos_free_queue   [PAL_CPPI41_SR_HOST_TO_PP_Q_COUNT];

    /*
     * CNI Rx Accumulator info
     */
    unsigned int            rxAcc_chan              [PAL_CPPI41_SR_CNI_INFRA_DMA_CH_COUNT]; /* The Rx accumulator channel numbers */
    PAL_Cppi4AccChHnd       rxAcc_chan_hnd          [PAL_CPPI41_SR_CNI_INFRA_DMA_CH_COUNT]; /* The Rx accumulator channel handle */
    Cppi4HostDescLinux**    rxAcc_chan_list         [PAL_CPPI41_SR_CNI_INFRA_DMA_CH_COUNT]; /* Rx acc channel lists */
    Ptr                     rxAcc_chan_list_base    [PAL_CPPI41_SR_CNI_INFRA_DMA_CH_COUNT]; /* Rx acc channel lists base*/

    /*
     * CNI Proxy channels info
     */
    PAL_Cppi4QueueHnd       rxProxy_free_queue_hnd  [PAL_CPPI41_SR_CNI_INFRA_DMA_CH_COUNT]; /* Rx Proxy free queue handles */

    struct net_device_stats stats;
    spinlock_t              devlock;
    unsigned long           state;
    // ARRIS ADD
    // LRO PATCH
#ifdef CONFIG_INET_LRO
    int                     lro_complete;
#endif
    // SOCKET FILTER and DOCSIS BRIDGE BYPASS
    unsigned int            Ipv4FilterAddress;
    unsigned int            Ipv4FilterPort;
    struct net_device_stats filtered_stats;
    int                     DbridgeBypass;
    // END ARRIS
}
cnid_private_t;
/************************************************************************/

#ifdef MTA_LIMIT_HIGH_PRIORITY_RATE
static unsigned long mtaPacketCheckMinTimeInJiffies = 0; /* MTA_PACKET_CHECK_MIN_TIME_MSEC converted to jiffies. 
                                                            We have this field in addition to the define because the reset time stamps will be in jiffies
                                                            and we don't want to convert the define to jiffies to msec for each calculation.

                                                            Note: this paramter can also be changed via kernel shell.
                                                                  * when changing take note that this is in jiffies!!! */

module_param(mtaPacketCheckMinTimeInJiffies, ulong, S_IRUGO | S_IWUSR);

static unsigned int mtaPacketCountCheck = 0; /* sets mta0 packet counter turnaround. 
                                                Note: this paramter can also be changed via kernel shell. */

module_param(mtaPacketCountCheck, uint, S_IRUGO | S_IWUSR);

static int mtaPacketCounter = 0;
static unsigned long lastMtaCounterResetTimeInJiffies = 0; /* We log the time stamp when last counter reset was made, will use this for rate evaluation. 
                                                              It is possible for first run time will not be as accurate to save if checks when possible
                                                              so first loop of mta0 high traffic might get missed. If it is indeed an attack not so 
                                                              serious since will be much longer than 1 loop and will get picked up on the second loop. */

static Bool isMtaRateHigh = False;          /* mta0 high rate was detected - PP was notified to disable high priority queue for mta0 traffic. */
module_param(isMtaRateHigh, bool, S_IRUGO); /* read-only 'cat'-able kernel param */

static unsigned int mtaHighQueueToggleCounter = 0;       /* This will count how many times PP changed states between Enable and Disable High Priority Queue
                                                            Note: this will always start at 1 as there is a first set to 0 at rx_open [to make sure default 
                                                            is enable high priority queue]. */

module_param(mtaHighQueueToggleCounter, uint, S_IRUGO);  /* read-only 'cat'-able kernel param */

#endif
/* Use this MAC address if none is supplied on the command line */
static unsigned char defmac[] = {0x00, 0x50, 0xF1, 0x80, 0x00, 0x00};
/* Allocation for MAC string supplied on the command line */
static char *inpmac = NULL;

static int __devinit    cnid_probe              (struct device *        dev);
static int __devexit    cnid_remove             (struct device *        dev);
static int              cnid_open               (struct net_device *    dev);
#if 0
static void             cnid_tx_timeout         (struct net_device *    dev);
#endif
static int              cnid_start_xmit         (struct sk_buff *skb, struct net_device *dev);
#ifdef CNI_USE_NAPI
static int              cnid_poll_low           (struct napi_struct *napi, int budget);
#endif
#ifdef CNI_USE_NAPI_HIGH
static int              cnid_poll_high          (struct napi_struct *napi, int budget);
#endif
static int              cnid_close              (struct net_device *dev);
static int              cnid_ioctl              (struct net_device *dev, struct ifreq *rq, int cmd);
static struct net_device_stats *cnid_get_stats  (struct net_device *dev);
static void             cnid_set_multicast      (struct net_device *dev);
#ifdef CONFIG_MACH_PUMA5
static irqreturn_t      cnid_tx_interrupt       (int irq, void *dev);
static void             cnid_do_tx_complete     (unsigned long data);
#endif
static irqreturn_t      cnid_rx_interrupt       (int irq, void *dev);

static int              cnid_close_rx_prep      (struct net_device* dev);
static int              cnid_close_rx_finish    (struct net_device* dev);
#ifdef CONFIG_INET_LRO
#ifdef CONFIG_MACH_PUMA6    // CJL-PUMA5 TODO
static void             cnid_set_ethtool_ops(struct net_device *dev);
#endif
#endif
#if defined(CONFIG_MACH_PUMA5) || defined(CONFIG_MACH_PUMA6)
static int 				cni_pp_set_TurnOffHighPriorityQueue(Uint8 val);
#endif

/*
 * TODO: This should not be here. It should be in a board/platform
 * specific file which is *actually* aware of what devices are
 * present on board.
 */
static ssize_t cnid_show_version(struct device_driver *drv, char *buf)
{
    return sprintf(buf, "%s, version: %s", DRV_NAME, DRV_VERSION);
}


static DRIVER_ATTR(version, S_IRUGO, cnid_show_version, NULL);

/**************************************************************************/
/*! \fn         cnid_tx_bd_link_skb
 **************************************************************************
 *
 *  \brief      Links an skb to a Tx Descriptor
 *
 *  \param[in]  Net Device
 *  \param[in]  Descriptor
 *  \param[in]  SK buff
 *  \return     length of final packet
 **************************************************************************/
static unsigned int cnid_tx_bd_link_skb(struct net_device* dev, Cppi4HostDescLinux* bd, struct sk_buff *skb)
{
    unsigned int len = ((skb->len < ETH_ZLEN)?ETH_ZLEN:skb->len);
    unsigned int   queueNum;
    unsigned int   sfIndex = (skb->ti_meta_info >> 24);

    bd->hw.descInfo = (PAL_CPPI4_HOSTDESC_DESC_TYPE_HOST << PAL_CPPI4_HOSTDESC_DESC_TYPE_SHIFT)
                    | (1                                 << PAL_CPPI4_HOSTDESC_PROT_WORD_CNT_SHIFT)
                    | len;
    bd->hw.bufPtr = PAL_CPPI4_VIRT_2_PHYS((unsigned int)skb->data);
    bd->hw.buffLen = len;
    bd->skb = skb;
    bd->psi[0] = skb->ti_meta_info;
    bd->psi[1] = 0;

    if (TI_PPM_EGRESS_QUEUE_INVALID != skb->pp_packet_info.egress_queue)
    {
        queueNum = skb->pp_packet_info.egress_queue;
    }
    else
    {
        queueNum = PAL_CPPI41_SR_DOCSIS_TX_DATA_Q_NUM( 1 + sfIndex );
    }

    /* Set SYNC Q PTID info in egress descriptor */
    if(skb->pp_packet_info.flags == TI_HIL_PACKET_FLAG_PP_SESSION_INGRESS_RECORDED)
    {
        memcpy(&(bd->hw.netInfoWord0), skb->pp_packet_info.ti_epi_header, 8);
        bd->hw.netInfoWord1 &= ~(0xFFFF);
        bd->hw.netInfoWord1 |= queueNum; /* after QPDSP, push to QoS/CNI Queues */

#ifdef CONFIG_MACH_PUMA6
        if (bd->hw.netInfoWord0 & PAL_CPPI4_HOSTDESC_NETINFW0_DIVERT_FLAG_MASK)
        {
            bd->hw.netInfoWord0 = PAL_CPPI4_HOSTDESC_NETINFW0_DO_NOT_DISCARD;
        }
        else
        {
            bd->hw.netInfoWord0 = 0;
        }
        if ((skb->pp_packet_info.pp_session.egress.enable & AVALANCHE_PP_EGRESS_FIELD_ENABLE_TCP_CTRL) != AVALANCHE_PP_EGRESS_FIELD_ENABLE_TCP_CTRL)
        {
            bd->hw.netInfoWord0 |= skb->pp_packet_info.pp_session.session_handle;
        }
        else
        {
            bd->hw.netInfoWord0 |= AVALANCHE_PP_MAX_ACCELERATED_SESSIONS;
        }
#endif
    }
    else
    {
#ifdef CONFIG_MACH_PUMA6
        bd->hw.netInfoWord0 = AVALANCHE_PP_MAX_ACCELERATED_SESSIONS;
#endif
        bd->hw.netInfoWord1 = queueNum;
    }

    PAL_CPPI4_CACHE_WRITEBACK((unsigned long)skb->data, skb->len);
    PAL_CPPI4_CACHE_WRITEBACK((unsigned long)bd, CPPI4_BD_LENGTH_FOR_CACHE);

    return len;
}


/**************************************************************************/
/*! \fn         cnid_rx_bd_link_skb
 **************************************************************************
 *
 *  \brief      Links an skb to an Rx Descriptor
 *
 *  \param[in]  Net Device
 *  \param[in]  Descriptor
 *  \param[in]  SK buff
 *  \return     length of final packet
 **************************************************************************/
static void cnid_rx_bd_link_skb(struct net_device* dev, Cppi4HostDescLinux* bd, struct sk_buff *skb)
{
    /* Invalidate the skb data cache: Do it before the data is presented to the DMA */
    skb_reserve (skb, NET_IP_ALIGN);    /* 16 byte align the IP fields. */
    bd->hw.orgBuffLen  = PAL_CPPI41_SR_CNI_INFRA_FD_HOST_BUFFER_SIZE - NET_IP_ALIGN;
    bd->hw.orgBufPtr   = PAL_CPPI4_VIRT_2_PHYS(skb->data);
    bd->skb            = skb;

    bd->psi[0] = 0;
    bd->psi[1] = 0;
    bd->psi[2] = 0;

    /* Write the BD to the RAM */
    PAL_CPPI4_CACHE_WRITEBACK(bd, CPPI4_BD_LENGTH_FOR_CACHE);

}

/** Tx processing functions start here **/

#ifdef CONFIG_MACH_PUMA5
/**************************************************************************/
/*! \fn         cnid_do_tx_complete
 **************************************************************************
 *
 *  \brief      Links an skb to a Rx Descriptor
 *
 *  \param[in]  Net Device
 *  \return     None
 **************************************************************************/
static void cnid_do_tx_complete(unsigned long data)
{
    struct net_device*      dev = (struct net_device*) data;
    cnid_private_t*         priv = netdev_priv(dev);
    Cppi4HostDescLinux*            bd;
    int packets_processed = 0;
    int done = 1;
    int txPriority;

    DPRINTK(KERN_DEBUG " Enter %s \n", __FUNCTION__);
    while(avalanche_intd_get_interrupt_count(CNID_INTD_HOST_NUM, PAL_CPPI41_TX_COMPLETE_LOW_ACC_CH_NUM))
    {
        while((bd = (Cppi4HostDescLinux*)((unsigned long)*priv->tx_list & QMGR_QUEUE_N_REG_D_DESC_ADDR_MASK)))
        {
            bd = PAL_CPPI4_PHYS_2_VIRT(bd);
            PAL_CPPI4_CACHE_INVALIDATE(bd, CPPI4_BD_LENGTH_FOR_CACHE);

            priv->stats.tx_packets++;
            priv->stats.tx_bytes += bd->skb->len;

            dev_kfree_skb_any(bd->skb);
            bd->skb = NULL;

            /* Queue back the BD to free pool */
            txPriority = bd->psi[2];
            PAL_cppi4QueuePush(priv->tx_qos_free_queue[txPriority] , (Ptr)PAL_CPPI4_VIRT_2_PHYS(bd), CNID_QM_DESC_SIZE_CODE, 0);

            packets_processed++;
            priv->tx_list++;

            /* did enough work, move now.. */
            if(!test_bit(0, &priv->state) && (packets_processed == CNID_TX_SERVICE_MAX))
            {
                done = 0;
                goto out;
            }
        }

        /* Update the list entry for next time */
        priv->tx_list = PAL_cppi4AccChGetNextList(priv->tx_acc_hnd);

        avalanche_intd_set_interrupt_count(CNID_INTD_HOST_NUM, PAL_CPPI41_TX_COMPLETE_LOW_ACC_CH_NUM, 1);
    }

out:

    if(done)
    {
        avalanche_intd_write_eoi(PAL_CPPI41_TX_COMPLETE_ACC_INTV_NUM);
    }
    else
    {
        tasklet_schedule(&priv->tx_tasklet);
    }
}
#endif


/* ARRIS ADD
 * Discard the MLDv2 packet that sent by internal mac, which could cause E6000 learn it as CPE (NULL_IP) 
*/
int arris_cni_check_src_mac(struct sk_buff *skb)
{
    struct ethhdr *eth;
    unsigned char * mac;
    struct net_device* tmp_dev;
    static unsigned char cni0_mac[ETH_ALEN];
    static unsigned char br0_mac[ETH_ALEN];
    static unsigned char lbr0_mac[ETH_ALEN];
    static unsigned char cni0_mac_valid=0;
    static unsigned char br0_mac_valid=0;
    static unsigned char lbr0_mac_valid=0;

    // now only MLDv2 packet sent unexpectly, so only check IPv6
    if (skb->protocol != ETH_P_IPV6)
    {
        return 0;
    }
    
    if ( !cni0_mac_valid )
    {
        tmp_dev = dev_get_by_name(&init_net, "cni0");
        if (tmp_dev)
        {
            if (tmp_dev->flags & IFF_UP)
            {
                memcpy(cni0_mac, tmp_dev->dev_addr, ETH_ALEN);
                cni0_mac_valid = TRUE;
            }
            dev_put(tmp_dev);
        }
    }

    if ( !br0_mac_valid )
    {
        tmp_dev = dev_get_by_name(&init_net, "br0");
        if (tmp_dev)
        {
            if (tmp_dev->flags & IFF_UP)
            {
                memcpy(br0_mac, tmp_dev->dev_addr, ETH_ALEN);
                br0_mac_valid = TRUE;
            }
            dev_put(tmp_dev);
        }
    }

    if ( !lbr0_mac_valid )
    {
        tmp_dev = dev_get_by_name(&init_net, "lbr0");
        if (tmp_dev)
        {
            if (tmp_dev->flags & IFF_UP)
            {
                memcpy(lbr0_mac, tmp_dev->dev_addr, ETH_ALEN);
                lbr0_mac_valid = TRUE;
            }
            dev_put(tmp_dev);
        }
    }

    if (!skb_mac_header_was_set(skb))
    {
        skb_reset_mac_header(skb);
        //cni0 sent packet did not reset mac header, others reset in Dbridge_netDeviceXmit
    }
    
    eth = (struct ethhdr *)skb_mac_header(skb);
    if (eth)
    {
        mac = eth->h_source;
        
        if ( cni0_mac_valid && memcmp(mac, cni0_mac, ETH_ALEN)==0 )
        {
            DPRINTK("discard packet: cni0 mac\n");
            return 1;
        }

        if ( br0_mac_valid && memcmp(mac, br0_mac, ETH_ALEN)==0 )
        {
            DPRINTK("discard packet: br0 mac\n");
            return 1;
        }
        
        if ( lbr0_mac_valid && memcmp(mac, lbr0_mac, ETH_ALEN)==0 )
        {
            DPRINTK("discard packet: lbr0 mac\n");
            return 1;
        }
    }

    return 0;    
}
/* ARRIS ADD END*/

/**************************************************************************/
/*! \fn         cnid_start_xmit
 **************************************************************************
 *
 *  \brief      Transmit Function
 *
 *  \param[in]  SK buff
 *  \param[in]  Net Device
 *  \return     OK or error
 **************************************************************************/
static int cnid_start_xmit (struct sk_buff *skb, struct net_device *dev)
{
    cnid_private_t* priv = netdev_priv(dev);
    Cppi4HostDescLinux*    bd;
    unsigned int len;
    unsigned int prio = 0;

    DPRINTK( " Enter \n");
    if (((wanDev) && (skb->skb_iif == wanDev->ifindex)) ||
        ((mtaDev) && (skb->skb_iif == mtaDev->ifindex)) )
    {
        prio = PAL_CPPI4x_PRTY_HIGH;
    }

    /* ARRIS ADD - discard the MLDv2 packet that sent by br0/lbr0/cni0, which could cause E6000 learn it as CPE (NULL_IP) */
    if (arris_cni_check_src_mac(skb) != 0)
    {
        priv->stats.tx_dropped++;
        dev_kfree_skb_any(skb);
        return NETDEV_TX_OK;
    }
    /* ARRIS ADD END */

    /* get a free Tx descriptor */
    if(!(bd = (Cppi4HostDescLinux *)PAL_cppi4QueuePop(priv->tx_qos_free_queue[prio]) ))
    {
        /* This should not occur in this driver
         * (because of what is done later in this function) */
        priv->stats.tx_dropped++;
        dev_kfree_skb_any(skb);
        return NETDEV_TX_OK;
    }

/* ARRIS ADD START - FROM SERCOMM */
#ifndef CONFIG_VENDOR_COMCAST
#ifdef INCLUDE_SERCOMM_GW
    if(sc_modify_src_mac_hook)
    {
        sc_modify_src_mac_hook(skb);
    }
#endif /* INCLUDE_SERCOMM_GW */
#endif /* Comcast */
    ARRISMOD_CALL( arris_rip_check_src_mac_hook, skb );
/* ARRIS ADD END */

    bd = PAL_CPPI4_PHYS_2_VIRT(bd);
    /* We save the queue for TX complete */
    bd->psi[2] = prio;
    bd->hw.pktInfo &= ~(PAL_CPPI4_HOSTDESC_PKT_RETQMGR_MASK | PAL_CPPI4_HOSTDESC_PKT_RETQNUM_MASK);
    bd->hw.pktInfo |= (PAL_CPPI41_QUEUE_MGR_PARTITION_SR    << PAL_CPPI4_HOSTDESC_PKT_RETQMGR_SHIFT)
#ifdef CONFIG_MACH_PUMA6
                   |  (PAL_CPPI41_SR_HOST_TX_COMPLETE_Q_NUM(prio) << PAL_CPPI4_HOSTDESC_PKT_RETQNUM_SHIFT);
#else
                   |  (PAL_CPPI41_SR_HOST_TX_COMPLETE_LOW_Q_NUM << PAL_CPPI4_HOSTDESC_PKT_RETQNUM_SHIFT);
#endif

    len = cnid_tx_bd_link_skb(dev, bd, skb);

    dev->trans_start = jiffies;

    /* Push to QPDSP Q - then after handle PTID it will be send to the CNI Tx queues */
    PAL_cppi4QueuePush(priv->tx_qos_queue[prio], (Uint32 *) PAL_CPPI4_VIRT_2_PHYS(bd), CNID_QM_DESC_SIZE_CODE, len);

    // ARRIS ADD - SOCKET FILTER
    if (skb->filter_match)
    {
        priv->filtered_stats.tx_packets++;
        priv->filtered_stats.tx_bytes += len;
    }
    // END ARRIS

#ifdef CONFIG_MACH_PUMA6
    priv->stats.tx_packets++;
    priv->stats.tx_bytes += bd->skb->len;
#endif

    return NETDEV_TX_OK;
}

#ifdef CONFIG_MACH_PUMA5
/**************************************************************************/
/*! \fn         cnid_tx_interrupt
 **************************************************************************
 *
 *  \brief      Transmit ISR
 *
 *  \param[in]  IRQ
 *  \param[in]  Device
 *  \param[in]  regs
 *  \return     OK or error
 **************************************************************************/
static irqreturn_t cnid_tx_interrupt (int irq, void *dev)
{
    cnid_private_t* priv = netdev_priv((struct net_device*) dev);

    tasklet_schedule(&priv->tx_tasklet);

    return IRQ_RETVAL(1);
}
#endif

/** Rx processing functions go here **/
// ARRIS ADD : SOCKET FILTER
int CheckIpv4FilterRx(struct sk_buff* rxskb, unsigned int address, unsigned int port)
{
    int match = 0;

    struct ethhdr* ptr_ethhdr = (struct ethhdr *)skb_mac_header(rxskb);
    if (ptr_ethhdr->h_proto == ETH_P_IP)   // 0x86DD = IPv6 
    {
        struct iphdr* ptr_iphdr = (struct iphdr*)(skb_mac_header(rxskb)+ETH_HLEN);
        int len_iphdr = (ptr_iphdr->ihl << 2);

        if ((ptr_iphdr->saddr == address) && (ptr_iphdr->protocol == IPPROTO_TCP))
        {
            struct tcphdr* ptr_tcphdr = (struct tcphdr*)(skb_mac_header(rxskb)+ETH_HLEN+len_iphdr);

            if (ptr_tcphdr->source == port)
            {
                match = 1;
            }
        }
    }

    return match;
}
// END ARRIS

/**************************************************************************/
/*! \fn         cnid_do_rx_complete_low
 **************************************************************************
 *
 *  \brief      Rx Complete handler
 *
 *  \param[in]  Net Device
 *  \param[in]  Processed packets budget
 *  \return     Number of processed packets
 **************************************************************************/
#ifdef CNI_USE_NAPI
static int cnid_do_rx_complete_low(struct net_device* dev, int budget)
#elif defined(CNI_USE_TASKLET)
static void cnid_do_rx_complete_low(unsigned long data)
#elif defined(CNI_USE_WORKQ)
static void cnid_do_rx_complete_low(struct work_struct *work)
#endif
{
#if defined(CNI_USE_TASKLET)
    struct net_device*      dev = (struct net_device*) data;
#elif defined(CNI_USE_WORKQ)
    cni_rx_work_t *curr_work = (cni_rx_work_t *)work;
    struct net_device* dev = curr_work->dev;
#endif
    cnid_private_t* priv = netdev_priv(dev);
    Cppi4HostDescLinux* bd;
    int packets_processed = 0;

#ifdef MTA_LIMIT_HIGH_PRIORITY_RATE
    /* We are in Rx low this will be used to determine if rate is low again
       if so we can notify PP to re-enable high priority for mta0 traffic. */
    unsigned long mtaTimeDiffInJiffies = 0;
#endif

#if defined(CNI_USE_WORKQ)
    static int first = 1;
    struct task_struct *ctask = current;

    if (first)
    {
        /* Set priority */
        set_user_nice(current, 1);
        first = 0;

        printk("**** ---- >>>> cnid_do_rx_complete_low - name \"%s\", pid %d\n", ctask->comm, ctask->pid);
    }
#endif

    while(avalanche_intd_get_interrupt_count(CNID_INTD_HOST_NUM, priv->rxAcc_chan[CNID_LOW_PRIORITY]) && (packets_processed < CNID_RX_SERVICE_MAX))
    {
        while((bd = (Cppi4HostDescLinux*)((unsigned long)*priv->rxAcc_chan_list[CNID_LOW_PRIORITY] & QMGR_QUEUE_N_REG_D_DESC_ADDR_MASK)))
        {
            struct sk_buff *newskb;

            bd = PAL_CPPI4_PHYS_2_VIRT(bd);

            /* get a new skb for this bd */

            if(!test_bit(0, &priv->state))
            {   /* if not cleaning up .. */

                if((newskb = dev_alloc_skb(PAL_CPPI41_SR_CNI_INFRA_FD_HOST_BUFFER_SIZE)))
                { /* .. and able to get a new buffer */

                    struct sk_buff* rxskb;

                    PAL_CPPI4_CACHE_INVALIDATE(bd, CPPI4_BD_LENGTH_FOR_CACHE);
                    rxskb = bd->skb;

                    PAL_CPPI4_CACHE_INVALIDATE(rxskb->data, bd->hw.buffLen - 4);
                    skb_put(rxskb, bd->hw.buffLen - 4); /* remove CRC from length */
                    dev->last_rx = jiffies;
                    priv->stats.rx_packets++;
                    priv->stats.rx_bytes += bd->hw.buffLen;

                    /* Keep SYNC Q PTID info in skb for egress */
                    if(bd->hw.netInfoWord1)
                    {
                        memcpy(rxskb->pp_packet_info.ti_epi_header, &(bd->hw.netInfoWord0), 8);
                        rxskb->pp_packet_info.flags = TI_HIL_PACKET_FLAG_PP_SESSION_INGRESS_RECORDED;
                    }

                    /* ... then send the packet up */
                    {
                        /* This is the DOCSIS Interface; pass the packet to the DOCSIS Bridge; initialize the
                        * MAC RAW Pointer before doing so. */
                        skb_reset_mac_header(rxskb);
                        rxskb->dev = dev;
                        rxskb->ti_meta_info = bd->psi[0];
                        rxskb->ti_meta_info2 = bd->psi[2];
                        /* ARRIS ADD START */
                        bool bridgebypass = FALSE;
                        /* ARRIS ADD END */
                        //ARRIS ADD START
                        ARRISMOD_CALL( arris_rip_check_dst_mac_hook, rxskb );
                        //ARRIS ADD END	
#ifdef RX_TIME_MEASUREMENTS // This will not work without matching DS FW and adding ti_meta_info1 to skbuff.h
                        rxskb->ti_meta_info1 = bd->psi[1];
                        {
                            #define ARRAY_NUM_ENTRIES           10
                            #define ARRAY_RESOLUTION_IN_MSEC    10 // msec
                            #define REPORT_EACH_PACKET
                            #define REPORT_RESOLUTION_IN_SEC    5 // seconds
                            int pktsTimestamp;
                            int currTimestamp;
                            unsigned int timediff;
                            unsigned int index;
                            static int lastTimestamp = 0;

                            static unsigned int timeDiffArray[ARRAY_NUM_ENTRIES] = {0};


                            pktsTimestamp = rxskb->ti_meta_info1;
                            currTimestamp = *((int*)IO_ADDRESS(0x02438114));
                            timediff = currTimestamp - pktsTimestamp;
                            #ifdef REPORT_EACH_PACKET
                            printk("CNI: pktsTimestamp=0x%08X, currTimestamp=0x%08X, timediff=0x%08X(%d) (%d msec)\n", pktsTimestamp, currTimestamp, timediff, timediff, (int)(timediff/10240));
                            #endif
                            index = timediff/(10240*ARRAY_RESOLUTION_IN_MSEC);
                            if (index > ARRAY_NUM_ENTRIES-1)
                            {
                                index = ARRAY_NUM_ENTRIES-1;
                            }
                            timeDiffArray[index]++;

                            if (lastTimestamp == 0)
                            {
                                lastTimestamp = currTimestamp;
                            }
                            else if (currTimestamp - lastTimestamp > (10240000 * REPORT_RESOLUTION_IN_SEC))
                            {
                                if (lastTimestamp != 0)
                                {
                                    int i;
                                    for (i = 0; i < ARRAY_NUM_ENTRIES-1; i++)
                                    {
                                        if (timeDiffArray[i] != 0)
                                        {
                                            printk("CNI: Total pkts with delay %dmsec-%dmses: %d\n", (i)*ARRAY_NUM_ENTRIES, (i+1)*ARRAY_NUM_ENTRIES, timeDiffArray[i]);
                                        }
                                    }
                                    if (timeDiffArray[ARRAY_NUM_ENTRIES-1] != 0)
                                    {
                                        printk("CNI: Total pkts with delay bigger than %dmses: %d\n", (i)*ARRAY_NUM_ENTRIES, timeDiffArray[i]);
                                    }
                                }
                                lastTimestamp = currTimestamp;
                            }
                        }
#endif
                        /* ARRIS ADD START */
                        /* Downstream DOCSIS bridge bypass */
                        if (priv->Ipv4FilterAddress)
                        {
                            if (CheckIpv4FilterRx(rxskb, priv->Ipv4FilterAddress, priv->Ipv4FilterPort))
                            {
                                // Yes we do - increment filtered rx packet count...
                                priv->filtered_stats.rx_packets++;
                                priv->filtered_stats.rx_bytes += bd->hw.buffLen;
                                if (priv->DbridgeBypass)
                                {
                                    // Check if we should bypass the docsis bridge and send straight to IP layer
                                    struct ethhdr* ptr_ethhdr = (struct ethhdr *)skb_mac_header(rxskb);
                                    bridgebypass = TRUE;
                                    
                                    // mark packet as PP bypass - dbridge would do this before passing up the stack
                                    rxskb->pp_packet_info.flags = TI_HIL_PACKET_FLAG_PP_SESSION_BYPASS;

                                    rxskb->ti_docsis_input_dev = dev;                 // TI input device is cni0 (for ICMP)
                                    rxskb->dev                 = wanDev;              // device passing the packet to the protocol stack is wan0
                                    rxskb->protocol            = ptr_ethhdr->h_proto; // we're about to strip the ethernet header - save the protocol
                                                
                                    // discard the ethernet header
                                    skb_pull(rxskb,ETH_HLEN);

#if defined(CONFIG_MACH_PUMA5) && defined(CONFIG_INET_LRO) //ARRIS ADD for PUMA5 only
                                    if(dev->features & NETIF_F_LRO )
                                    {
                                        // LRO enabled, aggregate it 
                                        lro_receive_skb(&priv->lro_mgr_low, rxskb, (void *)priv);
                                    }
                                    else
                                    {
                                        /* LRO not enabled, push the packet to the IP stack. */
                                        netif_receive_skb(rxskb);
                                    }
#else
                                    netif_receive_skb(rxskb);
#endif
                                }
                            }
                        }
                        /* ARRIS ADD END */
#ifdef CN_RX_DBRIDGE
                        if (!bridgebypass) /* ARRIS ADD for DBridge bypass */
						    DBridge_receive (rxskb);
#endif
#ifdef CNI_RX_NETIF
                        netif_receive_skb(rxskb);
#endif
                        DPRINTK( ": %d packets received\n",(int)(priv->stats.rx_packets));
                    }

                    /* Prepare to return to free Proxy queue */
                    cnid_rx_bd_link_skb(dev, bd, newskb);
                }
            }

#ifdef MTA_LIMIT_HIGH_PRIORITY_RATE
            if ((isMtaRateHigh) && (bd->hw.pktInfo & 0x10000)) 
            {
                /* only if we are in a high rate situation, mta0 traffic can arrive here, otherwise no use checking. */
                /* check pktinfo in descriptor: if bit 16 is lit it's mta0 packet (PP marks mta0 packets this way). */
                /* If we are here it's a diverted mta0 packet in low priority queue. */

                mtaPacketCounter++; 
                if (mtaPacketCountCheck <= mtaPacketCounter)
                {
                    /* We want to keep track of the diverted mta0 traffic to see if the rate is
                       low so we will be able to Notify PP to re-enable the high priority queue. */

                    mtaTimeDiffInJiffies = jiffies - lastMtaCounterResetTimeInJiffies;

                    /* Please note there's no need for timer wrap around consideration as we are using
                       unsigned and are only interested in the differance. The HIGHLY unlikely case of 
                       more than a complete wrap around between 2 resets is left unchecked (but is 
                       virtually impossible). */
#if 0 
                    /* can cause numerous printks so naturally should be turned off (even when debug unless crucial) */
                    DPRINTK_MTA("Rx low - Packet Count limit Reached, %d packets in %lu[Jiffies]",
                                mtaPacketCountCheck, mtaTimeDiffInJiffies);
#endif
                    if (mtaTimeDiffInJiffies > mtaPacketCheckMinTimeInJiffies) 
                    {
                        DPRINTK_MTA("Diverted traffic rate is low again! Notifying PP to switch mta0 traffic back to high priority.");
                        DPRINTK_MTA("%d packets in diff = %lu[Jiffies] > diffMin = %lu[Jiffies]",
                                    mtaPacketCountCheck, mtaTimeDiffInJiffies, mtaPacketCheckMinTimeInJiffies);
                        cni_pp_set_TurnOffHighPriorityQueue(0); /* Turn it back on */
                        isMtaRateHigh = False;
                    }
                    mtaPacketCounter = 0;
                    lastMtaCounterResetTimeInJiffies = jiffies; /* update with current time stamp */
                }
            }
#endif
            packets_processed++;
            priv->rxAcc_chan_list[CNID_LOW_PRIORITY]++;
            /* Return to free queue */
            PAL_cppi4QueuePush(priv->rxProxy_free_queue_hnd[CNID_LOW_PRIORITY], (Uint32 *) PAL_CPPI4_VIRT_2_PHYS(bd), CNID_QM_DESC_SIZE_CODE, 0);
#ifdef CNI_USE_NAPI
            /* thats it, we did enough. Jump out now! */
            if (budget == packets_processed)
            {
                return packets_processed;
            }
#endif
        }

        /* Update the list entry for next time */
        priv->rxAcc_chan_list[CNID_LOW_PRIORITY] = PAL_cppi4AccChGetNextList(priv->rxAcc_chan_hnd[CNID_LOW_PRIORITY]);
        avalanche_intd_set_interrupt_count (CNID_INTD_HOST_NUM, priv->rxAcc_chan[CNID_LOW_PRIORITY], 1);
    }

#ifdef CONFIG_MACH_PUMA5
// LRO PATCH for WorkQ and TASKLET. ARRIS ADD Start for PROD00215939
#if defined(CNI_USE_TASKLET) || defined(CNI_USE_WORKQ)
#ifdef CONFIG_INET_LRO  
    if( dev->features & NETIF_F_LRO )
    {
        if (priv->lro_complete)
        {
            dev->features &= ~NETIF_F_LRO;
        }
        lro_flush_all(&priv->lro_mgr_low);
    }
#endif 
#endif
// ARRIS ADD End for PROD00215939

#if defined(CNI_USE_WORKQ)
    avalanche_intd_write_eoi(PAL_CPPI41_CNI_LOW_ACC_CH_INTV_NUM);
    enable_irq(MAP_INTD_TO_INTC(PAL_CPPI41_CNI_LOW_ACC_CH_INTV_NUM));
#endif
#endif

#ifdef CONFIG_MACH_PUMA6
#if defined(CNI_USE_TASKLET) || defined(CNI_USE_WORKQ)
    /* First clear the IRQ in order not to get a false interrupt since INTD is level */
    ack_irq(MAP_INTD_TO_INTC(PAL_CPPI41_CNI_LOW_ACC_CH_INTV_NUM));

    /* Send INTD EOI */
    avalanche_intd_write_eoi(PAL_CPPI41_CNI_LOW_ACC_CH_INTV_NUM);

    /* It could be that between INTD count decrement and EOI the accumulator will issue another interrupt.
       The logic of INTD is such that level will remain active high even after EOI is set, so INTC will
       lose the interrupt after ack_irq is done (it now expects INTD polarity change).
       Therefore we must check INTD count and if it is not 0 - reschedule the tasklet */
    if (avalanche_intd_get_interrupt_count(CNID_INTD_HOST_NUM, priv->rxAcc_chan[CNID_LOW_PRIORITY]))
    {
#if defined(CNI_USE_TASKLET)
        tasklet_schedule(&priv->rx_tasklet);
#elif defined(CNI_USE_WORKQ)
        /* Put dev in work */
        priv->cni_rx_work.dev = dev;
        /* Que work */
        queue_work(priv->cni_rx_wq, &(priv->cni_rx_work.work));
#endif
        return;
    }

    /* Now enable the IRQ */
    enable_irq(MAP_INTD_TO_INTC(PAL_CPPI41_CNI_LOW_ACC_CH_INTV_NUM));

#endif
#endif

#ifdef CNI_USE_NAPI
    return packets_processed;
#endif
}

/**************************************************************************/
/*! \fn         cnid_do_rx_complete_high
 **************************************************************************
 *
 *  \brief      Rx Complete handler
 *
 *  \param[in]  Net Device
 *  \param[in]  Processed packets budget
 *  \return     Number of processed packets
 **************************************************************************/
#ifdef CNI_USE_NAPI_HIGH
static int cnid_do_rx_complete_high(struct net_device* dev, int budget)
#elif defined(CNI_USE_TASKLET_HIGH)
static void cnid_do_rx_complete_high(unsigned long data)
#elif defined(CNI_USE_WORKQ_HIGH)
static void cnid_do_rx_complete_high(struct work_struct *work)
#endif
{
#if defined(CNI_USE_TASKLET_HIGH)
    struct net_device*      dev = (struct net_device*) data;
#elif defined(CNI_USE_WORKQ_HIGH)
    cni_rx_work_t *curr_work = (cni_rx_work_t *)work;
    struct net_device* dev = curr_work->dev;
#endif
    cnid_private_t* priv = netdev_priv(dev);
    Cppi4HostDescLinux* bd;
    int packets_processed = 0;

#ifdef MTA_LIMIT_HIGH_PRIORITY_RATE
    /* We are in Rx high this will be used to determine if rate is high.
       If so we will notify PP to disable high priority queue for mta0 traffic.  */
    unsigned long mtaTimeDiffInJiffies = 0;
#endif

#if defined(CNI_USE_WORKQ_HIGH)
    static int first = 1;
    struct task_struct *ctask = current;

    if (first)
    {
        /* Set priority */
        set_user_nice(current, 1);
        first = 0;

        printk("**** ---- >>>> cnid_do_rx_complete_high - name \"%s\", pid %d\n", ctask->comm, ctask->pid);
    }
#endif

    /* process high priority packets first */
    while(avalanche_intd_get_interrupt_count(CNID_INTD_HOST_NUM, priv->rxAcc_chan[CNID_HIGH_PRIORITY]) && (packets_processed < CNID_RX_SERVICE_MAX))
    {
        while((bd = (Cppi4HostDescLinux*)((unsigned long)*priv->rxAcc_chan_list[CNID_HIGH_PRIORITY] & QMGR_QUEUE_N_REG_D_DESC_ADDR_MASK)))
        {
            struct sk_buff *newskb;

            bd = PAL_CPPI4_PHYS_2_VIRT(bd);

            /* get a new skb for this bd */

            if(!test_bit(0, &priv->state))
            {   /* if not cleaning up .. */

                if((newskb = dev_alloc_skb(PAL_CPPI41_SR_CNI_INFRA_FD_HOST_BUFFER_SIZE)))
                { /* .. and able to get a new buffer */

                    struct sk_buff* rxskb;

                    PAL_CPPI4_CACHE_INVALIDATE(bd, CPPI4_BD_LENGTH_FOR_CACHE);
                    rxskb = bd->skb;

                    PAL_CPPI4_CACHE_INVALIDATE(rxskb->data, bd->hw.buffLen - 4);
                    skb_put(rxskb, bd->hw.buffLen - 4); /* remove CRC from length */
                    dev->last_rx = jiffies;
                    priv->stats.rx_packets++;
                    priv->stats.rx_bytes += bd->hw.buffLen;

                    /* Keep SYNC Q PTID info in skb for egress */
                    if(bd->hw.netInfoWord1)
                    {
                        memcpy(rxskb->pp_packet_info.ti_epi_header, &(bd->hw.netInfoWord0), 8);
                        rxskb->pp_packet_info.flags = TI_HIL_PACKET_FLAG_PP_SESSION_INGRESS_RECORDED;
                    }

                    /* ... then send the packet up */
                    {
                            bool DoBridge = TRUE;    // ARRIS ADD : DBRIDGE BYPASS
                        /* This is the DOCSIS Interface; pass the packet to the DOCSIS Bridge; initialize the
                        * MAC RAW Pointer before doing so. */
                        skb_reset_mac_header(rxskb);
                        rxskb->dev = dev;
                        rxskb->ti_meta_info = bd->psi[0];
                        rxskb->ti_meta_info2 = bd->psi[2];

                            /* ARRIS ADD START - FROM SERCOMM */
                            #ifndef CONFIG_VENDOR_COMCAST
                            #ifdef INCLUDE_SERCOMM_GW
                            if(sc_modify_dest_mac_hook)
                            {
                                sc_modify_dest_mac_hook(rxskb);
                            }
                            #endif /* INCLUDE_SERCOMM_GW */
                            #endif /* Comcast */
                            ARRISMOD_CALL( arris_rip_check_dst_mac_hook, rxskb );
                            /* ARRIS ADD END */

                            // ARRIS ADD : SOCKET FILTER and DOCSIS BRIDGE BYPASS
                            // If the filter is on, keep these stats seperate
                            if (priv->Ipv4FilterAddress)
                            {
                                // find the Source IP address/port to see if we want to seperate this stream
                                if (CheckIpv4FilterRx(rxskb, priv->Ipv4FilterAddress, priv->Ipv4FilterPort))
                                {
                                    // Yes we do - increment filtered rx packet count...
                                    priv->filtered_stats.rx_packets++;
                                    priv->filtered_stats.rx_bytes += bd->hw.buffLen;

                                    // Check if we should bypass the docsis bridge and send straight to IP layer
                                    if (priv->DbridgeBypass)
                                    {
                                        struct ethhdr* ptr_ethhdr = (struct ethhdr *)skb_mac_header(rxskb);
                                        DoBridge = FALSE;
    
                                        // mark packet as PP bypass - dbridge would do this before passing up the stack
                                        rxskb->pp_packet_info.flags = TI_HIL_PACKET_FLAG_PP_SESSION_BYPASS;
    
                                        rxskb->ti_docsis_input_dev = dev;                 // TI input device is cni0 (for ICMP)
                                        rxskb->dev                 = wanDev;              // device passing the packet to the protocol stack is wan0
                                        rxskb->protocol            = ptr_ethhdr->h_proto; // we're about to strip the ethernet header - save the protocol
                                                    
                                        // discard the ethernet header
                                        skb_pull(rxskb,ETH_HLEN);
    
#ifdef CONFIG_INET_LRO
                                        if(dev->features & NETIF_F_LRO )
                                        {
                                            // LRO enabled, aggregate it 
                                            lro_receive_skb(&priv->lro_mgr_high, rxskb, (void *)priv);      // ARRIS MOD : use 2 aggregators
                                        }
                                        else
                                        {
                                            /* LRO not enabled, push the packet to the IP stack. */
                                            netif_receive_skb(rxskb);
                                        }
#else
                                        netif_receive_skb(rxskb);
#endif
                                    }
                                }
                            }
                            
                            if (DoBridge)
                            {
#ifdef CN_RX_DBRIDGE
                            DBridge_receive (rxskb);
                            }
                            // END ARRIS

#endif
#ifdef CNI_RX_NETIF
                        netif_receive_skb(rxskb);
#endif
                        DPRINTK( ": %d packets received\n",(int)(priv->stats.rx_packets));
                    }

                    /* Prepare to return to free Proxy queue */
                    cnid_rx_bd_link_skb(dev, bd, newskb);
                }
            }

#ifdef MTA_LIMIT_HIGH_PRIORITY_RATE
            mtaPacketCounter++;
            if ((!isMtaRateHigh) && (mtaPacketCountCheck <= mtaPacketCounter))
            {
                mtaTimeDiffInJiffies = jiffies - lastMtaCounterResetTimeInJiffies;

                /* Please note there's no need for timer wrap around consideration as we are using
                   unsigned and are only interested in the differance. The HIGHLY unlikely case of 
                   more than a complete wrap around between 2 resets is left unchecked (but is 
                   virtually impossible). */
#if 0
                /* can cause numerous printks so naturally should be turned off (even when debug unless crucial) */
                DPRINTK_MTA("Rx high - Packet Count limit Reached, %d packets in %lu[Jiffies]",
                            mtaPacketCountCheck, mtaTimeDiffInJiffies);
#endif
                if (mtaTimeDiffInJiffies < mtaPacketCheckMinTimeInJiffies) 
                {
                    DPRINTK_MTA("Rate is too high! Notifying PP to divert mta0 traffic to low priority.");
                    DPRINTK_MTA("%d packets in diff = %lu[Jiffies] < diffMin = %lu[Jiffies]",
                                mtaPacketCountCheck, mtaTimeDiffInJiffies, mtaPacketCheckMinTimeInJiffies);
                    cni_pp_set_TurnOffHighPriorityQueue(1); /* Turn it off */
                    isMtaRateHigh = True;
                }
                mtaPacketCounter = 0;
                lastMtaCounterResetTimeInJiffies = jiffies; /* update with current time stamp */
            }
#endif
            packets_processed++;
            priv->rxAcc_chan_list[CNID_HIGH_PRIORITY]++;
            /* Return to free queue */
            PAL_cppi4QueuePush(priv->rxProxy_free_queue_hnd[CNID_HIGH_PRIORITY], (Uint32 *) PAL_CPPI4_VIRT_2_PHYS(bd), CNID_QM_DESC_SIZE_CODE, 0);
#ifdef CNI_USE_NAPI_HIGH
            /* thats it, we did enough. Jump out now! */
            if (budget == packets_processed)
            {
                return packets_processed;
            }
#endif
        }

        /* Update the list entry for next time */
        priv->rxAcc_chan_list[CNID_HIGH_PRIORITY] = PAL_cppi4AccChGetNextList(priv->rxAcc_chan_hnd[CNID_HIGH_PRIORITY]);
        avalanche_intd_set_interrupt_count (CNID_INTD_HOST_NUM, priv->rxAcc_chan[CNID_HIGH_PRIORITY], 1);
    }

#ifdef CONFIG_MACH_PUMA5
// LRO PATCH for WorkQ and TASKLET. ARRIS ADD Start for PROD00215939
#if defined(CNI_USE_TASKLET_HIGH) || defined(CNI_USE_WORKQ_HIGH)
#ifdef CONFIG_INET_LRO  
    if( dev->features & NETIF_F_LRO )
    {
        if (priv->lro_complete)
        {
            dev->features &= ~NETIF_F_LRO;
        }
        lro_flush_all(&priv->lro_mgr_high);
    }
#endif 
#endif
#endif
// ARRIS ADD End for PROD00215939
        
#ifdef CONFIG_MACH_PUMA5
#if defined(CNI_USE_WORKQ_HIGH)
    avalanche_intd_write_eoi(PAL_CPPI41_CNI_HIGH_ACC_CH_INTV_NUM);
    enable_irq(MAP_INTD_TO_INTC(PAL_CPPI41_CNI_HIGH_ACC_CH_INTV_NUM));
#endif
#endif

#ifdef CONFIG_MACH_PUMA6
#if defined(CNI_USE_TASKLET_HIGH) || defined(CNI_USE_WORKQ_HIGH)
    /* First clear the IRQ in order not to get a false interrupt since INTD is level */
    ack_irq(MAP_INTD_TO_INTC(PAL_CPPI41_CNI_HIGH_ACC_CH_INTV_NUM));

    /* Send INTD EOI */
    avalanche_intd_write_eoi(PAL_CPPI41_CNI_HIGH_ACC_CH_INTV_NUM);

    /* It could be that between INTD count decrement and EOI the accumulator will issue another interrupt.
       The logic of INTD is such that level will remain active high even after EOI is set, so INTC will
       lose the interrupt after ack_irq is done (it now expects INTD polarity change).
       Therefore we must check INTD count and if it is not 0 - reschedule the tasklet */
    if (avalanche_intd_get_interrupt_count(CNID_INTD_HOST_NUM, priv->rxAcc_chan[CNID_HIGH_PRIORITY]))
    {
#if defined(CNI_USE_TASKLET_HIGH)
        tasklet_schedule(&priv->rx_tasklet_high);
#elif defined(CNI_USE_WORKQ_HIGH)
        /* Put dev in work */
        priv->cni_rx_work_high.dev = dev;
        /* Que work */
        queue_work(priv->cni_rx_wq_high, &(priv->cni_rx_work_high.work));
#endif
        return;
    }

    /* Now enable the IRQ */
    enable_irq(MAP_INTD_TO_INTC(PAL_CPPI41_CNI_HIGH_ACC_CH_INTV_NUM));

#endif
#endif

#ifdef CNI_USE_NAPI_HIGH
    return packets_processed;
#endif
}

#ifdef CNI_RX_NETIF
rx_handler_result_t cnid_rx_handler(struct sk_buff **pskb)
{
    if (pskb == NULL)
    {
        printk(KERN_ERR "ERROR in cnid_rx_handler: pskb == NULL\n");
        return RX_HANDLER_CONSUMED;
    }

    DBridge_receive (*pskb);

    return RX_HANDLER_CONSUMED;
}
#endif

/**************************************************************************/
/*! \fn         cnid_poll_low
 **************************************************************************
 *
 *  \brief      Polling function
 *
 *  \param[in]  Net Device
 *  \param[in]  Processed packets budget
 *  \return     Number of processed packets
 **************************************************************************/
#ifdef CNI_USE_NAPI
static int cnid_poll_low(struct napi_struct *napi , int budget)
{
    int work_done;
    cnid_private_t *priv = container_of(napi, cnid_private_t, napi);
    struct net_device *dev = napi->dev;     /* ARRIS ADD : Intel will need to rework, for LRO as well */
#ifdef CONFIG_MACH_PUMA6
    unsigned long flags;
#endif

    DPRINTK( " Enter \n");
    work_done = cnid_do_rx_complete_low(priv->netdev, budget);

    /* order is important here. If we do EOI before calling netif_rx_complete, an interrupt
     * can occur just before we take ourselves out of the poll list; we will not
     * schedule NAPI thread on that interrupt, no further Rx interrupts and
     * Rx will stall forever. Scary...
     * */
    if (work_done < budget)
    {
        //netif_rx_complete(napi);
        napi_complete(napi);

#ifdef CONFIG_MACH_PUMA6
        /* Accumulator looks at INTD counter in order to know if it can issue another interrupt.
           Since we decrement the counter at cnid_do_rx_complete_low it is possible that accumulator issued another interrupt.
           Due to the fact that interrupt is level and we do not want to get a false interrupt, we clear the INTC at the end of cnid_do_rx_complete_low.
           Next time INTC will wait for INTD to become active.
           But, since INTD is level there is a possibility that INTD will remain active.
           This can happen if accumulator issues an interrupt before the host sent EOI (this is done in next line of code).
           So, in this case we have INTD status not changed - still active, while INTC now waits for it to become active.
           This can lead to not getting the interrupt forever. This is why we must check if counter>0 and if so re-schedule NAPI.
           We lock the interrupts b4 doing EOI and up until NAPI schedule in order not to get double interrupt in the case that
           an interrupt is really issued between EOI and checking INTD count - we are going to reschedule NAPI anyway... */

        spin_lock_irqsave(&priv->devlock, flags);
        ack_irq(MAP_INTD_TO_INTC(PAL_CPPI41_CNI_LOW_ACC_CH_INTV_NUM));
        enable_irq(MAP_INTD_TO_INTC(PAL_CPPI41_CNI_LOW_ACC_CH_INTV_NUM));
#endif
        avalanche_intd_write_eoi(PAL_CPPI41_CNI_LOW_ACC_CH_INTV_NUM);

#ifdef CONFIG_MACH_PUMA6
        if (avalanche_intd_get_interrupt_count(CNID_INTD_HOST_NUM, priv->rxAcc_chan[CNID_LOW_PRIORITY]))
        {
            if (likely(napi_schedule_prep(napi)))
            {
                disable_irq_nosync(MAP_INTD_TO_INTC(PAL_CPPI41_CNI_LOW_ACC_CH_INTV_NUM));
                __napi_schedule(napi);
            }
        }
        spin_unlock_irqrestore(&priv->devlock, flags);
#endif
        //return 0;
    }

#ifdef CONFIG_INET_LRO
    if( dev->features & NETIF_F_LRO ) {

        // ARRIS ADD : LRO PATCH
        if (priv->lro_complete)
        {
            dev->features &= ~NETIF_F_LRO;
        }
        // END ARRIS

        lro_flush_all(&priv->lro_mgr_low);      // ARRIS MOD : Use 2 aggregators
    }
#endif
    return work_done;
    //return 1;
}
#endif


/**************************************************************************/
/*! \fn         cnid_poll_high
 **************************************************************************
 *
 *  \brief      Polling function
 *
 *  \param[in]  Net Device
 *  \param[in]  Processed packets budget
 *  \return     Number of processed packets
 **************************************************************************/
#ifdef CNI_USE_NAPI_HIGH
static int cnid_poll_high(struct napi_struct *napi , int budget)
{
    int work_done;
    cnid_private_t *priv = container_of(napi, cnid_private_t, napi_high);
#ifdef CONFIG_MACH_PUMA6
    unsigned long flags;
#endif

    DPRINTK( " Enter \n");
    work_done = cnid_do_rx_complete_high(priv->netdev, budget);

    /* order is important here. If we do EOI before calling netif_rx_complete, an interrupt
     * can occur just before we take ourselves out of the poll list; we will not
     * schedule NAPI thread on that interrupt, no further Rx interrupts and
     * Rx will stall forever. Scary...
     * */
    if (work_done < budget)
    {
        //netif_rx_complete(napi);
        napi_complete(napi);

#ifdef CONFIG_MACH_PUMA6
        /* Accumulator looks at INTD counter in order to know if it can issue another interrupt.
           Since we decrement the counter at cnid_do_rx_complete_high it is possible that accumulator issued another interrupt.
           Due to the fact that interrupt is level and we do not want to get a false interrupt, we clear the INTC at the end of cnid_do_rx_complete_high.
           Next time INTC will wait for INTD to become active.
           But, since INTD is level there is a possibility that INTD will remain active.
           This can happen if accumulator issues an interrupt before the host sent EOI (this is done in next line of code).
           So, in this case we have INTD status not changed - still active, while INTC now waits for it to become active.
           This can lead to not getting the interrupt forever. This is why we must check if counter>0 and if so re-schedule NAPI.
           We lock the interrupts b4 doing EOI and up until NAPI schedule in order not to get double interrupt in the case that
           an interrupt is really issued between EOI and checking INTD count - we are going to reschedule NAPI anyway... */

        spin_lock_irqsave(&priv->devlock, flags);
        ack_irq(MAP_INTD_TO_INTC(PAL_CPPI41_CNI_HIGH_ACC_CH_INTV_NUM));
        enable_irq(MAP_INTD_TO_INTC(PAL_CPPI41_CNI_HIGH_ACC_CH_INTV_NUM));
#endif
        avalanche_intd_write_eoi(PAL_CPPI41_CNI_HIGH_ACC_CH_INTV_NUM);

#ifdef CONFIG_MACH_PUMA6
        if (avalanche_intd_get_interrupt_count(CNID_INTD_HOST_NUM, priv->rxAcc_chan[CNID_HIGH_PRIORITY]))
        {
            if (likely(napi_schedule_prep(napi)))
            {
                disable_irq_nosync(MAP_INTD_TO_INTC(PAL_CPPI41_CNI_HIGH_ACC_CH_INTV_NUM));
                __napi_schedule(napi);
            }
        }
        spin_unlock_irqrestore(&priv->devlock, flags);
#endif
        //return 0;
    }

// ARRIS ADD : LRO PATCH
#ifdef CONFIG_INET_LRO  
#ifdef CONFIG_MACH_PUMA6    // CJL-PUMA5 TODO  
    if( dev->features & NETIF_F_LRO ) 
    {
        lro_flush_all(&priv->lro_mgr_high);
    }
#endif // CJL    

#ifdef CONFIG_MACH_PUMA5 /*ARRIS ADD Start for PD215882*/ 
    struct net_device *dev = napi->dev;
    if( dev->features & NETIF_F_LRO )
    {
        if (priv->lro_complete)
        {
            dev->features &= ~NETIF_F_LRO;
        }
        lro_flush_all(&priv->lro_mgr_high);
    }
#endif /*ARRIS ADD End for PD215882*/
#endif 
// END ARRIS

    return work_done;
    //return 1;
}
#endif

/**************************************************************************/
/*! \fn         cnid_rx_interrupt
 **************************************************************************
 *
 *  \brief      Receive ISR
 *
 *  \param[in]  IRQ
 *  \param[in]  Device
 *  \param[in]  regs
 *  \return     OK or error
 **************************************************************************/
static irqreturn_t cnid_rx_interrupt (int irq, void *dev_instance)
{
    struct net_device *dev = (struct net_device *) dev_instance;
    cnid_private_t *priv  = netdev_priv(dev);

#ifdef CNI_USE_NAPI
    DPRINTK( " Enter \n");
    /* if poll routine is not running, start it now. */
    if (likely(napi_schedule_prep(&priv->napi))) {
#ifdef CONFIG_MACH_PUMA6
        /* Since the INTD interrupts are level, need to disable the IRQ in order to run the tasklet */
        disable_irq_nosync(MAP_INTD_TO_INTC(PAL_CPPI41_CNI_LOW_ACC_CH_INTV_NUM));
#endif
        __napi_schedule(&priv->napi);
    }

#elif defined(CNI_USE_TASKLET)
#ifdef CONFIG_MACH_PUMA6
    /* Since the INTD interrupts are level, need to disable the IRQ in order to run the tasklet */
    disable_irq_nosync(MAP_INTD_TO_INTC(PAL_CPPI41_CNI_LOW_ACC_CH_INTV_NUM));
#endif
    tasklet_schedule(&priv->rx_tasklet);

#elif defined(CNI_USE_WORKQ)
    /* Since the INTD interrupts are level, need to disable the IRQ in order to run the tasklet */
    disable_irq_nosync(MAP_INTD_TO_INTC(PAL_CPPI41_CNI_LOW_ACC_CH_INTV_NUM));

    /* Put dev in work */
    priv->cni_rx_work.dev = dev;
    /* Que work */
    queue_work(priv->cni_rx_wq, &(priv->cni_rx_work.work));
#endif
    return IRQ_RETVAL(1);
}

/**************************************************************************/
/*! \fn         cnid_rx_interrupt_high
 **************************************************************************
 *
 *  \brief      Receive ISR
 *
 *  \param[in]  IRQ
 *  \param[in]  Device
 *  \param[in]  regs
 *  \return     OK or error
 **************************************************************************/
static irqreturn_t cnid_rx_interrupt_high (int irq, void *dev_instance)
{
    struct net_device *dev = (struct net_device *) dev_instance;
    cnid_private_t *priv  = netdev_priv(dev);

#ifdef CNI_USE_NAPI_HIGH
    DPRINTK( " Enter \n");
    /* if poll routine is not running, start it now. */
    if (likely(napi_schedule_prep(&priv->napi_high))) {
#ifdef CONFIG_MACH_PUMA6
        /* Since the INTD interrupts are level, need to disable the IRQ in order to run the tasklet */
        disable_irq_nosync(MAP_INTD_TO_INTC(PAL_CPPI41_CNI_HIGH_ACC_CH_INTV_NUM));
#endif
        __napi_schedule(&priv->napi_high);
    }

#elif defined(CNI_USE_TASKLET_HIGH)
#ifdef CONFIG_MACH_PUMA6
    /* Since the INTD interrupts are level, need to disable the IRQ in order to run the tasklet */
    disable_irq_nosync(MAP_INTD_TO_INTC(PAL_CPPI41_CNI_HIGH_ACC_CH_INTV_NUM));
#endif
    tasklet_schedule(&priv->rx_tasklet_high);

#elif defined(CNI_USE_WORKQ_HIGH)
    /* Since the INTD interrupts are level, need to disable the IRQ in order to run the tasklet */
    disable_irq_nosync(MAP_INTD_TO_INTC(PAL_CPPI41_CNI_HIGH_ACC_CH_INTV_NUM));

    /* Put dev in work */
    priv->cni_rx_work_high.dev = dev;
    /* Que work */
    queue_work(priv->cni_rx_wq_high, &(priv->cni_rx_work_high.work));
#endif
    return IRQ_RETVAL(1);
}

/** Driver init/open/close functions go here **/

/************************************************************************/
/*                                                                      */
/*  Interrupt Accumulator Channels INIT routine                         */
/*                                                                      */
/************************************************************************/
static int cnid_init_acc_chan(PAL_Handle pal_hnd, int chan_num, Cppi4Queue queue, PAL_Cppi4AccChHnd* acc_hnd)
{
    Cppi4AccumulatorCfg cfg;
    *acc_hnd = NULL;

    cfg.accChanNum             = chan_num;
    cfg.list.maxPageEntry      = CNID_ACC_PAGE_NUM_ENTRY;        /* This is entries per page (and we have 2 pages) */
    cfg.list.listEntrySize     = CNID_ACC_ENTRY_TYPE;            /* Only interested in register 'D' which has the desc pointer */
    cfg.list.listCountMode     = 0;                              /* Zero indicates null terminated list. */
    cfg.list.pacingMode        = 1;                              /* Wait for time since last interrupt */
    cfg.pacingTickCnt          = 40;                             /* Wait for 1000uS == 1ms */
    cfg.list.maxPageCnt        = CNID_ACC_NUM_PAGE;              /* Use two pages */
    cfg.list.stallAvoidance    = 1;                              /* Use the stall avoidance feature */
    cfg.queue                  = queue;
    cfg.mode                   = 0;

    /* kmalloc returns cache line aligned memory unless you are debugging the slab allocator (2.6.18) */
    if(!(cfg.list.listBase = kzalloc(CNID_ACC_LIST_BYTE_SZ, GFP_KERNEL)))
    {
        DPRINTK(" Unable to allocate list page\n");
        return -1;
    }

    PAL_CPPI4_CACHE_WRITEBACK((unsigned long)cfg.list.listBase, CNID_ACC_LIST_BYTE_SZ);

    if(!(*acc_hnd = PAL_cppi4AccChOpen(pal_hnd, &cfg)))
    {
        DPRINTK( " Unable to open accumulator channel #%d\n", chan_num);
        kfree(cfg.list.listBase);
        return -1;
    }

    return 0;
}


/**************************************************************************/
/*! \fn         cnid_open_tx
 **************************************************************************
 *
 *  \brief      Open Tx routine
 *
 *  \param[in]  Net Device
 *  \return     OK or error
 **************************************************************************/
static int cnid_open_tx(struct net_device* dev)
{
    cnid_private_t* priv = netdev_priv(dev);
#ifdef CONFIG_MACH_PUMA5
    Cppi4Queue tx_cmpl  = {PAL_CPPI41_QUEUE_MGR_PARTITION_SR, PAL_CPPI41_SR_HOST_TX_COMPLETE_LOW_Q_NUM};
#endif
    Cppi4Queue queue;   /* used generically */
    int i = 0, j;

    /************************************************/
    /*                                              */
    /* Initialize US DOCSIS Tx queues               */
    /*                                              */
    /************************************************/
    queue.qMgr = PAL_CPPI41_QUEUE_MGR_PARTITION_SR;
    for(i = 0; i < PAL_CPPI41_SR_DOCSIS_TX_DATA_Q_COUNT; i++)
    {
        queue.qNum = PAL_CPPI41_SR_DOCSIS_TX_DATA_Q_NUM(i);

        if(!(priv->tx_queue[i] = PAL_cppi4QueueOpen(priv->pal_hnd, queue)))
        {
            DPRINTK( " unable to open Tx Queue #%d!\n", i);
            goto err;
        }
    }
    /************************************************/

    for (i = 0; PAL_CPPI41_SR_HOST_TO_PP_Q_COUNT > i; i++)
    {
        /************************************************/
        /*                                              */
        /* Initialize System Generic Priority Tx queues */
        /*                                              */
        /************************************************/
        queue.qMgr = PAL_CPPI41_QUEUE_MGR_PARTITION_SR;
        queue.qNum = PAL_CPPI41_SR_HOST_TO_PP_INFRA_INPUT_Q_NUM(i);

        if(!(priv->tx_qos_queue[i] = PAL_cppi4QueueOpen(priv->pal_hnd, queue)))
        {
            DPRINTK(" unable to open Tx QOS Queue #%d!\n", i);
            goto err;
        }
        /************************************************/

        /************************************************/
        /*                                              */
        /* Initialize Free Host Descriptors Tx queues   */
        /*                                              */
        /************************************************/
        queue.qMgr = PAL_CPPI41_QUEUE_MGR_PARTITION_SR;
        queue.qNum = PAL_CPPI41_SR_HOST_TO_PP_FD_HOST_Q_NUM(i);

        if(!(priv->tx_qos_free_queue[i] = PAL_cppi4QueueOpen(priv->pal_hnd, queue)))
        {
            DPRINTK(" unable to open Tx Queue #%d!\n", queue.qNum);
            goto err;
        }
        /************************************************/
    }

#ifdef CONFIG_MACH_PUMA5
    /* reset Tx completion queue */
    PAL_cppi4QueueClose(priv->pal_hnd, PAL_cppi4QueueOpen(priv->pal_hnd, tx_cmpl));

    tasklet_init(&priv->tx_tasklet, cnid_do_tx_complete, (unsigned long) dev);

    /* Init the Tx complete accumulator channel */
    if(cnid_init_acc_chan(priv->pal_hnd, PAL_CPPI41_TX_COMPLETE_LOW_ACC_CH_NUM, tx_cmpl, &priv->tx_acc_hnd))
    {
        DPRINTK(KERN_ERR "%s: unable to open accumulator channel!\n", __FUNCTION__);
        goto err;
    }

    priv->tx_list_base = priv->tx_list = PAL_cppi4AccChGetNextList(priv->tx_acc_hnd);

    /* request the Tx IRQs */
    if(request_irq (MAP_INTD_TO_INTC(PAL_CPPI41_TX_COMPLETE_ACC_INTV_NUM), cnid_tx_interrupt, IRQF_DISABLED, "TX Complete", dev))
    {
        DPRINTK(KERN_ERR "%s: unable to get IRQ #%d!\n", __FUNCTION__, MAP_INTD_TO_INTC(PAL_CPPI41_TX_COMPLETE_ACC_INTV_NUM));
        goto err;
    }
#endif

    return 0;

err:
    for(j = 0; j < i ; j++)
    {
        queue.qNum = PAL_CPPI41_SR_DOCSIS_TX_DATA_Q_NUM(j);
        PAL_cppi4QueueClose(priv->pal_hnd, priv->tx_queue[j]);
    }

    return -1;
}


/**************************************************************************/
/*! \fn         cnid_open_rx
 **************************************************************************
 *
 *  \brief      Open Rx routine
 *
 *  \param[in]  Net Device
 *  \return     Ok or error
 **************************************************************************/
static int cnid_open_rx(struct net_device* dev)
{
    cnid_private_t* priv = netdev_priv(dev);
    int iProxyChan;
#if defined(CONFIG_MACH_PUMA5) || defined(CONFIG_MACH_PUMA6)
    /* Make sure PP high priority queue is enabled at the start */
    cni_pp_set_TurnOffHighPriorityQueue(0);
#endif

    for (iProxyChan=0; iProxyChan<PAL_CPPI41_SR_CNI_INFRA_DMA_CH_COUNT; iProxyChan++)
    {
        Cppi4Queue      rxFreeQueue = { PAL_CPPI41_QUEUE_MGR_PARTITION_SR, PAL_CPPI41_SR_CNI_INFRA_FD_HOST_Q_NUM( iProxyChan ) };

        if(!(priv->rxProxy_free_queue_hnd[iProxyChan] = PAL_cppi4QueueOpen(priv->pal_hnd, rxFreeQueue) ))
        {
            DPRINTK(" Unable to open free desc queue %d\n", rxFreeQueue.qNum);
            goto err;
        }
    }

    /***************************************************************
     * Prepare Accumulator channels
     * ============================
     */
    {
        int iAccChan;

        for(iAccChan=0; iAccChan < PAL_CPPI41_SR_CNI_INFRA_DMA_CH_COUNT; iAccChan++)
        {
            Cppi4Queue      rxQueue = { PAL_CPPI41_QUEUE_MGR_PARTITION_SR , PAL_CPPI41_SR_CNI_HOST_RX_Q_NUM( iAccChan ) };
            /*
             * Open Docsis Accumulator channels
             * --------------------------------
             */
            priv->rxAcc_chan_hnd[iAccChan] = NULL;
            priv->rxAcc_chan    [iAccChan] = PAL_CPPI41_CNI_ACC_CH_NUM(iAccChan);

            if( cnid_init_acc_chan( priv->pal_hnd,
                                    priv->rxAcc_chan[iAccChan],
                                    rxQueue,
                                   &priv->rxAcc_chan_hnd[iAccChan] ) )
            {
                DPRINTK(" Unable to open accumulator channel for %d priority channel\n", iAccChan);
                goto err;
            }

            priv->rxAcc_chan_list_base[iAccChan] = priv->rxAcc_chan_list[iAccChan] = PAL_cppi4AccChGetNextList(priv->rxAcc_chan_hnd[iAccChan]);
        }
    }

#ifdef CNI_USE_NAPI
    napi_enable(&priv->napi);
#elif defined(CNI_USE_TASKLET)
    tasklet_init(&priv->rx_tasklet, cnid_do_rx_complete_low, (unsigned long) dev);
#elif defined(CNI_USE_WORKQ)
    /* Create WQ */
    priv->cni_rx_wq = create_workqueue("cni_rx_wq");
    if (priv->cni_rx_wq == NULL)
    {
        printk("Failed to create cni_rx_wq\n");
        return -1;
    }
    /* Init the work */
    INIT_WORK(&(priv->cni_rx_work.work), cnid_do_rx_complete_low);
#endif

#ifdef CNI_USE_NAPI_HIGH
    napi_enable(&priv->napi_high);
#elif defined(CNI_USE_TASKLET_HIGH)
    tasklet_init(&priv->rx_tasklet_high, cnid_do_rx_complete_high, (unsigned long) dev);
#elif defined(CNI_USE_WORKQ_HIGH)
    /* Create WQ */
    priv->cni_rx_wq_high = create_workqueue("cni_rx_wq_high");
    if (priv->cni_rx_wq_high == NULL)
    {
        printk("Failed to create cni_rx_wq_high\n");
        return -1;
    }
    /* Init the work */
    INIT_WORK(&(priv->cni_rx_work_high.work), cnid_do_rx_complete_high);
#endif


#ifdef CNI_RX_NETIF
    /* Register a RX func */
    netdev_rx_handler_register(dev, cnid_rx_handler, NULL);
#endif
    /* request the Rx IRQs */
    if(request_irq (MAP_INTD_TO_INTC(PAL_CPPI41_CNI_LOW_ACC_CH_INTV_NUM), cnid_rx_interrupt, IRQF_DISABLED, "cniLow", dev))
    {
        DPRINTK(" unable to get IRQ #%d !\n", MAP_INTD_TO_INTC(PAL_CPPI41_CNI_LOW_ACC_CH_INTV_NUM));
        goto err;
    }

    /* request the Rx IRQs */
    if(request_irq (MAP_INTD_TO_INTC(PAL_CPPI41_CNI_HIGH_ACC_CH_INTV_NUM), cnid_rx_interrupt_high, IRQF_DISABLED, "cniHigh", dev))
    {
        DPRINTK(" unable to get IRQ #%d !\n", MAP_INTD_TO_INTC(PAL_CPPI41_CNI_HIGH_ACC_CH_INTV_NUM));
        goto err;
    }

#ifdef MTA_LIMIT_HIGH_PRIORITY_RATE
    mtaPacketCountCheck = MTA_PACKET_COUNT_CHECK;
    mtaPacketCheckMinTimeInJiffies = msecs_to_jiffies(MTA_PACKET_CHECK_MIN_TIME_MSEC); /* conversion here to not do for each packet counter reset */
    DPRINTK_MTA("is now set: %d[msec] = %lu[Jiffies] is the minimal time for %d packets to arrive in.",
                MTA_PACKET_CHECK_MIN_TIME_MSEC, mtaPacketCheckMinTimeInJiffies, mtaPacketCountCheck);
    lastMtaCounterResetTimeInJiffies = jiffies; /* so value for first loop will be a bit more updated */
#endif

    return 0;

err:
    cnid_close_rx_prep(dev);
    cnid_close_rx_finish(dev);

    return -1;
}


/**************************************************************************/
/*! \fn         cnid_open
 **************************************************************************
 *
 *  \brief      CNI Device Open API
 *
 *  \param[in]  Net Device
 *  \return     Ok
 **************************************************************************/
static int cnid_open (struct net_device *dev)
{
    struct cnid_private_t* priv = netdev_priv(dev);

    DPRINTK( " Enter \n");

    /* clear the state bit. We are getting opened */
    clear_bit(0, &priv->state);

#ifdef CONFIG_TI_PACKET_PROCESSOR
    cni_pp_set_pid_active( dev );
#endif

    if(cnid_open_tx(dev)) return -1;
    if(cnid_open_rx(dev)) return -1;

    netif_start_queue (dev);

    return 0;
}


#ifdef CONFIG_MACH_PUMA5
/************************************************************************/
/*                                                                      */
/*                                                                      */
/*                                                                      */
/************************************************************************/
static int cnid_close_tx_prep(struct net_device* dev)
{
    struct cnid_private_t* priv = netdev_priv(dev);

    /* de-init the Tx complete accumulator channel */
    PAL_cppi4AccChClose(priv->tx_acc_hnd, NULL);

    /* disable the Tx IRQs */
    disable_irq (MAP_INTD_TO_INTC(PAL_CPPI41_TX_COMPLETE_ACC_INTV_NUM));

    return 0;
}
#endif

/*----------------------------------------------------------------------*/
static int cnid_close_tx_finish(struct net_device* dev)
{
    cnid_private_t* priv = netdev_priv(dev);
    int i;

    /* de-init the Tx queues */
    for(i = 0; i < PAL_CPPI41_SR_DOCSIS_TX_DATA_Q_COUNT; i++)
    {
        PAL_cppi4QueueClose(priv->pal_hnd, priv->tx_queue[i]);
    }

#ifdef CONFIG_MACH_PUMA5

    tasklet_kill(&priv->tx_tasklet);

    kfree(priv->tx_list_base);

    free_irq (MAP_INTD_TO_INTC(PAL_CPPI41_TX_COMPLETE_ACC_INTV_NUM), dev);

#endif

    return 0;
}
/************************************************************************/


/************************************************************************/
/*                                                                      */
/*                                                                      */
/*                                                                      */
/************************************************************************/
static int cnid_close_rx_prep(struct net_device* dev)
{
    cnid_private_t* priv = netdev_priv(dev);
    int i;

    /* disable the Rx IRQs */
    disable_irq (MAP_INTD_TO_INTC(PAL_CPPI41_CNI_LOW_ACC_CH_INTV_NUM));
    disable_irq (MAP_INTD_TO_INTC(PAL_CPPI41_CNI_HIGH_ACC_CH_INTV_NUM));

    for(i = 0; i < PAL_CPPI41_SR_CNI_INFRA_DMA_CH_COUNT; i++)
    {
        PAL_cppi4AccChClose(priv->rxAcc_chan_hnd[i], NULL);
    }

    return 0;
}
/*----------------------------------------------------------------------*/
static int cnid_close_rx_finish(struct net_device* dev)
{
    int i;
    cnid_private_t* priv = netdev_priv(dev);

    for(i=0; i < PAL_CPPI41_SR_CNI_INFRA_DMA_CH_COUNT; i++)
    {
        if(priv->rxProxy_free_queue_hnd[i])
        {
            PAL_cppi4QueueClose(priv->pal_hnd, priv->rxProxy_free_queue_hnd[i]);
        }

        if(priv->rxAcc_chan_hnd[i])
        {
            kfree(priv->rxAcc_chan_list_base[i]);
        }
    }
#ifdef CNI_USE_NAPI
    napi_disable(&priv->napi);
#elif defined(CNI_USE_TASKLET)
    tasklet_kill(&priv->rx_tasklet);
#elif defined(CNI_USE_WORKQ)
    flush_workqueue(priv->cni_rx_wq);
    destroy_workqueue(priv->cni_rx_wq);
#endif

#ifdef CNI_USE_NAPI_HIGH
    napi_disable(&priv->napi_high);
#elif defined(CNI_USE_TASKLET_HIGH)
    tasklet_kill(&priv->rx_tasklet_high);
#elif defined(CNI_USE_WORKQ_HIGH)
    flush_workqueue(priv->cni_rx_wq_high);
    destroy_workqueue(priv->cni_rx_wq_high);
#endif

    /* free the Rx IRQs */
    free_irq (MAP_INTD_TO_INTC(PAL_CPPI41_CNI_LOW_ACC_CH_INTV_NUM), dev);
    free_irq (MAP_INTD_TO_INTC(PAL_CPPI41_CNI_HIGH_ACC_CH_INTV_NUM), dev);

    return 0;
}
/************************************************************************/


static int cnid_close (struct net_device *dev)
{
    cnid_private_t* priv = netdev_priv(dev);
    unsigned long flags;

    netif_stop_queue(dev);

#ifdef CONFIG_TI_PACKET_PROCESSOR
    cni_pp_set_pid_discard( dev );
#endif

    /*
     * Do the urgent stuff first
     */
#ifdef CONFIG_MACH_PUMA5
    cnid_close_tx_prep(dev);
#endif
    cnid_close_rx_prep(dev);

    spin_lock_irqsave(&priv->devlock, flags);
    set_bit(0, &priv->state);

#ifdef CNI_USE_NAPI
    if(cnid_do_rx_complete_low( dev, PAL_CPPI41_SR_CNI_INFRA_LOW_FD_HOST_DESC_COUNT ))
    {
        avalanche_intd_write_eoi(PAL_CPPI41_CNI_LOW_ACC_CH_INTV_NUM);
    }
#elif defined(CNI_USE_TASKLET)
    cnid_do_rx_complete_low((unsigned long)dev);
#endif

#ifdef CNI_USE_NAPI_HIGH
    if(cnid_do_rx_complete_high( dev, PAL_CPPI41_SR_CNI_INFRA_HIGH_FD_HOST_DESC_COUNT ))
    {
        avalanche_intd_write_eoi(PAL_CPPI41_CNI_HIGH_ACC_CH_INTV_NUM);
    }
#elif defined(CNI_USE_TASKLET_HIGH)
    cnid_do_rx_complete_high((unsigned long)dev);
#endif


#ifdef CNI_RX_NETIF
    /* Register a RX func */
    netdev_rx_handler_unregister(dev);
#endif

#ifdef CONFIG_MACH_PUMA5
    cnid_do_tx_complete((unsigned long)dev);
#endif

    spin_unlock_irqrestore(&priv->devlock, flags);

    /*
     * finish rest of janitorial stuff now
     */
    cnid_close_tx_finish(dev);
    cnid_close_rx_finish(dev);

    return 0;
}

// ARRIS ADD : SOCKET FILTER and DOCSIS BRIDGE BYPASS
extern void dbridgeSetIpv4Filter(struct net_device *ptr_netdev, unsigned int Ipv4FilterAddress, unsigned int Ipv4FilterPort);
extern void dbridgeSetBypass(struct net_device *ptr_netdev, int DbridgeBypass);
// END ARRIS

static int cnid_ioctl(struct net_device *dev, struct ifreq *rq, int cmd)
{
    // ARRIS ADD : LRO, SOCKET FILTER, and DOCSIS BRIDGE BYPASS APIs
    if (cmd == SIOCDEVPRIVATE) 
    {
        CniDrvPrivIoctl privIoctl;
        cnid_private_t *priv = (cnid_private_t *)netdev_priv(dev);

        /* Copy user data */
        if (copy_from_user((char *)&privIoctl, (char *)rq->ifr_data, sizeof(CniDrvPrivIoctl)))
            return -EFAULT;

        switch (privIoctl.cmd) 
        {
#ifdef CONFIG_INET_LRO
            case CNI_LRO_ENABLE:
                {
                    int enable;
                    if (copy_from_user((char *)&enable, (char *)privIoctl.data, sizeof(int)))
                        return -EFAULT;

                    if (enable)
                    {
                        dev->features |= NETIF_F_LRO;
                        priv->lro_complete = 0;
                    }
                    else
                    {
                        // set a flag to turn off LRO on next flush - if I flush here, we can crash because
                        // skb flushed may not be completely setup.
                        priv->lro_complete = 1;
                    }
                }
                break;
#endif

            case CNI_DOCBRIDGE_BYPASS:
                {
                    CniDrvDbridgeBypass bypass;                
                    if (copy_from_user((char *)&bypass, (char *)privIoctl.data, sizeof(CniDrvDbridgeBypass)))
                    {
                        // printk(KERN_ERR "Could not get the data\n");
                    
                        return -EFAULT;
                    }

                    switch(bypass.enable)
                    {
                        case ENABLE_FILTER_WITHOUT_BYPASS:
                        case ENABLE_FILTER_WITH_BYPASS:
                            if (bypass.addr.sa_family == AF_INET)
                            {
                                struct sockaddr_in *si4 = (struct sockaddr_in *)&bypass.addr;

                                priv->Ipv4FilterAddress = si4->sin_addr.s_addr;
                                priv->Ipv4FilterPort    = si4->sin_port;

                                dbridgeSetIpv4Filter(wanDev, priv->Ipv4FilterAddress, priv->Ipv4FilterPort);

                                if (bypass.enable == ENABLE_FILTER_WITH_BYPASS)
                                {
                                    // Set cni0 interface for downstream bypass
                                    priv->DbridgeBypass     = 1;

                                    // Set wan0 interface for upstream bypass
                                    dbridgeSetBypass(wanDev, 1);
                                }

                                //printk(KERN_ERR "Turn on Socket Filter for IPv4: %pI4:%d bypass is %d\n",
                                //       &(priv->Ipv4FilterAddress), priv->Ipv4FilterPort, priv->DbridgeBypass);
                            }
                            else if (bypass.addr.sa_family == AF_INET6)
                            {
                                printk(KERN_ERR "Turn on Socket Filter for IPv6 unsupported\n");
                            }
                            else
                            {
                                printk(KERN_ERR "Unknown FAMILY\n");
                            }
                            break;

                        case DISABLE_FILTER:
                            priv->Ipv4FilterAddress = 0;
                            priv->Ipv4FilterPort = 0;
                            priv->DbridgeBypass = 0;

                            dbridgeSetIpv4Filter(wanDev, 0, 0);
                            dbridgeSetBypass(wanDev, 0);

                            // printk(KERN_ERR "Turn off Socket Filter\n");
                            break;
                    }
                }
                break;

            case CNI_GET_STATS:
                {
                    CniDrvStats stats;
                    memset(&stats, 0, sizeof(CniDrvStats));

                    stats.rx_packets = priv->stats.rx_packets;
                    stats.tx_packets = priv->stats.tx_packets;
                    stats.rx_bytes   = priv->stats.rx_bytes;
                    stats.tx_bytes   = priv->stats.tx_bytes;
                    stats.rx_errors  = priv->stats.rx_errors;
                    stats.tx_errors  = priv->stats.tx_errors;

                    stats.filtered_rx_packets = priv->filtered_stats.rx_packets;
                    stats.filtered_tx_packets = priv->filtered_stats.tx_packets;
                    stats.filtered_rx_bytes   = priv->filtered_stats.rx_bytes;
                    stats.filtered_tx_bytes   = priv->filtered_stats.tx_bytes;

#ifdef CONFIG_INET_LRO
                    if (dev->features & NETIF_F_LRO)
                    {
                        // ARRIS MOD BEGIN : Use 2 aggregators
                        stats.lro_enabled = 1;
                        stats.rx_aggregated = priv->lro_mgr_high.stats.aggregated;
                        stats.rx_flushed    = priv->lro_mgr_high.stats.flushed;
                        
                        stats.rx_aggregated += priv->lro_mgr_low.stats.aggregated;
                        stats.rx_flushed    += priv->lro_mgr_low.stats.flushed;
                        // ARRIS MOD END
                    }
#endif

                    if (copy_to_user((char *)privIoctl.data, &stats, sizeof(CniDrvStats)))
                    {
                        printk(KERN_ERR "Could not send the data\n");
                        return -EFAULT;
                    }
                }
                break;

            default:
                /* ARRIS MOD START */
                printk(KERN_ERR "Unknown IOCTL (%d)!\n", privIoctl.cmd);
                /* ARRIS MOD END */
                break;
        }
    }
    // END ARRIS

    return 0;
}


static struct net_device_stats *cnid_get_stats (struct net_device *dev)
{
    cnid_private_t* priv = netdev_priv((struct net_device*) dev);

    return &priv->stats;
}


static void cnid_set_multicast (struct net_device *dev)
{

}

/* TODO: Hack this function to setup device address according to taste */
static void cnid_hard_addr_setup(struct net_device *dev)
{
    memcpy(dev->dev_addr, defmac, dev->addr_len);
    memcpy(dev->perm_addr, dev->dev_addr, dev->addr_len);
}


static const struct net_device_ops cnid__netdev_ops = {
         .ndo_open               = cnid_open,
         .ndo_start_xmit         = cnid_start_xmit,
         .ndo_stop               = cnid_close,
         .ndo_get_stats          = cnid_get_stats,
         .ndo_set_multicast_list = cnid_set_multicast,
         .ndo_do_ioctl           = cnid_ioctl,
         .ndo_validate_addr      = eth_validate_addr,
         .ndo_set_mac_address    = eth_mac_addr,
};


static void cnid_netdev_setup(struct net_device *dev)
{

     dev->netdev_ops = &cnid__netdev_ops;
   // dev->tx_timeout         =   cnid_tx_timeout;

    /* TODO: study the effects of these features */
    /* dev->features |= NETIF_F_SG | NETIF_F_HW_CSUM | NETIF_F_HIGHDMA */

    ether_setup(dev);
    cnid_hard_addr_setup(dev);
}


/* structure describing the CNID driver */
static struct device_driver cnid_driver = {
    .name       = "cni",
    .bus        = &platform_bus_type,
    .probe      = cnid_probe,
    .remove     = cnid_remove,
    .suspend    = NULL,
    .resume     = NULL,
};


#ifdef CONFIG_TI_PACKET_PROCESSOR


/* PID/VPID definitions */
#define PP_CNI_SR_DELAY     200

#ifdef CONFIG_MACH_PUMA5
static TI_PP_QOS_CLST_CFG               cni_qos_cluster_db[PAL_CPPI41_SR_DOCSIS_TX_QOS_CLUSTER_COUNT];
#else
static AVALANCHE_PP_QOS_CLST_CFG_t      cni_qos_cluster_db[PAL_CPPI41_SR_DOCSIS_TX_QOS_CLUSTER_COUNT];
#endif 

/*
 * Set PID Flags
 * -------------
 */
static int cni_pp_set_pid_discard(struct net_device *dev)
{
#ifdef CONFIG_MACH_PUMA5
    ti_ppm_set_pid_flags( dev->pid_handle, TI_PP_PID_DISCARD_ALL_RX );
#else
    avalanche_pp_pid_set_flags( dev->pid_handle, AVALANCHE_PP_PID_DISCARD_ALL_RX );
#endif
    /* this delay is to make sure all the packets with the PID successfully egress throgh the respective ports.*/
    mdelay(PP_CNI_SR_DELAY);
    return 0;
}

static int cni_pp_set_pid_active(struct net_device *dev)
{
#ifdef CONFIG_MACH_PUMA5
    ti_ppm_set_pid_flags( dev->pid_handle, 0 );
#else
    avalanche_pp_pid_set_flags( dev->pid_handle, 0 );
#endif
    /* this delay is to make sure all the packets with the PID successfully egress throgh the respective ports.*/
    mdelay(PP_CNI_SR_DELAY);
    return 0;
}


static int cni_pp_select_qos(struct sk_buff *skb)
{
#ifdef CONFIG_MACH_PUMA5
    #define SKB_PP_SESSION  ti_session
#else
    #define SKB_PP_SESSION  pp_session
#endif 
    unsigned int sfIndex = (skb->ti_meta_info >> 24);
    int queueNum;

    if (QOS_MAX_US_SF_BE_NUMBER > sfIndex)
    {
        skb->pp_packet_info.SKB_PP_SESSION.cluster = sfIndex;

        if (((wanDev) && (skb->skb_iif == wanDev->ifindex)) ||
            ((mtaDev) && (skb->skb_iif == mtaDev->ifindex)) )
        {
            /* Send packets from wan to best effort high priority QoS queues */
            queueNum = PAL_CPPI41_SR_DOCSIS_TX_HIGH_QPDSP_QOS_Q_NUM(sfIndex);
            skb->pp_packet_info.SKB_PP_SESSION.priority = 0; /* High Priority */
        }
        else
        {
            /* Send packets from wan to best effort low priority QoS queues */
            queueNum = PAL_CPPI41_SR_DOCSIS_TX_LOW_QPDSP_QOS_Q_NUM(sfIndex);
            skb->pp_packet_info.SKB_PP_SESSION.priority = 1; /* Low Priority */
        }
    }
    else
    {
        /* UGS case - no QoS applied */
        queueNum = PAL_CPPI41_SR_DOCSIS_TX_VOICE_Q_NUM;

        skb->pp_packet_info.SKB_PP_SESSION.cluster = 0xFF;
        skb->pp_packet_info.SKB_PP_SESSION.priority = 0; /* Default */
    }

    return queueNum;
}

#if defined(CONFIG_MACH_PUMA5) || defined(CONFIG_MACH_PUMA6)
/*
 * Set Turn Off High Priority Queue
 * -------------------------------- 
 * val == 0 to turn on [should be default value] 
 * use val != 0 to turn off the queue 
 */
static int cni_pp_set_TurnOffHighPriorityQueue(Uint8 val)
{
    Uint8 *dest = (Uint8*)(AVALANCHE_PP_MTA_TURN_OFF_HIGH_PRIORITY_QUEUE);
    DPRINTK_MTA("cni_pp_set_TurnOffHighPriorityQueue: %d -> %d", (int)(*dest), (int)val); 

    memset(dest, val, 1);
    mtaHighQueueToggleCounter++; /* count the PP state toggle for statistics */
    return 0;
}
#endif

#ifdef CONFIG_MACH_PUMA5

static int cni_pp_puma5_setup_qos (struct net_device *dev)
{
    int rc;
    unsigned int        clustIndex;
    TI_PP_QOS_QUEUE     *qcfg;

    for (clustIndex = PAL_CPPI41_SR_DOCSIS_TX_QOS_CLUSTER_BASE; clustIndex < PAL_CPPI41_SR_DOCSIS_TX_QOS_CLUSTER_COUNT; clustIndex++)
    {
        //
        // Setup 2 QOS queues, one that gets linespeed and then trickles down to the other.
        //
        cni_qos_cluster_db[clustIndex].qos_q_cnt  = 2;

        // Queue 0
        qcfg = &cni_qos_cluster_db[clustIndex].qos_q_cfg[0];
        qcfg->q_num               = PAL_CPPI41_SR_DOCSIS_TX_HIGH_QPDSP_QOS_Q_NUM(clustIndex) - PAL_CPPI41_SR_QPDSP_QOS_Q_BASE;
        qcfg->flags               = 0;
        qcfg->egr_q               = PAL_CPPI41_SR_DOCSIS_TX_DATA_Q_OFFSET(1 + clustIndex);
		qcfg->it_credit_bytes      = (120*1024*1024)/40000/8; /* <link speed - up to 4 US> / <PP ticks per sec> / <8 bits in byte> */
        
        qcfg->it_credit_packets    = 2;                     /* Rate=80KPPS, Resolution=1/64, TicksPerSec=40000(25usec) */
        qcfg->max_credit_bytes     = MIN_IP_PACKET_SIZE * 378;
        qcfg->max_credit_packets   = 256;
        qcfg->congst_thrsh_bytes   = MIN_IP_PACKET_SIZE * 378;
        qcfg->congst_thrsh_packets = 64;

        // Queue 1
        qcfg = &cni_qos_cluster_db[clustIndex].qos_q_cfg[1];
        qcfg->q_num                = PAL_CPPI41_SR_DOCSIS_TX_LOW_QPDSP_QOS_Q_NUM(clustIndex) - PAL_CPPI41_SR_QPDSP_QOS_Q_BASE;
        qcfg->flags                = 0;
        qcfg->egr_q                = PAL_CPPI41_SR_DOCSIS_TX_DATA_Q_OFFSET(1 + clustIndex);

        qcfg->it_credit_bytes      = 0;
        qcfg->it_credit_packets    = 0;
        qcfg->max_credit_bytes     = MAX_IP_PACKET_SIZE * 16;
        qcfg->max_credit_packets   = 16;
        qcfg->congst_thrsh_bytes   = MAX_IP_PACKET_SIZE * 20;
        qcfg->congst_thrsh_packets = 64;

        // Cluster 0
        cni_qos_cluster_db[clustIndex].global_credit_bytes   = 0;
        cni_qos_cluster_db[clustIndex].global_credit_packets = 0;
        cni_qos_cluster_db[clustIndex].max_global_credit_bytes  = 100000;
        cni_qos_cluster_db[clustIndex].max_global_credit_packets = 64;

        cni_qos_cluster_db[clustIndex].egr_congst_thrsh_bytes1  = ((MAX_IP_PACKET_SIZE)/ 64);       /* (Resolution: 64 bytes)*/
        cni_qos_cluster_db[clustIndex].egr_congst_thrsh_bytes2  = ((MAX_IP_PACKET_SIZE * 2)/ 64);   /* (Resolution: 64 bytes)*/
        cni_qos_cluster_db[clustIndex].egr_congst_thrsh_bytes3  = ((MAX_IP_PACKET_SIZE * 4)/ 64);   /* (Resolution: 64 bytes)*/
        cni_qos_cluster_db[clustIndex].egr_congst_thrsh_bytes4  = ((MAX_IP_PACKET_SIZE * 8)/ 64);   /* (Resolution: 64 bytes)*/
        cni_qos_cluster_db[clustIndex].egr_congst_thrsh_packets1 = 16 / 16;     /* (Resolution: 16 pkts)*/
        cni_qos_cluster_db[clustIndex].egr_congst_thrsh_packets2 = 32 / 16;     /* (Resolution: 16 pkts)*/
        cni_qos_cluster_db[clustIndex].egr_congst_thrsh_packets3 = 48 / 16;     /* (Resolution: 16 pkts)*/
        cni_qos_cluster_db[clustIndex].egr_congst_thrsh_packets4 = 64 / 16;     /* (Resolution: 16 pkts)*/

        rc = ti_ppm_qos_cluster_disable (clustIndex);
        rc = ti_ppm_qos_cluster_setup   (clustIndex, &cni_qos_cluster_db[clustIndex]);
        rc = ti_ppm_qos_cluster_enable  (clustIndex);

        dev->vpid_block.qos_cluster[clustIndex]  = &cni_qos_cluster_db[clustIndex];
    }

    dev->vpid_block.qos_clusters_count = clustIndex;

    return rc;
}


static int cni_pp_puma5_shutdown_qos (struct net_device *dev)
{
    int rc;
    unsigned int        clustIndex;

    for (clustIndex = PAL_CPPI41_SR_DOCSIS_TX_QOS_CLUSTER_BASE; clustIndex < PAL_CPPI41_SR_DOCSIS_TX_QOS_CLUSTER_COUNT; clustIndex++)
    {
        rc = ti_ppm_qos_cluster_disable (clustIndex);
    }

    dev->vpid_block.qos_clusters_count = 0;

    return rc;
}

static int cni_pp_puma5_prepare_pid(struct net_device *dev)
{
    TI_PP_PID           cni_pid_list[ PP_CNI_PID_COUNT ];
    TI_PP_PID_RANGE     pid_range_cni;
    int ret_val;
    int iPid;


    /*
     * Prepare interface for QoS settings - MUST be done before create PID is called!
     *
     */
    dev->qos_setup_hook                = cni_pp_puma5_setup_qos;
    dev->qos_shutdown_hook             = cni_pp_puma5_shutdown_qos;
    dev->qos_select_hook               = cni_pp_select_qos;

    /*
     * Config CNI PID range
     * --------------------
     */
    pid_range_cni.type        = TI_PP_PID_TYPE_DOCSIS;
    pid_range_cni.port_num    = PAL_CPPI41_SOURCE_PORT_DOCSIS;
    pid_range_cni.count       = PP_CNI_PID_COUNT;
    pid_range_cni.base_index  = PP_CNI_PID_BASE;

    if (ti_ppm_config_pid_range (&pid_range_cni))
    {
        DPRINTK (" config_pid_range failed\n");
    }

    /*
     * Create CNI PIDs
     * ---------------
     */
    iPid = 0;
    {
        cni_pid_list[iPid].type            = TI_PP_PID_TYPE_DOCSIS;
        cni_pid_list[iPid].ingress_framing = TI_PP_PID_INGRESS_ETHERNET
                                           | TI_PP_PID_INGRESS_IPV6
                                           | TI_PP_PID_INGRESS_IPV4;
        cni_pid_list[iPid].pri_mapping     = 1;    /* Num prio Qs for fwd */
        cni_pid_list[iPid].dflt_pri_drp    = 0;
        cni_pid_list[iPid].dflt_dst_tag    = 0x3FFF;
        cni_pid_list[iPid].dflt_fwd_q      = PAL_CPPI41_SR_CNI_INFRA_INPUT_Q_NUM(0);
        cni_pid_list[iPid].tx_pri_q_map[0] = PAL_CPPI41_SR_DOCSIS_TX_VOICE_Q_NUM; /* default Q for Egress is voice */
        cni_pid_list[iPid].tx_hw_data_len  = 0;
        cni_pid_list[iPid].pid_handle      = PP_CNI_PID_BASE+iPid;

        if ((ret_val = ti_ppm_create_pid (&cni_pid_list[iPid], dev)) < 0)
        {
            DPRINTK (" create_pid failed with error code %d.\n", ret_val);
            cni_pid_list[iPid].pid_handle = -1;
        }

        dev->pid_handle = cni_pid_list[iPid].pid_handle;
    }

    /*
     * Create CNI VPIDs
     * ----------------
     */
    dev->vpid_block.type               = TI_PP_ETHERNET;
    dev->vpid_block.parent_pid_handle  = dev->pid_handle;
    dev->vpid_block.egress_mtu         = 0;
    dev->vpid_block.priv_tx_data_len   = 0;
    return 0;
}

#else

static int cni_pp_puma6_setup_qos (struct net_device *dev)
{
    int rc;
    unsigned int                    clustIndex;
    AVALANCHE_PP_QOS_QUEUE_t *      qcfg;

    for (clustIndex = PAL_CPPI41_SR_DOCSIS_TX_QOS_CLUSTER_BASE; clustIndex < PAL_CPPI41_SR_DOCSIS_TX_QOS_CLUSTER_COUNT; clustIndex++)
    {
        //
        // Setup 2 QOS queues, one that gets linespeed and then trickles down to the other.
        //
        cni_qos_cluster_db[clustIndex].qos_q_cnt  = 2;

        // Queue 0
        qcfg = &cni_qos_cluster_db[clustIndex].qos_q_cfg[0];
        qcfg->q_num               = PAL_CPPI41_SR_DOCSIS_TX_HIGH_QPDSP_QOS_Q_NUM(clustIndex) - PAL_CPPI41_SR_QPDSP_QOS_Q_BASE;
        qcfg->flags               = 0;
        qcfg->egr_q               = PAL_CPPI41_SR_DOCSIS_TX_DATA_Q_NUM(1 + clustIndex); 
		qcfg->it_credit_bytes      = (240*1024*1024)/40000/8; /* <link speed - up to 8 US> / <PP ticks per sec> / <8 bits in byte> */
        qcfg->it_credit_packets    = 2;                     /* Rate=80KPPS, Resolution=1/64, TicksPerSec=40000(25usec) */
        qcfg->max_credit_bytes     = MIN_IP_PACKET_SIZE * 378;
        qcfg->max_credit_packets   = 256;
        qcfg->congst_thrsh_bytes   = MIN_IP_PACKET_SIZE * 378;
        qcfg->congst_thrsh_packets = 64;

        // Queue 1
        qcfg = &cni_qos_cluster_db[clustIndex].qos_q_cfg[1];
        qcfg->q_num                = PAL_CPPI41_SR_DOCSIS_TX_LOW_QPDSP_QOS_Q_NUM(clustIndex) - PAL_CPPI41_SR_QPDSP_QOS_Q_BASE;
        qcfg->flags                = 0;
        qcfg->egr_q               = PAL_CPPI41_SR_DOCSIS_TX_DATA_Q_NUM(1 + clustIndex); 
        qcfg->it_credit_bytes      = 0;
        qcfg->it_credit_packets    = 0;
        qcfg->max_credit_bytes     = MAX_IP_PACKET_SIZE * 16;
        qcfg->max_credit_packets   = 16;
        qcfg->congst_thrsh_bytes   = MAX_IP_PACKET_SIZE * 20;
        qcfg->congst_thrsh_packets = 64;

        // Cluster 0
        cni_qos_cluster_db[clustIndex].global_credit_bytes   = 0;
        cni_qos_cluster_db[clustIndex].global_credit_packets = 0;
        cni_qos_cluster_db[clustIndex].max_global_credit_bytes  = 100000;
        cni_qos_cluster_db[clustIndex].max_global_credit_packets = 64;        
        
        cni_qos_cluster_db[clustIndex].egr_congst_thrsh_bytes1  = MAX_IP_PACKET_SIZE;         
        cni_qos_cluster_db[clustIndex].egr_congst_thrsh_bytes2  = (MAX_IP_PACKET_SIZE * 2);   
        cni_qos_cluster_db[clustIndex].egr_congst_thrsh_bytes3  = (MAX_IP_PACKET_SIZE * 4);   
        cni_qos_cluster_db[clustIndex].egr_congst_thrsh_bytes4  = (MAX_IP_PACKET_SIZE * 8);  
        cni_qos_cluster_db[clustIndex].egr_congst_thrsh_packets1 = 16;    
        cni_qos_cluster_db[clustIndex].egr_congst_thrsh_packets2 = 32;     
        cni_qos_cluster_db[clustIndex].egr_congst_thrsh_packets3 = 48;     
        cni_qos_cluster_db[clustIndex].egr_congst_thrsh_packets4 = 64;     

        rc = avalanche_pp_qos_cluster_disable( clustIndex );
        rc = avalanche_pp_qos_cluster_setup  ( clustIndex, &cni_qos_cluster_db[clustIndex] );
        rc = avalanche_pp_qos_cluster_enable ( clustIndex );

        dev->vpid_block.qos_cluster[clustIndex]  = &cni_qos_cluster_db[clustIndex];
    }

    dev->vpid_block.qos_clusters_count = clustIndex;

    return rc;
}


static int cni_pp_puma6_shutdown_qos (struct net_device *dev)
{
    int rc;
    unsigned int        clustIndex;

    for (clustIndex = PAL_CPPI41_SR_DOCSIS_TX_QOS_CLUSTER_BASE; clustIndex < PAL_CPPI41_SR_DOCSIS_TX_QOS_CLUSTER_COUNT; clustIndex++)
    {
        rc = avalanche_pp_qos_cluster_disable (clustIndex);
    }

    dev->vpid_block.qos_clusters_count = 0;

    return rc;
}

/*
 * Create CNI PID range, PID, VPID
 * -------------------------------
 * Defualt Q when no match - 222 (proxy ch queue)
 * Defualt Q when match - 182 (DOS US)
 */
static int cni_pp_puma6_prepare_pid(struct net_device *dev)
{
    AVALANCHE_PP_PID_t           cni_pid;
    AVALANCHE_PP_PID_RANGE_t     cni_pid_range;
    int ret_val;

    /*
     * Prepare interface for QoS settings - MUST be done before create PID is called!
     *
     */
    dev->qos_setup_hook                = cni_pp_puma6_setup_qos;
    dev->qos_shutdown_hook             = cni_pp_puma6_shutdown_qos;
    dev->qos_select_hook               = cni_pp_select_qos;

    /*
     * Config CNI PID range
     * --------------------
     */
    cni_pid_range.type        = AVALANCHE_PP_PID_TYPE_DOCSIS;
    cni_pid_range.port_num    = PAL_CPPI41_SOURCE_PORT_DOCSIS;
    cni_pid_range.count       = PP_CNI_PID_COUNT;
    cni_pid_range.base_index  = PP_CNI_PID_BASE;

    if (avalanche_pp_pid_config_range (&cni_pid_range))
    {
        DPRINTK (" config_pid_range failed\n");
    }

    /*
     * Create CNI PIDs
     * ---------------
     */
    {
        cni_pid.type            = AVALANCHE_PP_PID_TYPE_DOCSIS;
        cni_pid.ingress_framing = AVALANCHE_PP_PID_INGRESS_ETHERNET
                                | AVALANCHE_PP_PID_INGRESS_IPV6
                                | AVALANCHE_PP_PID_INGRESS_IPV4;
        cni_pid.pri_mapping     = 1;    /* Num prio Qs for fwd */
        cni_pid.dflt_pri_drp    = 0;
        cni_pid.dflt_dst_tag    = 0x3FFF;
        cni_pid.dflt_fwd_q      = PAL_CPPI41_SR_CNI_INFRA_INPUT_Q_NUM(0);
        cni_pid.tx_pri_q_map[0] = PAL_CPPI41_SR_DOCSIS_TX_VOICE_Q_NUM; /* default Q for Egress is voice */
        cni_pid.tx_hw_data_len  = 0;
        cni_pid.pid_handle      = PP_CNI_PID_BASE;

        if ((ret_val = avalanche_pp_pid_create (&cni_pid, dev)) < 0)
        {
            DPRINTK (" create_pid failed with error code %d.\n", ret_val);
            cni_pid.pid_handle = -1;
        }

        dev->pid_handle = cni_pid.pid_handle;
    }

    /*
     * Create CNI VPIDs
     * ----------------
     */
    dev->vpid_block.type               = AVALANCHE_PP_VPID_ETHERNET;
    dev->vpid_block.parent_pid_handle  = dev->pid_handle;
    return 0;
}
#endif
#endif

static int cppi_usage_help(char *page, char **start, off_t off, int count, int *eof, void *data)
{
    int len = 0;

    len += sprintf (page+len,"\n The command usage: \n"
                             "\t dump queue\t<q manager (0/1)>\t<queue number>     [ silent ]\n"
                             "\t dump buff\t<b manager (0/1)>\t<buff pool number> [ silent ]\n"
                             "\t dump mem\t<address>\n"
                             "\t repeat \t<iterations> copy <len> <src addr> <dst addr>\n");
    /* Return the length. */
    return len;

}

#define CPPI_DBG_BUFFER_CACHE_SIZE      4096
Uint32 buffCache[CPPI_DBG_BUFFER_CACHE_SIZE];


/**************************************************************************
 * FUNCTION NAME : cppi_write_cmds
 **************************************************************************
 * DESCRIPTION   :
 *  This is used to debug and display various
 *  CPPI entity information from the console.
 *
 * RETURNS       :
 *  -1              - Error.
 *  Non-Zero        - Success.
 ***************************************************************************/
static int cppi_write_cmds (struct file *file, const char *buffer, unsigned long count, void *data)
{
    char    proc_cmd[100];
    char*   argv[10];
    int     argc = 0;
    char*   ptr_cmd;
    char*   delimitters = " \n\t";
    char*   ptr_next_tok;
    char    cmd_err = 0;

    /* Validate the length of data passed. */
    if (count > 100)
        count = 100;

    /* Initialize the buffer before using it. */
    memset ((void *)&proc_cmd[0], 0, sizeof(proc_cmd));
    memset ((void *)&argv[0], 0, sizeof(argv));

    /* Copy from user space. */
    if (copy_from_user (&proc_cmd, buffer, count))
        return -EFAULT;

    ptr_next_tok = &proc_cmd[0];

    /* Tokenize the command. Check if there was a NULL entry. If so be the case the
     * user did not know how to use the entry. Print the help screen. */
    ptr_cmd = strsep(&ptr_next_tok, delimitters);
    if (ptr_cmd == NULL)
        return -1;

    /* Parse all the commands typed. */
    do
    {
        /* Extract the first command. */
        argv[argc++] = ptr_cmd;

        /* Validate if the user entered more commands.*/
        if (argc >=10)
        {
            printk ("ERROR: Incorrect too many parameters dropping the command\n");
            return -EFAULT;
        }

        /* Get the next valid command. */
        ptr_cmd = strsep(&ptr_next_tok, delimitters);
    } while (ptr_cmd != NULL);

    /* We have an extra argument when strsep is used instead of strtok */
    argc--;

    /******************************* Command Handlers *******************************/

    /* Display Command Handlers */
    if (strncmp(argv[0], "repeat", strlen("repeat")) == 0)
    {
        /* repeat <iterations> <copy> <len> <src> <dst> */
        int iterations = (int) simple_strtol(argv[1], NULL, 0);

        if (argc == 6)
        {
            if (strncmp(argv[2], "copy", strlen("copy")) == 0)
            {
                unsigned int * src = (unsigned int *)simple_strtol(argv[4], NULL, 16);
                unsigned int * dst = (unsigned int *)simple_strtol(argv[5], NULL, 16);
                unsigned int * tmp_1;
                unsigned int * tmp_2;
                         int len = (int) simple_strtol(argv[3], NULL, 0);
                printk (" repeat: %d copy from 0x%08X to 0x%08X (len=%d)\n", iterations, (unsigned int)src, (unsigned int)dst, len);

                do
                {
                    printk("#");
                    tmp_1 = (src + len - 1);
                    tmp_2 = (dst + len - 1);

                    while (tmp_1 >= src)
                    {
                        *tmp_2-- = *tmp_1--;
                    }

                } while (iterations--);
            }
            else
            {
                cmd_err = 1;
            }
        }
        else
        {
            cmd_err = 1;
        }

        printk("\n repeat DONE !!\n");
    }
    else
    if ((strncmp(argv[0], "sh", strlen("sh")) == 0) ||
        (strncmp(argv[0], "du", strlen("du")) == 0))
    {
        static PAL_Handle handle  = NULL;
        static PAL_Handle dHandle = NULL;

        if (NULL == handle)
        {
            handle  = PAL_cppi4Init( NULL, (Ptr)CPPI41_DOMAIN_PRIMARY_SR );
        }

        if (NULL == dHandle)
        {
            dHandle = PAL_cppi4Init( NULL, (Ptr)CPPI41_DOMAIN_PRIMARY_DOCSIS );
        }

        if (strncmp(argv[1], "mem", strlen("mem")) == 0)
        {
            if (3 == argc)
            {

                Uint32 dmem = simple_strtol(argv[2], NULL, 16);
                Uint32 memVptr;
                int i;

                if (dmem > PHYS_OFFSET)
                {
                    memVptr = (Uint32)PAL_CPPI4_PHYS_2_VIRT(dmem);
                }
                else
                {
                    memVptr = (Uint32)IO_PHY2VIRT(dmem);
                }

                for (i=0; i<16; i++)
                {
                    printk("[0x%08X]: 0x%08lx 0x%08lx 0x%08lx 0x%08lx\n", dmem+i*16,
                        *((unsigned long *)(memVptr+i*16)),
                        *((unsigned long *)(memVptr+i*16+4)),
                        *((unsigned long *)(memVptr+i*16+8)),
                        *((unsigned long *)(memVptr+i*16+12))
                        );
                }
            }
            else
            {
                cmd_err = 1;
            }
        }
        else
        if (strncmp(argv[1], "que", strlen("que")) == 0)
        {
            if (4 <= argc)
            {
                PAL_Cppi4QueueHnd qHandle;
                PAL_Cppi4BD *desc;
                int idx = 0;
                int idx1;
                Uint32 minAddr = 0xFFFFFFFF;
                Uint32 maxAddr = 0;
                Uint32 cppiMgr      = (Uint32) simple_strtol(argv[2], NULL, 0);
                Uint32 cppiQueueNum = (Uint32) simple_strtol(argv[3], NULL, 0);
                PAL_Handle hnd = NULL;

                if (PAL_CPPI41_QUEUE_MGR_PARTITION_SR == cppiMgr)
                {
                    hnd = handle;
                }
                else
                {
                    hnd = dHandle;
                }

                printk(" == Dumping Queue [ %3d ][ %3d ]\n", cppiMgr, cppiQueueNum);

                qHandle = PAL_cppi4QueueOpen(hnd, (Cppi4Queue){cppiMgr, cppiQueueNum});

                do
                {
                    desc = PAL_cppi4QueuePop(qHandle);

                    buffCache[idx++] = (Uint32)(desc);

                    if (desc && ((Uint32)desc > maxAddr))
                    {
                        maxAddr = (Uint32)desc;
                    }

                    if (desc && ((Uint32)desc < minAddr))
                    {
                        minAddr = (Uint32)desc;
                    }

                } while ((NULL != desc) && (CPPI_DBG_BUFFER_CACHE_SIZE > idx));

                if (NULL == desc)
                {
                    idx--;
                }
                else
                {
                    printk(" == Reached cache limit of %d entries ==\n", CPPI_DBG_BUFFER_CACHE_SIZE);
                }


                printk (" == Queue [ %3d ] contains %d descriptors ==\n", cppiQueueNum, idx);

                if ((argc == 4) || (strncmp(argv[4], "sil", strlen("sil")) != 0))
                {
                    for (idx1=0; idx1<idx; idx1++)
                    {
                        Uint32 descVptr;

                        if (buffCache[idx1] > PHYS_OFFSET)
                        {
                            descVptr = (Uint32)PAL_CPPI4_PHYS_2_VIRT(buffCache[idx1]);
                        }
                        else
                        {
                            descVptr = (Uint32)IO_PHY2VIRT(buffCache[idx1]);
                        }

                        printk (" ========================================\n");
    #if 0
                        for (i=0; i<4; i++)
                        {
                            printk("[0x%08X]:\t0x%08lx 0x%08lx 0x%08lx 0x%08lx\n", (buffCache[idx1]+i*16),
                                *((unsigned long *)(descVptr + i*16)),
                                *((unsigned long *)(descVptr + i*16+4)),
                                *((unsigned long *)(descVptr + i*16+8)),
                                *((unsigned long *)(descVptr + i*16+12))
                                );
                        }
    #else
                        printk("[0x%08X]:I\t%08lx %08lx %08lx\n", (buffCache[idx1]),
                                *((unsigned long *)(descVptr)),
                                *((unsigned long *)(descVptr+4)),
                                *((unsigned long *)(descVptr+8))
                            );

                        printk("[0x%08X]:B\t%08lx %08lx %08lx %08lx %08lx %08lx %08lx %08lx\n", (buffCache[idx1]+12),
                                *((unsigned long *)(descVptr+12)),
                                *((unsigned long *)(descVptr+16)),
                                *((unsigned long *)(descVptr+20)),
                                *((unsigned long *)(descVptr+24)),
                                *((unsigned long *)(descVptr+28)),
                                *((unsigned long *)(descVptr+32)),
                                *((unsigned long *)(descVptr+36)),
                                *((unsigned long *)(descVptr+40))
                            );

                        printk("[0x%08X]:N\t%08lx %08lx \n", (buffCache[idx1]+44),
                                *((unsigned long *)(descVptr+44)),
                                *((unsigned long *)(descVptr+48))
                            );

                        printk("[0x%08X]:P\t%08lx %08lx %08lx\n", (buffCache[idx1]+52),
                                *((unsigned long *)(descVptr+52)),
                                *((unsigned long *)(descVptr+56)),
                                *((unsigned long *)(descVptr+60))
                            );
    #endif
                    }
                    printk (" ========================================\n");
                }

                for (idx1=0; idx1<idx; idx1++)
                {
                    PAL_cppi4QueuePush(qHandle, (Uint32 *)buffCache[idx1], (CPPI4_BD_LENGTH_FOR_CACHE-24)/4, 0);
                }

                PAL_cppi4QueueClose(hnd, qHandle);

                printk (" == All the %d descriptors are back into Queue [ %3d ]\n", idx, cppiQueueNum);
                printk ("    The desc addresses range is [%08X .. %08X]\n", minAddr, maxAddr);
            }
            else
            {
                cmd_err = 1;
            }
        }
        else
        if (strncmp(argv[1], "buf", strlen("buf")) == 0)
        {
            if (4 <= argc)
            {
                Ptr buffer;
                int idx = 0;
                int idx1;
                Uint32 minAddr = 0xFFFFFFFF;
                Uint32 maxAddr = 0;
                Uint32 cppiMgr          = (Uint32) simple_strtol(argv[2], NULL, 0);
                Uint32 cppiBufPoolNum   = (Uint32) simple_strtol(argv[3], NULL, 0);
                PAL_Handle hnd = NULL;

                if (PAL_CPPI41_QUEUE_MGR_PARTITION_SR == cppiMgr)
                {
                    hnd = handle;
                }
                else
                {
                    hnd = dHandle;
                }

                printk(" == Dumping Buffer Pool [ %3d ][ %3d ]\n", cppiMgr, cppiBufPoolNum);

                do
                {
                    buffer = PAL_cppi4BufPopBuf(hnd, (Cppi4BufPool){cppiMgr, cppiBufPoolNum});

                    buffCache[idx++] = (Uint32)(buffer);

                    if (buffer && ((Uint32)buffer > maxAddr))
                    {
                        maxAddr = (Uint32)buffer;
                    }

                    if (buffer && ((Uint32)buffer < minAddr))
                    {
                        minAddr = (Uint32)buffer;
                    }

                } while ((NULL != buffer) && (CPPI_DBG_BUFFER_CACHE_SIZE > idx));

                if (NULL == buffer)
                {
                    idx--;
                }
                else
                {
                    printk(" == Reached cache limit of %d entries ==\n", CPPI_DBG_BUFFER_CACHE_SIZE);
                }

                printk (" == The Buffer Pool [ %3d ] contains %d buffers ==\n", cppiBufPoolNum, idx);

                if ((argc == 4) || (strncmp(argv[4], "sil", strlen("sil")) != 0))
                {
                    printk (" ========================================");
                    for (idx1=0; idx1<idx; idx1++)
                    {
                        if (idx1%16 == 0)
                        {
                            printk("\n");
                        }
                        printk(" %08X", buffCache[idx1]);
                    }
                    printk ("\n ========================================\n");
                }

                for (idx1=0; idx1<idx; idx1++)
                {
                    PAL_cppi4BufDecRefCnt(hnd, (Cppi4BufPool){cppiMgr, cppiBufPoolNum}, (Ptr)buffCache[idx1]);
                }

                printk (" == All the %d buffers are back into the pool [%3d]\n", idx, cppiBufPoolNum);
                printk ("    The buffer addresses range is [%08X .. %08X]\n", minAddr, maxAddr);
            }
            else
            {
                cmd_err = 1;
            }
        }
        else
        {
            cmd_err = 1;
        }
    }

    if (cmd_err)
    {
        char print_buffer[128*16];
        cppi_usage_help(print_buffer,NULL,0,0,NULL,NULL);
        printk("%s",print_buffer);
    }

    return count;
}


// ARRIS ADD : LRO PATCH (updated for 2.39.3)
#ifdef CONFIG_INET_LRO
static int cnid_get_skb_hdr(struct sk_buff *skb, void **iphdr,
			       void **tcph, u64 *hdr_flags, void *priv)
{
	unsigned int ip_len;
	struct iphdr *iph;
	skb_reset_network_header(skb);
	iph = ip_hdr(skb);
	if (iph->protocol != IPPROTO_TCP)
		return -1;
	ip_len = ip_hdrlen(skb);
	skb_set_transport_header(skb, ip_len);
	*tcph = tcp_hdr(skb);

	if (iph->version == 4)
		*hdr_flags = LRO_IPV4;
	
	*hdr_flags |= LRO_TCP;
	*iphdr = iph;
	return 0;
}
#endif
// END ARRIS

/* TODO: in a fully linux framework compliant driver,
 * we will receive the resources: io, mem and intr
 * in the device structure.
 */
static int __devinit cnid_probe(struct device *dev)
{

    return 0;
}


static int __devexit cnid_remove (struct device *dev)
{
    struct net_device *netdev = platform_get_drvdata (to_platform_device(dev));
    cnid_private_t* priv = netdev_priv(netdev);

    PAL_cppi4Exit(priv->pal_hnd, NULL);

    unregister_netdev (netdev);

    platform_device_unregister(to_platform_device(dev));

    return 0;
}


static int __init cnid_init_module (void)
{
    static struct platform_device *cnid_dev;
    struct net_device *netdev = NULL;
    cnid_private_t *priv = NULL;
    int ret;

    DPRINTK (KERN_INFO DRV_NAME "\n");

    if(sizeof(Cppi4HostDescLinux) > PAL_CPPI41_SR_CNI_INFRA_FD_HOST_DESC_SIZE)
    {
       DPRINTK( "%s fundamentally broken. Contact maintainer!\n", DRV_NAME);
       return -1;
    }

    /* Initialize MAC address */
    if (inpmac && strlen(inpmac) == 17)
    {
        int i;
        int m[6];

        /* Translate MAC address from ASCII to binary */
        sscanf(inpmac, "%x:%x:%x:%x:%x:%x", &(m[0]), &(m[1]), &(m[2]), &(m[3]), &(m[4]), &(m[5]));
        for (i = 0; i < 6; i++)
        {
            defmac[i] = (Uint8)m[i];
        }
    }
    DPRINTK(KERN_INFO"cnid : Using MAC %x:%x:%x:%x:%x:%x", defmac[0], defmac[1], defmac[2], defmac[3], defmac[4], defmac[5]);

    /* TODO: this should be in a board file not here.
     * No fun registering driver and device in the same file. */
    cnid_dev = platform_device_register_simple("cni", -1, NULL, 0);

    if (IS_ERR(cnid_dev))
    {
        return -1;
    }

    DPRINTK(" [%p] \n", cnid_dev);

    ret = driver_register(&cnid_driver);
    if (ret)
    {
        DPRINTK(" rc=%d\n", ret);

        platform_device_unregister(cnid_dev);

        DPRINTK("\n");

        return -1;
    }

    DPRINTK("\n");

    ret = driver_create_file(&cnid_driver, &driver_attr_version);
    if (ret)
    {
        printk("cni0: Unable driver create file ret=%d\n",ret);
        return ret;
    }

    DPRINTK("\n");

    /* dev and priv zeroed in alloc_netdev. Thank God for small mercies... */
    netdev = alloc_netdev (sizeof(cnid_private_t), "cni%d", cnid_netdev_setup);
    if (netdev == NULL)
    {
        printk("cni0: Unable to alloc new net device\n");
        return -ENOMEM;
    }

    DPRINTK("\n");

    SET_NETDEV_DEV(netdev, &(cnid_dev->dev));
    platform_set_drvdata(cnid_dev, netdev);

    DPRINTK("\n");

    priv = netdev_priv(netdev);
    priv->netdev = netdev;

    DPRINTK("\n");

    ret = register_netdev (netdev);
    if(ret)
    {
        DPRINTK( "Unable to register device named %s (%p)...\n", netdev->name, netdev);
        return ret;
    }

    DPRINTK("\n");

#ifdef CNI_USE_NAPI
    /* Create cnid_rx task */
    netif_napi_add(netdev, &priv->napi, cnid_poll_low, CNID_RX_SERVICE_MAX);
#endif
#ifdef CNI_USE_NAPI_HIGH
    /* Create cnid_rx task */
    netif_napi_add(netdev, &priv->napi_high, cnid_poll_high, CNID_RX_SERVICE_MAX);
#endif

    DPRINTK( "Registered device named %s (%p)...\n", netdev->name, netdev);

    /* get a PAL handle */
    priv->pal_hnd = PAL_cppi4Init(NULL, NULL);

#ifdef CONFIG_TI_PACKET_PROCESSOR
    /*
     * Config CNI PID/VPID
     */
#ifdef CONFIG_MACH_PUMA5
    cni_pp_puma5_prepare_pid (netdev);
#else
    cni_pp_puma6_prepare_pid (netdev);
#endif

#endif

    spin_lock_init(&priv->devlock);

#ifdef CONFIG_INET_LRO
    /* LRO Setup */
    
    // ARRIS MOD BEGIN : Use 2 aggregators
    priv->lro_mgr_high.dev = netdev;
    memset(&priv->lro_mgr_high.stats, 0, sizeof(priv->lro_mgr_high.stats));
#ifdef CNI_USE_NAPI_HIGH
    priv->lro_mgr_high.features = LRO_F_NAPI;
#else
    priv->lro_mgr_high.features = 0;
#endif
    priv->lro_mgr_high.ip_summed = CHECKSUM_UNNECESSARY;
    priv->lro_mgr_high.ip_summed_aggr = CHECKSUM_UNNECESSARY; //CHECKSUM_NONE;
    priv->lro_mgr_high.max_desc = ARRAY_SIZE(priv->lro_arr_high);
    priv->lro_mgr_high.max_aggr = 32;
    priv->lro_mgr_high.frag_align_pad = 0;
    priv->lro_mgr_high.lro_arr = priv->lro_arr_high;
    priv->lro_mgr_high.get_skb_header = cnid_get_skb_hdr;
    memset(&priv->lro_arr_high, 0, sizeof(priv->lro_arr_high));

    priv->lro_mgr_low.dev = netdev;
    memset(&priv->lro_mgr_low.stats, 0, sizeof(priv->lro_mgr_low.stats));
#ifdef CNI_USE_NAPI
    priv->lro_mgr_low.features = LRO_F_NAPI;
#else
    priv->lro_mgr_low.features = 0;
#endif    
    priv->lro_mgr_low.ip_summed = CHECKSUM_UNNECESSARY;
    priv->lro_mgr_low.ip_summed_aggr = CHECKSUM_UNNECESSARY; //CHECKSUM_NONE;
    priv->lro_mgr_low.max_desc = ARRAY_SIZE(priv->lro_arr_low);
    priv->lro_mgr_low.max_aggr = 32;
    priv->lro_mgr_low.frag_align_pad = 0;
    priv->lro_mgr_low.lro_arr = priv->lro_arr_low;
    priv->lro_mgr_low.get_skb_header = cnid_get_skb_hdr;
    memset(&priv->lro_arr_low, 0, sizeof(priv->lro_arr_low));
    // ARRIS MOD END

    /* Disable LRO by default */
    netdev->features &= ~NETIF_F_LRO;

    /* Init ethtool options */
#ifdef CONFIG_MACH_PUMA6    // CJL-PUMA5 TODO
    cnid_set_ethtool_ops( netdev );
#endif // CJL    
#endif

    {
        struct proc_dir_entry * res = create_proc_entry("cppi" ,0644, init_net.proc_net);

        if (res)
        {
            res->data = NULL;
            res->write_proc = cppi_write_cmds;
            res->read_proc  = cppi_usage_help;
        }
    }

    DPRINTK(" Exit \n");

    return 0;
}


static void __exit cnid_cleanup_module (void)
{
    driver_remove_file(&cnid_driver, &driver_attr_version);
    driver_unregister(&cnid_driver);
}

#ifdef CONFIG_INET_LRO

#ifdef CONFIG_MACH_PUMA6    // CJL-PUMA5 TODO
/* Very minimal ethtool support for LRO */
static const struct ethtool_ops cnid_ethtool_ops = {
        .get_flags              = ethtool_op_get_flags,
        .set_flags              = ethtool_op_set_flags,
};


static void cnid_set_ethtool_ops(struct net_device *dev)
{
        SET_ETHTOOL_OPS(dev, (struct ethtool_ops *)&cnid_ethtool_ops);
}
#endif
#endif // CJL
module_param(inpmac, charp, 0);
module_init(cnid_init_module);
module_exit(cnid_cleanup_module);

