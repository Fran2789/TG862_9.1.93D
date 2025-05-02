/*
 * hil_intrusive.c - HIL Intrusive Mode Profile
 *
 * Description:
 *  The file contains the implementation of the HIL Intrusive Mode Profile.
 *  The profile use the following kernel features
 *   - CONFIG_TI_DEVICE_PROTOCOL_HANDLING
 *   - CONFIG_TI_EGRESS_HOOK
 *  The profile install hooks into the Ingress and Egress Data Path and
 *  populates the session structure which is stored in the SKB for the LUT
 *  and Modification record configuration. These hooks are installed on
 *  networking devices which have an PID or VPID handle associated with it.
 *
 *  The profile is provided as is and can be used as a template for
 *  development of more system specific profiles.
 *
 * Copyright (C) <2008>, Texas Instruments, Incorporated
 *
 *  This program is free software; you can distribute it and/or modify it
 *  under the terms of the GNU General Public License (Version 2) as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *  for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
 */
/*
Includes Intel Corporation's changes/modifications dated: 2015.
Changed/modified portions - Copyright ? 2015, Intel Corporation.
*/

/**************************************************************************
 *************************** Include Files ********************************
 **************************************************************************/

#include <linux/ctype.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/netdevice.h>
#include <linux/inetdevice.h>
#include <linux/in.h>
#include <linux/if_ether.h>
#include "../../8021q/vlan.h"
#include <linux/if_pppox.h>
#include <linux/ip.h>
#include <linux/ipv6.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/ppp_defs.h>
#include <asm-arm/arch-avalanche/generic/pal_cppi41.h>
#include <linux/ti_hil.h>
#include <linux/mroute.h>
#include <net/ip.h>
#include <net/ipv6.h>
#include "linux/ti_ppm.h"
#include "../../bridge/br_private.h"
#include <linux/proc_fs.h>

#include <asm-arm/arch-avalanche/generic/pal.h>
#include "linux/ti_pp_path.h"

#include <mach/puma.h>
#include <mach/hardware.h>

#include <asm-arm/arch-avalanche/generic/ti_ppd.h>

#include <asm-arm/arch-avalanche/puma5/puma5_pp.h>

#ifndef TI_MAX_DEVICE_INDEX
#define TI_MAX_DEVICE_INDEX 64
#endif




#ifdef CONFIG_NETFILTER
#include <linux/netfilter.h>
#include <net/netfilter/nf_conntrack.h>
#include <net/netfilter/nf_conntrack_helper.h>
#include <linux/netfilter_ipv4/ip_tables.h>
#endif

/**************************************************************************
 ************************************ Local Definitions *******************
 **************************************************************************/

/* Definitons required for the HIL Analysis. */
#define HIL_SESSION_STAT_BUCKET           64
#define HIL_MAX_NUM_BUCKETS               TI_PP_MAX_ACCLERABLE_SESSIONS / HIL_SESSION_STAT_BUCKET

/* PDSP Health Timer; to periodically check if the PDSP are executing properly or not. */
#define PDSP_HEALTH_TIMER_SEC             60

#define DUMMY_FOR_TUNNEL_1                1
#define DUMMY_FOR_TUNNEL_0                2

// ARRIS make all PP CLI printk high enough priority that they will print out
#define PRINTK_CLI(fmt, ...)   printk(KERN_ERR fmt, ## __VA_ARGS__)

#define DUMMY_FOR_TUNNEL_1                1
#define DUMMY_FOR_TUNNEL_0                2

/**************************************************************************
 *************************** Static Definitions ***************************
 **************************************************************************/

/* Debug Dumping Functions */
static void ti_hil_intrusive_display_l2         (TI_PP_SESSION_PROPERTY* sessionInfo, Bool isIngress);
static void ti_hil_intrusive_display_ipv4       (TI_PP_IPV4_DESC*   ptr_ipv4_lut_entry);
static void ti_hil_intrusive_display_ipv6       (TI_PP_IPV6_DESC*   ptr_ipv6_lut_entry);
static const char* ti_hil_intrusive_display_ipv6_addr(const void *cp, char *buf, size_t len);
static void ti_hil_intrusive_display_session    (int session_handle);


/* Profile Initialization/Deinitialization and networking event handlers. */
static int ti_hil_intrusive_init (void);
static int ti_hil_netsubsystem_event_handler(unsigned int module_id, unsigned long event_id, void* ptr);
static int ti_hil_intrusive_deinit(void);

#ifdef CONFIG_IP_MULTICAST
static int hil_mfc_delete_session_by_mc_group(int mc_group);
static int hil_mfc_check_entry (unsigned int mcast_group);
static int hil_mfc_add_entry (unsigned int mcast_group, unsigned char session_handle);
static int hil_mfc_del_entry (unsigned int mcast_group);
static int hil_mfc_to_session(struct pp_mr_param* ptr_mr_param, TI_PP_SESSION* ptrsession);
#endif /* CONFIG_IP_MULTICAST */

#ifdef CONFIG_NETFILTER
static void hil_add_session_handle_to_ct_mapper_list (Uint16 new_session_handle,Uint16 prev_new_session_handle);
static Bool  hil_find_session_handle_in_ct_mapper_list (Uint16 ct_session_handle,Uint16 pp_session_handle);
static void hil_delete_session_handle_in_ct_mapper_list (Uint16 pp_session_handle,enum ip_conntrack_dir dir);
#endif

#ifdef CONFIG_INTEL_PP_TUNNEL_SUPPORT
int ti_hil_delete_tunnel(void);
#endif

//extern char * qMNGRname[];

void ti_hil_test_session_creation(void);


inline Bool IsTdoxCandidate(TI_PP_SESSION* ptr_session)
{
    if (((ptr_session->egress[0].l3l4_packet.packet_type == TI_PP_IPV6_TYPE)         &&
        (ptr_session->egress[0].l3l4_packet.u.ipv6_desc.next_header == IPPROTO_IPIP) &&
        (ptr_session->ingress.l3l4_packet.packet_type == TI_PP_IPV4_TYPE)            &&
        (ptr_session->ingress.l3l4_packet.u.ipv4_desc.protocol == IPPROTO_TCP))      ||
        /* Populate the ack suppression - Tdox check and enable for IPv6 */
        ((ptr_session->egress[0].l3l4_packet.packet_type == TI_PP_IPV6_TYPE)         &&
        (ptr_session->egress[0].l3l4_packet.u.ipv6_desc.next_header == IPPROTO_TCP)) ||
        /* Populate the ack suppression - Tdox check and enable for IPv4 */
        ((ptr_session->egress[0].l3l4_packet.packet_type == TI_PP_IPV4_TYPE) &&
        (ptr_session->egress[0].l3l4_packet.u.ipv4_desc.protocol == IPPROTO_TCP)))
    {
        return True;
    }
    return False;
}

/**************************************************************************
 ********************************* Globals ********************************
 **************************************************************************/

/* Default Profile. */
TI_HIL_PROFILE      hil_intrusive_profile = {
    .name            = "intrusive",
    .profile_handler = ti_hil_netsubsystem_event_handler,
    .profile_init    = ti_hil_intrusive_init,
    .profile_deinit  = ti_hil_intrusive_deinit,
};

/* Packet Processor Subsystem Event Handler. */
unsigned int        ppsubsystem_event_handler;

/* PP PDSP Health Timer to verify if the PP is running correctly. */
struct timer_list   pdsp_health_timer;

#ifdef CONFIG_NETFILTER
/* This is a global data structure which maps the session handle to corresponding
 * connection tracking entries. */
struct ct_mapper
{
    Uint16 next;
    Uint16 previous;
    struct nf_conn* conntrack;

};
struct ct_mapper hil_session_ct_mapper [TI_PP_MAX_ACCLERABLE_SESSIONS];

#endif /* CONFIG_NETFILTER */

#ifdef CONFIG_IP_MULTICAST
unsigned int hil_mfc_session_mapper[TI_PP_MAX_ACCLERABLE_SESSIONS];
#endif /* CONFIG_IP_MULTICAST */

/* This is the default session IDLE timeout in seconds. */
#define DEFAULT_SESSION_TIMEOUT_SEC    1
int ti_session_timeout_sec = DEFAULT_SESSION_TIMEOUT_SEC;

#define DOCSIS_FW_PACKET_API_TCP_HIGH_PRIORITY    (1 << 10)
struct
{
    int                 hil_disabled;
    int                 dbg_disabled;
    int                 tdox_disabled;
    int                 qos_disabled;
    int                 ipv6rd_pp_support;
    Uint32              tdoxEvalMinTimeMsec;
    Uint32              tdoxEvalAvgPktSizeThresh;
    Uint32              tdoxEvalPpsThresh;
    bool                tdoxDbg;
#ifdef CONFIG_INTEL_PP_TUNNEL_SUPPORT
    int                 tunnelMode;
#endif

    unsigned int        num_bypassed_pkts;
    unsigned int        num_other_pkts;
    unsigned int        num_ingress_pkts;
    unsigned int        num_egress_pkts;
    unsigned int        num_null_drop_pkts;

/* Counters for keeping track of stats for the HIL Analysis. */
    unsigned int        num_total_sessions;
    unsigned int        num_error;
    unsigned int        session_bucket[HIL_MAX_NUM_BUCKETS];

}
global_ti_hil_db =
{
#ifdef CONFIG_INTEL_PP_TUNNEL_SUPPORT
    .hil_disabled       =   1,
#else
    .hil_disabled       =   !ARRIS_PP_ENABLED, /* ARRIS enable per feedback from TI */
#endif
    .dbg_disabled       =   1,
    .tdox_disabled      =   0,
    .qos_disabled       =   0,
    .ipv6rd_pp_support  =   0,
    .tdoxEvalMinTimeMsec        = 3000,
    .tdoxEvalAvgPktSizeThresh   = 128,
    .tdoxEvalPpsThresh          = 100,
    .tdoxDbg                    = 0,
#ifdef CONFIG_INTEL_PP_TUNNEL_SUPPORT
    .tunnelMode         =   1,
#endif
    .num_bypassed_pkts  =   0,
    .num_other_pkts     =   0,
    .num_ingress_pkts   =   0,
    .num_egress_pkts    =   0,
    .num_null_drop_pkts =   0,
    .num_total_sessions =   0,
    .num_error          =   0
};
static int certMode = 0 ; // ARRIS ADD
#ifdef CONFIG_INTEL_PP_TUNNEL_SUPPORT
static int gTunnel0Handle = -1;
static int gTunnel1Handle = -1;
#endif

/* Packet terminating VPID handle */
static int  docsis_null_vpid_handle = -1;
static int  netfilter_null_vpid_handle = -1;

/* GEN PP dropped packest, used with TI_CT_GEN_DISCARD_PKT */
/* This is a bitmap.
 * User sets bits according to protocol : IPv4/IPv6
 */
/* Macros for handling */
#define DROPPED_PACKETS_BITMAP_SET(__n) do { dropped_packets_bit_map |= (1 << (__n)); } while (0)
#define DROPPED_PACKETS_BITMAP_UNSET(__n) do { dropped_packets_bit_map &= ~(1 << (__n)); } while (0)
#define DROPPED_PACKETS_BITMAP_IS_SET(__n) ((dropped_packets_bit_map & (1 << (__n))) != 0)

static int dropped_packets_bit_map = 0;
/* get IPv6 traffic class */
#define IPV6TCLASS(h)       (((h->priority&0x0F)<<4) | ((h->flow_lbl[0]&0xF0)>>4))
#define IPV6FLOWLABEL(h)    ((h->flow_lbl[0] & 0x0F << 16) | (h->flow_lbl[1] << 8) | h->flow_lbl[2])

/************************************************************************/
/*                                                                      */
/*                                                                      */
/*     TDOX stuff                                                       */
/*                                                                      */
/*                                                                      */
/************************************************************************/
#define TI_HIL_TDOX_MAX_SESSIONS        16

typedef struct tdox_db_entry_t
{
    struct tdox_db_entry_t *    next;
    int                         pp_session_handle;
    int                         tdox_session_handle;
}
tdox_db_entry_t;

static tdox_db_entry_t  tdox_db_repository[TI_HIL_TDOX_MAX_SESSIONS];

typedef struct
{
    struct tdox_db_entry_t *    activeList;
    int                         activeNum;
    struct tdox_db_entry_t *    freeList;
    int                         freeNum;
}
tdox_db_ctrl_t;

static tdox_db_ctrl_t   tdox_db;

typedef struct
{
    Uint32  pktForward;
    Uint32  byteForward;
    Int32   tdoxHandle;
    Int32   lastUpdateTime;
    Uint32  pktDelta;
    Uint32  bytesDelta;
    Uint32  timeDelta;
    Uint32  tdoxSwaps;
}hil_session_entry_t;

hil_session_entry_t hilSessionDb[TI_PP_MAX_ACCLERABLE_SESSIONS];

/************************************************************************/

/* Test stuff */
Uint8   *usPktDataIngressP = NULL;
Uint16   usPktDataIngressSize = 0;
Uint8   *usPktDataEgressP = NULL;
Uint16   usPktDataEgressSize = 0;
Uint8   *dsPktDataIngressP = NULL;
Uint16   dsPktDataIngressSize = 0;
Uint8   *dsPktDataEgressP = NULL;
Uint16   dsPktDataEgressSize = 0;



/**************************************************************************
 *************************** Extern Definitions ***************************
 **************************************************************************/

extern int ti_deregister_egress_hook_handler (struct net_device* dev);
extern int ti_register_egress_hook_handler (struct net_device* dev, int (*egress_hook)(struct sk_buff *skb));

#ifdef CONFIG_TI_PACKET_PROCESSOR_STATS
/* DOCSIS Packet processor start session notification Callback */
extern TI_HIL_START_SESSION ti_hil_start_session_notification_cb;
/* DOCSIS Packet processor delete session notification Callback */
extern TI_HIL_DELETE_SESSION ti_hil_delete_session_notification_cb;
#endif /* CONFIG_TI_PACKET_PROCESSOR_STATS */

/**************************************************************************
 ******************************* Functions  *******************************
 **************************************************************************/

#define TDOX_LOCK(lockKey)      PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);
#define TDOX_UNLOCK(lockKey)    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);

/**************************************************************************
 * FUNCTION NAME : ti_hil_session_db_reset_entry
 **************************************************************************
 * DESCRIPTION   :
 *
 * RETURNS:
 *   0  -  OK
 *  -1  -  ERROR
 **************************************************************************/
static int ti_hil_session_db_reset_entry(int pp_session_handle)
{
    unsigned int lockKey;

    if ((pp_session_handle < 0) || (pp_session_handle >= TI_PP_MAX_ACCLERABLE_SESSIONS))
    {
        return -1;
    }

    TDOX_LOCK(lockKey);

    memset((void *)&hilSessionDb[pp_session_handle], 0, sizeof(hil_session_entry_t));
    hilSessionDb[pp_session_handle].tdoxHandle = -1;

    TDOX_UNLOCK(lockKey);

    return 0;
}

/**************************************************************************
 * FUNCTION NAME : ti_hil_tdox_alloc_session
 **************************************************************************
 * DESCRIPTION   :
 *  The function allocates TDOX session handle.
 *
 * RETURNS:
 *   0  -  Session is allocated.
 *  -1  -  There are no TDOX IDs available at the time.
 **************************************************************************/
static int ti_hil_tdox_alloc_session (void)
{
    struct tdox_db_entry_t * tmp;
    unsigned int lockKey;

    TDOX_LOCK(lockKey);

    if (0 == tdox_db.freeNum)
    {
        TDOX_UNLOCK(lockKey);
        return -1;
    }

    tdox_db.freeNum--;

    tmp =                   tdox_db.freeList;
    tdox_db.freeList =      tdox_db.freeList->next;
    tmp->next =             tdox_db.activeList;
    tdox_db.activeList =    tmp;

    tdox_db.activeNum++;

    TDOX_UNLOCK(lockKey);

#ifdef CONFIG_TI_HIL_DEBUG
#ifdef TDOX_DEBUG
    if (0 == global_ti_hil_db.dbg_disabled)
    {
        printk("\n==== TDOX: Allocate Session %d ====\n",tdox_db.activeList->tdox_session_handle);
    }
#endif
#endif

    return (tdox_db.activeList->tdox_session_handle);
}


/**************************************************************************
 * FUNCTION NAME : ti_hil_tdox_associate_session
 **************************************************************************
 * DESCRIPTION   :
 *  The function associates PP session to TDOX session handle.
 *
 * RETURNS:
 *   0  -  Session is associated.
 *  -1  -  Session parameters are invalid.
 **************************************************************************/
static int ti_hil_tdox_associate_session (unsigned char tdox_handle, int pp_session_handle)
{
    if (tdox_handle >= TI_HIL_TDOX_MAX_SESSIONS)
    {
        return -1;
    }

    if ((pp_session_handle < 0) || (pp_session_handle >=TI_PP_MAX_ACCLERABLE_SESSIONS))
    {
        return -1;
    }

#ifdef CONFIG_TI_HIL_DEBUG
#ifdef TDOX_DEBUG
    if (0 == global_ti_hil_db.dbg_disabled)
    {
        printk("\n==== TDOX: Associate tdox Session %d = pp %d ====\n", tdox_handle, pp_session_handle);
    }
#endif
#endif

    tdox_db_repository[tdox_handle].pp_session_handle = pp_session_handle;
    hilSessionDb[pp_session_handle].tdoxHandle = tdox_handle;
    return 0;
}


/**************************************************************************
 * FUNCTION NAME : ti_hil_tdox_free_session
 **************************************************************************
 * DESCRIPTION   :
 *  The function releases TDOX ID.
 *  The search can be done either per PP or TDOX session handle key.
 *  In case one of the search keys is unknown, the -1 should be specified.
 *
 * RETURNS:
 *   0  -  Session is freed.
 *  -1  -  Session is NOT found.
 **************************************************************************/
static int ti_hil_tdox_free_session (int pp_session_handle, int tdox_session_handle)
{
    tdox_db_entry_t *   curr;
    tdox_db_entry_t *   prev;
    tdox_db_entry_t *   tmp;
    unsigned int        lockKey;

    TDOX_LOCK(lockKey);

    if (0 == tdox_db.activeNum)
    {
        TDOX_UNLOCK(lockKey);
        return -1;
    }

#ifdef CONFIG_TI_HIL_DEBUG
#ifdef TDOX_DEBUG
    if (0 == global_ti_hil_db.dbg_disabled)
    {
        printk("\n==== TDOX: Free Session [%d] START     ====\n",pp_session_handle);
    }
#endif
#endif

    curr = tdox_db.activeList;
    prev = NULL;

    do
    {
        if (
            ((curr->pp_session_handle != -1)   && (curr->pp_session_handle == pp_session_handle)) ||
            ((curr->tdox_session_handle != -1) && (curr->tdox_session_handle == tdox_session_handle))
           )
        {
            tdox_db.activeNum--;

            if (prev)
            {
                prev->next =         curr->next;
            }
            else
            {
                tdox_db.activeList = curr->next;
            }

            tmp =                    tdox_db.freeList;
            tdox_db.freeList =       curr;
            tdox_db.freeList->next = tmp;

            if (curr->pp_session_handle != -1)
            {
                hilSessionDb[curr->pp_session_handle].tdoxHandle = -1;
            }

            tdox_db.freeList->pp_session_handle = -1;

            tdox_db.freeNum++;

            TDOX_UNLOCK(lockKey);

#ifdef CONFIG_TI_HIL_DEBUG
#ifdef TDOX_DEBUG
            if (0 == global_ti_hil_db.dbg_disabled)
            {
                printk("\n==== TDOX: Free Session [%d] OK        ====\n",pp_session_handle);
            }
#endif
#endif

            return 0;
        }

        prev = curr;
        curr = curr->next;
    }
    while (NULL != curr);

    TDOX_UNLOCK(lockKey);

#ifdef CONFIG_TI_HIL_DEBUG
#ifdef TDOX_DEBUG
    if (0 == global_ti_hil_db.dbg_disabled)
    {
        printk("\n==== TDOX: Free Session [%d] Not Found ====\n",pp_session_handle);
    }
#endif
#endif

    return -1;
}

/**************************************************************************
 * FUNCTION NAME : ti_hil_tdox_get_session_entry
 **************************************************************************
 * DESCRIPTION   :
 *  The function performs search for specified PP session ID in TDOX repository.
 *  In case it finds it returns the pointer to the entry.
 *
 * RETURNS:
 *  pointer to the entry.
 *  NULL -  Session is NOT found.
 **************************************************************************/
static tdox_db_entry_t * ti_hil_tdox_get_session_entry(int pp_session_handle)
{
    int index;

    for (index=0; index<TI_HIL_TDOX_MAX_SESSIONS; index++ )
    {
        if (tdox_db_repository[index].pp_session_handle == pp_session_handle)
        {
            return &(tdox_db_repository[index]);
        }
    }

    return NULL;
}


/**************************************************************************
 * FUNCTION NAME : ti_hil_tdox_init
 **************************************************************************
 * DESCRIPTION   :
 *  The initialization function of TDOX driver internal database.
 *
 * RETURNS       :
 *  0   -   Success
 **************************************************************************/
static int ti_hil_tdox_init (void)
{
    int                 idx;
    tdox_db_entry_t *   prev = NULL;

    for (idx=0; idx < TI_HIL_TDOX_MAX_SESSIONS; idx++)
    {
        tdox_db_repository[idx].tdox_session_handle = idx;
        tdox_db_repository[idx].pp_session_handle = -1;
        tdox_db_repository[idx].next = prev;
        prev = &tdox_db_repository[idx];
    }

    tdox_db.freeList    = prev;
    tdox_db.freeNum     = TI_HIL_TDOX_MAX_SESSIONS;
    tdox_db.activeList  = NULL;
    tdox_db.activeNum   = 0;

    return 0;
}


/**************************************************************************
 * FUNCTION NAME : ti_hil_tdox_print
 **************************************************************************
 * DESCRIPTION   :
 *  Utility function prints out the TDOX driver internal database.
 *
 * RETURNS       :
 *  0   -   Success
 **************************************************************************/
static int ti_hil_tdox_print (void)
{
    unsigned int       idx;

	// ARRIS MOD START
    PRINTK_CLI ("\n==== TDOX DB ====\n");

    PRINTK_CLI ("  Free [%08X], num [%d]\n", (unsigned int)tdox_db.freeList, tdox_db.freeNum);
    PRINTK_CLI ("  Act  [%08X], num [%d]\n", (unsigned int)tdox_db.activeList, tdox_db.activeNum);

    PRINTK_CLI ("\n==== TDOX RECORDS ====\n");
    PRINTK_CLI ("+-----+------+------------+------------+------+\n");
    PRINTK_CLI ("| idx | sess |    addr    |    next    |  PP  |\n");
    PRINTK_CLI ("+-----+------+------------+------------+------+\n");
	// ARRIS MOD END

    for (idx=0; idx < TI_HIL_TDOX_MAX_SESSIONS; idx++)
    {
        PRINTK_CLI("| %3d | %4d | 0x%08X | 0x%08X | %4d |\n",  // ARRIS MOD
                    idx,
                    tdox_db_repository[idx].tdox_session_handle,
                    (unsigned int)&tdox_db_repository[idx],
                    (unsigned int)tdox_db_repository[idx].next,
                    tdox_db_repository[idx].pp_session_handle
                    );
    }
    PRINTK_CLI ("+-----+------+------------+------------+------+---------------+\n"); // ARRIS MOD

    return 0;
}


#define TI_HIL_TDOX_ENABLED             TI_PP_SESSION_APP_RAW_INFO1_B3_VALID
#define TI_HIL_TDOX_SKIP_TIMESTAMP      TI_PP_SESSION_APP_RAW_INFO1_B3_PLUS_VALID
#define TI_HIL_TCP_SYN                  TI_PP_SESSION_APP_HIL_USED01

/************************************************************************/
/*  TCP Options Numbers                                                 */
/************************************************************************/
#define TCP_OPTION_CODE_EOL         0  // 0  End of Option List                     [RFC793]
#define TCP_OPTION_CODE_NOP         1  // 1  No-Operation                           [RFC793]
                                       // 2  Maximum Segment Size                   [RFC793]
                                       // 3  WSOPT - Window Scale                   [RFC1323]
                                       // 4  SACK Permitted                         [RFC2018]
                                       // 5  SACK                                   [RFC2018]
                                       // 6  Echo (obsoleted by option 8)           [RFC1072]
                                       // 7  Echo Reply (obsoleted by option 8)     [RFC1072]
#define TCP_OPTION_CODE_TIMESTAMP   8  // 8  TSOPT - Time Stamp Option              [RFC1323]
                                       // 9  Partial Order Connection Permitted     [RFC1693]
                                       // 10 Partial Order Service Profile          [RFC1693]
                                       // 11 CC                                     [RFC1644]
                                       // 12 CC.NEW                                 [RFC1644]
                                       // 13 CC.ECHO                                [RFC1644]
                                       // 14 TCP Alternate Checksum Request         [RFC1146]
                                       // 15 TCP Alternate Checksum Data            [RFC1146]
                                       // 16 Skeeter                                [Knowles]
                                       // 17 Bubba                                  [Knowles]
                                       // 18 Trailer Checksum Option                [Subbu & Monroe]
                                       // 19 MD5 Signature Option                   [RFC2385]
                                       // 20 SCPS Capabilities                      [Scott]
                                       // 21 Selective Negative Acknowledgements    [Scott]
                                       // 22 Record Boundaries                      [Scott]
                                       // 23 Corruption experienced                 [Scott]
                                       // 24 SNAP                                   [Sukonnik]
                                       // 25 Unassigned (released 2000-12-18)
                                       // 26 TCP Compression Filter                 [Bellovin]
                                       // 27 Quick-Start Response                   [RFC4782]
                                       // 28 User Timeout Option                    [RFC-ietf-tcpm-tcp-uto-11.txt]
/************************************************************************/


#if 0 /* TODO: Enable once router functionality is available */

/**************************************************************************
 * FUNCTION NAME : ti_hil_is_device_routed
 **************************************************************************
 * DESCRIPTION   :
 *  Utility function checks if the device is attached to the IP stack.
 *
 * RETURNS       :
 *  1   -   Device is attached to the IP stack.
 *  0   -   Device is not attached to the IP stack
 **************************************************************************/
static int ti_hil_is_device_routed (struct net_device* dev)
{
    struct in_device *in_dev = in_dev_get(dev);

    if ((in_dev == NULL) || (in_dev->ifa_list == NULL))
        return 0;

    return 1;
}

#endif

/**************************************************************************
 * FUNCTION NAME : ti_hil_ipv4_checksum
 **************************************************************************
 * DESCRIPTION   :
 *  Utility function to calculate IPv4 checksum field
 *
 * RETURNS       :
 *  ... the checksum
 **************************************************************************/
Uint16 ti_hil_ipv4_checksum(Uint8 *ipv4Header, u16 ipv4HeaderLen)
{
    Uint32 checksum;
    Uint16 i;

    for (i = 0, checksum = 0; i < ipv4HeaderLen ; i += 2)
    {
        checksum += (Uint32)((ipv4Header[i] << 8) & 0xFF00) + (ipv4Header[i+1] & 0xFF);
    }

    while (checksum & 0xFFFF0000)
    {
        checksum = (checksum >> 16) + (checksum & 0xFFFF);
    }

    return ((Uint16)~checksum);
}


/**************************************************************************
 * FUNCTION NAME : ti_hil_get_l2_l3_pointers
 **************************************************************************
 * DESCRIPTION   :
 *  extracts HIL relevant fields from the L2 header
 *
 * RETURNS       :
 *  0 for success
 **************************************************************************/
inline static int ti_hil_get_l2_l3_pointers(char* ptr_data, struct ethhdr** ptr_ethhdr, struct iphdr** ptr_iphdr, struct ipv6hdr** ptr_ipv6hdr, TI_PP_SESSION_PROPERTY* ptr_ses_property)
{
    unsigned short protocol_type;

    /* Get the pointer to the Ethernet header */
    *ptr_ethhdr = (struct ethhdr *)ptr_data;

    /* Skip the Ethernet header. */
    ptr_data = ptr_data + sizeof(struct ethhdr);

    /* Get the protocol type. */
    protocol_type = (*ptr_ethhdr)->h_proto;

    /* Check the protocol field. If the protocol is VLAN or not?  */
    if (protocol_type == __constant_htons(ETH_P_8021Q))
    {
        /* Get the VLAN header. */
        struct vlan_hdr* ptr_vlanheader = (struct vlan_hdr *)ptr_data;

        /* The new protocol is encapsulated into the VLAN header. */
        protocol_type = ptr_vlanheader->h_vlan_encapsulated_proto;

        ptr_ses_property->l2_packet.u.eth_desc.enables  |= TI_PP_SESSION_L2_VLAN_VALID;
        ptr_ses_property->l2_packet.u.eth_desc.vlan_tag  = ptr_vlanheader->h_vlan_TCI;

        /* Skip the 4 bytes VLAN header. We have already accounted for the Ethernet header. */
        ptr_data = ptr_data + sizeof(struct vlan_hdr);
    }
    else
    {
        ptr_ses_property->l2_packet.u.eth_desc.vlan_tag  = 0;
    }

    /* We have skipped the layer2 information; so try and get the layer3 information. */
    switch (protocol_type)
    {
        case __constant_htons(ETH_P_IP):
        {
            /* Get the pointer to the IPv4 Header. */
            *ptr_iphdr = (struct iphdr *)ptr_data;
            *ptr_ipv6hdr = NULL;
            break;
        }
        case __constant_htons(ETH_P_IPV6):
        {
            /* Get the pointer to the IPv6 Header. */
            *ptr_ipv6hdr = (struct ipv6hdr *)ptr_data;
            *ptr_iphdr = NULL;
            break;
        }
        case __constant_htons(ETH_P_PPP_DISC):
        case __constant_htons(ETH_P_PPP_SES):
        {
            /* PPP Packet: Skip the PPP header. */
            ptr_data = ptr_data + 6;

            /* Get the PPP Protocol Information*/
            protocol_type = *((unsigned short *)ptr_data);
            if (protocol_type == __constant_htons(PPP_IP))
            {
                /* IP Packet. */
                *ptr_iphdr = (struct iphdr *)(ptr_data + 2);
            }
            break;
        }
        default:
        {
            /* This is a default condition to handle any packet type which is not understood. These packets are currently not accelerated by the PP. Example ARP Packets etc. */
            return -1;
        }
    }

    return 0;
}

/**************************************************************************
 * FUNCTION NAME : ti_hil_extract_l2
 **************************************************************************
 * DESCRIPTION   :
 *  Extract L2 fields
 *
 * RETURNS       :
 *  0 for success
 **************************************************************************/
inline static int ti_hil_extract_l2(struct ethhdr* ptr_ethhdr, TI_PP_SESSION_PROPERTY* ptr_ses_property)
{
    TI_PP_PACKET_DESC* ptr_pkt_desc;

    ptr_pkt_desc = &ptr_ses_property->l2_packet;

    /* Check did we have an Ethernet header. */
    if (ptr_ethhdr != NULL)
    {
        /* YES. Ethernet header was detected. Initialize the various fields. */
        ptr_pkt_desc->packet_type = TI_PP_ETH_TYPE;

        /* Populate the destination MAC address. */
        memcpy(ptr_pkt_desc->u.eth_desc.dstmac, (void *)&ptr_ethhdr->h_dest, 6);
        ptr_pkt_desc->u.eth_desc.enables  |= TI_PP_SESSION_L2_DSTMAC_VALID;

        /* Populate the source MAC address. */
        memcpy(ptr_pkt_desc->u.eth_desc.srcmac, (void *)&ptr_ethhdr->h_source, 6);
        ptr_pkt_desc->u.eth_desc.enables  |= TI_PP_SESSION_L2_SRCMAC_VALID;
    }
    else
    {
        /* No Ethernet header was present. Reset all the fields in the packet descriptor */
        memset ((void *)ptr_pkt_desc, 0, sizeof(TI_PP_PACKET_DESC));
    }

    return 0;
}

/**************************************************************************
 * FUNCTION NAME : ti_hil_extract_ipv4
 **************************************************************************
 * DESCRIPTION   :
 *  Extract IPv4 fields
 *
 * RETURNS       :
 *  0 for success
 **************************************************************************/
/* ARRIS propagate INTEL MR25140 fix */
inline static int ti_hil_extract_ipv4(struct ethhdr* ptr_ethhdr, struct iphdr* ptr_iphdr, struct tcphdr** ptr_tcphdr, TI_PP_SESSION_PROPERTY* ptr_ses_property, Bool is_ingress, unsigned char pid_type)
{
    char* ptr_data;
    TI_PP_PACKET_DESC*  ptr_pkt_desc;

    ptr_pkt_desc = &ptr_ses_property->l3l4_packet;

    /* Currently we support only TCP & UDP. For both these protocols the PORT information lies at the same location. */
    if ((ptr_iphdr->protocol == IPPROTO_TCP) || (ptr_iphdr->protocol == IPPROTO_UDP))
    {
        /* L4 will be valid only within non fragmented packet or first fragment (both have offset = 0) */
        if ((ptr_iphdr->frag_off & IP_OFFSET) == 0)
        {
            *ptr_tcphdr = (struct tcphdr *)((char *)ptr_iphdr + ptr_iphdr->ihl*4);
        }
        else
        {
            return -1;
        }
    }
    /* ARRIS propagate INTEL MR25140 fix */
    if ((!is_ingress) && (ptr_iphdr->protocol == IPPROTO_GRE) )
    {
        if (pid_type == TI_PP_PID_TYPE_DOCSIS)
        {
            __sum16 savedCheck; 
            __be16  savedTotLen;
            __be16  savedId;
            Bool    err = False;

            /* Check if we support this type of US GRE by looking at the GRE protocol type field */
            ptr_data = (Uint8*)ptr_iphdr + (ptr_iphdr->ihl * 4);    /* Go to GRE header */
            if (*(Uint32*)ptr_data != ETH_P_TEB)  /* 0x6558 - Transparent Ethernet Bridging */
            {
                /* unsupported GRE */
                return -1;
            }

            /* Supported US GRE */
            ptr_ses_property->l2_packet.u.eth_desc.enables |= TI_PP_SESSION_L2_GRE_US_VALID;

            /* Now need to calculate checksum with payload length=0 and identification is incremented by =0x100
               (we start with Identification high number so that host and PP packets will not have same Identification at session start) */
            /* Save the origianl values */
            savedCheck = ptr_iphdr->check;
            savedTotLen = ptr_iphdr->tot_len;
            savedId = ptr_iphdr->id;
            /* Change them to allow better PP utilization */
            ptr_iphdr->check = 0;
            ptr_iphdr->tot_len = 0;
            ptr_iphdr->id = 0xFFF;
            ptr_iphdr->check = ti_hil_ipv4_checksum((Uint8*)ptr_iphdr, ptr_iphdr->ihl * 4);

            /* Setup ipv4HdrRaw parameters - Save encapsulating L2, L3, GRE and encapsulated L2 */
            ptr_pkt_desc->u.ipv4_desc.ipv4HdrRawOffset = (Uint8*)ptr_iphdr - (Uint8*)ptr_ethhdr;
            ptr_pkt_desc->u.ipv4_desc.ipv4HdrRawLen = (ptr_iphdr->ihl * 4) + 4 + sizeof(struct ethhdr); /* 4 is for the GRE header */
            if (ptr_pkt_desc->u.ipv4_desc.ipv4HdrRawLen <= TI_PP_IPV4_HEADER_RAW_SIZE_MAX)
            {
                struct ethhdr* encap_ptr_ethhdr;
                unsigned short encap_prortocol_type;

                /* Save encapsulating L2, L3, GRE and encapsulated L2 */
                memcpy(ptr_pkt_desc->u.ipv4_desc.ipv4HdrRaw, ptr_iphdr, ptr_pkt_desc->u.ipv4_desc.ipv4HdrRawLen);

                /* If the encapsulated packet contains VLAN then add it to the template */
                encap_ptr_ethhdr = (struct ethhdr *)(ptr_data + 4); /* Get the pointer to the encapsulated Ethernet header */
                encap_prortocol_type = encap_ptr_ethhdr->h_proto;   /* Get the protocol type. */

                /* Check the protocol field. If the protocol is VLAN or not?  */
                if (encap_prortocol_type == __constant_htons(ETH_P_8021Q))
                {
                    if (ptr_pkt_desc->u.ipv4_desc.ipv4HdrRawLen + sizeof(struct vlan_hdr) <= TI_PP_IPV4_HEADER_RAW_SIZE_MAX)
                    {
                        /* Get the VLAN header. */
                        struct vlan_hdr* ptr_vlanheader_gre = (struct vlan_hdr *)((Uint8*)encap_ptr_ethhdr + sizeof(struct ethhdr));

                        /* Add the VLAN header to the template */
                        memcpy(ptr_pkt_desc->u.ipv4_desc.ipv4HdrRaw + ptr_pkt_desc->u.ipv4_desc.ipv4HdrRawLen, ptr_vlanheader_gre, sizeof(struct vlan_hdr));
                        ptr_pkt_desc->u.ipv4_desc.ipv4HdrRawLen += sizeof(struct vlan_hdr);
                    }
                    else
                    {
                        /* There is not enough space to save the US GRE Encapsulation Header */
                        err = True;
                    }
                }
            }
            else
            {
                /* There is not enough space to save the US GRE Encapsulation Header */
                err = True;
            }
            /* Restore the origianl values */
            ptr_iphdr->check = savedCheck;
            ptr_iphdr->tot_len = savedTotLen;
            ptr_iphdr->id = savedId;
            if (err)
            {
                return -1;
            }
        }
        else
        {
            return -1;
        }
    }
    /* ARRIS propagate INTEL MR25140 fix END */
    
    /* Yes. IPv4 header was detected. Initialize the various fields. */
    ptr_pkt_desc->packet_type = TI_PP_IPV4_TYPE;

    /* Populate the Destination IP Address. */
    ptr_pkt_desc->u.ipv4_desc.dst_ip = ptr_iphdr->daddr;
    ptr_pkt_desc->u.ipv4_desc.enables  |= TI_PP_SESSION_IPV4_DSTIP_VALID;

    /* Populate the Source IP Address. */
    ptr_pkt_desc->u.ipv4_desc.src_ip = ptr_iphdr->saddr;
    ptr_pkt_desc->u.ipv4_desc.enables  |= TI_PP_SESSION_IPV4_SRCIP_VALID;

    /* Populate the TOS Byte */
    ptr_pkt_desc->u.ipv4_desc.tos = ptr_iphdr->tos;
    ptr_pkt_desc->u.ipv4_desc.enables  |= TI_PP_SESSION_IPV4_TOS_VALID;

    /* Populate the Protocol */
    ptr_pkt_desc->u.ipv4_desc.protocol = ptr_iphdr->protocol;
    ptr_pkt_desc->u.ipv4_desc.enables  |= TI_PP_SESSION_IPV4_PROTOCOL_VALID;

    return 0;
}

/**************************************************************************
 * FUNCTION NAME : ti_hil_scan_ipv6
 **************************************************************************
 * DESCRIPTION   :
 *  Scan IPv6 header till reaching one of our supported protocols
 *
 * RETURNS       :
 *  0 for success
 **************************************************************************/
inline static int ti_hil_scan_ipv6(struct ipv6hdr* ptr_ipv6hdr, Uint8* ipv6HeaderLen, Uint8* nexthdr)
{
    struct ipv6_opt_hdr* hdr = NULL;
    unsigned int hdrlen;
    unsigned int nextOffset;

    *nexthdr = ptr_ipv6hdr->nexthdr;
    *ipv6HeaderLen = sizeof(struct ipv6hdr);

    /* Stop at one of the supported protocols */
    /* Iterate through well-known extenstion headers till we reach a supported protocol */
    while ((*nexthdr != IPPROTO_TCP) && (*nexthdr != IPPROTO_UDP) && (*nexthdr != IPPROTO_IPIP) && (*nexthdr != IPPROTO_GRE))
    {
        /* If this is the last next header */
        if (*nexthdr == NEXTHDR_NONE) 
        {
            return -1;
        }

        /* Encrypted header - cannot parse it, treat as uknown header */
        if (*nexthdr == NEXTHDR_ESP)
        {
            return -1;
        }

        hdr = (struct ipv6_opt_hdr*)((unsigned char *)ptr_ipv6hdr + *ipv6HeaderLen);

        if (*nexthdr == NEXTHDR_FRAGMENT)
        {
            hdrlen = 8;
        }
        else if (*nexthdr == NEXTHDR_AUTH)
        {
            hdrlen = (hdr->hdrlen + 2) << 2;
        }
        else
        {
            hdrlen = ipv6_optlen(hdr);
        }

        nextOffset = (unsigned int)*ipv6HeaderLen + hdrlen;

        /* preventing a wrap - if the offset exceeds 255 bytes we exit the loop. */
        if (nextOffset > 0xFF)
        {
            return -1;
        }
        else
        {
            *ipv6HeaderLen = (unsigned char)nextOffset;
        }

        *nexthdr = hdr->nexthdr;
    }

    return 0;
}

/**************************************************************************
 * FUNCTION NAME : ti_hil_extract_ipv6
 **************************************************************************
 * DESCRIPTION   :
 *  extracts HIL relevant fields from the IPv6 header
 *
 * RETURNS       :
 *  0 for success
 **************************************************************************/
/* ARRIS propagate INTEL MR25140 fix */
inline static int ti_hil_extract_ipv6(struct ethhdr* ptr_ethhdr, struct ipv6hdr* ptr_ipv6hdr, struct tcphdr** ptr_tcphdr, struct iphdr** ptr_dsLiteIphdr, TI_PP_SESSION_PROPERTY* ptr_ses_property, Bool is_ingress, Uint8* ipv6HeaderLen, Uint8* nexthdr,unsigned char pid_type)
{
    TI_PP_PACKET_DESC* ptr_pkt_desc;

    if (ti_hil_scan_ipv6(ptr_ipv6hdr, ipv6HeaderLen, nexthdr) != 0)
    {
        return -1;
    }

    ptr_pkt_desc = &ptr_ses_property->l3l4_packet;
    
    /* ARRIS propagate INTEL MR25140 fix */
    if (*nexthdr == IPPROTO_IPIP)
    {
        *ptr_dsLiteIphdr = (struct iphdr *)((unsigned char *)ptr_ipv6hdr + *ipv6HeaderLen);
        if (is_ingress == True)
        {
            /* DSLite DS */
            if (((*ptr_dsLiteIphdr)->protocol == IPPROTO_TCP) || ((*ptr_dsLiteIphdr)->protocol == IPPROTO_UDP))
            {
                /* We take only non fragmented packets or first fragment since only these packets holds the layer 4 */
                if ( ( (*ptr_dsLiteIphdr)->frag_off & IP_OFFSET ) == 0 )
                {
                    *ptr_tcphdr = (struct tcphdr *)((unsigned char *)(*ptr_dsLiteIphdr) + (*ptr_dsLiteIphdr)->ihl * 4);
                }
                else
                {
                    return -1;
                }
            }
            else
            {
                /* Currently we support only TCP & UDP */
                return -1;
            }

        }
        else
        {
            /* DSLite US - Layer 4 is neglected */
            *ptr_tcphdr = (struct tcphdr *)((unsigned char *)(*ptr_dsLiteIphdr) + (*ptr_dsLiteIphdr)->ihl * 4);
        }
    }
    else if ((!is_ingress) && (*nexthdr == IPPROTO_GRE))
    {
         
        if (pid_type == TI_PP_PID_TYPE_DOCSIS )
        {
            Bool err = False;

            /* Check if we support this type of US GRE by looking at the GRE protocol type field */
            Uint8* ptr_data = (Uint8*)ptr_ipv6hdr + *ipv6HeaderLen;         /* Go to GRE header */

            if (*(Uint32*)ptr_data != ETH_P_TEB)  /* 0x6558 - Transparent Ethernet Bridging */
            {
                /* unsupported GRE */
                return -1;
            }

            /* Supported US GRE */
            ptr_ses_property->l2_packet.u.eth_desc.enables |= TI_PP_SESSION_L2_GRE_US_VALID;

            /* Setup ipv4HdrRaw parameters - Save encapsulating L2, L3, GRE and encapsulated L2 */
            ptr_pkt_desc->u.ipv6_desc.ipv6HdrRawOffset = (Uint8*)ptr_ipv6hdr - (Uint8*)ptr_ethhdr;
            ptr_pkt_desc->u.ipv6_desc.ipv6HdrTotalSize = *ipv6HeaderLen + 4 + sizeof(struct ethhdr); /* 4 is for the GRE header */
            if (ptr_pkt_desc->u.ipv6_desc.ipv6HdrTotalSize <= TI_PP_IPV6_HEADER_RAW_SIZE_MAX)
            {
                struct ethhdr* encap_ptr_ethhdr;
                unsigned short encap_prortocol_type;

                /* Save encapsulating L2, L3, GRE and encapsulated L2 */
                memcpy(ptr_pkt_desc->u.ipv6_desc.ipv6HdrRaw, ptr_ipv6hdr, ptr_pkt_desc->u.ipv6_desc.ipv6HdrTotalSize);

                /* If the encapsulated packet contains VLAN then add it to the template */
                encap_ptr_ethhdr = (struct ethhdr *)(ptr_data + 4); /* Get the pointer to the encapsulated Ethernet header */
                encap_prortocol_type = encap_ptr_ethhdr->h_proto;   /* Get the protocol type. */

                /* Check the protocol field. If the protocol is VLAN or not?  */
                if (encap_prortocol_type == __constant_htons(ETH_P_8021Q))
                {
                    if (ptr_pkt_desc->u.ipv6_desc.ipv6HdrTotalSize + sizeof(struct vlan_hdr) <= TI_PP_IPV6_HEADER_RAW_SIZE_MAX)
                    {
                        /* Get the VLAN header. */
                        struct vlan_hdr* ptr_vlanheader_gre = (struct vlan_hdr *)((Uint8*)encap_ptr_ethhdr + sizeof(struct ethhdr));

                        /* Add the VLAN header to the template */
                        memcpy(ptr_pkt_desc->u.ipv6_desc.ipv6HdrRaw + ptr_pkt_desc->u.ipv6_desc.ipv6HdrTotalSize, ptr_vlanheader_gre, sizeof(struct vlan_hdr));
                        ptr_pkt_desc->u.ipv6_desc.ipv6HdrTotalSize += sizeof(struct vlan_hdr);
                    }
                    else
                    {
                        /* There is not enough space to save the US GRE Encapsulation Header */
                        err = True;
                    }
                }
            }
            else
            {
                err = True;
            }

            if (err)
            {
                printk("US GRE: not enough space for the header\n");
                return -1;
            }
        }
        else
        {
            return -1;
        }
        
    }
    else
    {
        *ptr_tcphdr = (struct tcphdr *)((unsigned char *)ptr_ipv6hdr + *ipv6HeaderLen);
    }
    /* ARRIS propagate INTEL MR25140 fix END */

    ptr_pkt_desc->packet_type = TI_PP_IPV6_TYPE;

    if (*ptr_dsLiteIphdr != NULL)
    {
        if (is_ingress == True)
        {
            // We would like to have the dsLite_dst_ip only for the ingress, to be able to classify according to it
            ptr_pkt_desc->u.ipv6_desc.dsLite_dst_ip = (*ptr_dsLiteIphdr)->daddr;
            ptr_pkt_desc->u.ipv6_desc.enables  |= TI_PP_SESSION_IPV6_DSLITE_DSTIP_VALID;
        }
        else
        {
            if (*ipv6HeaderLen > TI_PP_IPV6_HEADER_RAW_SIZE_MAX)
            {
                *ipv6HeaderLen = TI_PP_IPV6_HEADER_RAW_SIZE_MAX;
            }

            // We would like to have the whole IPv6 header only for US DsLite
            ptr_pkt_desc->u.ipv6_desc.ipv6HdrTotalSize = *ipv6HeaderLen;
            memcpy(ptr_pkt_desc->u.ipv6_desc.ipv6HdrRaw, ptr_ipv6hdr, *ipv6HeaderLen);
            ((struct ipv6hdr*)ptr_pkt_desc->u.ipv6_desc.ipv6HdrRaw)->payload_len = 0; // reset the payload length value as it is irrelevant for the session creation and the DsLite template
        }
    }

    /* Populate the Destination IP Address. */
    memcpy(ptr_pkt_desc->u.ipv6_desc.dst_ip, ptr_ipv6hdr->daddr.s6_addr32, sizeof(ptr_pkt_desc->u.ipv6_desc.dst_ip));
    ptr_pkt_desc->u.ipv6_desc.enables  |= TI_PP_SESSION_IPV6_DSTIP_VALID;

    /* Populate the Source IP Address. */
    memcpy(ptr_pkt_desc->u.ipv6_desc.src_ip, ptr_ipv6hdr->saddr.s6_addr32, sizeof(ptr_pkt_desc->u.ipv6_desc.src_ip));
    ptr_pkt_desc->u.ipv6_desc.enables  |= TI_PP_SESSION_IPV6_SRCIP_VALID;

    /* Populate the Traffic Class Byte */
    ptr_pkt_desc->u.ipv6_desc.traffic_class = IPV6TCLASS(ptr_ipv6hdr);
    ptr_pkt_desc->u.ipv6_desc.enables  |= TI_PP_SESSION_IPV6_TRCLASS_VALID;

    /* Populate the Next Header */
    ptr_pkt_desc->u.ipv6_desc.next_header = ptr_ipv6hdr->nexthdr;
    ptr_pkt_desc->u.ipv6_desc.enables  |= TI_PP_SESSION_IPV6_NEXTHDR_VALID;

    return 0;
}

/**************************************************************************
 * FUNCTION NAME : ti_hil_TurboDox_Check_And_Enable
 **************************************************************************
 * DESCRIPTION   : This function is called only for TCP protocol.
 * It set the Tdox Enable Flag If set of condition are filled.
 * RETURNS:
 *  0 = OK, -1 = error.
 **************************************************************************/
int ti_hil_TurboDox_Check_And_Enable(struct tcphdr* ptr_tcphdr, TI_PP_SESSION_PROPERTY* ptr_ses_property, struct net_device* dev)
{
    TI_PP_PID   pid;

    ti_ppm_get_pid_info(dev->pid_handle, &pid);
    if (TI_PP_PID_TYPE_DOCSIS != pid.type)
    {
        /* TCP acceleration is done only on traffic toward the RF */
        return 0;
    }

    if (ptr_tcphdr->syn)
    {
        ptr_ses_property->app_specific_data.u.app_desc.enables |= TI_HIL_TCP_SYN;
    }
    else if (!global_ti_hil_db.tdox_disabled) /* if TDOX is enabled */
    {
        /* extract ack number */
        if (ptr_tcphdr->ack)
        {
            char *  ptr_data_head   = (char *)ptr_tcphdr + ptr_tcphdr->doff*4;
            char *  ptr_tcp_options = (char *)ptr_tcphdr + sizeof(struct tcphdr);

            if (ptr_data_head > ptr_tcp_options)
            {
                /********************************************************/
                /* Go over the options and look for timestamp option    */
                /********************************************************/
                while (ptr_tcp_options < ptr_data_head)
                {
                    char tcp_opt_type;
                    char tcp_opt_len;

                    tcp_opt_type = *(ptr_tcp_options);

                    if (tcp_opt_type > TCP_OPTION_CODE_NOP)
                    {
                        /************************************************/
                        /* T.L.V. styled option                         */
                        /************************************************/
                        tcp_opt_len =  *(ptr_tcp_options + 1);

                        if (TCP_OPTION_CODE_TIMESTAMP == tcp_opt_type)
                        {
                            ptr_ses_property->app_specific_data.u.app_desc.enables |= TI_HIL_TDOX_SKIP_TIMESTAMP;
                        }

                        if(tcp_opt_len != 0)
                        {
                            ptr_tcp_options += tcp_opt_len;
                        }
                        else
                        {
                            /* an illegal option length - don't accelerate this session. Probably the server will reset the connection */
                            return -1;
                        }
                    }
                    else
                    {
                        /************************************************/
                        /* Options EOL and NOP are without parameters   */
                        /************************************************/
                        ptr_tcp_options++;
                    }
                }
            }

            ptr_ses_property->app_specific_data.u.app_desc.raw_app_info2 = ptr_tcphdr->ack_seq;
            ptr_ses_property->app_specific_data.u.app_desc.enables |= TI_HIL_TDOX_ENABLED;
        }
        else
        {
            /**************************************************/
            /* TCP with NO ACK flag set - do not accelerate   */
            /**************************************************/
            return -1;
        }
    }
    return 0;
}

/**************************************************************************
 * FUNCTION NAME : ti_hil_extract_l4
 **************************************************************************
 * DESCRIPTION   :
 *  extracts HIL relevant fields from the L4 header
 *
 * RETURNS       :
 *  0 for success
 **************************************************************************/
inline static int ti_hil_extract_l4(struct ethhdr* ptr_ethhdr, struct iphdr* ptr_iphdr, struct ipv6hdr* ptr_ipv6hdr, struct tcphdr* ptr_tcphdr, struct iphdr* ptr_dsLiteIphdr, TI_PP_SESSION_PROPERTY* ptr_ses_property, Bool is_ingress, struct net_device* dev)
{
    Uint8 protocol = 0;
    TI_PP_PACKET_DESC* ptr_pkt_desc;

    ptr_pkt_desc = &ptr_ses_property->l3l4_packet;

    if (ptr_ipv6hdr != NULL)
    {
        if((ptr_dsLiteIphdr == NULL) || ((ptr_dsLiteIphdr != NULL) && (is_ingress == True)))
        {
            ptr_pkt_desc->u.ipv6_desc.dst_port = ptr_tcphdr->dest;
            ptr_pkt_desc->u.ipv6_desc.enables |= TI_PP_SESSION_IPV6_DST_PORT_VALID;
            ptr_pkt_desc->u.ipv6_desc.src_port = ptr_tcphdr->source;
            ptr_pkt_desc->u.ipv6_desc.enables |= TI_PP_SESSION_IPV6_SRC_PORT_VALID;
        }

        if (ptr_dsLiteIphdr != NULL)
        {
            protocol = ptr_dsLiteIphdr->protocol;
        }
        else
        {
            protocol = ptr_ipv6hdr->nexthdr;
        }
    }
    else if (ptr_iphdr != NULL)
    {
        ptr_pkt_desc->u.ipv4_desc.dst_port = ptr_tcphdr->dest;
        ptr_pkt_desc->u.ipv4_desc.enables |= TI_PP_SESSION_IPV4_DST_PORT_VALID;
        ptr_pkt_desc->u.ipv4_desc.src_port = ptr_tcphdr->source;
        ptr_pkt_desc->u.ipv4_desc.enables |= TI_PP_SESSION_IPV4_SRC_PORT_VALID;

        protocol = ptr_iphdr->protocol;
    }

    if ((protocol == IPPROTO_TCP) && !is_ingress)
    {
        if( ti_hil_TurboDox_Check_And_Enable(ptr_tcphdr, ptr_ses_property, dev) < 0)
        {
            return -1;
        }
    }

    return 0;
}

/**************************************************************************
 * FUNCTION NAME : ti_hil_extract_packet_desc
 **************************************************************************
 * DESCRIPTION   :
 *  The function is called to extract the various Layer2, Layer3 and
 *  Layer4 fields and populate the packet description structure.
 *
 * RETURNS       :
 *  0   -   Success
 *  <0  -   Error
 **************************************************************************/
/* ARRIS propagate INTEL MR25140 fix */
static int ti_hil_extract_packet_desc(char* ptr_data, struct net_device* dev, TI_PP_SESSION_PROPERTY* ptr_ses_property, Bool is_ingress)
{
    struct ethhdr*      ptr_ethhdr = NULL;
    struct iphdr*       ptr_iphdr = NULL;
    struct iphdr*       ptr_dsLiteIphdr = NULL;
    struct tcphdr*      ptr_tcphdr = NULL;
    TI_PP_PACKET_DESC*  ptr_pkt_desc;
    TI_PP_PID           pid;
    Bool                foundGreDs = False;

    /* IPv6 params */
    struct ipv6hdr*     ptr_ipv6hdr = NULL;
    Uint8               ipv6HeaderLen;
    Uint8               nexthdr;
    

    /* Check if the device is a PID or not? */
    if (dev->pid_handle != -1)
    {
        /* Valid PID Handle: Packet has been received at the lowest level; at this point in time we can extract all the L2/L3/L4 information from the packet */

        if( !ptr_data )
        {
            /* Without this data we cannot continue */
            return -1;
        }

        if (ti_hil_get_l2_l3_pointers(ptr_data, &ptr_ethhdr, &ptr_iphdr, &ptr_ipv6hdr, ptr_ses_property) != 0)
        {
            return -1;
        }


        ti_ppm_get_pid_info(dev->pid_handle, &pid);
   
        /* Check for DS GRE - in such case we need to extract the encapsulated packet fields */
        if ( (is_ingress)&& (TI_PP_PID_TYPE_DOCSIS == pid.type) )
        {
            if (ptr_ipv6hdr != NULL)
            {
                if (ti_hil_scan_ipv6(ptr_ipv6hdr, &ipv6HeaderLen, &nexthdr) != 0)
                {
                    return -1;
                }
                if (nexthdr == IPPROTO_GRE)
                {
                    foundGreDs = True;
                    ptr_data = (Uint8*)ptr_ipv6hdr + ipv6HeaderLen;         /* Go to GRE header */
                }
            }
            else if (ptr_iphdr != NULL)
            {
                if (ptr_iphdr->protocol == IPPROTO_GRE)
                {
                    foundGreDs = True;
                    ptr_data = (Uint8*)ptr_iphdr + (ptr_iphdr->ihl * 4);    /* Go to GRE header */
                }
            }
            if (foundGreDs)
            {
                if (*(Uint32*)ptr_data != ETH_P_TEB)  /* 0x6558 - Transparent Ethernet Bridging */
                {
                    /* unsupported GRE */
                    return -1;
                }

                /* Supported DS GRE */
                ptr_ses_property->l2_packet.u.eth_desc.enables |= TI_PP_SESSION_L2_GRE_DS_VALID;

                ptr_data += 4; /* Skip GRE header */
                if (ti_hil_get_l2_l3_pointers(ptr_data, &ptr_ethhdr, &ptr_iphdr, &ptr_ipv6hdr, ptr_ses_property) != 0)
                {
                    return -1;
                }
            }
        }

        /* At this stage all the headers have been located and are pointing at the correct locations. Time to start populating the Packet Properties */

        /* Extract Layer2 information */
        if (ti_hil_extract_l2(ptr_ethhdr, ptr_ses_property) != 0)
        {
            return -1;
        }

        /* Extract Layer3 information */
        ptr_pkt_desc = &ptr_ses_property->l3l4_packet;

        if (ptr_iphdr != NULL)
        {
            if (ti_hil_extract_ipv4(ptr_ethhdr, ptr_iphdr, &ptr_tcphdr, ptr_ses_property, is_ingress,pid.type) != 0)
            {
                return -1;
            }
        }
        else if (ptr_ipv6hdr != NULL)
        {
            if (ti_hil_extract_ipv6(ptr_ethhdr, ptr_ipv6hdr, &ptr_tcphdr, &ptr_dsLiteIphdr, ptr_ses_property, is_ingress, &ipv6HeaderLen, &nexthdr,pid.type) != 0)
            {
                return -1;
            }
        }
        else
        {
            /* No IPv4 or IPv6 header was present. Reset all the fields in the packet descriptor */
            memset ((void *)ptr_pkt_desc, 0, sizeof(TI_PP_PACKET_DESC));
        }

        /* Extract Layer4 information */
        if ((ptr_tcphdr != NULL) && !foundGreDs)
        {
            if (ti_hil_extract_l4(ptr_ethhdr, ptr_iphdr, ptr_ipv6hdr, ptr_tcphdr, ptr_dsLiteIphdr, ptr_ses_property, is_ingress, dev) != 0)
            {
                return -1;
            }
        }
    }

    /* Check if the device is a VPID handle or not? */
    if (dev->vpid_handle != -1)
    {
        /* The only missing information not available at the PID layer is the VPID handle on which the packet was actually received/transmitted */
        ptr_ses_property->vpid_handle = dev->vpid_handle;
    }

    /* All the fields have been extracted. */
    return 0;
}
/* ARRIS propagate INTEL MR25140 fix END */

/**************************************************************************
 * FUNCTION NAME : ti_hil_ingress_hook
 **************************************************************************
 * DESCRIPTION   :
 *  The function is registered as the device specific protocol handler for
 *  all networking devices which exist in the system which have a valid
 *  VPID. The function extracts the information pertinent to creation of
 *  the session interface and stores it in the SKB.
 *
 * RETURNS:
 *  Always returns 0.
 **************************************************************************/
int ti_hil_ingress_hook(struct sk_buff* skb)
{

    if (global_ti_hil_db.hil_disabled || (ti_pp_get_status() != ACTIVE))
    {
        return 0;
    }

    /* Extract all the fields from the packet and populate the Ingress Packet Descriptor. */
    if (ti_hil_extract_packet_desc((char *)skb_mac_header(skb), skb->dev, &skb->pp_packet_info.ti_session.ingress, True) < 0)
    {
        return 0;
    }

    global_ti_hil_db.num_ingress_pkts++;

    /* Packet has passed through the Ingress Hooks. */
    skb->pp_packet_info.flags |= TI_HIL_PACKET_FLAG_PP_SESSION_INGRESS_RECORDED;
    skb->pp_packet_info.ti_session.priority = skb->ti_meta_info & 0x7;
    skb->pp_packet_info.ti_session.cluster = 0;

    return 0;
}

/**************************************************************************
 * FUNCTION NAME : ti_hil_session_intelligence
 **************************************************************************
 * DESCRIPTION   :
 *  The function is the host intelligence layer which decides if the Session
 *  is worthy of accleration or not?
 *
 * RETURNS:
 *  0   -  Session is NOT accelerated
 *  1   -  Session is accelerated
 **************************************************************************/
static int ti_hil_session_intelligence (TI_PP_SESSION* ptr_session)
{
    /* Acclerate only IPv4 or IPv6 traffic. */
    if ((ptr_session->ingress.l3l4_packet.packet_type & (TI_PP_IPV4_TYPE | TI_PP_IPV6_TYPE)) == 0)
        return 0;

    /* Dont accelerate MAC Broadcast Packets.
     *  - Check the Ingress Properties and ensure that the DST MAC Valid Bit is set.
     *  - Destination MAC Address is not a Broadcast */
    if ((ptr_session->ingress.l2_packet.u.eth_desc.enables & TI_PP_SESSION_L2_DSTMAC_VALID) &&
        (ptr_session->ingress.l2_packet.u.eth_desc.dstmac[0] == 0xFF))
        return 0;

    /* ARRIS CHANGE, merge from Intel to support 6RD (protocol 41) in PP, 
       ipv6rd_pp_support added to make the feature configurable, because there will be 2 problematic effects,
       1) for sessions with protocol 41 - the session will be created based on L2 & L3 only, 
          without L4, meaning - packets that will arrive to the PP and will match the L2 & L3 
          addresses - will use this session, without any differentiation between different details of L4.
       2) Need to make sure that sessions of UDP or TCP will not have same parameters of L2&L3 
          as the session of the protocol 41, because for the 41 protocol session - L4 will not be 
          examined for match at all. */
    /* Accelerate only TCP and UDP, and protocols 41. */
    /* IPv4 */
    if ((ptr_session->ingress.l3l4_packet.packet_type == TI_PP_IPV4_TYPE) &&
        (ptr_session->ingress.l3l4_packet.u.ipv4_desc.protocol != IPPROTO_UDP) &&
        (ptr_session->ingress.l3l4_packet.u.ipv4_desc.protocol != IPPROTO_TCP) &&
        !(global_ti_hil_db.ipv6rd_pp_support && ptr_session->ingress.l3l4_packet.u.ipv4_desc.protocol == IPPROTO_IPV6))
    {
        return 0;
    }
    #if 0
    /* Accelerate only TCP and UDP. */
    /* IPv4 */
    if ((ptr_session->ingress.l3l4_packet.packet_type == TI_PP_IPV4_TYPE) &&
        (ptr_session->ingress.l3l4_packet.u.ipv4_desc.protocol != IPPROTO_UDP) &&
        (ptr_session->ingress.l3l4_packet.u.ipv4_desc.protocol != IPPROTO_TCP))
        return 0;
    #endif
    // END ARRIS CHANGE

    /* IPv6 */
    else if ((ptr_session->ingress.l3l4_packet.packet_type == TI_PP_IPV6_TYPE) &&
        (ptr_session->ingress.l3l4_packet.u.ipv6_desc.next_header != IPPROTO_UDP) &&
        (ptr_session->ingress.l3l4_packet.u.ipv6_desc.next_header != IPPROTO_TCP) &&
        (ptr_session->ingress.l3l4_packet.u.ipv6_desc.next_header != IPPROTO_IPIP))
        return 0;

    /* Accelerate the session. */
    return 1;
}


/**************************************************************************
 * FUNCTION NAME : ti_hil_null_hook
 **************************************************************************
 * DESCRIPTION   :
 *  This function creates session to terminating VPID.
 *  All the packets like ingress one will be routed to it
 *  and dropped in the future.
 *  The function is very similar to ti_hil_egress_hook.
 * RETURNS       :
 *  Always returns 0.
 **************************************************************************/
int ti_hil_null_hook(struct sk_buff* skb, int null_vpid)
{
    TI_PP_SESSION*      ptr_session;
    int                 session_handle;
    struct net_device*    input_dev;
    int isNew;

    if (global_ti_hil_db.hil_disabled || (ti_pp_get_status() != ACTIVE))
    {
        return 0;
    }

    /* These checks have the following purpose:-
     *  a) If the Packet has not HIT the ingress hook there is no point in creating the session
     *     since the packet is locally generated and these sessions cannot be acclerated.
     *  b) The Host Intelligence layers have decided not to "acclerate" this session. */
    if ((skb->pp_packet_info.flags & TI_HIL_PACKET_FLAG_PP_SESSION_INGRESS_RECORDED) == 0)
    {
        global_ti_hil_db.num_other_pkts++;
        return 0;
    }

    if  (skb->pp_packet_info.flags & TI_HIL_PACKET_FLAG_PP_SESSION_BYPASS)
    {
        global_ti_hil_db.num_bypassed_pkts++;
        return 0;
    }

    global_ti_hil_db.num_null_drop_pkts++;

    /* Get the pointer to the session information block. */
    ptr_session = &skb->pp_packet_info.ti_session;

    /* There can be only one egress record at this stage. */
    ptr_session->num_egress = 1;

#ifdef CONFIG_TI_META_DATA
    /* DOCSIS DSID is not supported yet... */
    ptr_session->ingress.app_specific_data.u.app_desc.enables = 0;
 /* ptr_session->ingress.app_specific_data.u.app_desc.u.raw_app_info1 = -1; */
#endif
    input_dev = dev_get_by_index(&init_net,skb->skb_iif);
    if(input_dev == NULL)
    {
         return 0;
    }
    /* Extract all the fields from the packet and populate the Egress Packet Descriptor. */
    if (ti_hil_extract_packet_desc ((char *)skb_mac_header(skb), input_dev, &ptr_session->egress[0], False) < 0)
    {
        dev_put(input_dev);
        return 0;
    }

    /* We can only proceed to the next step if all the information has been extracted. This implies
     * that the Egress Hook has hit the Egress Hook on both the PID and VPID. */
    if (input_dev->pid_handle == -1)
    {
        dev_put(input_dev);
        return 0;
    }
    dev_put(input_dev);
    /* Override the VPID */
    ptr_session->egress[0].vpid_handle = null_vpid;

    /* Check if the session is ROUTABLE or not? One way of doing this is to compare the L2 destination
     * MAC Address at the Ingress and Egress and if they are not the same it is safe to assume that
     * the session was routed. There are other system wide optimizations that could be done here. For
     * example if the box is a layer2 bridge then this check is not required. Similarly if the box
     * operates only in ROUTED mode then the value can always be set. The check here is the most fail
     * proof as it handles conditions where both Bridging and Routing can coexist. */
    if (ptr_session->ingress.l2_packet.packet_type & TI_PP_ETH_TYPE)
    {
        /* OK. Ingress had recorded Layer2 properties. Does the Egress have the same */
        if (ptr_session->egress[0].l2_packet.packet_type & TI_PP_ETH_TYPE)
        {
            /* OK. Egress had also got recorded Layer2 properties. So compare the same. */
            if (memcmp ((void *)&ptr_session->ingress.l2_packet.u.eth_desc.dstmac,
                        (void *)&ptr_session->egress[0].l2_packet.u.eth_desc.dstmac, 6) != 0)
            {
                /* The destination MAC address are not the same; most definately a routed session. */
                ptr_session->is_routable_session = 1;
            }
            else
            {
                /* The destination MAC address are the same. This is bridged for sure. */
                ptr_session->is_routable_session = 0;
            }
        }
        else
        {
            /* No Egress information at layer2. We cannot take a decision now as all the information is not present.
             * Setting the flag is not correct because the framing in the PDSP will not work. So default to bridged */
            ptr_session->is_routable_session = 0;
        }
    }
    else
    {
        /* No Ingress information at layer2. We cannot take a decision now as all the information is not present.
         * Setting the flag is not correct because the framing in the PDSP will not work. So default to bridged */
        ptr_session->is_routable_session = 0;
    }

    /* Once all the fields have been extracted. Check if the session can be created or not? */
    if (ti_hil_session_intelligence (ptr_session) == 0)
        return 0;

    /* All sessions created here have a standard session timeout. */
    ptr_session->session_timeout = ti_session_timeout_sec * 1000000;
    ptr_session->priority = 0;
    ptr_session->cluster  = 0;

    /* Create the session in the Packet Processor. */
    session_handle = ti_ppm_create_session (ptr_session, (void*)skb, 0, 0, &isNew);
    if (session_handle < 0)
    {
        /* Session Creation Failed. Increment the error counter. */
        global_ti_hil_db.num_error++;
        return 0;
    }

#ifdef CONFIG_TI_HIL_DEBUG
    if (0 == global_ti_hil_db.dbg_disabled)
    {
        printk("\n ---- Session [%3d] has been created ---- Discarding ----\n", session_handle);
        ti_hil_intrusive_display_ipv4(&ptr_session->egress[0].l3l4_packet.u.ipv4_desc);
        printk(" ----------------------------------------\n");
    }
#endif

    return 0;
}


/**************************************************************************
 * FUNCTION NAME : ti_hil_egress_hook
 **************************************************************************
 * DESCRIPTION   :
 *  The function is the egress SRM hook that is called when a packet is to
 *  be transmitted on a session interfaces. The function extracts the
 *  information pertinent to creation of the session interface. It then
 *  checks if this packet was "routed/bridged". If yes then control is
 *  passed to the Plugin Logic to determine creation of the session.
 *
 * RETURNS       :
 *  Always returns 0.
 **************************************************************************/
int ti_hil_egress_hook(struct sk_buff* skb)
{
    TI_PP_SESSION*      ptr_session;
    int                 i;

    /* Get the pointer to the session information block. */
    ptr_session = &skb->pp_packet_info.ti_session;

    if (TI_PPM_EGRESS_QUEUE_INVALID == skb->pp_packet_info.egress_queue)
    {
        /************************************************************************/
        /*  Scale down the priority of the egress queue                         */
        /************************************************************************/
        if ((skb->dev->vpid_block.qos_clusters_count) && (0 == global_ti_hil_db.qos_disabled))
        {
            if (NULL != skb->dev->qos_select_hook)
            {
                skb->pp_packet_info.egress_queue = skb->dev->qos_select_hook(skb);
            }
        }
        else 
        {
            ptr_session->cluster  = 0xFF;
            ptr_session->priority = 0;
        }
        /************************************************************************/
    }

    if (global_ti_hil_db.hil_disabled || (ti_pp_get_status() != ACTIVE))
    {
        return 0;
    }

    /* These checks have the following purpose:-
     *  a) If the Packet has not HIT the ingress hook there is no point in creating the session
     *     since the packet is locally generated and these sessions cannot be acclerated.
     *  b) The Host Intelligence layers have decided not to "acclerate" this session. */
    if ((skb->pp_packet_info.flags & TI_HIL_PACKET_FLAG_PP_SESSION_INGRESS_RECORDED) == 0)
    {
        global_ti_hil_db.num_other_pkts++;
        return 0;
    }

    if  (skb->pp_packet_info.flags & TI_HIL_PACKET_FLAG_PP_SESSION_BYPASS)
    {
        global_ti_hil_db.num_bypassed_pkts++;
        return 0;
    }

    global_ti_hil_db.num_egress_pkts++;

    /* There can be only one egress record at this stage. */
    ptr_session->num_egress = 1;

#ifdef CONFIG_TI_META_DATA
    for (i=0; i<ptr_session->num_egress; i++)
    {
        ptr_session->egress[i].app_specific_data.u.app_desc.u.raw_app_info1 = skb->ti_meta_info;
        ptr_session->egress[i].app_specific_data.u.app_desc.enables = TI_PP_SESSION_APP_RAW_INFO1_VALID;
    }

    /* DOCSIS DSID is not supported yet... */
    ptr_session->ingress.app_specific_data.u.app_desc.enables = 0;
 /* ptr_session->ingress.app_specific_data.u.app_desc.u.raw_app_info1 = -1; */
#endif

    /* Extract all the fields from the packet and populate the Egress Packet Descriptor. */
    if (ti_hil_extract_packet_desc ((char *)skb->data, skb->dev, &ptr_session->egress[0], False) < 0)
    {
        return 0;
    }

    /* We can only proceed to the next step if all the information has been extracted. This implies
     * that the Egress Hook has hit the Egress Hook on both the PID and VPID. */
    if (skb->dev->pid_handle == -1)
        return 0;

    /* Check if the session is ROUTABLE or not? One way of doing this is to compare the L2 destination
     * MAC Address at the Ingress and Egress and if they are not the same it is safe to assume that
     * the session was routed. There are other system wide optimizations that could be done here. For
     * example if the box is a layer2 bridge then this check is not required. Similarly if the box
     * operates only in ROUTED mode then the value can always be set. The check here is the most fail
     * proof as it handles conditions where both Bridging and Routing can coexist. */
    if (ptr_session->ingress.l2_packet.packet_type & TI_PP_ETH_TYPE)
    {
        /* OK. Ingress had recorded Layer2 properties. Does the Egress have the same */
        if (ptr_session->egress[0].l2_packet.packet_type & TI_PP_ETH_TYPE)
        {
            /* OK. Egress had also got recorded Layer2 properties. So compare the same. */
            if (memcmp ((void *)&ptr_session->ingress.l2_packet.u.eth_desc.dstmac,
                        (void *)&ptr_session->egress[0].l2_packet.u.eth_desc.dstmac, 6) != 0)
            {
                /* The destination MAC address are not the same; most definately a routed session. */
                ptr_session->is_routable_session = 1;
            }
            else
            {
                /* The destination MAC address are the same. This is bridged for sure. */
                ptr_session->is_routable_session = 0;
            }
        }
        else
        {
            /* No Egress information at layer2. We cannot take a decision now as all the information is not present.
             * Setting the flag is not correct because the framing in the PDSP will not work. So default to bridged */
            ptr_session->is_routable_session = 0;
        }
    }
    else
    {
        /* No Ingress information at layer2. We cannot take a decision now as all the information is not present.
         * Setting the flag is not correct because the framing in the PDSP will not work. So default to bridged */
        ptr_session->is_routable_session = 0;
    }

    /* Once all the fields have been extracted. Check if the session can be created or not? */
    if (ti_hil_session_intelligence (ptr_session) == 0)
        return 0;

    /* All sessions created here have a standard session timeout. */
    ptr_session->session_timeout = ti_session_timeout_sec * 1000000;

    /************************************************************/
    /*                  CRITICAL SECTION START                  */
    /************************************************************/
    {
        unsigned int    lockKey;
        int             session_handle;
        int             tdox_ID = -1;

        int isNew;

        PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);

        /* Populate the ack suppression - Tdox check and enable for DSLite */
        if (IsTdoxCandidate(ptr_session))
        {
            if (ptr_session->egress[0].app_specific_data.u.app_desc.enables & TI_HIL_TCP_SYN)
            {
                /* In case it's a SYN packet - do not open the session yet, but make it expedited in DOCSIS Upstream */
                skb->ti_meta_info |= DOCSIS_FW_PACKET_API_TCP_HIGH_PRIORITY;

                // ARRIS ADD : reenable interrups before exiting
                PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);    
                // END ARRIS

                return 0;
            }
            else
            {
              /************************************************************************/
              /* The following code handles TURBO DOX feature case.                   */
              /************************************************************************/
                if (-1 == (session_handle = ti_ppm_check_session(ptr_session)))
                {
                    if (ptr_session->egress[0].app_specific_data.u.app_desc.enables & TI_HIL_TDOX_ENABLED)
                    {
                        /* This is a first packet - make it expedited in DOCSIS Upstream in order to get response quickly */
                        skb->ti_meta_info |= DOCSIS_FW_PACKET_API_TCP_HIGH_PRIORITY;
                        tdox_ID = ti_hil_tdox_alloc_session();
                        ptr_session->egress[0].app_specific_data.u.app_desc.u.raw_app_info1_b3 = tdox_ID;
                        ptr_session->priority = 0;
                    }
                }
                else
                {
                    tdox_db_entry_t *tdox_Entry_p = ti_hil_tdox_get_session_entry(session_handle);

                    if (tdox_Entry_p)
                    {
                        skb->ti_meta_info |= DOCSIS_FW_PACKET_API_TCP_HIGH_PRIORITY;
                        ptr_session->priority = 0;
                    }
                }
              /************************************************************************/
            }
        }

#ifdef CONFIG_TI_PACKET_PROCESSOR_EXT_SWITCH
       /************************************************************************/
        /*  The following is an application specific code sample that need      */
        /*  to be changed according to the external switch type and chosen      */
        /*  priority maintenance technique.                                     */
        /*  This case shows the sample of priority insertion using VLAN tagging */
        /*                                                                      */
        /*  Add VLAN tag with corresponding priority to the outgoing packet     */
        /*  Note that priority appears in 0(high) to 3(low) values range        */
        /************************************************************************/

        if (0 == strcmp(skb->dev->name, "eth0"))
        {
            if (ptr_session->egress[0].l2_packet.u.eth_desc.enables & TI_PP_SESSION_L2_VLAN_VALID)
            {
                PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);

                /*  The double encapsulation of VLAN tag is NOT currently supported. */
                return 0;
            }

            /* First, disable existing L2 header (assumes it was found earlier) */
            ptr_session->egress[0].l2_packet.packet_type        = TI_PP_L2_RAW_TYPE;
            ptr_session->egress[0].l2_packet.u.eth_desc.enables = 0;

            /* Second, enable raw L2 header to be inserted */
            ptr_session->egress[0].l2_raw_packet.packet_type    = TI_PP_L2_RAW_TYPE;
            ptr_session->egress[0].l2_raw_packet.u.l2raw_desc.enables = TI_PP_SESSION_L2_RAW_VALID;

            /* Third, build the L2 header that includes VLAN tag */
            {
                struct ethhdr*          ptr_ethhdr   = (struct ethhdr *)skb->data;
                struct vlan_ethhdr *    ptr_l2header = (struct vlan_ethhdr *)&ptr_session->egress[0].l2_raw_packet.u.l2raw_desc.tx_buff[0];

                memcpy( ptr_l2header->h_dest    ,ptr_ethhdr->h_dest,    ETH_ALEN );
                memcpy( ptr_l2header->h_source  ,ptr_ethhdr->h_source,  ETH_ALEN );
                ptr_l2header->h_vlan_proto = __constant_htons(ETH_P_8021Q);

                /* Insert the VLAN ID 1 together with the priority that came from DOCSIS DS (ti_meta_info) */
                // ptr_l2header->h_vlan_TCI   = ( VLAN_PRIO_MASK & (((u16)(skb->ti_meta_info)) << VLAN_PRIO_SHIFT) ) | 0x01 ;

                // ARRIS MODIFY to set VLAN tag.
                // Left shift priority bits 14 places to make room for 4 QOS queues in
                // the downstream direction.
#if (CONFIG_MACH_PUMA6)
                ptr_l2header->h_vlan_TCI   = ( VLAN_PRIO_MASK & (((u16)(skb->ti_meta_info)) << VLAN_PRIO_SHIFT) ) | 0x01 ;
#else
                // Puma 5 with broadcom switch
                ptr_l2header->h_vlan_TCI = ((((unsigned short)(3 - (ptr_session->priority & 0x3)))<< 14)|0x01);
#endif
                // ARRIS MODIFY end

                ptr_l2header->h_vlan_encapsulated_proto = ptr_ethhdr->h_proto;
            }

            /* Finally update the length of newly built L2 header */
            ptr_session->egress[0].l2_raw_packet.u.l2raw_desc.tx_buff_len = VLAN_ETH_HLEN;
        }
        /************************************************************************/

#endif  // CONFIG_TI_PACKET_PROCESSOR_EXT_SWITCH

        /* Create the session in the Packet Processor. */
        {
            TI_PP_VPID  vpid;
            TI_PP_PID   pid;
            unsigned char isVoice = false;

            ti_ppm_get_vpid_info (ptr_session->ingress.vpid_handle, &vpid);
            ti_ppm_get_pid_info  (vpid.parent_pid_handle, &pid);
            if (pid.pid_handle >= PP_C55_PID_BASE && pid.pid_handle < PP_C55_PID_BASE + PP_C55_PID_COUNT)
            {
                isVoice = true;
            }
            else
            {
                ti_ppm_get_vpid_info (ptr_session->egress[0].vpid_handle, &vpid);
                ti_ppm_get_pid_info  (vpid.parent_pid_handle, &pid);
                if (pid.pid_handle >= PP_C55_PID_BASE && pid.pid_handle < PP_C55_PID_BASE + PP_C55_PID_COUNT)
                {
                    isVoice = true;
                }
            }

            session_handle = ti_ppm_create_session(ptr_session, (void*)skb, 0, isVoice, &isNew);
        }

        if (global_ti_hil_db.tdoxDbg && isNew)
        {
            printk("TDOX-DBG CreateSession: jiffies=%ld, ses=%d, vpid=%d, protocol=%d, src=%d, dst=%d\n", jiffies, session_handle, ptr_session->egress[0].vpid_handle, ptr_session->egress[0].l3l4_packet.u.ipv4_desc.protocol, ptr_session->egress[0].l3l4_packet.u.ipv4_desc.src_port, ptr_session->egress[0].l3l4_packet.u.ipv4_desc.dst_port);
        }

        if (session_handle < 0)
        {
            if (tdox_ID != -1)
            {
                if (global_ti_hil_db.tdoxDbg)
                {
                    printk(KERN_ERR"TDOX-DBG TdoxFree (CreateSession failed): jiffies=%ld, tdox=%d\n", jiffies, tdox_ID);//ARRIS MOD
                }
                ti_hil_tdox_free_session( -1, tdox_ID );
            }

            /* Session Creation Failed. Increment the error counter. */
            global_ti_hil_db.num_error++;
            PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);

            return 0;
        }

#ifdef CONFIG_TI_HIL_DEBUG
        if (0 == global_ti_hil_db.dbg_disabled)
        {
            printk("\n ---- Session [%3d] has been created ----\n", session_handle);
            ti_hil_intrusive_display_ipv4(&ptr_session->egress[0].l3l4_packet.u.ipv4_desc);
            printk(" ----------------------------------------\n");
        }
#endif

        if (tdox_ID != -1)
        {
            if (global_ti_hil_db.tdoxDbg)
            {
                printk("TDOX-DBG AssociateTdox (Start): jiffies=%ld, ses=%d, tdox=%d\n", jiffies, session_handle, tdox_ID);
            }
            ti_hil_tdox_associate_session(tdox_ID, session_handle);
        }

        PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
    }
    /************************************************************/
    /*                  CRITICAL SECTION END                    */
    /************************************************************/

    return 0;
}

int ti_hil_set_mta_mac_address(unsigned char *mtaAddress)
{
    return ti_ppm_set_mta_mac_address(mtaAddress);
}

#ifdef CONFIG_INTEL_PP_TUNNEL_SUPPORT
/**************************************************************************
 * FUNCTION NAME : ti_hil_set_cm_mac_address
 **************************************************************************
 * DESCRIPTION   :
 *  Sets the CM MAC address in PP
 *
 * RETURNS       :
 *  0 = OK, other values = error
 **************************************************************************/
int ti_hil_set_cm_mac_address(unsigned char *cmAddress)
{
    return ti_ppm_set_cm_mac_address(cmAddress);
}

/**************************************************************************
 * FUNCTION NAME : ti_hil_set_tunnel_mode
 **************************************************************************
 * DESCRIPTION   :
 *  Sets tunnelMode in PP
 *
 * RETURNS       :
 *  0 = OK, other values = error
 **************************************************************************/
int ti_hil_set_tunnel_mode(unsigned char tunnelMode)
{
    global_ti_hil_db.tunnelMode = tunnelMode;
    if (tunnelMode == 0)
    {
        ti_hil_delete_tunnel();
    }
    return ti_ppm_set_tunnel_mode(tunnelMode);
}

/**************************************************************************
 * FUNCTION NAME : ti_hil_create_tunnel
 **************************************************************************
 * DESCRIPTION   :
 *  Creates a tunnel in PP
 *  In current implementation we have only one logical tunnel which is divided
 *  to two actual tunnels - one for US and one for DS. We will create both tunnels
 *  and then set tunnel mode (to be sure that PP will work with the configured tunnels)
 *
 * RETURNS       :
 *  0 = OK, other values = error
 **************************************************************************/
int ti_hil_create_tunnel(char *tunnelHeader, unsigned char tunnelHeaderLen, unsigned char l2L3HeaderLen,
                         TUNNEL_TYPE_E tunnelType, unsigned char udpMode)
{
    TI_PP_SESSION       *ptr_session;
    TI_PP_PACKET_DESC*  ptr_pkt_desc;
    TI_PP_VPID vpid;
    TI_PP_PID   pid;
    char cniVpid;
    char ethVpid;
    unsigned char tempVpid;
    struct sk_buff *skb;
    struct net_device *cniDev;
    struct net_device *ethDev;
    int tunnelConfig =
        (tunnelHeaderLen | (l2L3HeaderLen << 8) | (tunnelType << 16) | (udpMode << 24));
    char ipVersion;
    int isNew;

    if (global_ti_hil_db.tunnelMode == 0)
    {
        printk(KERN_ERR"ti_hil_create_tunnel: Cannot create tunnels while not in tunnel mode\n");//ARRIS MOD
        return -1;
    }

    /* Check if need to set IPv6 flag */
    if (tunnelHeader[12] == 0x86 && tunnelHeader[13] == 0xDD)
    {
        tunnelConfig |= (1 << 25);
    }

    printk("ti_hil_create_tunnel: tunnelHeaderLen=%d, l2L3HeaderLen=%d, tunnelType=%d, udpMode=%d\n",
            tunnelHeaderLen, l2L3HeaderLen, tunnelType, udpMode);

    cniVpid = -1;
    ethVpid = -1;
    for (tempVpid = 0; tempVpid < TI_PP_MAX_VPID; tempVpid++)
    {
        ti_ppm_get_vpid_info (tempVpid, &vpid);
        ti_ppm_get_pid_info  (vpid.parent_pid_handle, &pid);
        if (TI_PP_PID_TYPE_DOCSIS == pid.type)
        {
            cniVpid = tempVpid;
            if (ethVpid != 0xFF)
            {
                break;
            }
        }
        // TBD: when several ethernet VPID will be configured, need to know which is relevant
        if (TI_PP_PID_TYPE_ETHERNET == pid.type)
        {
            ethVpid = tempVpid;
            if (cniVpid != 0xFF)
            {
                break;
            }
        }
    }
    cniDev = __dev_get_by_name(&init_net, "cni0");
    ethDev = __dev_get_by_name(&init_net, "eth0");

    if(!(skb = dev_alloc_skb(2048)))
    {
        printk(KERN_ERR"ti_hil_create_tunnel: Failed to allocate skb. Will not create a tunnel\n");//ARRIS MOD
        return -1;
    }

    ptr_session = &skb->pp_packet_info.ti_session;


    /********************/
    /* WAN-->LAN tunnel */
    /********************/
    memset(&ptr_session->ingress, 0, sizeof(ptr_session->ingress));
    ptr_session->ingress.vpid_handle = cniVpid;
    ptr_session->num_egress = 1;
    ptr_session->egress[0].vpid_handle = ethVpid;

    ptr_session->is_routable_session = 0;
    ptr_session->session_timeout = tunnelConfig;

    ptr_session->egress[0].l2_packet.packet_type = TI_PP_L2_RAW_TYPE;
    ptr_pkt_desc = &ptr_session->egress[0].l2_raw_packet;
    ptr_pkt_desc->packet_type = TI_PP_L2_RAW_TYPE;
    ptr_pkt_desc->u.l2raw_desc.enables = TI_PP_SESSION_L2_RAW_VALID;

    ptr_session->ingress.l3l4_packet.packet_type = TI_PP_IPV4_TYPE;
    ptr_session->egress[0].l3l4_packet.packet_type = TI_PP_IPV4_TYPE;
    // The following two lines are done only for hash maneuver
    ptr_session->ingress.l2_packet.u.ipv4_desc.src_port = DUMMY_FOR_TUNNEL_1;
    ptr_session->ingress.l2_packet.u.ipv4_desc.enables = TI_PP_SESSION_IPV4_SRC_PORT_VALID;

    /************************************************************************/

    if(skb && cniDev)
    {
        skb->skb_iif = cniDev->ifindex;
    }


    skb->dev = ethDev;

    skb->pp_packet_info.ti_match_llc_filter = NULL;
    skb->pp_packet_info.ti_match_inbound_ip_filter = NULL;
    skb->pp_packet_info.ti_match_outbound_ip_filter = NULL;
    skb->pp_packet_info.ti_match_qos_classifier = NULL;
	skb->pp_packet_info.ti_match_dsg_filter = NULL;

    /* Create the WAN-->LAN tunnel in the Packet Processor. */
    gTunnel1Handle = ti_ppm_create_session (ptr_session, (void*)skb, 1, 0, &isNew);
    if (gTunnel1Handle < 0)
    {
        printk(KERN_ERR"ti_hil_create_tunnel: ti_ppm_create_session failed\n");//ARRIS MOD

        /* Session Creation Failed. Increment the error counter. */
        global_ti_hil_db.num_error++;
        dev_kfree_skb_any(skb);
        return 0;
    }

    /********************/
    /* LAN-->WAN tunnel */
    /********************/
    ptr_session->ingress.vpid_handle = ethVpid;
    ptr_session->egress[0].vpid_handle = cniVpid;

#ifdef CONFIG_TI_META_DATA
    // Configured SF=0, PHS=0xFF for US FW
    ptr_session->egress[0].app_specific_data.u.app_desc.u.raw_app_info1 = 0x00FF0000;
    ptr_session->egress[0].app_specific_data.u.app_desc.enables = TI_PP_SESSION_APP_RAW_INFO1_VALID;
#endif

    ptr_session->egress[0].l2_packet.packet_type = TI_PP_L2_RAW_TYPE;
    ptr_pkt_desc = &ptr_session->egress[0].l2_raw_packet;
    ptr_pkt_desc->packet_type = TI_PP_L2_RAW_TYPE;

    memcpy(ptr_pkt_desc->u.l2raw_desc.tx_buff, tunnelHeader, tunnelHeaderLen);
    ptr_pkt_desc->u.l2raw_desc.tx_buff_len = tunnelHeaderLen;
    ptr_pkt_desc->u.l2raw_desc.enables = TI_PP_SESSION_L2_RAW_VALID;

    // The following two lines are done only for hash maneuver
    ptr_session->ingress.l2_packet.u.ipv4_desc.src_port = DUMMY_FOR_TUNNEL_0;
    ptr_session->ingress.l2_packet.u.ipv4_desc.enables = TI_PP_SESSION_IPV4_SRC_PORT_VALID;

    /* Create the LAN-->WAN tunnel in the Packet Processor. */

    skb->skb_iif = ethDev->ifindex;

    skb->dev = cniDev;

    skb->pp_packet_info.ti_match_llc_filter = NULL;
    skb->pp_packet_info.ti_match_inbound_ip_filter = NULL;
    skb->pp_packet_info.ti_match_outbound_ip_filter = NULL;
    skb->pp_packet_info.ti_match_qos_classifier = NULL;
	skb->pp_packet_info.ti_match_dsg_filter = NULL;

    gTunnel0Handle = ti_ppm_create_session (ptr_session, (void*)skb, 1, 0, &isNew);
    if (gTunnel0Handle < 0)
    {
        printk(KERN_ERR"ti_hil_create_tunnel: ti_ppm_create_session failed\n");//ARRIS MOD

        /* Session Creation Failed. Increment the error counter. */
        global_ti_hil_db.num_error++;
        dev_kfree_skb_any(skb);
        return 0;
    }

    dev_kfree_skb_any(skb);
    return 0;
}

/**************************************************************************
 * FUNCTION NAME : ti_hil_delete_tunnel
 **************************************************************************
 * DESCRIPTION   :
 *  Deletes a tunnel from PP
 * In current implementation we have only one logical tunnel which is divided
 * to two actual tunnels - one for US and one for DS. We will delete both tunnels
 *
 * RETURNS       :
 *  0 = OK, other values = error
 **************************************************************************/
int ti_hil_delete_tunnel(void)
{
    printk("ti_hil_delete_tunnel gTunnel0Handle=%d, gTunnel1Handle=%d\n", gTunnel0Handle, gTunnel1Handle);
    if (gTunnel0Handle >= 0)
    {
        ti_ppm_delete_session(gTunnel0Handle, NULL);
        gTunnel0Handle = -1;
    }
    if (gTunnel1Handle >= 0)
    {
        ti_ppm_delete_session(gTunnel1Handle, NULL);
        gTunnel1Handle = -1;
    }

    return 0;
}
#endif


#if 0 /* TODO: Enable once router functionality is available */

/**************************************************************************
 * FUNCTION NAME : ti_hil_device_handler
 **************************************************************************
 * DESCRIPTION   :
 *  Default HIL Device Handler.
 *
 * RETURNS       :
 *  0   -   Success
 *  <0  -   Error
 **************************************************************************/
static int ti_hil_device_handler(unsigned long event_id, void* ptr)
{
    struct net_device *dev = (struct net_device *)ptr;

    /* Dont do anything for Loopback devices; since the interface cannot participate
     * in the packet processor. */
    if (dev->flags & IFF_LOOPBACK)
    {
        /* YES. In that case there is no PID or VPID associated with the device */
        dev->pid_handle = -1;
        dev->vpid_handle= -1;
        return 0;
    }

    /* Processing is based on the Event. */
    switch (event_id)
    {
        case NETDEV_UP:
        {
            /* Network device is going UP. */
            printk ("Device %s is going UP!\n", dev->name);

            /* VPID needs to be created only if either of the following conditions are met:-
             *  a) Device is connected to the bridge.
             *     The bridge runs its internal state machine as ports move from LISTENING,
             *     LEARNING to FORWARDING state. There are events generated from the state
             *     machine which should handle the VPID creation/deletion. Thus this check
             *     is not explicitly seen here.
             *  b) Device is connected to the IP stack i.e has an IP Address. */
            if (ti_hil_is_device_routed(dev) == 1)
            {
                /* Check if the device had a valid VPID handle. */
                if (dev->vpid_handle == -1)
                {
                    /* NO. The device needs to be created as a VPID in the packet processor. */
                    dev->vpid_handle = ti_ppm_create_vpid (&dev->vpid_block);
                    if (dev->vpid_handle < 0)
                    {
                        printk ("Error: Unable to create VPID %d PID %d Device %s\n", dev->vpid_handle, dev->pid_handle, dev->name);
                        return -1;
                    }
                    printk ("Succesfully created VPID %d PID %d Device %s\n",  dev->vpid_handle, dev->pid_handle, dev->name);
                }
            }

            /* Install an Ingress Hook on the networking device. This is done if either
             * of the following conditions are met:-
             *  (a) Device has valid PID Handle.
             *  (b) Device has valid VPID Handle. */
            if ((dev->vpid_handle != -1) || (dev->pid_handle != -1))
            {
                /* Install the Ingress Hook. */
                if (ti_register_protocol_handler (dev, ti_hil_ingress_hook) == 0)
                    printk ("Ingress Hook on PID/VPID: %d/%d Name: %s\n", dev->pid_handle, dev->vpid_handle, dev->name);

                /* Install the Egress Hook. */
                if (ti_register_egress_hook_handler(dev, ti_hil_egress_hook) == 0)
                    printk ("Egress Hook on PID/VPID: %d/%d Name: %s\n", dev->pid_handle, dev->vpid_handle, dev->name);
            }
            break;
        }
        case NETDEV_DOWN:
        {
            /* Network device is going DOWN. */
            printk ("Device %s with VPID: %d is going DOWN!\n", dev->name, dev->vpid_handle);

            /* Check if the device had a valid VPID handle. */
            if (dev->vpid_handle != -1)
            {
                /* YES. The device needs to be removed from the packet processor. */
                if (ti_ppm_delete_vpid (dev->vpid_handle) < 0)
                {
                    printk ("Error: Unable to delete VPID %d PID %d Device %s\n", dev->vpid_handle, dev->pid_handle, dev->name);
                    return -1;
                }
                printk ("Succesfully deleted VPID %d PID %d Device %s\n",  dev->vpid_handle, dev->pid_handle, dev->name);
                dev->vpid_handle = -1;
            }

            /* Uninstall the Ingress Hook; since because if the device is no longer a VPID there is no
             * point to listen for packets on it. */
            ti_deregister_protocol_handler (dev);
            ti_deregister_egress_hook_handler (dev);
            break;
        }
        default:
        {
            /* All other events are ignored. */
            return 0;
        }
    }

    /* Work has been successfully completed! */
    return 0;
}

/**************************************************************************
 * FUNCTION NAME : ti_hil_inet_handler
 **************************************************************************
 * DESCRIPTION   :
 *  Default HIL INET Handler.
 *
 * RETURNS       :
 *  0   -   Success
 *  <0  -   Error
 **************************************************************************/
static int ti_hil_inet_handler(unsigned long event_id, void* ptr)
{
    struct in_ifaddr *ifa = (struct in_ifaddr*)ptr;
    struct in_device *in_dev = ifa->ifa_dev;
    struct net_device *dev = in_dev->dev;

    /* Dont do anything for Loopback devices; since the interface cannot participate
     * in the packet processor. */
    if (dev->flags & IFF_LOOPBACK)
    {
        /* YES. In that case there is no PID or VPID associated with the device */
        dev->pid_handle = -1;
        dev->vpid_handle= -1;
        return 0;
    }

    /* Processing is based on the Event. */
    switch (event_id)
    {
        case NETDEV_UP:
        {
            /* Network device is going UP. */
            printk ("Device %s is going UP!\n", dev->name);

            /* VPID needs to be created only if either of the following conditions are met:-
             *  a) Device is connected to the bridge.
             *     The bridge runs its internal state machine as ports move from LISTENING,
             *     LEARNING to FORWARDING state. There are events generated from the state
             *     machine which should handle the VPID creation/deletion. Thus this check
             *     is not explicitly seen here.
             *  b) Device is connected to the IP stack i.e has an IP Address. */
            if (ti_hil_is_device_routed(dev) == 1)
            {
                /* Check if the device had a valid VPID handle. */
                if (dev->vpid_handle == -1)
                {
                    /* NO. The device needs to be created as a VPID in the packet processor. */
                    dev->vpid_handle = ti_ppm_create_vpid (&dev->vpid_block);
                    if (dev->vpid_handle < 0)
                    {
                        printk ("Error: Unable to create VPID %d PID %d Device %s\n", dev->vpid_handle, dev->pid_handle, dev->name);
                        return -1;
                    }
                    printk ("Succesfully created VPID %d PID %d Device %s\n",  dev->vpid_handle, dev->pid_handle, dev->name);
                }
            }

            /* Install an Ingress Hook on the networking device. This is done if either
             * of the following conditions are met:-
             *  (a) Device has valid PID Handle.
             *  (b) Device has valid VPID Handle. */
            if ((dev->vpid_handle != -1) || (dev->pid_handle != -1))
            {
                /* Install the Ingress Hook. */
                if (ti_register_protocol_handler (dev, ti_hil_ingress_hook) == 0)
                    printk ("Ingress Hook on PID/VPID: %d/%d Name: %s\n", dev->pid_handle, dev->vpid_handle, dev->name);

                /* Install the Egress Hook. */
                if (ti_register_egress_hook_handler(dev, ti_hil_egress_hook) == 0)
                    printk ("Egress Hook on PID/VPID: %d/%d Name: %s\n", dev->pid_handle, dev->vpid_handle, dev->name);
            }
            break;
        }
        case NETDEV_DOWN:
        {
            /* Network device is going DOWN. */
            printk ("Device %s with VPID: %d is going DOWN!\n", dev->name, dev->vpid_handle);

            /* Check if the device had a valid VPID handle. */
            if (dev->vpid_handle != -1)
            {
                /* YES. The device needs to be removed from the packet processor. */
                if (ti_ppm_delete_vpid (dev->vpid_handle) < 0)
                {
                    printk ("Error: Unable to delete VPID %d PID %d Device %s\n", dev->vpid_handle, dev->pid_handle, dev->name);
                    return -1;
                }
                printk ("Succesfully deleted VPID %d PID %d Device %s\n",  dev->vpid_handle, dev->pid_handle, dev->name);
                dev->vpid_handle = -1;
            }

            /* Uninstall the Ingress Hook; since because if the device is no longer a VPID there is no
             * point to listen for packets on it. */
            ti_deregister_protocol_handler (dev);
            ti_deregister_egress_hook_handler (dev);
            break;
        }
        default:
        {
            /* All other events are ignored. */
            return 0;
        }
    }

    /* Work has been successfully completed! */
    return 0;
}

#endif

/**************************************************************************
 * FUNCTION NAME : ti_hil_pp_handler
 **************************************************************************
 * DESCRIPTION   :
 *  Default HIL PP Handler.
 *
 * RETURNS       :
 *  0   -   Success
 *  <0  -   Error
 **************************************************************************/
static int ti_hil_pp_handler(unsigned long event_id, void* ptr)
{
    struct net_device*           dev,*input_dev;
    struct net_bridge_fdb_entry* fdb;
    struct sk_buff*              skb;
#ifdef CONFIG_NETFILTER
    struct nf_conn*             conntrack;
#endif
#ifdef CONFIG_IP_MULTICAST
    struct mfc_cache*            ptr_mfc_cache;
#endif
#ifdef CONFIG_NETFILTER
    struct xt_table*            t;
#endif

    /* Process the events. */
    switch (event_id)
    {
        case TI_DOCSIS_FLTR_DISCARD_PKT:
        {
            unsigned int lockKey;

            PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);

            skb = (struct sk_buff*) ptr;
            input_dev = dev_get_by_index(&init_net,skb->skb_iif);
            if ((docsis_null_vpid_handle == -1) && (input_dev && input_dev->vpid_handle != -1))
            {
                docsis_null_vpid_handle = ti_ppm_create_vpid(&input_dev->vpid_block);

                if (docsis_null_vpid_handle < 0)
                {
                    printk (KERN_ERR"Error: Unable to create Null VPID %d PID %d Device %s\n", docsis_null_vpid_handle, 9, input_dev->name);//ARRIS MOD
                }
                else
                {
#ifdef CONFIG_TI_HIL_DEBUG
                    if (0 == global_ti_hil_db.dbg_disabled)
                    {
                        printk ("Successfully created Null VPID %d PID %d Device %s\n",  docsis_null_vpid_handle, 9, input_dev->name);
                    }
#endif /* CONFIG_TI_HIL_DEBUG */

                    ti_ppm_set_vpid_flags( docsis_null_vpid_handle,
                                           TI_PP_VPID_FLG_TX_DISBL | TI_PP_VPID_FLG_RX_DISBL );
                }
            }

            if (!WARN(input_dev == NULL, "%s - %d: dev_get_by_index(&init_net,skb->skb_iif %d); returned NULL", 
                     __func__, __LINE__, skb->skb_iif))
            {
                dev_put(input_dev); 
            }

            ti_hil_null_hook(skb, docsis_null_vpid_handle);
            PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);

            break;
        }

        case TI_DOCSIS_FLTR_ADD:
        case TI_DOCSIS_FLTR_DEL:
        case TI_DOCSIS_FLTR_CHG:
        {
            /* Always flush ALL sessions since we never know the behavior of the filter.
             * It may discard or allow packets. In case filter is of "allow" kind it is
             * not different from any other regular session.
             * Thus it is required to flush everything. */
            ti_ppm_flush_sessions( -1 );
            break;
        }

        //ARRIS MOD START
        case TI_DOCSIS_CLASSIFY_ADD:
        case TI_DOCSIS_CLASSIFY_DEL:
        case TI_DOCSIS_CLASSIFY_CHG:
        {
            if(certMode)
            {
               ti_ppm_flush_sessions(-1);
            }
            break;
        }
        case TI_DOCSIS_MCAST_DEL:
        {
            ti_ppm_flush_sessions(-1);
            break;
        }
        // ARRIS MOD END

        case TI_DOCSIS_SESSIONS_DEL:
        {
            Uint32 numSessions = ((Uint32*)ptr)[0];
            Uint32 *sessList = &(((Uint32*)ptr)[1]);
            Uint32 i;
            for (i = 0; i < numSessions; i++)
            {
                ti_ppm_delete_session(sessList[i], NULL);

                if (global_ti_hil_db.tdoxDbg)
                {
                    printk("TDOX-DBG DeleteSession: jiffies=%ld, ses=%d\n", jiffies, sessList[i]);
                }
            }

            break;
        }

        case TI_DOCSIS_VOICE_SESSIONS_DEL:
        {
            if (TI_PP_MAX_ACCLERABLE_VOICE_SESSIONS != 0)
            {
                static struct net_device *ptr_voiceni_dev;
                ptr_voiceni_dev = dev_get_by_name(&init_net, "vni0");
    			if (ptr_voiceni_dev)
                {
                    ti_ppm_flush_sessions( ptr_voiceni_dev->vpid_handle );
                    printk("Flush all VOICE sessions\n");
                    dev_put(ptr_voiceni_dev);
                }
            }
            break; 
        }

        case TI_DOCSIS_DSID_CHG:
        {
            /* Flush existing sessions in case of any change in DSID configuration */
            ti_ppm_flush_sessions(-1);
            break;
        }

        case TI_BRIDGE_PORT_DELETE:
        {
            /* Event indicates that a device has been removed from the bridge. This event is generated
             * when the user executes the brctl delif command to remove a port from the bridge. */
            dev = (struct net_device*)ptr;

            /* Do not touch VPIDs that do not have any parent PID defined */
            if (dev->vpid_block.parent_pid_handle == (unsigned char)(-1))
            {
                break;
            }

            /* This implies that the device is no longer capable of pariticipating
             * in the networking and should be removed from the packet processor.
             * But before doing so check if the device was attached to the Packet
             * processor or not i.e. valid VPID handle. */
            if (dev->vpid_handle != -1)
            {
                /* Delete the VPID. */
                if (ti_ppm_delete_vpid (dev->vpid_handle) < 0)
                {
                    printk (KERN_ERR"Error: Unable to delete VPID %d PID %d Device %s\n", dev->vpid_handle, dev->pid_handle, dev->name);//ARRIS MOD
                    return -1;
                }
#ifdef CONFIG_TI_HIL_DEBUG
                if (0 == global_ti_hil_db.dbg_disabled)
                {
                    printk ("Succesfully deleted VPID %d PID %d Device %s\n", dev->vpid_handle, dev->pid_handle, dev->name);
                }
#endif /* CONFIG_TI_HIL_DEBUG */
                dev->vpid_handle = -1;
            }
            /* Uninstall the Ingress Hook; since because if the device is no longer a VPID there is no
             * point to listen for packets on it. */
            ti_deregister_protocol_handler (dev);
            ti_deregister_egress_hook_handler (dev);
            break;
        }
        case TI_PP_ADD_VPID:
            /* Event to add vpid, for example when GW add vlan to LSD */
        case TI_BRIDGE_PORT_FORWARD:
        {
            /* Event indicates that a port attached to the bridge has moved to the FORWARDING state.
             * The host bridge will only forward packets in this state. Thus this is the time we
             * create the VPID.*/
            dev = (struct net_device*)ptr;

            /* Do not create any VPIDs if there is no parent PID defined */
            if (dev->vpid_block.parent_pid_handle == (unsigned char)(-1))
            {
                break;
            }

            if (dev->vpid_handle == -1)
            {
                if (0 == global_ti_hil_db.qos_disabled)
                {
                    if (NULL != dev->qos_setup_hook)
                    {
                        if (NETDEV_PP_QOS_PROFILE_DEFAULT != dev->qos_virtual_scheme_idx)
                        {
                            dev->qos_setup_hook(dev);
                        }
                    }
                }

                /* Create the VPID. */
                dev->vpid_handle = ti_ppm_create_vpid (&dev->vpid_block);
                if (dev->vpid_handle < 0)
                {
                    printk (KERN_ERR"Error: Unable to create VPID %d PID %d Device %s\n", dev->vpid_handle, dev->pid_handle, dev->name);//ARRIS MOD
                    return -1;
                }

#ifdef CONFIG_TI_HIL_DEBUG
                if (0 == global_ti_hil_db.dbg_disabled)
                {
                    printk ("Succesfully created VPID %d PID %d Device %s\n", dev->vpid_handle, dev->pid_handle, dev->name);
                }
#endif /* CONFIG_TI_HIL_DEBUG */
            }
            /* Install an Ingress Hook on the networking device. This is done if either
             * of the following conditions are met:-
             *  (a) Device has valid PID Handle.
             *  (b) Device has valid VPID Handle. */
            if ((dev->vpid_handle != -1) || (dev->pid_handle != -1))
            {
                /* Install the Ingress Hook. */
                if (ti_register_protocol_handler (dev, ti_hil_ingress_hook) == 0)
                {
#ifdef CONFIG_TI_HIL_DEBUG
                    if (0 == global_ti_hil_db.dbg_disabled)
                    {
                        printk ("Ingress Hook on PID/VPID: %d/%d Name: %s\n", dev->pid_handle, dev->vpid_handle, dev->name);
                    }
#endif /* CONFIG_TI_HIL_DEBUG */
                }

                /* Install the Egress Hook. */
                if (ti_register_egress_hook_handler(dev, ti_hil_egress_hook) == 0)
                {
#ifdef CONFIG_TI_HIL_DEBUG
                    if (0 == global_ti_hil_db.dbg_disabled)
                    {
                        printk ("Egress  Hook on PID/VPID: %d/%d Name: %s\n", dev->pid_handle, dev->vpid_handle, dev->name);
                    }
#endif /* CONFIG_TI_HIL_DEBUG */
                }
            }
            break;
        }
        case TI_PP_REMOVE_VPID:
            /* Event to remove vpid, for example when GW add vlan to LSD */
        case TI_BRIDGE_PORT_DISABLED:
        {
            /* Event indicates that a port attached to the bridge has been moved to the disabled state.
             * This event if of concern only if the bridge is running the spanning tree protocol. The event
             * occurs if the spanning tree protocol detects a condition where the port is causing a loop and
             * disables the port. In such a scenario the VPID and thus all sessions related to the port need
             * to be removed from the packet processor. */
            dev = (struct net_device*)ptr;

            /* Do not touch VPIDs that do not have any parent PID defined */
            if (dev->vpid_block.parent_pid_handle == (unsigned char)(-1))
            {
                break;
            }

            /* If the device has a VPID handle; it needs to be removed from the packet processor. */
            if (dev->vpid_handle != -1)
            {
                /* Delete the VPID. */
                if (ti_ppm_delete_vpid (dev->vpid_handle) < 0)
                {
                    printk (KERN_ERR"Error: Unable to delete VPID %d PID %d Device %s\n", dev->vpid_handle, dev->pid_handle, dev->name);//ARRIS MOD
                    return -1;
                }

                {
                    TI_PP_PID ptr_pid;
                    ti_ppm_get_pid_info (dev->vpid_block.parent_pid_handle, &ptr_pid);
                    if (ptr_pid.type == TI_PP_PID_TYPE_ETHERNET)
                    {
                        Uint32 queueIndex;
                        Uint32 divertCommand;
                        Uint32 qmgr = PAL_CPPI41_QUEUE_MGR_PARTITION_SR;
                        PAL_Handle handle = PAL_cppi4Init (NULL,(Ptr)PAL_CPPI41_QUEUE_MGR_PARTITION_SR);

                        for (queueIndex = 0; queueIndex < 1; queueIndex++)
                        {
                            divertCommand =  (PAL_CPPI41_RECYCLE_INFRA_INPUT_LOW_Q_NUM << 16);     // Setup destination Queue
                            divertCommand += ptr_pid.tx_pri_q_map[queueIndex];  // Setup source Queue

                            PAL_cppi4Control( handle, PAL_CPPI41_IOCTL_QUEUE_DIVERT, (Ptr)divertCommand, &qmgr );
                        }

                        PAL_cppi4Exit(handle, NULL);
                    }
                }

#ifdef CONFIG_TI_HIL_DEBUG
                if (0 == global_ti_hil_db.dbg_disabled)
                {
                    printk ("Succesfully deleted VPID %d PID %d Device %s\n", dev->vpid_handle, dev->pid_handle, dev->name);
                }
#endif /* CONFIG_TI_HIL_DEBUG */
                dev->vpid_handle = -1;
            }
            /* Uninstall the Ingress Hook; since because if the device is no longer a VPID there is no
             * point to listen for packets on it. */
            ti_deregister_protocol_handler (dev);
            ti_deregister_egress_hook_handler (dev);
            break;
        }
        case TI_BRIDGE_FDB_CREATED:
        {
            /* Event indicates that an FDB entry is being created. This event is of concern if the single session
             * per interface mode is selected as an option for session creation. If this is not the case then this
             * event can be ignored. */
            fdb = (struct net_bridge_fdb_entry *)ptr;

#ifdef CONFIG_TI_HIL_DEBUG
            if (0 == global_ti_hil_db.dbg_disabled)
            {
                /* Debug Message: To indicate how to access all the information? */
                printk ("Intrusive -> Create Bridge Session MAC=%02x-%02x-%02x-%02x-%02x-%02x on %s VPID:%d\n",
                    fdb->addr.addr[0], fdb->addr.addr[1], fdb->addr.addr[2], fdb->addr.addr[3], fdb->addr.addr[4],
                    fdb->addr.addr[5], fdb->dst->dev->name, fdb->dst->dev->vpid_handle);
            }
#endif /* CONFIG_TI_HIL_DEBUG */
            break;
        }
        case TI_BRIDGE_FDB_DELETED:
        {
            /* Event indicates that an FDB entry is being deleted. The event is of concern if the single session
             * per interface mode is selected as an option for session creation. If this is not the case then this
             * event can be ignored. */
            fdb = (struct net_bridge_fdb_entry *)ptr;

#ifdef CONFIG_TI_HIL_DEBUG
            if (0 == global_ti_hil_db.dbg_disabled)
            {
                /* Debug Message: To indicate how to access all the information? */
                printk ("Intrusive -> Delete Session MAC=%02x-%02x-%02x-%02x-%02x-%02x on %s VPID:%d\n",
                    fdb->addr.addr[0], fdb->addr.addr[1], fdb->addr.addr[2], fdb->addr.addr[3], fdb->addr.addr[4],
                    fdb->addr.addr[5], fdb->dst->dev->name, fdb->dst->dev->vpid_handle);
            }
#endif /* CONFIG_TI_HIL_DEBUG */
            break;
        }
        case TI_BRIDGE_PACKET_FLOODED:
        {
            /* Event indicates that the packet will now be flooded onto all interfaces. This can happen in
             * any of the following cases:-
             *  a) Unicast packet but no matching FDB entry is found.
             *  b) Broadcast packet
             *  c) Multicast packet but no layer2 extensions eg IGMP snooping exists */
            skb = (struct sk_buff*) ptr;

#ifdef CONFIG_TI_HIL_DEBUG
            if (0 == global_ti_hil_db.dbg_disabled)
            {
                printk ("Intrusive -> Packet %p is flooded on all interfaces\n", skb);
            }
#endif /* CONFIG_TI_HIL_DEBUG */

            /* In the intrusive mode profile these packets are not considered as candidates for acceleration.
             * So mark the packet BYPASS mode so that the egress hook is bypassed. */
            skb->pp_packet_info.flags |= TI_HIL_PACKET_FLAG_PP_SESSION_BYPASS;
            break;
        }

        case TI_ROUTE_ADDED:
        {
            /*  Event indicates that a new route is being added. A route change
             *  could affect the existing sessions in the PP. So, the HIL profile
             *  could choose to delete all sessions that are affected by the route
             *  change / flush the entire session table to make sure that there
             *  is no inconsistency due to the route change.
             */
            break;
        }
        case TI_ROUTE_DELETED:
        {
            /*  Event indicates that an existing route is being deleted. A route change
             *  could affect the existing sessions in the PP. So, the HIL profile
             *  could choose to delete all sessions that are affected by the route
             *  change / flush the entire session table to make sure that there
             *  is no inconsistency due to the route change.
             */
            break;
        }

#ifdef CONFIG_NETFILTER
        case TI_CT_ENTRY_CREATED:
        {
            /* Event generated from the connection tracking layer to indicate that a connection tracking entry
             * has been created. This could be used by the system profile to analyze the connection and indicate
             * immediately if the connection is worthy of accleration or not? If the profile deems that the
             * connection can not be accelerated it sets the status flag in the connection tracking entry to BYPASS
             * mode. This will ensure that all packets matching the connection will also have the BYPASS mode
             * set. This can be used for performance optimizations as the Egress Hook and Session Intelligence
             * will be less burdened. */
            conntrack = (struct nf_conn *)ptr;

#ifdef CONFIG_TI_HIL_DEBUG
            if (0 == global_ti_hil_db.dbg_disabled)
            {
                printk ("Intrusive -> Connection Tracking Entry %p has been created\n", conntrack);
            }
#endif /* CONFIG_TI_HIL_DEBUG */

            /* Check if the conntrack was associated with an ALG? In our profile we dont want these sessions
             * to be accelerated so set the connection tracking entry to operate in BYPASS mode. */
            if ((nfct_help(conntrack)) != NULL)
            {
                conntrack->ti_pp_status_flag |= TI_PP_BYPASS;
            }

            break;
        }
        case TI_CT_DEATH_BY_TIMEOUT:
        {
            /* Event indicates that the connection tracking entry has timed out. Use this event to
             * determine if the connection tracking entry needs to be deleted or not? If the profile
             * wants to prevent the death of the connection tracking entry it should ensure that the
             * PP staus flag does NOT set the TI_PP_KILL_CONNTRACK bit and if so be the case the
             * connection tracking entry is now owned by the System Profile and it has the repsonsibility
             * of cleaning it.
             * This event needs to be handled only if CONFIG_NETFILTER is enabled else this event can
             * be safetly ignored. */
            conntrack = (struct nf_conn *)ptr;

#ifdef CONFIG_TI_HIL_DEBUG
            if (0 == global_ti_hil_db.dbg_disabled)
            {
                printk ("Intrusive -> Connection Tracking Entry %p has timed out.\n", conntrack);
            }
#endif /* CONFIG_TI_HIL_DEBUG */

            if (IS_TI_PP_SESSION_CT_INVALID(conntrack->tuplehash[IP_CT_DIR_ORIGINAL].ti_pp_session_handle) &&
                IS_TI_PP_SESSION_CT_INVALID(conntrack->tuplehash[IP_CT_DIR_REPLY   ].ti_pp_session_handle))
            {
                /* Neither of the flows in the connection tracking entry are being accelerated.
                 * This implies that we should just go ahead and delete the entry? */
                conntrack->ti_pp_status_flag |= TI_PP_KILL_CONNTRACK;
            }
            else
            {
                /* The flows are still being accelerated; so keep the connection tracking timer alive */
#ifdef INCLUDE_SERCOMM_GW //Use 30 instead of 3000. Fix all sessions are occupied by PP.
        		conntrack->timeout.expires = 30*HZ + jiffies;
#else
                conntrack->timeout.expires = (ti_session_timeout_sec * HZ) + jiffies;
                add_timer(&conntrack->timeout);
#endif
            }
            break;
        }
        case TI_CT_NETFILTER_TABLE_UPDATE:
        {
            /* Get the netfilter table */
            t = (struct xt_table *)ptr;

#ifdef CONFIG_TI_HIL_DEBUG
            if (0 == global_ti_hil_db.dbg_disabled)
            {
                printk ("Intrusive -> Netfilter Table %s has been modified\n", t->name);
            }
#endif /* CONFIG_TI_HIL_DEBUG */

#ifndef INCLUDE_SERCOMM_GW  //We don't want to flush session everytime.
            /* Flush all sessions only for NAT... No need to do anything for Mangle and Firewall */
            if (strcmp (t->name, "nat") == 0 || strcmp (t->name, "filter") == 0)
            {
                if (ti_ppm_flush_sessions(-1) < 0 )
                {
                    printk (KERN_ERR"Error: Unable to flush all sessions\n");//ARRIS MOD
                    return 0;
                }
                printk ("NAT Table update all sessions flushed\n");
            }
#endif
            break;
        }

        case TI_CT_NETFILTER_CANCEL_DISCARD_ACCELERATION:
        {
            __be32      src_ip;
            __be32      dst_ip;
            __be16      src_port;
            __be16      dst_port;

            conntrack = (struct nf_conn *)ptr;

            src_ip      = conntrack->tuplehash[IP_CT_DIR_REPLY].tuple.src.u3.ip;
            dst_ip      = conntrack->tuplehash[IP_CT_DIR_REPLY].tuple.dst.u3.ip;
            src_port    = conntrack->tuplehash[IP_CT_DIR_REPLY].tuple.src.u.all;
            dst_port    = conntrack->tuplehash[IP_CT_DIR_REPLY].tuple.dst.u.all;

            ti_ppm_delete_drop_sessions( &netfilter_null_vpid_handle, &docsis_null_vpid_handle, 
                                         src_ip, dst_ip, src_port, dst_port );

            break ;
        }

#endif /* CONFIG_NETFILTER */

        /********************************************/
        /*                                          */
        /* This case falls through into the next    */
        /*                                          */
        case TI_IP_DISCARD_PKT_IPV4:
        case TI_IP_DISCARD_PKT_IPV6:
            if (((event_id == TI_IP_DISCARD_PKT_IPV4) && DROPPED_PACKETS_BITMAP_IS_SET(4)) ||
                ((event_id == TI_IP_DISCARD_PKT_IPV6) && DROPPED_PACKETS_BITMAP_IS_SET(6)))
            {
                /* Hanlde event, fallthru */
            }
            else
            {
			    /* Appropriate bit is not set, discard event */
                break;
            }
        /*                                          */
        /* fallthrogh                               */
        /*                                          */
#ifdef CONFIG_NETFILTER
        case TI_CT_NETFILTER_DISCARD_PKT:
#endif /* CONFIG_NETFILTER */
        /*                                          */
        /********************************************/
        {
            unsigned int lockKey;

            PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);

            skb = (struct sk_buff*) ptr;

            if (!(skb->skb_iif))
            {
                PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
                break;
            }
            input_dev = dev_get_by_index(&init_net,skb->skb_iif);
            if ((netfilter_null_vpid_handle == -1) && (input_dev && input_dev->vpid_handle != -1) && (ti_pp_get_status() != PPM_PSM))
            {
                netfilter_null_vpid_handle = ti_ppm_create_vpid(&input_dev->vpid_block);

                if (netfilter_null_vpid_handle < 0)
                {
                    printk (KERN_ERR"Error: Unable to create Null VPID %d Device %s\n", netfilter_null_vpid_handle, input_dev->name);//ARRIS MOD
                }
                else
                {
#ifdef CONFIG_TI_HIL_DEBUG
                    if (0 == global_ti_hil_db.dbg_disabled)
                    {
                        printk ("Successfully created Null VPID %d Device %s\n",  netfilter_null_vpid_handle, input_dev->name);
                    }
#endif /* CONFIG_TI_HIL_DEBUG */

                    ti_ppm_set_vpid_flags( netfilter_null_vpid_handle,
                        TI_PP_VPID_FLG_TX_DISBL | TI_PP_VPID_FLG_RX_DISBL );
                }
            }

            ti_hil_null_hook(skb, netfilter_null_vpid_handle);
            if (!WARN(input_dev == NULL, "%s - %d: dev_get_by_index(&init_net,skb->skb_iif %d); returned NULL", 
                     __func__, __LINE__, skb->skb_iif))
            {
                dev_put(input_dev); 
            }
            PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);

            break;
        }

#ifdef CONFIG_IP_MULTICAST
    case TI_MC_SESSION_DELETED:
        {

            if(NULL == ptr)
                {
                printk (KERN_ERR"FATAL Error: Multicast params is NULL\n");//ARRIS MOD
                break;
                }
            ptr_mfc_cache = (struct mfc_cache* )ptr;
            hil_mfc_delete_session_by_mc_group(ptr_mfc_cache->mfc_mcastgrp);
            break;
        }
        case TI_MFC_ENTRY_CREATED:
        {
            /* Event indicates that a Multicast Forwarding Cache Entry has been created. This is typically
             * the case when a proxy or multicast routing daemon has detected a multicast group to be active
             * and has created the entry which will allow multicast packets to flow through the box.
             * The parameter passed to the event is the Multicast Forwarding Cache Entry.
             *
             * This event can be ignored if the System does not support Multicast Routing. */
            int              session_handle;
            unsigned int     lockKey;
            TI_PP_SESSION    *session = (TI_PP_SESSION *)kmalloc(sizeof(TI_PP_SESSION), GFP_KERNEL);

            if (session == NULL)
            {
                printk(KERN_ERR "%s kmalloc failed\n",__FUNCTION__);
                break;
            }

            if(NULL == ptr)
            {
                printk ("FATAL Error: Multicast params is NULL\n");
                kfree(session);
                break;
            }

            PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);

            /* Get the MFC Cache Entry. */

            ptr_mfc_cache =  ((struct pp_mr_param *)(ptr))->cache;

#ifdef CONFIG_TI_HIL_DEBUG
            if (0 == global_ti_hil_db.dbg_disabled)
            {
                printk ("Intrusive -> Join Multicast Group %x\n", ptr_mfc_cache->mfc_mcastgrp);
            }
#endif /* CONFIG_TI_HIL_DEBUG */

            /* Convert the MFC Cache to a session structure */
            if (hil_mfc_to_session((struct pp_mr_param *)ptr, session) == 0)
            {
                int isNew;
                /* Conversion was successful; so lets try and create a session */

                // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! //
                // !!!!!!!!!!!!! TBD: skb is not initialized here !!!!!!!!!!!!! //
                // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! //
                session_handle = ti_ppm_create_session(session, (void*)skb, 0, 0, &isNew);

                if (global_ti_hil_db.tdoxDbg)
                {
                    printk("TDOX-DBG CreateSession: jiffies=%ld, ses=%d\n", jiffies, session_handle);
                }
                if (session_handle < 0)
                {
                    printk ("FATAL Error: Multicast session creation failed\n");
                }
                else
                {
                    /* Map the session and MFC Entry together. */
                    printk ("Multicast Session %d created succesfully\n", session_handle);
                    hil_mfc_add_entry (ptr_mfc_cache->mfc_mcastgrp, session_handle);
                }
            }
            else
            {
                /* Conversion was not successful. */
                printk ("Error: MFC to session conversion failed\n");
            }

            PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
            kfree(session);
            break;
        }
        case TI_MFC_ENTRY_DELETED:
        {
            /* Event indicates that a Multicast Forwarding Cache Entry has been deleted. This is typically
             * the case when a proxy or multicast routing daemon has detected a multicast group to be inactive
             * and has deleted the Multicast Forwarding cavhe entry
             *
             * This event can be ignored if the System does not support Multicast Routing. */
            int session_handle;
            unsigned int     lockKey;

            PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);

            /* Get the MFC cache entry which needs to be deleted. */
            ptr_mfc_cache = (struct mfc_cache* )ptr;

#ifdef CONFIG_TI_HIL_DEBUG
            if (0 == global_ti_hil_db.dbg_disabled)
            {
                printk ("Intrusive -> Leave Multicast Group %x\n", ptr_mfc_cache->mfc_mcastgrp);
            }
#endif /* CONFIG_TI_HIL_DEBUG */

            /* Get the session handle and delete the MFC and Session Handle mapping */
            session_handle = hil_mfc_del_entry(ptr_mfc_cache->mfc_mcastgrp);
            if (session_handle >= 0)
            {
                /* Timer has expired for a session; time to delete the session. */
                if (ti_ppm_delete_session (session_handle, NULL) < 0)
                    printk ("Error: Unable to delete session %d\n", session_handle);

                if (global_ti_hil_db.tdoxDbg)
                {
                    printk("TDOX-DBG DeleteSession: jiffies=%ld, ses=%d\n", jiffies, session_handle);
                }
            }

            PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
            break;
        }
#endif /* CONFIG_IP_MULTICAST */

#ifdef CONFIG_VLAN_8021Q
        case TI_VLAN_DEV_CREATED:
        {
            /* Get the pointer to the network device. */
            dev = (struct net_device*)ptr;

#ifdef CONFIG_TI_HIL_DEBUG
            if (0 == global_ti_hil_db.dbg_disabled)
            {
                printk ("Intrusive -> VLAN Device %s has been created\n", dev->name);
            }
#endif /* CONFIG_TI_HIL_DEBUG */

            /* We know for sure that this is attached to a VLAN interface.
             * So configure the VPID Information Block appropriately. The egress
             * MTU of the interface needs to be correctly handled to account for
             * the VLAN Header. */
            dev->vpid_block.type            = TI_PP_VLAN;
            dev->vpid_block.vlan_identifier = (vlan_dev_info(dev))->vlan_id;
            dev->vpid_block.egress_mtu      = dev->vpid_block.egress_mtu - VLAN_HLEN;
            break;
        }
        case TI_VLAN_DEV_DELETED:
        {
            /* Get the pointer to the network device. */
            dev = (struct net_device*)ptr;

#ifdef CONFIG_TI_HIL_DEBUG
            if (0 == global_ti_hil_db.dbg_disabled)
            {
                printk ("Intrusive -> VLAN Device %s is being removed\n", dev->name);
            }
#endif /* CONFIG_TI_HIL_DEBUG */
            break;
        }
#endif /* CONFIG_VLAN_8021Q */

#ifdef CONFIG_PPPOE
        case TI_PPP_INTERFACE_CREATED:
        {
            /* Get the pointer to the network device. */
            dev = (struct net_device*)ptr;

#ifdef CONFIG_TI_HIL_DEBUG
            if (0 == global_ti_hil_db.dbg_disabled)
            {
                printk ("Intrusive -> PPP Device %s has been created Session ID:0x%x\n", dev->name, dev->padded);
            }
#endif /* CONFIG_TI_HIL_DEBUG */

            /* PPPoE could be executed over a VLAN connection or on a vanilla Ethernet
             * connection. Configure the VPID device type appropriately. */
            if (dev->vpid_block.type == TI_PP_VLAN)
            {
                /* The PPP connection is initialized over a VLAN connection. Configure the Egress MTU
                 * appropriately accounting for the PPP and VLAN headers. */
                dev->vpid_block.type       = TI_PP_VLAN_PPPoE;
                dev->vpid_block.egress_mtu = dev->vpid_block.egress_mtu - sizeof(struct pppoe_hdr) - 2 - VLAN_HLEN;
            }
            else
            {
                /* The PPP Connection is being bought over a vanilla Ethernet connection. Configure
                 * the Egress MTU appropriately accounting only for the PPP header */
                dev->vpid_block.type        = TI_PP_PPPoE;
                dev->vpid_block.egress_mtu  = dev->vpid_block.egress_mtu - sizeof(struct pppoe_hdr) - 2;
            }

            /* Extract and configure the PPP Session ID. */
            dev->vpid_block.ppp_session_id  = dev->padded;
            break;
        }
#endif /* CONFIG_PPPOE */
        default:
        {
            printk ("Intrusive -> Does not handle event 0x%lx\n", event_id);
            break;
        }
    }

    /* Successfully handled the event. */
    return 0;
}

/**************************************************************************
 * FUNCTION NAME : ti_hil_netsubsystem_event_handler
 **************************************************************************
 * DESCRIPTION   :
 *  This is the HIL Intrusive event handler which will capture all events from
 *  the networking sub-system.
 *
 * RETURNS       :
 *  0   -   Success
 *  <0  -   Error
 **************************************************************************/
static int ti_hil_netsubsystem_event_handler(unsigned int module_id, unsigned long event_id, void* ptr)
{
    /* Process based on the module identifier */
    switch (module_id)
    {
#if 0 /* TODO: Enable once router functionality is available */
        case TI_DEVICE:
        {
            /* HIL Device Handler. */
            ti_hil_device_handler (event_id, ptr);
            break;
        }
        case TI_INET:
        {
            /* HIL Inet Handler. */
            ti_hil_inet_handler (event_id, ptr);
            break;
        }
#endif
        case TI_PP:
        {
            /* HIL PP Handler. */
            ti_hil_pp_handler (event_id, ptr);
            break;
        }

        default:
            break;
    }
    return 0;
}

/**************************************************************************
 * FUNCTION NAME : ti_hil_pp_ppm_event_handler
 **************************************************************************
 * DESCRIPTION   :
 *  The function handles all the events generated by the PPM part of the
 *  Packet Processor Subsystem.
 **************************************************************************/
static void ti_hil_pp_ppm_event_handler (int event, unsigned int param1, unsigned int param2)
{
    /* Process the event accordingly... */
    switch (event)
    {
        case TI_PPM_OUT_OF_MEMORY:
        {
            printk("FATAL Error: PPM is OUT of memory\n");
            break;
        }
        case TI_PPM_INTERNAL_ERROR:
        {
            printk("FATAL Error: PPM Internal Error\n");
            break;
        }
        case TI_PPM_CREATE_PID_FAILED:
        {
            struct net_device *dev = (struct net_device *) param2;

            printk("FATAL Error: PP Operation TI_PPM_CREATE_PID_FAILED, pid_handle=%d @ %s device\n", param1, param2 ? dev->name : "NULL");
            break;
        }
        case TI_PPM_DELETE_PID_FAILED:
        {
            printk("FATAL Error: PP Operation TI_PPM_DELETE_PID_FAILED, pid_handle=%d\n", param1);
            break;
        }
        case TI_PPM_CREATE_VPID_FAILED:
        {
            printk("FATAL Error: PP Operation TI_PPM_CREATE_VPID_FAILED, vpid_handle=%d\n", param1);
            break;
        }
        case TI_PPM_DELETE_VPID_FAILED:
        {
            printk("FATAL Error: PP Operation TI_PPM_DELETE_VPID_FAILED, vpid_handle=%d\n", param1);
            break;
        }
        case TI_PPM_CREATE_SESSION_FAILED:
        {
            printk("Error: PP Operation TI_PPM_CREATE_SESSION_FAILED, session_handle=%d\n", param1);
            break;
        }
        case TI_PPM_DELETE_SESSION_FAILED:
        {
            printk("Error: PP Operation TI_PPM_DELETE_SESSION_FAILED, session_handle=%d\n", param1);
            break;
        }
        case TI_PPM_MODIFY_SESSION_FAILED:
        {
            printk ("Error: PP Operation TI_PPM_MODIFY_SESSION_FAILED, session_handle=%d\n", param1);
            break;
        }
        case TI_PPM_PID_CREATED:
        {
            struct net_device *dev = (struct net_device *) param2;

            if (0 == global_ti_hil_db.qos_disabled)
            {
                if ((NULL != dev) && (NULL != dev->qos_setup_hook))
                {
                    dev->qos_setup_hook( dev );
                }
            }
            printk ("PP Operation TI_PPM_PID_CREATED, pid_handle=%d @ %s device\n", param1, param2 ? dev->name : "NULL" );
            break;
        }
        case TI_PPM_PID_DELETED:
        {
            printk ("PP Operation TI_PPM_PID_DELETED, pid_handle=%d\n", param1);
            break;
        }
        case TI_PPM_VPID_CREATED:
        {
            printk ("PP Operation TI_PPM_VPID_CREATED, vpid_handle=%d\n", param1);
            break;
        }
        case TI_PPM_VPID_DELETED:
        {
            printk ("PP Operation TI_PPM_VPID_DELETED, vpid_handle=%d\n", param1);
            break;
        }
        case TI_PPM_SESSION_MODIFIED:
        {
            break;
        }
        case TI_PPM_SESSION_CREATED:
        {
            ti_hil_session_db_reset_entry(param1);

#ifdef CONFIG_TI_PACKET_PROCESSOR_STATS
            if (ti_hil_start_session_notification_cb)
            {
                if (((struct sk_buff*)param2)->pp_packet_info.ti_session.egress[0].vpid_handle ==
                    docsis_null_vpid_handle)
                {
                    ti_hil_start_session_notification_cb(param1, TI_DOCSIS_PP_SESSION_TYPE_DISCARDING,
                                                         ((struct sk_buff*)param2));
                }
                else
                {
                    ti_hil_start_session_notification_cb(param1, TI_DOCSIS_PP_SESSION_TYPE_FORWARDING,
                                                         ((struct sk_buff*)param2));
                }
            }
#endif /* CONFIG_TI_PACKET_PROCESSOR_STATS */

            /* Do the necessary work for the HIL Analysis. */
            {
                int                 num_current_session;
                int                 bucket;

                global_ti_hil_db.num_total_sessions++;
                num_current_session = ti_ppm_get_session (-1, 0, NULL);
                bucket = num_current_session / HIL_SESSION_STAT_BUCKET;

                /* If the number of sessions is the maximum number of sessions, then the above equation results
                * in calculating a bucket one more than the expected number leading to erroneous calculations.
                * Thus decrement the bucket number so as to increment the session counter in the last bucket.
                * For e.g.,
                *  num_current_session = 256 then, bucket = 256 / 64 = 4 so we end up incrementing the
                *  counter in bucket[4] when the valid buckets are 0,1,2,and 3. */
                if(bucket == HIL_MAX_NUM_BUCKETS)
                    bucket--;
                global_ti_hil_db.session_bucket[bucket]++;
            }

#ifdef CONFIG_NETFILTER
            {
                struct sk_buff * skb = (struct sk_buff *)param2;

                /* Once the session has been created; check if the packet had passed through the connection tracking hooks and pass the session handle to that layer */
                if (skb->nfct != NULL)
                {
                    /* Session has passed through the connection tracking hooks. Now get the connection tracking entry and set the hooks correctly */
                    struct nf_conn*     conntrack   = (struct nf_conn *)skb->nfct;
                    Uint8               dir         = CTINFO2DIR(skb->nfctinfo);  // enum ip_conntrack_dir
                    Uint16              ct_session  = conntrack->tuplehash[ dir ].ti_pp_session_handle;
                    Uint16 new_session_handle;
                    Uint16 prev_new_session_handle;
		    Uint32 lockKey;

                    /* Check if the current session handle is valid or not? */
                    if (!IS_TI_PP_SESSION_CT_INVALID( ct_session ))
                    {
                        /* Handle was valid. Now we need to ensure that the current session handle matches the one we just created */
                        if ( ct_session != (Uint16)param1 )
                        {
                            /* The existing session handle does not match the one we had.
                             *
                             * This should typically not happen because the PPM has an inbuilt duplicate session detection logic which will detect this and return the same session handle.
                             * (Example * of this is the TCP Control Packets will have the same session handle passed to them at this stage so this code will never get executed)
                             *
                             * The fact that the control came here is that the PPM duplicate session detection logic failed or because there was something in the packet different which the conntrack
                             * did not care about. One of the known occurrences is the TOS Byte difference. Anyway in this case we need to handle the condition gracefully.
                             * So currently we ignore the new session handle and dont link it with the connection tracking entry.
                             *
                             * One more technique that can be used to solve this problem is to ignore the TOS byte in the LUT configuration */


                            /*if the ti_pp_sessions_count is 0 then something went wrong. 
                              it means that the connection tracking entry is mapped to a session
                              but the refrence count which should reflect the number of sessions that this connection tracking entry is mapped to does not match*/
                            if (conntrack->tuplehash[ dir ].ti_pp_sessions_count == 0)
                                printk ("ERROR--> Existing session %d new session %d.\n", ct_session, param1);
                        }
                    }
                    PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);

                    /*add the new_session_handle to the ct mapper list 
                      the mapper's list is a bidirectional list containg sessions that belong to the same connection tracking entry */
					  
                    new_session_handle = (Uint16)param1;
                    prev_new_session_handle = conntrack->tuplehash[ dir ].ti_pp_session_handle;

                   
                    hil_add_session_handle_to_ct_mapper_list (new_session_handle,prev_new_session_handle);

                    /*update the session handle in the conntrack. the 4 upper bits represent the validity of the session- 
                      0 means session is valid, any other value - session is not valid .
                      the 12 lower bits represent the session number. the bit operation & guarantee .                        
                      that the ti_pp_session_handle is saved as a valid session in the conntrack */
                    conntrack->tuplehash[ dir ].ti_pp_session_handle = (new_session_handle & 0x0fff);  
                    conntrack->tuplehash[ dir ].ti_pp_sessions_count++;  
					 /* Map the session handle and connection tracking entry together */		
                    hil_session_ct_mapper[ new_session_handle ].conntrack = conntrack;  
                    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
                }
            }
#endif

            break;
        }
        case TI_PPM_SESSION_DELETED:
        {
#ifdef CONFIG_TI_PACKET_PROCESSOR_STATS
            if (ti_hil_delete_session_notification_cb)
            {
				unsigned long long bytes_forworded = 0;

				/* Bytes forwarded */
				bytes_forworded = ((TI_PP_SESSION_STATS*)param2)->bytes_forwarded_hi;
				bytes_forworded <<= 32;
				bytes_forworded += ((TI_PP_SESSION_STATS*)param2)->bytes_forwarded_lo;

                /* add the 4 bytes the PP is ignoring for each packet */
                bytes_forworded += (4 * ((TI_PP_SESSION_STATS*)param2)->packets_forwarded);


                ti_hil_delete_session_notification_cb(param1, ((TI_PP_SESSION_STATS*)param2)->packets_forwarded, bytes_forworded);
            }
#endif /* CONFIG_TI_PACKET_PROCESSOR_STATS */
            {
                unsigned int lockKey;

                PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);

                if (global_ti_hil_db.tdoxDbg)
                {
                    printk("TDOX-DBG FreeTdox (DeleteSession): jiffies=%ld, ses=%d, tdox=%d\n", jiffies, param1, hilSessionDb[param1].tdoxHandle);
                }
                ti_hil_tdox_free_session(param1, -1);
                ti_hil_session_db_reset_entry(param1);

                PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);

#ifdef CONFIG_NETFILTER
                {
                    struct nf_conn* ct = hil_session_ct_mapper[ param1 ].conntrack;
                    Uint32 lockKey;
                    enum ip_conntrack_dir dir;

                    /* Once the session has been removed; check if the session handle was present in the mapper.
                       This implies that the session handle and connection tracking entry were connected to each other */
                    if (ct != NULL)
                    {
                        PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);

                        if (hil_find_session_handle_in_ct_mapper_list (ct->tuplehash[ IP_CT_DIR_REPLY ].ti_pp_session_handle,param1) == true) 
                        {
                            dir = IP_CT_DIR_REPLY;
                            
                            ct->tuplehash[IP_CT_DIR_REPLY].ti_pp_sessions_count--; 
                            if (ct->tuplehash[ IP_CT_DIR_REPLY ].ti_pp_sessions_count == 0)
                                {
                                ct->tuplehash[ IP_CT_DIR_REPLY ].ti_pp_session_handle = TI_PP_SESSION_CT_TCP_UPDATE;
                                if (IS_TI_PP_SESSION_CT_INVALID(ct->tuplehash[ IP_CT_DIR_ORIGINAL ].ti_pp_session_handle))
                                    {
                                    ct->tuplehash[ IP_CT_DIR_ORIGINAL ].ti_pp_session_handle = TI_PP_SESSION_CT_TCP_UPDATE;
                                    }
                                }
			    hil_delete_session_handle_in_ct_mapper_list (param1,dir);
			}
                        else if (hil_find_session_handle_in_ct_mapper_list (ct->tuplehash[ IP_CT_DIR_ORIGINAL ].ti_pp_session_handle,param1) == true)
                        {
                            dir = IP_CT_DIR_ORIGINAL;
                            
                            ct->tuplehash[ IP_CT_DIR_ORIGINAL ].ti_pp_sessions_count--;
                            if (ct->tuplehash[ IP_CT_DIR_ORIGINAL ].ti_pp_sessions_count == 0)
                                {
                                ct->tuplehash[ IP_CT_DIR_ORIGINAL ].ti_pp_session_handle = TI_PP_SESSION_CT_TCP_UPDATE;
                                if (IS_TI_PP_SESSION_CT_INVALID(ct->tuplehash[ IP_CT_DIR_REPLY ].ti_pp_session_handle))
                            {
                                ct->tuplehash[IP_CT_DIR_REPLY].ti_pp_session_handle = TI_PP_SESSION_CT_TCP_UPDATE;
                            }
                        }
			    hil_delete_session_handle_in_ct_mapper_list (param1,dir);
			}
                        else
                        {
			                /*update current entry*/
                            hil_session_ct_mapper[param1].next = TI_PP_SESSION_CT_IDLE;
                            hil_session_ct_mapper[param1].previous = TI_PP_SESSION_CT_IDLE;
                            hil_session_ct_mapper[param1].conntrack = NULL;
                        }
			PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
                    }
                    
		}
#endif
                break;
            }
        }
    }

    /* Work is done! */
    return;
}

#ifdef CONFIG_NETFILTER
/**************************************************************************
 * FUNCTION NAME : hil_find_session_handle_in_ct_mapper_list
 **************************************************************************
 * DESCRIPTION   :
 * param - ct_session_handle - the session handle held in the connection tracking entry
 * session_handle - the session_handle needed to be searched
 * return- true if session_handle was found in the list , otherwise false
 **************************************************************************/
static Bool hil_find_session_handle_in_ct_mapper_list (Uint16 ct_session_handle,Uint16 session_handle)
{
    Bool rc = false;
	
    if ((IS_TI_PP_SESSION_CT_INVALID(ct_session_handle)) || (IS_TI_PP_SESSION_CT_INVALID(session_handle)))
    {
        rc = false;
    }
    else if (ct_session_handle == session_handle)
    {
        rc = true;
    }
    else
	{
		Uint16 cur_session = ct_session_handle;
        int count = 0;
        
    	while (hil_session_ct_mapper[cur_session].previous != ct_session_handle && (count < TI_PP_MAX_ACCLERABLE_SESSIONS))
    	{
        	if (hil_session_ct_mapper[cur_session].previous == session_handle)
            {
            	rc = true;
                break;
            }	
        	cur_session = hil_session_ct_mapper[cur_session].previous;
        	if (cur_session >= TI_PP_MAX_ACCLERABLE_SESSIONS)
        	{
                printk(KERN_ERR"Invalid session - %d \n", cur_session);
                break;
        	}
        	
        	count++;
        }

        if (count >= TI_PP_MAX_ACCLERABLE_SESSIONS)
        {
            printk(KERN_ERR"Error found in hil_find_session_handle_in_ct_mapper_list. \n");
        }
    }
    return rc;
}

/**************************************************************************
 * FUNCTION NAME : hil_add_session_handle_to_ct_mapper_list
 **************************************************************************
 * DESCRIPTION   : the function adds the session handle to the ct_mapper's list
 * param - new_session_handle - the new session handle to add to the ct_mapper's list
 * param - prev_new_session_handle - the last session handle that was inserted to the list
 * return - void
 **************************************************************************/

static void  hil_add_session_handle_to_ct_mapper_list (Uint16 new_session_handle,Uint16 prev_new_session_handle)
{
    Uint16 post_new_session;
    
    if (IS_TI_PP_SESSION_CT_INVALID( prev_new_session_handle ))
    {
        prev_new_session_handle = new_session_handle;
        post_new_session = new_session_handle;
    }
	else
	{
        post_new_session = hil_session_ct_mapper[prev_new_session_handle ].next;
	}

    /*update the ct_mapper*/
    hil_session_ct_mapper[new_session_handle].next = post_new_session;
    hil_session_ct_mapper[new_session_handle].previous = prev_new_session_handle;
    hil_session_ct_mapper[post_new_session].previous = new_session_handle;
    hil_session_ct_mapper[prev_new_session_handle].next = new_session_handle;
}

/**************************************************************************
 * FUNCTION NAME : hil_delete_session_handle_in_ct_mapper_list
 **************************************************************************
 * DESCRIPTION   : the function deletes the session handle from the ct_mapper's list
 * param - del_session_handle - the session handle to delete from the ct_mapper
 * dir - the connection tracking tuple's direction (could be either IP_CT_ORIGINAL/REPLY)
 * return - void
 **************************************************************************/
static void hil_delete_session_handle_in_ct_mapper_list (Uint16 del_session_handle,enum ip_conntrack_dir dir)
{
    Uint16 next_session;
    Uint16 previous_session;
    
    /*if the current ti_pp_session_handle of the conntrack points to session_handle then we need to change its value to a different session handle 
      since the current session_handle will be removed from the list shortly*/
    if(hil_session_ct_mapper[del_session_handle].conntrack->tuplehash[dir].ti_pp_session_handle == del_session_handle)
           hil_session_ct_mapper[del_session_handle].conntrack->tuplehash[dir].ti_pp_session_handle = hil_session_ct_mapper[del_session_handle].next;

    /*update the list */
    next_session = hil_session_ct_mapper[del_session_handle].next;
    previous_session = hil_session_ct_mapper[del_session_handle].previous;
    hil_session_ct_mapper[next_session].previous = previous_session;
    hil_session_ct_mapper[previous_session].next = next_session;
	
    /*update current entry*/
    hil_session_ct_mapper[del_session_handle].next = TI_PP_SESSION_CT_IDLE;
	hil_session_ct_mapper[del_session_handle].previous = TI_PP_SESSION_CT_IDLE;
    hil_session_ct_mapper[del_session_handle].conntrack = NULL;

    return;
}

#endif /*CONFIG_NETFILTER*/
/**************************************************************************
 * FUNCTION NAME : ti_hil_pp_event_handler
 **************************************************************************
 * DESCRIPTION   :
 *  The function handles all the events generated by the PP PDSP.
 **************************************************************************/
static void ti_hil_pp_event_handler (int event, unsigned int param1, unsigned int param2)
{
    /* Process the event accordingly... */
    switch (event)
    {
        case TI_PP_SESSION_EXPIRATION:
        {
            /* Session has EXPIRED; param1 is the session handle that has expired. */
            TI_PP_SESSION_STATS    session_stats;

            /* Timer has expired for a session; time to delete the session. */
            if (ti_ppm_delete_session (param1, &session_stats) < 0)
            {
                printk ("Error: Unable to delete session %d\n", param1);
                return;
            }

            if (global_ti_hil_db.tdoxDbg)
            {
                printk("TDOX-DBG DeleteSession: jiffies=%ld, ses=%d\n", jiffies, param1);
            }

#ifdef CONFIG_TI_HIL_DEBUG
            if (0 == global_ti_hil_db.dbg_disabled)
            {
                /* Print the session stats. */
                printk ("--------- Session Stats %d ---------\n", param1);
                printk ("Number of Packets Forwarded: %u\n", session_stats.packets_forwarded);
                printk ("Number of Bytes   Forwarded: %u\n", session_stats.bytes_forwarded_hi);
                printk ("Number of Bytes   Forwarded: %u\n", session_stats.bytes_forwarded_lo);
                printk ("------------------------------------\n");
            }
#endif /* CONFIG_TI_HIL_DEBUG */
            break;
        }

        default:
        {
            /* Unknown PP Event Generated! */
            printk ("PP Generated Event 0x%x which is UNHANDLED\n", event);
            break;
        }
    }
    /* Work is done! */
    return;
}

/**************************************************************************
 * FUNCTION NAME : ti_hil_pp_ppd_event_handler
 **************************************************************************
 * DESCRIPTION   :
 *  The function handles all the events generated by the PPD part of the
 *  Packet Processor Subsystem.
 **************************************************************************/
static void ti_hil_pp_ppd_event_handler (int event, unsigned int param1, unsigned int param2)
{
    switch (event)
    {
        case TI_PPD_TDOX_EVALUATION:
        {
            Uint32 sessHandle;
            TI_PP_SESSION session;

            if (global_ti_hil_db.tdox_disabled == 1)
            {
                return;
            }

            for (sessHandle = 0; sessHandle < TI_PP_MAX_ACCLERABLE_SESSIONS; sessHandle++)
            {
                if (ti_ppm_get_session_info(sessHandle, &session))
                {
                    continue;
                }

                if (IsTdoxCandidate(&session))
                {
                    TI_PP_VPID      vpid;
                    TI_PP_PID       pid;
                    Int32  currentTime;
                    Int32  deltaTimeMsec;
                    TI_PP_SESSION_STATS   sessionStats;
                    Uint32 averagePktSize;
                    Uint32 packetsPerSecond;

                    ti_ppm_get_vpid_info (session.egress[0].vpid_handle, &vpid);
                    ti_ppm_get_pid_info  (vpid.parent_pid_handle, &pid);
                    if (TI_PP_PID_TYPE_DOCSIS != pid.type)
                    {
                        /* TDOX can be assigned only for US traffic */
                        continue;
                    }

                    if (hilSessionDb[sessHandle].tdoxHandle != -1)
                    {
                        if (0 == ti_ppd_session_is_tdox_set(sessHandle))
                        {
                            /* This is TDOX session that has been deprecated by the PP FW. The associated TDOX ID need to be freed */
                            ti_hil_tdox_free_session(sessHandle, hilSessionDb[sessHandle].tdoxHandle);
                            session.egress[0].app_specific_data.u.app_desc.enables &= ~TI_HIL_TDOX_ENABLED;
                            hilSessionDb[sessHandle].tdoxSwaps++;
                            continue;
                        }
                    }

                    currentTime = jiffies;
                    deltaTimeMsec = ((currentTime - hilSessionDb[sessHandle].lastUpdateTime) * 1000) / HZ;  /* Scale down the delta to milliseconds */

                    if ((hilSessionDb[sessHandle].lastUpdateTime != 0) && (deltaTimeMsec < global_ti_hil_db.tdoxEvalMinTimeMsec))
                    {
                        /* Resolution for checking the traffic is at least tdoxEvalMinTime */
                        continue;
                    }

                    /* Get the session statistics */
                    if (ti_ppm_get_session_stats(sessHandle, &sessionStats) != 0)
                    {
                        /* Could not get the statistics for this session */
                        continue;
                    }

                    hilSessionDb[sessHandle].pktDelta = sessionStats.packets_forwarded - hilSessionDb[sessHandle].pktForward;
                    hilSessionDb[sessHandle].bytesDelta = sessionStats.bytes_forwarded_lo - hilSessionDb[sessHandle].byteForward;
                    hilSessionDb[sessHandle].timeDelta = deltaTimeMsec;
                    hilSessionDb[sessHandle].pktForward = sessionStats.packets_forwarded;
                    hilSessionDb[sessHandle].byteForward = sessionStats.bytes_forwarded_lo;

                    if (hilSessionDb[sessHandle].lastUpdateTime == 0)
                    {
                        if (global_ti_hil_db.tdoxDbg)
                        {
                            printk("TDOX-DBG Update: ses=%d, lastUpdateTime=%d, currentTime=%d, deltaTimeMsec=%d, packets_forwarded=%d, pktDelta=%d, byteForward=%d, bytesDelta=%d\n",
                               sessHandle, hilSessionDb[sessHandle].lastUpdateTime, currentTime, 0, sessionStats.packets_forwarded, hilSessionDb[sessHandle].pktDelta, sessionStats.bytes_forwarded_lo, hilSessionDb[sessHandle].bytesDelta);
                        }
                        /* If this is the first time, just update current time and wait to next iteration */
                        hilSessionDb[sessHandle].lastUpdateTime = currentTime;
                        continue;
                    }
                    if (global_ti_hil_db.tdoxDbg)
                    {
                        printk("TDOX-DBG Update: ses=%d, lastUpdateTime=%d, currentTime=%d, deltaTimeMsec=%d, packets_forwarded=%d, pktDelta=%d, byteForward=%d, bytesDelta=%d\n",
                               sessHandle, hilSessionDb[sessHandle].lastUpdateTime, currentTime, deltaTimeMsec, sessionStats.packets_forwarded, hilSessionDb[sessHandle].pktDelta, sessionStats.bytes_forwarded_lo, hilSessionDb[sessHandle].bytesDelta);
                    }
                    hilSessionDb[sessHandle].lastUpdateTime = currentTime;

                    if (sessionStats.packets_forwarded < 100)
                    {
                        /* Do not do any logic on the session if it is only established */
                        continue;
                    }

                    averagePktSize = hilSessionDb[sessHandle].pktDelta ? (hilSessionDb[sessHandle].bytesDelta / hilSessionDb[sessHandle].pktDelta) : 0;
                    if (hilSessionDb[sessHandle].pktDelta < 0xFFFFFFFF / 1000)
                    {
                        packetsPerSecond = (hilSessionDb[sessHandle].pktDelta * 1000) / deltaTimeMsec;
                    }
                    else
                    {
                        packetsPerSecond = (hilSessionDb[sessHandle].pktDelta / deltaTimeMsec) * 1000;
                    }

                    if (hilSessionDb[sessHandle].tdoxHandle != -1)
                    {
                        /* This is a TDOX session - check for conditions to disable TDOX on this session */
                        if ((packetsPerSecond < global_ti_hil_db.tdoxEvalPpsThresh) || (averagePktSize > global_ti_hil_db.tdoxEvalAvgPktSizeThresh))
                        {
                            if (global_ti_hil_db.tdoxDbg)
                            {
                                printk("TDOX-DBG FreeTdox (Evaluation): jiffies=%ld, ses=%d, tdox=%d, PPS=%d, PktSize=%d\n", jiffies, sessHandle, hilSessionDb[sessHandle].tdoxHandle, packetsPerSecond, averagePktSize);
                            }

                            ti_hil_tdox_free_session(sessHandle, hilSessionDb[sessHandle].tdoxHandle);
                            session.egress[0].app_specific_data.u.app_desc.enables &= ~TI_HIL_TDOX_ENABLED;
                            ti_ppd_session_tdox_change(sessHandle, hilSessionDb[sessHandle].tdoxHandle, session.egress[0].app_specific_data.u.app_desc.enables);
                            hilSessionDb[sessHandle].tdoxSwaps++;
                        }
                    }
                    else
                    {
                        /* This is not a TDOX session - check for conditions to enable TDOX on this session */
                        if ((packetsPerSecond >= global_ti_hil_db.tdoxEvalPpsThresh) && (averagePktSize <= global_ti_hil_db.tdoxEvalAvgPktSizeThresh))
                        {
                            Int32 tdox_ID = -1;

                            tdox_ID = ti_hil_tdox_alloc_session();

                            if (tdox_ID != -1)
                            {
                                session.egress[0].app_specific_data.u.app_desc.enables |= TI_HIL_TDOX_ENABLED;
                                session.egress[0].app_specific_data.u.app_desc.u.raw_app_info1_b3 = tdox_ID;
                                if (global_ti_hil_db.tdoxDbg)
                                {
                                    printk("TDOX-DBG AssociateTdox (Evaluation): jiffies=%ld, ses=%d, tdox=%d, PPS=%d, PktSize=%d\n", jiffies, sessHandle, tdox_ID, packetsPerSecond, averagePktSize);
                                }
                                ti_hil_tdox_associate_session(tdox_ID, sessHandle);
                                ti_ppd_session_tdox_change(sessHandle, tdox_ID, session.egress[0].app_specific_data.u.app_desc.enables);
                                hilSessionDb[sessHandle].tdoxSwaps++;
                            }
                        }
                    }
                }
            }
            break;
        }

        default:
        {
            /* Unknown PPD Event Generated! */
            printk ("PPD Generated Event 0x%x which is UNHANDLED\n", event);
            break;
        }
    }
    /* Work is done! */

    return;
}


/**************************************************************************
 * FUNCTION NAME : ti_hil_ppsubsystem_event_handler
 **************************************************************************
 * DESCRIPTION   :
 *  The function is the registered event handler which listens to all events
 *  that arise from the Packet Processor Subsystem.
 **************************************************************************/
static void ti_hil_ppsubsystem_event_handler(unsigned int event_id, unsigned int param1, unsigned int param2)
{
    int module;
    int subsystem;

    /* We should only get events generated by the PP Sub-System. */
    subsystem = event_id & TI_PP_SUBSYSTEM_BIT_MASK;
    if (subsystem != 0)
    {
        printk ("FATAL Error: Event 0x%x violates the specification\n", event_id);
        return;
    }

    /* Get information on the module which generated the event and the actual event identifier. */
    module = event_id & TI_PP_MODULE_BIT_MASK;

    /* Process each event based on the module which generated the event. */
    switch (module)
    {
        case TI_PP_MODULE:
        {
            /* Packet Processor Event */
            ti_hil_pp_event_handler (event_id, param1, param2);
            break;
        }
        case TI_PPM_MODULE:
        {
            /* Packet Processor Manager Event. */
            ti_hil_pp_ppm_event_handler (event_id, param1, param2);
            break;
        }
        case TI_PPD_MODULE:
        {
            /* Packet Processor Driver Event. */
            ti_hil_pp_ppd_event_handler (event_id, param1, param2);
            break;
        }
        default:
        {
            printk ("FATAL Error: Event 0x%x violates the specification\n", event_id);
            break;
        }
    }

    /* Our work is done. */
    return;
}

#ifdef CONFIG_IP_MULTICAST

/**************************************************************************
 * FUNCTION NAME : hil_mfc_check_entry
 **************************************************************************
 * DESCRIPTION   :
 *  The function checks the HIL MFC Database for a "session id" match for
 *  a particular mcast_group. The database keeps track of all MFC Entries
 *  that have been accelerated.
 *
 * RETURNS       :
 *  Handle of the session     - Match.
 *  -1                        - No Match.
 ***************************************************************************/
static int hil_mfc_check_entry (unsigned int mcast_group)
{
    int session_handle = 0;

    /* Cycle through all the entries */
    for (session_handle = 0; session_handle < TI_PP_MAX_ACCLERABLE_SESSIONS; session_handle++)
    {
        /* Check if there is a valid entry or not? */
        if (hil_mfc_session_mapper[session_handle] != 0)
        {
            /* YES. Match the entry to the multicast group specified. */
            if (hil_mfc_session_mapper[session_handle] == mcast_group)
            {
                /* Perfect Match: Return the PP Session handle */
                return session_handle;
            }
        }
    }

    /* Control comes here implies that there was no match! */
    return -1;
}

/**************************************************************************
 * FUNCTION NAME : hil_mfc_add_entry
 **************************************************************************
 * DESCRIPTION   :
 *  The function adds an entry to the Session to MFC Database
 *
 * RETURNS       :
 *  0     - Success.
 *  <0    - Error.
 ***************************************************************************/
static int hil_mfc_add_entry (unsigned int mcast_group, unsigned char session_handle)
{
    /* Check if the slot is free! */
    if (hil_mfc_session_mapper[session_handle] != 0)
    {
        /* No; slot is not free!*/
        printk ("Session Handle %d already mapped to group: 0x%x\n", session_handle, hil_mfc_session_mapper[session_handle]);
        return -1;
    }

    /* Map and return. */
    hil_mfc_session_mapper[session_handle] = mcast_group;
    return 0;
}

/**************************************************************************
 * FUNCTION NAME : hil_mfc_del_entry
 **************************************************************************
 * DESCRIPTION   :
 *  The function deletes an entry from the Session to MFC Database
 *
 * RETURNS       :
 *  Session Handle     - Success.
 *  <0                 - Error.
 ***************************************************************************/
static int hil_mfc_del_entry (unsigned int mcast_group)
{
    int session_handle = hil_mfc_check_entry(mcast_group);

    /* Check if the session handle exists? */
    if (session_handle == -1)
    {
        /* No Matching entry found. */
        printk ("Multicast group 0x%x not mapped\n", mcast_group);
        return -1;
    }

    /* Unmap and return. */
    hil_mfc_session_mapper[session_handle] = 0;
    return session_handle;
}

/**************************************************************************
 * FUNCTION NAME : hil_mfc_to_session
 **************************************************************************
 * DESCRIPTION   :
 *  The function converts an MFC Entry used for multicast routing to a PP
 *  session structure.
 *
 * RETURNS       :
 *  -1      - Error.
 *  0       - Success.
 ***************************************************************************/
static int hil_mfc_to_session(struct pp_mr_param * ptr_mr_param, TI_PP_SESSION* ptr_session)
{
    int downstream_vif_index;

    /* Initialize the session structure */
    memset ((void *)ptr_session, 0, sizeof(TI_PP_SESSION));

    /* Configure the session timeout. For MFC we can create a static session; this is added till we
     * get the new header file; since we need to configure the session flag correctly. */
    ptr_session->session_timeout = 0;

    /* This is for sure a ROUTED SESSION. */
    ptr_session->is_routable_session = 1;

    /* Configure the Ingress Session Properties. We need to know from which VPID the packet will
     * be arriving. This can be retrieved from the UPSTREAM VIF Index.But we would need to convert
     * the VIF Index to VPID before we can do this. */
    if (ptr_mr_param->vif_table[ptr_mr_param->cache->mfc_parent].dev != NULL)
    {
        /* Check if the VPID has a VPID handle or not? If not then we cannot create a session in the PP
         * since the device does not belong to the Packet processor subsystem. */
        if (ptr_mr_param->vif_table[ptr_mr_param->cache->mfc_parent].dev->vpid_handle == -1)
            return -1;
    }
    else
    {
        printk ("FATAL Error: VIF Index %d does not map to a device\n", ptr_mr_param->cache->mfc_parent);
        return -1;
    }

    /* Configure the Ingress Session Properties. */
    ptr_session->ingress.vpid_handle           = ptr_mr_param->vif_table[ptr_mr_param->cache->mfc_parent].dev->vpid_handle;
    ptr_session->ingress.l2_packet.packet_type = TI_PP_ETH_TYPE;

    /* Configuration of the Destination MAC Address; we could take two approaches here!
     *  a) Have a wild carded destination MAC Address
     *  b) Convert the Multicast IPv4 address to a L2 address and configure it.
     * Approach (b) seems to be a more robust solution.
     * We make sure that the SOURCE MAC Address is WILDCARDED! Since at this time we dont
     * know from where the packet is arriving. */
    ip_eth_mc_map (ptr_mr_param->cache->mfc_mcastgrp, &ptr_session->ingress.l2_packet.u.eth_desc.dstmac[0]);
    ptr_session->ingress.l2_packet.u.eth_desc.enables = TI_PP_SESSION_L2_DSTMAC_VALID;

    /* Configure the LUT Entry for Layer3 Classification.
     * The following fields are WILDCARDED @ Layer3
     *  a) Source IP Address
     *  b) Protocol
     *  c) TOS
     *  f) Source Port
     *  g) Destination Port
     * This information is not known @ the time the MFC entry is created. */
    ptr_session->ingress.l3l4_packet.packet_type        = TI_PP_IPV4_TYPE;
    ptr_session->ingress.l3l4_packet.u.ipv4_desc.dst_ip = ptr_mr_param->cache->mfc_mcastgrp;
    ptr_session->ingress.l3l4_packet.u.eth_desc.enables = TI_PP_SESSION_IPV4_DSTIP_VALID;

    /* Configure the Egress Session Properties.
     *   - Multicast packets are not subjected to any NAT and so there are no modifications
     *     which need to be done at Layer3 or Layer4. Thus all we populate in the Egress
     *     Session Properties are the Egress VPID on which the packets need to be sent out. */
    for (downstream_vif_index=0; downstream_vif_index < MAXVIFS; downstream_vif_index++)
    {
        /* Check if a valid VIF is present or not? */
        if (ptr_mr_param->cache->mfc_un.res.ttls[downstream_vif_index] != 0xFF)
        {
            /* Found it! Get the device information from the VIF. */
            if (ptr_mr_param->vif_table[downstream_vif_index].dev != NULL)
            {
                /* Now we need to get the VPID handle. If the VPID handle does not exist then the packet
                 * is being transmitted on an interface which does not support the packet processor and
                 * thus this session is not capable of being accelerated! */
                if (ptr_mr_param->vif_table[downstream_vif_index].dev->vpid_handle == -1)
                    return -1;

                /* Remember the Egress VPID handle. */
                ptr_session->egress[ptr_session->num_egress++].vpid_handle = ptr_mr_param->vif_table[downstream_vif_index].dev->vpid_handle;
            }
            else
            {
                printk ("FATAL Error: VIF Index %d does not map to a device\n", downstream_vif_index);
                return -1;
            }
        }
    }

    /* Once we come out of the loop we should have at least one egress record. */
    if (ptr_session->num_egress == 0)
    {
        printk ("No downstream interface for group 0x%x\n", ptr_mr_param->cache->mfc_mcastgrp);
        return -1;
    }

    /* Successfully translated. */
    return 0;
}

#endif /* CONFIG_IP_MULTICAST */

/**************************************************************************
 * FUNCTION NAME : ti_hil_intrusive_display_l2
 **************************************************************************
 * DESCRIPTION   :
 *  The function prints the Layer2 Information
 **************************************************************************/
static void ti_hil_intrusive_display_l2(TI_PP_SESSION_PROPERTY* sessionInfo, Bool isIngress)
{
    TI_PP_ETH_DESC* ptr_eth_desc = &sessionInfo->l2_packet.u.eth_desc;
    TI_PP_VPID      vpid;
    Bool doClassify = False;

    /* Check if the Destination MAC Address has been specified. */
    if (ptr_eth_desc->enables & TI_PP_SESSION_L2_DSTMAC_VALID)
    {
        PRINTK_CLI ("Dst. MAC     = %02x:%02x:%02x:%02x:%02x:%02x\n", ptr_eth_desc->dstmac[0], ptr_eth_desc->dstmac[1], // ARRIS MOD
                ptr_eth_desc->dstmac[2], ptr_eth_desc->dstmac[3], ptr_eth_desc->dstmac[4], ptr_eth_desc->dstmac[5]);
    }
    else
    {
        PRINTK_CLI ("Dst. MAC     = XXX\n"); // ARRIS MOD
    }

    /* Check if the Source MAC Address has been specified. */
    if (ptr_eth_desc->enables & TI_PP_SESSION_L2_SRCMAC_VALID)
    {
        PRINTK_CLI ("Src. MAC     = %02x:%02x:%02x:%02x:%02x:%02x\n", ptr_eth_desc->srcmac[0], ptr_eth_desc->srcmac[1], // ARRIS MOD
                ptr_eth_desc->srcmac[2], ptr_eth_desc->srcmac[3], ptr_eth_desc->srcmac[4], ptr_eth_desc->srcmac[5]);
    }
    else
    {
        PRINTK_CLI ("Src. MAC     = XXX\n");//ARRIS MOD
    }


    ti_ppm_get_vpid_info (sessionInfo->vpid_handle, &vpid);

    if (vpid.type == TI_PP_VLAN)
    {
        if (isIngress)
        {
            if (!(ptr_eth_desc->enables & TI_PP_SESSION_L2_VLAN_VALID))
            {
                doClassify = True;
            }
        }
        PRINTK_CLI ("VLAN (VPID)  = 0x%04x %s\n", vpid.vlan_identifier, doClassify ? "[Clsf]" : ""); // ARRIS MOD
    }
    if (ptr_eth_desc->enables & TI_PP_SESSION_L2_VLAN_VALID)
    {
        if (isIngress)
        {
            doClassify = True;
        }
        PRINTK_CLI ("VLAN (PKT)   = 0x%04x %s\n", ptr_eth_desc->vlan_tag, doClassify ? "[Clsf]" : ""); // ARRIS MOD
    }

    /* Work is completed. */
    return;
}

/**************************************************************************
 * FUNCTION NAME : ti_hil_intrusive_display_ipv4
 **************************************************************************
 * DESCRIPTION   :
 *  The function prints the IPv4 Information
 **************************************************************************/
static void ti_hil_intrusive_display_ipv4 (TI_PP_IPV4_DESC* ptr_ipv4_lut_entry)
{
    /* Check if the Destination IP has been specified. */
    if (ptr_ipv4_lut_entry->enables & TI_PP_SESSION_IPV4_DSTIP_VALID)
    {
        unsigned char * ipPtr = (unsigned char *)&ptr_ipv4_lut_entry->dst_ip;
        PRINTK_CLI ("Dst IP       = 0x%08X [ %d.%d.%d.%d ]\n", ptr_ipv4_lut_entry->dst_ip, ipPtr[0], ipPtr[1], ipPtr[2], ipPtr[3]);//ARRIS MOD
    }
    else
        PRINTK_CLI ("Dst IP       = XXX\n");//ARRIS MOD

    /* Check if the Source IP has been specified. */
    if (ptr_ipv4_lut_entry->enables & TI_PP_SESSION_IPV4_SRCIP_VALID)
    {
        unsigned char * ipPtr = (unsigned char *)&ptr_ipv4_lut_entry->src_ip;
        PRINTK_CLI ("Src IP       = 0x%08X [ %d.%d.%d.%d ]\n", ptr_ipv4_lut_entry->src_ip, ipPtr[0], ipPtr[1], ipPtr[2], ipPtr[3]);//ARRIS MOD
    }
    else
        PRINTK_CLI ("Src IP       = XXX\n"); //ARRIS MOD

    /* Check if the protocol has been specified. */
    if (ptr_ipv4_lut_entry->enables & TI_PP_SESSION_IPV4_PROTOCOL_VALID)
    {
        PRINTK_CLI ("Protocol     = 0x%02X ", ptr_ipv4_lut_entry->protocol);//ARRIS MOD
        switch (ptr_ipv4_lut_entry->protocol)
        {
        case IPPROTO_IPIP:
            PRINTK_CLI("[IPinIP]\n"); // ARRIS MOD
            break;
        case IPPROTO_TCP:
            PRINTK_CLI("[TCP]\n");// ARRIS MOD
            break;
        case IPPROTO_UDP:
            PRINTK_CLI("[UDP]\n");//ARRIS MOD
            break;
        case IPPROTO_GRE:
            PRINTK_CLI("[GRE]\n"); //ARRIS MOD
            break;
        case IPPROTO_IPV6:
            PRINTK_CLI("[IPv6-in-IPv4 tunnelling]\n");//ARRIS MOD
            break;
        default:
            PRINTK_CLI("[???]\n");//ARRIS MOD
            break;
        }
    }
    else
    {
        PRINTK_CLI ("Protocol     = XXX\n");//ARRIS MOD
    }

    /* Check if the TOS Byte has been specified. */
    if (ptr_ipv4_lut_entry->enables & TI_PP_SESSION_IPV4_TOS_VALID)
        PRINTK_CLI ("TOS          = 0x%02X\n", ptr_ipv4_lut_entry->tos);//ARRIS MOD
    else
        PRINTK_CLI ("TOS          = XXX\n");//ARRIS MOD

    /* Check if the Destination Port has been specified. */
    if (ptr_ipv4_lut_entry->enables & TI_PP_SESSION_IPV4_DST_PORT_VALID)
        PRINTK_CLI ("Dst Port     = 0x%04X [%d]\n", ptr_ipv4_lut_entry->dst_port, ptr_ipv4_lut_entry->dst_port);//ARRIS MOD
    else
        PRINTK_CLI ("Dst Port     = XXX\n");//ARRIS MOD

    /* Check if the Source Port has been specified. */
    if (ptr_ipv4_lut_entry->enables & TI_PP_SESSION_IPV4_SRC_PORT_VALID)
        PRINTK_CLI ("Src Port     = 0x%04X [%d]\n", ptr_ipv4_lut_entry->src_port, ptr_ipv4_lut_entry->src_port);//ARRIS MOD
    else
        PRINTK_CLI ("Src Port     = XXX\n"); //ARRIS MOD

    /* Work is completed. */
    return;
}

/**************************************************************************
 * FUNCTION NAME : ti_hil_intrusive_display_ipv6
 **************************************************************************
 * DESCRIPTION   :
 *  The function prints the IPv4 Information
 **************************************************************************/
static void ti_hil_intrusive_display_ipv6 (TI_PP_IPV6_DESC*   ptr_ipv6_lut_entry)
{
    char ipv6addr[64];
    const char* buf;

    /* Check if the Destination IP has been specified. */
    if (ptr_ipv6_lut_entry->enables & TI_PP_SESSION_IPV6_DSTIP_VALID)
    {
        buf = ti_hil_intrusive_display_ipv6_addr(ptr_ipv6_lut_entry->dst_ip, ipv6addr, sizeof(ipv6addr));
        if (buf != NULL)
        {
            PRINTK_CLI ("Dst IP       = %s\n", buf);//ARRIS MOD
        }
        else
        {
            PRINTK_CLI ("Dst IP       = XXX\n");//ARRIs MOD
        }
    }
    else
    {
        PRINTK_CLI ("Dst IP       = XXX\n");//ARRIS MOD
    }

    /* Check if the Source IP has been specified. */
    if (ptr_ipv6_lut_entry->enables & TI_PP_SESSION_IPV6_SRCIP_VALID)
    {
        buf = ti_hil_intrusive_display_ipv6_addr(ptr_ipv6_lut_entry->src_ip, ipv6addr, sizeof(ipv6addr));
        if (buf != NULL)
        {
            PRINTK_CLI ("Src IP       = %s\n", buf);//ARRIS MOD
        }
        else
        {
            PRINTK_CLI ("Src IP       = XXX\n"); //ARRIS MOD
        }
    }
    else
    {
        PRINTK_CLI ("Src IP       = XXX\n");//ARRIS MOD
    }

    /* Check if the next header has been specified. */
    if (ptr_ipv6_lut_entry->enables & TI_PP_SESSION_IPV6_NEXTHDR_VALID)
    {
        PRINTK_CLI ("Protocol     = 0x%02X ", ptr_ipv6_lut_entry->next_header);//ARRIS MOD
        switch (ptr_ipv6_lut_entry->next_header)
        {
        case IPPROTO_IPIP:
            PRINTK_CLI("[IPinIP]\n");//ARRIS MOD
            break;
        case IPPROTO_TCP:
            PRINTK_CLI("[TCP]\n");//ARRIS MOD
            break;
        case IPPROTO_UDP:
            PRINTK_CLI("[UDP]\n");//ARRIS MOD
            break;
        case IPPROTO_GRE:
            PRINTK_CLI("[GRE]\n");//ARRIS MOD
            break;
        case IPPROTO_IPV6:
            PRINTK_CLI("[IPv6-in-IPv4 tunnelling]\n");// ARRIS MOD
            break;
        default:
            PRINTK_CLI("[???]\n");//ARRIS MOD
            break;
        }
    }
    else
    {
        PRINTK_CLI ("Protocol     = XXX\n");//ARRIS MOD
    }

    /* Check if the Traffic Class Byte has been specified. */
    if (ptr_ipv6_lut_entry->enables & TI_PP_SESSION_IPV6_TRCLASS_VALID)
        PRINTK_CLI ("TOS          = 0x%02X\n", ptr_ipv6_lut_entry->traffic_class);//ARRIS MOD
    else
        PRINTK_CLI ("TOS          = XXX\n");//ARRIS MOD

    /* Check if the Destination Port has been specified. */
    if (ptr_ipv6_lut_entry->enables & TI_PP_SESSION_IPV6_DST_PORT_VALID)
        PRINTK_CLI ("Dst Port     = 0x%04X [%d]\n", ptr_ipv6_lut_entry->dst_port, ptr_ipv6_lut_entry->dst_port);//ARRIS MOD
    else
        PRINTK_CLI ("Dst Port     = XXX\n");//ARRIS MOD

    /* Check if the Source Port has been specified. */
    if (ptr_ipv6_lut_entry->enables & TI_PP_SESSION_IPV6_SRC_PORT_VALID)
        PRINTK_CLI ("Src Port     = 0x%04X [%d]\n", ptr_ipv6_lut_entry->src_port, ptr_ipv6_lut_entry->src_port);//ARRIS MOD
    else
        PRINTK_CLI ("Src Port     = XXX\n");//ARRIS MOD

    if (ptr_ipv6_lut_entry->enables & TI_PP_SESSION_IPV6_DSLITE_DSTIP_VALID)
    {
        unsigned char * ipPtr = (unsigned char *)&ptr_ipv6_lut_entry->dsLite_dst_ip;
        PRINTK_CLI ("DsLite DstIP = 0x%08x [ %d.%d.%d.%d ]\n", ptr_ipv6_lut_entry->dsLite_dst_ip, ipPtr[0], ipPtr[1], ipPtr[2], ipPtr[3]);//ARRIS MOD
    }

    /* Work is completed. */
    return;
}

/**************************************************************************
 * FUNCTION NAME : ti_hil_intrusive_display_ipv6_addr
 **************************************************************************
 * DESCRIPTION   :
 *  The function converts a numeric address into a text string suitable for presentation.
 **************************************************************************/
static const char* ti_hil_intrusive_display_ipv6_addr (const void *cp, char *buf, size_t len)
{
    size_t xlen;

    const struct in6_addr *s = (const struct in6_addr *)cp;

    xlen = snprintf(buf, len, "%x:%x:%x:%x:%x:%x:%x:%x",
                    ntohs(s->s6_addr16[0]), ntohs(s->s6_addr16[1]),
                    ntohs(s->s6_addr16[2]), ntohs(s->s6_addr16[3]),
                    ntohs(s->s6_addr16[4]), ntohs(s->s6_addr16[5]),
                    ntohs(s->s6_addr16[6]), ntohs(s->s6_addr16[7]));

    if (xlen > len)
    {
        return NULL;
    }

    return buf;
}

/**************************************************************************
 * FUNCTION NAME : hil_mfc_delete_session_by_mc_group
 **************************************************************************
 * DESCRIPTION   :
 *  The function deletes a session by a multicast group identifier
 **************************************************************************/

static int hil_mfc_delete_session_by_mc_group(int mc_group)
{
    int session_handle ;
    TI_PP_SESSION    *session = (TI_PP_SESSION *)kmalloc(sizeof(TI_PP_SESSION), GFP_KERNEL);
    if (session == NULL)
    {
        printk(KERN_ERR "%s kmalloc failed\n",__FUNCTION__);
        return -1;
    }
    /* Cycle through all the entries */
    for (session_handle = TI_PP_MAX_ACCLERABLE_SESSIONS-1; session_handle > -1; session_handle--)
    {
        /* Get the pointer to the Session Information. */
        if (ti_ppm_get_session_info(session_handle, session) < 0)
            {
            continue;
            }
        if (session->ingress.l3l4_packet.u.ipv4_desc.dst_ip == mc_group)
        {
            ti_ppm_delete_session(session_handle, NULL);

            if (global_ti_hil_db.tdoxDbg)
            {
                printk("TDOX-DBG DeleteSession: jiffies=%ld, ses=%d\n", jiffies, session_handle);
            }
            kfree(session);
            return 0;
        }

    }

    kfree(session);

    return -1;

}

/**************************************************************************
 * FUNCTION NAME : ti_hil_intrusive_display_session
 **************************************************************************
 * DESCRIPTION   :
 *  The function displays the session properties.
 **************************************************************************/
static void ti_hil_intrusive_display_session (int session_handle)
{
    TI_PP_SESSION    session;
    int              index;
    TI_PP_VPID      vpid;
    TI_PP_PID       pid;

    /* Get the pointer to the Session Information. */
    if (ti_ppm_get_session_info(session_handle, &session) < 0)
    {
        return;
    }
	//ARRIS MOD START
    /* Print the Session Parameters. */
    PRINTK_CLI ("Property : %s\n", session.is_routable_session ? "Routed": "Bridged");
    PRINTK_CLI ("Timeout  : %d sec\n", session.session_timeout / 1000000);
    PRINTK_CLI ("Priority : %d\n", session.priority);

    /* Print the Ingress Session Properties; which include the L2/L3 and L4 properties. */
    PRINTK_CLI ("\nIngress Properties");
	//ARRIS MOD END
    if (session.ingress.l2_packet.u.eth_desc.enables & TI_PP_SESSION_L2_GRE_DS_VALID)
    {
        PRINTK_CLI(" (DS GRE - The ingress properties are of the encapsulated packet)");//ARRIS MOD
    }
    PRINTK_CLI ("\n------------------\n");//ARRIS MOD

    ti_ppm_get_vpid_info (session.ingress.vpid_handle, &vpid);
    ti_ppm_get_pid_info  (vpid.parent_pid_handle, &pid);
    if (TI_PP_PID_TYPE_DOCSIS == pid.type)
    {
        PRINTK_CLI ("Ingress VPID = %d [CNI]\n", session.ingress.vpid_handle);//ARRIS MOD
    }
    else if (TI_PP_PID_TYPE_ETHERNET == pid.type)
    {
        PRINTK_CLI ("Ingress VPID = %d [ETH]\n", session.ingress.vpid_handle);//ARRIS MOD
    }
    else if (TI_PP_PID_TYPE_ETHERNETSWITCH == pid.type)
    {
        PRINTK_CLI ("Ingress VPID = %d [ETH Switch]\n", session.ingress.vpid_handle);//ARRIS MOD
    }
    else if (TI_PP_PID_TYPE_INFRASTRUCTURE == pid.type)
    {
        PRINTK_CLI ("Ingress VPID = %d [VOICE DSP]\n", session.ingress.vpid_handle);//ARRIS MOD
    }
    else
    {
        PRINTK_CLI ("Ingress VPID = %d\n", session.ingress.vpid_handle);//ARRIS MOD
    }

    if (session.ingress.l2_packet.packet_type == TI_PP_ETH_TYPE)
    {
        ti_hil_intrusive_display_l2(&session.ingress, True);
    }
    else
    {
        PRINTK_CLI ("No L2 Properties\n");//ARRIS MOD
    }
    if (session.ingress.l3l4_packet.packet_type == TI_PP_IPV4_TYPE)
    {
        PRINTK_CLI ("IP Version   = IPv4\n");//ARRIS MOD
        ti_hil_intrusive_display_ipv4 (&session.ingress.l3l4_packet.u.ipv4_desc);
    }
    else if (session.ingress.l3l4_packet.packet_type == TI_PP_IPV6_TYPE)
    {
        PRINTK_CLI ("IP Version   = IPv6\n");//ARRIS MOD
        ti_hil_intrusive_display_ipv6 (&session.ingress.l3l4_packet.u.ipv6_desc);
    }
    else
        PRINTK_CLI ("No L3/L4 Properties\n");//ARRIs MOD

    /* Cycle through all the Egress Session Properties. */
    for (index = 0; index < session.num_egress; index++)
    {
        PRINTK_CLI ("\nEgress Properties (%d)", index+1);//ARRIS MOD
        PRINTK_CLI ("\n---------------------\n");//ARRIs MOD

        /* Print the Ingress Session Properties; which include the L2/L3 and L4 properties. */
        ti_ppm_get_vpid_info (session.egress[index].vpid_handle, &vpid);
        ti_ppm_get_pid_info  (vpid.parent_pid_handle, &pid);
        if (TI_PP_PID_TYPE_DOCSIS == pid.type)
        {
            PRINTK_CLI ("Egress VPID  = %d [CNI]\n", session.egress[index].vpid_handle);//ARRIS MOD
        }
        else if (TI_PP_PID_TYPE_ETHERNET == pid.type)
        {
            PRINTK_CLI ("Egress VPID  = %d [ETH]\n", session.egress[index].vpid_handle);//ARRIS MOD
        }
        else if (TI_PP_PID_TYPE_ETHERNETSWITCH == pid.type)
        {
            PRINTK_CLI ("Egress VPID  = %d [ETH Switch]\n", session.egress[index].vpid_handle);//ARRIS MOD
        }
        else if (TI_PP_PID_TYPE_INFRASTRUCTURE == pid.type)
        {
            PRINTK_CLI ("Egress VPID  = %d [VOICE DSP]\n", session.egress[index].vpid_handle);//ARRIS MOD
        }
        else
        {
            PRINTK_CLI ("Egress VPID  = %d\n", session.egress[index].vpid_handle);//ARRIS MOD
        }

        if (session.egress[index].l2_packet.packet_type == TI_PP_ETH_TYPE)
        {
            ti_hil_intrusive_display_l2 (&session.egress[index], False);
        }
        else
        {
            PRINTK_CLI ("No L2 Properties\n");//ARRIS MOD
        }
        if (session.egress[index].l3l4_packet.packet_type == TI_PP_IPV4_TYPE)
        {
            PRINTK_CLI ("IP Version   = IPv4\n");//ARRIS MOD
            ti_hil_intrusive_display_ipv4 (&session.egress[index].l3l4_packet.u.ipv4_desc);
        }
        else if (session.egress[index].l3l4_packet.packet_type == TI_PP_IPV6_TYPE)
        {
            PRINTK_CLI ("IP Version   = IPv6\n");//ARRIS MOD
            ti_hil_intrusive_display_ipv6 (&session.egress[index].l3l4_packet.u.ipv6_desc);
        }
        else
            PRINTK_CLI ("No L3/L4 Properties\n");//ARRIS MOD
    }

    /* Work has been completed. */
    return;
}


/**************************************************************************
 * FUNCTION NAME : ti_hil_show_cmd_handler
 **************************************************************************
 * DESCRIPTION   :
 *  This function is the Packet Processor show command handler.
 *
 * RETURNS       :
 *  -1      - Error.
 *  0       - Success.
 ***************************************************************************/
static int ti_hil_show_cmd_handler(int argc, char* argv[])
{
    /****************************** VALIDATIONS ***************************/

    /* Validate the number of arguments that have been passed. */
    if (argc != 2 && argc != 3)
    {
        PRINTK_CLI ("ERROR: Incorrect Number of parameters passed.\n");//ARRIS MOD
        return -1;
    }

    /**************************** End of VALIDATIONS ***********************/

    if (strcmp(argv[1], "ver") == 0)
    {
        TI_PP_VERSION version;

        if (0 != ti_ppm_get_version( &version ))
        {
            PRINTK_CLI(" Version retrieve failed ... \n");//ARRIS MOD
        }
        else
        {
            PRINTK_CLI(" Packet Processor Firmware Version : %d.%d.%d.%d \n", version.v0, version.v1, version.v2, version.v3 );//ARRIS MOD
        }
        return 0;
    }


    if (strcmp(argv[1], "tdox") == 0)
    {
        ti_hil_tdox_print();
        return 0;
    }

     // ARRIS ADD START
    if (strcmp(argv[1], "cert") == 0)
    {
        if(certMode)
        {
            PRINTK_CLI(" Cert Mode Enabled ... \n");//ARRIS MOD
        }
        else
        {
            PRINTK_CLI(" Cert Mode Disabled ... \n");//ARRIS MOD
        }
        return 0;
    }
    // ARRIS ADD END


    /* Check if the "Global" stats were requested. */
    if (strcmp(argv[1], "global") == 0)
    {
        TI_PP_GLOBAL_STATS  pp_stats;

        /* YES. Get the global stats through the PPM */
        ti_ppm_get_global_stats(&pp_stats);

        /* Print the stats on the console. */
		//ARRIS MOD START
        PRINTK_CLI ("Packets received in the PP   : %u\n", pp_stats.packets_rxed);
        PRINTK_CLI ("Number of search attemps     : %u\n", pp_stats.packets_searched);
        PRINTK_CLI ("Number of matched searches   : %u\n", pp_stats.search_matched);
        PRINTK_CLI ("Number of Synch Delays       : %u\n", pp_stats.sync_delay);
        PRINTK_CLI ("Packet forwarded by the PP   : %u\n", pp_stats.packets_fwd);
        PRINTK_CLI ("IPv4 Packets Forwarded       : %u\n", pp_stats.ipv4_packets_fwd);
        PRINTK_CLI ("Descriptors Starved          : %u\n", pp_stats.desc_starved);
        PRINTK_CLI ("Buffers Starved              : %u\n", pp_stats.buffer_starved);
        //ARIS MOD END
        /* Work is completed. */
        return 0;
    }

    /* Check if VPID statistics were requested? */
    if (strcmp(argv[1], "vpid") == 0)
    {

        int         num_vpid;
        int         index = 0;
        int         size = (sizeof(TI_PP_VPID))*(TI_PP_MAX_PID + 1);
        TI_PP_VPID  *vpid = (TI_PP_VPID *)kmalloc(size, GFP_KERNEL);
        
        if (vpid == NULL)
        {
            PRINTK_CLI(KERN_ERR "%s kmalloc failed\n", __FUNCTION__); //ARRIS MOD START
            return 0;
        }

        /* Get a list of all VPID that exist in the System */
        num_vpid = ti_ppm_get_vpid (-1, TI_PP_MAX_PID, vpid);

        PRINTK_CLI ("-----------------------------------------\n");//ARRIS MOD
        PRINTK_CLI ("    HIL State is  : %s\n", (global_ti_hil_db.hil_disabled)?"Disabled":"Enabled" );//ARRIS MOD


        /* Cycle through all the VPID and get the stats */
        while (index < num_vpid)
        {
            TI_PP_VPID_STATS   vpid_stats;
            char * vpid_type;

            switch (vpid[index].type)
            {
                case TI_PP_ETHERNET     : vpid_type = "ETHERNET   " ; break;
                case TI_PP_VLAN         : vpid_type = "VLAN       " ; break;
                case TI_PP_PPPoE        : vpid_type = "PPPoE      " ; break;
                case TI_PP_VLAN_PPPoE   : vpid_type = "VLAN_PPPoE " ; break;
                default:                  vpid_type = "UNKNOWN"     ; break;
            }
            /* Get the VPID statistics. */
            ti_ppm_get_vpid_stats(vpid[index].vpid_handle, &vpid_stats);

            /* Print the statistics on the console. */
			//ARRIS MOD START
            PRINTK_CLI ("---------------- VPID %d (PID %d) ----------------\n", vpid[index].vpid_handle, vpid[index].parent_pid_handle );
            PRINTK_CLI ("                 type: %s \n", vpid_type );
            PRINTK_CLI ("Rx Unicast   Packets: %u\n",     vpid_stats.rx_unicast_pkt);
            PRINTK_CLI ("Rx Broadcast Packets: %u\n",     vpid_stats.rx_broadcast_pkt);
            PRINTK_CLI ("Rx Multicast Packets: %u\n",     vpid_stats.rx_multicast_pkt);
            PRINTK_CLI ("Rx Bytes            : 0x%08X%08X\n",  vpid_stats.rx_byte_hi, vpid_stats.rx_byte_lo);
            PRINTK_CLI ("Rx Bytes - Low (dec): %u\n",     vpid_stats.rx_byte_lo);
            PRINTK_CLI ("Rx Discard          : %u\n",     vpid_stats.rx_discard);
            PRINTK_CLI ("Tx Unicast   Packets: %u\n",     vpid_stats.tx_unicast_pkt);
            PRINTK_CLI ("Tx Broadcast Packets: %u\n",     vpid_stats.tx_broadcast_pkt);
            PRINTK_CLI ("Tx Multicast Packets: %u\n",     vpid_stats.tx_multicast_pkt);
            PRINTK_CLI ("Tx Bytes            : 0x%08X%08X\n",  vpid_stats.tx_byte_hi, vpid_stats.tx_byte_lo);
            PRINTK_CLI ("Tx Bytes - Low (dec): %u\n",     vpid_stats.tx_byte_lo);
            PRINTK_CLI ("Tx Errors           : %u\n",     vpid_stats.tx_error);
            PRINTK_CLI ("Tx Discards         : %u\n",     vpid_stats.tx_discard);
            PRINTK_CLI ("-----------------------------------------\n");
            //ARRIS MOD END
            /* Goto the next VPID. */
            index = index + 1;
        }
        /* Free the vpid array */
        kfree(vpid);
        /* Work is completed. */
        return 0;
    }

    /* Check if PID needs to be displayed? */
    if (strcmp(argv[1], "pid") == 0)
    {
        int         num_pid;
        int         index = 0;
        int         temp;
        int         size = (sizeof(TI_PP_PID))*(TI_PP_MAX_PID + 1);
        TI_PP_PID  *pid = (TI_PP_PID *)kmalloc(size, GFP_KERNEL);

        if (pid == NULL)
        {
            PRINTK_CLI(KERN_ERR "%s kmalloc failed\n", __FUNCTION__); //ARRIS MOD START
            return 0;
        }

        /* Get the PID Information. */
        num_pid = ti_ppm_get_pid (TI_PP_MAX_PID, pid);

        /* Cycle through all the VPID and get the stats */
        while (index < num_pid)
        {
            /* Print the PID Information on the console. */
			//ARRIS MOD START
            PRINTK_CLI("##########\n");
            PRINTK_CLI("# PID %02d #\n", pid[index].pid_handle);
            PRINTK_CLI("##########\n\n");
            PRINTK_CLI("Type                = %d [", pid[index].type);
			//ARRIS MOD END
            switch (pid[index].type)
            {
			//ARRIS MOD START
            case TI_PP_PID_TYPE_UNDEFINED:      PRINTK_CLI("Undefined");        break;
            case TI_PP_PID_TYPE_ETHERNET:       PRINTK_CLI("Ethernet");         break;
            case TI_PP_PID_TYPE_INFRASTRUCTURE: PRINTK_CLI("Infrastructure");   break;
            case TI_PP_PID_TYPE_USBBULK:        PRINTK_CLI("USB Bulk");         break;
            case TI_PP_PID_TYPE_CDC:            PRINTK_CLI("CDC");              break;
            case TI_PP_PID_TYPE_DOCSIS:         PRINTK_CLI("DOCSIS");           break;
            case TI_PP_PID_TYPE_ETHERNETSWITCH: PRINTK_CLI("Ethernet Switch");  break;
            default:                            PRINTK_CLI("???");              break;
			//ARRIS MOD END
            }
            PRINTK_CLI("]\n");//ARRIS MOD
            PRINTK_CLI("PriMapping          = %d\n", pid[index].pri_mapping);//ARRIS MOD
            temp = pid[index].dflt_pri_drp;
            PRINTK_CLI("Priority            = 0x%02X [Priority=%d, DropPrecedence=%d]\n", temp, temp&0x7, (temp>>3)&0x3);//ARRIS MOD
            temp = pid[index].ingress_framing;
            PRINTK_CLI("Framing             = 0x%02X ", temp);//ARRIS MOD
            if (temp)
            {
				//ARRIS MOD START
                PRINTK_CLI("[ ");
                if (temp & TI_PP_PID_INGRESS_ETHERNET)  {PRINTK_CLI("ETHERNET ");}
                if (temp & TI_PP_PID_INGRESS_IPV4)      {PRINTK_CLI("IPV4 ");}
                if (temp & TI_PP_PID_INGRESS_IPV6)      {PRINTK_CLI("IPV6 ");}
                if (temp & TI_PP_PID_INGRESS_IPOE)      {PRINTK_CLI("IPOE ");}
                if (temp & TI_PP_PID_INGRESS_IPOA)      {PRINTK_CLI("IPOA ");}
                if (temp & TI_PP_PID_INGRESS_PPPOE)     {PRINTK_CLI("PPPOE ");}
                if (temp & TI_PP_PID_INGRESS_PPPOA)     {PRINTK_CLI("PPPOA ");}
                PRINTK_CLI("]");
				//ARRIS MOD END
            }
            PRINTK_CLI("\n");// ARRIS MOD
            temp = pid[index].priv_flags;
            PRINTK_CLI("Flags               = 0x%02X ", temp);// ARRIS MOD
			//ARRIS MOD START
            if (temp)
            {
                PRINTK_CLI("[ ");
                if (temp & TI_PP_PID_VLAN_PRIO_MAP)         {PRINTK_CLI("fMapVLAN ");}
                if (temp & TI_PP_PID_DIFFSRV_PRIO_MAP)      {PRINTK_CLI("fMapDIFSRV ");}
                if (temp & TI_PP_PID_PRIO_OFF_TX_DST_TAG)   {PRINTK_CLI("fUseDestTag ");}
                if (temp & TI_PP_PID_CLASSIFY_BYPASS)       {PRINTK_CLI("fRxDisable ");}
                if (temp & TI_PP_PID_DISCARD_ALL_RX)        {PRINTK_CLI("fDiscardRx ");}
                PRINTK_CLI("]");
            }
            PRINTK_CLI("\n");
            PRINTK_CLI("TxDestTag           = 0x%04X\n", pid[index].dflt_dst_tag);
            PRINTK_CLI("TxQueueBase         = %d [%s]\n\n", pid[index].dflt_fwd_q, PAL_CPPI41_GET_QNAME( pid[index].dflt_fwd_q ) );
			//ARRIS MOD END
	
            /* Goto the next PID. */
            index = index + 1;
        }
        /* Free the pids array */
        kfree(pid);

        /* Work is completed. */
        return 0;
    }

    /* Check if Analysis Information needs to be displayed? */
    if (strcmp(argv[1], "stats") == 0)
    {
        int index;
        TI_PP_QOS_QUEUE_STATS   qos_stats;
        PPM_STATUS pp_status;
		//ARRIS MOD START
        /* Print the HIL Analysis report. */
        PRINTK_CLI (" HIL  State is : %s\n", (global_ti_hil_db.hil_disabled)? "Disabled":"Enabled" );
        pp_status = ti_pp_get_status();
        PRINTK_CLI (" PPM  State is                             : %s\n", (pp_status == INACTIVE) ? "Inactive" : ((pp_status == ACTIVE) ? "Active" : ((pp_status == PPM_PSM) ? "Power Save" : "Unknown")));
        PRINTK_CLI (" DBG  State is                             : %s\n", (global_ti_hil_db.dbg_disabled)? "Disabled":"Enabled" );
        PRINTK_CLI (" TDOX State is                             : %s\n", (global_ti_hil_db.tdox_disabled)? "Disabled":"Enabled" );
        PRINTK_CLI (" QoS  State is                             : %s\n", (global_ti_hil_db.qos_disabled)? "Disabled":"Enabled" );
        PRINTK_CLI (" TDOX Evaluation Time                      : %u ms\n", global_ti_hil_db.tdoxEvalMinTimeMsec);
        PRINTK_CLI (" TDOX Evaluation Average PktSize Threshold : %u bytes\n", global_ti_hil_db.tdoxEvalAvgPktSizeThresh);
        PRINTK_CLI (" TDOX Evaluation PPS Threshold             : %u\n", global_ti_hil_db.tdoxEvalPpsThresh);
#ifdef CONFIG_INTEL_PP_TUNNEL_SUPPORT
        PRINTK_CLI (" Tunnel Mode                               : %s\n", (global_ti_hil_db.tunnelMode)? "Enabled":"Disabled" );
#endif
        PRINTK_CLI (" Bypassed pkts                             : %u\n", global_ti_hil_db.num_bypassed_pkts );
        PRINTK_CLI (" Other    pkts                             : %u\n", global_ti_hil_db.num_other_pkts    );
        PRINTK_CLI (" Ingress  pkts                             : %u\n", global_ti_hil_db.num_ingress_pkts  );
        PRINTK_CLI (" Egress   pkts                             : %u\n", global_ti_hil_db.num_egress_pkts   );
        PRINTK_CLI (" Null     pkts                             : %u\n", global_ti_hil_db.num_null_drop_pkts );
        PRINTK_CLI (" Total Sessions                            : %u\n", global_ti_hil_db.num_total_sessions);
        PRINTK_CLI (" Total Errors                              : %u\n", global_ti_hil_db.num_error);
        PRINTK_CLI (" Global Timeout                            : %u sec\n", ti_session_timeout_sec);
		//ARRIS MOD END
        for(index = 0; index < HIL_MAX_NUM_BUCKETS; index++)
        {
            PRINTK_CLI(" Bucket %d numbers sessions                 : %d\n", index, global_ti_hil_db.session_bucket[index]); //ARRIS MOD 
        }

        if (pp_status == ACTIVE)
        {
			//ARRIS MOD START
            PRINTK_CLI ("\n QoS queues statistics:\n");
            PRINTK_CLI (" queue | forwarded  | discarded  | owner\n");
            PRINTK_CLI ("-------+------------+------------+--------------\n");
			//ARRIS MOD END
            for(index = 0; index <= (PAL_CPPI41_SR_QPDSP_QOS_Q_LAST - PAL_CPPI41_SR_QPDSP_QOS_Q_BASE); index++)
            {
                ti_ppm_get_qos_q_stats (index, &qos_stats);
                if (qos_stats.fwd_pkts | qos_stats.drp_cnt)
                {
					//ARRIS MOD START
                    PRINTK_CLI (" %5u | %10u | %10u | %s\n",index, qos_stats.fwd_pkts, qos_stats.drp_cnt, PAL_CPPI41_GET_QNAME( index + PAL_CPPI41_SR_QPDSP_QOS_Q_BASE ));
					//ARRIS MOD END
                }
            }
            PRINTK_CLI ("-------+------------+------------+--------------\n");//ARRIS MOD

        }

        /* Work is completed. */
        return 0;
    }

    /* Check if Session needs to be displayed? */
    if (strcmp(argv[1], "session") == 0)
    {
        int               num_session;
        unsigned char*    session;
        int               index = 0;
        unsigned int    lockKey;

        PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);
        /* Get the number of sessions that are available. */
        num_session = ti_ppm_get_session (-1, 0, NULL);
        PRINTK_CLI ("Detected %d sessions in packet processor\n", num_session);//ARRIS MOD and move up

        if (num_session == 0)
        {
            PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
            return 0;
        }

        /* Allocate memory for the sessions. */
        session = (unsigned char *)kmalloc(sizeof(unsigned char)*num_session, GFP_KERNEL);
        if (session == NULL)
        {
            PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
            return -1;
        }

        /* Get the Session Information - array of all the session indices */
        num_session = ti_ppm_get_session (-1, num_session, session);

        /* Cycle through all the VPID and get the stats */
        while (index < num_session)
        {
            /* Print the Session Information on the console. */
			//ARRIS MOD START
            PRINTK_CLI("\n");
            PRINTK_CLI("####################\n");
            PRINTK_CLI("# Session %03d info #\n", session[index]);
            PRINTK_CLI("####################\n\n");
			//ARRIS MOD END
            ti_hil_intrusive_display_session(session[index]);

            /* Goto the next Session */
            index = index + 1;
        }

        printk ("\nDetected %d sessions in packet processor\n\n", num_session);

        PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);

        /* Free the session buffer memory */
        kfree(session);

        /* Work is completed. */
        return 0;
    }

    if (strcmp(argv[1], "xSession") == 0)
    {
        Uint32 session = (Uint8)simple_strtol(argv[2], NULL, 0);
		//ARRIS MOD START
        PRINTK_CLI("\n");
        PRINTK_CLI("#############################\n");
        PRINTK_CLI("# Session %03d extended info #\n", session);
        PRINTK_CLI("#############################\n\n");
		//ARRIS MOD END

       ti_ppm_dispaly_session_info(session);
       if (hilSessionDb[session].lastUpdateTime)
       {
           Uint32 pps, bandwidth;
           PRINTK_CLI("tdoxHandle = %d\n", hilSessionDb[session].tdoxHandle);// ARRIS MOD
           PRINTK_CLI("timeDelta = %d\n", hilSessionDb[session].timeDelta);//ARRIS MOD
           if (hilSessionDb[session].pktDelta < 0xFFFFFFFF / 1000)
           {
               pps = (hilSessionDb[session].pktDelta * 1000) / hilSessionDb[session].timeDelta;
           }
           else
           {
               pps = (hilSessionDb[session].pktDelta / hilSessionDb[session].timeDelta) * 1000;
           }
           PRINTK_CLI("PPS = %d\n", pps);//ARRIS MOD
           if (hilSessionDb[session].bytesDelta < 0xFFFFFFFF / 8000)
           {
               bandwidth = (hilSessionDb[session].bytesDelta * 8000) / hilSessionDb[session].timeDelta;
           }
           else
           {
               bandwidth = (hilSessionDb[session].bytesDelta / hilSessionDb[session].timeDelta) * 8000;
           }
           PRINTK_CLI("Bandwidth = %d bps\n", bandwidth);//ARRIS MOD
       }

        /* Work is completed. */
        return 0;
    }

    if (strcmp(argv[1], "xQQ") == 0)
    {
        Uint32 queue = (Uint8)simple_strtol(argv[2], NULL, 0);
		//ARRIS MOD START
        PRINTK_CLI("\n");
        PRINTK_CLI("###############################\n");
        PRINTK_CLI("# QoS Queue %03d extended info #\n", queue);
        PRINTK_CLI("###############################\n\n");
		//ARRIS MOD END
       ti_ppm_dispaly_qos_queue_info(queue);

        /* Work is completed. */
        return 0;
    }

      

    if (strcmp(argv[1], "xQC") == 0)
    {
        Uint32 cluster = (Uint8)simple_strtol(argv[2], NULL, 0);
		//ARRIS MOD START
        PRINTK_CLI("\n");
        PRINTK_CLI("################################\n");
        PRINTK_CLI("# QoS Cluster %02d extended info #\n", cluster);
        PRINTK_CLI("################################\n\n");
		//ARRIS MOD END
       ti_ppm_dispaly_qos_cluster_info(cluster);

        /* Work is completed. */
        return 0;
    }
#ifdef CONFIG_NETFILTER
        if (strcmp(argv[1], "mapper") == 0)
            {
			unsigned int    lockKey;

        PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);
			//ARRIS MOD START
            PRINTK_CLI("\n");
            PRINTK_CLI("###############################\n");
            PRINTK_CLI("# mapper info #\n");
            PRINTK_CLI("###############################\n\n");
			//ARRIS MOD END
            int i;
            Uint16 num_displayed_sessions = (Uint16)simple_strtol(argv[2], NULL, 0);
            if (num_displayed_sessions == 0) 
            {
                num_displayed_sessions = TI_PP_MAX_ACCLERABLE_SESSIONS;
            }
            for (i = (TI_PP_MAX_ACCLERABLE_SESSIONS - 1); i >= (TI_PP_MAX_ACCLERABLE_SESSIONS - num_displayed_sessions); i--) 
                {
                PRINTK_CLI("# session [%d] ",i);//ARRIS MOD
                if (IS_TI_PP_SESSION_CT_INVALID(hil_session_ct_mapper[i].previous )) 
                    {
                    PRINTK_CLI("# previous [invalid] ");//ARRIS MOD
                    }
                else
                    {
                    PRINTK_CLI("# previous [%d] ",hil_session_ct_mapper[i].previous);//ARRIS MOD
                    }
                if (IS_TI_PP_SESSION_CT_INVALID(hil_session_ct_mapper[i].next)) 
                    {
                    PRINTK_CLI("# next [invalid] ");//ARRIS MOD
                    }
                else
                    {
                    PRINTK_CLI("# next [%d] ",hil_session_ct_mapper[i].next);//ARRIS MOD
                    }
                if (hil_session_ct_mapper[i].conntrack != NULL) 
                    {
                    if (hil_find_session_handle_in_ct_mapper_list(hil_session_ct_mapper[i].conntrack->tuplehash[IP_CT_DIR_REPLY].ti_pp_session_handle,i) == true)
                        {
                        PRINTK_CLI("# conntrack_session [%d] \n",hil_session_ct_mapper[i].conntrack->tuplehash[IP_CT_DIR_REPLY].ti_pp_session_handle);//ARRIS MOD
                        }
                   else if (hil_find_session_handle_in_ct_mapper_list(hil_session_ct_mapper[i].conntrack->tuplehash[IP_CT_DIR_ORIGINAL].ti_pp_session_handle,i) == true)
                        {
                        PRINTK_CLI("# conntrack_session [%d] \n",hil_session_ct_mapper[i].conntrack->tuplehash[IP_CT_DIR_ORIGINAL].ti_pp_session_handle);//ARRIS MOD
                        }
                   else 
                       {
                       PRINTK_CLI("\n");//ARRIS MOD
                       }
                   }
                else
                    {
                    PRINTK_CLI("conntrack is null\n");//ARRIS MOD
                    }
                }
			 PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
             return 0;
         }
#endif
    /* cable_pp: add brief summary printing */
    if (strcmp(argv[1], "brief") == 0)
    {
        /* GLOBAL */
        {
            TI_PP_GLOBAL_STATS  pp_stats;

            /* YES. Get the global stats through the PPM */
            ti_ppm_get_global_stats(&pp_stats);

            /* Print the stats on the console. */
			//ARRIS MOD START
            PRINTK_CLI ("Packets received in the PP   : %u\n", pp_stats.packets_rxed);
            PRINTK_CLI ("Number of search attemps     : %u\n", pp_stats.packets_searched);
            PRINTK_CLI ("Number of matched searches   : %u\n", pp_stats.search_matched);
            PRINTK_CLI ("Number of Synch Delays       : %u\n", pp_stats.sync_delay);
            PRINTK_CLI ("Packet forwarded by the PP   : %u\n", pp_stats.packets_fwd);
            PRINTK_CLI ("IPv4 Packets Forwarded       : %u\n", pp_stats.ipv4_packets_fwd);
			//ARRIS MOD END
        }
        /* VPIDs */
        {
            TI_PP_VPID  vpid[TI_PP_MAX_PID + 1];
            int         num_vpid;
            int         index = 0;
            unsigned int lockKey;

            PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);

            /* Get a list of all VPID that exist in the System */
            num_vpid = ti_ppm_get_vpid (-1, TI_PP_MAX_PID, &vpid[0]);

            /* Cycle through all the VPID and get the stats */
            while (index < num_vpid)
            {
                TI_PP_VPID_STATS   vpid_stats;

                /* Get the VPID statistics. */
                ti_ppm_get_vpid_stats(vpid[index].vpid_handle, &vpid_stats);
                PRINTK_CLI ("VPID index=%02d, handle=%02d: Rx=%10d, Tx=%10d\n", index, vpid[index].vpid_handle, //ARRIS MOD
                    vpid_stats.rx_unicast_pkt+vpid_stats.rx_broadcast_pkt+vpid_stats.rx_multicast_pkt,
                    vpid_stats.tx_unicast_pkt+vpid_stats.tx_broadcast_pkt+vpid_stats.tx_multicast_pkt);

                /* Goto the next VPID. */
                index = index + 1;
            }
            PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
        }
        /* SESSIONs */
        {
            int               num_session;
            unsigned char*    session;
            int               index = 0;
            unsigned int lockKey;

            PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);
            /* Get the number of sessions that are available. */
            num_session = ti_ppm_get_session (-1, 0, NULL);

            /* Allocate memory for the sessions. */
            session = (unsigned char *)kmalloc(sizeof(unsigned char)*num_session, GFP_KERNEL);
            if (session == NULL)
            {
                PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
                return -1;
            }

            /* Get the Session Information - array of all the session indices */
            num_session = ti_ppm_get_session (-1, num_session, session);

            /* Cycle through all the VPID and get the stats */
            while (index < num_session)
            {

                TI_PP_SESSION_STATS   session_stats;

                /* Get the VPID statistics. */
                ti_ppm_get_session_stats(session[index], &session_stats);

                PRINTK_CLI ("SESSION index=%02d, handle=%02d: Fwd=%10d\n", index, session[index],//ARRIS MOD
                    session_stats.packets_forwarded);
                /* Goto the next Session */
                index = index + 1;
            }

            PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);

            /* Free the session buffer memory */
            kfree(session);
        }
        return 0;
    }

    if (strcmp(argv[1], "sz") == 0)
    {
		//ARRIS MOD START
        PRINTK_CLI (" TI_PP_ETH_DESC:       %d \n", sizeof(TI_PP_ETH_DESC));
        PRINTK_CLI (" TI_PP_IPV4_DESC:      %d \n", sizeof(TI_PP_IPV4_DESC));
        PRINTK_CLI (" TI_PP_IPV6_DESC:      %d \n", sizeof(TI_PP_IPV6_DESC));
        PRINTK_CLI (" TI_PP_APP_DESC:       %d \n", sizeof(TI_PP_APP_DESC));
        PRINTK_CLI (" TI_PP_L2_RAW_DESC:    %d \n", sizeof(TI_PP_L2_RAW_DESC));
        PRINTK_CLI (" TI_PP_PACKET_DESC:    %d \n", sizeof(TI_PP_PACKET_DESC));
		//ARRIS MOD END
        return 0;
    }


    /* Control comes here if the scope was not understood. */
    return -1;
}

/**************************************************************************
 * FUNCTION NAME : ti_hil_set_cmd_handler
 **************************************************************************
 * DESCRIPTION   :
 *  This function is the Packet Processor set command handler.
 *
 * RETURNS       :
 *  -1      - Error.
 *  0       - Success.
 ***************************************************************************/
static int ti_hil_set_cmd_handler(int argc, char* argv[])
{
    Uint32 i;

    Uint8   **testPktP = NULL;
    Uint16  *testPktSizeP = NULL;

    if (strcmp(argv[1], "timeout") == 0)
    {
        int tmp = (int) simple_strtol(argv[2], NULL, 0);

        if (tmp < 0)
            return -1;

        ti_session_timeout_sec = tmp;

        /* Work is completed. */
        return 0;
    }

    else if (strcmp(argv[1], "tdoxDbg") == 0)
    {
        int enDis = (int)simple_strtol(argv[2], NULL, 0);

        if (enDis < 0 || enDis > 1)
            return -1;

        global_ti_hil_db.tdoxDbg = enDis;

        /* Work is completed. */
        return 0;
    }

    else if (strcmp(argv[1], "tdoxEvalAvgPktSize") == 0)
    {
        int threshold = (int) simple_strtol(argv[2], NULL, 0);

        if (threshold < 0)
            return -1;

        global_ti_hil_db.tdoxEvalAvgPktSizeThresh = threshold;

        /* Work is completed. */
        return 0;
    }

    else if (strcmp(argv[1], "tdoxEvalPPS") == 0)
    {
        int threshold = (int) simple_strtol(argv[2], NULL, 0);

        if (threshold < 0)
            return -1;

        global_ti_hil_db.tdoxEvalPpsThresh = threshold;

        /* Work is completed. */
        return 0;
    }

    else if (strcmp(argv[1], "tdoxEvalTime") == 0)
    {
        int threshold = (int) simple_strtol(argv[2], NULL, 0);

        if (threshold < 0)
            return -1;

        global_ti_hil_db.tdoxEvalMinTimeMsec = threshold * 1000;

        /* Work is completed. */
        return 0;
    }

    else if (strcmp(argv[1], "mtaAddress") == 0)
    {
        char    mtaAddress[6];

        for (i = 0; i < 6; i++)
        {
            mtaAddress[i] = (int) simple_strtol(argv[2], NULL, 16);
            argv[2] += 3;
        }

        ti_ppm_set_mta_mac_address(mtaAddress);

        /* Work is completed. */
        return 0;
    }

#ifdef CONFIG_INTEL_PP_TUNNEL_SUPPORT
    else if (strcmp(argv[1], "tunnelMode") == 0)
    {
        int tunnelMode = (int) simple_strtol(argv[2], NULL, 0);

        global_ti_hil_db.tunnelMode = tunnelMode;

        ti_ppm_set_tunnel_mode(tunnelMode);

        /* Work is completed. */
        return 0;
    }

    else if (strcmp(argv[1], "cmAddress") == 0)
    {
        char    cmAddress[6];

        for (i = 0; i < 6; i++)
        {
            cmAddress[i] = (int) simple_strtol(argv[2], NULL, 16);
            argv[2] += 3;
        }

        ti_ppm_set_cm_mac_address(cmAddress);

        /* Work is completed. */
        return 0;
    }
#endif

    else if (strcmp(argv[1], "vpid") == 0)
    {
        struct net_device * dev = dev_get_by_name (&init_net, argv[2]);
        if(dev)
        {
            if (argc == 5)
            {
                dev->qos_virtual_scheme_idx = simple_strtol(argv[3], NULL, 0);
            }
            else
            {
                dev->qos_virtual_scheme_idx = NETDEV_PP_QOS_PROFILE_DEFAULT;
            }
            ti_hil_pp_event (TI_PP_ADD_VPID, (void *)dev);
            dev_put(dev);
        }

        /* Work is completed. */
        return 0;
    }

    else if (strcmp(argv[1], "setClusterMaxCredit") == 0)
    {
        Uint32 clusterId = (Uint32) simple_strtol(argv[3], NULL, 0);
        Uint32 maxGlobalCredit = (Uint32) simple_strtol(argv[4], NULL, 0);

        if ((clusterId < 0)|| (clusterId > 32))
        {
            return -1;
        }
        if (maxGlobalCredit < 0)
        {
           return -1;
        }
        PRINTK_CLI("setting cluster %d with max %d\n",clusterId, maxGlobalCredit);// ARRIS MOD

        if (strcmp(argv[2], "Byte") == 0)
        {
            ti_ppm_set_qos_cluster_max_global_credit(1, clusterId, maxGlobalCredit);
        }
        else
        {
            ti_ppm_set_qos_cluster_max_global_credit(0, clusterId, maxGlobalCredit);
        }

        /* Work is completed. */

        return 0;
    }

    else if (strcmp(argv[1], "setQueueMaxCredit") == 0)
    {
        Uint32 queueId = (Uint32) simple_strtol(argv[3], NULL, 0);
        Uint32 maxCredit = (Uint32) simple_strtol(argv[4], NULL, 0);

        if ((queueId < 0)|| (queueId > 128))
        {
            return -1;
        }

        if (maxCredit < 0)
        {
           return -1;
        }

        PRINTK_CLI("setting queue %d with max %d\n", queueId, maxCredit);// ARRIS MOD

        if (strcmp(argv[2], "Byte") == 0)
        {
            ti_ppm_set_qos_queue_max_credit(1, queueId, maxCredit);
        }
        else
        {
            ti_ppm_set_qos_queue_max_credit(0, queueId, maxCredit);
        }

        /* Work is completed. */
        return 0;
    }

    else if (strcmp(argv[1], "setQueueItCredit") == 0)
    {
        Uint32 queueId = (Uint32) simple_strtol(argv[3], NULL, 0);
        Uint32 itCredit = (Uint32) simple_strtol(argv[4], NULL, 0);

        if ((queueId < 0)|| (queueId > 128))
        {
            return -1;
        }

        if (itCredit < 0)
        {
           return -1;
        }

        PRINTK_CLI("setting queue %d with iteration credit %d\n", queueId, itCredit);// ARRIS MOD

        if (strcmp(argv[2], "Byte") == 0)
        {
            ti_ppm_set_qos_queue_iteration_credit(1, queueId, itCredit);
        }
        else
        {
            ti_ppm_set_qos_queue_iteration_credit(0, queueId, itCredit);
        }

        /* Work is completed. */
        return 0;
    }

    else if (strcmp(argv[1], "dropped_packets_bit_map") == 0)
    {
        int cmd;
        int val;
 
        /* Check for bug of last arg */ 
        if (!isalnum(*argv[argc-1]))
        {
            /* Ignore last */
            argc--;
        }

        if (argc == 2) 
        {
            /* Show */
            PRINTK_CLI("dropped_packets_bit_map 0x%X\n", dropped_packets_bit_map);//ARRIS MOD
            return 0;
        }
        else if (argc < 4)
        {
            /* Message. Silenty ignore argc > 4 due to bug(??) in caller when providing argc/argv */
            PRINTK_CLI("ERROR: dropped_packets_bit_map 0x%X - %d parameters: argv[0] %s, argv[1] %s, argv[2] %s, argv[3] %s",//ARRIS MOD
                   dropped_packets_bit_map, argc
                   ,argc > 0 ? argv[0] : NULL       /* Print agrv[i] if it exists */
                   ,argc > 1 ? argv[1] : NULL
                   ,argc > 2 ? argv[2] : NULL
                   ,argc > 3 ? argv[3] : NULL
                   );
            return -1;
        }

        cmd = (int) simple_strtol(argv[2], NULL, 0);
        val = (int) simple_strtol(argv[3], NULL, 0);
        if (cmd == 0)
        {
            /* Unset, remove appropriate bit */
            DROPPED_PACKETS_BITMAP_UNSET(val);
        }
        else
        {
            /* Set, set appropriate bit */
            DROPPED_PACKETS_BITMAP_SET(val);
        }
        /* Work is completed. */
        return 0;
    }

    else if (strcmp(argv[1], "usPktDataIngress") == 0)
    {
        testPktP = &usPktDataIngressP;
        testPktSizeP = &usPktDataIngressSize;
    }
    else if (strcmp(argv[1], "usPktDataEgress") == 0)
    {
        testPktP = &usPktDataEgressP;
        testPktSizeP = &usPktDataEgressSize;
    }
    else if (strcmp(argv[1], "dsPktDataIngress") == 0)
    {
        testPktP = &dsPktDataIngressP;
        testPktSizeP = &dsPktDataIngressSize;
    }
    else if (strcmp(argv[1], "dsPktDataEgress") == 0)
    {
        testPktP = &dsPktDataEgressP;
        testPktSizeP = &dsPktDataEgressSize;
    }

    if (testPktSizeP != NULL)
    {
        unsigned int templateSize = 0;
        Char *endptr;

        if (argc != 4)
        {
            PRINTK_CLI("Test commands format: set **PktData*gress PktString PktLength\n");//ARRIS MOD
            return -EINVAL;
        }

        /* if testPktP is already allocated - make sure to free it before reallocating */
        if (*testPktP)
        {
            kfree(*testPktP);
        }

        /* Allocate *testPktP and reset it */
        *testPktSizeP = (Uint32)simple_strtol(argv[3], NULL, 0);
        *testPktP = kmalloc(*testPktSizeP, GFP_KERNEL);
        if (*testPktP == NULL)
        {
            PRINTK_CLI("Could not allocate %d bytes for testPktP\n", *testPktSizeP);//ARRIS MOD
            return -ENOMEM;
        }
        memset(*testPktP, 0, *testPktSizeP);

        /* Copy the packet */
        endptr = argv[2];
        while (true)
        {
            long val;
            val = (Int32)simple_strtol(endptr, &endptr, 16);
            if ((val < 0) || (val > 0xFF) || ((*endptr != ':') && (*endptr != '-') && (*endptr != '\0')))
            {
                PRINTK_CLI("\nError found\n");//ARRIS MOD
                kfree(*testPktP);
                return -EINVAL;
            }
            (*testPktP)[templateSize++] = (Char)val;
            if (*endptr == '\0')
            {
                break;
            }
            if (templateSize > *testPktSizeP)
            {
                PRINTK_CLI("Warning: packet size received is shorter than the template - truncating packet\n"); // ARRIS MOD
                break;
            }
            endptr++;
        }

        /* Work is completed. */
        return 0;
    }

    /* Control comes here if the command was not understood. */
    return -EINVAL;
}

/**************************************************************************
 * FUNCTION NAME : ti_hil_reset_cmd_handler
 **************************************************************************
 * DESCRIPTION   :
 *  This function is the Packet Processor reset command handler.
 *
 * RETURNS       :
 *  -1      - Error.
 *  0       - Success.
 ***************************************************************************/
static int ti_hil_reset_cmd_handler(int argc, char* argv[])
{
    int i = 0;

    /* Validate the number of arguments that have been passed. */
    if (argc < 2)
    {
        printk (KERN_ERR"ERROR: Incorrect Number of parameters passed.\n");//ARRIS MOD
        return -1;
    }

    /* Reset the session timeout to default, i.e. */
    if (strcmp(argv[1], "timeout") == 0)
    {
        ti_session_timeout_sec = DEFAULT_SESSION_TIMEOUT_SEC;
        printk ("Session timeout returned to default of %d seconds.\n", DEFAULT_SESSION_TIMEOUT_SEC);

        /* Work is completed. */
        return 0;
    }

    /* Reset the HIL Analysis stats. */
    if (strcmp(argv[1], "stats") == 0)
    {
        /* Initialize the counters for the HIL Analysis. */
        for(i = 0; i < HIL_MAX_NUM_BUCKETS; i++)
            global_ti_hil_db.session_bucket[i] = 0;

        global_ti_hil_db.num_total_sessions = 0;
        global_ti_hil_db.num_error = 0;
        global_ti_hil_db.num_bypassed_pkts = 0;
        global_ti_hil_db.num_other_pkts = 0;
        global_ti_hil_db.num_ingress_pkts = 0;
        global_ti_hil_db.num_egress_pkts  = 0;
        global_ti_hil_db.num_null_drop_pkts = 0;

        /* Work is completed. */
        return 0;
    }
    if (strcmp(argv[1], "vpid") == 0)
    {
        struct net_device * dev = dev_get_by_name (&init_net, argv[2]);
        if(dev)
        {
            ti_hil_pp_event (TI_PP_REMOVE_VPID, (void *)dev);
            dev_put(dev);
        }

        /* Work is completed. */
        return 0;
    }

    /* Control comes here if the command was not understood. */
    return -1;
}

/**************************************************************************
 * FUNCTION NAME : ti_hil_write_cmds
 **************************************************************************
 * DESCRIPTION   :
 *  Interface for the Intrusive HIL. This is used to debug and display various
 *  packet processor entity information from the console.
 *
 * RETURNS       :
 *  -1              - Error.
 *  Non-Zero        - Success.
 ***************************************************************************/
static int ti_hil_write_cmds (struct file *file, const char *buffer, unsigned long count, void *data)
{
    char*   pp_cmd;
    char*   argv[10];
    int     argc = 0;
    char*   ptr_cmd;
    char*   delimitters = " \n\t";
    char*   ptr_next_tok;

    Uint8   **testPktP = NULL;
    Uint16  *testPktSizeP = NULL;

    pp_cmd = kmalloc(count+1, GFP_KERNEL);
    if (pp_cmd == NULL)
    {
        printk(KERN_ERR"Could not allocate %d bytes for pp_cmd\n", count);// ARRIS MOD
        return -ENOMEM;
    }

    /* Initialize the buffer before using it. count+1 in order to put null at the end of the buffer after it will be copy from user*/
    memset ((void *)&pp_cmd[0], 0, count+1);
    memset ((void *)&argv[0], 0, sizeof(argv));

    /* Copy from user space. */
    if (copy_from_user (pp_cmd, buffer, count))
    {
        kfree(pp_cmd);
        return -EFAULT;
    }

    if (pp_cmd[0] == 0)
    {
         kfree(pp_cmd);
         return -EINVAL;
    }

    /* Extract the first command. */
    ptr_next_tok = pp_cmd;

    /* Parse all the commands typed. */
    while (1)
    {
        ptr_cmd = strsep(&ptr_next_tok, delimitters);
        if (ptr_cmd == NULL)
        {
            /* 'strsep' returns null if there was no tok when it gets to '\0' */
            break;
        }
        argv[argc++] = ptr_cmd;

        if (ptr_next_tok == NULL)
        {
            /* no next tok*/
            break;
        }
        /* Validate if the user entered more commands.*/
        if (argc >=10)
        {
            printk (KERN_ERR"ERROR: Incorrect too many parameters dropping the command\n");//ARRIS MOD
            kfree(pp_cmd);
            return -EFAULT;
        }
    }


    /******************************* Command Handlers *******************************/
    /* Display Command Handlers */
    if (strncmp(argv[0], "show", strlen("show")) == 0)
    {
        /* Call the Show Command Handler. */
        if (ti_hil_show_cmd_handler (argc, argv) < 0)
        {
            kfree(pp_cmd);
            return -EFAULT;
        }
    }

    /* Set Command Handlers */
    else if (strncmp(argv[0], "set", strlen("set")) == 0)
    {
        /* Call the Set Command Handler. */
        if (ti_hil_set_cmd_handler (argc, argv) < 0)
        {
            kfree(pp_cmd);
            return -EFAULT;
        }
    }

    /* Deinitialize Command Handlers */
    else if (strncmp(argv[0], "reset", strlen("reset")) == 0)
    {
        /* Call the Reset Command Handler. */
        if (ti_hil_reset_cmd_handler (argc, argv) < 0)
        {
            kfree(pp_cmd);
            return -EFAULT;
        }
    }

    /* cable_pp: disable/enable capability */
    else if (strcmp(argv[0], "enable") == 0)
    {
        global_ti_hil_db.hil_disabled = 0;
    }

    else if (strcmp(argv[0], "disable") == 0)
    {
        global_ti_hil_db.hil_disabled = 1;
        ti_ppm_flush_sessions(-1);
    }

    else if (strcmp(argv[0], "dbg") == 0)
    {
        global_ti_hil_db.dbg_disabled = 0;
    }

    else if (strcmp(argv[0], "nodbg") == 0)
    {
        global_ti_hil_db.dbg_disabled = 1;
    }

    else if (strcmp(argv[0], "tdox") == 0)
    {
        global_ti_hil_db.tdox_disabled = 0;
    }

    else if (strcmp(argv[0], "notdox") == 0)
    {
        global_ti_hil_db.tdox_disabled = 1;
    }
    else if (strcmp(argv[0], "ackSupp") == 0)
    {
        char pram = *(argv[1]);
        int enDis = 0;
        if(pram == '1')
        {
            enDis = 1;
        }
        ti_ppm_set_ack_suppression(enDis);
    }
    else if ((strcmp(argv[0], "qos") == 0) && (1 == global_ti_hil_db.qos_disabled) )
    {
        unsigned int i;
        unsigned int lockKey = 0;

        PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);

        ti_ppm_flush_sessions(-1);
        // go over all devices and setup QoS
        for( i = 0; i < TI_MAX_DEVICE_INDEX; i++ )
        {
            struct net_device *cur_dev = 0;
            cur_dev = dev_get_by_index(&init_net, i);
            if( !cur_dev )
                continue;

            if (-1 == cur_dev->vpid_handle)
            {
                dev_put(cur_dev);
                continue;
            }

            if (NULL != cur_dev->qos_setup_hook)
            {
                cur_dev->qos_setup_hook( cur_dev );
            }
            dev_put(cur_dev);
        }

        PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
    }

    else if ((strcmp(argv[0], "noqos") == 0) && (0 == global_ti_hil_db.qos_disabled) )
    {
        unsigned int i;
        unsigned int lockKey = 0;

        PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);

        ti_ppm_flush_sessions(-1);
        // go over all devices and shutdown QoS
        for( i = 0; i < TI_MAX_DEVICE_INDEX; i++ )
        {
            struct net_device *cur_dev = 0;
            cur_dev = dev_get_by_index(&init_net, i);
            if( !cur_dev )
                continue;

            if (-1 == cur_dev->vpid_handle)
            {
                dev_put(cur_dev);
                continue;
            }

            if (NULL != cur_dev->qos_shutdown_hook)
            {
                cur_dev->qos_shutdown_hook( cur_dev );
            }
            dev_put(cur_dev);
        }

        PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
    }

   
    else if (strcmp(argv[0], "psm") == 0)
    {

        ti_ppm_flush_sessions(-1);
        ti_ppm_enable_psm();
    }

    else if (strcmp(argv[0], "nopsm") == 0)
    {
        ti_ppm_disable_psm();
    }


    else if (strcmp(argv[0], "flush_all_sessions") == 0)
    {
        /* Call flush sessions API with -1 */
        ti_ppm_flush_sessions(-1);
    }

    else if (strcmp(argv[0], "createSession") == 0)
    {
        ti_hil_test_session_creation();
    }

    else if (strcmp(argv[0], "printUsPktDataIngress") == 0)
    {
        testPktP = &usPktDataIngressP;
        testPktSizeP = &usPktDataIngressSize;
    }
    else if (strcmp(argv[0], "printUsPktDataEgress") == 0)
    {
        testPktP = &usPktDataEgressP;
        testPktSizeP = &usPktDataEgressSize;
    }
    else if (strcmp(argv[0], "printDsPktDataIngress") == 0)
    {
        testPktP = &dsPktDataIngressP;
        testPktSizeP = &dsPktDataIngressSize;
    }
    else if (strcmp(argv[0], "printDsPktDataEgress") == 0)
    {
        testPktP = &dsPktDataEgressP;
        testPktSizeP = &dsPktDataEgressSize;
    }
    // ARRIS ADD START
    else if (strcmp(argv[0], "cert") == 0)
    {
        certMode = 1;
    }
    // ARRIS ADD END    

    if (testPktSizeP != NULL)
    {
        Uint32 i;

        if (*testPktP)
        {
            Bool backspaceNeeded = False;
            for (i = 0; i < *testPktSizeP; i++)
            {
                backspaceNeeded = True;
                printk("%02X:", (*testPktP)[i]);
                if ((i & 0x7) == 0x7)
                {
                    printk("\b \b\n");
                    backspaceNeeded = False;
                }
            }
            if (backspaceNeeded)
            {
                printk("\b \b");
            }
            printk("\n");
        }
        else
        {
            printk("Was not set!!!\n");
        }
    }

    kfree(pp_cmd);
    return count;
}

/**************************************************************************
 * FUNCTION NAME : ti_hil_health_timer_expired
 **************************************************************************
 * DESCRIPTION   :
 *  The function is the health timer expiration routine which is called by
 *  periodically by the HOST to verify the sanity of the PDSP.
 **************************************************************************/
static void ti_hil_health_timer_expired (unsigned long data)
{
    /* Use the PPM API to determine the health of the PDSP. */

    if (ti_ppm_health_check() < 0)
    {
        /* PDSP dont seem to be working correctly... This is a FATAL Condition and
         * needs to be handled... We cause the system to crash here as an indication
         * that this needs to be addressed. System Profiles need to handle this in
         * a more robust manner. */
        printk (KERN_ERR"------- FATAL Error: Packet Processor PDSP is not healthy ------- \n");//ARRIS MOD
        BUG();
        return;
    }

    /* The PDSP passed the health check; restart the timer */
    pdsp_health_timer.expires = jiffies + (PDSP_HEALTH_TIMER_SEC * HZ);
    add_timer (&pdsp_health_timer);
    return;
}

int ti_hil_read_devs(char* buf, char **start, off_t offset, int count,
                 int *eof, void *data)
{
    unsigned int i;
    int len=0;
    unsigned int limit = count - 80;

    for( i = 0; i < TI_MAX_DEVICE_INDEX; i++ )
    {
         struct net_device *cur_dev = 0;
         TI_PP_VPID_STATS   vpid_stats;

         cur_dev = dev_get_by_index(&init_net, i);
         if( !cur_dev )
         {
              continue;
         }
         if (cur_dev->vpid_handle >= TI_PP_MAX_VPID || cur_dev->vpid_handle < 0)
         {
             dev_put(cur_dev);
             continue;
         }

         /* Get the VPID statistics. */
         if( ti_ppm_get_vpid_stats( cur_dev->vpid_handle , &vpid_stats) < 0 )
         {
              dev_put(cur_dev);
              continue;
         }

         /* Print the statistics on the console. */
         if( len < limit )
             len += sprintf(buf + len, "   /dev/%s: vpid=%d pid=%d \n", cur_dev->name, cur_dev->vpid_handle, cur_dev->pid_handle );
         if( len < limit )
             len += sprintf(buf + len, "-----------------------------------------\n");
         if( len < limit )
             len += sprintf(buf + len, "Rx Unicast   Packets: %u\n", vpid_stats.rx_unicast_pkt);
         if( len < limit )
             len += sprintf(buf + len, "Rx Broadcast Packets: %u\n", vpid_stats.rx_broadcast_pkt);
         if( len < limit )
             len += sprintf(buf + len, "Rx Multicast Packets: %u\n", vpid_stats.rx_multicast_pkt);
         if( len < limit )
             len += sprintf(buf + len, "Rx Bytes            : 0x%08X%08X\n", vpid_stats.rx_byte_hi, vpid_stats.rx_byte_lo);
         if( len < limit )
             len += sprintf(buf + len, "Rx Bytes - low      : %u\n", vpid_stats.rx_byte_lo);
         if( len < limit )
             len += sprintf(buf + len, "Rx Discard          : %u\n", vpid_stats.rx_discard);
         if( len < limit )
             len += sprintf(buf + len, "Tx Unicast   Packets: %u\n", vpid_stats.tx_unicast_pkt);
         if( len < limit )
             len += sprintf(buf + len, "Tx Broadcast Packets: %u\n", vpid_stats.tx_broadcast_pkt);
         if( len < limit )
             len += sprintf(buf + len, "Tx Multicast Packets: %u\n", vpid_stats.tx_multicast_pkt);
         if( len < limit )
             len += sprintf(buf + len, "Tx Bytes            : 0x%08X%08X\n", vpid_stats.tx_byte_hi, vpid_stats.tx_byte_lo);
         if( len < limit )
             len += sprintf(buf + len, "Tx Bytes - low      : %u\n", vpid_stats.tx_byte_lo);
         if( len < limit )
             len += sprintf(buf + len, "Tx Errors           : %u\n", vpid_stats.tx_error);
         if( len < limit )
             len += sprintf(buf + len, "Tx Discards         : %u\n\n", vpid_stats.tx_discard);
         dev_put(cur_dev);
    }

    return len;
}

/**************************************************************************
 * FUNCTION NAME : ti_hil_intrusive_init
 **************************************************************************
 * DESCRIPTION   :
 *  Initialization function for the Intrusive mode profile.
 *
 * RETURNS       :
 *  0   -   Success
 *  <0  -   Error
 **************************************************************************/
static int ti_hil_intrusive_init (void)
{
    struct proc_dir_entry*  ptr_dir_entry;
    struct proc_dir_entry*  ptr_dev_entry;
    int                     index = 0;

    /* Register an event handler to listen to events. */
    ppsubsystem_event_handler = ti_ppm_register_event_handler (ti_hil_ppsubsystem_event_handler);
    if (ppsubsystem_event_handler == 0)
    {
        printk (KERN_ERR"Error: Event Handler register failed\n");//ARRIS MOD
        return -1;
    }

    /* Create the PROC Entry used by the TCA Configuration Engine. */
    ptr_dir_entry = create_proc_entry("ti_pp" ,0644, init_net.proc_net);
    ptr_dev_entry = create_proc_entry("ti_pp_dev" ,0644, init_net.proc_net);

    if( (ptr_dir_entry == NULL) || (ptr_dev_entry == NULL ) )
    {
        printk (KERN_ERR"Error: Unable to create Packet Processor proc entry.\n");//ARRIS MOD
        return -1;
    }
    ptr_dir_entry->data      = NULL;
    ptr_dir_entry->read_proc  = NULL;
    ptr_dir_entry->write_proc = ti_hil_write_cmds;


    ptr_dev_entry->data      = NULL;
    ptr_dev_entry->read_proc  = ti_hil_read_devs;
    ptr_dev_entry->write_proc = NULL;;

    /* Create a timer to poll for the health */
    init_timer (&pdsp_health_timer);
    pdsp_health_timer.function = ti_hil_health_timer_expired;
    pdsp_health_timer.data     = 0;

    /* Start the timer. */
    pdsp_health_timer.expires = jiffies + (PDSP_HEALTH_TIMER_SEC * HZ);
    add_timer (&pdsp_health_timer);

    /* Initialize the counters for the HIL Analysis. */
    for(index = 0; index < HIL_MAX_NUM_BUCKETS; index++)
    {
        global_ti_hil_db.session_bucket[index] = 0;
    }

#ifdef CONFIG_NETFILTER
    /* Initialize the HIL Session to Connection Tracking Mapper. */
    for (index = 0; index< TI_PP_MAX_ACCLERABLE_SESSIONS ;index++)
    {
        hil_session_ct_mapper[index].conntrack = NULL;
        hil_session_ct_mapper[index].next = TI_PP_SESSION_CT_IDLE;
        hil_session_ct_mapper[index].previous = TI_PP_SESSION_CT_IDLE;
    }
#endif /* CONFIG_NETFILTER */

#ifdef CONFIG_IP_MULTICAST
    /* Initialize the MFC to Session Mapper. */
    memset ((void *)&hil_mfc_session_mapper[0], 0, sizeof(hil_mfc_session_mapper));;
#endif /* CONFIG_IP_MULTICAST */

    ti_hil_tdox_init();

    /* Initialize PP Path */
    PPP_Init();

    FREE_RUNNING_COUNTER_ENABLE();

    /* Profile has been successfully initialized. */
    return 0;
}

/**************************************************************************
 * FUNCTION NAME : ti_hil_intrusive_deinit
 **************************************************************************
 * DESCRIPTION   :
 *  Deinitialization function which deinitializes and unregisters the default
 *  profile with the HIL Core.
 *
 * RETURNS       :
 *  0   -   Success
 *  <0  -   Error
 **************************************************************************/
static int ti_hil_intrusive_deinit(void)
{
    return 0;
}



/**************************************************************************
 * FUNCTION NAME : ti_hil_enable_psm
 **************************************************************************
 * DESCRIPTION   :
 *  Enable Power Saving Mode (PSM) API
 *
 * RETURNS       :
 *  0   -   Success
 *  <0  -   Error
 **************************************************************************/
int ti_hil_enable_psm (void)
{
    /* Delete old sessions in the system */
    printk(KERN_INFO "%s: Flush all old sessions\n", __FUNCTION__);
    if (ti_ppm_flush_sessions(-1) < 0)
    {
        printk (KERN_ERR"Error: Unable to flush all sessions\n");//ARRIS MOD
        return -1;
    }

    /* Call PPM enable PSM */
    return ti_ppm_enable_psm();
       
}

/**************************************************************************
 * FUNCTION NAME : ti_hil_disable_psm
 **************************************************************************
 * DESCRIPTION   :
 *  Disable Power Saving Mode (PSM) API
 *
 * RETURNS       :
 *  0   -   Success
 *  <0  -   Error
 **************************************************************************/
int ti_hil_disable_psm (void)
{
    /* Call PPM disable PSM */
    if (ti_ppm_disable_psm() < 0)
    {
        printk (KERN_ERR"%s: Error: Unable to disable PP PSM\n", __FUNCTION__);//ARRIS MOD
        return -1;
    }

    return 0;
}

/**************************************************************************
 * FUNCTION NAME : ti_hil_enable_6rd_pp
 **************************************************************************
 * DESCRIPTION   :
 *  Enable 6RD PP Support (Protocol 41) API 
 *
 * RETURNS       :
 *  0   -   Success
 *  <0  -   Error
 **************************************************************************/
int ti_hil_enable_6rd_pp (void)
{
    printk(KERN_DEBUG "%s: Start Enable 6RD PP Support\n", __FUNCTION__);

    global_ti_hil_db.ipv6rd_pp_support = 1;

    return 0;
}

/**************************************************************************
 * FUNCTION NAME : ti_hil_disable_6rd_pp
 **************************************************************************
 * DESCRIPTION   :
 *  Disable 6RD PP Support (Protocol 41) API 
 *
 * RETURNS       :
 *  0   -   Success
 *  <0  -   Error
 **************************************************************************/
int ti_hil_disable_6rd_pp (void)
{
    printk(KERN_DEBUG "%s: Start Disable PP 6RD Support\n", __FUNCTION__);

    global_ti_hil_db.ipv6rd_pp_support = 0;

    return 0;
}


#if 0
Uint8 usPktDataIngress[] =
{
    0x00, 0x14, 0xF1, 0xE5, 0x4C, 0x32, // ETH DA
    0x00, 0x0D, 0xBC, 0x20, 0xB1, 0x0E, // ETH SA
    0x81, 0x00,                         // VLAN Header 0x8100
    0x00, 0x37,                         // VLAN Identifier - 55
    0x08, 0x00,                         // ETH Type
    0x45, 0x00,                         // IP Version/Header Length, IP TOS
    0x03, 0xEE,                         // IP Total Length - updated per packet
    0x00, 0x00, 0x00, 0x00,             // IP Identification, IP Fragment
    0x40,                               // IP TTL
    0x11,                               // IP Protocol - UDP
    0xFA, 0xA8,                         // IP Checksum
    0x0A, 0x64, 0x34, 0xC7,             // IP SA - 10.100.52.199
    0x0A, 0x64, 0x32, 0xC8,             // IP DA - 10.100.50.200
    0x00, 0x40, 0x07, 0xD0,             // UDP SRC Port (64), UDP DST Port (2000)
    0x03, 0xDA,                         // UDP Length
    0x00, 0x00,                         // UDP Checksum
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00  // Payload
};

Uint8 usPktDataEgress[] =
{
    0x00, 0x14, 0xF1, 0xE5, 0x4C, 0x32, // ETH DA
    0x00, 0x0D, 0xBC, 0x20, 0xB1, 0x0E, // ETH SA
    0x08, 0x00,                         // ETH Type
    0x45, 0x00,                         // IP Version/Header Length, IP TOS
    0x05, 0xDC,                         // IP Total Length - updated per packet
    0x00, 0x00, 0x00, 0x00,             // IP Identification, IP Fragment
    0x40,                               // IP TTL
    0x2F,                               // IP Protocol - UDP
    0xF8, 0x9C,                         // IP Checksum
    0x0A, 0x64, 0x34, 0xC7,             // IP SA - 10.100.52.199
    0x0A, 0x64, 0x32, 0xC8,             // IP DA - 10.100.50.200
    0x00, 0x00, 0x65, 0x58,             // GRE header

    0x00, 0x14, 0xF1, 0xE5, 0x4C, 0x32, // ETH DA
    0x00, 0x0D, 0xBC, 0x20, 0xB1, 0x0E, // ETH SA
    0x08, 0x00,                         // ETH Type
    0x45, 0x00,                         // IP Version/Header Length, IP TOS
    0x03, 0xD6,                         // IP Total Length - updated per packet
    0x00, 0x00, 0x00, 0x00,             // IP Identification, IP Fragment
    0x40,                               // IP TTL
    0x11,                               // IP Protocol - UDP
    0xFA, 0xC0,                         // IP Checksum
    0x0A, 0x64, 0x34, 0xC7,             // IP SA - 10.100.52.199
    0x0A, 0x64, 0x32, 0xC8,             // IP DA - 10.100.50.200
    0x00, 0x40, 0x07, 0xD0,             // UDP SRC Port (64), UDP DST Port (2000)
    0x03, 0xDA,                         // UDP Length
    0x00, 0x00,                         // UDP Checksum
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00  // Payload
};

Uint8 dsPktDataIngress[] =
{
    0x00, 0x0D, 0xBC, 0x20, 0xB1, 0x0E, // ETH DA
    0x00, 0x00, 0x00, 0x06, 0x16, 0x01, // ETH SA
    0x08, 0x00,                         // ETH Type
    0x45, 0x00,                         // IP Version/Header Length, IP TOS
    0x05, 0xDC,                         // IP Total Length - updated per packet
    0x00, 0x00, 0x00, 0x00,             // IP Identification, IP Fragment
    0x40,                               // IP TTL
    0x2F,                               // IP Protocol - GRE
    0xF8, 0x9C,                         // IP Checksum
    0x0A, 0x64, 0x32, 0xC8,             // IP SA - 10.100.50.200
    0x0A, 0x64, 0x34, 0xC7,             // IP DA - 10.100.52.199
    0x00, 0x00, 0x65, 0x58,             // GRE header

    0x00, 0x0D, 0xBC, 0x20, 0xB1, 0x0E, // ETH DA
    0x00, 0x00, 0x00, 0x06, 0x16, 0x01, // ETH SA
    0x08, 0x00,                         // ETH Type
    0x45, 0x00,                         // IP Version/Header Length, IP TOS
    0x03, 0xD6,                         // IP Total Length - updated per packet
    0x00, 0x00, 0x00, 0x00,             // IP Identification, IP Fragment
    0x40,                               // IP TTL
    0x11,                               // IP Protocol - UDP
    0xFA, 0xC0,                         // IP Checksum
    0x0A, 0x64, 0x32, 0xC8,             // IP SA - 10.100.50.200
    0x0A, 0x64, 0x34, 0xC7,             // IP DA - 10.100.52.199
    0x00, 0x64, 0x00, 0x64,             // UDP SRC Port (100), UDP DST Port (100)
    0x03, 0xC2,                         // UDP Length
    0x00, 0x00,                         // UDP Checksum
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00  // Payload
};

Uint8 dsPktDataEgress[] =
{
    0x00, 0x0D, 0xBC, 0x20, 0xB1, 0x0E, // ETH DA
    0x00, 0x00, 0x00, 0x06, 0x16, 0x01, // ETH SA
    0x81, 0x00,                         // VLAN Header 0x8100
    0x00, 0x37,                         // VLAN Identifier - 55
    0x08, 0x00,                         // ETH Type
    0x45, 0x00,                         // IP Version/Header Length, IP TOS
    0x03, 0xD6,                         // IP Total Length - updated per packet
    0x00, 0x00, 0x00, 0x00,             // IP Identification, IP Fragment
    0x40,                               // IP TTL
    0x11,                               // IP Protocol - UDP
    0xFA, 0xC0,                         // IP Checksum
    0x0A, 0x64, 0x32, 0xC8,             // IP SA - 10.100.50.200
    0x0A, 0x64, 0x34, 0xC7,             // IP DA - 10.100.52.199
    0x00, 0x64, 0x00, 0x64,             // UDP SRC Port (100), UDP DST Port (100)
    0x03, 0xC2,                         // UDP Length
    0x00, 0x00,                         // UDP Checksum
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00  // Payload
};
#endif

void ti_hil_test_session_creation(void)
{
    struct sk_buff *skb;
    struct net_device *cniDev;
    struct net_device *ethDev;
    char cniVpid;
    char ethVpid;
    char tempVpid;
    TI_PP_VPID vpid;
    TI_PP_PID   pid;
    TI_PP_SESSION       *ptr_session;
    Bool    usSession = False;
    Bool    dsSession = False;

    if ((usPktDataIngressP != NULL) && (usPktDataEgressP != NULL))
    {
        usSession = True;
    }
    if ((dsPktDataIngressP != NULL) && (dsPktDataEgressP != NULL))
    {
        dsSession = True;
    }
    if ((usSession == False) && (dsSession == False))
    {
        printk("ti_hil_test_session_creation: Cannot create session since one of the packets was not configured\n");
        return;
    }
    else if (usSession == False)
    {
        printk("ti_hil_test_session_creation: Cannot create US session since one of the packets was not configured\n");
    }
    else if (dsSession == False)
    {
        printk(KERN_ERR "ti_hil_test_session_creation: Cannot create DS session since one of the packets was not configured\n"); // ARRIS MOD
    }

    cniVpid = -1;
    ethVpid = -1;
    for (tempVpid = TI_PP_MAX_VPID-1; tempVpid >= 0; tempVpid--)
    {
        if (ti_ppm_get_vpid_info (tempVpid, &vpid) == 0)
        {
            if (ti_ppm_get_pid_info  (vpid.parent_pid_handle, &pid) == 0)
            {
                if (TI_PP_PID_TYPE_DOCSIS == pid.type)
                {
                    cniVpid = tempVpid;
                    if (ethVpid != 0xFF)
                    {
                        break;
                    }
                }
                // TBD: when several ethernet VPID will be configured, need to know which is relevant
                if (TI_PP_PID_TYPE_ETHERNET == pid.type)
                {
                    ethVpid = tempVpid;
                    if (cniVpid != 0xFF)
                    {
                        break;
                    }
                }
            }
        }
    }
    if ((ethVpid == -1) || (cniVpid == -1))
    {
        printk(KERN_ERR"ti_hil_test_session_creation: Failed to acquire ethVpid or cniVpid\n");//ARRIS MOD
        return;
    }

    cniDev = __dev_get_by_name(&init_net, "cni0");

    if(!(skb = dev_alloc_skb(2048)))
    {
        printk(KERN_ERR"ti_hil_test_session_creation: Failed to allocate skb\n");//ARRIS MOD
        return;
    }

    skb->mac_header = skb->data;
    skb->pp_packet_info.egress_queue = TI_PPM_EGRESS_QUEUE_INVALID;

    /********************/
    /* US ingress packet*/
    /********************/
    ptr_session = &skb->pp_packet_info.ti_session;
    ptr_session->ingress.vpid_handle = ethVpid;
    ptr_session->egress[0].vpid_handle = cniVpid;
    memcpy(skb->data, usPktDataIngressP, usPktDataIngressSize);
    ethDev = __dev_get_by_name(&init_net, "eth0");
    skb->skb_iif = ethDev->ifindex;
    if (usSession)
    {
        ti_hil_ingress_hook(skb); 
    }
    

    /*******************/
    /* US egress packet*/
    /*******************/
    skb->skb_iif = cniDev->ifindex;
    skb->dev = cniDev;
    memcpy(skb->data, usPktDataEgressP, usPktDataEgressSize);
    if (usSession)
    {
        ti_hil_egress_hook(skb);
    }

    /********************/
    /* DS ingress packet*/
    /********************/
    skb->skb_iif = cniDev->ifindex;
    skb->dev = cniDev;
    memset((Uint8*)&skb->pp_packet_info.ti_session, 0, sizeof(skb->pp_packet_info.ti_session));
    ptr_session = &skb->pp_packet_info.ti_session;
    ptr_session->ingress.vpid_handle = cniVpid;
    ptr_session->egress[0].vpid_handle = ethVpid;
    memcpy(skb->data, dsPktDataIngressP, dsPktDataIngressSize);
    if (dsSession)
    {
        ti_hil_ingress_hook(skb); 
    }

    /*******************/
    /* DS egress packet*/
    /*******************/
    memcpy(skb->data, dsPktDataEgressP, dsPktDataEgressSize);
    ethDev = __dev_get_by_name(&init_net, "eth0");
    skb->skb_iif = ethDev->ifindex;
    if (dsSession)
    {
        ti_hil_egress_hook(skb);
    }

    /* Change sessions state to forwarding and set session serial number */
    *(Uint16*)((Uint32)(((Uint32)IO_ADDRESS(0x03100900)) + (255 * 2))) = 0x40;
    if (usSession && dsSession)
    {
        *(Uint16*)((Uint32)(((Uint32)IO_ADDRESS(0x03100900)) + (254 * 2))) = 0x41;
    }
}
void ti_hil_delete_drop_session(unsigned int src_ip,unsigned int dst_ip,unsigned short src_port, unsigned short dst_port) 
{
    ti_ppm_delete_drop_sessions(&netfilter_null_vpid_handle,&docsis_null_vpid_handle, src_ip, dst_ip,src_port, dst_port);
}

EXPORT_SYMBOL(ti_hil_enable_psm);
EXPORT_SYMBOL(ti_hil_disable_psm);
EXPORT_SYMBOL(ti_hil_enable_6rd_pp);
EXPORT_SYMBOL(ti_hil_disable_6rd_pp);
EXPORT_SYMBOL(ti_hil_set_mta_mac_address);
#ifdef CONFIG_INTEL_PP_TUNNEL_SUPPORT
EXPORT_SYMBOL(ti_hil_create_tunnel);
EXPORT_SYMBOL(ti_hil_delete_tunnel);
EXPORT_SYMBOL(ti_hil_set_tunnel_mode);
EXPORT_SYMBOL(ti_hil_set_cm_mac_address);
#endif
#ifdef INCLUDE_SERCOMM_GW //Sercomm PP ehancement
void ti_pp_enable(unsigned int on)
{
	if(on)
		global_ti_hil_db.hil_disabled = 0;
	else
		global_ti_hil_db.hil_disabled = 1;
}
//Enabled (1)  or Disabled (0)
int ti_pp_status(void)
{
	return (global_ti_hil_db.hil_disabled == 0)?1:0;
}
EXPORT_SYMBOL(ti_pp_enable);
EXPORT_SYMBOL(ti_pp_status);
#endif


