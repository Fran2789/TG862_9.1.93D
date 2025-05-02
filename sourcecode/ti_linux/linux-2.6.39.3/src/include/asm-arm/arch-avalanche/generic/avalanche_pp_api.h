/*
  This file is provided under a dual BSD/GPLv2 license.  When using or 
  redistributing this file, you may do so under either license.

  GPL LICENSE SUMMARY

  Copyright(c) 2015 Intel Corporation.

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

  BSD LICENSE 

  Copyright(c) 2014 Intel Corporation. All rights reserved.

  Redistribution and use in source and binary forms, with or without 
  modification, are permitted provided that the following conditions 
  are met:

    * Redistributions of source code must retain the above copyright 
      notice, this list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above copyright 
      notice, this list of conditions and the following disclaimer in 
      the documentation and/or other materials provided with the 
      distribution.

    * Neither the name of Intel Corporation nor the names of its 
      contributors may be used to endorse or promote products derived 
      from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR 
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY 
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/
#ifndef     _AVALANCHE_PP_H
#define     _AVALANCHE_PP_H

#include <asm-arm/arch-avalanche/generic/_tistdtypes.h>

#ifdef __KERNEL__
#include <asm-arm/arch-avalanche/puma6/puma6_cppi_prv.h>
#else
#include <puma6_cppi_prv.h>
#endif

#include <linux/ioctl.h>

/**************************************************************************
 ****************************** Limit Definitions *************************
 **************************************************************************/


/* These are the maximum number of PID,VPID & Sessions that are supported.*/
#define AVALANCHE_PP_MAX_PID                            32
#define AVALANCHE_PP_MAX_VPID                           32
#define AVALANCHE_PP_MAX_ACCELERATED_SESSIONS           2048
#define AVALANCHE_PP_MAX_LUT1_KEYS                      256
#define AVALANCHE_PP_MAX_ACCELERATED_VOICE_SESSIONS     16

#define AVALANCHE_PP_MAX_ACCELERATED_TDOX_SESSIONS      256

#define MAX_ALLOWED_QOS_CLUSTERS_PER_DEVICE             16

/**************************************************************************
 * STRUCTURE NAME : AVALANCHE_PP_VERSION_t
 **************************************************************************
 * DESCRIPTION   :
 *  The structure contains four version notation numbers.
 **************************************************************************/
typedef struct  // former TI_PP_VERSION
{
    unsigned char   v0;
    unsigned char   v1;
    unsigned char   v2;
    unsigned char   v3;
}
AVALANCHE_PP_VERSION_t;




/* ******************************************************************** */
/*                                                                      */
/*                      ____ ___ ____                                   */
/*                     |  _ \_ _|  _ \                                  */
/*                     | |_) | || | | |                                 */
/*                     |  __/| || |_| |                                 */
/*                     |_|  |___|____/                                  */
/*                                                                      */
/*                                                                      */
/* ******************************************************************** */

/**************************************************************************
 ****************************** PID Flag Definitions **********************
 **************************************************************************/

#define AVALANCHE_PP_PID_VLAN_PRIO_MAP            0x1
#define AVALANCHE_PP_PID_DIFFSRV_PRIO_MAP         0x2
#define AVALANCHE_PP_PID_PRIO_OFF_TX_DST_TAG      0x4
#define AVALANCHE_PP_PID_CLASSIFY_BYPASS          0x8
#define AVALANCHE_PP_PID_DISCARD_ALL_RX           0x40

/**************************************************************************
 ****************************** PID Type Definitions **********************
 **************************************************************************/

#define AVALANCHE_PP_PID_TYPE_UNDEFINED            0x0
#define AVALANCHE_PP_PID_TYPE_ETHERNET             0x1
#define AVALANCHE_PP_PID_TYPE_INFRASTRUCTURE       0x2
#define AVALANCHE_PP_PID_TYPE_USBBULK              0x3
#define AVALANCHE_PP_PID_TYPE_CDC                  0x4
#define AVALANCHE_PP_PID_TYPE_DOCSIS               0x5
#define AVALANCHE_PP_PID_TYPE_ETHERNETSWITCH       0x6

/**************************************************************************
 ****************************** PID Ingress Framing ***********************
 **************************************************************************/

#define AVALANCHE_PP_PID_INGRESS_ETHERNET          0x1
#define AVALANCHE_PP_PID_INGRESS_IPV4              0x2
#define AVALANCHE_PP_PID_INGRESS_IPV6              0x4
#define AVALANCHE_PP_PID_INGRESS_IPOE              0x8
#define AVALANCHE_PP_PID_INGRESS_IPOA              0x10
#define AVALANCHE_PP_PID_INGRESS_PPPOE             0x20
#define AVALANCHE_PP_PID_INGRESS_PPPOA             0x40

/**************************************************************************
 ****************************** PID Generic Definitions *******************
 **************************************************************************/
#define AVALANCHE_PP_PID_NO_DST_TAG                0x3FFF

/**************************************************************************
 * STRUCTURE NAME : AVALANCHE_PP_PID_t
 **************************************************************************
 * DESCRIPTION   :
 *  The structure describes the PID. The information in the PID structure
 *  is very hardware specific and is typically filled up by the drivers
 **************************************************************************/
typedef struct // former TI_PP_PID
{
    Uint8           pid_handle;
    Uint8           priv_flags;
    Uint8           type;
    Uint8           ingress_framing;

    Uint8           dflt_pri_drp;
    Uint8           pri_mapping;
    Uint16          dflt_fwd_q;

    Uint16          dflt_dst_tag;
    Uint16          tx_pri_q_map[ 8 ];
    Uint8           tx_hw_data[ 64 ];
    Uint8           tx_hw_data_len;
}
AVALANCHE_PP_PID_t;

/**************************************************************************
 * STRUCTURE NAME : AVALANCHE_PP_PID_RANGE_t
 **************************************************************************
 * DESCRIPTION   :
 *  The structure describes the PID range.
 **************************************************************************/
typedef struct // former TI_PP_PID_RANGE
{
    Uint8       port_num;
    Uint8       type;
    Uint8       count;
    Uint8       base_index;
}
AVALANCHE_PP_PID_RANGE_t;


/* ******************************************************************** */
/*                                                                      */
/*                       ___       ____                                 */
/*                      / _ \  ___/ ___|                                */
/*                     | | | |/ _ \___ \                                */
/*                     | |_| | (_) |__) |                               */
/*                      \__\_\\___/____/                                */
/*                                                                      */
/*                                                                      */
/* ******************************************************************** */

#define AVALANCHE_PP_QOS_CLST_MAX_QCNT          8
#define AVALANCHE_PP_QOS_CLST_MAX_EGRESS_QCNT   4
#define AVALANCHE_PP_QOS_CLST_MAX_INDX          31
#define AVALANCHE_PP_QOS_QUEUE_MAX_INDX         (PAL_CPPI41_SR_QPDSP_QOS_Q_LAST - PAL_CPPI41_SR_QPDSP_QOS_Q_BASE)
#define AVALANCHE_PP_QOS_Q_REALTIME            (1<<0)




/**************************************************************************
 * STRUCTURE NAME : AVALANCHE_PP_QOS_QUEUE
 **************************************************************************
 * DESCRIPTION   :
 *  The structure describes a QOS Queue configuration. Generally QOS
 *  configuration for all queues associated to a single cluster is contained in
 *  corresponding Cluster configuration.
 **************************************************************************/
typedef struct // former TI_PP_QOS_QUEUE
{
    Uint8               q_num;                  /* Index of the QOS queue (offset from QOS queue base) */
    Uint8               flags;                  /* Control how packets in the queue should be handled. Available options: AVALANCHE_PP_QOS_Q_REALTIME - Disable scaling of the credit. */
    Uint16              egr_q;                  /* Queue manager and queue index of forwarding queue */

    Uint32              it_credit_bytes;        /* The amount of forwarding byte “credit” that the queue receives every 25us. */
    Uint16              it_credit_packets;      /* The amount of forwarding packets “credit” that the queue receives every 25us. */
    Uint32              max_credit_bytes;       /* The maximum amount of forwarding byte “credit” that the queue is allowed to hold at the end of the 25us iteration. */
    Uint16              max_credit_packets;     /* The maximum amount of forwarding byte “credit” that the queue is allowed to hold at the end of the 25us iteration. */
    Uint32              congst_thrsh_bytes;     /* The size in bytes at which point the QOS queue is considered to be congested. */
    Uint16              congst_thrsh_packets;   /* The maximum number of packets to be kept in QOS queue */
}
AVALANCHE_PP_QOS_QUEUE_t;

/**************************************************************************
 * STRUCTURE NAME : AVALANCHE_PP_QOS_CLST_CFG
 **************************************************************************
 * DESCRIPTION   :
 *  The structure describes a QOS cluster. It contains the configuration for
 *  all the associated Queues
 **************************************************************************/
typedef struct // former TI_PP_QOS_CLST_CFG
{
    AVALANCHE_PP_QOS_QUEUE_t    qos_q_cfg[ AVALANCHE_PP_QOS_CLST_MAX_QCNT ]; /* Configuration for all the queues associated with the cluster, arranged in order of priority (qos_q_cfg[0] being lower in priority than qos_q_cfg[1]) */
    Uint8                       qos_q_cnt;                /* Number of QOS queues in the cluster (1 to 8) */

    Uint32                      global_credit_bytes;      /* The amount of global bytes credit available to the next QOS queue in the cluster */
    Uint32                      max_global_credit_bytes;  /* The maximum amount of global credit allowed to carry over to the next  queue. */

    Uint16                      global_credit_packets;    /* The amount of global pkt credit available to the next QOS queue in the cluster */
    Uint16                      max_global_credit_packets; /* The maximum amount of global credit allowed to carry over to the next  queue. */

    Uint32                      egr_congst_thrsh_bytes1;  /* Egress Congestion Bytes Threshold 1 */
    Uint32                      egr_congst_thrsh_bytes2;  /* Egress Congestion Bytes Threshold 2 */
    Uint32                      egr_congst_thrsh_bytes3;  /* Egress Congestion Bytes Threshold 3 */
    Uint32                      egr_congst_thrsh_bytes4;  /* Egress Congestion Bytes Threshold 4 */

    Uint16                      egr_congst_thrsh_packets1; /* Egress Congestion Pkts Threshold 1*/
    Uint16                      egr_congst_thrsh_packets2; /* Egress Congestion Pkts Threshold 2 */
    Uint16                      egr_congst_thrsh_packets3; /* Egress Congestion Pkts Threshold 3 */
    Uint16                      egr_congst_thrsh_packets4; /* Egress Congestion Pkts Threshold 4 */

}
AVALANCHE_PP_QOS_CLST_CFG_t;


/**************************************************************************
 * STRUCTURE NAME : AVALANCHE_PP_QOS_QUEUE_STATS
 **************************************************************************
 * DESCRIPTION   :
 *  The structure contains packet statistics for a QOS queue.
 **************************************************************************/
typedef struct // former TI_PP_QOS_QUEUE_STATS
{
    Uint32      fwd_pkts;            /* Number of packets forwarded to the Egress Queue */
    Uint32      drp_cnt;             /* Number of packets dropped due to congestion */
}
AVALANCHE_PP_QOS_QUEUE_STATS_t;


/* ******************************************************************** */
/*                                                                      */
/*                __     ______ ___ ____                                */
/*                \ \   / /  _ \_ _|  _ \                               */
/*                 \ \ / /| |_) | || | | |                              */
/*                  \ V / |  __/| || |_| |                              */
/*                   \_/  |_|  |___|____/                               */
/*                                                                      */
/*                                                                      */
/* ******************************************************************** */


/**************************************************************************
 * STRUCTURE NAME : AVALANCHE_PP_VPID_TYPE_e
 **************************************************************************
 * DESCRIPTION   :
 *  The enumeration defines the type of network to which a VPID is
 *  connected.
 **************************************************************************/
typedef enum // former TI_PP_VPID_TYPE
{
    AVALANCHE_PP_VPID_ETHERNET   = 0x0,
    AVALANCHE_PP_VPID_VLAN       = 0x1,
    AVALANCHE_PP_VPID_PPPoE      = 0x2,
    AVALANCHE_PP_VPID_VLAN_PPPoE = 0x3,
}
AVALANCHE_PP_VPID_TYPE_e;

/**************************************************************************
 ****************************** VPID Flag Definitions *********************
 **************************************************************************/
#define AVALANCHE_PP_VPID_FLG_RX_DFLT_FWD          (1 << 3)
#define AVALANCHE_PP_VPID_FLG_TX_DISBL             (1 << 5)
#define AVALANCHE_PP_VPID_FLG_RX_DISBL             (1 << 6)
#define AVALANCHE_PP_VPID_FLG_VALID                (1 << 7)



/**************************************************************************
 * STRUCTURE NAME : AVALANCHE_PP_VPID_INFO_t
 **************************************************************************
 * DESCRIPTION   :
 *  The structure describes the VPID entity. All packets which enter and
 *  exit the PP do so via a VPID Entity. Thus VPID's can be considered as
 *  end points for all the networking traffic through the box. They map
 *  to the OS networking interface objects which are the end points
 *  connecting the driver to the OS networking stack.
 **************************************************************************/
typedef struct // former TI_PP_VPID
{
    /* This is the VPID Handle. Users dont need to populate this field since
     * this handle will be filled up the PPM on the successful creation
     * of the VPID. */
    Uint8                           vpid_handle;

    /* This is the parent PID handle. All VPID are related to the PID
     * Users need to specify the Parent PID handle in this field. */
    Uint8                           parent_pid_handle;

    /* This describes the VPID Type. VPID are networking endpoints and
     * which describe the type of network to which they are connected. */
    AVALANCHE_PP_VPID_TYPE_e        type;

    /* Private Data. */
    Uint8                           flags;

    /* This is an optional VLAN Identifier associated with the VPID. This
     * is required only if the VPID is attached to a VLAN enabled network
     * Thus this field will be used only if the "type" is either AVALANCHE_PP_VLAN
     * or AVALANCHE_PP_VLAN_PPPoE */
    Uint16                          vlan_identifier;

    /* This is an optional PPP Session Identifier associated with the VPID
     * This is required if the interface is a PPP Endpoint. This fields is
     *
     * checked only if the "type" field is AVALANCHE_PP_PPPoE or AVALANCHE_PP_VLAN_PPPoE */
    Uint16                          ppp_session_id;


    /* These are the QoS related settings */
    AVALANCHE_PP_QOS_CLST_CFG_t *   qos_cluster[ MAX_ALLOWED_QOS_CLUSTERS_PER_DEVICE ];
    unsigned char                   qos_clusters_count;
}
AVALANCHE_PP_VPID_INFO_t;


/**************************************************************************
 * STRUCTURE NAME : TI_PP_VPID_STATS
 **************************************************************************
 * DESCRIPTION   :
 *  The structure describes the VPID statistics for the Packet Processor.
 **************************************************************************/
typedef struct // former TI_PP_VPID_STATS
{
    Uint64      rx_byte;
    Uint32      rx_unicast_pkt;
    Uint32      rx_broadcast_pkt;
    Uint32      rx_multicast_pkt;
    Uint32      rx_discard_pkt;

    Uint64      tx_byte;
    Uint32      tx_unicast_pkt;
    Uint32      tx_broadcast_pkt;
    Uint32      tx_multicast_pkt;
    Uint32      tx_discard_pkt;

    Uint32      tx_error;
} 
AVALANCHE_PP_VPID_STATS_t;



/* ******************************************************************** */
/*                                                                      */
/*                 ____                _                                */
/*                / ___|  ___  ___ ___(_) ___  _ __                     */
/*                \___ \ / _ \/ __/ __| |/ _ \| '_ \                    */
/*                 ___) |  __/\__ \__ \ | (_) | | | |                   */
/*                |____/ \___||___/___/_|\___/|_| |_|                   */
/*                                                                      */
/*                                                                      */
/* ******************************************************************** */
typedef enum
{
    AVALANCHE_PP_LUT_ENTRY_L2_ETHERNET,
    AVALANCHE_PP_LUT_ENTRY_reserved_1,
    AVALANCHE_PP_LUT_ENTRY_reserved_2,
    AVALANCHE_PP_LUT_ENTRY_reserved_3,
    AVALANCHE_PP_LUT_ENTRY_reserved_4,
    AVALANCHE_PP_LUT_ENTRY_reserved_5,
    AVALANCHE_PP_LUT_ENTRY_reserved_6,
    AVALANCHE_PP_LUT_ENTRY_L2_UNDEFINED,

    AVALANCHE_PP_LUT_ENTRY_L3_IPV4,
    AVALANCHE_PP_LUT_ENTRY_L3_IPV6,
    AVALANCHE_PP_LUT_ENTRY_L3_DSLITE,
    AVALANCHE_PP_LUT_ENTRY_L3_GRE,
    AVALANCHE_PP_LUT_ENTRY_reserved_12,
    AVALANCHE_PP_LUT_ENTRY_reserved_13,
    AVALANCHE_PP_LUT_ENTRY_reserved_14,
    AVALANCHE_PP_LUT_ENTRY_L3_UNDEFINED,
}
AVALANCHE_PP_LUT_ENTRY_TYPE_e;

/* LUT1 L2-ETH Enable Flags */
typedef enum
{
    AVALANCHE_PP_LUT1_FIELD_ENABLE_PID           = 0x00000040,
    AVALANCHE_PP_LUT1_FIELD_ENABLE_MAC_DST       = 0x00000003,
    AVALANCHE_PP_LUT1_FIELD_ENABLE_MAC_SRC       = 0x0000000E,
    AVALANCHE_PP_LUT1_FIELD_ENABLE_ETH_TYPE      = 0x00000020, 
    AVALANCHE_PP_LUT1_FIELD_ENABLE_L2_ENTRY_TYPE = 0x00000080,
    AVALANCHE_PP_LUT1_FIELD_ENABLE_L2_MAX_VAL    = 0xFFFFFFFF
}
AVALANCHE_PP_LUT1_L2_FIELD_ENABLE_e;

/* LUT1 L3-IP Enable Flags */
typedef enum
{
    AVALANCHE_PP_LUT1_FIELD_ENABLE_LAN_IPv4      = 0x00000001,
    AVALANCHE_PP_LUT1_FIELD_ENABLE_LAN_IPv6      = 0x0000003F,
    AVALANCHE_PP_LUT1_FIELD_ENABLE_IP_PROTOCOL   = 0x00000100,
    AVALANCHE_PP_LUT1_FIELD_ENABLE_PPoE_SESS_ID  = 0x000000C0,
    AVALANCHE_PP_LUT1_FIELD_ENABLE_L3_ENTRY_TYPE = 0x00000200,
    AVALANCHE_PP_LUT1_FIELD_ENABLE_L3_MAX_VAL    = 0xFFFFFFFF
}
AVALANCHE_PP_LUT1_L3_FIELD_ENABLE_e;



typedef struct
{
    union
    {
        /*------------------------------------------*/
        Uint8       raw[48];
        /*------------------------------------------*/
        struct
        {
            struct
            {
                Uint8                                   dstmac[6];
                Uint8                                   srcmac[6];
                Uint16                                  eth_type;
                Uint16                                  res_S3;
                Uint8                                   res_B3;
                Uint8                                   res_B2;
                Uint8                                   entry_type;             /* Type: AVALANCHE_PP_LUT_ENTRY_TYPE_e */ 
                Uint8                                   pid_handle;             /* Type: PP_PID_NUM_e */

                AVALANCHE_PP_LUT1_L2_FIELD_ENABLE_e     enable_flags;
            }
            L2;

            struct
            {
                union
                {
                    Uint32                              v4;
                    Uint32                              v6[ 4 ];
                }
                LAN_addr_IP;

                Uint8                                   entry_type;              /* Type: AVALANCHE_PP_LUT_ENTRY_TYPE_e */ 
                Uint8                                   ip_protocol;
                Uint16                                  PPPoE_session_id;

                AVALANCHE_PP_LUT1_L3_FIELD_ENABLE_e     enable_flags;
            }
            L3;

        }
        fields;
        /*------------------------------------------*/
    }u;
}
__Avalanche_PP_LUT1_inputs_t;



/* LUT2 Enable Flags */
typedef enum
{
    AVALANCHE_PP_LUT2_FIELD_ENABLE_WAN_IP           = 0x01,  
    AVALANCHE_PP_LUT2_FIELD_ENABLE_IPV6_FLOW        = 0x02,
    AVALANCHE_PP_LUT2_FIELD_ENABLE_DSLITE_IPV4      = AVALANCHE_PP_LUT2_FIELD_ENABLE_IPV6_FLOW,
    AVALANCHE_PP_LUT2_FIELD_ENABLE_1ST_VLAN         = 0x04,
    AVALANCHE_PP_LUT2_FIELD_ENABLE_2ND_VLAN         = 0x08,
    AVALANCHE_PP_LUT2_FIELD_ENABLE_SRC_PORT         = 0x10,
    AVALANCHE_PP_LUT2_FIELD_ENABLE_DST_PORT         = 0x20,
    AVALANCHE_PP_LUT2_FIELD_ENABLE_IP_TOS           = 0x40
}
AVALANCHE_PP_LUT2_FIELD_ENABLE_e;


typedef struct
{
    union
    {
        /*------------------------------------------*/
        Uint8       raw[32];
        /*------------------------------------------*/
        struct
        {
            union
            {
                Uint32                              v4;
                Uint32                              v6[ 4 ];
            }
            WAN_addr_IP;

            union
            {
                Uint32                              v4_dsLite;
                Uint8                               v6_FlowLabel[ 4 ];
            }IP;

            Uint16                                  firstVLAN;
            Uint16                                  secondVLAN;
            Uint16                                  L4_SrcPort;
            Uint16                                  L4_DstPort;

            Uint8                                   TOS;
            Uint8                                   LUT1_key;
            Uint8                                   entry_type;                  /* Type: AVALANCHE_PP_LUT_ENTRY_TYPE_e */ 

            Uint8                                   enable_flags;               /* Type: AVALANCHE_PP_LUT2_FIELD_ENABLE_e */
        }
        fields;
        /*------------------------------------------*/
    }u;
}
__Avalanche_PP_LUT2_inputs_t;


typedef enum
{
    AVALANCHE_PP_DEV_TYPE_UNKNOWN,
    AVALANCHE_PP_DEV_TYPE_WAN,
    AVALANCHE_PP_DEV_TYPE_LAN,
}
AVALANCHE_PP_DEV_TYPE_e;

typedef struct
{
    __Avalanche_PP_LUT1_inputs_t        LUT1;
    __Avalanche_PP_LUT2_inputs_t        LUT2;
}
__Avalanche_PP_LUTs_Data_t;


/**************************************************************************
 * STRUCTURE NAME : AVALANCHE_PP_INGRESS_SESSION_PROPERTY_t
 **************************************************************************
 * DESCRIPTION   :
 *  The structure describes the session properties. Fundamentally a session
 *  consists of 2 properties i.e. Ingress and Egress. Each prorty internally
 *  describes the interface on which the packet was rxed (Ingress) or txed
 *  (Egress) and how the packet looked at Ingress or Egress.
 **************************************************************************/
typedef struct // former TI_PP_SESSION_PROPERTY
{
    Uint8                               vpid_handle;
    Uint8                               dev_type;       /* Type : AVALANCHE_PP_DEV_TYPE_e */
    Uint16                              reserved;

    __Avalanche_PP_LUTs_Data_t          lookup;
}
AVALANCHE_PP_INGRESS_SESSION_PROPERTY_t;


#define AVALNCHE_PP_WRAP_HEADER_MAX_LEN 64

/* ARRIS propagate INTEL MR25220 fix */
typedef enum
{
    AVALANCHE_PP_EGRESS_FIELD_ENABLE_L2                     = 0x0001,
    AVALANCHE_PP_EGRESS_FIELD_ENABLE_VLAN                   = 0x0002,
    AVALANCHE_PP_EGRESS_FIELD_ENABLE_IP                     = 0x0004,
    AVALANCHE_PP_EGRESS_FIELD_ENABLE_L4                     = 0x0008,
    AVALANCHE_PP_EGRESS_FIELD_ENABLE_PSI                    = 0x0010,
    AVALANCHE_PP_EGRESS_FIELD_ENABLE_ENCAPSULATION          = 0x0020,
    AVALANCHE_PP_EGRESS_FIELD_ENABLE_TDOX_ENABLED           = 0x0040,
    AVALANCHE_PP_EGRESS_FIELD_ENABLE_TDOX_SKIP_TIMESTAMP    = 0x0080, 
    AVALANCHE_PP_EGRESS_FIELD_ENABLE_TCP_SYN                = 0x0100,
    AVALANCHE_PP_EGRESS_FIELD_ENABLE_TCP_CTRL               = 0x0200
	/* Note: The maximum for this enum is 16 bits */
}
AVALANCHE_PP_EGRESS_FIELD_ENABLE_e;
/* ARRIS propagate end */

typedef struct
{
    Uint8                               sf_index;
    Uint8                               phs;
    Uint8                               tcp_flags;
    Uint8                               tdox_id;
}
AVALANCHE_PP_PSI_t;

/**************************************************************************
 * STRUCTURE NAME : AVALANCHE_PP_EGRESS_SESSION_PROPERTY_t
 **************************************************************************
 * DESCRIPTION   :
 *  The structure describes the session properties. Fundamentally a session
 *  consists of 2 properties i.e. Ingress and Egress. Each property internally
 *  describes the interface on which the packet was rxed (Ingress) or txed
 *  (Egress) and how the packet looked at Ingress or Egress.
 **************************************************************************/
typedef struct // former TI_PP_SESSION_PROPERTY
{
    Uint8                               vpid_handle;
    Uint8                               dev_type;           /* Type : AVALANCHE_PP_DEV_TYPE_e */
    /* ARRIS propagate INTEL MR25220 fix */
    Uint16                              enable;             /* Type: AVALANCHE_PP_EGRESS_FIELD_ENABLE_e */ 
    /* ARRIS propagate end */
    union 
    {
        AVALANCHE_PP_PSI_t              us_fields;
        Uint32                          us_psi_word;
    } 
    psi;
#define psi_word			            psi.us_psi_word
#define psi_tdox_id                     psi.us_fields.tdox_id

    Uint32                              tdox_tcp_ack_number;

    Uint8                               tdox_handle;
    Uint8                               l2_packet_type;     /* Type : AVALANCHE_PP_LUT_ENTRY_TYPE_e */
    Uint8                               wrapHeaderDataLenOffset;
    Uint8                               wrapHeaderLen;

    Uint8                               wrapHeader[ AVALNCHE_PP_WRAP_HEADER_MAX_LEN ];

    Uint8                               dstmac[6];
    Uint8                               srcmac[6];
    Uint16                              vlan;
    Uint16                              eth_type;
    
    Uint8                               l3_packet_type;     /* Type : AVALANCHE_PP_LUT_ENTRY_TYPE_e */
    Uint8                               tunnel_type;        /* Type : AVALANCHE_PP_LUT_ENTRY_TYPE_e */
    Uint8                               ip_protocol;
    Uint8                               TOS;
    union
    {
        Uint32                          v4;
        Uint32                          v6[ 4 ];
    }
    SRC_IP;
    union
    {
        Uint32                          v4;
        Uint32                          v6[ 4 ];
    }
    DST_IP;

    Uint16                              L4_SrcPort;
    Uint16                              L4_DstPort;
}
AVALANCHE_PP_EGRESS_SESSION_PROPERTY_t;

typedef enum
{
    AVALANCHE_PP_SESSIONS_POOL_DATA,
    AVALANCHE_PP_SESSIONS_POOL_VOICE,
    AVALANCHE_PP_SESSIONS_POOL_MAX
}
AVALANCHE_PP_SESSIONS_POOL_ID_e;

typedef struct
{
    Uint32  packets_forwarded;
    Uint32  bytes_forwarded;
    Int32   last_update_time;
}AVALANCHE_PP_SESSION_TDOX_STATS_t;

/**************************************************************************
 * STRUCTURE NAME : AVALANCHE_PP_SESSION_INFO_t
 **************************************************************************
 * DESCRIPTION   :
 *  The structure describes the Session
 **************************************************************************/
typedef struct // former TI_PP_SESSION
{
    /* This is the session handle. Users dont need to populate this
     * information since this handle will be filled up the PPM on the
     * successful creation of the session. */
    Uint32                  session_handle;

    /* Session Timeout indicates the number of micro-seconds of inactivity
     * after which the PP generates an event to the host. The field if set
     * to 0 indicates that the session needs to be configured permanently
     * and is not subject to IDLE based timeouts. */
    Uint32                  session_timeout;

    /* Flag which indicates the priority of the session.
     * With the introduction of QoS this will play an important part. */
    Uint8                   priority;
    Uint8                   cluster;

    /* Flag which indicates if the session was for a ROUTER or BRIDGE
     * This information is required because if the session is for the
     * ROUTER the PDSP needs to ensure that all packets matching their
     * session have their TTL decremented. On the other hand for BRIDGE
     * sessions this is not do be done. */
    Uint8                   is_routable_session;
    Uint8                   session_pool;

    /* Ingress Properties:-
     *  These describe the ingress interface on which the packet was
     *  received and how the packet looks like when it arrives into the
     *  box. */
    AVALANCHE_PP_INGRESS_SESSION_PROPERTY_t     ingress;

    /* Egress Properties:-
     *  These properties describe the egress interface on which the packet
     *  is to be transmitted and how the packet will look like when it
     *  leaves the box. */
    AVALANCHE_PP_EGRESS_SESSION_PROPERTY_t  egress;

    AVALANCHE_PP_SESSION_TDOX_STATS_t   tdox_stats;
}
AVALANCHE_PP_SESSION_INFO_t;

/**************************************************************************
 * STRUCTURE NAME : AVALANCHE_PP_SESSION_STATS_t
 **************************************************************************
 * DESCRIPTION   :
 *  The structure describes the session statistics for the Packet Processor.
 **************************************************************************/
typedef struct // former TI_PP_SESSION_STATS
{
    Uint64          bytes_forwarded;
    Uint32          packets_forwarded;
} 
AVALANCHE_PP_SESSION_STATS_t;


/**************************************************************************
 * STRUCTURE NAME : TI_PP_GLOBAL_STATS
 **************************************************************************
 * DESCRIPTION   :
 *  The structure describes the global statistics for the Packet Processor.
 **************************************************************************/
typedef struct // former TI_PP_GLOBAL_STATS
{
    Uint32      ppdsp_rx_pkts;
    Uint32      ppdsp_pkts_frwrd_to_cpdsp1;
    Uint32      ppdsp_not_enough_descriptors;

    Uint32      cpdsp1_rx_pkts;
    Uint32      cpdsp1_lut1_search_attempts;
    Uint32      cpdsp1_lut1_matches;
    Uint32      cpdsp1_pkts_frwrd_to_cpdsp2;
     
    Uint32      cpdsp2_rx_pkts;
    Uint32      cpdsp2_lut2_search_attempts;
    Uint32      cpdsp2_lut2_matches;
    Uint32      cpdsp2_pkts_frwrd_to_mpdsp;
    Uint32      cpdsp2_synch_timeout_events;
    Uint32      cpdsp2_reassembly_db_full;
    Uint32      cpdsp2_reassembly_db_timeout;
     
    Uint32      mpdsp_rx_pkts;
    Uint32      mpdsp_ipv4_rx_pkts;
    Uint32      mpdsp_ipv6_rx_pkts;
    Uint32      mpdsp_frwrd_to_host;
    Uint32      mpdsp_frwrd_to_qpdsp;
    Uint32      mpdsp_frwrd_to_synch_q;
    Uint32      mpdsp_discards;
    Uint32      mpdsp_synchq_overflow_events;

    Uint32      qpdsp_ooo_discards;
}
AVALANCHE_PP_GLOBAL_STATS_t;



#ifdef __KERNEL__
/* **************************************************************************************** */
/*                                                                                          */
/*                                                                                          */
/*                                                                                          */
/*                      KERNEL only Stuff                                                   */
/*                                                                                          */
/*                                                                                          */
/*                                                                                          */
/* **************************************************************************************** */

typedef enum
{
    PP_RC_SUCCESS,
    PP_RC_FAILURE,
    PP_RC_INVALID_PARAM,
    PP_RC_OUT_OF_MEMORY,
    PP_RC_OBJECT_EXIST,

    PP_RC_FW_FAILURE = 100,
}
AVALANCHE_PP_RET_e;

typedef enum
{
    PP_LIST_ID_INGRESS,
    PP_LIST_ID_EGRESS,
    PP_LIST_ID_EGRESS_TCP,
    PP_LIST_ID_EGRESS_TDOX,
    PP_LIST_ID_ALL,
}
PP_LIST_ID_e;

typedef AVALANCHE_PP_RET_e   (* AVALANCHE_EXEC_HOOK_FN_t) ( AVALANCHE_PP_SESSION_INFO_t *  ptr_session, Ptr     data );


/* PID and VPID Management API */
extern AVALANCHE_PP_RET_e    avalanche_pp_pid_create            ( AVALANCHE_PP_PID_t * ptr_pid, void * ptr_netdev );
extern AVALANCHE_PP_RET_e    avalanche_pp_pid_delete            ( Uint8     pid_handle );
extern AVALANCHE_PP_RET_e    avalanche_pp_pid_config_range      ( AVALANCHE_PP_PID_RANGE_t *    pid_range );
extern AVALANCHE_PP_RET_e    avalanche_pp_pid_remove_range      ( Uint32    port_num );
extern AVALANCHE_PP_RET_e    avalanche_pp_pid_set_flags         ( Uint8     pid_handle,     Uint32  new_flags );
extern AVALANCHE_PP_RET_e    avalanche_pp_pid_get_list          ( Uint8 *   num_entries, AVALANCHE_PP_PID_t ** pid_list );
extern AVALANCHE_PP_RET_e    avalanche_pp_pid_get_info          ( Uint8     pid_handle,  AVALANCHE_PP_PID_t ** ptr_pid );

extern AVALANCHE_PP_RET_e    avalanche_pp_vpid_create           ( AVALANCHE_PP_VPID_INFO_t *    ptr_vpid );
extern AVALANCHE_PP_RET_e    avalanche_pp_vpid_delete           ( Uint8     vpid_handle );
extern AVALANCHE_PP_RET_e    avalanche_pp_vpid_set_flags        ( Uint8     vpid_handle,    Uint32  new_flags );
extern AVALANCHE_PP_RET_e    avalanche_pp_vpid_get_list         ( Uint8     parent_pid_handle,  Uint8 *     num_entries, AVALANCHE_PP_VPID_INFO_t **     vpid_list );
extern AVALANCHE_PP_RET_e    avalanche_pp_vpid_get_info         ( Uint8     vpid_handle,                                 AVALANCHE_PP_VPID_INFO_t **     ptr_vpid );

/* Session Management API */
extern AVALANCHE_PP_RET_e    avalanche_pp_session_create        ( AVALANCHE_PP_SESSION_INFO_t *  ptr_session, void * pkt_ptr );
extern AVALANCHE_PP_RET_e    avalanche_pp_session_delete        ( Uint32    session_handle,     AVALANCHE_PP_SESSION_STATS_t *  ptr_session_stats );
extern AVALANCHE_PP_RET_e    avalanche_pp_session_get_list      ( Uint8     vpid_handle,        PP_LIST_ID_e   list_id, Uint32 * num_entries, Uint32 * session_handle_list );
extern AVALANCHE_PP_RET_e    avalanche_pp_session_get_info      ( Uint32    session_handle,     AVALANCHE_PP_SESSION_INFO_t**  ptr_session_info );
extern AVALANCHE_PP_RET_e    avalanche_pp_flush_sessions        ( Uint8     vpid_handle, PP_LIST_ID_e   list_id );

extern AVALANCHE_PP_RET_e   avalanche_pp_session_list_execute      ( Uint8     vpid_handle, PP_LIST_ID_e   list_id,    AVALANCHE_EXEC_HOOK_FN_t   handler, Ptr  data );
extern AVALANCHE_PP_RET_e   avalanche_pp_session_pre_action_bind   ( Uint8     vpid_handle,                            AVALANCHE_EXEC_HOOK_FN_t   handler, Ptr  data );
extern AVALANCHE_PP_RET_e   avalanche_pp_session_post_action_bind  ( Uint8     vpid_handle,                            AVALANCHE_EXEC_HOOK_FN_t   handler, Ptr  data );

/* Statistics API */
extern AVALANCHE_PP_RET_e   avalanche_pp_get_stats_session   ( Uint32 session_handle,    AVALANCHE_PP_SESSION_STATS_t*    ptr_session_stats );
extern AVALANCHE_PP_RET_e   avalanche_pp_get_stats_vpid      ( Uint8  vpid_handle,       AVALANCHE_PP_VPID_STATS_t*       ptr_vpid_stats );
extern AVALANCHE_PP_RET_e   avalanche_pp_get_stats_global    (                           AVALANCHE_PP_GLOBAL_STATS_t*     ptr_stats );



/* Event Handler Framework API */
typedef enum
{
    PP_EV_PID_CREATED                   ,
    PP_EV_PID_CREATE_FAIL               ,
    PP_EV_PID_DELETED                   ,
    PP_EV_PID_DELETE_FAIL               ,
    PP_EV_VPID_CREATED                  ,
    PP_EV_VPID_CREATE_FAILED            ,
    PP_EV_VPID_DELETED                  ,
    PP_EV_VPID_DELETE_FAILED            ,
    PP_EV_SESSION_CREATED               ,
    PP_EV_SESSION_CREATE_FAILED         ,
    PP_EV_SESSION_DELETED               ,
    PP_EV_SESSION_DELETE_FAILED         ,
    PP_EV_SESSION_EXPIRED               ,
    PP_EV_MISC_TRIGGER_TDOX_EVALUATION  ,
    PP_EV_MAX                           ,
    PP_EV_MAXVAL = 0xFFFFFFFF
}
AVALANCHE_PP_EVENT_e;

typedef AVALANCHE_PP_RET_e  (* AVALANCHE_EVENT_HANDLER_t)   ( AVALANCHE_PP_EVENT_e  event, Uint32  param1, Uint32 param2 );

extern AVALANCHE_PP_RET_e   avalanche_pp_set_ack_suppression        ( Uint8     enDis );

extern AVALANCHE_PP_RET_e   avalanche_pp_event_handler_register     ( Uint32 *  handle_event_handler, AVALANCHE_EVENT_HANDLER_t   handler );
extern AVALANCHE_PP_RET_e   avalanche_pp_event_handler_unregister   ( Uint32    handle_event_handler );
extern AVALANCHE_PP_RET_e   avalanche_pp_event_report( AVALANCHE_PP_EVENT_e  event, Uint32 param1, Uint32 param2 );

/* QoS API. */
extern AVALANCHE_PP_RET_e   avalanche_pp_qos_cluster_setup      ( Uint8     clst_indx,  AVALANCHE_PP_QOS_CLST_CFG_t*    clst_cfg );
extern AVALANCHE_PP_RET_e   avalanche_pp_qos_cluster_enable     ( Uint8     clst_indx );
extern AVALANCHE_PP_RET_e   avalanche_pp_qos_cluster_disable    ( Uint8     clst_indx );
extern AVALANCHE_PP_RET_e   avalanche_pp_qos_get_queue_stats    ( Uint32    qos_qnum,   AVALANCHE_PP_QOS_QUEUE_STATS_t*     stats );

extern AVALANCHE_PP_RET_e   avalanche_pp_qos_set_cluster_max_global_credit  ( Bool creditTypeBytes, Uint8 cluster_id,   Uint32 max_global_credit );
extern AVALANCHE_PP_RET_e   avalanche_pp_qos_set_queue_max_credit           ( Bool creditTypeBytes, Uint8 queue_id,     Uint32 max_credit );
extern AVALANCHE_PP_RET_e   avalanche_pp_qos_set_queue_iteration_credit     ( Bool creditTypeBytes, Uint8 queue_id,     Uint32 it_credit  );

/* Power Saving Mode (PSM) API. */
extern AVALANCHE_PP_RET_e    avalanche_pp_psm                   ( Uint8     onOff );
extern AVALANCHE_PP_RET_e    avalanche_pp_hw_init               ( void );

/* MISC APIs */
#define AVALANCHE_PP_LUT_HISTOGRAM_SIZE             8
#define AVALANCHE_PP_LUT1_HISTOGRAM_RESOLUTION      (AVALANCHE_PP_MAX_LUT1_KEYS / AVALANCHE_PP_LUT_HISTOGRAM_SIZE)
#define AVALANCHE_PP_LUT2_HISTOGRAM_RESOLUTION      (AVALANCHE_PP_MAX_ACCELERATED_SESSIONS / AVALANCHE_PP_LUT_HISTOGRAM_SIZE)

typedef struct
{
    Uint32                              active_PIDs;
    Uint32                              active_VPIDs;
    Uint32                              active_lut1_keys;
    Uint32                              max_active_lut1_keys;
    Uint32                              active_sessions;
    Uint32                              max_active_sessions;
    Uint32                              lut1_histogram[AVALANCHE_PP_LUT_HISTOGRAM_SIZE];
    Uint32                              lut1_starvation;
    Uint32                              lut2_histogram[AVALANCHE_PP_LUT_HISTOGRAM_SIZE];
    Uint32                              lut2_starvation;
    Uint32                              tdox_starvation;
}
AVALANCHE_PP_Misc_Statistics_t;


extern AVALANCHE_PP_RET_e    avalanche_pp_event_poll_timer_init( void );

extern AVALANCHE_PP_RET_e    avalanche_pp_session_tdox_capability_set( Uint32 session_handle, Bool     enable );
extern AVALANCHE_PP_RET_e    avalanche_pp_session_tdox_capability_get( Uint32 session_handle, Bool *   enable );

extern AVALANCHE_PP_RET_e    avalanche_pp_version_get( AVALANCHE_PP_VERSION_t * version );
extern AVALANCHE_PP_RET_e    avalanche_pp_set_mta_mac_address ( Uint8 * mtaAddress );
extern AVALANCHE_PP_RET_e    avalanche_pp_get_db_stats ( AVALANCHE_PP_Misc_Statistics_t * stats_ptr );
extern AVALANCHE_PP_RET_e    avalanche_pp_reset_db_stats ( void );

extern Bool                  avalanche_pp_state_is_active( void );
extern Bool                  avalanche_pp_state_is_psm( void );


#if 0

/* Utility API. */

extern int avalanche_pp_set_ack_suppression(int enDis);
#endif

#endif

typedef     Uint8     avalanche_pp_ackSupp_ioctl_param_t;
typedef     Uint8     avalanche_pp_psm_ioctl_param_t;
typedef     Uint32    avalanche_pp_dslite_ioctl_param_t;
typedef     Uint8     avalanche_pp_mtaMacAddr_ioctl_param_t[6];

typedef     struct
{

    Uint8       bytePkts;
    Uint32      index;
    Uint32      newValue;

}avalanche_pp_Qos_ioctl_params_t;

/* There QoS may be defined either for physical or virtual device
The QoS setting hooks are being triggered by PID creation.
In case there is a need in alternative QoS scheme to be created it can be
specified by setting the  qos_virtual_scheme_idx to a valid non default index
This alternative scheme creation is being triggered from VPID creation.
It has to be defined by the appropriate device drived */
#define AVALANCHE_PP_NETDEV_PP_QOS_PROFILE_DEFAULT   (-1)
typedef     struct
{
    Int8     device_name[16];
    Int32    qos_virtual_scheme_idx;  
       
}avalanche_pp_dev_ioctl_param_t;
/********************************************************************************************************/
/* IOCTL commands:

   If you are adding new ioctl's to the kernel, you should use the _IO
   macros defined in <linux/ioctl.h> _IO macros are used to create ioctl numbers:

    _IO(type, nr)         - an ioctl with no parameter.
   _IOW(type, nr, size)  - an ioctl with write parameters (copy_from_user), kernel would actually read data from user space
   _IOR(type, nr, size)  - an ioctl with read parameters (copy_to_user), kernel would actually write data to user space
   _IOWR(type, nr, size) - an ioctl with both write and read parameters

   'Write' and 'read' are from the user's point of view, just like the
    system calls 'write' and 'read'.  For example, a SET_FOO ioctl would
    be _IOW, although the kernel would actually read data from user space;
    a GET_FOO ioctl would be _IOR, although the kernel would actually write
    data to user space.

    The first argument to _IO, _IOW, _IOR, or _IOWR is an identifying letter
    or number from the SoC_ModuleIds_e enum located in this file.

    The second argument to _IO, _IOW, _IOR, or _IOWR is a sequence number
    to distinguish ioctls from each other.

   The third argument to _IOW, _IOR, or _IOWR is the type of the data going
   into the kernel or coming out of the kernel (e.g.  'int' or 'struct foo').

   NOTE!  Do NOT use sizeof(arg) as the third argument as this results in
   your ioctl thinking it passes an argument of type size_t.

*/
#define PP_DRIVER_MODULE_ID                    (0xDF) 

#define PP_DRIVER_FLUSH_ALL_SESSIONS                _IO   (PP_DRIVER_MODULE_ID, 1)
#define PP_DRIVER_PSM                               _IOWR (PP_DRIVER_MODULE_ID, 2, avalanche_pp_psm_ioctl_param_t)
#define PP_DRIVER_DELETE_VPID                       _IOWR (PP_DRIVER_MODULE_ID, 3, avalanche_pp_dev_ioctl_param_t)
#define PP_DRIVER_ADD_VPID                          _IOWR (PP_DRIVER_MODULE_ID, 4, avalanche_pp_dev_ioctl_param_t)
#define PP_DRIVER_SET_QOS_CLST_MAX_CREDIT           _IOWR (PP_DRIVER_MODULE_ID, 5, avalanche_pp_Qos_ioctl_params_t)
#define PP_DRIVER_SET_QOS_QUEUE_MAX_CREDIT          _IOWR (PP_DRIVER_MODULE_ID, 6, avalanche_pp_Qos_ioctl_params_t)
#define PP_DRIVER_SET_QOS_QUEUE_ITERATION_CREDIT    _IOWR (PP_DRIVER_MODULE_ID, 7, avalanche_pp_Qos_ioctl_params_t)
#define PP_DRIVER_SET_DS_LITE_US_FRAG_IPV4          _IOWR (PP_DRIVER_MODULE_ID, 8, avalanche_pp_dslite_ioctl_param_t)
#define PP_DRIVER_SET_MTA_ADDR                      _IOWR (PP_DRIVER_MODULE_ID, 9, avalanche_pp_mtaMacAddr_ioctl_param_t)
#define PP_DRIVER_KERNEL_POST_INIT                  _IO   (PP_DRIVER_MODULE_ID, 10)
#define PP_DRIVER_SET_ACK_SUPP                      _IOWR (PP_DRIVER_MODULE_ID, 11, avalanche_pp_ackSupp_ioctl_param_t)

#endif //   _AVALANCHE_PP_H

