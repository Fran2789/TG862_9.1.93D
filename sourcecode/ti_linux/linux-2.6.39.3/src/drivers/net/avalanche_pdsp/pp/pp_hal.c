/*
  GPL LICENSE SUMMARY

  Copyright(c) 2014 Intel Corporation.

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


#include <linux/kernel.h>
#include <linux/list.h>
#include <string.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/if_ether.h>
#include <linux/if_vlan.h>
#include <io.h>
#include "pp_db.h"
#include "pp_hal.h"
#include <mach/puma.h>
#include <asm-arm/arch-avalanche/puma6/puma6_cppi.h>
#include "pal.h"

#define SIZE_IN_WORD(p) ((sizeof(p) + 0x3) >> 2)

#define PP_HAL_MTA_MAC_BASE_PHY                     (0x034000F0)

/* PP Base PHY addresses */
#define PP_HAL_HOST_SESSION_INFO_BASE_PHY           (0x0320800C)
#define PP_HAL_HOST_SESSION_INFO_ENTRY_SIZE_PHY     (0x03208010)
#define PP_HAL_HOST_SESSION_INFO_LUT2_OFF_PHY       (0x03208014)
#define PP_HAL_SESSION_INFO_BASE_PHY                (0x03300000)
#define PP_HAL_MULTICAST_INFO_BASE_PHY              (0x03328700)
#define PP_HAL_LARGE_NEW_HEADERS_BASE_PHY           (0x0332B000)
#define PP_HAL_PID_INFO_BASE_PHY                    (0x0332BE00)
#define PP_HAL_VPID_FLAGS_BASE_PHY                  (0x0332C000)
#define PP_HAL_PORT_2_PID_BASE_PHY                  (0x0332C040)
#define PP_HAL_QOS_QCFG_BLK_BASE_PHY                (0x03400300)
#define PP_HAL_QOS_CLST_BLK_BASE_PHY                (0x03400B00)


/* Base Session */
#define TI_PP_SES_FLAG_IDLE_TMOUT           (1 << 0)
#define TI_PP_SES_FLAG_TCP_CONTROL          (1 << 1)
#define TI_PP_SES_FLAG_NO_INGRESS_STATS     (1 << 2)
#define TI_PP_SES_FLAG_NO_EGRESS_STATS      (1 << 3)
#define TI_PP_SES_FLAG_XLUDE_ETH_HDR_STATS  (1 << 4)
#define TI_PP_SES_FLAG_UPDATE_TTL           (1 << 5)
#define TI_PP_SES_FLAG_PROC_TTL_EXP         (1 << 6)
#define TI_PP_SES_FLAG_DO_REASSEMBLY        (1 << 7)
#define TI_PP_SES_FLAG_PROC_IP_OPTS         (1 << 8)

#define TI_PP_SES_FLAG_IPV6_CLASS_MASK      (3 << 8)
#define TI_PP_SES_FLAG_IPV6_CLASS_SET(x)    (((x) <<  8) & TI_PP_SES_FLAG_IPV6_CLASS_MASK)

#define TI_PP_SES_TUNNEL                    (1 << 10)
#define TI_PP_SES_TUNNEL_DS                 (0 << 11)
#define TI_PP_SES_TUNNEL_US                 (1 << 11)
#define TI_PP_SES_TUNNEL_GRE                (0 << 12)
#define TI_PP_SES_TUNNEL_DS_LITE            (1 << 12)
#define TI_PP_SES_TUNNEL_RES2               (2 << 12)
#define TI_PP_SES_TUNNEL_RES3               (3 << 12)

#define TI_PP_SES_FLAG_TUNNEL_MASK          (0xF << 10)
#define TI_PP_SES_FLAG_GRE_US               (TI_PP_SES_TUNNEL | TI_PP_SES_TUNNEL_US | TI_PP_SES_TUNNEL_GRE)
#define TI_PP_SES_FLAG_GRE_DS               (TI_PP_SES_TUNNEL | TI_PP_SES_TUNNEL_DS | TI_PP_SES_TUNNEL_GRE)
#define TI_PP_SES_FLAG_DS_LITE_US           (TI_PP_SES_TUNNEL | TI_PP_SES_TUNNEL_US | TI_PP_SES_TUNNEL_DS_LITE)
#define TI_PP_SES_FLAG_DS_LITE_DS           (TI_PP_SES_TUNNEL | TI_PP_SES_TUNNEL_DS | TI_PP_SES_TUNNEL_DS_LITE)

#define IS_GRE_DS(flags)        (((flags) & TI_PP_SES_FLAG_TUNNEL_MASK) == TI_PP_SES_FLAG_GRE_DS)
#define IS_GRE_US(flags)        (((flags) & TI_PP_SES_FLAG_TUNNEL_MASK) == TI_PP_SES_FLAG_GRE_US)
#define IS_DS_LITE_US(flags)    (((flags) & TI_PP_SES_FLAG_TUNNEL_MASK) == TI_PP_SES_FLAG_DS_LITE_US)
#define IS_DS_LITE_DS(flags)    (((flags) & TI_PP_SES_FLAG_TUNNEL_MASK) == TI_PP_SES_FLAG_DS_LITE_DS)


#define TI_PP_SES_FLAG_DSLITE_US_FRAG_IPv4  (1 << 14)


/* Egress Framing Record */
#define TI_PP_EGR_FRM_STRIP_L2          (1<<0)
#define TI_PP_EGR_FRM_PATCH_802_3       (1<<1)
#define TI_PP_EGR_FRM_TURBODOX_EN       (1<<2)
#define TI_PP_EGR_FRM_PPPOE_HDR         (1<<4)
#define TI_PP_EGR_FRM_REFRAME_IP        (1<<5)
#define TI_PP_EGR_FRM_TURBODOX_ADV_EN   (1<<7)

/* Egress Flags */
#define TI_PP_EGR_FLAG_NEW_HEADER_HAS_IPV4  (1 << 2)
#define TI_PP_EGR_FLAG_NEW_HEADER_HAS_IPV6  (1 << 3)
#define TI_PP_EGR_FLAG_NEW_HEADER_PTR       (1 << 4)
#define TI_PP_EGR_FLAG_NEW_HEADER_INTERNAL  (1 << 5)
#define TI_PP_EGR_FLAG_MOD_REC_VALID        (1 << 6)

/* Valid flags in Modification Record */
#define TI_PP_MOD_IPSRC_VALID       (1<<0)
#define TI_PP_MOD_IPDST_VALID       (1<<1)
#define TI_PP_MOD_IPADR_VALID       (1<<2)
#define TI_PP_MOD_L3CHK_VALID       (1<<3)
#define TI_PP_MOD_SRCPORT_VALID     (1<<4)
#define TI_PP_MOD_DSTPORT_VALID     (1<<5)
#define TI_PP_MOD_PORTS_VALID       (1<<6)
#define TI_PP_MOD_L4CHK_VALID       (1<<7)
#define TI_PP_MOD_IPTOS_VALID       (1<<8)



typedef struct
{
    Uint16      cmd_param;
    Uint8       cmd_minor;
    Uint8       cmd;
}
pp_hal_cmd_t;

typedef union
{
    pp_hal_cmd_t    pp_cmd;
    pdsp_cmd_t      pdsp_cmd;
}
PP_HAL_CMD_u;

/* PP PDSP commands */
typedef enum
{
    PP_HAL_PDSP_CMD_OPEN    =   0x80,
    PP_HAL_PDSP_CMD_FLUSH_ALL,
    PP_HAL_PDSP_CMD_FLUSH_MANY,
    PP_HAL_PDSP_CMD_reserved_x83,
    PP_HAL_PDSP_CMD_PID,
    PP_HAL_PDSP_CMD_VPID,
    PP_HAL_PDSP_CMD_SESSION,
    PP_HAL_PDSP_CMD_STATUS,
    PP_HAL_PDSP_CMD_PSM,
    PP_HAL_PDSP_CMD_VERSION,
    PP_HAL_PDSP_CMD_reserved_x8A,
    PP_HAL_PDSP_CMD_reserved_x8B,
    PP_HAL_PDSP_CMD_ACK_SUPPRESS,

    PP_HAL_PDSP_CMD_QOS_CLUSTER = 0xA0,

}
PP_HAL_PDSP_CMD_e;

typedef enum
{
    PP_HAL_PDSP_MINOR_NONE,
    PP_HAL_PDSP_MINOR_ADD,
    PP_HAL_PDSP_MINOR_DEL,
    PP_HAL_PDSP_MINOR_CHG,
    PP_HAL_PDSP_MINOR_RANGE_ADD,
    PP_HAL_PDSP_MINOR_RANGE_DEL,

    PP_HAL_PDSP_MINOR_DISABLE   = 0,
    PP_HAL_PDSP_MINOR_ENABLE    = 1,
}
PP_HAL_PDSP_CMD_MINOR_e;


static inline Uint32 __pp_hal_xcsum_u32(Uint32 csum_delta, Uint32 old_word, Uint32 new_word)
{
    csum_delta += (old_word & 0xFFFF) + ((old_word >> 16) & 0xFFFF);
    csum_delta -= (new_word & 0xFFFF) + ((new_word >> 16) & 0xFFFF);

    //CHK: Doesn't work when old_word < new_word csum_delta  = (csum_delta &
    //0xFFFF) + ((csum_delta >> 16) & 0xFFFF);
    return (csum_delta);
}

static inline Uint32 __pp_hal_xcsum_u16 (Uint32 csum_delta, Uint16 old_hword, Uint16 new_hword)
{
    csum_delta += old_hword - new_hword;
    csum_delta  = (csum_delta & 0xFFFF) + ((csum_delta >> 16) & 0xFFFF);
    return (csum_delta);
}


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
typedef struct
{
    Uint8   PidFlags;
    Uint8   DefaultPriority;
    Uint16  DefaultTxQueueHost;

    Uint16  DefaultTxDestTag;
    Uint16  DefaultTxQueueInfra;

    Uint16  HostResourcesThreshold;
    Uint16  HostResourcesCounter;
}
pp_hal_pid_info_t;

/**************************************************************************
 * FUNCTION NAME : AVALANCHE_PP_RET_e pp_hal_pid_create ( AVALANCHE_PP_PID_t * ptr_pid )
 **************************************************************************
 * DESCRIPTION   :
 *  The function uses the information passed to create a PID in the PDSP.
 *  param[in] ptr_pid - pointer to pid information
 * RETURNS       :
 *      0  -   Success
 *      >0  -   Error
 **************************************************************************/
AVALANCHE_PP_RET_e      pp_hal_pid_create( AVALANCHE_PP_PID_t * ptr_pid )
{
    pp_hal_pid_info_t       pd;
    PP_HAL_CMD_u            cmd_word;
    Int32                   rc;
        
    cmd_word.pp_cmd.cmd            = PP_HAL_PDSP_CMD_PID;
    cmd_word.pp_cmd.cmd_minor      = PP_HAL_PDSP_MINOR_ADD;
    cmd_word.pp_cmd.cmd_param      = ptr_pid->pid_handle;

    pd.PidFlags                 = 0;
    pd.DefaultPriority          = ptr_pid->dflt_pri_drp;
    pd.DefaultTxQueueHost       = 0; // Not used yet
    pd.DefaultTxDestTag         = ptr_pid->dflt_dst_tag;
    pd.DefaultTxQueueInfra      = ptr_pid->dflt_fwd_q;
    pd.HostResourcesThreshold   = 0; // Not used yet
    pd.HostResourcesCounter     = 0;

    rc = pdsp_cmd_send( PDSP_ID_Classifier, 
                        cmd_word.pdsp_cmd, 
                        &pd,   SIZE_IN_WORD(pd),
                        NULL,   0 );

    if (rc)
    {
        return (rc + PP_RC_FAILURE);
    }

    return (PP_RC_SUCCESS);
}
/**************************************************************************
 * FUNCTION NAME : AVALANCHE_PP_RET_e pp_hal_pid_delete ( Uint8 pid_handle )
 **************************************************************************
 * DESCRIPTION   :
 *  The function deletes the PID in the PDSP.
 *  param[in] pid_handle - handle of pid to delete
 * RETURNS       :
 *      0  -   Success
 *      >0  -   Error
 **************************************************************************/
AVALANCHE_PP_RET_e      pp_hal_pid_delete( Uint8 pid_handle )
{
    PP_HAL_CMD_u            cmd_word;
    Int32                   rc;
        
    cmd_word.pp_cmd.cmd            = PP_HAL_PDSP_CMD_PID;
    cmd_word.pp_cmd.cmd_minor      = PP_HAL_PDSP_MINOR_DEL;
    cmd_word.pp_cmd.cmd_param      = (Uint16) pid_handle;

    rc = pdsp_cmd_send( PDSP_ID_Classifier, 
                        cmd_word.pdsp_cmd, 
                        NULL,   0,
                        NULL,   0 );

    if (rc)
    {
        return (rc + PP_RC_FAILURE);
    }

    return (PP_RC_SUCCESS);
}
/**************************************************************************
 * FUNCTION NAME : AVALANCHE_PP_RET_e pp_hal_pid_range_create ( AVALANCHE_PP_PID_RANGE_t * ptr_pid_range_cfg )
 **************************************************************************
 * DESCRIPTION   :
 *  The function uses the information passed to config PID range in the PDSP.
 *  param[in] ptr_pid_range_cfg - pointer to pid_range structure
 * RETURNS       :
 *      0  -   Success
 *      >0  -   Error
 **************************************************************************/
AVALANCHE_PP_RET_e      pp_hal_pid_range_create( AVALANCHE_PP_PID_RANGE_t * ptr_pid_range_cfg )
{
    PP_HAL_CMD_u            cmd_word;
    Int32                   rc;
        
    cmd_word.pp_cmd.cmd            = PP_HAL_PDSP_CMD_PID;
    cmd_word.pp_cmd.cmd_minor      = PP_HAL_PDSP_MINOR_RANGE_ADD;
    cmd_word.pp_cmd.cmd_param      = 0;

    rc = pdsp_cmd_send( PDSP_ID_Classifier, 
                        cmd_word.pdsp_cmd, 
                        ptr_pid_range_cfg,   SIZE_IN_WORD(*ptr_pid_range_cfg),
                        NULL,   0 );

    if (rc)
    {
        return (rc + PP_RC_FAILURE);
    }

    return (PP_RC_SUCCESS);
}
/**************************************************************************
 * FUNCTION NAME : AVALANCHE_PP_RET_e pp_hal_pid_range_delete ( Uint8 port )
 **************************************************************************
 * DESCRIPTION   :
 *  The function uses the information passed to remove PID range in the PDSP.
 *  param[in] port - number of port to remove
 * RETURNS       :
 *      0  -   Success
 *      >0  -   Error
 **************************************************************************/
AVALANCHE_PP_RET_e      pp_hal_pid_range_delete( Uint8 port )
{
    PP_HAL_CMD_u            cmd_word;
    Int32                   rc;
        
    cmd_word.pp_cmd.cmd            = PP_HAL_PDSP_CMD_PID;
    cmd_word.pp_cmd.cmd_minor      = PP_HAL_PDSP_MINOR_RANGE_DEL;
    cmd_word.pp_cmd.cmd_param      = (Uint16) port;

    rc = pdsp_cmd_send( PDSP_ID_Classifier, 
                        cmd_word.pdsp_cmd, 
                        NULL,   0,
                        NULL,   0 );

    if (rc)
    {
        return (rc + PP_RC_FAILURE);
    }

    return (PP_RC_SUCCESS);
}
/**************************************************************************
 * FUNCTION NAME : AVALANCHE_PP_RET_e pp_hal_pid_flags_set ( AVALANCHE_PP_PID_t * ptr_pid )
 **************************************************************************
 * DESCRIPTION   :
 *  The function uses the information passed to modify the PID flags in the PDSP.
 *  param[in] ptr_pid - pointer tp PID structure
 * RETURNS       :
 *      0  -   Success
 *      >0  -   Error
 **************************************************************************/
AVALANCHE_PP_RET_e      pp_hal_pid_flags_set( AVALANCHE_PP_PID_t * ptr_pid )
{
    PP_HAL_CMD_u            cmd_word;
    Uint32                  flags = (Uint32) ptr_pid->priv_flags;
    Int32                   rc;
        
    cmd_word.pp_cmd.cmd            = PP_HAL_PDSP_CMD_PID;
    cmd_word.pp_cmd.cmd_minor      = PP_HAL_PDSP_MINOR_CHG;
    cmd_word.pp_cmd.cmd_param      = ptr_pid->pid_handle;

    rc = pdsp_cmd_send( PDSP_ID_Classifier, 
                        cmd_word.pdsp_cmd, 
                        &flags,   SIZE_IN_WORD(flags),
                        NULL,   0 );

    if (rc)
    {
        return (rc + PP_RC_FAILURE);
    }

    return (PP_RC_SUCCESS);
}

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
 * FUNCTION NAME : AVALANCHE_PP_RET_e pp_hal_vpid_create ( AVALANCHE_PP_VPID_INFO_t * ptr_vpid )
 **************************************************************************
 * DESCRIPTION   :
 *  The function uses the information passed to create a VPID in the PDSP.
 *  param[in] ptr_vpid - pointer to VPIT information
 * RETURNS       :
 *      0  -   Success
 *      >0  -   Error
 **************************************************************************/
AVALANCHE_PP_RET_e      pp_hal_vpid_create( AVALANCHE_PP_VPID_INFO_t * ptr_vpid )
{
    PP_HAL_CMD_u            cmd_word;
    Uint32                  flags = 0;
    Int32                   rc;
        
    cmd_word.pp_cmd.cmd            = PP_HAL_PDSP_CMD_VPID;
    cmd_word.pp_cmd.cmd_minor      = PP_HAL_PDSP_MINOR_ADD;
    cmd_word.pp_cmd.cmd_param      = ptr_vpid->vpid_handle;

    rc = pdsp_cmd_send( PDSP_ID_Classifier, 
                        cmd_word.pdsp_cmd, 
                        &flags,   SIZE_IN_WORD(flags),
                        NULL,   0 );

    if (rc)
    {
        return (rc + PP_RC_FAILURE);
    }

    return (PP_RC_SUCCESS);
}
/**************************************************************************
 * FUNCTION NAME : AVALANCHE_PP_RET_e pp_hal_vpid_delete ( Uint8 vpid_handle )
 **************************************************************************
 * DESCRIPTION   :
 *  The function deletes the VPID in the PDSP.
 *  param[in] vpid_handle - handle of VPID to delete 
 * RETURNS       :
 *      0  -   Success
 *      >0  -   Error
 **************************************************************************/
AVALANCHE_PP_RET_e      pp_hal_vpid_delete( Uint8 vpid_handle )
{
    PP_HAL_CMD_u            cmd_word;
    Int32                   rc;
        
    cmd_word.pp_cmd.cmd            = PP_HAL_PDSP_CMD_VPID;
    cmd_word.pp_cmd.cmd_minor      = PP_HAL_PDSP_MINOR_DEL;
    cmd_word.pp_cmd.cmd_param      = (Uint16) vpid_handle;

    rc = pdsp_cmd_send( PDSP_ID_Classifier, 
                        cmd_word.pdsp_cmd, 
                        NULL,   0,
                        NULL,   0 );

    if (rc)
    {
        return (rc + PP_RC_FAILURE);
    }

    return (PP_RC_SUCCESS);
}
/**************************************************************************
 * FUNCTION NAME : AVALANCHE_PP_RET_e pp_hal_vpid_flags_set ( AVALANCHE_PP_VPID_INFO_t * ptr_vpid )
 **************************************************************************
 * DESCRIPTION   :
 *  The function uses the information passed to modify the VPID flags in the PDSP.
 *  param[in] ptr_vpid - pointer to VPID info structure
 * RETURNS       :
 *      0  -   Success
 *      >0  -   Error
 **************************************************************************/
AVALANCHE_PP_RET_e      pp_hal_vpid_flags_set( AVALANCHE_PP_VPID_INFO_t * ptr_vpid )
{
    PP_HAL_CMD_u            cmd_word;
    Uint32                  flags = (Uint32) ptr_vpid->flags;
    Int32                   rc;
        
    cmd_word.pp_cmd.cmd            = PP_HAL_PDSP_CMD_VPID;
    cmd_word.pp_cmd.cmd_minor      = PP_HAL_PDSP_MINOR_CHG;
    cmd_word.pp_cmd.cmd_param      = ptr_vpid->vpid_handle;

    rc = pdsp_cmd_send( PDSP_ID_Classifier, 
                        cmd_word.pdsp_cmd, 
                        &flags,   SIZE_IN_WORD(flags),
                        NULL,   0 );

    if (rc)
    {
        return (rc + PP_RC_FAILURE);
    }

    return (PP_RC_SUCCESS);
}


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

#define SESSION_ADD_SYNC                (0 << 12) /* bit 12, bit 13 are not set */
#define SESSION_REMOVE                  (1 << 12)
#define SESSION_CHANGE                  (2 << 12)
#define SESSION_LUT1_INDEX_VALID        (1 << 14)

/* --------------------------------------------- */
typedef union pp_hal_block_mcast
{
    union pp_hal_block_mcast *              next;
    PP_HAL_SESSION_INFO_MULTICAST_t         info;
} 
PP_HAL_BLOCK_MCAST_t;
/* --------------------------------------------- */

/* --------------------------------------------- */
typedef union pp_hal_block_extended_new_hdr
{
    union pp_hal_block_extended_new_hdr *   next;
    Uint8                                   NewHeader[128];
}
PP_HAL_BLOCK_EXTENDED_NEW_HDR_t;
/* --------------------------------------------- */


static volatile PP_HAL_BLOCK_MCAST_t *              gPpHalFreeList_BlockMulticast = NULL;
static volatile PP_HAL_BLOCK_EXTENDED_NEW_HDR_t *   gPpHalFreeList_BlockExtHdr    = NULL;
static volatile PP_HAL_SESSION_INFO_t *             gPpHalSessionInfo             = NULL;
volatile Uint32 gPpHalDsLiteUsFragIPv4 = 0;   // Flag that indicates if to fragment IPv4(!=0) or IPv6(0) in case of US DS-Lite session (and packet size > MTU)




static AVALANCHE_PP_RET_e   __pp_hal_init_pool_mcast( void )
{
    PP_HAL_BLOCK_MCAST_t *  pBlkStart;
    PP_HAL_BLOCK_MCAST_t *  pBlkEnd;

    gPpHalFreeList_BlockMulticast = (PP_HAL_BLOCK_MCAST_t *)IO_PHY2VIRT( PP_HAL_MULTICAST_INFO_BASE_PHY );
    pBlkStart   = gPpHalFreeList_BlockMulticast;
    pBlkEnd     = gPpHalFreeList_BlockMulticast + 156;

    for(; pBlkStart  < pBlkEnd; pBlkStart++)
    {
        pBlkStart->next = pBlkStart + 1;
    }

    pBlkStart->next = NULL;

    return( PP_RC_SUCCESS );
}

static inline PP_HAL_BLOCK_MCAST_t *    __pp_hal_block_mcast_alloc( void )
{
    PP_HAL_BLOCK_MCAST_t * pBlk;
    Uint32 cookie;

    local_irq_save( cookie );

    pBlk = gPpHalFreeList_BlockMulticast;

    if (pBlk)
    {
        gPpHalFreeList_BlockMulticast = pBlk->next;
    }

    local_irq_restore( cookie );

    return (pBlk);
}

static inline void __pp_hal_block_mcast_free( PP_HAL_BLOCK_MCAST_t *  pBlk )
{
    Uint32 cookie;

    local_irq_save( cookie );

    pBlk->next = gPpHalFreeList_BlockMulticast;
    gPpHalFreeList_BlockMulticast = pBlk;

    local_irq_restore( cookie );
}

static AVALANCHE_PP_RET_e   __pp_hal_init_pool_ext_headers( void )
{
    PP_HAL_BLOCK_EXTENDED_NEW_HDR_t *pBlkStart;
    PP_HAL_BLOCK_EXTENDED_NEW_HDR_t *pBlkEnd;

    gPpHalFreeList_BlockExtHdr = (PP_HAL_BLOCK_EXTENDED_NEW_HDR_t *)IO_PHY2VIRT( PP_HAL_LARGE_NEW_HEADERS_BASE_PHY );
    pBlkStart   = gPpHalFreeList_BlockExtHdr;
    pBlkEnd     = gPpHalFreeList_BlockExtHdr + 16;

    for(; pBlkStart  < pBlkEnd; pBlkStart++)
    {
        pBlkStart->next = pBlkStart + 1;
    }

    pBlkStart->next = 0;

    return( PP_RC_SUCCESS );
}

static inline PP_HAL_BLOCK_EXTENDED_NEW_HDR_t*  __pp_hal_block_ext_hdr_alloc( void )
{
    PP_HAL_BLOCK_EXTENDED_NEW_HDR_t *pBlk;
    Uint32 cookie;

    local_irq_save( cookie );

    pBlk = gPpHalFreeList_BlockExtHdr;

    if (pBlk)
    {
        gPpHalFreeList_BlockExtHdr = pBlk->next;
    }

    local_irq_restore( cookie );

    return (pBlk);
}

static inline void __pp_hal_block_ext_hdr_free( PP_HAL_BLOCK_EXTENDED_NEW_HDR_t *    pBlk )
{
    Uint32 cookie;

    local_irq_save( cookie );

    pBlk->next = gPpHalFreeList_BlockExtHdr;
    gPpHalFreeList_BlockExtHdr = pBlk;

    local_irq_restore( cookie );
}


static AVALANCHE_PP_RET_e   __pp_hal_SetModificationRecord( AVALANCHE_PP_SESSION_INFO_t * session_cfg , PP_HAL_SESSION_INFO_t *sesFwInfo )
{
    PP_HAL_MODIFICATION_REC_t  *modRec = &sesFwInfo->ModificationRecord;
    /* ARRIS propagate INTEL MR24372 fix */
    if ( ( session_cfg->ingress.lookup.LUT1.u.fields.L3.entry_type == AVALANCHE_PP_LUT_ENTRY_L3_DSLITE ) ||
    		( session_cfg->egress.tunnel_type == AVALANCHE_PP_LUT_ENTRY_L3_DSLITE) )
    {
        /* For DS-Lite sessions there is no modification record */
        /* ARRIS propagate INTEL MR24372 fix end */
        return (PP_RC_SUCCESS);
    }

    if ( session_cfg->egress.l3_packet_type == AVALANCHE_PP_LUT_ENTRY_L3_IPV6 )
    {
        /* For IPv6 sessions there is no modification record */
        return (PP_RC_SUCCESS);
    }

    if ( session_cfg->egress.l3_packet_type == AVALANCHE_PP_LUT_ENTRY_L3_IPV4 )
    {
        Uint32  l3_xcsum = 0;
        Uint16  l4_xcsum = 0;

        l3_xcsum = 0;
        
        modRec->ModFlags |= TI_PP_MOD_IPDST_VALID | TI_PP_MOD_L3CHK_VALID | TI_PP_MOD_IPSRC_VALID | TI_PP_MOD_IPTOS_VALID;

        if ( session_cfg->ingress.dev_type == AVALANCHE_PP_DEV_TYPE_LAN )
        {
            l3_xcsum = __pp_hal_xcsum_u32( l3_xcsum, 
                                      session_cfg->ingress.lookup.LUT2.u.fields.WAN_addr_IP.v4,    
                                      session_cfg->egress.DST_IP.v4 );
            l3_xcsum = __pp_hal_xcsum_u32( l3_xcsum, 
                                      session_cfg->ingress.lookup.LUT1.u.fields.L3.LAN_addr_IP.v4,
                                      session_cfg->egress.SRC_IP.v4 );
        }
        else
        {
            l3_xcsum = __pp_hal_xcsum_u32( l3_xcsum, 
                                      session_cfg->ingress.lookup.LUT1.u.fields.L3.LAN_addr_IP.v4, 
                                      session_cfg->egress.DST_IP.v4 );
            l3_xcsum = __pp_hal_xcsum_u32( l3_xcsum, 
                                      session_cfg->ingress.lookup.LUT2.u.fields.WAN_addr_IP.v4, 
                                      session_cfg->egress.SRC_IP.v4 );
        }

        l3_xcsum  = (l3_xcsum & 0xFFFF) + ((l3_xcsum >> 16) & 0xFFFF);
        l4_xcsum = l3_xcsum; // ?

        if ( session_cfg->egress.enable & AVALANCHE_PP_EGRESS_FIELD_ENABLE_L4 )
        {
            modRec->ModFlags |= TI_PP_MOD_DSTPORT_VALID | TI_PP_MOD_L4CHK_VALID;
            l4_xcsum = __pp_hal_xcsum_u16( l4_xcsum, 
                                      session_cfg->ingress.lookup.LUT2.u.fields.L4_DstPort, 
                                      session_cfg->egress.L4_DstPort );

            modRec->ModFlags |= TI_PP_MOD_SRCPORT_VALID | TI_PP_MOD_L4CHK_VALID;
            l4_xcsum = __pp_hal_xcsum_u16( l4_xcsum, 
                                      session_cfg->ingress.lookup.LUT2.u.fields.L4_SrcPort, 
                                      session_cfg->egress.L4_SrcPort );
        }

        modRec->Tos             = session_cfg->egress.TOS;
        modRec->IpSrc           = session_cfg->egress.SRC_IP.v4;
        modRec->IpDst           = session_cfg->egress.DST_IP.v4;
        modRec->PortSrc         = session_cfg->egress.L4_SrcPort;
        modRec->PortDst         = session_cfg->egress.L4_DstPort;
        modRec->L3ChecksumDelta = l3_xcsum;
        modRec->L4ChecksumDelta = l4_xcsum;

        sesFwInfo->EgressRecord.EgressFlags |= TI_PP_EGR_FLAG_MOD_REC_VALID;

        return (PP_RC_SUCCESS);
    }

    printk ("ERROR: Unsupported egress L3/L4 packet type (%d).\n", session_cfg->egress.l3_packet_type);
    return (PP_RC_FAILURE);
}



static AVALANCHE_PP_RET_e   __pp_hal_SetNewHeader( AVALANCHE_PP_SESSION_INFO_t *    session_cfg , 
                                                   PP_HAL_SESSION_INFO_t *          sesFwInfo )
{
    Uint8   tmp_buf[ 128 ];
    Uint8   newHeaderLen;
    Uint32  vlanTag;

    AVALANCHE_PP_VPID_INFO_t *                  ptr_vpid;
    AVALANCHE_PP_PID_t *                        ptr_pid;
    AVALANCHE_PP_EGRESS_SESSION_PROPERTY_t *    out_prop    = &session_cfg->egress;
    PP_HAL_SESSION_EGRESS_REC_t *               egress_rec  = &sesFwInfo->EgressRecord;

    avalanche_pp_vpid_get_info( session_cfg->egress.vpid_handle, &ptr_vpid );
    avalanche_pp_pid_get_info( ptr_vpid->parent_pid_handle, &ptr_pid );

/*==============================================================*/

    newHeaderLen = 0;

    if (ptr_pid->tx_hw_data_len)
    {
        memcpy( &tmp_buf[newHeaderLen], &ptr_pid->tx_hw_data[0], ptr_pid->tx_hw_data_len);
        newHeaderLen += ptr_pid->tx_hw_data_len;
    }

    /* Append the DST MAC & SRC MAC to the header */
    memcpy( &tmp_buf[newHeaderLen], &out_prop->dstmac[0], sizeof(out_prop->dstmac) + sizeof(out_prop->srcmac) );
    newHeaderLen += sizeof(out_prop->dstmac) + sizeof(out_prop->srcmac);

    /* Add 802.1Q VLAN header if needed */

    /* In case VLAN exist both in VPID and in packet, we will first add the VPID's VLAN and then the packet's VLAN */
    if ((ptr_vpid->type == AVALANCHE_PP_VPID_VLAN) || (ptr_vpid->type == AVALANCHE_PP_VPID_VLAN_PPPoE))
    {
        /* For VPID of type VLAN we take the ID from VPID and the priority from the packet */
        vlanTag = (ETH_P_8021Q << 16) | (out_prop->vlan & VLAN_PRIO_MASK) | (ptr_vpid->vlan_identifier & VLAN_VID_MASK);
        memcpy( &tmp_buf[ newHeaderLen ], (Uint8*)&vlanTag, sizeof(vlanTag)); 
        newHeaderLen += sizeof(vlanTag);
    }

    if ( out_prop->enable & AVALANCHE_PP_EGRESS_FIELD_ENABLE_VLAN )
    {
        /* For egress packet with VPID, we take all information from the packet */
        vlanTag = (ETH_P_8021Q << 16) | out_prop->vlan;
        memcpy( &tmp_buf[ newHeaderLen ], (Uint8*)&vlanTag, sizeof(vlanTag)); 
        newHeaderLen += sizeof(vlanTag);
    }

    if((ptr_vpid->type == AVALANCHE_PP_VPID_PPPoE) || (ptr_vpid->type == AVALANCHE_PP_VPID_VLAN_PPPoE))
    {
        /* Apply header as per RFC 2516 */
        Uint16 val;

        val = 0x8864; /* Ether type for PPPoE Session packets */
        memcpy( &tmp_buf[ newHeaderLen ], (Uint8*)&val, sizeof(val)); 
        newHeaderLen +=  sizeof(val);

        val = (((1<<4) | 1) << 8) | 0; /* ver=1, type=1, code=0 */
        memcpy( &tmp_buf[ newHeaderLen ], (Uint8*)&val, sizeof(val)); 
        newHeaderLen +=  sizeof(val);

        memcpy( &tmp_buf[ newHeaderLen ], (Uint8*)&ptr_vpid->ppp_session_id, sizeof(val)); 
        newHeaderLen +=  sizeof(val);

        egress_rec->FramingCode |= TI_PP_EGR_FRM_PPPOE_HDR;
    }

    memcpy( &tmp_buf[ newHeaderLen ], (Uint8*)&out_prop->eth_type, sizeof(out_prop->eth_type)); 
    newHeaderLen +=  sizeof(out_prop->eth_type);

    if (AVALANCHE_PP_EGRESS_FIELD_ENABLE_ENCAPSULATION & out_prop->enable)
    {
        memcpy( &tmp_buf[newHeaderLen], &out_prop->wrapHeader[0], out_prop->wrapHeaderLen);
        newHeaderLen += out_prop->wrapHeaderLen;
    }

    /* This setting is actually only supported for PPPoE classified packets (with IP as PPP type). But should be don't care for other packets */
    if (( out_prop->l3_packet_type == AVALANCHE_PP_LUT_ENTRY_L3_IPV4 ) || 
        ( out_prop->l3_packet_type == AVALANCHE_PP_LUT_ENTRY_L3_IPV6 ))
    {
        egress_rec->FramingCode |= TI_PP_EGR_FRM_REFRAME_IP;
    }

    egress_rec->FramingCode |= TI_PP_EGR_FRM_STRIP_L2;


    /*=================================================================*/

    *(Uint32*)sesFwInfo->NewHeader = IO_VIRT2PHY(&sesFwInfo->NewHeader);


    if(newHeaderLen > sizeof(sesFwInfo->NewHeader))
    {
        /* Need to allocate new header since the record is not enough */

        PP_HAL_BLOCK_EXTENDED_NEW_HDR_t *   newHeader;

        sesFwInfo->EgressRecord.EgressFlags |= TI_PP_EGR_FLAG_NEW_HEADER_PTR;

        /* First, try to allocate from PP internal memory */
        newHeader = __pp_hal_block_ext_hdr_alloc();

        if(newHeader == NULL)
        {
            /* Allocate from DDR (malloc 128 bytes) */
            newHeader = kmalloc(sizeof(PP_HAL_BLOCK_EXTENDED_NEW_HDR_t), GFP_ATOMIC);

            if(newHeader == NULL)
            {
                printk("%s:%d FATAL !!! OUT OF MEMORY !!!\n",__FUNCTION__,__LINE__);
                return (PP_RC_OUT_OF_MEMORY);
            }

            sesFwInfo->EgressRecord.EgressFlags &= ~TI_PP_EGR_FLAG_NEW_HEADER_INTERNAL;

            memcpy( newHeader, tmp_buf, newHeaderLen );

            PAL_CPPI4_CACHE_WRITEBACK_INVALIDATE((void*)newHeader, newHeaderLen);

            *(Uint32*)sesFwInfo->NewHeader = PAL_CPPI4_VIRT_2_PHYS(newHeader);
        }
        else
        {
            sesFwInfo->EgressRecord.EgressFlags |= TI_PP_EGR_FLAG_NEW_HEADER_INTERNAL;

            memcpy( newHeader, tmp_buf, newHeaderLen );

            *(Uint32*)sesFwInfo->NewHeader = (Uint32)(IO_VIRT2PHY( newHeader ));
        }
    }
    else
    {
        memcpy( sesFwInfo->NewHeader, tmp_buf, newHeaderLen );
    }

    egress_rec->NewHeaderSize   = newHeaderLen;

    return (PP_RC_SUCCESS);
}



AVALANCHE_PP_RET_e      __pp_hal_SetEgressRecord( AVALANCHE_PP_SESSION_INFO_t * session_cfg , PP_HAL_SESSION_INFO_t *sesFwInfo )
{
    AVALANCHE_PP_EGRESS_SESSION_PROPERTY_t * out_prop   = &session_cfg->egress;
    Uint8 is_routable                                   =  session_cfg->is_routable_session;
    Uint8 priority                                      =  session_cfg->priority;
    Uint8 cluster                                       =  session_cfg->cluster;

    PP_HAL_SESSION_EGRESS_REC_t *       egress_rec  = &sesFwInfo->EgressRecord;
    PP_HAL_SESSION_BASE_REC_t *         base_rec    = &sesFwInfo->BaseRecord;

    AVALANCHE_PP_VPID_INFO_t *      ptr_vpid;
    AVALANCHE_PP_PID_t *            ptr_pid;

    Uint16  egress_queue;

    egress_rec->FramingCode = 0;
    egress_rec->EgressFlags = 0;

    if(sesFwInfo == NULL)
    {
        return (PP_RC_INVALID_PARAM);
    }

    avalanche_pp_vpid_get_info( session_cfg->egress.vpid_handle, &ptr_vpid );
    avalanche_pp_pid_get_info( ptr_vpid->parent_pid_handle, &ptr_pid );

    if (is_routable)
    {
        /* ARRIS propagate INTEL MR24372 fix */
        if(__pp_hal_SetModificationRecord( session_cfg, sesFwInfo ) )
        {
            return (PP_RC_FAILURE);
        }
        /* ARRIS propagate INTEL MR24372 fix end */
    }

    if (__pp_hal_SetNewHeader(session_cfg, sesFwInfo) != PP_RC_SUCCESS)
    {
        printk("%s ERROR: ppdSetNewHeader failure\n", __FUNCTION__);
        return (PP_RC_FAILURE);
    }

    if ( IS_DS_LITE_US( base_rec->SessionFlags ) )
    {
        egress_rec->EgressFlags |= TI_PP_EGR_FLAG_NEW_HEADER_HAS_IPV6;
    }
    else if ( IS_GRE_US( base_rec->SessionFlags ) )
    {
        if (AVALANCHE_PP_LUT_ENTRY_L3_IPV4 == out_prop->l3_packet_type)
        {
            egress_rec->EgressFlags |= TI_PP_EGR_FLAG_NEW_HEADER_HAS_IPV4;
        }
        else if (AVALANCHE_PP_LUT_ENTRY_L3_IPV6 == out_prop->l3_packet_type)
        {
            egress_rec->EgressFlags |= TI_PP_EGR_FLAG_NEW_HEADER_HAS_IPV6;
        }
    }

    egress_rec->NextEgressRecIdx = 0xFF;

    /* Support TurboDox */
    if (AVALANCHE_PP_EGRESS_FIELD_ENABLE_TDOX_ENABLED & out_prop->enable)
    {
        egress_rec->FramingCode    |= TI_PP_EGR_FRM_TURBODOX_EN;
        egress_rec->UsTurboDoxAck   = out_prop->tdox_tcp_ack_number;

      /* Support advanced TurboDox option to allow suppression of 12 bytes length TCP option */
        if (AVALANCHE_PP_EGRESS_FIELD_ENABLE_TDOX_SKIP_TIMESTAMP & out_prop->enable)
        {
            egress_rec->FramingCode |= TI_PP_EGR_FRM_TURBODOX_ADV_EN;
        }
    }

    if ((0 == ptr_vpid->qos_clusters_count) || (0xFF == cluster) || (NULL == ptr_vpid->qos_cluster[cluster]))
    {
        egress_queue = ptr_pid->tx_pri_q_map[priority];
    }
    else
    {
        egress_queue = ptr_vpid->qos_cluster[cluster]->qos_q_cfg[priority].q_num;
        egress_queue += PAL_CPPI41_SR_QPDSP_QOS_Q_BASE;
    }

    egress_rec->Priority        = ptr_pid->dflt_pri_drp;
    egress_rec->EgressVPID      = ptr_vpid->vpid_handle;
    egress_rec->TxDestTag       = ptr_pid->dflt_dst_tag;
    egress_rec->TxQueueBase     = egress_queue;


    egress_rec->UsPayloadLenOff = out_prop->wrapHeaderDataLenOffset;
    egress_rec->EgressPidType   = ptr_pid->type;

    /* DOCSIS/TurboDox application specific handling */
    if (AVALANCHE_PP_EGRESS_FIELD_ENABLE_PSI & out_prop->enable)
    {
        egress_rec->psi = out_prop->psi.us_fields;

        if (AVALANCHE_PP_EGRESS_FIELD_ENABLE_TDOX_ENABLED & out_prop->enable)
        {
            egress_rec->psi.tdox_id = out_prop->tdox_handle;
        }
    }

    return (PP_RC_SUCCESS);
}




AVALANCHE_PP_RET_e      __pp_hal_SetBaseRecordFlags( AVALANCHE_PP_SESSION_INFO_t * session_cfg, Uint16 * sessionFlags )
{
    /*
     * CHK: Always setting IDLE base timeout since there is no way to configure
     * otherwise in current data structure
     */

    *sessionFlags = TI_PP_SES_FLAG_IDLE_TMOUT;

    if (AVALANCHE_PP_DEV_TYPE_WAN == session_cfg->ingress.dev_type)
    {
        if (AVALANCHE_PP_LUT_ENTRY_L3_GRE == session_cfg->ingress.lookup.LUT1.u.fields.L3.entry_type)
        {
            *sessionFlags |= TI_PP_SES_FLAG_GRE_DS | TI_PP_SES_FLAG_DO_REASSEMBLY;
        }
        else if (AVALANCHE_PP_LUT_ENTRY_L3_DSLITE == session_cfg->ingress.lookup.LUT1.u.fields.L3.entry_type)
        {
            *sessionFlags |= TI_PP_SES_FLAG_DS_LITE_DS | TI_PP_SES_FLAG_DO_REASSEMBLY;
        }
    }
    else if (AVALANCHE_PP_DEV_TYPE_WAN == session_cfg->egress.dev_type)
    {
        if (AVALANCHE_PP_LUT_ENTRY_L3_GRE == session_cfg->egress.tunnel_type)
        {
            *sessionFlags |= TI_PP_SES_FLAG_GRE_US;
        }
        else if (AVALANCHE_PP_LUT_ENTRY_L3_DSLITE == session_cfg->egress.tunnel_type)
        {
            *sessionFlags |= TI_PP_SES_FLAG_DS_LITE_US | TI_PP_SES_FLAG_DO_REASSEMBLY;

            if (gPpHalDsLiteUsFragIPv4)
            {
                *sessionFlags |= TI_PP_SES_FLAG_DSLITE_US_FRAG_IPv4;
            }
        }
    }

    /* Check if this is a bridged or routed session? */
    if (session_cfg->is_routable_session == 1)
    {
        /* All routable sessions will need their TTL Flags decremented by the PDSP */
        *sessionFlags |= TI_PP_SES_FLAG_UPDATE_TTL;
    }
    else
    {
        *sessionFlags |= TI_PP_SES_FLAG_PROC_IP_OPTS;
    }

    /* Set IPv6 Address Record */
    if (session_cfg->ingress.lookup.LUT2.u.fields.entry_type == AVALANCHE_PP_LUT_ENTRY_L3_IPV6)
    {
        /* Set IPv6 packet class level.
         * TODO: Currently forcing highest possible class level so that all
         * majority of the session packets pass trhough. Once we have some way
         * to get this value from application, we need to modify here
         */
        *sessionFlags |= TI_PP_SES_FLAG_IPV6_CLASS_SET(0x3);
    }

    return (PP_RC_SUCCESS);
}


AVALANCHE_PP_RET_e  __pp_hal_DestroySession( Uint32 session_handle )
{
    volatile PP_HAL_SESSION_INFO_t * hal_session = &gPpHalSessionInfo[ session_handle ];

    if(hal_session->EgressRecord.EgressFlags & TI_PP_EGR_FLAG_NEW_HEADER_PTR)
    {
        if(hal_session->EgressRecord.EgressFlags & TI_PP_EGR_FLAG_NEW_HEADER_INTERNAL)
        {
            __pp_hal_block_ext_hdr_free( (PP_HAL_BLOCK_EXTENDED_NEW_HDR_t *) IO_PHY2VIRT( *(Uint32*)hal_session->NewHeader) ); // TBD - support for multicast
        }
        else
        {
            kfree((void*) PAL_CPPI4_PHYS_2_VIRT( *(Uint32*)hal_session->NewHeader) );
        }
    }
    
    return (PP_RC_SUCCESS);
}


/**************************************************************************
 * FUNCTION NAME : AVALANCHE_PP_RET_e pp_hal_session_create ( AVALANCHE_PP_SESSION_INFO_t * session_cfg, Bool create_LUT1 )
 **************************************************************************
 * DESCRIPTION   :
 *  The function is used to create a session in PDSP.
 *  param[in] session_cfg - pointer to session information
 *  param[in] create_LUT1 - 
 * RETURNS       :
 *      0  -   Success
 *      >0  -   Error
 **************************************************************************/
AVALANCHE_PP_RET_e      pp_hal_session_create( AVALANCHE_PP_SESSION_INFO_t * session_cfg, Bool create_LUT1 )
{
    volatile PP_HAL_SESSION_INFO_t * hal_session = &gPpHalSessionInfo[ session_cfg->session_handle ];
    AVALANCHE_PP_RET_e          rc;

    __pp_hal_SetBaseRecordFlags( session_cfg, &hal_session->BaseRecord.SessionFlags );
    __pp_hal_SetEgressRecord( session_cfg, hal_session );

    hal_session->BaseRecord.IngressVPID = session_cfg->ingress.vpid_handle;
    hal_session->BaseRecord.StatusFlags = 0;

    /* Convert provided usecs as 10^-5 seconds with mimnimum of 10us */
    hal_session->BaseRecord.TimeoutThresh = (session_cfg->session_timeout/10) + ((session_cfg->session_timeout%10) ? 1 : 0);

    {
        PP_HAL_CMD_u    cmd_word;

        cmd_word.pp_cmd.cmd         = PP_HAL_PDSP_CMD_SESSION;
        cmd_word.pp_cmd.cmd_minor   = session_cfg->ingress.lookup.LUT2.u.fields.LUT1_key;
        cmd_word.pp_cmd.cmd_param   = session_cfg->session_handle | SESSION_ADD_SYNC;

        if (create_LUT1)
        {
            cmd_word.pp_cmd.cmd_param |= SESSION_LUT1_INDEX_VALID;
        }

#if 0
        {
            Uint32 * cmdParam = (Uint32 *)&session_cfg->ingress.lookup;

            printk("Add ses_id=%d,\nLUT1.L2: 0x%08X, 0x%08X, 0x%08X, 0x%08X ,0x%08X, 0x%08X\nLUT1.L3: 0x%08X ,0x%08X ,0x%08X ,0x%08X ,0x%08X, 0x%08X\n",
                   session_cfg->session_handle, 
                   cmdParam[0], cmdParam[1], cmdParam[2], cmdParam[3], cmdParam[4], cmdParam[5], 
                   cmdParam[6], cmdParam[7], cmdParam[8], cmdParam[9], cmdParam[10], cmdParam[11]);

            printk("LUT2: 0x%08X, 0x%08X, 0x%08X, 0x%08X ,0x%08X ,0x%08X ,0x%08X ,0x%08X\n",
                   cmdParam[12], cmdParam[13], cmdParam[14], cmdParam[15], cmdParam[16], cmdParam[17], cmdParam[18], cmdParam[19]);
        }
#endif

        rc = pdsp_cmd_send( PDSP_ID_Classifier, 
                            cmd_word.pdsp_cmd, 
                            (Uint32 *)&session_cfg->ingress.lookup,   SIZE_IN_WORD( session_cfg->ingress.lookup ),
                            NULL,   0 );

        if (rc)
        {
            return (rc + PP_RC_FAILURE);
        }
    }

    return (PP_RC_SUCCESS);
}

/**************************************************************************
 * FUNCTION NAME : AVALANCHE_PP_RET_e pp_hal_session_delete ( AVALANCHE_PP_SESSION_INFO_t * session_cfg, Bool delete_LUT1 )
 **************************************************************************
 * DESCRIPTION   :
 *  The function is used to delete the session from PDSP.
 *  param[in] session_cfg - pointer to session information
 *  param[in] delete_LUT1 - 
 * RETURNS       :
 *      0  -   Success
 *      >0  -   Error
 **************************************************************************/
AVALANCHE_PP_RET_e      pp_hal_session_delete( AVALANCHE_PP_SESSION_INFO_t *    session_cfg, Bool delete_LUT1 )
{
    AVALANCHE_PP_RET_e                  rc;
    PP_HAL_CMD_u                        cmd_word;

    cmd_word.pp_cmd.cmd         = PP_HAL_PDSP_CMD_SESSION;
    cmd_word.pp_cmd.cmd_minor   = session_cfg->ingress.lookup.LUT2.u.fields.LUT1_key;
    cmd_word.pp_cmd.cmd_param   = session_cfg->session_handle | SESSION_REMOVE;

    if (delete_LUT1)
    {
        cmd_word.pp_cmd.cmd_param |= SESSION_LUT1_INDEX_VALID;
    }

    rc = pdsp_cmd_send( PDSP_ID_Classifier, 
                        cmd_word.pdsp_cmd, 
                        (Uint32 *)&session_cfg->ingress.lookup.LUT2,   SIZE_IN_WORD(session_cfg->ingress.lookup.LUT2),
                        NULL,   0 );

    if (rc)
    {
        return (rc + PP_RC_FAILURE);
    }


    /* CHK: We are cleaning up after destroying the session. This will work if
     * the egress lists are not cleared by firmware on session remove.
     */

    __pp_hal_DestroySession( session_cfg->session_handle );

    return (PP_RC_SUCCESS);
}

/* ******************************************************************** */
/*                __  __ ___ ____   ____                                */
/*               |  \/  |_ _/ ___| / ___|                               */
/*               | |\/| || |\___ \| |                                   */
/*               | |  | || | ___) | |___                                */
/*               |_|  |_|___|____/ \____|                               */
/*                                                                      */
/* ******************************************************************** */

#define PP_HAL_TDOX_HIGH_PRIORITY_QOS_QUEUE_INDEX  0
#define PP_HAL_TDOX_LOW_PRIORITY_QOS_QUEUE_INDEX   1



AVALANCHE_PP_RET_e      pp_hal_session_tdox_update( AVALANCHE_PP_SESSION_INFO_t *    session_cfg )
{
    volatile PP_HAL_SESSION_EGRESS_REC_t * egress_rec = &gPpHalSessionInfo[ session_cfg->session_handle ].EgressRecord;
    AVALANCHE_PP_VPID_INFO_t *          ptr_vpid;
    AVALANCHE_PP_PID_t *                ptr_pid;
    Uint16                              egress_queue;

    avalanche_pp_vpid_get_info( session_cfg->egress.vpid_handle, &ptr_vpid );
    avalanche_pp_pid_get_info( ptr_vpid->parent_pid_handle, &ptr_pid );

    if (AVALANCHE_PP_EGRESS_FIELD_ENABLE_TDOX_ENABLED & session_cfg->egress.enable)
    {
        egress_rec->FramingCode    |=  TI_PP_EGR_FRM_TURBODOX_EN;
        egress_rec->psi.tdox_id     = session_cfg->egress.tdox_handle;

        if (session_cfg->priority == PP_HAL_TDOX_LOW_PRIORITY_QOS_QUEUE_INDEX)
        {
            session_cfg->priority = PP_HAL_TDOX_HIGH_PRIORITY_QOS_QUEUE_INDEX; // Move to high priority QoS queue
        }
    }
    else
    {
        egress_rec->FramingCode    &= ~TI_PP_EGR_FRM_TURBODOX_EN;

        if (session_cfg->priority == PP_HAL_TDOX_HIGH_PRIORITY_QOS_QUEUE_INDEX)
        {
            session_cfg->priority = PP_HAL_TDOX_LOW_PRIORITY_QOS_QUEUE_INDEX; // Move to low priority QoS queue
        }
    }

    if ((0 == ptr_vpid->qos_clusters_count) || (0xFF == session_cfg->cluster) || (NULL == ptr_vpid->qos_cluster[ session_cfg->cluster ]))
    {
        egress_queue = ptr_pid->tx_pri_q_map[ session_cfg->priority ];
    }
    else
    {
        egress_queue = ptr_vpid->qos_cluster[ session_cfg->cluster ]->qos_q_cfg[ session_cfg->priority ].q_num;
        egress_queue += PAL_CPPI41_SR_QPDSP_QOS_Q_BASE;
    }

    egress_rec->TxQueueBase = egress_queue;

    return (PP_RC_SUCCESS);
}

AVALANCHE_PP_RET_e      pp_hal_session_tdox_get( Uint32 session_handle, Bool * enabled )
{
    *enabled = ( 0 != (gPpHalSessionInfo[ session_handle ].EgressRecord.FramingCode & TI_PP_EGR_FRM_TURBODOX_EN ));

    return (PP_RC_SUCCESS);
}

/**************************************************************************
 * FUNCTION NAME : AVALANCHE_PP_RET_e pp_hal_version_get( AVALANCHE_PP_VERSION_t * version )
 **************************************************************************
 * DESCRIPTION   :
 *  The function is called to get the version information from the packet processor.
 *  param[in] version - pointer to version struct 
 * RETURNS       :
 *      0  -   Success
 *      >0  -   Error
 **************************************************************************/
AVALANCHE_PP_RET_e      pp_hal_version_get( AVALANCHE_PP_VERSION_t * version )
{
    AVALANCHE_PP_RET_e                  rc;
    PP_HAL_CMD_u                        cmd_word;

    cmd_word.pp_cmd.cmd         = PP_HAL_PDSP_CMD_VERSION;
    cmd_word.pp_cmd.cmd_minor   = 0;
    cmd_word.pp_cmd.cmd_param   = 0;

    rc = pdsp_cmd_send( PDSP_ID_Classifier, 
                        cmd_word.pdsp_cmd, 
                        NULL,    0,
                        version, SIZE_IN_WORD(*version) );

    if (rc)
    {
        return (rc + PP_RC_FAILURE);
    }

    return (PP_RC_SUCCESS);
}
/**************************************************************************
 * FUNCTION NAME : AVALANCHE_PP_RET_e pp_hal_mta_address_set ( Uint8 * mtaAddress )
 **************************************************************************
 * DESCRIPTION   :
 *  The function sets MTA MAC address for the packet processor.
 *  param[in] mtaAddress - pointer to MTA MAC address 
 * RETURNS       :
 *      0  -   Success
 *      >0  -   Error
 **************************************************************************/
AVALANCHE_PP_RET_e    pp_hal_mta_address_set( Uint8 * mtaAddress )
{
    Uint8* dest = (Uint8*) IO_PHY2VIRT( PP_HAL_MTA_MAC_BASE_PHY );
    memcpy(dest, mtaAddress, 6);

    return (PP_RC_SUCCESS);
}

/**************************************************************************
 * FUNCTION NAME : AVALANCHE_PP_RET_e pp_hal_qos_queue_config_set( AVALANCHE_PP_QOS_QUEUE_t* qos_q_cfg )
 **************************************************************************
 * DESCRIPTION   :
 *  This function set the qos queue configuration.
 *  param[in] qos_q_cfg - pointer to qos queue structure
 * RETURNS       :
 *      0  -   Success
 *      >0  -   Error
 **************************************************************************/
AVALANCHE_PP_RET_e      pp_hal_qos_queue_config_set( AVALANCHE_PP_QOS_QUEUE_t*       qos_q_cfg )
{
    PP_HAL_QOS_QUEUE_t*   qcfg = (PP_HAL_QOS_QUEUE_t*)(IO_PHY2VIRT(PP_HAL_QOS_QCFG_BLK_BASE_PHY)) + qos_q_cfg->q_num;

    qcfg->egr_q                     = qos_q_cfg->egr_q;
    qcfg->flags                     = qos_q_cfg->flags;
    qcfg->iteration_credit_bytes    = qos_q_cfg->it_credit_bytes;
    qcfg->total_credit_bytes        = 0;
    qcfg->max_credit_bytes          = qos_q_cfg->max_credit_bytes;
    qcfg->iteration_credit_pkts     = qos_q_cfg->it_credit_packets;
    qcfg->total_credit_pkts         = 0;
    qcfg->max_credit_pkts           = qos_q_cfg->max_credit_packets;
    qcfg->congst_thrsh_pkts         = qos_q_cfg->congst_thrsh_packets;
    qcfg->congst_thrsh_bytes        = qos_q_cfg->congst_thrsh_bytes;

    return (PP_RC_SUCCESS);
}
/**************************************************************************
 * FUNCTION NAME : AVALANCHE_PP_RET_e pp_hal_qos_queue_config_get( AVALANCHE_PP_QOS_QUEUE_t* qos_q_cfg )
 **************************************************************************
 * DESCRIPTION   :
 *  This function get the qos queue configuration.
 *  param[in] qos_q_cfg - pointer to qos queue structure
 * RETURNS       :
 *      0  -   Success
 *      >0  -   Error
 **************************************************************************/
AVALANCHE_PP_RET_e      pp_hal_qos_queue_config_get( AVALANCHE_PP_QOS_QUEUE_t*       qos_q_cfg )
{
    PP_HAL_QOS_QUEUE_t*   qcfg = (PP_HAL_QOS_QUEUE_t*)(IO_PHY2VIRT(PP_HAL_QOS_QCFG_BLK_BASE_PHY)) + qos_q_cfg->q_num;

    qos_q_cfg->egr_q                 =  qcfg->egr_q                 ;
    qos_q_cfg->flags                 =  qcfg->flags                 ;
    qos_q_cfg->it_credit_bytes       =  qcfg->iteration_credit_bytes;
    qos_q_cfg->max_credit_bytes      =  qcfg->max_credit_bytes      ;
    qos_q_cfg->it_credit_packets     =  qcfg->iteration_credit_pkts ;
    qos_q_cfg->max_credit_packets    =  qcfg->max_credit_pkts       ;
    qos_q_cfg->congst_thrsh_packets  =  qcfg->congst_thrsh_pkts     ;
    qos_q_cfg->congst_thrsh_bytes    =  qcfg->congst_thrsh_bytes    ;

    return (PP_RC_SUCCESS);
}

/**************************************************************************
 * FUNCTION NAME : AVALANCHE_PP_RET_e pp_hal_qos_cluster_config_set( Uint8  clst_indx,  AVALANCHE_PP_QOS_CLST_CFG_t* clst_cfg, Uint16* egr_queues, Uint8 egr_qcount )
 **************************************************************************
 * DESCRIPTION   :
 *  This function set the qos cluster configuration.
 *  param[in] clst_indx - cluster index
 *  param[in] clst_cfg - pointer to cluster configuration structure
 *  param[in] egr_queues - pointer to egress queue
 *  param[in] egr_qcount - number of egress queues
 * RETURNS       :
 *      0  -   Success
 *      >0  -   Error
 **************************************************************************/
AVALANCHE_PP_RET_e   pp_hal_qos_cluster_config_set( Uint8  clst_indx,  AVALANCHE_PP_QOS_CLST_CFG_t*    clst_cfg, Uint16* egr_queues, Uint8 egr_qcount )
{
    PP_HAL_QOS_CLST_CFG_t*  clst = (PP_HAL_QOS_CLST_CFG_t*)(IO_PHY2VIRT(PP_HAL_QOS_CLST_BLK_BASE_PHY)) + clst_indx;

    clst->global_credit_bytes           = clst_cfg->global_credit_bytes;
    clst->max_global_credit_bytes       = clst_cfg->max_global_credit_bytes;
    clst->global_credit_pkts            = clst_cfg->global_credit_packets;
    clst->max_global_credit_pkts        = clst_cfg->max_global_credit_packets;
    clst->qos_q_cnt                     = clst_cfg->qos_q_cnt;
    clst->egr_q_cnt                     = egr_qcount;
    clst->reserved                      = 0;
    clst->qos_q0                        = clst_cfg->qos_q_cfg[0].q_num;
    clst->qos_q1                        = clst_cfg->qos_q_cfg[1].q_num;
    clst->qos_q2                        = clst_cfg->qos_q_cfg[2].q_num;
    clst->qos_q3                        = clst_cfg->qos_q_cfg[3].q_num;
    clst->qos_q4                        = clst_cfg->qos_q_cfg[4].q_num;
    clst->qos_q5                        = clst_cfg->qos_q_cfg[5].q_num;
    clst->qos_q6                        = clst_cfg->qos_q_cfg[6].q_num;
    clst->qos_q7                        = clst_cfg->qos_q_cfg[7].q_num;
    clst->egr_q0                        = egr_queues[0];
    clst->egr_q1                        = egr_queues[1];
    clst->egr_q2                        = egr_queues[2];
    clst->egr_q3                        = egr_queues[3];
    clst->egr_congst_thrsh_bytes1       = clst_cfg->egr_congst_thrsh_bytes1;
    clst->egr_congst_thrsh_bytes2       = clst_cfg->egr_congst_thrsh_bytes2;
    clst->egr_congst_thrsh_bytes3       = clst_cfg->egr_congst_thrsh_bytes3;
    clst->egr_congst_thrsh_bytes4       = clst_cfg->egr_congst_thrsh_bytes4;

    if( (clst_cfg->egr_congst_thrsh_packets1) ||(clst_cfg->egr_congst_thrsh_packets2) || (clst_cfg->egr_congst_thrsh_packets3) ||(clst_cfg->egr_congst_thrsh_packets4))
    {
        clst->egr_congst_thrsh_pkts1 = clst_cfg->egr_congst_thrsh_packets1;
        clst->egr_congst_thrsh_pkts2 = clst_cfg->egr_congst_thrsh_packets2;
        clst->egr_congst_thrsh_pkts3 = clst_cfg->egr_congst_thrsh_packets3;
        clst->egr_congst_thrsh_pkts4 = clst_cfg->egr_congst_thrsh_packets4;
    }
    else
    {
        /* In order for the thrshold to be disabled - setting highest values */
        clst->egr_congst_thrsh_pkts1 = 0xFFFF;
        clst->egr_congst_thrsh_pkts2 = 0xFFFF;
        clst->egr_congst_thrsh_pkts3 = 0xFFFF;
        clst->egr_congst_thrsh_pkts4 = 0xFFFF;
    }

    return (PP_RC_SUCCESS);
}
/**************************************************************************
 * FUNCTION NAME : AVALANCHE_PP_RET_e pp_hal_qos_cluster_config_get( Uint8  clst_indx,  AVALANCHE_PP_QOS_CLST_CFG_t* clst_cfg, Uint16* egr_queues, Uint8 egr_qcount )
 **************************************************************************
 * DESCRIPTION   :
 *  This function get the qos cluster configuration.
 *  param[in] clst_indx - cluster index
 *  param[in] clst_cfg - pointer to cluster configuration structure
 *  param[in] egr_queues - pointer to egress queue
 *  param[in] egr_qcount - pointer to number of egress queues
 * RETURNS       :
 *      0  -   Success
 *      >0  -   Error
 **************************************************************************/
AVALANCHE_PP_RET_e   pp_hal_qos_cluster_config_get( Uint8  clst_indx,  AVALANCHE_PP_QOS_CLST_CFG_t*    clst_cfg, Uint16*  egr_queues, Uint8*  egr_qcount )
{
    PP_HAL_QOS_CLST_CFG_t*  clst = (PP_HAL_QOS_CLST_CFG_t*)(IO_PHY2VIRT(PP_HAL_QOS_CLST_BLK_BASE_PHY)) + clst_indx;

    clst_cfg->global_credit_bytes        =  clst->global_credit_bytes       ;
    clst_cfg->max_global_credit_bytes    =  clst->max_global_credit_bytes   ;
    clst_cfg->global_credit_packets      =  clst->global_credit_pkts        ;
    clst_cfg->max_global_credit_packets  =  clst->max_global_credit_pkts    ;
    clst_cfg->qos_q_cnt                  =  clst->qos_q_cnt                 ;
    clst_cfg->qos_q_cfg[0].q_num         =  clst->qos_q0                    ;
    clst_cfg->qos_q_cfg[1].q_num         =  clst->qos_q1                    ;
    clst_cfg->qos_q_cfg[2].q_num         =  clst->qos_q2                    ;
    clst_cfg->qos_q_cfg[3].q_num         =  clst->qos_q3                    ;
    clst_cfg->qos_q_cfg[4].q_num         =  clst->qos_q4                    ;
    clst_cfg->qos_q_cfg[5].q_num         =  clst->qos_q5                    ;
    clst_cfg->qos_q_cfg[6].q_num         =  clst->qos_q6                    ;
    clst_cfg->qos_q_cfg[7].q_num         =  clst->qos_q7                    ;
    clst_cfg->egr_congst_thrsh_bytes1    =  clst->egr_congst_thrsh_bytes1   ;
    clst_cfg->egr_congst_thrsh_bytes2    =  clst->egr_congst_thrsh_bytes2   ;
    clst_cfg->egr_congst_thrsh_bytes3    =  clst->egr_congst_thrsh_bytes3   ;
    clst_cfg->egr_congst_thrsh_bytes4    =  clst->egr_congst_thrsh_bytes4   ;


    if (( clst->egr_congst_thrsh_pkts1 == 0xFFFF ) && 
        ( clst->egr_congst_thrsh_pkts2 == 0xFFFF ) &&
        ( clst->egr_congst_thrsh_pkts3 == 0xFFFF ) &&
        ( clst->egr_congst_thrsh_pkts4 == 0xFFFF ))
    {
        clst_cfg->egr_congst_thrsh_packets1 =
        clst_cfg->egr_congst_thrsh_packets2 =
        clst_cfg->egr_congst_thrsh_packets3 =
        clst_cfg->egr_congst_thrsh_packets4 = 0;
    }
    else
    {
        clst_cfg->egr_congst_thrsh_packets1 = clst->egr_congst_thrsh_pkts1;
        clst_cfg->egr_congst_thrsh_packets2 = clst->egr_congst_thrsh_pkts2;
        clst_cfg->egr_congst_thrsh_packets3 = clst->egr_congst_thrsh_pkts3;
        clst_cfg->egr_congst_thrsh_packets4 = clst->egr_congst_thrsh_pkts4;
    }

    egr_queues[0] = clst->egr_q0 ;
    egr_queues[1] = clst->egr_q1 ;
    egr_queues[2] = clst->egr_q2 ;
    egr_queues[3] = clst->egr_q3 ;    

    *egr_qcount =  clst->egr_q_cnt;

    return (PP_RC_SUCCESS);
}

/**************************************************************************
 * FUNCTION NAME : AVALANCHE_PP_RET_e pp_hal_qos_cluster_enable( Uint8 clst_indx )
 **************************************************************************
 * DESCRIPTION   :
 *  This function enables specified QoS cluster.
 *  param[in] clst_indx - cluster id 
 * RETURNS       :
 *      0  -   Success
 *      >0  -   Error
 **************************************************************************/
AVALANCHE_PP_RET_e      pp_hal_qos_cluster_enable( Uint8 clust_index )
{
    AVALANCHE_PP_RET_e                  rc;
    PP_HAL_CMD_u                        cmd_word;

    cmd_word.pp_cmd.cmd         = PP_HAL_PDSP_CMD_QOS_CLUSTER;
    cmd_word.pp_cmd.cmd_minor   = PP_HAL_PDSP_MINOR_ENABLE;
    cmd_word.pp_cmd.cmd_param   = clust_index;

    rc = pdsp_cmd_send( PDSP_ID_Classifier, 
                        cmd_word.pdsp_cmd, 
                        NULL,   0,
                        NULL,   0 );

    if (rc)
    {
        return (rc + PP_RC_FAILURE);
    }

    return (PP_RC_SUCCESS);
}

/**************************************************************************
 * FUNCTION NAME : AVALANCHE_PP_RET_e pp_hal_qos_cluster_disable( Uint8 clst_indx )
 **************************************************************************
 * DESCRIPTION   :
 *  This function disables specified QoS cluster.
 *  param[in] clst_indx - cluster id 
 * RETURNS       :
 *      0  -   Success
 *      >0  -   Error
 **************************************************************************/
AVALANCHE_PP_RET_e      pp_hal_qos_cluster_disable( Uint8 clust_index )
{
    AVALANCHE_PP_RET_e                  rc;
    PP_HAL_CMD_u                        cmd_word;

    cmd_word.pp_cmd.cmd         = PP_HAL_PDSP_CMD_QOS_CLUSTER;
    cmd_word.pp_cmd.cmd_minor   = PP_HAL_PDSP_MINOR_DISABLE;
    cmd_word.pp_cmd.cmd_param   = clust_index;

    rc = pdsp_cmd_send( PDSP_ID_Classifier, 
                        cmd_word.pdsp_cmd, 
                        NULL,   0,
                        NULL,   0 );

    if (rc)
    {
        return (rc + PP_RC_FAILURE);
    }

    return (PP_RC_SUCCESS);
}
/**************************************************************************************/
/*! \fn static int pp_hal_display_qos_queue_info (Uint32 queue_id, Int8 *buffer, Int32 *size)
 **************************************************************************************
 *  \brief This function print all qos_queue_info
 *  \return return the number of bytes wirtten to the buffer.
 **************************************************************************************/
AVALANCHE_PP_RET_e pp_hal_display_qos_queue_info(Uint32 queue_id, Int8 *buffer, Int32 *size)
{
    Uint32  temp;
    Uint32 queueStcAddr;
    Uint32 len = 0;
    PP_HAL_QOS_QUEUE_t*   qcfg = (PP_HAL_QOS_QUEUE_t*)(IO_PHY2VIRT(PP_HAL_QOS_QCFG_BLK_BASE_PHY)) + queue_id;

    if (queue_id > AVALANCHE_PP_QOS_QUEUE_MAX_INDX)
    {
        len += sprintf(buffer + len, "queue_id(%d) out of range\n", queue_id);
        *size = len;
        return PP_RC_SUCCESS;
    }

    if (qcfg->max_credit_bytes == 0)
    {
        len += sprintf(buffer + len, "queue_id(%d) not configured\n", queue_id);
        *size = len;
        return PP_RC_SUCCESS;
    }

    len += sprintf(buffer + len, "IterationCreditBytes     = %d\n", qcfg->iteration_credit_bytes);
    len += sprintf(buffer + len, "IterationCreditPkts      = %d\n", qcfg->iteration_credit_pkts);

    len += sprintf(buffer + len, "EgressQueue         = [%d=%s]\n", qcfg->egr_q, PAL_CPPI41_GET_QNAME(qcfg->egr_q));

    temp = qcfg->flags;
    len += sprintf(buffer + len, "Flags               = 0x%02X ", temp);
    if (temp)
    {
        len += sprintf(buffer + len, "[ ");
        if (temp & 0x1)
        {
            len += sprintf(buffer + len, "fQQRealTime ");
        }
        len += sprintf(buffer + len, "]");
    }
    len += sprintf(buffer + len, "\n");
    len += sprintf(buffer + len, "TotalCreditBytes    = %d\n", qcfg->total_credit_bytes);
    len += sprintf(buffer + len, "TotalCreditPkts     = %d\n", qcfg->total_credit_pkts);
    len += sprintf(buffer + len, "MaxCreditBytes      = %d\n", qcfg->max_credit_bytes);
    len += sprintf(buffer + len, "MaxCreditPkts       = %d\n", qcfg->max_credit_pkts);
    len += sprintf(buffer + len, "CongestedBytes      = %d\n", qcfg->congst_thrsh_bytes);
    len += sprintf(buffer + len, "CongestedPkts       = %d\n", qcfg->congst_thrsh_pkts);

    /* Get info from statical counters*/
    queueStcAddr = ((IO_PHY2VIRT(PP_HAL_QOS_QCFG_BLK_BASE_PHY)) + (PP_COUNTERS_QPDSP_Q_OFF * queue_id));            
    len += sprintf(buffer + len, "PktForward          = %d\n", *(Uint32*)(queueStcAddr + PP_COUNTERS_QPDSP_Q_PKT_FRWRD_OFF));
    len += sprintf(buffer + len, "PktDrop             = %d\n\n", *(Uint32*)(queueStcAddr + PP_COUNTERS_QPDSP_Q_PKT_DROP_OFF));

    *size = len;
    return PP_RC_SUCCESS;
}
/**************************************************************************************/
/*! \fn static int pp_hal_display_qos_cluster_info (Uint32 cluster_id, Int8 *buffer, Int32 *size)
 **************************************************************************************
 *  \brief This function print all qos_cluster_info
 *  \return return the number of bytes wirtten to the buffer.
 **************************************************************************************/
AVALANCHE_PP_RET_e pp_hal_display_qos_cluster_info(Uint32 cluster_id, Int8 *buffer, Int32 *size)
{
    Uint32  temp;
    Uint32 len = 0;
    PP_HAL_QOS_CLST_CFG_t *clst = (PP_HAL_QOS_CLST_CFG_t*)(IO_PHY2VIRT(PP_HAL_QOS_CLST_BLK_BASE_PHY)) + cluster_id;

    if (cluster_id > AVALANCHE_PP_QOS_CLST_MAX_INDX)
    {
        len += sprintf(buffer + len, "cluster_id(%d) out of range\n", cluster_id);
        return PP_RC_SUCCESS;
    }
    if (clst->qos_q_cnt == 0)
    {
        len += sprintf(buffer + len, "cluster_id(%d) not configured\n", cluster_id);
        return PP_RC_SUCCESS;
    }

    len += sprintf(buffer + len, "GlobalCreditBytes        = %d\n", clst->global_credit_bytes);
    len += sprintf(buffer + len, "GlobalCreditPkts         = %d\n", clst->global_credit_pkts);

    len += sprintf(buffer + len, "MaxGlobalBytes           = %d\n", clst->max_global_credit_bytes);
    len += sprintf(buffer + len, "MaxGlobalPkts            = %d\n", clst->max_global_credit_pkts);

    temp = clst->qos_q_cnt;

    len += sprintf(buffer + len, "QQCount             = %d\n", temp);
    if (temp) {len += sprintf(buffer + len, "                      %d+%d=%d[%s]\n", clst->qos_q0, PAL_CPPI41_SR_QPDSP_QOS_Q_BASE, clst->qos_q0 + PAL_CPPI41_SR_QPDSP_QOS_Q_BASE, PAL_CPPI41_GET_QNAME(clst->qos_q0+ PAL_CPPI41_SR_QPDSP_QOS_Q_BASE)); temp--;}
    if (temp) {len += sprintf(buffer + len, "                      %d+%d=%d[%s]\n", clst->qos_q1, PAL_CPPI41_SR_QPDSP_QOS_Q_BASE, clst->qos_q1 + PAL_CPPI41_SR_QPDSP_QOS_Q_BASE, PAL_CPPI41_GET_QNAME(clst->qos_q1+ PAL_CPPI41_SR_QPDSP_QOS_Q_BASE)); temp--;}
    if (temp) {len += sprintf(buffer + len, "                      %d+%d=%d[%s]\n", clst->qos_q2, PAL_CPPI41_SR_QPDSP_QOS_Q_BASE, clst->qos_q2 + PAL_CPPI41_SR_QPDSP_QOS_Q_BASE, PAL_CPPI41_GET_QNAME(clst->qos_q2+ PAL_CPPI41_SR_QPDSP_QOS_Q_BASE)); temp--;}
    if (temp) {len += sprintf(buffer + len, "                      %d+%d=%d[%s]\n", clst->qos_q3, PAL_CPPI41_SR_QPDSP_QOS_Q_BASE, clst->qos_q3 + PAL_CPPI41_SR_QPDSP_QOS_Q_BASE, PAL_CPPI41_GET_QNAME(clst->qos_q3+ PAL_CPPI41_SR_QPDSP_QOS_Q_BASE)); temp--;}
    if (temp) {len += sprintf(buffer + len, "                      %d+%d=%d[%s]\n", clst->qos_q4, PAL_CPPI41_SR_QPDSP_QOS_Q_BASE, clst->qos_q4 + PAL_CPPI41_SR_QPDSP_QOS_Q_BASE, PAL_CPPI41_GET_QNAME(clst->qos_q4+ PAL_CPPI41_SR_QPDSP_QOS_Q_BASE)); temp--;}
    if (temp) {len += sprintf(buffer + len, "                      %d+%d=%d[%s]\n", clst->qos_q5, PAL_CPPI41_SR_QPDSP_QOS_Q_BASE, clst->qos_q5 + PAL_CPPI41_SR_QPDSP_QOS_Q_BASE, PAL_CPPI41_GET_QNAME(clst->qos_q5+ PAL_CPPI41_SR_QPDSP_QOS_Q_BASE)); temp--;}
    if (temp) {len += sprintf(buffer + len, "                      %d+%d=%d[%s]\n", clst->qos_q6, PAL_CPPI41_SR_QPDSP_QOS_Q_BASE, clst->qos_q6 + PAL_CPPI41_SR_QPDSP_QOS_Q_BASE, PAL_CPPI41_GET_QNAME(clst->qos_q6+ PAL_CPPI41_SR_QPDSP_QOS_Q_BASE)); temp--;}
    if (temp) {len += sprintf(buffer + len, "                      %d+%d=%d[%s]\n", clst->qos_q7, PAL_CPPI41_SR_QPDSP_QOS_Q_BASE, clst->qos_q7 + PAL_CPPI41_SR_QPDSP_QOS_Q_BASE, PAL_CPPI41_GET_QNAME(clst->qos_q7+ PAL_CPPI41_SR_QPDSP_QOS_Q_BASE)); temp--;}
    

    temp = clst->egr_q_cnt;

    len += sprintf(buffer + len, "EQCount             = %d\n", temp);
    if (temp) {len += sprintf(buffer + len, "                      %d[%s]\n", clst->egr_q0, PAL_CPPI41_GET_QNAME(clst->egr_q0)); temp--;}
    if (temp) {len += sprintf(buffer + len, "                      %d[%s]\n", clst->egr_q1, PAL_CPPI41_GET_QNAME(clst->egr_q1)); temp--;}
    if (temp) {len += sprintf(buffer + len, "                      %d[%s]\n", clst->egr_q2, PAL_CPPI41_GET_QNAME(clst->egr_q2)); temp--;}
    if (temp) {len += sprintf(buffer + len, "                      %d[%s]\n", clst->egr_q3, PAL_CPPI41_GET_QNAME(clst->egr_q3)); temp--;}
    
    len += sprintf(buffer + len, "ECThresh1           = %d\n", clst->egr_congst_thrsh_bytes1);
    len += sprintf(buffer + len, "ECThresh2           = %d\n", clst->egr_congst_thrsh_bytes2);
    len += sprintf(buffer + len, "ECThresh3           = %d\n", clst->egr_congst_thrsh_bytes3);
    len += sprintf(buffer + len, "ECThresh4           = %d\n", clst->egr_congst_thrsh_bytes4);
    len += sprintf(buffer + len, "ECThreshPkts1       = %d\n", clst->egr_congst_thrsh_pkts1);
    len += sprintf(buffer + len, "ECThreshPkts2       = %d\n", clst->egr_congst_thrsh_pkts2);
    len += sprintf(buffer + len, "ECThreshPkts3       = %d\n", clst->egr_congst_thrsh_pkts3);
    len += sprintf(buffer + len, "ECThreshPkts4       = %d\n", clst->egr_congst_thrsh_pkts4);

    *size = len;

    return PP_RC_SUCCESS;
}
/**************************************************************************************/
/*! \fn static int pp_hal_display_session_extended_info (Uint32 session_handle, Int8 *buffer, Int32 *size)
 **************************************************************************************
 *  \brief This function print all session_extended_info
 *  \return return the number of bytes wirtten to the buffer.
 **************************************************************************************/
AVALANCHE_PP_RET_e pp_hal_display_session_extended_info(Uint32 session_handle, Int8 *buffer, Int32 *size)
{
    Uint32 flags;
    volatile PP_HAL_SESSION_INFO_t       *   hal_session = &gPpHalSessionInfo[ session_handle ];
    volatile PP_HAL_SESSION_EGRESS_REC_t *   egress_rec  = &hal_session->EgressRecord;
    volatile Uint8 *newHeaderPtr;
    AVALANCHE_PP_SESSION_STATS_t stats;
    Uint32 len = 0;
    Bool   tdox = False;

    flags = hal_session->BaseRecord.StatusFlags;
    if ((flags & 0x1) == 0)
    {
        len += sprintf(buffer + len, "Session %d not valid\n", session_handle);
        *size = len;
        return PP_RC_SUCCESS;
    }
    len += sprintf(buffer + len, "\nSession Information\n");
    len += sprintf(buffer + len, "===================\n");
    len += sprintf(buffer + len, "\nBase Session Record [0x%08X]:\n", (Uint32)&hal_session->BaseRecord);
    len += sprintf(buffer + len, "--------------------------------\n");

    len += sprintf(buffer + len, "State               = %d [", hal_session->BaseRecord.State);
    switch (hal_session->BaseRecord.State)
    {
    case 0: len += sprintf(buffer + len, "Idle");             break;
    case 1: len += sprintf(buffer + len, "Need Synch");       break;
    case 2: len += sprintf(buffer + len, "Queueing");         break;
    case 3: len += sprintf(buffer + len, "Forwarding");       break;
    case 4: len += sprintf(buffer + len, "Diverting");        break;
    default:                            len += sprintf(buffer + len, "Unknown!!!");       break;
    }
    len += sprintf(buffer + len, "]\n");
    len += sprintf(buffer + len, "SynchQ              = %d\n", hal_session->BaseRecord.SynchQ);

    len += sprintf(buffer + len, "IngressVPID         = %d\n", hal_session->BaseRecord.IngressVPID);
    len += sprintf(buffer + len, "StatusFlags         = 0x%02X ", flags);
    if (flags)
    {
        len += sprintf(buffer + len, "[ ");
        if (flags & 0x1) {len += sprintf(buffer + len, "fSessValid ");}
        if (flags & 0x2) {len += sprintf(buffer + len, "fStatEvent ");}
        if (flags & 0x4) {len += sprintf(buffer + len, "fTimeout ");}
        len += sprintf(buffer + len, "]");
    }
    len += sprintf(buffer + len, "\n");

    flags = hal_session->BaseRecord.SessionFlags;
    len += sprintf(buffer + len, "SessionFlags        = 0x%04X ", flags);
    if (flags)
    {
        len += sprintf(buffer + len, "[ ");
        if (flags & TI_PP_SES_FLAG_IDLE_TMOUT)          {len += sprintf(buffer + len, "fIdleTime ");}
        if (flags & TI_PP_SES_FLAG_TCP_CONTROL)         {len += sprintf(buffer + len, "fDoTcpCtrl ");}
        if (flags & TI_PP_SES_FLAG_NO_INGRESS_STATS)    {len += sprintf(buffer + len, "fNoRxStats ");}
        if (flags & TI_PP_SES_FLAG_NO_EGRESS_STATS)     {len += sprintf(buffer + len, "fNoTxStats ");}
        if (flags & TI_PP_SES_FLAG_XLUDE_ETH_HDR_STATS) {len += sprintf(buffer + len, "fNoEthBytes ");}
        if (flags & TI_PP_SES_FLAG_UPDATE_TTL)          {len += sprintf(buffer + len, "fDecIpTtl ");}
        if (flags & TI_PP_SES_FLAG_PROC_TTL_EXP)        {len += sprintf(buffer + len, "fDoIpTtlExp ");}
        if (flags & TI_PP_SES_FLAG_DO_REASSEMBLY)       {len += sprintf(buffer + len, "fDoReassembly ");}
        if (flags & TI_PP_SES_FLAG_PROC_IP_OPTS)        {len += sprintf(buffer + len, "fDoIpOpt ");}
        if (flags & TI_PP_SES_TUNNEL)
        {
            len += sprintf(buffer + len, "fSesTunnel=");
            flags &= TI_PP_SES_FLAG_TUNNEL_MASK;
            if (flags == TI_PP_SES_FLAG_GRE_US)         {len += sprintf(buffer + len, "GreUs ");}
            else if (flags == TI_PP_SES_FLAG_GRE_DS)    {len += sprintf(buffer + len, "GreDs ");}
            else if (flags == TI_PP_SES_FLAG_DS_LITE_US)
            {
                len += sprintf(buffer + len, "DsLiteUs");
                if (hal_session->BaseRecord.SessionFlags & TI_PP_SES_FLAG_DSLITE_US_FRAG_IPv4)
                {
                    len += sprintf(buffer + len, "[fragIPv4] ");
                }
                else
                {
                    len += sprintf(buffer + len, "[fragIPv6] ");
                }
            }
            else if (flags == TI_PP_SES_FLAG_DS_LITE_DS){len += sprintf(buffer + len, "DsLiteDs ");}
            else                                        {len += sprintf(buffer + len, "Unknown ");}
        }
        len += sprintf(buffer + len, "]");
    }
    len += sprintf(buffer + len, "\n");
    len += sprintf(buffer + len, "TimeoutThresh       = %d usec\n", hal_session->BaseRecord.TimeoutThresh *10);
    len += sprintf(buffer + len, "RefernceTime        = 0x%08X\n", hal_session->BaseRecord.ReferenceTime);

    avalanche_pp_get_stats_session(session_handle, &stats);
    len += sprintf(buffer + len, "packets_forwarded   = %u\n", stats.packets_forwarded);
    len += sprintf(buffer + len, "bytes_forwarded     = %llu\n\n", stats.bytes_forwarded);
    


    len += sprintf(buffer + len, "Egress Record [0x%08X]:\n", (Uint32)egress_rec);
    len += sprintf(buffer + len, "--------------------------\n");                   

    len += sprintf(buffer + len, "Priority            = %d\n", egress_rec->Priority);
    len += sprintf(buffer + len, "EgressVPID          = %d\n", egress_rec->EgressVPID);
    flags = egress_rec->FramingCode;
    len += sprintf(buffer + len, "FramingCode         = 0x%02X ", flags);
    if (flags)
    {
        len += sprintf(buffer + len, "[ ");
        if (flags & TI_PP_EGR_FRM_STRIP_L2)         {len += sprintf(buffer + len, "fStripL2 ");}
        if (flags & TI_PP_EGR_FRM_PATCH_802_3)      {len += sprintf(buffer + len, "fPatch8023 ");}
        if (flags & TI_PP_EGR_FRM_TURBODOX_EN)      {len += sprintf(buffer + len, "fDoTurboDox "); tdox=True;}
        if (flags & TI_PP_EGR_FRM_PPPOE_HDR)        {len += sprintf(buffer + len, "fPatchPPPoE ");}
        if (flags & TI_PP_EGR_FRM_REFRAME_IP)       {len += sprintf(buffer + len, "fReframeIP ");}
        if (flags & TI_PP_EGR_FRM_TURBODOX_ADV_EN)  {len += sprintf(buffer + len, "fDoTurboDoxIgnoreTSOption ");}
        len += sprintf(buffer + len, "]");
    }
    len += sprintf(buffer + len, "\n");
    len += sprintf(buffer + len, "TxDestTag           = 0x%02X\n", egress_rec->TxDestTag);
    len += sprintf(buffer + len, "TxQueueBase         = %d [%s]\n",  egress_rec->TxQueueBase, PAL_CPPI41_GET_QNAME(egress_rec->TxQueueBase));
    flags = egress_rec->EgressFlags;
    len += sprintf(buffer + len, "EgressFlags         = 0x%02X ", flags);
    if (flags)
    {
        len += sprintf(buffer + len, "[ ");
        if (flags & TI_PP_EGR_FLAG_NEW_HEADER_HAS_IPV4) {len += sprintf(buffer + len, "flgEgrNewHeaderHasIpv4 ");}
        if (flags & TI_PP_EGR_FLAG_NEW_HEADER_HAS_IPV6) {len += sprintf(buffer + len, "flgEgrNewHeaderHasIpv6 ");}
        if (flags & TI_PP_EGR_FLAG_NEW_HEADER_PTR)      {len += sprintf(buffer + len, "flgEgrNewHeaderPtr ");}
        if (flags & TI_PP_EGR_FLAG_NEW_HEADER_INTERNAL) {len += sprintf(buffer + len, "flgEgrNewHeaderInternal ");}
        if (flags & TI_PP_EGR_FLAG_MOD_REC_VALID)       {len += sprintf(buffer + len, "flgEgrModRecValid ");}
        len += sprintf(buffer + len, "]");
    }
    len += sprintf(buffer + len, "\n");
    len += sprintf(buffer + len, "NextEgressRecIdx    = %d\n", egress_rec->NextEgressRecIdx);
    len += sprintf(buffer + len, "EgressPidType       = %d ", egress_rec->EgressPidType);
    switch (egress_rec->EgressPidType)
    {
    case TI_PP_PID_TYPE_UNDEFINED:          len += sprintf(buffer + len, "[PID_TYPE_UNDEFINED]\n");       break;
    case TI_PP_PID_TYPE_ETHERNET:           len += sprintf(buffer + len, "[PID_TYPE_ETHERNET]\n");        break;
    case TI_PP_PID_TYPE_INFRASTRUCTURE:     len += sprintf(buffer + len, "[PID_TYPE_INFRASTRUCTURE]\n");  break;
    case TI_PP_PID_TYPE_USBBULK:            len += sprintf(buffer + len, "[PID_TYPE_USB_RNDIS]\n");       break;
    case TI_PP_PID_TYPE_CDC:                len += sprintf(buffer + len, "[PID_TYPE_USB_CDC]\n");         break;
    case TI_PP_PID_TYPE_DOCSIS:             len += sprintf(buffer + len, "[PID_TYPE_DOCSIS]\n");          break;
    case TI_PP_PID_TYPE_ETHERNETSWITCH:     len += sprintf(buffer + len, "[PID_TYPE_ETHERNETSWITCH]\n");  break;
    default:                                len += sprintf(buffer + len, "[???]\n");                      break;
    }

    if (tdox)
    {
        len += sprintf(buffer + len, "UsTurboDoxAck       = %d\n", egress_rec->UsTurboDoxAck); 
    }
    if (egress_rec->TxQueueBase >= PAL_CPPI41_SR_DOCSIS_TX_QPDSP_QOS_Q_BASE && egress_rec->TxQueueBase <= PAL_CPPI41_SR_DOCSIS_TX_QPDSP_QOS_Q_LAST)
    {
        len += sprintf(buffer + len, "sf_index            = %d\n", egress_rec->psi.sf_index);
        len += sprintf(buffer + len, "phs                 = %d\n", egress_rec->psi.phs);
        len += sprintf(buffer + len, "tcp_flags           = %d\n", egress_rec->psi.tcp_flags);
        if (tdox)
        {
            len += sprintf(buffer + len, "tdox_id             = %d\n", egress_rec->psi.tdox_id);
        }
    }
    
    if (egress_rec->UsPayloadLenOff)
    {
        len += sprintf(buffer + len, "L3 'TotalLen' offst = %d\n", egress_rec->UsPayloadLenOff);
    }
    if (egress_rec->EgressFlags & TI_PP_EGR_FLAG_MOD_REC_VALID)
    {
        volatile PP_HAL_MODIFICATION_REC_t *   pkt_mod  = &hal_session->ModificationRecord;

        len += sprintf(buffer + len, "\n");
        len += sprintf(buffer + len, "Modification Record [0x%08X]:\n", (Uint32)pkt_mod);
        len += sprintf(buffer + len, "--------------------------------\n");                   

        flags = pkt_mod->ModFlags;
        len += sprintf(buffer + len, "Flags               = 0x%04X\n", flags);
        if (flags & TI_PP_MOD_IPSRC_VALID)      {len += sprintf(buffer + len, "New IPSrc           = 0x%08X\n", pkt_mod->IpSrc);}
        if (flags & TI_PP_MOD_IPDST_VALID)      {len += sprintf(buffer + len, "New IPDst           = 0x%08X\n", pkt_mod->IpDst);}
        if (flags & TI_PP_MOD_L3CHK_VALID)      {len += sprintf(buffer + len, "L3ChecksumDelta     = 0x%04X\n", pkt_mod->L3ChecksumDelta);}
        if (flags & TI_PP_MOD_SRCPORT_VALID)    {len += sprintf(buffer + len, "New PortSrc         = 0x%04X\n", pkt_mod->PortSrc);}
        if (flags & TI_PP_MOD_DSTPORT_VALID)    {len += sprintf(buffer + len, "New DstSrc          = 0x%04X\n", pkt_mod->PortDst);}
        if (flags & TI_PP_MOD_L4CHK_VALID)      {len += sprintf(buffer + len, "L4ChecksumDelta     = 0x%04X\n", pkt_mod->L4ChecksumDelta);}
        if (flags & TI_PP_MOD_IPTOS_VALID)      {len += sprintf(buffer + len, "New IP TOS          = 0x%02X\n", pkt_mod->Tos);}
    }

    len += sprintf(buffer + len, "\n");

    if(egress_rec->NewHeaderSize == 0)
    {
        len += sprintf(buffer + len, "\n");
        *size = len;
        return PP_RC_SUCCESS;
    }

    if(egress_rec->EgressFlags & TI_PP_EGR_FLAG_NEW_HEADER_PTR) 
    {
        if(egress_rec->EgressFlags & TI_PP_EGR_FLAG_NEW_HEADER_INTERNAL)
        {
            newHeaderPtr = IO_PHY2VIRT((void *)*(Uint32 *)hal_session->NewHeader);
        }
        else
        {
            newHeaderPtr = (Uint8*)*(Uint32*)hal_session->NewHeader;
        }
    }
    else
    {
        newHeaderPtr = hal_session->NewHeader;
    }

    if(egress_rec->NewHeaderSize)
    {
        int i, j;
        len += sprintf(buffer + len, "NewHeader [0x%08X]:\n", (Uint32)newHeaderPtr);
        len += sprintf(buffer + len, "----------------------\n");
        len += sprintf(buffer + len, "NewHeaderSize       = %d\n", egress_rec->NewHeaderSize);
        for (i = 0, j = 0; i < egress_rec->NewHeaderSize; i += 4, j++)
        {
            len += sprintf(buffer + len, "%08X.", *((Uint32*)(newHeaderPtr) + j));
        }
        len += sprintf(buffer + len, "%c\n\n", 8);
    }

    *size = len;

    return PP_RC_SUCCESS;
}



AVALANCHE_PP_RET_e      pp_hal_init( void )
{
    gPpHalSessionInfo = (PP_HAL_SESSION_INFO_t *) IO_PHY2VIRT( PP_HAL_SESSION_INFO_BASE_PHY );

    __pp_hal_init_pool_mcast();
    __pp_hal_init_pool_ext_headers();

    return (PP_RC_SUCCESS);
}
