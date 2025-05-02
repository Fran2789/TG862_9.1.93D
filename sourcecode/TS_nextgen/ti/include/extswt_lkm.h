/*

Copyright (c) 2014-2017 ARRIS Enterprises, LLC

All rights reserved.

Redistribution and use in source and binary forms, with or without 
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, 
   this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice, 
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.
3. The name of the author may not be used to endorse or promote products 
   derived from this software without specific prior written permission.


Alternatively, this software may be distributed under the terms of the
GNU General Public License ("GPL") version 2 as published by the Free
Software Foundation. 
 

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED 
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. 
IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; 
OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR 
OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF 
ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

GPLv2 license:

This program is free software; you can redistribute it and/or modify it 
under the terms of the GNU General Public License as published by the 
Free Software Foundation; either version 2 of the License, or 
(at your option) any later version.

This program is distributed in the hope that it will be useful, but 
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY 
or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License 
for more details.

You should have received a copy of the GNU General Public License along with 
this program; if not, write to the Free Software Foundation, Inc., 
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

*/

#ifndef DEFS_EXTSWT_LKM_H
#define DEFS_EXTSWT_LKM_H

#include <_tistdtypes.h>
#include <puma_autoconf.h> 

#ifdef CONFIG_SYSTEM_REALTEKSWITCH
// Realtek stuff
#include "rtk_api.h"
#define NUM_PORTS                                   (7)
#define EXTSWT_INTERNAL_PORT                        (NUM_PORTS-1)
#else
// Broadcom stuff
#if PUMA5_SOC_TYPE
// TG852/TG862
// 53124 doesn't actually have 9 ports but the real ports are spread out across 9 bits so it's 
// easiest to look at it like this
// 53124 Ports
// External Port 1 - Bit 0
// External Port 2 - Bit 1
// External Port 3 - Bit 2
// External Port 4 - Bit 3
// Internal Port 4 - Bit 4 (wlan port on TG852/TG862)
// Internal Port 5 - Bit 5 (main port on TG852/TG682)
// Internal Port 6 - Bit 6 (Unused on TG852/TG682)
// IMP Port        - Bit 8 (CPU port - unused on TG852/TG862)
#define NUM_PORTS                   (9)     
#define EXTSWT_INTERNAL_PORT        (5)     /* The management port is 5 on TG852/TG862 */
#define VALID_PORT_BITMASK          (0x13f) /* Mark bits valid - even if unused - so we can disable them*/
#define IS_VALID_PORT(port)         ((1 << port) & VALID_PORT_BITMASK)
#define PHY_PORT_BITMASK            (0x0f)  /* TG862 uses 0,1,2,3 as phy ports*/
#define IS_PHY_PORT(port)           ((1 << port) & PHY_PORT_BITMASK)
#else
// TG1682 v1 & v2 - BCM 53124 Ports
// 53124 doesn't actually have 9 ports but the real ports are spread out across 9 bits so it's 
// easiest to look at it like this
// 53124 Ports
// External Port 1 - Bit 1
// External Port 2 - Bit 2
// External Port 3 - Bit 3
// External Port 4 - Bit 4
// Internal Port 5 - Bit 5 (Unused in TG1682)
// IMP Port        - Bit 8 (CPU port in TG1682)

// TG1682 CR - BCM 53134 Ports
// External Port 1 - Bit 0
// External Port 2 - Bit 1
// External Port 3 - Bit 2
// External Port 4 - Bit 3
// IMP Port        - Bit 8 (CPU port in TG1682)
#define NUM_PORTS                                   (9)     
#define EXTSWT_INTERNAL_PORT                        (8)
#define VALID_PORT_BITMASK_53124                    (0x13e)
#define VALID_PORT_BITMASK_53134                    (0x10f)
#define VALID_PORT_BITMASK                          (dev->valid_port_bitmask)
#define IS_VALID_PORT(port)                         ((1 << port) & VALID_PORT_BITMASK)
#define PHY_PORT_BITMASK_53124                      (0x1e)
#define PHY_PORT_BITMASK_53134                      (0x0f)
#define PHY_PORT_BITMASK                            (dev->phy_port_bitmask)
#define IS_PHY_PORT(port)                           ((1 << port) & PHY_PORT_BITMASK)

#define EXTSWT_RESET_N_GPIO                         (96)
#define EXTSWT_RESET_PULSE_LENGTH_MS                (100)   /* BCM53124 requires 100ms minimum */
#define EXTSWT_RESET_WAIT_LENGTH_MS                 (5)     /* BCM53124 requires 5ms minimum */
#endif

#define GPIO_DIR_OUT                                (0)
#define GPIO_DIR_IN                                 (1)

#define VID_MASK                                    (0xfff)
#endif

/* Device numbers for mknod */
#define EXTSWT_MAJOR 113
#define EXTSWT_MINOR 0

#define SWITCH_DEV_NAME             "/dev/arris_switch"

#define PORT_IS_MEMBER_EGRESS_UNMODIFIED            (0)
#define PORT_IS_MEMBER_EGRESS_UNTAGGED              (1)
#define PORT_IS_MEMBER_EGRESS_TAGGED                (2)
#define PORT_IS_NOT_MEMBER_EGRESS_INGRESS_DISCARD   (3)

#ifndef ETHER_ADDR_LEN
#define ETHER_ADDR_LEN		6
#endif

/* new Ioctl's created - using a value with a base that can be adjusted as per needs */
#define EXTSWT_IOCTL_BASE          0

/* Filtering IOCTL */
enum
{
    EXTSWT_CONFIG_MODEL = EXTSWT_IOCTL_BASE,
    EXTSWT_POWER_DOWN,
    EXTSWT_IS_INITIALIZED,
    EXTSWT_GETPORT_STATUS,
    EXTSWT_GETPORT_STATS,
    EXTSWT_SETPORT_MODE,
    EXTSWT_VLAN_ADD_PORT,
    EXTSWT_VLAN_DELETE_PORT,
    EXTSWT_ADD_PORT_TAG,
    EXTSWT_GET_VLAN_INFO,
    EXTSWT_CLEAR_PORT_STATS,
    EXTSWT_SET_TEST_MODE,
    EXTSWT_READ_REGISTER,
    EXTSWT_WRITE_REGISTER,
    EXTSWT_PORT_ENABLE,
    EXTSWT_PHY_PORT_ENABLE,
    EXTSWT_DUMP_MACS,
    EXTSWT_SEARCH_MAC,
    EXTSWT_ADD_MAC,
    EXTSWT_DEL_MAC,
    //EXTSWT_CERTIFICATION_CONFIG,
    EXTSWT_STP_ENABLE,
    EXTSWT_QINQ_ENABLE,
    EXTSWT_QINQ_PROV,
    EXTSWT_QINQ_CUST,
    EXTSWT_GET_PHY_PORT_ENABLE,
    EXTSWT_GETPORT_IFTABLE_STATS,
    EXTSWT_GETPORT_CLI_STATS,
    EXTSWT_LOOP_DETECT_ENABLE,
    EXTSWT_REG_LINKSTATUS_FD,
    EXTSWT_SET_LEDS_STATE,
    EXTSWT_SET_ETH_EEE_MODE,
    EXTSWT_GET_ETH_EEE_MODE,
    EXTSWT_GET_ETH_EEE_STATS,
    EXTSWT_GET_ETH_EEE_STATS_CLR
};

// Industry standard 0-based port speed (unlike the Arris version)
typedef enum
{
    EXTSWT_PORT_SPEED_10M = 0,
    EXTSWT_PORT_SPEED_100M,
    EXTSWT_PORT_SPEED_1000M,
    EXTSWT_PORT_SPEED_RESERVED
} EXTSWT_PORT_SPEED_t;


typedef enum 
{
    EXTSWT_DUPLEX_HALF = 0,
    EXTSWT_DUPLEX_FULL
} EXTSWT_DUPLEX_t;


typedef struct IfDbStats_Entry
{
    /* common ifXTable, ifTable counters */
    Uint64 ifInOcts64;
    Uint64 ifInUcastPkts64;
    Uint64 ifInMcastPkts64;
    Uint64 ifInBcastPkts64;

    Uint64 ifOutOcts64;
    Uint64 ifOutUcastPkts64;
    Uint64 ifOutMcastPkts64;
    Uint64 ifOutBcastPkts64;

    /* The ifTable specific counters */
    Uint32 ifInDiscards;        /* ifTable[RO] */
    Uint32 ifInErrors;          /* ifTable[RO] */
    Uint32 ifInUnknownProtos;   /* ifTable[RO] */
    Uint32 ifOutDiscards;       /* ifTable[RO] */ 
    Uint32 ifOutErrors;         /* ifTable[RO] */ 
} IfDbStats_Entry_t;


/* generic command with no parameters */
typedef struct
{
    unsigned int                cmd;
} EXTSWT_ioctl_command_t;

/* generic request with no parameters */
typedef struct
{
    unsigned int                req;
    unsigned int                rsp;
} EXTSWT_ioctl_request_t;

typedef struct
{
    unsigned int linkStatus[NUM_PORTS];
    unsigned int autonegotiate[NUM_PORTS];
    unsigned int duplex[NUM_PORTS];
    unsigned int speed[NUM_PORTS];
} EXTSWT_port_status_t;

typedef struct
{
    unsigned int                cmd;
    EXTSWT_port_status_t        ps;
} EXTSWT_ioctl_get_port_status_t;

typedef struct
{
    int     port;
    Uint64  rx_bytes;
    Uint64  tx_bytes;
} EXTSWT_port_stats_t;   

typedef struct
{
    unsigned int                cmd;
    EXTSWT_port_stats_t         ps;
} EXTSWT_ioctl_get_port_stats_t;

typedef struct
{
    int     port;    
    IfDbStats_Entry_t ifTableStats;
} EXTSWT_port_iftable_stats_t;

typedef struct
{
    unsigned int                cmd;
    EXTSWT_port_iftable_stats_t ps;
} EXTSWT_ioctl_get_port_iftable_stats_t;


typedef struct
{

    int                         port[NUM_PORTS];
    unsigned int                port_num;
    unsigned int                enable[NUM_PORTS];
} EXTSWT_eth_eee_mode_t;

typedef struct
{
    unsigned int                cmd;
    EXTSWT_eth_eee_mode_t       emode;
}EXTSWT_ioctl_eth_eee_mode_t;

typedef struct
{
    int                         port[NUM_PORTS];
    unsigned int                port_num;
    Uint16                lpi1000[NUM_PORTS];
    Uint16                lpi100[NUM_PORTS];
    Uint32              low_idle_counter[NUM_PORTS];
    Uint32              low_dura_counter[NUM_PORTS];

}
EXTSWT_eth_eee_stats_t;

typedef struct
{
    unsigned int                cmd;
    EXTSWT_eth_eee_stats_t       stats;
}
EXTSWT_ioctl_eth_eee_stats_t;

typedef struct
{
    unsigned int                port_num;
}
EXTSWT_eth_eee_stats_clr_t;

typedef struct
{
    unsigned int                cmd;
    EXTSWT_eth_eee_stats_clr_t  clr;
}
EXTSWT_ioctl_eth_eee_stats_clr_t;

typedef struct
{
    // Broadcom switch stats
    Uint64  TxOctets;
    Uint32  TxDropPkts;
    Uint32  TxBroadcastPkts;
    Uint32  TxMulticastPkts;
    Uint32  TxUnicastPkts;
    Uint32  TxPausePkts;
    Uint32  TxCollisions;
    Uint32  TxSingleCollisions;
    Uint32  TxMultiCollisions;
    Uint32  TxDeferredTransmit;
    Uint32  TxLateCollision;
    Uint32  TxExcessiveCollision;
    Uint32  TxFrameInDisc;

    Uint64  RxOctets;
    Uint32  RxDropPkts;
    Uint32  RxBroadcastPkts;
    Uint32  RxMulticastPkts;
    Uint32  RxUnicastPkts;
    Uint32  RxPausePkts;
    Uint32  RxUndersizePkts;
    Uint32  RxOversizePkts;
    Uint32  RxJabbers;
    Uint32  RxAlignmentErrors;
    Uint32  RxFCSErrors;
    Uint32  RxFragments;
    Uint32  RxJumboPkt;
    Uint32  RxSymbolError;
    Uint32  RxInRangeErrors;
    Uint32  RxOutOfRangeErrors;
    Uint32  RxDiscard;

    Uint32  EEELowPowerIdleEvent;
    Uint32  EEELowPowerDuration;
} bcm_switch_stats_t;

typedef struct
{
    // TODO!!!!!!!!!!!!!!!!!!!!!!!!!  Populate and implement this for RTK
    // Realteak switch stats
    Uint64  TxOctets;
    Uint32  TxDropPkts;
    Uint32  TxBroadcastPkts;
    Uint32  TxMulticastPkts;
    Uint32  TxUnicastPkts;
    Uint32  TxPausePkts;
    Uint32  TxCollisions;
    Uint32  TxSingleCollisions;
    Uint32  TxMultiCollisions;
    Uint32  TxDeferredTransmit;
    Uint32  TxLateCollision;
    Uint32  TxExcessiveCollision;
    Uint32  TxFrameInDisc;

    Uint64  RxOctets;
    Uint32  RxDropPkts;
    Uint32  RxBroadcastPkts;
    Uint32  RxMulticastPkts;
    Uint32  RxUnicastPkts;
    Uint32  RxPausePkts;
    Uint32  RxUndersizePkts;
    Uint32  RxOversizePkts;
    Uint32  RxJabbers;
    Uint32  RxAlignmentErrors;
    Uint32  RxFCSErrors;
    Uint32  RxFragments;
    Uint32  RxJumboPkt;
    Uint32  RxSymbolError;
    Uint32  RxInRangeErrors;
    Uint32  RxOutOfRangeErrors;
    Uint32  RxDiscard;

    Uint32  EEELowPowerIdleEvent;
    Uint32  EEELowPowerDuration;
} rtk_switch_stats_t;

typedef struct
{
    int     port;    
    union
    {
        bcm_switch_stats_t bcm_stats;
        rtk_switch_stats_t rtk_stats;
    };
} EXTSWT_port_cli_stats_t;

typedef struct
{
    unsigned int                cmd;
    EXTSWT_port_cli_stats_t     ps;
} EXTSWT_ioctl_get_port_cli_stats_t;

typedef struct
{
    int     port;
    Uint32  autonegotiate;
    Uint32  speed;
    Uint32  duplex;
} EXTSWT_port_mode_t;

typedef struct
{
    unsigned int                cmd;
    EXTSWT_port_mode_t          pm;
} EXTSWT_ioctl_set_port_mode_t;


typedef struct
{
    int     port;
    int     vid;
    int     mode;
} EXTSWT_vlan_add_port_t;

typedef struct
{
    unsigned int                cmd;
    EXTSWT_vlan_add_port_t      ap;
} EXTSWT_ioctl_vlan_add_port_t;


typedef struct
{
    int     port;
    int     vid;
//    int     mode;
} EXTSWT_vlan_delete_port_t;

typedef struct
{
    unsigned int                cmd;
    EXTSWT_vlan_delete_port_t   dp;
} EXTSWT_ioctl_vlan_delete_port_t;


typedef struct
{
    int     port;
    int     vid;
} EXTSWT_add_port_tag_t;

typedef struct
{
    unsigned int                cmd;
    EXTSWT_add_port_tag_t       apt;
} EXTSWT_ioctl_add_port_tag_t;


typedef struct
{
    int     vid;
    int     port_mode[NUM_PORTS];
} EXTSWT_vlan_info_t;

typedef struct
{
    unsigned int                cmd;
    EXTSWT_vlan_info_t          vi;
} EXTSWT_ioctl_get_vlan_info_t;


typedef struct
{
    int     port;
    int     mode;
} EXTSWT_set_testmode_t;

typedef struct
{
    unsigned int                cmd;
    EXTSWT_set_testmode_t       tm;
} EXTSWT_ioctl_set_testmode_t;


typedef struct
{
    int     page_phy;
    int     reg;
    int     val;
} EXTSWT_rw_register_t;

typedef struct
{
    unsigned int                cmd;
    EXTSWT_rw_register_t        reg;
} EXTSWT_ioctl_rw_register_t;

typedef struct 
{
    unsigned char octet[ETHER_ADDR_LEN];
}EXTSWT_mac_t;

typedef struct
{
    unsigned int                cmd;
    unsigned int                model;
    EXTSWT_mac_t                mac;
    unsigned int                isAppMode;       // Did the modem boot up in App mode?
    unsigned int                isNewGpio;       // Use the new MDC GPIO scheme?
} EXTSWT_ioctl_config_model_t;

typedef struct
{
    int     port;
    int     enable;
} EXTSWT_port_enable_t;

typedef struct
{
    unsigned int                cmd;
    EXTSWT_port_enable_t        ep;
} EXTSWT_ioctl_port_enable_t;

typedef struct
{
    int     port;
    int     enable;
} EXTSWT_phy_port_enable_t;

typedef struct
{
    unsigned int                cmd;
    EXTSWT_phy_port_enable_t    pea;
} EXTSWT_ioctl_phy_port_enable_t;

typedef struct
{
    unsigned short vid;
    unsigned char macAddress[6];
    unsigned char port;
} EXTSWT_mac_portmap_t;

typedef struct
{
    EXTSWT_mac_portmap_t        *macBuffer;
    unsigned long               macBufferSize;
    unsigned int                macsFound;
} EXTSWT_dump_mac_t;

typedef struct
{
    unsigned int                cmd;
    EXTSWT_dump_mac_t           dm;
} EXTSWT_ioctl_dump_mac_t;

typedef struct
{
    EXTSWT_mac_t  mac;
    int            found;
    int            port;    
} EXTSWT_search_mac_t;

typedef struct
{
    unsigned int                cmd;
    EXTSWT_search_mac_t         sm;
} EXTSWT_ioctl_search_mac_t;

//UNIHAN ADD: START
typedef struct
{
    EXTSWT_mac_t                mac;   // MAC=XX:XX:XX:XX:XX:XX+\0
    int                         port;
} EXTSWT_phy_port_index_t;

typedef struct
{
    unsigned int                cmd;
    EXTSWT_phy_port_index_t     l2phy;
} EXTSWT_ioctl_phy_port_index_t;
//UNIHAN ADD: END

typedef struct
{
    int     port;
    int     is_static;
    EXTSWT_mac_t mac;
} EXTSWT_add_mac_t;

typedef struct
{
    unsigned int                cmd;
    EXTSWT_add_mac_t            am;
} EXTSWT_ioctl_add_mac_t;

typedef struct
{
    int     port;
    EXTSWT_mac_t mac;
} EXTSWT_del_mac_t;

typedef struct
{
    unsigned int                cmd;
    EXTSWT_del_mac_t            dm;
} EXTSWT_ioctl_del_mac_t;

typedef struct
{
    int     enable;
} EXTSWT_stpcfg_t;

typedef struct
{
    unsigned int                cmd;
    EXTSWT_stpcfg_t             stp;
} EXTSWT_ioctl_stpcfg_t;

typedef struct
{
    int                         port;
    int                         tpid;
} EXTSWT_qinq_provider_t;

typedef struct
{
    unsigned int                cmd;
    EXTSWT_qinq_provider_t      prov;
} EXTSWT_ioctl_qinq_provider_t;

typedef struct
{
    int                         port;
} EXTSWT_qinq_customer_t;

typedef struct
{
    unsigned int                cmd;
    EXTSWT_qinq_customer_t      cust;
} EXTSWT_ioctl_qinq_customer_t;

typedef struct
{
    unsigned int                cmd;
    int                         enable;
} EXTSWT_ioctl_loop_detect_enable_t;

typedef struct
{
    int                         fd;
} EXTSWT_reg_linkstatus_fd_t;

typedef struct
{
    unsigned int                cmd;
    EXTSWT_reg_linkstatus_fd_t  reg;
} EXTSWT_ioctl_reg_linkstatus_fd_t;

typedef struct
{
    unsigned int                cmd;
    int                         enable;
} EXTSWT_ioctl_set_leds_t;

#endif // DEFS_EXTSWT_LKM_H


