/*
 *
 * ddc_cpgmac_f_ioctl.h
 * Description:
 * see below
 *
 *
 * Copyright (C) 2008, Texas Instruments, Incorporated
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
 *
 */

/** \file   ddc_cpgmac_f_ioctl.h
    \brief  DDC CPGMAC_F Ioctl header file

    This file provides data structures that are required by the Ioctls. The
    application or the driver can include this file to use the Ioctl's

    To use this file, _titypedefs.h needs to be also included

    @author     Greg Guyotte
 */

#ifndef __CPGMAC_F_IOCTL_H__
#define __CPGMAC_F_IOCTL_H__


/* CPMAC new Ioctl's created - using a value with a base that can be adjusted as per needs */
#define CPMAC_DDA_IOCTL_BASE                0

/* Filtering IOCTL */
#define CPMAC_DDA_PRIV_FILTERING            (CPMAC_DDA_IOCTL_BASE + 1)

/* Read/Write MII */
#define CPMAC_DDA_PRIV_MII_READ             (CPMAC_DDA_IOCTL_BASE + 2)
#define CPMAC_DDA_PRIV_MII_WRITE            (CPMAC_DDA_IOCTL_BASE + 3)

/* Get/Clear Statistics */
#define CPMAC_DDA_PRIV_GET_STATS            (CPMAC_DDA_IOCTL_BASE + 4)
#define CPMAC_DDA_PRIV_CLR_STATS            (CPMAC_DDA_IOCTL_BASE + 5)

/* External Switch configuration */
#define CPMAC_DDA_EXTERNAL_SWITCH           (CPMAC_DDA_IOCTL_BASE + 6)

/* Change the CPMAC mode from tasklet to interrupt mode */
#define CPMAC_DDA_SET_ISR_MODE              (CPMAC_DDA_IOCTL_BASE + 7)

/* Change the CPMAC mode from interrupt to tasklet mode */
#define CPMAC_DDA_SET_TASKLET_MODE           (CPMAC_DDA_IOCTL_BASE + 8)

/* Add RX buffers descriptor  */
#define CPMAC_DDA_ADD_RX_BD                  (CPMAC_DDA_IOCTL_BASE + 9)

/**
 * \brief  CPMAC (DDA) Private Ioctl Structure
 *
 * Private Ioctl commands provided by the CPMAC Linux Driver use this structure
 */
typedef struct {
    unsigned int cmd;       /**< Command */
    void *data;             /**< Data provided with the command - depending upon command */
} CpmacDrvPrivIoctl;

/**
 *  \brief CPMAC Single Multicast Ioctl
 *
 *  - CPMAC_DDC_IOCTL_MULTICAST_ADDR operations
 *  - Add/Del operations for adding/deleting a single multicast address
 */
typedef enum {
    CPMAC_MULTICAST_ADD = 0,    /**< Add a single mcast address to the hardware mcast list */
    CPMAC_MULTICAST_DEL         /**< Delete a single mcast address from the hardware mcast list */
} CpmacSingleMultiOper;

/**
 *  \brief CPMAC All Multicast Ioctl
 *
 *  - CPMAC_DDC_IOCTL_ALL_MULTI operations
 *  - Set/Clear all multicast operation
 */
typedef enum {
    CPMAC_ALL_MULTI_SET = 0,
    CPMAC_ALL_MULTI_CLR
} CpmacAllMultiOper;

/**
 * \brief MII Read/Write PHY register
 *
 * Parameters to read/write a PHY register via MII interface
 */
typedef struct {
    Uint32 phyNum;                  /**< Phy number to be read/written */
    Uint32 regAddr;                 /**< Register to be read/written */
    Uint32 data;                    /**< Data to be read/written */
} CpmacPhyParams;

/**
 * \brief MAC  Address params
 *
 * Parameters for Configuring Mac address
 */
typedef struct {
    Uint32 channel;                 /**< Channel number for addr params */
    String macAddress;              /**< Mac address  */
} CpmacAddressParams;

/**
 * \brief Type 2/3 Addressing
 *
 * Parameters for programming CFIG 2/3 addressing mode
 */
typedef struct {
    Uint32 channel;                 /**< Channel number for filtering params apply */
    String macAddress;              /**< Mac address for filtering */
    Int index;                      /**< Index of filtering list to update */
    Bool valid;                     /**< Entry Valid */
    Int match;                      /**< Entry Matching  */
} CpmacType2_3_AddrFilterParams;

/**
 * \brief CPMAC Hardware Statistics
 *
 *  Statistics counters provided by CPMAC Hardware. The names of the counters in
 *  this structure are of "MIB style" and corrospond directly to the hardware
 *  counters provided by CPMAC.
 */
typedef struct {
    Uint32 ifInGoodFrames;
    Uint32 ifInBroadcasts;
    Uint32 ifInMulticasts;
    Uint32 ifInPauseFrames;
    Uint32 ifInCRCErrors;
    Uint32 ifInAlignCodeErrors;
    Uint32 ifInOversizedFrames;
    Uint32 ifInJabberFrames;
    Uint32 ifInUndersizedFrames;
    Uint32 ifInFragments;
    Uint32 ifInFilteredFrames;
    Uint32 ifInQosFilteredFrames;
    Uint32 ifInOctets;
    Uint32 ifOutGoodFrames;
    Uint32 ifOutBroadcasts;
    Uint32 ifOutMulticasts;
    Uint32 ifOutPauseFrames;
    Uint32 ifDeferredTransmissions;
    Uint32 ifCollisionFrames;
    Uint32 ifSingleCollisionFrames;
    Uint32 ifMultipleCollisionFrames;
    Uint32 ifExcessiveCollisionFrames;
    Uint32 ifLateCollisions;
    Uint32 ifOutUnderrun;
    Uint32 ifCarrierSenseErrors;
    Uint32 ifOutOctets;
    Uint32 if64OctetFrames;
    Uint32 if65To127OctetFrames;
    Uint32 if128To255OctetFrames;
    Uint32 if256To511OctetFrames;
    Uint32 if512To1023OctetFrames;
    Uint32 if1024ToUPOctetFrames;
    Uint32 ifNetOctets;
    Uint32 ifRxSofOverruns;
    Uint32 ifRxMofOverruns;
    Uint32 ifRxDMAOverruns;
} CpmacHwStatistics;

typedef struct {
    Uint32 ethAlignmentErrors;
    Uint32 ethFCSErrors;
    Uint32 ethSingleCollisions;
    Uint32 ethMultipleCollisions;
    Uint32 ethSQETestErrors;
    Uint32 ethDeferredTxFrames;
    Uint32 ethLateCollisions;
    Uint32 ethExcessiveCollisions;
    Uint32 ethInternalMacTxErrors;
    Uint32 ethCarrierSenseErrors;
    Uint32 ethTooLongRxFrames;
    Uint32 ethInternalMacRxErrors;
    Uint32 ethSymbolErrors;

} SWITCH_COUNTERS, *PSWITCH_COUNTERS;

/**************************************************************************/
/*                 MIB-2 byte/packet Counter Types                        */
/**************************************************************************/
typedef struct {
    Uint32 InBytes;
    Uint32 InBytesHigh;
    Uint32 InUnicastPkts;
    Uint32 InUnicastPktsHigh;
    Uint32 InMulticastPkts;
    Uint32 InMulticastPktsHigh;
    Uint32 InBroadcastPkts;
    Uint32 InBroadcastPktsHigh;
    Uint32 InDiscardPkts;
    Uint32 InErrorPkts;
    Uint32 InUnknownProtPkts;
    Uint32 OutBytes;
    Uint32 OutBytesHigh;
    Uint32 OutUnicastPkts;
    Uint32 OutUnicastPktsHigh;
    Uint32 OutMulticastPkts;
    Uint32 OutMulticastPktsHigh;
    Uint32 OutBroadcastPkts;
    Uint32 OutBroadcastPktsHigh;
    Uint32 OutDiscardPkts;
    Uint32 OutErrorPkts;
    SWITCH_COUNTERS swCounters;
} MIB2_COUNTER, *PMIB2_COUNTER;

/* Ingress VLAN behaviour */
typedef enum {
    /* VLAN check is on (VLAN must exist). Ingress check is on (ingress port must be part of the VLAN).
       If not both drop the packet
     */
    INGRESS_VLN_MODE_8021Q_SECURE,

    /* VLAN check is on (VLAN must exist). If not drop the packet */
    INGRESS_VLN_MODE_8021Q_CHECK,

    /* Use Base Port VLAN only */
    INGRESS_VLN_MODE_8021Q_DISABLE
} INGRESS_VLN_MODE;

/* Extended features - unique for each switch model */
typedef struct switch_special_caps {
    /* VLAN tunneling on/off is the ability to forward packets to the dest port according to the address table
       even if this port is not part of the VLAN (if off the packet is dropped)
     */

    Bool VlanTunnel;

    INGRESS_VLN_MODE IngressMode;

    /* The ability to add the port Id to the packet itself, thus allowing the CPU to recognize source port */

    Bool PortRecognitionOn;

    /* The ability to send the frame to the given port */

    Bool TxOnFixedPort;

    /* ...
       ... */
} SWITCH_SPECIAL_CAPS, *PSWITCH_SPECIAL_CAPS;

/* Switch Identification */
typedef struct {
    Uint32 Model;
    Uint32 maxNumberOfPorts;
    Uint32 maxNumberOfVLANs;
    Uint32 maxNumberOfPriorities;       /* port based */
    Uint32 is802_1_Q_capable;
    Uint32 is802_1_P_capable;
    /* ...
       ... */
} SW_VER_INFO, *PSW_VER_INFO;

/* VLAN Tagging */
typedef enum
{
    VLN_PORT_UNTAG,
    VLN_PORT_TAG,
    VLN_PORT_UNMODIFIED,
    VLN_PORT_HYBRID,
}
VLN_TAG;

/* ARRIS ADD BEGIN : Single ioctl for port status */
typedef struct  {
    Uint32          LinkStatus;
    Uint32          AutoNegotiate;
    Uint32          Duplex;
    Uint32          Speed;
} SW_PORT_STATUS, *PSW_PORT_STATUS;
/* ARRIS ADD END */

/***********************************************************************************************
            PORT MANAGEMENT
***********************************************************************************************/
typedef enum {
    PORT_LINK_UP = 1,
    PORT_LINK_DOWN,
} PORT_LINK;

/*
   Duplex status descriptor
 */

typedef enum {
    PORT_DUPLEX_HALF = 1,
    PORT_DUPLEX_FULL
} PORT_DUPLEX;

/*
   Speed status descriptor
 */

typedef enum {
    PORT_SPEED_10 = 1,
    PORT_SPEED_100,
    PORT_SPEED_1000
} PORT_SPEED;

/*
   Flow Control status descriptor
 */

typedef enum {
    /* autorecognition of the flow control (if exists) */
    PORT_FC_AUTO = 1,

    /* flow control on */
    PORT_FC_ON,

    /* flow control off */
    PORT_FC_OFF
} PORT_FC;

/*
   AutoNegotiation for duplex and speed status descriptor
 */

typedef enum {
    PORT_AUTO_YES = 1,
    PORT_AUTO_NO,
    PORT_AUTO_RESTART
} PORT_AUTO;

/* Port status */
typedef enum {
    /* Port Disabled */
    PORT_DISABLE = 1,

    /* Special frames only (BPDU e.t.c.) */
    PORT_BLOCK,

    /* Packets are dropped. Macs are learned */
    PORT_LEARNING,

    /* Forwarding - Normal switch mode */
    PORT_FORWARDING
} PORT_STATUS;

typedef enum
{
    SWITCH_GETVERSION = 1,
    SWITCH_SPECCAPSET,
    SWITCH_SPECCAPGET,
    SWITCH_TRAILERSET,
    PORT_STATUSSET,
    PORT_STATUSGET,
    PORT_LINKGET,
    PORT_FCSET,
    PORT_FCGET,
    PORT_DUPLEXSET,
    PORT_DUPLEXGET,
    PORT_SPEEDSET,
    PORT_SPEEDGET,
    PORT_AUTOSET,
    PORT_AUTOGET,
    PORT_STATISTICGET,
    PORT_STATISTICRESET,
    VLAN_CREATE,
    VLAN_DELETE,
    VLAN_PORT_ADD,
    VLAN_PORT_DELETE,
    VLAN_SET_DEF,
    VLAN_PORT_UPDATE,
    VLAN_SET_PORT_PRI,
    VLAN_GET_PORT_PRI,
    DELETE_MAC_ADDR,
    ARRIS_PORT_STATUS_GET,
    ARRIS_PORT_STATS_GET,
    ARRIS_SET_CERT_CONFIG,
    ARRIS_BOUNCE_LINK,
}SwitchIoctlOpcode;

typedef struct _switchIoctlCmdType_
{
    SwitchIoctlOpcode t_opcode;
    union
    {
        /* ARRIS ADD BEGIN : Get all the port information in one ioctl */
        struct
        {
            Uint32          PortNum;
            SW_PORT_STATUS  PortStatus;
        }
        PORT_status;

        struct
        {
            Uint32          PortNum;
            Uint32          Status;
        }
        PORT_config;

        struct
        {
            Uint32          PortNum;
            Uint64          TxBytes;
            Uint64          RxBytes;
        }
        PORT_stats;

        /* ARRIS MOD END */

        struct
        {
            Uint32          PortNum;
            MIB2_COUNTER    Counters;
        }
        PORT_counters;

        struct
        {
            Uint32                  data_size;
            SWITCH_SPECIAL_CAPS     SpecialCap;
        }
        SWITCH_specialcap;

        struct
        {
            SW_VER_INFO     verinfo;
        }
        SWITCH_verinfo;

        struct
        {
            Uint32          PortNum;
            Uint32          vlnId;
            VLN_TAG         tag;
            Uint32          priority;
        }
        VLAN_info;

        struct
        {
            char    MacAddr[6];
        }
        MAC_addr;

   }msg;

}
SwitchIoctlType;

#endif /* __CPGMAC_F_IOCTL_H__ */
