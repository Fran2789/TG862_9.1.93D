/*
  GPL LICENSE SUMMARY

  Copyright(c) 2014-2015 Intel Corporation.

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
  The full GNU General Public License is included in this distribution
  in the file called LICENSE.GPL.

  Contact Information:
    Intel Corporation
    2200 Mission College Blvd.
    Santa Clara, CA  97052
*/

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
#include "../../bridge/br_private.h"
#include <linux/proc_fs.h>

#include <asm-arm/arch-avalanche/generic/pal.h>
#include "linux/ti_pp_path.h"

#include <mach/puma.h>
#include <mach/hardware.h>


#include <asm-arm/arch-avalanche/puma6/puma6_pp.h>

#ifndef TI_MAX_DEVICE_INDEX
#define TI_MAX_DEVICE_INDEX 64
#endif




#ifdef CONFIG_NETFILTER
#include <linux/netfilter.h>
#include <net/netfilter/nf_conntrack.h>
#include <net/netfilter/nf_conntrack_helper.h>
#include <linux/netfilter_ipv4/ip_tables.h>
#endif

#include "linux/cat_l2switch_netdev.h"


//-------------------------------------------------------------------
//
// ########  ######## ######## #### ##    ## ########  ######
// ##     ## ##       ##        ##  ###   ## ##       ##    ##
// ##     ## ##       ##        ##  ####  ## ##       ##
// ##     ## ######   ######    ##  ## ## ## ######    ######
// ##     ## ##       ##        ##  ##  #### ##             ##
// ##     ## ##       ##        ##  ##   ### ##       ##    ##
// ########  ######## ##       #### ##    ## ########  ######
//
//-------------------------------------------------------------------

#define IPV6_TRAFFIC_CLASS(h)                       (((h->priority & 0x0F) << 4) | ((h->flow_lbl[0] & 0xF0) >> 4))
#define DEFAULT_SESSION_TIMEOUT_SEC                 1

#define DOCSIS_FW_PACKET_API_TCP_HIGH_PRIORITY      (1 << 10)

/* GEN PP dropped packest, used with TI_CT_GEN_DISCARD_PKT. This is a bitmap. User sets bits according to protocol : IPv4/IPv6 */
#define DROPPED_PACKETS_BITMAP_SET(__n)             do { dropped_packets_bit_map |= (1 << (__n)); } while (0)
#define DROPPED_PACKETS_BITMAP_UNSET(__n)           do { dropped_packets_bit_map &= ~(1 << (__n)); } while (0)
#define DROPPED_PACKETS_BITMAP_IS_SET(__n)          ((dropped_packets_bit_map & (1 << (__n))) != 0)

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



//----------------------------------------------------------------------------------------------
//
//  ######  ######## ########  ##     ##  ######  ######## ##     ## ########  ########  ######
// ##    ##    ##    ##     ## ##     ## ##    ##    ##    ##     ## ##     ## ##       ##    ##
// ##          ##    ##     ## ##     ## ##          ##    ##     ## ##     ## ##       ##
//  ######     ##    ########  ##     ## ##          ##    ##     ## ########  ######    ######
//       ##    ##    ##   ##   ##     ## ##          ##    ##     ## ##   ##   ##             ##
// ##    ##    ##    ##    ##  ##     ## ##    ##    ##    ##     ## ##    ##  ##       ##    ##
//  ######     ##    ##     ##  #######   ######     ##     #######  ##     ## ########  ######
//
//----------------------------------------------------------------------------------------------


typedef struct
{
    Bool    hil_disabled;
    Bool    tdox_disabled;
    Bool    tdox_dbg;
    Bool    qos_disabled;
	Bool	ipv6rd_pp_support;

    Uint32  tdoxEvalMinTimeMsec;
    Uint32  tdoxEvalAvgPktSizeThresh;
    Uint32  tdoxEvalPpsThresh;
    

    Uint32  num_bypassed_pkts;
    Uint32  num_other_pkts;
    Uint32  num_ingress_pkts;
    Uint32  num_egress_pkts;
    Uint32  num_null_drop_pkts;

    Uint32  num_total_sessions;
    Uint32  num_error;
}
global_hil_db_t;


//-----------------------------------------------------------------------------------------------------------------------------------------------
//
// ########  ########   #######  ########  #######  ######## ##    ## ########  ########  ######
// ##     ## ##     ## ##     ##    ##    ##     ##    ##     ##  ##  ##     ## ##       ##    ##
// ##     ## ##     ## ##     ##    ##    ##     ##    ##      ####   ##     ## ##       ##
// ########  ########  ##     ##    ##    ##     ##    ##       ##    ########  ######    ######
// ##        ##   ##   ##     ##    ##    ##     ##    ##       ##    ##        ##             ##
// ##        ##    ##  ##     ##    ##    ##     ##    ##       ##    ##        ##       ##    ##
// ##        ##     ##  #######     ##     #######     ##       ##    ##        ########  ######
//
//-----------------------------------------------------------------------------------------------------------------------------------------------

/************************************************/
/* HIL Profile Functions                        */
/************************************************/
static Int32 hil_intrusive_init(void);
static Int32 hil_intrusive_deinit(void);
static int hil_netsubsystem_event_handler(unsigned int module_id, unsigned long event_id, void* ptr);
static Int32 hil_netsubsystem_pp_handler(Uint32 event_id, void* ptr);
static AVALANCHE_PP_RET_e hil_pp_event_handler(AVALANCHE_PP_EVENT_e event_id, Uint32 param1, Uint32 param2);

/************************************************/
/* Ingress Hook Functions                       */
/************************************************/
Int32 hil_ingress_hook(struct sk_buff* skb);
static Int32 hil_extract_packet_ingress(struct sk_buff* skb, struct net_device* dev);
static Int32 hil_extract_l2_ingress(struct ethhdr* ptr_ethhdr, AVALANCHE_PP_INGRESS_SESSION_PROPERTY_t* ingress_property, Uint16 vpid_vlan_tci);
static Int32 hil_extract_ipv4_ingress(struct iphdr* ptr_iphdr, struct tcphdr** ptr_tcphdr, AVALANCHE_PP_INGRESS_SESSION_PROPERTY_t* ingress_property);
static Int32 hil_extract_ipv6_ingress(struct ipv6hdr* ptr_ipv6hdr, struct tcphdr** ptr_tcphdr, AVALANCHE_PP_INGRESS_SESSION_PROPERTY_t* ingress_property);
static Int32 hil_extract_l4_ingress(struct tcphdr* ptr_tcphdr, AVALANCHE_PP_INGRESS_SESSION_PROPERTY_t* ingress_property);

/************************************************/
/* Egress Hook Functions                        */
/************************************************/
Int32 hil_egress_hook(struct sk_buff* skb);
static Int32 hil_extract_packet_egress(struct sk_buff* skb, struct net_device* dev, AVALANCHE_PP_SESSION_INFO_t* session_info);
static Int32 hil_extract_l2_egress(struct ethhdr* ptr_ethhdr, struct sk_buff* skb, AVALANCHE_PP_EGRESS_SESSION_PROPERTY_t* egress_property);
static Int32 hil_extract_ipv4_egress(struct ethhdr* ptr_ethhdr, struct iphdr* ptr_iphdr, struct tcphdr** ptr_tcphdr, AVALANCHE_PP_SESSION_INFO_t* session_info);
static Int32 hil_extract_ipv6_egress(struct ethhdr* ptr_ethhdr, struct ipv6hdr* ptr_ipv6hdr, struct tcphdr** ptr_tcphdr, struct iphdr** ptr_dsLiteIphdr, AVALANCHE_PP_EGRESS_SESSION_PROPERTY_t* egress_property);
static Int32 hil_extract_l4_egress(struct iphdr* ptr_iphdr, struct ipv6hdr* ptr_ipv6hdr, struct tcphdr* ptr_tcphdr, struct iphdr* ptr_dsLiteIphdr, AVALANCHE_PP_EGRESS_SESSION_PROPERTY_t* egress_property);
static Int32 hil_session_intelligence(AVALANCHE_PP_SESSION_INFO_t* session_info);
static Int32 hil_choose_session_pool(AVALANCHE_PP_SESSION_INFO_t* session_info);

/************************************************/
/* NULL Hook Functions                          */
/************************************************/
Int32 hil_null_hook(struct sk_buff* skb, Int32 null_vpid);

/************************************************/
/* General Packet Parsing Functions             */
/************************************************/
static Int32 hil_get_l2_l3_pointers(Int8* ptr_data, struct ethhdr** ptr_ethhdr, struct iphdr** ptr_iphdr, struct ipv6hdr** ptr_ipv6hdr);
static Uint16 hil_ipv4_checksum(Uint8 *ipv4Header, Uint16 ipv4HeaderLen);
static Int32 hil_scan_ipv6(struct ipv6hdr* ptr_ipv6hdr, Uint8* ipv6HeaderLen, Uint8* nexthdr);

/************************************************/
/* MFC Functions                                */
/************************************************/
#ifdef CONFIG_IP_MULTICAST
static Int32 hil_mfc_delete_session_by_mc_group(Int32 mc_group);
#endif /* CONFIG_IP_MULTICAST */


/************************************************/
/* TDOX Functions                               */
/************************************************/
static Int32 hil_tdox_check_and_enable(struct tcphdr* ptr_tcphdr, AVALANCHE_PP_SESSION_INFO_t* session_info);
AVALANCHE_PP_RET_e hil_tdox_manager(AVALANCHE_PP_SESSION_INFO_t *session_info, Ptr data);

/************************************************/
/* PROC Functions                               */
/************************************************/
static int hil_write_cmds(struct file *file, const char *buffer, unsigned long  count, void *data);
static Int32 hil_show_cmd_handler(Int32 argc, Int8* argv[]);
static Int32 hil_set_cmd_handler(Int32 argc, Int8* argv[]);
static Int32 hil_reset_cmd_handler(Int32 argc, Int8* argv[]);
static int hil_read_devs(char* buf, char **start, off_t offset, int count, int *eof, void *data);
static void hil_test_session_creation(void);
void hil_delete_drop_session(Uint32 src_ip,Uint32 dst_ip,Uint16 src_port, Uint16 dst_port) ;

/************************************************/
/* Connection Tracking Function                 */
/************************************************/
#ifdef CONFIG_NETFILTER
static void hil_add_session_handle_to_ct_mapper_list (Uint16 new_session_handle,Uint16 prev_new_session_handle);
static Bool  hil_find_session_handle_in_ct_mapper_list (Uint16 ct_session_handle,Uint16 pp_session_handle);
static void hil_delete_session_handle_in_ct_mapper_list (Uint16 pp_session_handle,enum ip_conntrack_dir dir);
#endif


/************************************************/
/* extern Functions                             */
/************************************************/
extern Int32 ti_register_egress_hook_handler(struct net_device* dev, Int32 (*egress_hook)(struct sk_buff *skb));
extern Int32 ti_deregister_egress_hook_handler(struct net_device* dev);
#ifdef CONFIG_TI_PACKET_PROCESSOR_STATS
extern TI_HIL_START_SESSION ti_hil_start_session_notification_cb;
extern TI_HIL_DELETE_SESSION ti_hil_delete_session_notification_cb;
#endif


//-----------------------------------------------------------------------------------------------------------------------------------------------
//
//  ######   ##        #######  ########     ###    ##          ##     ##    ###    ########  ####    ###    ########  ##       ########  ######
// ##    ##  ##       ##     ## ##     ##   ## ##   ##          ##     ##   ## ##   ##     ##  ##    ## ##   ##     ## ##       ##       ##    ##
// ##        ##       ##     ## ##     ##  ##   ##  ##          ##     ##  ##   ##  ##     ##  ##   ##   ##  ##     ## ##       ##       ##
// ##   #### ##       ##     ## ########  ##     ## ##          ##     ## ##     ## ########   ##  ##     ## ########  ##       ######    ######
// ##    ##  ##       ##     ## ##     ## ######### ##           ##   ##  ######### ##   ##    ##  ######### ##     ## ##       ##             ##
// ##    ##  ##       ##     ## ##     ## ##     ## ##            ## ##   ##     ## ##    ##   ##  ##     ## ##     ## ##       ##       ##    ##
//  ######   ########  #######  ########  ##     ## ########       ###    ##     ## ##     ## #### ##     ## ########  ######## ########  ######
//
//-----------------------------------------------------------------------------------------------------------------------------------------------
Int32 pp_session_timeout_sec = DEFAULT_SESSION_TIMEOUT_SEC;   /* This is the default session IDLE timeout in seconds. */

global_hil_db_t global_hil_db =
{
    .hil_disabled               =   False,
    .tdox_disabled              =   False,
    .ipv6rd_pp_support			= 	False,
    .tdox_dbg                   =   False,
    .qos_disabled               =   False,
    .tdoxEvalMinTimeMsec        =   3000,
    .tdoxEvalAvgPktSizeThresh   =   128,
    .tdoxEvalPpsThresh          =   100,
    .num_bypassed_pkts          =   0,
    .num_other_pkts             =   0,
    .num_ingress_pkts           =   0,
    .num_egress_pkts            =   0,
    .num_null_drop_pkts         =   0,
    .num_total_sessions         =   0,
    .num_error                  =   0
};

/* Default Profile. */
TI_HIL_PROFILE hil_intrusive_profile = 
{
    .name            = "intrusive_pp2k",
    .profile_handler = hil_netsubsystem_event_handler,
    .profile_init    = hil_intrusive_init,
    .profile_deinit  = hil_intrusive_deinit,
};

Uint32 pp_event_handler;

#ifdef CONFIG_NETFILTER
/* This is a global data structure which maps the session handle to corresponding connection tracking entries. */
struct ct_mapper
{
    Uint16 next;
    Uint16 previous;
    struct nf_conn* conntrack;

};
struct ct_mapper hil_session_ct_mapper [AVALANCHE_PP_MAX_ACCELERATED_SESSIONS];
#endif

/* Packet terminating VPID handle */
static Int32  docsis_vpid_handle = -1;
static Int32  docsis_null_vpid_handle = -1;
static Int32  netfilter_null_vpid_handle = -1;

static Int32 dropped_packets_bit_map = 0;

Uint8   *usPktDataIngressP = NULL;
Uint16   usPktDataIngressSize = 0;
Uint8   *usPktDataEgressP = NULL;
Uint16   usPktDataEgressSize = 0;
Uint8   *dsPktDataIngressP = NULL;
Uint16   dsPktDataIngressSize = 0;
Uint8   *dsPktDataEgressP = NULL;
Uint16   dsPktDataEgressSize = 0;


//-----------------------------------------------------------------------------------------------------------------------------------------------
//
// ##        #######   ######     ###    ##          ######## ##     ## ##    ##  ######  ######## ####  #######  ##    ##  ######
// ##       ##     ## ##    ##   ## ##   ##          ##       ##     ## ###   ## ##    ##    ##     ##  ##     ## ###   ## ##    ##
// ##       ##     ## ##        ##   ##  ##          ##       ##     ## ####  ## ##          ##     ##  ##     ## ####  ## ##
// ##       ##     ## ##       ##     ## ##          ######   ##     ## ## ## ## ##          ##     ##  ##     ## ## ## ##  ######
// ##       ##     ## ##       ######### ##          ##       ##     ## ##  #### ##          ##     ##  ##     ## ##  ####       ##
// ##       ##     ## ##    ## ##     ## ##          ##       ##     ## ##   ### ##    ##    ##     ##  ##     ## ##   ### ##    ##
// ########  #######   ######  ##     ## ########    ##        #######  ##    ##  ######     ##    ####  #######  ##    ##  ######
//
//-----------------------------------------------------------------------------------------------------------------------------------------------


/**************************************************************************
 * FUNCTION NAME : hil_intrusive_init
 **************************************************************************
 * DESCRIPTION:
 *  Initialization function for the PP2K Intrusive mode profile.
 *
 * RETURNS:
 *  0   -   Success
 *  <0  -   Error
 **************************************************************************/
static Int32 hil_intrusive_init(void)
{
    struct proc_dir_entry*  ptr_dir_entry;
    struct proc_dir_entry*  ptr_dev_entry;
#ifdef CONFIG_NETFILTER
    int                     index = 0;
#endif

    /* Register an event handler to listen to events. */
    if (avalanche_pp_event_handler_register(&pp_event_handler, hil_pp_event_handler) != PP_RC_SUCCESS)
    {
        printk ("Error: Event Handler register failed\n");
        return -1;
    }

    /* Create the PROC Entry used by the TCA Configuration Engine. */
    ptr_dir_entry = create_proc_entry("ti_pp" ,0644, init_net.proc_net);
    ptr_dev_entry = create_proc_entry("ti_pp_dev" ,0644, init_net.proc_net);

    if( (ptr_dir_entry == NULL) || (ptr_dev_entry == NULL ) )
    {
        printk ("Error: Unable to create Packet Processor proc entry.\n");
        return -1;
    }
    ptr_dir_entry->data      = NULL;
    ptr_dir_entry->read_proc  = NULL;
    ptr_dir_entry->write_proc = hil_write_cmds;


    ptr_dev_entry->data      = NULL;
    ptr_dev_entry->read_proc  = hil_read_devs;
    ptr_dev_entry->write_proc = NULL;;

#ifdef CONFIG_NETFILTER
    /* Initialize the HIL Session to Connection Tracking Mapper. */
    for (index = 0; index < AVALANCHE_PP_MAX_ACCELERATED_SESSIONS; index++)
    {
        hil_session_ct_mapper[index].conntrack = NULL;
        hil_session_ct_mapper[index].next = TI_PP_SESSION_CT_IDLE;
        hil_session_ct_mapper[index].previous = TI_PP_SESSION_CT_IDLE;
    }
#endif /* CONFIG_NETFILTER */

    /* Initialize PP Path */
    PPP_Init();

    FREE_RUNNING_COUNTER_ENABLE();

    /* Profile has been successfully initialized. */
    return 0;
}

/**************************************************************************
 * FUNCTION NAME : hil_intrusive_deinit
 **************************************************************************
 * DESCRIPTION   :
 *  Deinitialization function which deinitializes and unregisters the default
 *  profile with the HIL Core.
 *
 * RETURNS       :
 *  0   -   Success
 *  <0  -   Error
 **************************************************************************/
static Int32 hil_intrusive_deinit(void)
{
    return 0;
}

#ifdef CONFIG_NETFILTER
AVALANCHE_PP_RET_e    __hil_delete_drop_session( AVALANCHE_PP_SESSION_INFO_t *  ptr_session, Ptr     data )
{
    int verdict = 0;
    struct nf_conn *    conntrack = (struct nf_conn *)data;

    Uint32  ip_cmp_size = 4;

    if (AVALANCHE_PP_LUT_ENTRY_L3_IPV4 != ptr_session->ingress.lookup.LUT1.u.fields.L3.entry_type)
    {
        return (PP_RC_SUCCESS);
    }

    if (AVALANCHE_PP_DEV_TYPE_WAN == ptr_session->ingress.dev_type)
    {
        verdict |= memcmp( &conntrack->tuplehash[ IP_CT_DIR_REPLY ].tuple.src.u3.ip,   &ptr_session->ingress.lookup.LUT2.u.fields.WAN_addr_IP.v4   , ip_cmp_size );
        verdict |= memcmp( &conntrack->tuplehash[ IP_CT_DIR_REPLY ].tuple.dst.u3.ip,   &ptr_session->ingress.lookup.LUT1.u.fields.L3.LAN_addr_IP.v4, ip_cmp_size );
    } 
    else
    {
        verdict |= memcmp( &conntrack->tuplehash[ IP_CT_DIR_REPLY ].tuple.dst.u3.ip,   &ptr_session->ingress.lookup.LUT2.u.fields.WAN_addr_IP.v4   , ip_cmp_size );
        verdict |= memcmp( &conntrack->tuplehash[ IP_CT_DIR_REPLY ].tuple.src.u3.ip,   &ptr_session->ingress.lookup.LUT1.u.fields.L3.LAN_addr_IP.v4, ip_cmp_size );
    }

    verdict |= memcmp( &conntrack->tuplehash[ IP_CT_DIR_REPLY ].tuple.dst.u.all, &ptr_session->ingress.lookup.LUT2.u.fields.L4_DstPort, 2 );
    verdict |= memcmp( &conntrack->tuplehash[ IP_CT_DIR_REPLY ].tuple.src.u.all, &ptr_session->ingress.lookup.LUT2.u.fields.L4_SrcPort, 2 );

    if (0 == verdict)
    {
        avalanche_pp_session_delete( ptr_session->session_handle, NULL );
    }

    return (PP_RC_SUCCESS);
}
#endif


/**************************************************************************
 * FUNCTION NAME : hil_netsubsystem_event_handler
 **************************************************************************
 * DESCRIPTION   :
 *  This is the HIL Intrusive event handler which will capture all events from the networking sub-system.
 *
 * RETURNS       :
 *  0   -   Success
 *  <0  -   Error
 **************************************************************************/
static int hil_netsubsystem_event_handler(unsigned int module_id, unsigned long event_id, void* ptr)
{
    /* Process based on the module identifier */
    switch (module_id)
    {
        case TI_PP:
        {
            hil_netsubsystem_pp_handler(event_id, ptr);
            break;
        }

        default:
            break;
    }
    return 0;
}

/**************************************************************************
 * FUNCTION NAME : hil_netsubsystem_pp_handler
 **************************************************************************
 * DESCRIPTION   :
 *  Default HIL PP Handler.
 *
 * RETURNS       :
 *  0   -   Success
 *  <0  -   Error
 **************************************************************************/
static Int32 hil_netsubsystem_pp_handler(Uint32 event_id, void* ptr)
{
    struct net_device*           dev, *input_dev;
    struct sk_buff*              skb;
#ifdef CONFIG_NETFILTER
    struct nf_conn*             conntrack;
    struct xt_table*            t;
#endif

    /* Process the events. */
    switch (event_id)
    {
        case TI_DOCSIS_FLTR_DISCARD_PKT:
        {
            Uint32 lockKey;

            PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);

            skb = (struct sk_buff*) ptr;
            input_dev = dev_get_by_index(&init_net, skb->skb_iif);
            if ((docsis_null_vpid_handle == -1) && (input_dev && input_dev->vpid_handle != -1))
            {
                if (avalanche_pp_vpid_create(&input_dev->vpid_block) != PP_RC_SUCCESS)
                {
                    printk ("Error: Unable to create Null VPID %d for Device %s\n", docsis_null_vpid_handle, input_dev->name);
                }
                else
                {
                    docsis_null_vpid_handle = input_dev->vpid_block.vpid_handle;
                    avalanche_pp_vpid_set_flags(docsis_null_vpid_handle, AVALANCHE_PP_VPID_FLG_TX_DISBL | AVALANCHE_PP_VPID_FLG_RX_DISBL);
                }
            }

            if (!WARN(input_dev == NULL, "%s - %d: dev_get_by_index(&init_net,skb->skb_iif %d); returned NULL", __func__, __LINE__, skb->skb_iif))
            {
                dev_put(input_dev); 
            }

            hil_null_hook(skb, docsis_null_vpid_handle);
            PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
            break;
        }

        case TI_DOCSIS_FLTR_ADD:
        case TI_DOCSIS_FLTR_DEL:
        case TI_DOCSIS_FLTR_CHG:
        case TI_DOCSIS_CLASSIFY_ADD:
        case TI_DOCSIS_CLASSIFY_DEL:
        case TI_DOCSIS_CLASSIFY_CHG:
        case TI_DOCSIS_MCAST_DEL:
        case TI_DOCSIS_DSID_CHG:
        {
            avalanche_pp_flush_sessions( AVALANCHE_PP_MAX_VPID, PP_LIST_ID_ALL );
            break;
        }

        case TI_DOCSIS_SESSIONS_DEL:
        {
            Uint32 numSessions = ((Uint32*)ptr)[0];
            Uint32 *sessList = &(((Uint32*)ptr)[1]);
            Uint32 i;
            for (i = 0; i < numSessions; i++)
            {
                avalanche_pp_session_delete(sessList[i], NULL);

                if (global_hil_db.tdox_dbg)
                {
                    printk("TDOX-DBG DeleteSession: ses=%d\n", sessList[i]);
                }
            }

            break;
        }

        case TI_DOCSIS_VOICE_SESSIONS_DEL:
        {
            static struct net_device *ptr_voiceni_dev;
            ptr_voiceni_dev = dev_get_by_name(&init_net, "vni0");
			if (ptr_voiceni_dev)
            {
                avalanche_pp_flush_sessions( ptr_voiceni_dev->vpid_handle, PP_LIST_ID_ALL );
                printk("Flush all VOICE sessions\n");
                dev_put(ptr_voiceni_dev);
            }
            break; 
        }

        case TI_PP_ADD_VPID:            /* Event to add vpid, for example when GW add vlan to LSD */
        case TI_BRIDGE_PORT_FORWARD:
        {
            /* Event indicates that a port attached to the bridge has moved to the FORWARDING state.
             * The host bridge will only forward packets in this state. Thus this is the time we create the VPID */
            dev = (struct net_device*)ptr;

            /* Do not create any VPIDs if there is no parent PID defined */
            if (dev->vpid_block.parent_pid_handle == (Uint8)(-1))
            {
                break;
            }

            if (dev->vpid_handle == -1)
            {
                AVALANCHE_PP_PID_t *ptr_pid;

                if (!global_hil_db.qos_disabled)
                {
                    if (NULL != dev->qos_setup_hook)
                    {
                        if (NETDEV_PP_QOS_PROFILE_DEFAULT != dev->qos_virtual_scheme_idx)
                        {
                            dev->qos_setup_hook(dev);
                        }
                    }
                }

                /* Create the VPID */
                if (avalanche_pp_vpid_create(&dev->vpid_block) != PP_RC_SUCCESS)
                {
                    printk ("Error: Unable to create VPID %d PID %d Device %s\n", dev->vpid_handle, dev->pid_handle, dev->name);
                    return -1;
                }
                dev->vpid_handle = dev->vpid_block.vpid_handle;

                if (avalanche_pp_pid_get_info(dev->vpid_block.parent_pid_handle, &ptr_pid) == PP_RC_SUCCESS)
                {
                    if (ptr_pid->type == AVALANCHE_PP_PID_TYPE_DOCSIS)
                    {
                        docsis_vpid_handle = dev->vpid_handle;
                    }
                }
                
            }

            {
                int rc ;
                rc = ti_register_protocol_handler(dev, hil_ingress_hook);     /* Install the Ingress Hook. */
                printk("====> PP2K Ingress registered %d [%p] <=====\n", rc, hil_ingress_hook);

                rc = ti_register_egress_hook_handler(dev, hil_egress_hook);   /* Install the Egress Hook. */
                printk("====> PP2K Egress registered %d [%p] <=====\n", rc, hil_egress_hook);
            }
            break;
        }

        case TI_PP_REMOVE_VPID:         /* Event to remove vpid, for example when GW add vlan to LSD */
        case TI_BRIDGE_PORT_DELETE:     /* Event indicates that a device has been removed from the bridge. This event is generated when the user executes the brctl delif command to remove a port from the bridge */
        case TI_BRIDGE_PORT_DISABLED:   /* Event indicates that a port attached to the bridge has been moved to the disabled state */
        {
            dev = (struct net_device*)ptr;

            if (dev->vpid_block.parent_pid_handle == (Uint8)(-1))
            {
                /* Do not touch VPIDs that do not have any parent PID defined */
                break;
            }

            /* If the device has a VPID handle; it needs to be removed from the packet processor. */
            if (dev->vpid_handle != -1)
            {
                /* Delete the VPID */
                if (avalanche_pp_vpid_delete(dev->vpid_handle) != PP_RC_SUCCESS)
                {
                    printk ("Error: Unable to delete VPID %d PID %d Device %s\n", dev->vpid_handle, dev->pid_handle, dev->name);
                    return -1;
                }
                dev->vpid_handle = -1;

                if (event_id == TI_PP_REMOVE_VPID || event_id == TI_BRIDGE_PORT_DISABLED)
                {
                    AVALANCHE_PP_PID_t *ptr_pid;

                    avalanche_pp_pid_get_info(dev->vpid_block.parent_pid_handle, &ptr_pid);
                    if (ptr_pid->type == AVALANCHE_PP_PID_TYPE_ETHERNET)
                    {
                        Uint32 queueIndex;
                        Uint32 divertCommand;
                        Uint32 qmgr = PAL_CPPI41_QUEUE_MGR_PARTITION_SR;
                        PAL_Handle handle = PAL_cppi4Init (NULL,(Ptr)PAL_CPPI41_QUEUE_MGR_PARTITION_SR);

                        for (queueIndex = 0; queueIndex < 1; queueIndex++)
                        {
                            divertCommand =  (PAL_CPPI41_RECYCLE_INFRA_INPUT_LOW_Q_NUM << 16);  // Setup destination Queue
                            divertCommand += ptr_pid->tx_pri_q_map[queueIndex];                 // Setup source Queue

                            PAL_cppi4Control( handle, PAL_CPPI41_IOCTL_QUEUE_DIVERT, (Ptr)divertCommand, &qmgr );
                        }

                        PAL_cppi4Exit(handle, NULL);
                    }
                }
            }

            /* Uninstall the Ingress & Egress Hooks */
            ti_deregister_protocol_handler (dev);
            ti_deregister_egress_hook_handler (dev);
            break;
        }

        case TI_BRIDGE_FDB_CREATED:
        case TI_BRIDGE_FDB_DELETED:
        {
            break;
        }
        
        case TI_BRIDGE_PACKET_FLOODED:
        {
            /* Event indicates that the packet will now be flooded onto all interfaces. This can happen in any of the following cases:-
             *  a) Unicast packet but no matching FDB entry is found.
             *  b) Broadcast packet
             *  c) Multicast packet but no layer2 extensions eg IGMP snooping exists */
            skb = (struct sk_buff*) ptr;

            /* In the intrusive mode profile these packets are not considered as candidates for acceleration so mark the packet BYPASS mode so that the egress hook is bypassed */
            skb->pp_packet_info.flags |= TI_HIL_PACKET_FLAG_PP_SESSION_BYPASS;
            break;
        }

        case TI_ROUTE_ADDED:
        case TI_ROUTE_DELETED:
        {
            /*  Event indicates that a new route is being added/deleted. A route change could affect the existing sessions in the PP.
             *  So, the HIL profile  could choose to delete all sessions that are affected by the route change / flush the entire session table to make sure that there is no inconsistency due to the route change */
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
                conntrack->timeout.expires = (pp_session_timeout_sec * HZ) + jiffies;
                add_timer(&conntrack->timeout);
            }
            break;
        }

        case TI_CT_NETFILTER_TABLE_UPDATE:
        {
            t = (struct xt_table *)ptr; /* Get the netfilter table */

            /* Flush all sessions only for NAT... No need to do anything for Mangle and Firewall */
            if (strcmp (t->name, "nat") == 0 || strcmp (t->name, "filter") == 0)
            {
                if (avalanche_pp_flush_sessions( AVALANCHE_PP_MAX_VPID, PP_LIST_ID_ALL ) != PP_RC_SUCCESS)
                {
                    printk ("Error: Unable to flush all sessions\n");
                    return 0;
                }
                printk ("NAT Table update all sessions flushed\n");
            }
            break;
        }

        case TI_CT_NETFILTER_CANCEL_DISCARD_ACCELERATION:
        {
            avalanche_pp_session_list_execute( netfilter_null_vpid_handle,  PP_LIST_ID_EGRESS, __hil_delete_drop_session, ptr );
            avalanche_pp_session_list_execute( docsis_null_vpid_handle,     PP_LIST_ID_EGRESS, __hil_delete_drop_session, ptr );
            break;
        }
#endif

        case TI_IP_DISCARD_PKT_IPV4:
        case TI_IP_DISCARD_PKT_IPV6:
            if (((event_id == TI_IP_DISCARD_PKT_IPV4) && DROPPED_PACKETS_BITMAP_IS_SET(4)) ||
                ((event_id == TI_IP_DISCARD_PKT_IPV6) && DROPPED_PACKETS_BITMAP_IS_SET(6)))
            {
                /* Hanlde event*/
            }
            else
            {
			    break;  /* Appropriate bit is not set, discard event */
            }
#ifdef CONFIG_NETFILTER
        case TI_CT_NETFILTER_DISCARD_PKT:
#endif
        {
            Uint32 lockKey;

            PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);

            skb = (struct sk_buff*) ptr;

            if (!(skb->skb_iif))
            {
                PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
                break;
            }
            input_dev = dev_get_by_index(&init_net,skb->skb_iif);
            
            if ((netfilter_null_vpid_handle == -1) && (input_dev && input_dev->vpid_handle != -1) && (!avalanche_pp_state_is_psm()))
            {
                if (avalanche_pp_vpid_create(&input_dev->vpid_block) != PP_RC_SUCCESS)
                {
                    printk ("Error: Unable to create Null VPID %d Device %s\n", netfilter_null_vpid_handle, input_dev->name);
                }
                else
                {
                    netfilter_null_vpid_handle = input_dev->vpid_block.vpid_handle;
                    avalanche_pp_vpid_set_flags(netfilter_null_vpid_handle, AVALANCHE_PP_VPID_FLG_TX_DISBL | AVALANCHE_PP_VPID_FLG_RX_DISBL);
                }
            }

            hil_null_hook(skb, netfilter_null_vpid_handle);
            if (!WARN(input_dev == NULL, "%s - %d: dev_get_by_index(&init_net,skb->skb_iif %d); returned NULL", __func__, __LINE__, skb->skb_iif))
            {
                dev_put(input_dev); 
            }
            PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
            break;
        }

#ifdef CONFIG_IP_MULTICAST
        case TI_MC_SESSION_DELETED:
        {
            struct mfc_cache* ptr_mfc_cache;
            if(NULL == ptr)
            {
                printk ("FATAL Error: Multicast params is NULL\n");
                break;
            }

            ptr_mfc_cache = (struct mfc_cache* )ptr;
            hil_mfc_delete_session_by_mc_group(ptr_mfc_cache->mfc_mcastgrp);
            break;
        }
#endif

#ifdef CONFIG_VLAN_8021Q
        case TI_VLAN_DEV_CREATED:
        {
            /* Get the pointer to the network device. */
            dev = (struct net_device*)ptr;

            /* We know for sure that this is attached to a VLAN interface, so configure the VPID Information Block appropriately.
             * The egress MTU of the interface needs to be correctly handled to account for the VLAN Header */
            dev->vpid_block.type            = AVALANCHE_PP_VPID_VLAN;
            dev->vpid_block.vlan_identifier = (vlan_dev_info(dev))->vlan_id;
            break;
        }

        case TI_VLAN_DEV_DELETED:
        {
            break;
        }
#endif

#ifdef CONFIG_PPPOE
        case TI_PPP_INTERFACE_CREATED:
        {
            /* Get the pointer to the network device. */
            dev = (struct net_device*)ptr;

            /* PPPoE could be executed over a VLAN connection or on a vanilla Ethernet connection. Configure the VPID device type appropriately. */
            if (dev->vpid_block.type == AVALANCHE_PP_VPID_VLAN)
            {
                /* The PPP connection is initialized over a VLAN connection. Configure the Egress MTU appropriately accounting for the PPP and VLAN headers. */
                dev->vpid_block.type       = AVALANCHE_PP_VPID_VLAN_PPPoE;
                dev->vpid_block.egress_mtu = dev->vpid_block.egress_mtu - sizeof(struct pppoe_hdr) - 2 - VLAN_HLEN;
            }
            else
            {
                /* The PPP Connection is being bought over a vanilla Ethernet connection. Configure the Egress MTU appropriately accounting only for the PPP header */
                dev->vpid_block.type        = AVALANCHE_PP_VPID_PPPoE;
                dev->vpid_block.egress_mtu  = dev->vpid_block.egress_mtu - sizeof(struct pppoe_hdr) - 2;
            }

            /* Extract and configure the PPP Session ID. */
            dev->vpid_block.ppp_session_id  = dev->padded;
            break;
        }
#endif

        default:
        {
            printk ("Intrusive -> Does not handle event 0x%x\n", event_id);
            break;
        }
    }

    /* Successfully handled the event. */
    return 0;
}


/**************************************************************************
 * FUNCTION NAME : hil_pp_event_handler
 **************************************************************************
 * DESCRIPTION   :
 *  The function is the registered event handler which listens to all events
 *  that arise from the Packet Processor Subsystem.
 **************************************************************************/
static AVALANCHE_PP_RET_e hil_pp_event_handler(AVALANCHE_PP_EVENT_e event_id, Uint32 param1, Uint32 param2)
{
    /* Process each event based on the module which generated the event. */
    switch (event_id)
    {
        case PP_EV_PID_CREATED:
        {
            struct net_device *dev = (struct net_device *)param2;

            if (!global_hil_db.qos_disabled)
            {
                if ((NULL != dev) && (NULL != dev->qos_setup_hook))
                {
                    dev->qos_setup_hook( dev );
                }
            }
            printk ("PP Operation PP_EV_PID_CREATED, pid_handle=%d @ %s device\n", param1, param2 ? dev->name : "NULL" );
            break;
        }

        case PP_EV_PID_DELETED:
        {
            printk ("PP Operation PP_EV_PID_DELETED, pid_handle=%d\n", param1);
            break;
        }

        case PP_EV_VPID_CREATED:
        {
            printk ("PP Operation PP_EV_VPID_CREATED, vpid_handle=%d\n", param1);
            break;
        }
        case PP_EV_VPID_DELETED:
        {
            printk ("PP Operation PP_EV_VPID_DELETED, vpid_handle=%d\n", param1);
            break;
        }

        case PP_EV_SESSION_CREATED:
        {
#ifdef CONFIG_NETFILTER
            struct  sk_buff* skb = (struct sk_buff*)param2;
#endif
            global_hil_db.num_total_sessions++;

#ifdef CONFIG_TI_PACKET_PROCESSOR_STATS
            if (ti_hil_start_session_notification_cb)
            {
                if (((struct sk_buff*)param2)->pp_packet_info.pp_session.egress.vpid_handle == docsis_null_vpid_handle)
                {
                    ti_hil_start_session_notification_cb(param1, TI_DOCSIS_PP_SESSION_TYPE_DISCARDING, ((struct sk_buff*)param2));
                }
                else
                {
                    ti_hil_start_session_notification_cb(param1, TI_DOCSIS_PP_SESSION_TYPE_FORWARDING, ((struct sk_buff*)param2));
                }
            }
#endif

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
                                printk ("ERROR --> Existing session %d new session %d.\n", ct_session, param1);
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

        case PP_EV_SESSION_EXPIRED:
        {
            if (avalanche_pp_session_delete(param1, NULL) < 0)
            {
                printk ("Error: Unable to delete session %d\n", param1);
                return PP_RC_FAILURE;
            }

            if (global_hil_db.tdox_dbg)
            {
                printk("TDOX-DBG DeleteSession: ses=%d\n", param1);
            }
            break;
        }

        case PP_EV_SESSION_DELETED:
        {
#ifdef CONFIG_TI_PACKET_PROCESSOR_STATS
            if (ti_hil_delete_session_notification_cb)
            {
                AVALANCHE_PP_SESSION_STATS_t *session_stats = (AVALANCHE_PP_SESSION_STATS_t*)param2;
                
                session_stats->bytes_forwarded += (4 * session_stats->packets_forwarded);  /* add the 4 bytes the PP is ignoring for each packet */

                ti_hil_delete_session_notification_cb(param1, session_stats->packets_forwarded, session_stats->bytes_forwarded);
            }
#endif

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
    
    
        case PP_EV_MISC_TRIGGER_TDOX_EVALUATION:
        {
            if (!global_hil_db.tdox_disabled)
            {
                avalanche_pp_session_list_execute(docsis_vpid_handle, PP_LIST_ID_EGRESS_TCP, hil_tdox_manager, NULL);
            }
            break;
        }

        case PP_EV_PID_CREATE_FAIL:
        {
            struct net_device *dev = (struct net_device *)param2;

            printk("FATAL Error: PP Operation PP_EV_PID_CREATE_FAIL, pid_handle=%d @ %s device\n", param1, param2 ? dev->name : "NULL");
            break;
        }

        case PP_EV_PID_DELETE_FAIL:
        {
            printk("FATAL Error: PP Operation PP_EV_PID_DELETE_FAIL, pid_handle=%d\n", param1);
            break;
        }

        case PP_EV_VPID_CREATE_FAILED:
        {
            printk("FATAL Error: PP Operation PP_EV_VPID_CREATE_FAILED, vpid_handle=%d\n", param1);
            break;
        }

        case PP_EV_VPID_DELETE_FAILED:
        {
            printk("FATAL Error: PP Operation PP_EV_VPID_DELETE_FAILED, vpid_handle=%d\n", param1);
            break;
        }

        case PP_EV_SESSION_CREATE_FAILED:
        {
            printk("Error: PP Operation PP_EV_SESSION_CREATE_FAILED, session_handle=%d\n", param1);
            break;
        }

        case PP_EV_SESSION_DELETE_FAILED:
        {
            printk("Error: PP Operation PP_EV_SESSION_DELETE_FAILED, session_handle=%d\n", param1);
            break;
        }

        default:
        {
            printk ("FATAL Error: Unknown Event 0x%x\n", event_id);
            break;
        }
    }

    return PP_RC_SUCCESS;
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
        
    	while (hil_session_ct_mapper[cur_session].previous != ct_session_handle && (count < AVALANCHE_PP_MAX_ACCELERATED_SESSIONS))
    	{
        	if (hil_session_ct_mapper[cur_session].previous == session_handle)
            {
            	rc = true;
                break;
            }	
        	cur_session = hil_session_ct_mapper[cur_session].previous;
        	if (cur_session >= AVALANCHE_PP_MAX_ACCELERATED_SESSIONS)
        	{
                printk(KERN_ERR"Invalid session - %d \n", cur_session);
                break;
        	}
        	
        	count++;
        }

        if (count >= AVALANCHE_PP_MAX_ACCELERATED_SESSIONS)
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
 * FUNCTION NAME : hil_ingress_hook
 **************************************************************************
 * DESCRIPTION   :
 *  The function is registered as the device specific protocol handler for all networking devices which exist in the system which have a valid VPID.
 *  The function extracts the information pertinent to creation of the session interface and stores it in the SKB.
 *
 * RETURNS:
 *  Always returns 0.
 **************************************************************************/
Int32 hil_ingress_hook(struct sk_buff* skb)
{
    if (global_hil_db.hil_disabled || (!avalanche_pp_state_is_active()))
    {
        return 0;
    }

    /* Extract all the fields from the packet and populate the Ingress Packet Descriptor. */
    if (hil_extract_packet_ingress(skb, skb->dev) < 0)
    {
        return 0;
    }

    global_hil_db.num_ingress_pkts++;

    /* Packet has passed through the Ingress Hooks. */
    skb->pp_packet_info.flags |= TI_HIL_PACKET_FLAG_PP_SESSION_INGRESS_RECORDED;
    skb->pp_packet_info.pp_session.priority = skb->ti_meta_info & 0x7;
    skb->pp_packet_info.pp_session.cluster = 0;

    return 0;
}


/**************************************************************************
 * FUNCTION NAME : hil_extract_packet_ingress
 **************************************************************************
 * DESCRIPTION   :
 *  The function is called to extract the various Layer2, Layer3 and Layer4 fields and populate the packet description structure for ingress packet
 *
 * RETURNS       :
 *  0   -   Success
 *  <0  -   Error
 **************************************************************************/
static Int32 hil_extract_packet_ingress(struct sk_buff* skb, struct net_device* dev)
{
    struct ethhdr*      ptr_ethhdr = NULL;
    struct iphdr*       ptr_iphdr = NULL;
    struct tcphdr*      ptr_tcphdr = NULL;
    Int8*               ptr_data = NULL;
    Bool                foundGreDs = False;

    /* IPv6 params */
    struct ipv6hdr*     ptr_ipv6hdr = NULL;
    Uint8               ipv6HeaderLen;
    Uint8               nexthdr;

    
    AVALANCHE_PP_INGRESS_SESSION_PROPERTY_t* ingress_property = &skb->pp_packet_info.pp_session.ingress;

    memset( &ingress_property->lookup.LUT1, 0, (sizeof( ingress_property->lookup.LUT1 ) + sizeof( ingress_property->lookup.LUT2 )) );

    ingress_property->lookup.LUT1.u.fields.L2.entry_type = AVALANCHE_PP_LUT_ENTRY_L2_UNDEFINED;
    ingress_property->lookup.LUT1.u.fields.L3.entry_type = AVALANCHE_PP_LUT_ENTRY_L3_UNDEFINED;
    
    /* Check if the device is a PID or not? */
    if (dev->pid_handle != -1)
    {
        /* Valid PID Handle: Packet has been received at the lowest level; at this point in time we can extract all the L2/L3/L4 information from the packet */
        ptr_data = (Int8 *)skb_mac_header(skb);
        if( !ptr_data )
        {
            return -1;  /* Without this data we cannot continue */
        }

        /* Check if the device is a VPID handle or not? */
        if (dev->vpid_handle != -1)
        {
            /* The only missing information not available at the PID layer is the VPID handle on which the packet was actually received/transmitted */
            ingress_property->vpid_handle = dev->vpid_handle;
        }

        /* Decide on direction type */
        if (ingress_property->dev_type == AVALANCHE_PP_DEV_TYPE_UNKNOWN)
        {
            AVALANCHE_PP_PID_t *pid;
            if (avalanche_pp_pid_get_info(dev->pid_handle, &pid) != PP_RC_SUCCESS)
            {
                return -1;
            }
            if (AVALANCHE_PP_PID_TYPE_DOCSIS == pid->type)
            {
                ingress_property->dev_type = AVALANCHE_PP_DEV_TYPE_WAN;
            }
            else
            {
                ingress_property->dev_type = AVALANCHE_PP_DEV_TYPE_LAN;
            }

            ingress_property->lookup.LUT1.u.fields.L2.pid_handle = pid->pid_handle;
        }

        if (hil_get_l2_l3_pointers(ptr_data, &ptr_ethhdr, &ptr_iphdr, &ptr_ipv6hdr) != 0)
        {
            return -1;
        }

        /* Check for DS GRE - in such case we need to extract the encapsulated packet fields */
        if (ingress_property->dev_type == AVALANCHE_PP_DEV_TYPE_WAN)
        {
            if (ptr_ipv6hdr != NULL)
            {
                if (hil_scan_ipv6(ptr_ipv6hdr, &ipv6HeaderLen, &nexthdr) != 0)
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
                    return -1;  /* unsupported GRE */
                }

                /* Supported DS GRE */
                ingress_property->lookup.LUT1.u.fields.L3.entry_type = AVALANCHE_PP_LUT_ENTRY_L3_GRE;
                ingress_property->lookup.LUT1.u.fields.L3.enable_flags |= AVALANCHE_PP_LUT1_FIELD_ENABLE_L3_ENTRY_TYPE;
                
                ptr_data += 4; /* Skip GRE header */
                if (hil_get_l2_l3_pointers(ptr_data, &ptr_ethhdr, &ptr_iphdr, &ptr_ipv6hdr) != 0)
                {
                    return -1;
                }
            }
        }

        /* At this stage all the headers have been located and are pointing at the correct locations. Time to start populating the Packet Properties */

        /* Extract Layer2 information */
        if (hil_extract_l2_ingress(ptr_ethhdr, ingress_property, skb->vpid_vlan_tci ) != 0)
        {
            return -1;
        }

        /* Extract Layer3 information */
        if (ptr_iphdr != NULL)
        {
            if (hil_extract_ipv4_ingress(ptr_iphdr, &ptr_tcphdr, ingress_property) != 0)
            {
                return -1;
            }
        }
        else if (ptr_ipv6hdr != NULL)
        {
            if (hil_extract_ipv6_ingress(ptr_ipv6hdr, &ptr_tcphdr, ingress_property) != 0)
            {
                return -1;
            }
        }

        /* Extract Layer4 information */
        if ((ptr_tcphdr != NULL) && !foundGreDs)
        {
            if (hil_extract_l4_ingress(ptr_tcphdr, ingress_property) != 0)
            {
                return -1;
            }
        }

        ingress_property->lookup.LUT2.u.fields.entry_type = ingress_property->lookup.LUT1.u.fields.L3.entry_type;
    }

    return 0;
}

/**************************************************************************
 * FUNCTION NAME : hil_extract_l2_ingress
 **************************************************************************
 * DESCRIPTION   :
 *  Extract L2 fields
 *
 * RETURNS       :
 *  0 for success
 **************************************************************************/
static Int32 hil_extract_l2_ingress(struct ethhdr* ptr_ethhdr, AVALANCHE_PP_INGRESS_SESSION_PROPERTY_t* ingress_property, Uint16 vpid_vlan_tci)
{
    /* Check did we have an Ethernet header. */
    if (ptr_ethhdr != NULL)
    {
        AVALANCHE_PP_VPID_INFO_t *ptr_vpid;

        /* Ethernet header was detected. Initialize the various fields */
        ingress_property->lookup.LUT1.u.fields.L2.entry_type = AVALANCHE_PP_LUT_ENTRY_L2_ETHERNET;
        ingress_property->lookup.LUT1.u.fields.L2.enable_flags |= AVALANCHE_PP_LUT1_FIELD_ENABLE_L2_ENTRY_TYPE;


        /* Set VPID's VLAN (if exists) */
        avalanche_pp_vpid_get_info(ingress_property->vpid_handle, &ptr_vpid);

        if ((ptr_vpid->type == AVALANCHE_PP_VPID_VLAN) || (ptr_vpid->type == AVALANCHE_PP_VPID_VLAN_PPPoE))
        {
            if ( (vpid_vlan_tci & VLAN_VID_MASK) != (ptr_vpid->vlan_identifier) )
            {
                printk("hil_extract_l2_ingress: Cannot create session since there is no-match in VPID VLAN identifier \n");
                return -1;
            }
            ingress_property->lookup.LUT2.u.fields.firstVLAN = vpid_vlan_tci; 
            ingress_property->lookup.LUT2.u.fields.enable_flags |= AVALANCHE_PP_LUT2_FIELD_ENABLE_1ST_VLAN;
        }

        /* Set MAC SRC/DST */
        memcpy(ingress_property->lookup.LUT1.u.fields.L2.dstmac, (void *)&ptr_ethhdr->h_dest, 6);
        memcpy(ingress_property->lookup.LUT1.u.fields.L2.srcmac, (void *)&ptr_ethhdr->h_source, 6);
        ingress_property->lookup.LUT1.u.fields.L2.enable_flags |= AVALANCHE_PP_LUT1_FIELD_ENABLE_MAC_DST | AVALANCHE_PP_LUT1_FIELD_ENABLE_MAC_SRC;


        /* Set packet's VLAN (if exists) */
        if (ptr_ethhdr->h_proto == __constant_htons(ETH_P_8021Q))
        {
            struct vlan_hdr* ptr_vlanheader = (struct vlan_hdr *)((Int8*)ptr_ethhdr + sizeof(struct ethhdr));      /* Get the VLAN header */

            if (ingress_property->lookup.LUT2.u.fields.enable_flags & AVALANCHE_PP_LUT2_FIELD_ENABLE_1ST_VLAN)
            {
                ingress_property->lookup.LUT2.u.fields.secondVLAN = ptr_vlanheader->h_vlan_TCI;
                ingress_property->lookup.LUT2.u.fields.enable_flags |= AVALANCHE_PP_LUT2_FIELD_ENABLE_2ND_VLAN;
            }
            else
            {
                ingress_property->lookup.LUT2.u.fields.firstVLAN = ptr_vlanheader->h_vlan_TCI;
                ingress_property->lookup.LUT2.u.fields.enable_flags |= AVALANCHE_PP_LUT2_FIELD_ENABLE_1ST_VLAN;
            }

            ingress_property->lookup.LUT1.u.fields.L2.eth_type = ptr_vlanheader->h_vlan_encapsulated_proto;
        }
        else
        {
            ingress_property->lookup.LUT1.u.fields.L2.eth_type = ptr_ethhdr->h_proto;
        }

        ingress_property->lookup.LUT1.u.fields.L2.enable_flags |= AVALANCHE_PP_LUT1_FIELD_ENABLE_ETH_TYPE;
    }

    return 0;
}

/**************************************************************************
 * FUNCTION NAME : hil_extract_ipv4_ingress
 **************************************************************************
 * DESCRIPTION   :
 *  Extract IPv4 fields
 *
 * RETURNS       :
 *  0 for success
 **************************************************************************/
static Int32 hil_extract_ipv4_ingress(struct iphdr* ptr_iphdr, struct tcphdr** ptr_tcphdr, AVALANCHE_PP_INGRESS_SESSION_PROPERTY_t* ingress_property)
{
    /* Currently we support only TCP & UDP. For both these protocols the PORT information lies at the same location. */
    if ((ptr_iphdr->protocol == IPPROTO_TCP) || (ptr_iphdr->protocol == IPPROTO_UDP))
    {
        /* L4 will be valid only within non fragmented packet or first fragment (both have offset = 0) */
        if ((ptr_iphdr->frag_off & IP_OFFSET) == 0)
        {
            *ptr_tcphdr = (struct tcphdr *)((Int8 *)ptr_iphdr + ptr_iphdr->ihl * 4);
        }
        else
        {
            return -1;
        }
    }
    else
    {
        return -1;

    }
    /* IPv4 header was detected. Initialize the various fields */
    if (ingress_property->lookup.LUT1.u.fields.L3.entry_type == AVALANCHE_PP_LUT_ENTRY_L3_UNDEFINED)
    {
        ingress_property->lookup.LUT1.u.fields.L3.entry_type = AVALANCHE_PP_LUT_ENTRY_L3_IPV4; 
        ingress_property->lookup.LUT1.u.fields.L3.enable_flags |= AVALANCHE_PP_LUT1_FIELD_ENABLE_L3_ENTRY_TYPE;
    }
    
    if (ingress_property->dev_type == AVALANCHE_PP_DEV_TYPE_WAN)
    {
        ingress_property->lookup.LUT1.u.fields.L3.LAN_addr_IP.v4 = ptr_iphdr->daddr;
        ingress_property->lookup.LUT2.u.fields.WAN_addr_IP.v4 = ptr_iphdr->saddr;
    }
    else
    {
        ingress_property->lookup.LUT1.u.fields.L3.LAN_addr_IP.v4 = ptr_iphdr->saddr;
        ingress_property->lookup.LUT2.u.fields.WAN_addr_IP.v4 = ptr_iphdr->daddr;
    }
    
    ingress_property->lookup.LUT2.u.fields.TOS = ptr_iphdr->tos;
    ingress_property->lookup.LUT1.u.fields.L3.ip_protocol = ptr_iphdr->protocol;

    ingress_property->lookup.LUT1.u.fields.L3.enable_flags |= AVALANCHE_PP_LUT1_FIELD_ENABLE_LAN_IPv4 | AVALANCHE_PP_LUT1_FIELD_ENABLE_IP_PROTOCOL;
    ingress_property->lookup.LUT2.u.fields.enable_flags |= AVALANCHE_PP_LUT2_FIELD_ENABLE_WAN_IP | AVALANCHE_PP_LUT2_FIELD_ENABLE_IP_TOS;
    
    return 0;
}

/**************************************************************************
 * FUNCTION NAME : hil_extract_ipv6_ingress
 **************************************************************************
 * DESCRIPTION   :
 *  extracts HIL relevant fields from the IPv6 header
 *
 * RETURNS       :
 *  0 for success
 **************************************************************************/
static Int32 hil_extract_ipv6_ingress(struct ipv6hdr* ptr_ipv6hdr, struct tcphdr** ptr_tcphdr, AVALANCHE_PP_INGRESS_SESSION_PROPERTY_t* ingress_property)
{
    Uint8 nexthdr;
    Uint8 ipv6HeaderLen;

    if (hil_scan_ipv6(ptr_ipv6hdr, &ipv6HeaderLen, &nexthdr) != 0)
    {
        return -1;
    }

    if (nexthdr == IPPROTO_IPIP)
    {
        /* DSLite DS */
        struct iphdr* ptr_dsLiteIphdr = (struct iphdr *)((Uint8 *)ptr_ipv6hdr + ipv6HeaderLen);

        if ((ptr_dsLiteIphdr->protocol == IPPROTO_TCP) || (ptr_dsLiteIphdr->protocol == IPPROTO_UDP))
        {
            /* We take only non fragmented packets or first fragment since only these packets holds the layer 4 */
            if ((ptr_dsLiteIphdr->frag_off & IP_OFFSET) == 0)
            {
                *ptr_tcphdr = (struct tcphdr *)((Uint8 *)(ptr_dsLiteIphdr) + ptr_dsLiteIphdr->ihl * 4);
                ptr_ipv6hdr->nexthdr = IPPROTO_IPIP; // Since ptr_ipv6hdr->nexthdr could be Frag, and we want it to pass hil_session_intelligence

                ingress_property->lookup.LUT1.u.fields.L3.entry_type = AVALANCHE_PP_LUT_ENTRY_L3_DSLITE;
                ingress_property->lookup.LUT1.u.fields.L3.enable_flags |= AVALANCHE_PP_LUT1_FIELD_ENABLE_L3_ENTRY_TYPE;
                ingress_property->lookup.LUT2.u.fields.IP.v4_dsLite = ptr_dsLiteIphdr->daddr;           /* Save dsLite_dst_ip to be able to classify according to it */
                ingress_property->lookup.LUT2.u.fields.enable_flags |= AVALANCHE_PP_LUT2_FIELD_ENABLE_DSLITE_IPV4;
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
        if ((nexthdr == IPPROTO_TCP) || (nexthdr == IPPROTO_UDP))
        {
            *ptr_tcphdr = (struct tcphdr *)((Uint8 *)ptr_ipv6hdr + ipv6HeaderLen);
            if (ingress_property->lookup.LUT1.u.fields.L3.entry_type == AVALANCHE_PP_LUT_ENTRY_L3_UNDEFINED)
            {
                ingress_property->lookup.LUT1.u.fields.L3.entry_type = AVALANCHE_PP_LUT_ENTRY_L3_IPV6;
                ingress_property->lookup.LUT1.u.fields.L3.enable_flags |= AVALANCHE_PP_LUT1_FIELD_ENABLE_L3_ENTRY_TYPE;
            }

            memcpy(ingress_property->lookup.LUT2.u.fields.IP.v6_FlowLabel, ptr_ipv6hdr->flow_lbl, sizeof(ptr_ipv6hdr->flow_lbl));
            ingress_property->lookup.LUT2.u.fields.IP.v6_FlowLabel[0] &= 0xF;   // 4 MSbits belongs to Traffic Class field
            ingress_property->lookup.LUT2.u.fields.IP.v6_FlowLabel[3] = 0;      // Make sure last byte is cleared
            ingress_property->lookup.LUT2.u.fields.enable_flags |= AVALANCHE_PP_LUT2_FIELD_ENABLE_IPV6_FLOW;
        }
        else
        {
            return -1;
        }
    }

    if (ingress_property->dev_type == AVALANCHE_PP_DEV_TYPE_WAN)
    {
        memcpy(ingress_property->lookup.LUT1.u.fields.L3.LAN_addr_IP.v6, ptr_ipv6hdr->daddr.s6_addr32, sizeof(ptr_ipv6hdr->daddr.s6_addr32));
        memcpy(ingress_property->lookup.LUT2.u.fields.WAN_addr_IP.v6, ptr_ipv6hdr->saddr.s6_addr32, sizeof(ptr_ipv6hdr->saddr.s6_addr32));
    }
    else
    {
        memcpy(ingress_property->lookup.LUT1.u.fields.L3.LAN_addr_IP.v6, ptr_ipv6hdr->saddr.s6_addr32, sizeof(ptr_ipv6hdr->saddr.s6_addr32));
        memcpy(ingress_property->lookup.LUT2.u.fields.WAN_addr_IP.v6, ptr_ipv6hdr->daddr.s6_addr32, sizeof(ptr_ipv6hdr->daddr.s6_addr32));
    }

    ingress_property->lookup.LUT2.u.fields.TOS = IPV6_TRAFFIC_CLASS(ptr_ipv6hdr);
    ingress_property->lookup.LUT1.u.fields.L3.ip_protocol = ptr_ipv6hdr->nexthdr;

    ingress_property->lookup.LUT1.u.fields.L3.enable_flags |= AVALANCHE_PP_LUT1_FIELD_ENABLE_LAN_IPv6 | AVALANCHE_PP_LUT1_FIELD_ENABLE_IP_PROTOCOL;
    ingress_property->lookup.LUT2.u.fields.enable_flags |= AVALANCHE_PP_LUT2_FIELD_ENABLE_WAN_IP | AVALANCHE_PP_LUT2_FIELD_ENABLE_IP_TOS;
    
    return 0;
}

/**************************************************************************
 * FUNCTION NAME : hil_extract_l4_ingress
 **************************************************************************
 * DESCRIPTION   :
 *  extracts HIL relevant fields from the L4 header
 *
 * RETURNS       :
 *  0 for success
 **************************************************************************/
static Int32 hil_extract_l4_ingress(struct tcphdr* ptr_tcphdr, AVALANCHE_PP_INGRESS_SESSION_PROPERTY_t* ingress_property)
{
    if (ptr_tcphdr != NULL)
    {
        ingress_property->lookup.LUT2.u.fields.L4_SrcPort = ptr_tcphdr->source;
        ingress_property->lookup.LUT2.u.fields.L4_DstPort = ptr_tcphdr->dest;
        ingress_property->lookup.LUT2.u.fields.enable_flags |= AVALANCHE_PP_LUT2_FIELD_ENABLE_SRC_PORT | AVALANCHE_PP_LUT2_FIELD_ENABLE_DST_PORT;
    }

    return 0;
}


/**************************************************************************
 * FUNCTION NAME : hil_egress_hook
 **************************************************************************
 * DESCRIPTION   :
 *  The function is the egress hook that is called when a packet is to be transmitted on a session interfaces.
 *  The function extracts the information pertinent to creation of the session interface. It then checks if this packet was "routed/bridged".
 *  If yes then control is passed to the Plugin Logic to determine creation of the session.
 *
 * RETURNS:
 *  Always returns 0.
 **************************************************************************/
Int32 hil_egress_hook(struct sk_buff* skb)
{
    AVALANCHE_PP_RET_e rc;
    AVALANCHE_PP_SESSION_INFO_t* session_info = &skb->pp_packet_info.pp_session; /* Get the pointer to the session information block */

    /*mac header points to the wrong address in case of dslite tunneling. 
      the reset operation moves the pointer to the correct location.
      in case of non tunneling it will not change its position*/
    skb_reset_mac_header(skb);

    session_info->session_handle = AVALANCHE_PP_MAX_ACCELERATED_SESSIONS;

    // TBD TI_PPM_EGRESS_QUEUE_INVALID
    if (TI_PPM_EGRESS_QUEUE_INVALID == skb->pp_packet_info.egress_queue)
    {
        /* Scale down the priority of the egress queue */
        if ((skb->dev->vpid_block.qos_clusters_count) && (!global_hil_db.qos_disabled))
        {
            if (NULL != skb->dev->qos_select_hook)
            {
                skb->pp_packet_info.egress_queue = skb->dev->qos_select_hook(skb);
            }
        }
        else 
        {
            session_info->cluster  = 0xFF;
            session_info->priority = 0;
        }
        /************************************************************************/
    }

    if (global_hil_db.hil_disabled || (!avalanche_pp_state_is_active()))
    {
        return 0;
    }

    if ((skb->pp_packet_info.flags & TI_HIL_PACKET_FLAG_PP_SESSION_INGRESS_RECORDED) == 0)
    {
        /* If the Packet has not HIT the ingress hook there is no point in creating the session since the packet is locally generated */
        global_hil_db.num_other_pkts++;
        return 0;
    }

    if  (skb->pp_packet_info.flags & TI_HIL_PACKET_FLAG_PP_SESSION_BYPASS)
    {
        /* The Host Intelligence layers have decided not to "acclerate" this session */
        global_hil_db.num_bypassed_pkts++;
        return 0;
    }

    global_hil_db.num_egress_pkts++;

    /* We can only proceed to the next step if all the information has been extracted */
    if (skb->dev->pid_handle == -1)
    {
        return 0;
    }

    /* Extract all the fields from the packet and populate the Egress Packet Descriptor */
    if (hil_extract_packet_egress(skb, skb->dev, session_info) < 0)
    {
        return 0;
    }

    if (session_info->egress.enable & AVALANCHE_PP_EGRESS_FIELD_ENABLE_TCP_SYN)
    {
        /* In case it's a SYN packet - do not open the session yet, but make it expedited in DOCSIS Upstream */
        skb->ti_meta_info |= DOCSIS_FW_PACKET_API_TCP_HIGH_PRIORITY;
        return 0;
    }

    /* Check if the session is ROUTABLE or not by comparing the L2 destination MAC Address at the Ingress and Egress.
     * If they are not the same it is safe to assume that the session was routed */
    session_info->is_routable_session = 0; /* Default to bridged */
    if ((session_info->ingress.lookup.LUT1.u.fields.L2.entry_type == AVALANCHE_PP_LUT_ENTRY_L2_ETHERNET) && (session_info->egress.l2_packet_type == AVALANCHE_PP_LUT_ENTRY_L2_ETHERNET))
    {
        if (memcmp((void *)&session_info->ingress.lookup.LUT1.u.fields.L2.dstmac, (void *)&session_info->egress.dstmac, 6) != 0)
        {
            session_info->is_routable_session = 1; /* The destination MAC address are not the same; most definately a routed session */
        }
    }

    /* Once all the fields have been extracted. Check if the session can be created or not? */
    if (hil_session_intelligence(session_info) == 0)
    {
        return 0;
    }

    /* All sessions created here have a standard session timeout */
    session_info->session_timeout = pp_session_timeout_sec * 1000000;

    if (session_info->egress.enable & AVALANCHE_PP_EGRESS_FIELD_ENABLE_TDOX_ENABLED)
    {
        skb->ti_meta_info |= DOCSIS_FW_PACKET_API_TCP_HIGH_PRIORITY;
    }

    if (hil_choose_session_pool(session_info) < 0)
    {
        return 0;
    }

    rc = avalanche_pp_session_create(session_info, (void*)skb);

    if (rc == PP_RC_SUCCESS)
    {
        if (global_hil_db.tdox_dbg)
        {
            printk("TDOX-DBG CreateSession: ses=%d, vpid=%d, protocol=%d, src=%d, dst=%d\n", session_info->session_handle, session_info->egress.vpid_handle, session_info->egress.ip_protocol, session_info->egress.L4_SrcPort, session_info->egress.L4_DstPort);

            if (session_info->egress.enable & AVALANCHE_PP_EGRESS_FIELD_ENABLE_TDOX_ENABLED)
            {
                printk("TDOX-DBG AssociateTdox (Start): ses=%d, tdox=%d\n", session_info->session_handle, session_info->egress.tdox_handle);
            }
        }
    }

    return 0;
}

/**************************************************************************
 * FUNCTION NAME : hil_extract_packet_egress
 **************************************************************************
 * DESCRIPTION   :
 *  The function is called to extract the various Layer2, Layer3 and Layer4 fields and populate the packet description structure for egress packet
 * 
 * RETURNS       :
 *  0   -   Success
 *  <0  -   Error
 **************************************************************************/
static Int32 hil_extract_packet_egress(struct sk_buff* skb, struct net_device* dev, AVALANCHE_PP_SESSION_INFO_t* session_info)
{
    struct ethhdr* ptr_ethhdr = NULL;
    struct iphdr* ptr_iphdr = NULL;
    struct iphdr* ptr_dsLiteIphdr = NULL;
    struct tcphdr* ptr_tcphdr = NULL;
    struct ipv6hdr* ptr_ipv6hdr = NULL;
    Int8* ptr_data = (Int8 *)skb_mac_header(skb);
    AVALANCHE_PP_EGRESS_SESSION_PROPERTY_t* egress_property = &session_info->egress;

    /* Set some fields with default values before start working */
    egress_property->enable = 0;
    egress_property->l2_packet_type = AVALANCHE_PP_LUT_ENTRY_L2_UNDEFINED;
    egress_property->l3_packet_type = AVALANCHE_PP_LUT_ENTRY_L3_UNDEFINED;
    egress_property->tunnel_type    = AVALANCHE_PP_LUT_ENTRY_L3_UNDEFINED;

#ifdef CONFIG_TI_META_DATA
    egress_property->psi_word = skb->ti_meta_info;
    egress_property->enable |= AVALANCHE_PP_EGRESS_FIELD_ENABLE_PSI;
#endif
    
    /* Check if the device is a PID or not? */
    if (dev->pid_handle != -1)
    {
        /* Valid PID Handle: Packet has been received at the lowest level; at this point in time we can extract all the L2/L3/L4 information from the packet */
        if( !ptr_data )
        {
            /* Without this data we cannot continue */
            return -1;
        }

        /* Check if the device is a VPID handle or not? */
        if (dev->vpid_handle != -1)
        {
            /* The only missing information not available at the PID layer is the VPID handle on which the packet was actually received/transmitted */
            egress_property->vpid_handle = dev->vpid_handle;
        }

        /* Decide on direction type */
        if (egress_property->dev_type == AVALANCHE_PP_DEV_TYPE_UNKNOWN)
        {
            AVALANCHE_PP_PID_t *pid;
            avalanche_pp_pid_get_info(dev->pid_handle, &pid);
            if (AVALANCHE_PP_PID_TYPE_DOCSIS == pid->type)
            {
                egress_property->dev_type = AVALANCHE_PP_DEV_TYPE_WAN;
            }
            else
            {
                egress_property->dev_type = AVALANCHE_PP_DEV_TYPE_LAN;
            }
        }

        if (hil_get_l2_l3_pointers(ptr_data, &ptr_ethhdr, &ptr_iphdr, &ptr_ipv6hdr) != 0)
        {
            return -1;
        }

        /* At this stage all the headers have been located and are pointing at the correct locations. Time to start populating the Packet Properties */

        /* Extract Layer2 information */
        if (hil_extract_l2_egress(ptr_ethhdr, skb, egress_property) != 0)
        {
            return -1;
        }

        /* Extract Layer3 information */
        if (ptr_iphdr != NULL)
        {
            if (hil_extract_ipv4_egress(ptr_ethhdr, ptr_iphdr, &ptr_tcphdr, session_info) != 0)
            {
                return -1;
            }
        }
        else if (ptr_ipv6hdr != NULL)
        {
            if (hil_extract_ipv6_egress(ptr_ethhdr, ptr_ipv6hdr, &ptr_tcphdr, &ptr_dsLiteIphdr, egress_property) != 0)
            {
                return -1;
            }
        }

        /* Extract Layer4 information */
        if (ptr_tcphdr != NULL)
        {
            if (hil_extract_l4_egress(ptr_iphdr, ptr_ipv6hdr, ptr_tcphdr, ptr_dsLiteIphdr, egress_property) != 0)
            {
                return -1;
            }

            if (hil_tdox_check_and_enable(ptr_tcphdr, session_info) != 0)
            {
                return -1;
            }
        }
    }

    return 0;
}

/**************************************************************************
 * FUNCTION NAME : hil_extract_l2_egress
 **************************************************************************
 * DESCRIPTION   :
 *  Extract L2 fields
 *
 * RETURNS       :
 *  0 for success
 **************************************************************************/
static Int32 hil_extract_l2_egress(struct ethhdr* ptr_ethhdr, struct sk_buff* skb, AVALANCHE_PP_EGRESS_SESSION_PROPERTY_t* egress_property)
{
    /* Check did we have an Ethernet header. */
    if (ptr_ethhdr != NULL)
    {
        /* Ethernet header was detected. Initialize the various fields */
        egress_property->l2_packet_type = AVALANCHE_PP_LUT_ENTRY_L2_ETHERNET;

        /* Set MAC SRC/DST */
        memcpy(egress_property->dstmac, (void *)&ptr_ethhdr->h_dest, 6);
        memcpy(egress_property->srcmac, (void *)&ptr_ethhdr->h_source, 6);
        egress_property->enable |= AVALANCHE_PP_EGRESS_FIELD_ENABLE_L2;

        /* Set packet's VLAN (if exists) */
        if (ptr_ethhdr->h_proto == __constant_htons(ETH_P_8021Q))
        {
            struct vlan_hdr* ptr_vlanheader = (struct vlan_hdr *)((Int8*)ptr_ethhdr + sizeof(struct ethhdr));      /* Get the VLAN header */

            egress_property->vlan = ptr_vlanheader->h_vlan_TCI;
            egress_property->vlan |= VLAN_PRIO_MASK & (((Uint16)(skb->ti_meta_info)) << VLAN_PRIO_SHIFT);
            egress_property->enable |= AVALANCHE_PP_EGRESS_FIELD_ENABLE_VLAN;

            egress_property->eth_type = ptr_vlanheader->h_vlan_encapsulated_proto;
        }
        else
        {
            AVALANCHE_PP_VPID_INFO_t *ptr_vpid;

            avalanche_pp_vpid_get_info(egress_property->vpid_handle, &ptr_vpid);

            if ((ptr_vpid->type == AVALANCHE_PP_VPID_VLAN) || (ptr_vpid->type == AVALANCHE_PP_VPID_VLAN_PPPoE))
            {
                egress_property->vlan |= VLAN_PRIO_MASK & (((Uint16)(skb->ti_meta_info)) << VLAN_PRIO_SHIFT);
            }

            egress_property->eth_type = ptr_ethhdr->h_proto;
        }
    }

    return 0;
}

/**************************************************************************
 * FUNCTION NAME : hil_extract_ipv4_egress
 **************************************************************************
 * DESCRIPTION   :
 *  Extract IPv4 fields
 *
 * RETURNS       :
 *  0 for success
 **************************************************************************/
static Int32 hil_extract_ipv4_egress(struct ethhdr* ptr_ethhdr, struct iphdr* ptr_iphdr, struct tcphdr** ptr_tcphdr, AVALANCHE_PP_SESSION_INFO_t* session_info)
{
    Int8* ptr_data;
    AVALANCHE_PP_EGRESS_SESSION_PROPERTY_t* egress_property = &session_info->egress;
    AVALANCHE_PP_INGRESS_SESSION_PROPERTY_t* ingress_property = &session_info->ingress;

    /* Check fragmented packet - open session only for DS tunneling... */
    if ((ptr_iphdr->frag_off & IP_MF) || (ptr_iphdr->frag_off & IP_OFFSET)) 
    {
        /* Fragmented packet found */
        if (ingress_property->dev_type == AVALANCHE_PP_DEV_TYPE_LAN)
        {
            /* Not a DS packet - not supported */
            return -1;
        }
        if ((ingress_property->lookup.LUT1.u.fields.L3.entry_type != AVALANCHE_PP_LUT_ENTRY_L3_GRE) && (ingress_property->lookup.LUT1.u.fields.L3.entry_type != AVALANCHE_PP_LUT_ENTRY_L3_DSLITE))
        {
            /* Not a tunneling packet - not supported */
            return -1;
        }
    }
	
	
    /* Currently we support only TCP & UDP. For both these protocols the PORT information lies at the same location. */
    if ((ptr_iphdr->protocol == IPPROTO_TCP) || (ptr_iphdr->protocol == IPPROTO_UDP))
    {
        /* L4 will be valid only within non fragmented packet or first fragment (both have offset = 0) */
        if ((ptr_iphdr->frag_off & IP_OFFSET) == 0)
        {
            *ptr_tcphdr = (struct tcphdr *)((Int8 *)ptr_iphdr + ptr_iphdr->ihl*4);
        }
        else
        {
            return -1;
        }
    }
	/* Currently do not support GRE DS in brigde mode */
    if ((ptr_iphdr->protocol == IPPROTO_GRE) && (egress_property->dev_type == AVALANCHE_PP_DEV_TYPE_LAN))
	{
		 return -1;
	}

    if (ptr_iphdr->protocol == IPPROTO_GRE && egress_property->dev_type == AVALANCHE_PP_DEV_TYPE_WAN)
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
        egress_property->tunnel_type = AVALANCHE_PP_LUT_ENTRY_L3_GRE;

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
        ptr_iphdr->check = hil_ipv4_checksum((Uint8*)ptr_iphdr, ptr_iphdr->ihl * 4);

        /* Setup ipv4HdrRaw parameters - Save encapsulating L2, L3, GRE and encapsulated L2 */
        egress_property->wrapHeaderDataLenOffset = (Uint8*)ptr_iphdr - (Uint8*)ptr_ethhdr + offsetof(struct iphdr, tot_len);
        egress_property->wrapHeaderLen = (ptr_iphdr->ihl * 4) + 4 + sizeof(struct ethhdr); /* 4 is for the GRE header */
        if (egress_property->wrapHeaderLen <= AVALNCHE_PP_WRAP_HEADER_MAX_LEN)
        {
            struct ethhdr* encap_ptr_ethhdr;
            Uint16 encap_prortocol_type;

            /* Save encapsulating L2, L3, GRE and encapsulated L2 */
            memcpy(egress_property->wrapHeader, ptr_iphdr, egress_property->wrapHeaderLen);

            /* If the encapsulated packet contains VLAN then add it to the template */
            encap_ptr_ethhdr = (struct ethhdr *)(ptr_data + 4); /* Get the pointer to the encapsulated Ethernet header */
            encap_prortocol_type = encap_ptr_ethhdr->h_proto;   /* Get the protocol type. */

            /* Check the protocol field. If the protocol is VLAN or not?  */
            if (encap_prortocol_type == __constant_htons(ETH_P_8021Q))
            {
                if (egress_property->wrapHeaderLen + sizeof(struct vlan_hdr) <= AVALNCHE_PP_WRAP_HEADER_MAX_LEN)
                {
                    /* Get the VLAN header. */
                    struct vlan_hdr* ptr_vlanheader_gre = (struct vlan_hdr *)((Uint8*)encap_ptr_ethhdr + sizeof(struct ethhdr));

                    /* Add the VLAN header to the template */
                    memcpy(egress_property->wrapHeader + egress_property->wrapHeaderLen, ptr_vlanheader_gre, sizeof(struct vlan_hdr));
                    egress_property->wrapHeaderLen += sizeof(struct vlan_hdr);
                }
                else
                {
                    /* There is not enough space to save the US GRE Encapsulation Header */
                    err = True;
                }
            }

#if 0
            printk("%s[%d]: wrapHeader= %08X.%08X.%08X.%08X.%08X.%08X.%08X.%08X.%08X.%08X.%08X.%08X.%08X\n", __FUNCTION__, __LINE__,
                   *(Uint32*)&egress_property->wrapHeader[0], *(Uint32*)&egress_property->wrapHeader[4], *(Uint32*)&egress_property->wrapHeader[8],
                   *(Uint32*)&egress_property->wrapHeader[12], *(Uint32*)&egress_property->wrapHeader[16], *(Uint32*)&egress_property->wrapHeader[20],
                   *(Uint32*)&egress_property->wrapHeader[24], *(Uint32*)&egress_property->wrapHeader[28], *(Uint32*)&egress_property->wrapHeader[32],
                   *(Uint32*)&egress_property->wrapHeader[36], *(Uint32*)&egress_property->wrapHeader[40], *(Uint32*)&egress_property->wrapHeader[44]);
#endif
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
        else
        {
            egress_property->enable |= AVALANCHE_PP_EGRESS_FIELD_ENABLE_ENCAPSULATION;
        }
    }

    /* IPv4 header was detected. Initialize the various fields. */
    if (egress_property->l3_packet_type == AVALANCHE_PP_LUT_ENTRY_L3_UNDEFINED)
    {
        egress_property->l3_packet_type = AVALANCHE_PP_LUT_ENTRY_L3_IPV4; 
    }

    egress_property->DST_IP.v4 = ptr_iphdr->daddr;
    egress_property->SRC_IP.v4 = ptr_iphdr->saddr;
    egress_property->TOS = ptr_iphdr->tos;
    egress_property->ip_protocol = ptr_iphdr->protocol;
    egress_property->enable |= AVALANCHE_PP_EGRESS_FIELD_ENABLE_IP;

    return 0;
}

/**************************************************************************
 * FUNCTION NAME : hil_extract_ipv6_egress
 **************************************************************************
 * DESCRIPTION   :
 *  extracts HIL relevant fields from the IPv6 header
 *
 * RETURNS       :
 *  0 for success
 **************************************************************************/
static Int32 hil_extract_ipv6_egress(struct ethhdr* ptr_ethhdr, struct ipv6hdr* ptr_ipv6hdr, struct tcphdr** ptr_tcphdr, struct iphdr** ptr_dsLiteIphdr, AVALANCHE_PP_EGRESS_SESSION_PROPERTY_t* egress_property)
{
    Uint8  ipv6HeaderLen;
    Uint8  nexthdr;

    if (hil_scan_ipv6(ptr_ipv6hdr, &ipv6HeaderLen, &nexthdr) != 0)
    {
        return -1;
    }

    if (nexthdr == IPPROTO_IPIP)
    {
        *ptr_dsLiteIphdr = (struct iphdr *)((Uint8 *)ptr_ipv6hdr + ipv6HeaderLen);
        if (((*ptr_dsLiteIphdr)->protocol == IPPROTO_TCP) || ((*ptr_dsLiteIphdr)->protocol == IPPROTO_UDP))
        {
            /* L4 will be valid only within non fragmented packet or first fragment (both have offset = 0) */
            if (((*ptr_dsLiteIphdr)->frag_off & IP_OFFSET) == 0)
            {
        *ptr_tcphdr = (struct tcphdr *)((Uint8 *)(*ptr_dsLiteIphdr) + (*ptr_dsLiteIphdr)->ihl * 4);
            }
            else
            {
                return -1;
            }
        }
        else
        {
            return -1;
        }
        
        if (ipv6HeaderLen > AVALNCHE_PP_WRAP_HEADER_MAX_LEN)
        {
            ipv6HeaderLen = AVALNCHE_PP_WRAP_HEADER_MAX_LEN;
        }

        egress_property->wrapHeaderDataLenOffset = (Uint8*)ptr_ipv6hdr - (Uint8*)ptr_ethhdr + offsetof(struct ipv6hdr, payload_len);
        egress_property->wrapHeaderLen = ipv6HeaderLen;
        memcpy(egress_property->wrapHeader, ptr_ipv6hdr, ipv6HeaderLen);
        ((struct ipv6hdr*)egress_property->wrapHeader)->payload_len = 0; // reset the payload length value as it is irrelevant for the session creation and the DsLite template

        egress_property->enable |= AVALANCHE_PP_EGRESS_FIELD_ENABLE_ENCAPSULATION;
        egress_property->tunnel_type = AVALANCHE_PP_LUT_ENTRY_L3_DSLITE;
    }
    else if (nexthdr == IPPROTO_GRE)
    {
        
        if(egress_property->dev_type == AVALANCHE_PP_DEV_TYPE_WAN)
        {
            
            Bool err = False;

            /* Check if we support this type of US GRE by looking at the GRE protocol type field */
            Uint8* ptr_data = (Uint8*)ptr_ipv6hdr + ipv6HeaderLen;         /* Go to GRE header */

            if (*(Uint32*)ptr_data != ETH_P_TEB)  /* 0x6558 - Transparent Ethernet Bridging */
            {
                /* unsupported GRE */
                return -1;
            }

            /* Supported US GRE */
            egress_property->tunnel_type = AVALANCHE_PP_LUT_ENTRY_L3_GRE;

            /* Setup ipv4HdrRaw parameters - Save encapsulating L2, L3, GRE and encapsulated L2 */
            egress_property->wrapHeaderDataLenOffset = (Uint8*)ptr_ipv6hdr - (Uint8*)ptr_ethhdr + offsetof(struct ipv6hdr, payload_len);
            egress_property->wrapHeaderLen = ipv6HeaderLen + 4 + sizeof(struct ethhdr); /* 4 is for the GRE header */
            if (egress_property->wrapHeaderLen <= AVALNCHE_PP_WRAP_HEADER_MAX_LEN)
            {
                struct ethhdr* encap_ptr_ethhdr;
                Uint16 encap_prortocol_type;

                /* Save encapsulating L2, L3, GRE and encapsulated L2 */
                memcpy(egress_property->wrapHeader, ptr_ipv6hdr, egress_property->wrapHeaderLen);

                /* If the encapsulated packet contains VLAN then add it to the template */
                encap_ptr_ethhdr = (struct ethhdr *)(ptr_data + 4); /* Get the pointer to the encapsulated Ethernet header */
                encap_prortocol_type = encap_ptr_ethhdr->h_proto;   /* Get the protocol type. */

                /* Check the protocol field. If the protocol is VLAN or not?  */
                if (encap_prortocol_type == __constant_htons(ETH_P_8021Q))
                {
                    if (egress_property->wrapHeaderLen + sizeof(struct vlan_hdr) <= AVALNCHE_PP_WRAP_HEADER_MAX_LEN)
                    {
                        /* Get the VLAN header. */
                        struct vlan_hdr* ptr_vlanheader_gre = (struct vlan_hdr *)((Uint8*)encap_ptr_ethhdr + sizeof(struct ethhdr));

                        /* Add the VLAN header to the template */
                        memcpy(egress_property->wrapHeader + egress_property->wrapHeaderLen, ptr_vlanheader_gre, sizeof(struct vlan_hdr));
                        egress_property->wrapHeaderLen += sizeof(struct vlan_hdr);
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
            else
            {
                egress_property->enable |= AVALANCHE_PP_EGRESS_FIELD_ENABLE_ENCAPSULATION;
            }
        }
        else
        {
            return -1;
        }
    }
	else if ((nexthdr == IPPROTO_TCP) || (nexthdr == IPPROTO_UDP))
    {
        *ptr_tcphdr = (struct tcphdr *)((Uint8 *)ptr_ipv6hdr + ipv6HeaderLen);
    }
    else
    {
        return -1;
    }

    if (egress_property->l3_packet_type == AVALANCHE_PP_LUT_ENTRY_L3_UNDEFINED)
    {
        egress_property->l3_packet_type = AVALANCHE_PP_LUT_ENTRY_L3_IPV6;
    }

    memcpy(egress_property->DST_IP.v6, ptr_ipv6hdr->daddr.s6_addr32, sizeof(ptr_ipv6hdr->daddr.s6_addr32));
    memcpy(egress_property->SRC_IP.v6, ptr_ipv6hdr->saddr.s6_addr32, sizeof(ptr_ipv6hdr->saddr.s6_addr32));
    egress_property->TOS = IPV6_TRAFFIC_CLASS(ptr_ipv6hdr);
    egress_property->ip_protocol = ptr_ipv6hdr->nexthdr;
    egress_property->enable |= AVALANCHE_PP_EGRESS_FIELD_ENABLE_IP;

    return 0;
}

/**************************************************************************
 * FUNCTION NAME : hil_extract_l4_egress
 **************************************************************************
 * DESCRIPTION   :
 *  extracts HIL relevant fields from the L4 header
 *
 * RETURNS       :
 *  0 for success
 **************************************************************************/
static Int32 hil_extract_l4_egress(struct iphdr* ptr_iphdr, struct ipv6hdr* ptr_ipv6hdr, struct tcphdr* ptr_tcphdr, struct iphdr* ptr_dsLiteIphdr, AVALANCHE_PP_EGRESS_SESSION_PROPERTY_t* egress_property)
{
    if (ptr_tcphdr != NULL)
    {
        egress_property->L4_SrcPort = ptr_tcphdr->source;
        egress_property->L4_DstPort = ptr_tcphdr->dest;
        egress_property->enable |= AVALANCHE_PP_EGRESS_FIELD_ENABLE_L4;

        /* ARRIS MR25031 propagation */
        // ptr_tcphdr header IS L4 HEADER not manadory that it will be TCP
        if(egress_property->ip_protocol == IPPROTO_TCP)
        {
            if (ptr_tcphdr->syn || ptr_tcphdr->fin || ptr_tcphdr->rst)
            {
                /* ARRIS propagate INTEL MR25220 fix */
                egress_property->enable |= AVALANCHE_PP_EGRESS_FIELD_ENABLE_TCP_SYN |   /* Setting SYN will skip session opening logic and pass the packet as high priority */
                                           AVALANCHE_PP_EGRESS_FIELD_ENABLE_TCP_CTRL;   /* Setting CTRL will cause the network interface to send the packet without a valid session, so packet will not be discarded */
                /* ARRIS propagate end */
            }
        }
    }

    return 0;
}

/**************************************************************************
 * FUNCTION NAME : hil_session_intelligence
 **************************************************************************
 * DESCRIPTION   :
 *  The function is the host intelligence layer which decides if the Session
 *  is worthy of accleration or not?
 *
 * RETURNS:
 *  0   -  Session is NOT accelerated
 *  1   -  Session is accelerated
 **************************************************************************/
static Int32 hil_session_intelligence(AVALANCHE_PP_SESSION_INFO_t* session_info)
{
    /* Acclerate only IPv4 or IPv6 traffic */
    if (session_info->ingress.lookup.LUT1.u.fields.L3.entry_type == AVALANCHE_PP_LUT_ENTRY_L3_UNDEFINED)
    {
        return 0;
    }

    /* Don't accelerate MAC Broadcast Packets */
    if ((session_info->ingress.lookup.LUT1.u.fields.L2.enable_flags & AVALANCHE_PP_LUT1_FIELD_ENABLE_MAC_DST) && (session_info->ingress.lookup.LUT1.u.fields.L2.dstmac[0] == 0xFF))
    {
        return 0;
    }

    /* ARRIS CHANGE, merge from Intel to support 6RD (protocol 41) in PP, 
       ipv6rd_pp_support added to make the feature configurable, because there will be 2 problematic effects,
       1) for sessions with protocol 41 - the session will be created based on L2 & L3 only, 
          without L4, meaning - packets that will arrive to the PP and will match the L2 & L3 
          addresses - will use this session, without any differentiation between different details of L4.
       2) Need to make sure that sessions of UDP or TCP will not have same parameters of L2&L3 
          as the session of the protocol 41, because for the 41 protocol session - L4 will not be 
          examined for match at all. */
    /* Accelerate only TCP, UDP, IPv6 and DS-Lite */
    if ((session_info->ingress.lookup.LUT1.u.fields.L3.ip_protocol != IPPROTO_UDP) &&
        (session_info->ingress.lookup.LUT1.u.fields.L3.ip_protocol != IPPROTO_TCP) &&
        (session_info->ingress.lookup.LUT1.u.fields.L3.ip_protocol != IPPROTO_IPIP) &&
        !(global_hil_db.ipv6rd_pp_support && session_info->ingress.lookup.LUT1.u.fields.L3.ip_protocol == IPPROTO_IPV6) )
    {
        return 0;
    }

    /* Accelerate the session. */
    return 1;
}

/**************************************************************************
 * FUNCTION NAME : hil_choose_session_pool
 **************************************************************************
 * DESCRIPTION:
 *  The function initializes the session_pool parameter according to set of conditions
 *
 * RETURNS:
 *  None
 **************************************************************************/
static Int32 hil_choose_session_pool(AVALANCHE_PP_SESSION_INFO_t* session_info)
{
    AVALANCHE_PP_VPID_INFO_t *vpid;
    AVALANCHE_PP_PID_t *pid;

    session_info->session_pool = AVALANCHE_PP_SESSIONS_POOL_DATA;   /* Default */

    if (avalanche_pp_vpid_get_info(session_info->ingress.vpid_handle, &vpid) != PP_RC_SUCCESS)
    {
        return -1;
    }
    if (avalanche_pp_pid_get_info(vpid->parent_pid_handle, &pid) != PP_RC_SUCCESS)
    {
        return -1;
    }
    if (pid->pid_handle >= PP_C55_PID_BASE && pid->pid_handle < PP_C55_PID_BASE + PP_C55_PID_COUNT)
    {
        session_info->session_pool = AVALANCHE_PP_SESSIONS_POOL_VOICE;
    }
    else
    {
        if (avalanche_pp_vpid_get_info(session_info->egress.vpid_handle, &vpid) != PP_RC_SUCCESS)
        {
            return -1;
        }
        if (avalanche_pp_pid_get_info(vpid->parent_pid_handle, &pid) != PP_RC_SUCCESS)
        {
            return -1;
        }
        if (pid->pid_handle >= PP_C55_PID_BASE && pid->pid_handle < PP_C55_PID_BASE + PP_C55_PID_COUNT)
        {
            session_info->session_pool = AVALANCHE_PP_SESSIONS_POOL_VOICE;
        }
    }
    return 0;
}


/**************************************************************************
 * FUNCTION NAME : hil_null_hook
 **************************************************************************
 * DESCRIPTION:
 *  This function creates session to terminating VPID. All the packets like ingress one will be routed to it and dropped in the future.
 *  The function is very similar to hil_egress_hook.
 * 
 * RETURNS:
 *  Always returns 0.
 **************************************************************************/
Int32 hil_null_hook(struct sk_buff* skb, Int32 null_vpid)
{
    AVALANCHE_PP_RET_e rc;
    AVALANCHE_PP_SESSION_INFO_t* session_info = &skb->pp_packet_info.pp_session;
    struct net_device* input_dev;

    if (global_hil_db.hil_disabled || (!avalanche_pp_state_is_active()) || (null_vpid == -1))
    {
        return 0;
    }

    if ((skb->pp_packet_info.flags & TI_HIL_PACKET_FLAG_PP_SESSION_INGRESS_RECORDED) == 0)
    {
        /* If the Packet has not HIT the ingress hook there is no point in creating the session since the packet is locally generated */
        global_hil_db.num_other_pkts++;
        return 0;
    }

    if  (skb->pp_packet_info.flags & TI_HIL_PACKET_FLAG_PP_SESSION_BYPASS)
    {
        /* The Host Intelligence layers have decided not to "acclerate" this session */
        global_hil_db.num_bypassed_pkts++;
        return 0;
    }

    global_hil_db.num_null_drop_pkts++;

    input_dev = dev_get_by_index(&init_net,skb->skb_iif);
    if(input_dev == NULL)
    {
         return 0;
    }

    /* Extract all the fields from the packet and populate the Egress Packet Descriptor */
    if (hil_extract_packet_egress(skb, input_dev, session_info) < 0)
	 {
        dev_put(input_dev);
        return 0;
     }

    /* We can only proceed to the next step if all the information has been extracted */
    if (input_dev->pid_handle == -1)
    {
        dev_put(input_dev);
        return 0;
    }
    dev_put(input_dev);

    /* Override the VPID */
    session_info->egress.vpid_handle = null_vpid;

    /* Check if the session is ROUTABLE or not by comparing the L2 destination MAC Address at the Ingress and Egress.
     * If they are not the same it is safe to assume that the session was routed */
    session_info->is_routable_session = 0; /* Default to bridged */
    if ((session_info->ingress.lookup.LUT1.u.fields.L2.entry_type == AVALANCHE_PP_LUT_ENTRY_L2_ETHERNET) && (session_info->egress.l2_packet_type == AVALANCHE_PP_LUT_ENTRY_L2_ETHERNET))
    {
        if (memcmp((void *)&session_info->ingress.lookup.LUT1.u.fields.L2.dstmac, (void *)&session_info->egress.dstmac, 6) != 0)
        {
            session_info->is_routable_session = 1; /* The destination MAC address are not the same; most definately a routed session */
        }
    }
    
    /* Once all the fields have been extracted. Check if the session can be created or not? */
    if (hil_session_intelligence(session_info) == 0)
    {
        return 0;
    }

    /* All sessions created here have a standard session timeout. */
    session_info->session_timeout = pp_session_timeout_sec * 1000000;
    session_info->priority = 0;
    session_info->cluster  = 0;

    session_info->session_pool = AVALANCHE_PP_SESSIONS_POOL_DATA;

    rc = avalanche_pp_session_create(session_info, (void*)skb);
    if ((rc != PP_RC_SUCCESS) && (rc != PP_RC_OBJECT_EXIST))
    {
        global_hil_db.num_error++;
        return -1;
    }

    return 0;
}

/**************************************************************************
 * FUNCTION NAME : hil_get_l2_l3_pointers
 **************************************************************************
 * DESCRIPTION   :
 *  extracts HIL relevant fields from the L2 header
 *
 * RETURNS       :
 *  0 for success
 **************************************************************************/
static Int32 hil_get_l2_l3_pointers(Int8* ptr_data, struct ethhdr** ptr_ethhdr, struct iphdr** ptr_iphdr, struct ipv6hdr** ptr_ipv6hdr)
{
    Uint16 protocol_type;

    *ptr_ethhdr = (struct ethhdr *)ptr_data;        /* Get the pointer to the Ethernet header */
    ptr_data = ptr_data + sizeof(struct ethhdr);    /* Skip the Ethernet header */
    protocol_type = (*ptr_ethhdr)->h_proto;         /* Get the protocol type */

    /* Check the protocol field. If the protocol is VLAN or not? */
    if (protocol_type == __constant_htons(ETH_P_8021Q))
    {
        protocol_type = ((struct vlan_hdr *)ptr_data)->h_vlan_encapsulated_proto;   /* The new protocol is encapsulated into the VLAN header */
        ptr_data += sizeof(struct vlan_hdr);                                        /* Skip the 4 bytes of the VLAN header */
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
            protocol_type = *((Uint16 *)ptr_data);
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
 * FUNCTION NAME : hil_ipv4_checksum
 **************************************************************************
 * DESCRIPTION   :
 *  Utility function to calculate IPv4 checksum field
 *
 * RETURNS       :
 *  ... the checksum
 **************************************************************************/
static Uint16 hil_ipv4_checksum(Uint8 *ipv4Header, Uint16 ipv4HeaderLen)
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
 * FUNCTION NAME : hil_scan_ipv6
 **************************************************************************
 * DESCRIPTION   :
 *  Scan IPv6 header till reaching one of our supported protocols
 *
 * RETURNS       :
 *  0 for success
 **************************************************************************/
static Int32 hil_scan_ipv6(struct ipv6hdr* ptr_ipv6hdr, Uint8* ipv6HeaderLen, Uint8* nexthdr)
{
    struct ipv6_opt_hdr* hdr = NULL;
    Uint32 hdrlen;
    Uint32 nextOffset;

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

        hdr = (struct ipv6_opt_hdr*)((Uint8 *)ptr_ipv6hdr + *ipv6HeaderLen);

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

        nextOffset = (Uint32)*ipv6HeaderLen + hdrlen;

        /* preventing a wrap - if the offset exceeds 255 bytes we exit the loop. */
        if (nextOffset > 0xFF)
        {
            return -1;
        }
        else
        {
            *ipv6HeaderLen = (Uint8)nextOffset;
        }

        *nexthdr = hdr->nexthdr;
    }

    return 0;
}

#ifdef CONFIG_IP_MULTICAST
/**************************************************************************
 * FUNCTION NAME : hil_mfc_delete_session_by_mc_group
 **************************************************************************
 * DESCRIPTION   :
 *  The function deletes a session by a multicast group identifier
 **************************************************************************/
static Int32 hil_mfc_delete_session_by_mc_group(Int32 mc_group)
{
    Int32 session_handle ;
    AVALANCHE_PP_SESSION_INFO_t *session;

    /* Cycle through all the entries */
    for (session_handle = AVALANCHE_PP_MAX_ACCELERATED_SESSIONS-1; session_handle > -1; session_handle--)
    {
        /* Get the pointer to the Session Information */
        if (avalanche_pp_session_get_info(session_handle, &session) != PP_RC_SUCCESS)
        {
            continue;
        }

        if (session->ingress.lookup.LUT1.u.fields.L3.LAN_addr_IP.v4 == mc_group)
        {
            avalanche_pp_session_delete(session_handle, NULL);

            if (global_hil_db.tdox_dbg)
            {
                printk("TDOX-DBG DeleteSession: ses=%d\n", session_handle);
            }
            return 0;
        }
    }

    return -1;
}
#endif


/**************************************************************************
 * FUNCTION NAME : hil_tdox_check_and_enable
 **************************************************************************
 * DESCRIPTION   : This function sets the Tdox Enable Flag according to set of conditions
 * RETURNS:
 *  0 = OK, -1 = error.
 **************************************************************************/
static Int32 hil_tdox_check_and_enable(struct tcphdr* ptr_tcphdr, AVALANCHE_PP_SESSION_INFO_t* session_info)
{
    AVALANCHE_PP_INGRESS_SESSION_PROPERTY_t* ingress_property = &session_info->ingress;
    AVALANCHE_PP_EGRESS_SESSION_PROPERTY_t* egress_property = &session_info->egress;

    if (global_hil_db.tdox_disabled)
    {
        return 0;   /* TCP acceleration feature is disabled */
    }

    if (egress_property->dev_type != AVALANCHE_PP_DEV_TYPE_WAN) 
    {
        return 0;   /* TCP acceleration is done only on traffic toward the RF */
    }

    if (ingress_property->lookup.LUT1.u.fields.L3.ip_protocol != IPPROTO_TCP)
    {
        return 0;   /* TCP acceleration is done only on TCP... */
    }

    if (session_info->egress.l3_packet_type == AVALANCHE_PP_LUT_ENTRY_L3_UNDEFINED)
    {
        return 0;   /* TCP acceleration is done only on supported L3 */
    }

    if (ptr_tcphdr->syn)
    {
        egress_property->enable |= AVALANCHE_PP_EGRESS_FIELD_ENABLE_TCP_SYN;
        return -1;  /* In case it's a SYN packet - do not open the session yet */
    }

    /* extract ack number */
    if (ptr_tcphdr->ack)
    {
        Int8 *ptr_data_head   = (Int8 *)ptr_tcphdr + ptr_tcphdr->doff * 4;
        Int8 *ptr_tcp_options = (Int8 *)ptr_tcphdr + sizeof(struct tcphdr);

        if (ptr_data_head > ptr_tcp_options)
        {
            /********************************************************/
            /* Go over the options and look for timestamp option    */
            /********************************************************/
            while (ptr_tcp_options < ptr_data_head)
            {
                Int8 tcp_opt_type;
                Int8 tcp_opt_len;

                tcp_opt_type = *(ptr_tcp_options);

                if (tcp_opt_type > TCP_OPTION_CODE_NOP)
                {
                    /************************************************/
                    /* T.L.V. styled option                         */
                    /************************************************/
                    tcp_opt_len =  *(ptr_tcp_options + 1);

                    if (TCP_OPTION_CODE_TIMESTAMP == tcp_opt_type)
                    {
                        egress_property->enable |= AVALANCHE_PP_EGRESS_FIELD_ENABLE_TDOX_SKIP_TIMESTAMP;
                    }

                    if(tcp_opt_len != 0)
                    {
                        ptr_tcp_options += tcp_opt_len;
                    }
                    else
                    {
                        return -1;  /* an illegal option length - don't accelerate this session. Probably the server will reset the connection */
                    }
                }
                else
                {
                    ptr_tcp_options++;  /* Options EOL and NOP are without parameters */
                }
            }
        }

        egress_property->tdox_tcp_ack_number = ptr_tcphdr->ack_seq;
        egress_property->enable |= AVALANCHE_PP_EGRESS_FIELD_ENABLE_TDOX_ENABLED;
    }
    else
    {
        return -1;  /* TCP with NO ACK flag set - do not accelerate */
    }

    return 0;
}


/**************************************************************************
 * FUNCTION NAME : hil_tdox_manager
 **************************************************************************
 * DESCRIPTION: This function is the TDOX manager - making decisions if a session is applicabale for TDOX or not
 * RETURNS:
 *  AVALANCHE_PP_RET_e
 **************************************************************************/
AVALANCHE_PP_RET_e hil_tdox_manager(AVALANCHE_PP_SESSION_INFO_t *session_info, Ptr data)
{
    AVALANCHE_PP_SESSION_STATS_t sessionStats;
    AVALANCHE_PP_RET_e rc;
    Uint32  currentTime;
    Uint32  deltaTimeMsec;
    Uint32  pktDelta;
    Uint32  bytesDelta;
    Uint32  averagePktSize;
    Uint32  packetsPerSecond;
    Bool    tdoxEnabled;

    AVALANCHE_PP_SESSION_TDOX_STATS_t * tdox_stats;
    
    if (session_info->egress.enable & AVALANCHE_PP_EGRESS_FIELD_ENABLE_TDOX_ENABLED)
    {
        if ( avalanche_pp_session_tdox_capability_get( session_info->session_handle, &tdoxEnabled ) )
        {
            return (PP_RC_FAILURE);
        }
        
        if (False == tdoxEnabled)
        {
            /* This is TDOX session that has been deprecated by the PP FW. The associated TDOX ID need to be freed */
            if ( avalanche_pp_session_tdox_capability_set( session_info->session_handle, tdoxEnabled ) )
            {
               return (PP_RC_FAILURE);
            }
            return PP_RC_SUCCESS;
        }
    }

    if ((rc = (avalanche_pp_get_stats_session(session_info->session_handle, &sessionStats))) != PP_RC_SUCCESS)
    {
        return rc;
    }

    tdox_stats = &session_info->tdox_stats;

    currentTime = jiffies;
    deltaTimeMsec = ((currentTime - tdox_stats->last_update_time) * 1000) / HZ;  /* Scale down the delta to milliseconds */

    if ((tdox_stats->last_update_time != 0) && (deltaTimeMsec < global_hil_db.tdoxEvalMinTimeMsec))
    {
        /* Resolution for checking the traffic is at least tdoxEvalMinTime */
        return PP_RC_SUCCESS;
    }

    pktDelta = sessionStats.packets_forwarded - tdox_stats->packets_forwarded;
    bytesDelta = sessionStats.bytes_forwarded - tdox_stats->bytes_forwarded;
    tdox_stats->packets_forwarded = sessionStats.packets_forwarded;
    tdox_stats->bytes_forwarded = sessionStats.bytes_forwarded;

    if (tdox_stats->last_update_time == 0)
    {
        /* If this is the first time, just update current time and wait to next iteration */
        if (global_hil_db.tdox_dbg)
        {
            printk("TDOX-DBG Update: ses=%d, TDOX=%s, deltaTimeMsec=%d, pktDelta=%d, bytesDelta=%d\n", 
                   session_info->session_handle, ((session_info->egress.enable & AVALANCHE_PP_EGRESS_FIELD_ENABLE_TDOX_ENABLED) ? "EN" : "DIS"), 
                   0, pktDelta, bytesDelta);
        }
        
        tdox_stats->last_update_time = currentTime;
        return PP_RC_SUCCESS;
    }

    tdox_stats->last_update_time = currentTime;

    if (sessionStats.packets_forwarded < 100)
    {
        return (PP_RC_SUCCESS);   /* Do not do any logic on the session if it is only established */
    }

    averagePktSize = pktDelta ? (bytesDelta / pktDelta) : 0;
    if (pktDelta < 0xFFFFFFFF / 1000)
    {
        packetsPerSecond = (pktDelta * 1000) / deltaTimeMsec;
    }
    else
    {
        packetsPerSecond = (pktDelta / deltaTimeMsec) * 1000;
    }

    if (global_hil_db.tdox_dbg)
    {
        printk("TDOX-DBG Update: ses=%d, TDOX=%s, tdox=%d, deltaTimeMsec=%d, pktDelta=%d, bytesDelta=%d, PPS=%d, AvgPktSize=%d\n", 
               session_info->session_handle, ((session_info->egress.enable & AVALANCHE_PP_EGRESS_FIELD_ENABLE_TDOX_ENABLED) ? "EN" : "DIS"), 
               session_info->egress.tdox_handle, deltaTimeMsec, pktDelta, bytesDelta, packetsPerSecond, averagePktSize);
    }

    if (session_info->egress.enable & AVALANCHE_PP_EGRESS_FIELD_ENABLE_TDOX_ENABLED)
    {
        /* This is a TDOX session - check for conditions to disable TDOX on this session */
        if ((packetsPerSecond < global_hil_db.tdoxEvalPpsThresh) || (averagePktSize > global_hil_db.tdoxEvalAvgPktSizeThresh))
        {
            if (global_hil_db.tdox_dbg)
            {
                printk("TDOX-DBG FreeTdox (Evaluation): ses=%d, tdox=%d, PPS=%d, PktSize=%d\n", session_info->session_handle, session_info->egress.tdox_handle, packetsPerSecond, averagePktSize);
            }

            if ( avalanche_pp_session_tdox_capability_set( session_info->session_handle, False ) )
            {
                return (PP_RC_FAILURE);
            }
        }
    }
    else
    {
        /* This is not a TDOX session - check for conditions to enable TDOX on this session */
        if ((packetsPerSecond >= global_hil_db.tdoxEvalPpsThresh) && (averagePktSize <= global_hil_db.tdoxEvalAvgPktSizeThresh))
        {
            if ( avalanche_pp_session_tdox_capability_set( session_info->session_handle, True ) )
            {
                return (PP_RC_FAILURE);
            }
            if (global_hil_db.tdox_dbg)
            {
                printk("TDOX-DBG AssociateTdox (Threshold): ses=%d, tdox=%d\n", session_info->session_handle, session_info->egress.tdox_handle);
            }
        }
    }

    return PP_RC_SUCCESS;
}


/**************************************************************************
 * FUNCTION NAME : hil_write_cmds
 **************************************************************************
 * DESCRIPTION   :
 *  Interface for the Intrusive HIL. This is used to debug and display various
 *  packet processor entity information from the console.
 *
 * RETURNS       :
 *  -1              - Error.
 *  Non-Zero        - Success.
 ***************************************************************************/
static int hil_write_cmds(struct file *file, const char *buffer, unsigned long count, void *data)
{
    Int8*   pp_cmd;
    Int8*   argv[10];
    Int32   argc = 0;
    Int8*   ptr_cmd;
    Int8*   delimitters = " \n\t";
    char*   ptr_next_tok;

    Uint8   **testPktP = NULL;
    Uint16  *testPktSizeP = NULL;

    pp_cmd = kmalloc(count+1, GFP_KERNEL);
    if (pp_cmd == NULL)
    {
        printk("Could not allocate %lu bytes for pp_cmd\n", count);
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
            printk ("ERROR: Incorrect too many parameters dropping the command\n");
            kfree(pp_cmd);
            return -EFAULT;
        }
    }


    /******************************* Command Handlers *******************************/
    /* Display Command Handlers */
    if (strncmp(argv[0], "show", strlen("show")) == 0)
    {
        /* Call the Show Command Handler. */
        if (hil_show_cmd_handler(argc, argv) < 0)
        {
            kfree(pp_cmd);
            return -EFAULT;
        }
    }

    /* Set Command Handlers */
    else if (strncmp(argv[0], "set", strlen("set")) == 0)
    {
        /* Call the Set Command Handler. */
        if (hil_set_cmd_handler (argc, argv) < 0)
        {
            kfree(pp_cmd);
            return -EFAULT;
        }
    }

    /* Deinitialize Command Handlers */
    else if (strncmp(argv[0], "reset", strlen("reset")) == 0)
    {
        /* Call the Reset Command Handler. */
        if (hil_reset_cmd_handler (argc, argv) < 0)
        {
            kfree(pp_cmd);
            return -EFAULT;
        }
    }

    /* cable_pp: disable/enable capability */
    else if (strcmp(argv[0], "enable") == 0)
    {
        global_hil_db.hil_disabled = False;
    }

    else if (strcmp(argv[0], "disable") == 0)
    {
        global_hil_db.hil_disabled = True;
        avalanche_pp_flush_sessions( AVALANCHE_PP_MAX_VPID, PP_LIST_ID_ALL );
    }

    else if (strcmp(argv[0], "tdox") == 0)
    {
        global_hil_db.tdox_disabled = False;
    }

    else if (strcmp(argv[0], "notdox") == 0)
    {
        global_hil_db.tdox_disabled = True;
    }

    else if (strcmp(argv[0], "ackSupp") == 0)
    {
        Int8 pram = *(argv[1]);
        Int32 enDis = 0;
        if(pram == '1')
        {
            enDis = 1;
        }
        // TBD
        // avalanche_pp_set_ack_suppression(enDis);
    }

    else if ((strcmp(argv[0], "qos") == 0) && (global_hil_db.qos_disabled) )
    {
        Uint32 i;
        Uint32 lockKey = 0;

        PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);

        avalanche_pp_flush_sessions( AVALANCHE_PP_MAX_VPID, PP_LIST_ID_ALL );

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

    else if ((strcmp(argv[0], "noqos") == 0) && (!global_hil_db.qos_disabled) )
    {
        Uint32 i;
        Uint32 lockKey = 0;

        PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);

        avalanche_pp_flush_sessions( AVALANCHE_PP_MAX_VPID, PP_LIST_ID_ALL );

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
    else if (strcmp(argv[0], "flush_all_sessions") == 0)
    {
        avalanche_pp_flush_sessions( AVALANCHE_PP_MAX_VPID, PP_LIST_ID_ALL );
    }

    else if (strcmp(argv[0], "createSession") == 0)
    {
        hil_test_session_creation();
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
 * FUNCTION NAME : hil_show_cmd_handler
 **************************************************************************
 * DESCRIPTION   :
 *  This function is the Packet Processor show command handler.
 *
 * RETURNS       :
 *  -1      - Error.
 *  0       - Success.
 ***************************************************************************/
static Int32 hil_show_cmd_handler(Int32 argc, Int8* argv[])
{
    /* Validate the number of arguments that have been passed */
    if (argc != 2 && argc != 3 && argc != 4)
    {
        printk ("ERROR: Incorrect Number of parameters passed.\n");
        return -1;
    }

    /* Check if Analysis Information needs to be displayed? */
    if (strcmp(argv[1], "stats") == 0)
    {
        AVALANCHE_PP_Misc_Statistics_t stats;
        Uint32 index;

        printk ("HIL  State is                             : %s\n", (global_hil_db.hil_disabled)? "Disabled":"Enabled" );
        printk ("PP   State is                             : %s\n", (avalanche_pp_state_is_active()) ? "Active" : ((avalanche_pp_state_is_psm()) ? "Power Save" : "Inactive"));
        printk ("TDOX State is                             : %s\n", (global_hil_db.tdox_disabled)? "Disabled":"Enabled" );
        printk ("QoS  State is                             : %s\n", (global_hil_db.qos_disabled)? "Disabled":"Enabled" );
        printk ("TDOX Evaluation Time                      : %u ms\n", global_hil_db.tdoxEvalMinTimeMsec);
        printk ("TDOX Evaluation Average PktSize Threshold : %u bytes\n", global_hil_db.tdoxEvalAvgPktSizeThresh);
        printk ("TDOX Evaluation PPS Threshold             : %u PPS\n", global_hil_db.tdoxEvalPpsThresh);
        printk ("Bypassed pkts                             : %u\n", global_hil_db.num_bypassed_pkts );
        printk ("Other    pkts                             : %u\n", global_hil_db.num_other_pkts    );
        printk ("Ingress  pkts                             : %u\n", global_hil_db.num_ingress_pkts  );
        printk ("Egress   pkts                             : %u\n", global_hil_db.num_egress_pkts   );
        printk ("Null     pkts                             : %u\n", global_hil_db.num_null_drop_pkts );
        printk ("Total Sessions                            : %u\n", global_hil_db.num_total_sessions);
        printk ("Total Errors                              : %u\n", global_hil_db.num_error);
        printk ("Global Timeout                            : %u sec\n", pp_session_timeout_sec);
        printk ("Reserved LUT1 Keys for voice sessions     : %u\n", AVALANCHE_PP_MAX_ACCELERATED_VOICE_SESSIONS/2);
        printk ("Reserved voice sessions                   : %u\n", AVALANCHE_PP_MAX_ACCELERATED_VOICE_SESSIONS);
        avalanche_pp_get_db_stats(&stats);
        printk ("Active PIDs                               : %u\n", stats.active_PIDs);
        printk ("Active VPIDs                              : %u\n", stats.active_VPIDs);
        printk ("Active LUT1 keys                          : %u\n", stats.active_lut1_keys);
        printk ("Max active LUT1 keys                      : %u\n", stats.max_active_lut1_keys);
        printk ("Active sessions                           : %u\n", stats.active_sessions);
        printk ("Max active sessions                       : %u\n", stats.max_active_sessions);

        printk ("LUT1 starvation                           : %u\n", stats.lut1_starvation);
        printk ("LUT2 starvation                           : %u\n", stats.lut2_starvation);
        printk ("TDOX starvation                           : %u\n", stats.tdox_starvation);

        printk ("\nLUT1 keys utilization histogram\n-------------------------------\n");
        for (index = 0; index < AVALANCHE_PP_LUT_HISTOGRAM_SIZE; index++)
        {
            printk ("range [%3d to %3d]                        : %u\n", (AVALANCHE_PP_MAX_LUT1_KEYS/AVALANCHE_PP_LUT_HISTOGRAM_SIZE)*index, (AVALANCHE_PP_MAX_LUT1_KEYS/AVALANCHE_PP_LUT_HISTOGRAM_SIZE)*(index+1) -1, stats.lut1_histogram[index]);
        }
        printk ("\nLUT2 sessions utilization histogram\n-----------------------------------\n");
        for (index = 0; index < AVALANCHE_PP_LUT_HISTOGRAM_SIZE; index++)
        {
            printk ("range [%4d to %4d]                      : %u\n", (AVALANCHE_PP_MAX_ACCELERATED_SESSIONS/AVALANCHE_PP_LUT_HISTOGRAM_SIZE)*index, (AVALANCHE_PP_MAX_ACCELERATED_SESSIONS/AVALANCHE_PP_LUT_HISTOGRAM_SIZE)*(index+1) -1, stats.lut2_histogram[index]);
        }
                
        if (avalanche_pp_state_is_active()) 
        {
            Int32 index;
            AVALANCHE_PP_QOS_QUEUE_STATS_t qos_stats;

            printk ("\nQoS queues statistics:\n");
            printk (" queue | forwarded  | discarded  | owner\n");
            printk ("-------+------------+------------+--------------\n");
            for(index = 0; index <= (PAL_CPPI41_SR_QPDSP_QOS_Q_LAST - PAL_CPPI41_SR_QPDSP_QOS_Q_BASE); index++)
            {
                avalanche_pp_qos_get_queue_stats(index, &qos_stats);
                if (qos_stats.fwd_pkts | qos_stats.drp_cnt)
                {
                    printk (" %5u | %10u | %10u | %s\n",index, qos_stats.fwd_pkts, qos_stats.drp_cnt, PAL_CPPI41_GET_QNAME( index + PAL_CPPI41_SR_QPDSP_QOS_Q_BASE ));
                }
            }
            printk ("-------+------------+------------+--------------\n");
        }
        return 0;
    }

#ifdef CONFIG_NETFILTER

    if (strcmp(argv[1], "mapper") == 0)
    {
        unsigned int    lockKey;
        Uint16          i;
        Uint16          num_displayed_sessions = (Uint16)simple_strtol(argv[2], NULL, 0);

        /*return -1 if number of sessions not 1-2048*/
        if ((num_displayed_sessions < 1 ) || (num_displayed_sessions > AVALANCHE_PP_MAX_ACCELERATED_SESSIONS ) )
        {     
            return -1;
        }

        PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);
        printk("\n");
        printk("###############################\n");
        printk("# mapper info #\n");
        printk("###############################\n\n");
                
        if (num_displayed_sessions == 0) 
        {
            num_displayed_sessions = AVALANCHE_PP_MAX_ACCELERATED_SESSIONS;
        }
        for (i = (AVALANCHE_PP_MAX_ACCELERATED_SESSIONS - 1); i >= (AVALANCHE_PP_MAX_ACCELERATED_SESSIONS - num_displayed_sessions); i--) 
        {
            printk("# session [%d] ",i);
            if (IS_TI_PP_SESSION_CT_INVALID(hil_session_ct_mapper[i].previous )) 
            {
                printk("# previous [invalid] ");
            }
            else
            {
                printk("# previous [%d] ",hil_session_ct_mapper[i].previous);
            }
            if (IS_TI_PP_SESSION_CT_INVALID(hil_session_ct_mapper[i].next)) 
            {
                printk("# next [invalid] ");
            }
            else
            {
                printk("# next [%d] ",hil_session_ct_mapper[i].next);
            }
            if (hil_session_ct_mapper[i].conntrack != NULL) 
            {
                if (hil_find_session_handle_in_ct_mapper_list(hil_session_ct_mapper[i].conntrack->tuplehash[IP_CT_DIR_REPLY].ti_pp_session_handle,i) == true)
                {
                    printk("# conntrack_session [%d] \n",hil_session_ct_mapper[i].conntrack->tuplehash[IP_CT_DIR_REPLY].ti_pp_session_handle);
                }
                else if (hil_find_session_handle_in_ct_mapper_list(hil_session_ct_mapper[i].conntrack->tuplehash[IP_CT_DIR_ORIGINAL].ti_pp_session_handle,i) == true)
                {
                    printk("# conntrack_session [%d] \n",hil_session_ct_mapper[i].conntrack->tuplehash[IP_CT_DIR_ORIGINAL].ti_pp_session_handle);
                }
                else 
                {
                    printk("\n");
                }
            }
            else
            {
                printk("conntrack is null\n");
            }
        }
        PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
        return 0;
     }
#endif

    /* Control comes here if the scope was not understood */
    return -1;
}

/**************************************************************************
 * FUNCTION NAME : hil_set_cmd_handler
 **************************************************************************
 * DESCRIPTION   :
 *  This function is the Packet Processor set command handler.
 *
 * RETURNS       :
 *  -1      - Error.
 *  0       - Success.
 ***************************************************************************/
static Int32 hil_set_cmd_handler(Int32 argc, Int8* argv[])
{
    Uint8   **testPktP = NULL;
    Uint16  *testPktSizeP = NULL;

    if (strcmp(argv[1], "timeout") == 0)
    {
        Int32 tmp = (Int32) simple_strtol(argv[2], NULL, 0);

        if (tmp < 0)
        {
            return -1;
        }

        pp_session_timeout_sec = tmp;

        return 0;
    }

    else if (strcmp(argv[1], "tdoxDbg") == 0)
    {
        Int32 enDis = (Int32)simple_strtol(argv[2], NULL, 0);

        if (enDis < 0 || enDis > 1)
        {
            return -1;
        }

        global_hil_db.tdox_dbg = enDis;

        return 0;
    }

    else if (strcmp(argv[1], "tdoxEvalAvgPktSize") == 0)
    {
        Int32 threshold = (Int32) simple_strtol(argv[2], NULL, 0);

        if (threshold < 0)
        {
            return -1;
        }

        global_hil_db.tdoxEvalAvgPktSizeThresh = threshold;

        return 0;
    }

    else if (strcmp(argv[1], "tdoxEvalPPS") == 0)
    {
        Int32 threshold = (Int32) simple_strtol(argv[2], NULL, 0);

        if (threshold < 0)
        {
            return -1;
        }

        global_hil_db.tdoxEvalPpsThresh = threshold;

        return 0;
    }

    else if (strcmp(argv[1], "tdoxEvalTime") == 0)
    {
        Int32 threshold = (Int32) simple_strtol(argv[2], NULL, 0);

        if (threshold < 0)
        {
            return -1;
        }

        global_hil_db.tdoxEvalMinTimeMsec = threshold * 1000;

        return 0;
    }

    else if (strcmp(argv[1], "dropped_packets_bit_map") == 0)
    {
        Int32 cmd;
        Int32 val;
 
        /* Check for bug of last arg */ 
        if (!isalnum(*argv[argc-1]))
        {
            /* Ignore last */
            argc--;
        }

        if (argc == 2) 
        {
            /* Show */
            printk("dropped_packets_bit_map 0x%X\n", dropped_packets_bit_map);
            return 0;
        }
        else if (argc < 4)
        {
            /* Message. Silenty ignore argc > 4 due to bug(??) in caller when providing argc/argv */
            printk("ERROR: dropped_packets_bit_map 0x%X - %d parameters: argv[0] %s, argv[1] %s, argv[2] %s, argv[3] %s",
                   dropped_packets_bit_map, argc
                   ,argc > 0 ? argv[0] : NULL       /* Print agrv[i] if it exists */
                   ,argc > 1 ? argv[1] : NULL
                   ,argc > 2 ? argv[2] : NULL
                   ,argc > 3 ? argv[3] : NULL
                   );
            return -1;
        }

        cmd = (Int32) simple_strtol(argv[2], NULL, 0);
        val = (Int32) simple_strtol(argv[3], NULL, 0);
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
        Uint32 templateSize = 0;
        Int8 *endptr;

        if (argc != 4)
        {
            printk("Test commands format: set **PktData*gress PktString PktLength\n");
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
            printk("Could not allocate %d bytes for testPktP\n", *testPktSizeP);
            return -ENOMEM;
        }
        memset(*testPktP, 0, *testPktSizeP);

        /* Copy the packet */
        endptr = argv[2];
        while (true)
        {
            long val;
            val = (Int32)simple_strtol(endptr, (char**)&endptr, 16);
            if ((val < 0) || (val > 0xFF) || ((*endptr != ':') && (*endptr != '-') && (*endptr != '\0')))
            {
                printk("\nError found\n");
                kfree(*testPktP);
                return -EINVAL;
            }
            (*testPktP)[templateSize++] = (Int8)val;
            if (*endptr == '\0')
            {
                break;
            }
            if (templateSize > *testPktSizeP)
            {
                printk("Warning: packet size received is shorter than the template - truncating packet\n");
                break;
            }
            endptr++;
        }

        return 0;
    }

    /* Control comes here if the command was not understood. */
    return -EINVAL;
}

/**************************************************************************
 * FUNCTION NAME : hil_reset_cmd_handler
 **************************************************************************
 * DESCRIPTION   :
 *  This function is the Packet Processor reset command handler.
 *
 * RETURNS       :
 *  -1      - Error.
 *  0       - Success.
 ***************************************************************************/
static Int32 hil_reset_cmd_handler(Int32 argc, Int8* argv[])
{
    /* Validate the number of arguments that have been passed. */
    if (argc < 2)
    {
        printk ("ERROR: Incorrect Number of parameters passed.\n");
        return -1;
    }

    /* Reset the session timeout to default, i.e. */
    if (strcmp(argv[1], "timeout") == 0)
    {
        pp_session_timeout_sec = DEFAULT_SESSION_TIMEOUT_SEC;
        printk ("Session timeout returned to default of %d seconds.\n", DEFAULT_SESSION_TIMEOUT_SEC);

        /* Work is completed. */
        return 0;
    }

    /* Reset the HIL Analysis stats. */
    if (strcmp(argv[1], "stats") == 0)
    {
        /* Initialize the counters for the HIL Analysis. */
        global_hil_db.num_total_sessions = 0;
        global_hil_db.num_error = 0;
        global_hil_db.num_bypassed_pkts = 0;
        global_hil_db.num_other_pkts = 0;
        global_hil_db.num_ingress_pkts = 0;
        global_hil_db.num_egress_pkts  = 0;
        global_hil_db.num_null_drop_pkts = 0;

        avalanche_pp_reset_db_stats();

        return 0;
    }
    

    return -1;
}


static int hil_read_devs(char* buf, char **start, off_t offset, int count, int *eof, void *data)
{
    Uint32 i;
    Int32 len=0;
    Uint32 limit = count - 80;

    for(i = 0; i < TI_MAX_DEVICE_INDEX; i++ )
    {
         struct net_device *cur_dev = 0;
         AVALANCHE_PP_VPID_STATS_t vpid_stats;

         cur_dev = dev_get_by_index(&init_net, i);
         if( !cur_dev )
         {
              continue;
         }
         if (cur_dev->vpid_handle >= AVALANCHE_PP_MAX_VPID || cur_dev->vpid_handle < 0)
         {
             dev_put(cur_dev);
             continue;
         }

         /* Get the VPID statistics. */
         if( avalanche_pp_get_stats_vpid(cur_dev->vpid_handle , &vpid_stats) != PP_RC_SUCCESS )
         {
              dev_put(cur_dev);
              continue;
         }

         /* Print the statistics on the console */
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
             len += sprintf(buf + len, "Rx Bytes            : %llu\n", vpid_stats.rx_byte);
         if( len < limit )
             len += sprintf(buf + len, "Rx Discard          : %u\n", vpid_stats.rx_discard_pkt);
         if( len < limit )
             len += sprintf(buf + len, "Tx Unicast   Packets: %u\n", vpid_stats.tx_unicast_pkt);
         if( len < limit )
             len += sprintf(buf + len, "Tx Broadcast Packets: %u\n", vpid_stats.tx_broadcast_pkt);
         if( len < limit )
             len += sprintf(buf + len, "Tx Multicast Packets: %u\n", vpid_stats.tx_multicast_pkt);
         if( len < limit )
             len += sprintf(buf + len, "Tx Bytes            : %llu\n", vpid_stats.tx_byte);
         if( len < limit )
             len += sprintf(buf + len, "Tx Errors           : %u\n", vpid_stats.tx_error);
         if( len < limit )
             len += sprintf(buf + len, "Tx Discards         : %u\n\n", vpid_stats.tx_discard_pkt);
         dev_put(cur_dev);
    }

    return len;
}

static void hil_test_session_creation(void)
{
    struct sk_buff *skb;
    struct net_device *cniDev;
    struct net_device *ethDev;
    Int8 cniVpid;
    Int8 ethVpid;
    Int8 tempVpid;
    AVALANCHE_PP_VPID_INFO_t *vpid;
    AVALANCHE_PP_PID_t *pid;
    AVALANCHE_PP_SESSION_INFO_t *session_info;
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
        printk("hil_test_session_creation: Cannot create session since one of the packets was not configured\n");
        return;
    }
    else if (usSession == False)
    {
        printk("hil_test_session_creation: Cannot create US session since one of the packets was not configured\n");
    }
    else if (dsSession == False)
    {
        printk("hil_test_session_creation: Cannot create DS session since one of the packets was not configured\n");
    }

    cniVpid = -1;
    ethVpid = -1;
    for (tempVpid = AVALANCHE_PP_MAX_VPID - 1; tempVpid >= 0; tempVpid--)
    {
        if (avalanche_pp_vpid_get_info(tempVpid, &vpid) == 0)
        {
            if (avalanche_pp_pid_get_info(vpid->parent_pid_handle, &pid) == 0)
            {
                if (AVALANCHE_PP_PID_TYPE_DOCSIS == pid->type)
                {
                    cniVpid = tempVpid;
                    if (ethVpid != -1)
                    {
                        break;
                    }
                }
                if (AVALANCHE_PP_PID_TYPE_ETHERNET == pid->type)
                {
                    ethVpid = tempVpid;
                    if (cniVpid != -1)
                    {
                        break;
                    }
                }
            }
        }
    }
    if ((ethVpid == -1) || (cniVpid == -1))
    {
        printk("hil_test_session_creation: Failed to acquire ethVpid or cniVpid\n");
        return;
    }

    if(!(skb = dev_alloc_skb(2048)))
    {
        printk("hil_test_session_creation: Failed to allocate skb\n");
        return;
    }

    skb->mac_header = skb->data;
    // TBD
    skb->pp_packet_info.egress_queue = TI_PPM_EGRESS_QUEUE_INVALID;

    /********************/
    /* US ingress packet*/
    /********************/
    session_info = &skb->pp_packet_info.pp_session;
    session_info->ingress.vpid_handle = ethVpid;
    session_info->egress.vpid_handle = cniVpid;
    memcpy(skb->data, usPktDataIngressP, usPktDataIngressSize);

    ethDev = __dev_get_by_name(&init_net, "l2sd0");
    skb->skb_iif = ethDev->ifindex;
    skb->dev = ethDev;
    hil_ingress_hook(skb);
    ethDev = __dev_get_by_name(&init_net, "l2sd0.2");
    skb->skb_iif = ethDev->ifindex;
    skb->dev = ethDev;

    if (usSession)
    {
        hil_ingress_hook(skb); 
    }
    

    /*******************/
    /* US egress packet*/
    /*******************/
    cniDev = __dev_get_by_name(&init_net, "cni0");
    skb->skb_iif = cniDev->ifindex;
    skb->dev = cniDev;
    memcpy(skb->data, usPktDataEgressP, usPktDataEgressSize);
    if (usSession)
    {
        hil_egress_hook(skb);
    }

    /********************/
    /* DS ingress packet*/
    /********************/
    skb->skb_iif = cniDev->ifindex;
    skb->dev = cniDev;
    memset((Uint8*)&skb->pp_packet_info.pp_session, 0, sizeof(skb->pp_packet_info.pp_session));
    session_info = &skb->pp_packet_info.pp_session;
    session_info->ingress.vpid_handle = cniVpid;
    session_info->egress.vpid_handle = ethVpid;
    memcpy(skb->data, dsPktDataIngressP, dsPktDataIngressSize);
    if (dsSession)
    {
        hil_ingress_hook(skb); 
    }
    dev_put(cniDev);

    /*******************/
    /* DS egress packet*/
    /*******************/
    memcpy(skb->data, dsPktDataEgressP, dsPktDataEgressSize);

    ethDev = __dev_get_by_name(&init_net, "l2sd0.2");
    skb->skb_iif = ethDev->ifindex;
    skb->dev = ethDev;
    hil_egress_hook(skb);
    ethDev = __dev_get_by_name(&init_net, "l2sd0");
    skb->skb_iif = ethDev->ifindex;
    skb->dev = ethDev;

    if (dsSession)
    {
        hil_egress_hook(skb);
    }
    dev_put(ethDev);

    /* Change sessions state to forwarding and set session serial number */
    *(Uint16*)((Uint32)(((Uint32)IO_ADDRESS(0x0330000C)) + (2047 * 0x50))) = 0x0003;
    if (usSession && dsSession)
    {
        *(Uint16*)((Uint32)(((Uint32)IO_ADDRESS(0x0330000C)) + (2046 * 0x50))) = 0x0103;
    }
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

    global_hil_db.ipv6rd_pp_support = True;

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

    global_hil_db.ipv6rd_pp_support = False;

    return 0;
}

EXPORT_SYMBOL(ti_hil_enable_6rd_pp);
EXPORT_SYMBOL(ti_hil_disable_6rd_pp);

