/*
  GPL LICENSE SUMMARY

  Copyright(c) 2011 Intel Corporation.

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
#include <pal.h>
#include <puma6.h>
#include <puma6_cppi.h>
#include <linux/skbuff.h>
#include <linux/proc_fs.h>
#include <asm-arm/arch-avalanche/puma6/sram_api.h>

// #define DOCSIS_CPPI_DEBUG

#ifdef DOCSIS_CPPI_DEBUG
/* note: prints function name for you */
#  define DPRINTK(fmt, args...) printk("%s:%d " fmt, __FUNCTION__ , __LINE__ , ## args)
#else
#  define DPRINTK(fmt, args...)
#endif

static PAL_Result puma6_cppi_docsis_us_proc_init (Ptr hnd, Ptr param);
static PAL_Result puma6_cppi_docsis_ds0_proc_init (Ptr hnd, Ptr param);
static PAL_Result puma6_cppi_docsis_ds1_proc_init (Ptr hnd, Ptr param);


typedef struct channel_info_t
{
    int         dmaChannel;
    int         dmaBlock;
    Cppi4Queue  outputQueue;
    Cppi4Queue  freeQueue;
    Uint32      bufPoolNum_s0;
    Uint32      bufPoolNum_s1;
    Uint32      bufPoolNum_s2;
    Uint32      bufPoolNum_s3;
}
channel_info_t;

typedef struct
{
    Uint32  numDesc;
    Uint32  qNum;

    Ptr     firstDescPtr;
    Uint32  buffSize;

    /* Desc config for the  group */
    Uint32  pktType;
} BDBlkInfo;


typedef struct
{
    Ptr         buffDescRegionPtr;
    Uint32      qMgr;
    Uint32      numDesc;
    Uint32      szDesc;
    Uint32      numBlks;
    BDBlkInfo   BDBlk [PAL_CPPI41_SR_FD_HOST_Q_LAST - PAL_CPPI41_SR_FD_HOST_Q_BASE];
} HostBDCfg;


HostBDCfg docsisHostBDCfg_g =
{
    .qMgr       = PAL_CPPI41_QUEUE_MGR_PARTITION_SR,
    .numDesc    = PAL_CPPI41_SR_CNI_INFRA_FD_HOST_DESC_COUNT,   /* Must be same as in Cppi Cgf structure */
    .szDesc     = PAL_CPPI41_SR_CNI_INFRA_FD_HOST_DESC_SIZE,    /* Must be same as for specific region   */
    .numBlks    = PAL_CPPI41_SR_CNI_INFRA_DMA_CH_COUNT,         /* Change for each block added/removed   */

    /* Host2PP Low */
    .BDBlk[0].numDesc   = PAL_CPPI41_SR_CNI_INFRA_LOW_FD_HOST_DESC_COUNT,
    .BDBlk[0].qNum      = PAL_CPPI41_SR_CNI_INFRA_LOW_FD_HOST_Q_NUM,
    .BDBlk[0].pktType   = PAL_CPPI4_HOSTDESC_PKT_TYPE_ETH,
    .BDBlk[0].buffSize  = PAL_CPPI41_SR_CNI_INFRA_FD_HOST_BUFFER_SIZE,

    /* Host2PP High */
    .BDBlk[1].numDesc   = PAL_CPPI41_SR_CNI_INFRA_HIGH_FD_HOST_DESC_COUNT,
    .BDBlk[1].qNum      = PAL_CPPI41_SR_CNI_INFRA_HIGH_FD_HOST_Q_NUM,
    .BDBlk[1].pktType   = PAL_CPPI4_HOSTDESC_PKT_TYPE_ETH,
    .BDBlk[1].buffSize  = PAL_CPPI41_SR_CNI_INFRA_FD_HOST_BUFFER_SIZE,
};


/************************************************************************/
/*      PP -> HOST  CNI Infrastructure channels information             */
/************************************************************************/
static  channel_info_t  rxInfra[PAL_CPPI41_SR_CNI_INFRA_DMA_CH_COUNT]=
{
    {
        .outputQueue.qMgr   = PAL_CPPI41_QUEUE_MGR_PARTITION_SR,
        .outputQueue.qNum   = PAL_CPPI41_SR_CNI_LOW_HOST_RX_Q_NUM,
        .freeQueue.qMgr     = PAL_CPPI41_QUEUE_MGR_PARTITION_SR,
        .freeQueue.qNum     = PAL_CPPI41_SR_CNI_INFRA_LOW_FD_HOST_Q_NUM,
        .dmaChannel         = PAL_CPPI41_SR_CNI_INFRA_DMA_CH_NUM(0),
        .dmaBlock           = PAL_CPPI41_DMA_BLOCK2,
    },
    {
        .outputQueue.qMgr   = PAL_CPPI41_QUEUE_MGR_PARTITION_SR,
        .outputQueue.qNum   = PAL_CPPI41_SR_CNI_HIGH_HOST_RX_Q_NUM,
        .freeQueue.qMgr     = PAL_CPPI41_QUEUE_MGR_PARTITION_SR,
        .freeQueue.qNum     = PAL_CPPI41_SR_CNI_INFRA_HIGH_FD_HOST_Q_NUM,
        .dmaChannel         = PAL_CPPI41_SR_CNI_INFRA_DMA_CH_NUM(1),
        .dmaBlock           = PAL_CPPI41_DMA_BLOCK2,
    }
};
/************************************************************************/



/************************************************************************/
/*      DOCSIS -> PP channels information                               */
/************************************************************************/
static  channel_info_t  dsCopDmaChannels[PAL_CPPI41_SR_DOCSIS_DS_CoP_DATA_NUM_RX_CH]=
{
    /*  === DOCSIS RX Low 256B non MC even DS CH === */
    {
        .outputQueue.qNum   = PAL_CPPI41_SR_PPDSP_LOW_Q_NUM,
        .freeQueue.qNum     = PAL_CPPI41_SR_DOCSIS_RX_CoP_LOW_FD_EMB_Q_NUM,
        .dmaChannel         = PAL_CPPI41_SR_DOCSIS_DS_CoP_LOW_256B_NONMC_EVEN_DMA01_RX_CH_NUM,
        .bufPoolNum_s0      = PAL_CPPI41_BMGR_POOL2
    },

    /*  === DOCSIS RX Low 256B non MC odd DS CH === */
    {
        .outputQueue.qNum   = PAL_CPPI41_SR_PPDSP_LOW_Q_NUM,
        .freeQueue.qNum     = PAL_CPPI41_SR_DOCSIS_RX_CoP_LOW_FD_EMB_Q_NUM,
        .dmaChannel         = PAL_CPPI41_SR_DOCSIS_DS_CoP_LOW_256B_NONMC_ODD_DMA01_RX_CH_NUM,
        .bufPoolNum_s0      = PAL_CPPI41_BMGR_POOL3
    },

    /*  === DOCSIS RX Low 512B non MC === */
    {
        .outputQueue.qNum   = PAL_CPPI41_SR_PPDSP_LOW_Q_NUM,
        .freeQueue.qNum     = PAL_CPPI41_SR_DOCSIS_RX_CoP_LOW_FD_EMB_Q_NUM,
        .dmaChannel         = PAL_CPPI41_SR_DOCSIS_DS_CoP_LOW_512B_NONMC_DMA01_RX_CH_NUM,
        .bufPoolNum_s0      = PAL_CPPI41_BMGR_POOL4
    },

    /*  === DOCSIS RX Low 2KB non MC === */
    {
        .outputQueue.qNum   = PAL_CPPI41_SR_PPDSP_LOW_Q_NUM,
        .freeQueue.qNum     = PAL_CPPI41_SR_DOCSIS_RX_CoP_LOW_FD_EMB_Q_NUM,
        .dmaChannel         = PAL_CPPI41_SR_DOCSIS_DS_CoP_LOW_2KB_NONMC_DMA01_RX_CH_NUM,
        .bufPoolNum_s0      = PAL_CPPI41_BMGR_POOL5
    },

    /*  === DOCSIS RX Low 2KB MC === */
    {
        .outputQueue.qNum   = PAL_CPPI41_SR_PPDSP_LOW_Q_NUM,
        .freeQueue.qNum     = PAL_CPPI41_SR_DOCSIS_RX_CoP_LOW_FD_EMB_Q_NUM,
        .dmaChannel         = PAL_CPPI41_SR_DOCSIS_DS_CoP_LOW_2KB_MC_DMA01_RX_CH_NUM,
        .bufPoolNum_s0      = PAL_CPPI41_BMGR_POOL6
    },

    /*  === DOCSIS RX High === */
    {
        .outputQueue.qNum   = PAL_CPPI41_SR_PPDSP_HIGH_Q_NUM,
        .freeQueue.qNum     = PAL_CPPI41_SR_DOCSIS_RX_CoP_HIGH_FD_EMB_Q_NUM,
        .dmaChannel         = PAL_CPPI41_SR_DOCSIS_DS_CoP_HIGH_DMA01_RX_CH_NUM,
        .bufPoolNum_s0      = PAL_CPPI41_BMGR_POOL7
    },

    /*  === DOCSIS RX Voice === */
    {
        .outputQueue.qNum   = PAL_CPPI41_SR_PPDSP_HIGH_Q_NUM,
        .freeQueue.qNum     = PAL_CPPI41_SR_DOCSIS_RX_VOICE_FD_EMB_Q_NUM,
        .dmaChannel         = PAL_CPPI41_SR_DOCSIS_DS_CoP_VOICE_DMA01_RX_MAX_CH,
        .bufPoolNum_s0      = PAL_CPPI41_BMGR_POOL8
    },

};


/************************************************************************/



/***************************************************************************************************************/
/***************************************************************************************************************/
/***************************************************************************************************************/
/***************************************************************************************************************/
/*                                     Internal DDOCSIS CPPI code                                              */
/***************************************************************************************************************/
/***************************************************************************************************************/
/***************************************************************************************************************/
/***************************************************************************************************************/
static Cppi4InitCfg cppi4_DSG0_InitCfg_g =
{
    .resetLine              = 0,

    /************************************************************************/
    /*                                                                      */
    /************************************************************************/

    .queueMgrInfo[PAL_CPPI41_QUEUE_MGR_PARTITION_DOCSIS_DS_GROUP].queueMgrRgnBase        = (Ptr) AVALANCHE_DOCSIS_SS_DS_GROUP0_QMGR_RGN_BASE,
    .queueMgrInfo[PAL_CPPI41_QUEUE_MGR_PARTITION_DOCSIS_DS_GROUP].descMemRgnBase         = (Ptr) AVALANCHE_DOCSIS_SS_DS_GROUP0_DESCMEM_RGN_BASE,
    .queueMgrInfo[PAL_CPPI41_QUEUE_MGR_PARTITION_DOCSIS_DS_GROUP].queueMgmtRgnBase       = (Ptr) AVALANCHE_DOCSIS_SS_DS_GROUP0_QMGMT_RGN_BASE,
    .queueMgrInfo[PAL_CPPI41_QUEUE_MGR_PARTITION_DOCSIS_DS_GROUP].queueStatusRgnBase     = (Ptr) AVALANCHE_DOCSIS_SS_DS_GROUP0_QSTATUS_RGN_BASE,
    .queueMgrInfo[PAL_CPPI41_QUEUE_MGR_PARTITION_DOCSIS_DS_GROUP].totalQNum              = PAL_CPPI41_DOCSIS_DS_QMGR_TOTAL_Q_COUNT,

    .queueMgrInfo[PAL_CPPI41_QUEUE_MGR_PARTITION_DOCSIS_DS_GROUP].LinkingRAM0Base        = IO_VIRT2PHY_DSGx( AVALANCHE_DOCSIS_SS_DS_GROUP0_LINKING_RAM_BASE ),
    .queueMgrInfo[PAL_CPPI41_QUEUE_MGR_PARTITION_DOCSIS_DS_GROUP].LinkingRAM0Size        = 128,
    .queueMgrInfo[PAL_CPPI41_QUEUE_MGR_PARTITION_DOCSIS_DS_GROUP].LinkingRAM1Base        = 0,

                                                                                 /* Docsis MAC DS internal memory */
    .queueMgrInfo[PAL_CPPI41_QUEUE_MGR_PARTITION_DOCSIS_DS_GROUP].descRegion[0].base     = (Ptr) IO_VIRT2PHY_DSGx( AVALANCHE_DOCSIS_SS_DS_GROUP0_PACKET_RAM_BASE ),
    .queueMgrInfo[PAL_CPPI41_QUEUE_MGR_PARTITION_DOCSIS_DS_GROUP].descRegion[0].szDesc   = PAL_CPPI41_DOCSIS_DS_FD_EMB_DESC_SIZE,
    .queueMgrInfo[PAL_CPPI41_QUEUE_MGR_PARTITION_DOCSIS_DS_GROUP].descRegion[0].numDesc  = PAL_CPPI41_DOCSIS_DS_FD_EMB_DESC_COUNT,
    .queueMgrInfo[PAL_CPPI41_QUEUE_MGR_PARTITION_DOCSIS_DS_GROUP].descRegion[0].isOnChip = 1,

    .queueMgrInfo[PAL_CPPI41_QUEUE_MGR_PARTITION_DOCSIS_DS_GROUP].descRegion[1].base     = 0,
    .queueMgrInfo[PAL_CPPI41_QUEUE_MGR_PARTITION_DOCSIS_DS_GROUP].descRegion[1].szDesc   = sizeof(Cppi4TeardownDesc),    /* 32 byte sized        */
    .queueMgrInfo[PAL_CPPI41_QUEUE_MGR_PARTITION_DOCSIS_DS_GROUP].descRegion[1].numDesc  = PAL_CPPI41_NUM_TD_DESC,       /* Have to serve 1 DMA  */

    /************************************************************************/
    /*                                                                      */
    /************************************************************************/

    .bufMgrBase[PAL_CPPI41_BUF_MGR_PARTITION_DOCSIS_DS_GROUP]    = (CSL_BufMgr_RegsOvly) AVALANCHE_DOCSIS_SS_DS_GROUP0_BMGR_BASE,

    /************************************************************************/
    /*                                                                      */
    /************************************************************************/

    .dmaBlock[PAL_CPPI41_DMA_BLOCK0].globalCtrlBase     = (Ptr) AVALANCHE_DOCSIS_SS_DS_GROUP0_DMA_GBLCFG_BASE,
    .dmaBlock[PAL_CPPI41_DMA_BLOCK0].chCtrlStatusBase   = (Ptr) AVALANCHE_DOCSIS_SS_DS_GROUP0_DMA_CHNCFG_BASE,
    .dmaBlock[PAL_CPPI41_DMA_BLOCK0].schedCtrlBase      = (Ptr) AVALANCHE_DOCSIS_SS_DS_GROUP0_DMA_SCHEDCFG_BASE,
    .dmaBlock[PAL_CPPI41_DMA_BLOCK0].schedTableBase     = (Ptr) AVALANCHE_DOCSIS_SS_DS_GROUP0_DMA_SCHEDTBL_BASE,

    .dmaBlock[PAL_CPPI41_DMA_BLOCK0].tdFQueue.qMgr      = 0,
    .dmaBlock[PAL_CPPI41_DMA_BLOCK0].tdFQueue.qNum      = 59,

    .dmaBlock[PAL_CPPI41_DMA_BLOCK0].schedTable.numEntries  = 32,
    .dmaBlock[PAL_CPPI41_DMA_BLOCK0].schedTable.entry =
    {
        PAL_CPPI41_DMA_CH_CONFIG( PAL_CPPI41_DMA_TX_CH, 16 ),
        PAL_CPPI41_DMA_CH_CONFIG( PAL_CPPI41_DMA_RX_CH, 0 ),
        PAL_CPPI41_DMA_CH_CONFIG( PAL_CPPI41_DMA_TX_CH, 16 ),
        PAL_CPPI41_DMA_CH_CONFIG( PAL_CPPI41_DMA_RX_CH, 1 ),
        PAL_CPPI41_DMA_CH_CONFIG( PAL_CPPI41_DMA_TX_CH, 16 ),
        PAL_CPPI41_DMA_CH_CONFIG( PAL_CPPI41_DMA_RX_CH, 2 ),
        PAL_CPPI41_DMA_CH_CONFIG( PAL_CPPI41_DMA_TX_CH, 16 ),
        PAL_CPPI41_DMA_CH_CONFIG( PAL_CPPI41_DMA_RX_CH, 3 ),
        PAL_CPPI41_DMA_CH_CONFIG( PAL_CPPI41_DMA_TX_CH, 16 ),
        PAL_CPPI41_DMA_CH_CONFIG( PAL_CPPI41_DMA_RX_CH, 4 ),
        PAL_CPPI41_DMA_CH_CONFIG( PAL_CPPI41_DMA_TX_CH, 16 ),
        PAL_CPPI41_DMA_CH_CONFIG( PAL_CPPI41_DMA_RX_CH, 5 ),
        PAL_CPPI41_DMA_CH_CONFIG( PAL_CPPI41_DMA_TX_CH, 16 ),
        PAL_CPPI41_DMA_CH_CONFIG( PAL_CPPI41_DMA_RX_CH, 6 ),
        PAL_CPPI41_DMA_CH_CONFIG( PAL_CPPI41_DMA_TX_CH, 16 ),
        PAL_CPPI41_DMA_CH_CONFIG( PAL_CPPI41_DMA_RX_CH, 7 ),
        PAL_CPPI41_DMA_CH_CONFIG( PAL_CPPI41_DMA_TX_CH, 16 ),
        PAL_CPPI41_DMA_CH_CONFIG( PAL_CPPI41_DMA_RX_CH, 8 ),
        PAL_CPPI41_DMA_CH_CONFIG( PAL_CPPI41_DMA_TX_CH, 16 ),
        PAL_CPPI41_DMA_CH_CONFIG( PAL_CPPI41_DMA_RX_CH, 9 ),
        PAL_CPPI41_DMA_CH_CONFIG( PAL_CPPI41_DMA_TX_CH, 16 ),
        PAL_CPPI41_DMA_CH_CONFIG( PAL_CPPI41_DMA_RX_CH, 10 ),
        PAL_CPPI41_DMA_CH_CONFIG( PAL_CPPI41_DMA_TX_CH, 16 ),
        PAL_CPPI41_DMA_CH_CONFIG( PAL_CPPI41_DMA_RX_CH, 11 ),
        PAL_CPPI41_DMA_CH_CONFIG( PAL_CPPI41_DMA_TX_CH, 16 ),
        PAL_CPPI41_DMA_CH_CONFIG( PAL_CPPI41_DMA_RX_CH, 12 ),
        PAL_CPPI41_DMA_CH_CONFIG( PAL_CPPI41_DMA_TX_CH, 16 ),
        PAL_CPPI41_DMA_CH_CONFIG( PAL_CPPI41_DMA_RX_CH, 13 ),
        PAL_CPPI41_DMA_CH_CONFIG( PAL_CPPI41_DMA_TX_CH, 16 ),
        PAL_CPPI41_DMA_CH_CONFIG( PAL_CPPI41_DMA_RX_CH, 14 ),
        PAL_CPPI41_DMA_CH_CONFIG( PAL_CPPI41_DMA_TX_CH, 16 ),
        PAL_CPPI41_DMA_CH_CONFIG( PAL_CPPI41_DMA_RX_CH, 15 ),
    },

    .debugToolBind = puma6_cppi_docsis_ds0_proc_init,
};

static Cppi4InitCfg cppi4_DSG1_InitCfg_g =
{
    .resetLine              = 0,

    /************************************************************************/
    /*                                                                      */
    /************************************************************************/

    .queueMgrInfo[PAL_CPPI41_QUEUE_MGR_PARTITION_DOCSIS_DS_GROUP].queueMgrRgnBase        = (Ptr) AVALANCHE_DOCSIS_SS_DS_GROUP1_QMGR_RGN_BASE,
    .queueMgrInfo[PAL_CPPI41_QUEUE_MGR_PARTITION_DOCSIS_DS_GROUP].descMemRgnBase         = (Ptr) AVALANCHE_DOCSIS_SS_DS_GROUP1_DESCMEM_RGN_BASE,
    .queueMgrInfo[PAL_CPPI41_QUEUE_MGR_PARTITION_DOCSIS_DS_GROUP].queueMgmtRgnBase       = (Ptr) AVALANCHE_DOCSIS_SS_DS_GROUP1_QMGMT_RGN_BASE,
    .queueMgrInfo[PAL_CPPI41_QUEUE_MGR_PARTITION_DOCSIS_DS_GROUP].queueStatusRgnBase     = (Ptr) AVALANCHE_DOCSIS_SS_DS_GROUP1_QSTATUS_RGN_BASE,
    .queueMgrInfo[PAL_CPPI41_QUEUE_MGR_PARTITION_DOCSIS_DS_GROUP].totalQNum              = PAL_CPPI41_DOCSIS_DS_QMGR_TOTAL_Q_COUNT,

    .queueMgrInfo[PAL_CPPI41_QUEUE_MGR_PARTITION_DOCSIS_DS_GROUP].LinkingRAM0Base        = IO_VIRT2PHY_DSGx( AVALANCHE_DOCSIS_SS_DS_GROUP1_LINKING_RAM_BASE ),
    .queueMgrInfo[PAL_CPPI41_QUEUE_MGR_PARTITION_DOCSIS_DS_GROUP].LinkingRAM0Size        = 128,
    .queueMgrInfo[PAL_CPPI41_QUEUE_MGR_PARTITION_DOCSIS_DS_GROUP].LinkingRAM1Base        = 0,

                                                                                 /* Docsis MAC DS internal memory */
    .queueMgrInfo[PAL_CPPI41_QUEUE_MGR_PARTITION_DOCSIS_DS_GROUP].descRegion[0].base     = (Ptr) IO_VIRT2PHY_DSGx( AVALANCHE_DOCSIS_SS_DS_GROUP1_PACKET_RAM_BASE ),
    .queueMgrInfo[PAL_CPPI41_QUEUE_MGR_PARTITION_DOCSIS_DS_GROUP].descRegion[0].szDesc   = PAL_CPPI41_DOCSIS_DS_FD_EMB_DESC_SIZE,
    .queueMgrInfo[PAL_CPPI41_QUEUE_MGR_PARTITION_DOCSIS_DS_GROUP].descRegion[0].numDesc  = PAL_CPPI41_DOCSIS_DS_FD_EMB_DESC_COUNT,
    .queueMgrInfo[PAL_CPPI41_QUEUE_MGR_PARTITION_DOCSIS_DS_GROUP].descRegion[0].isOnChip = 1,

    .queueMgrInfo[PAL_CPPI41_QUEUE_MGR_PARTITION_DOCSIS_DS_GROUP].descRegion[1].base     = 0,
    .queueMgrInfo[PAL_CPPI41_QUEUE_MGR_PARTITION_DOCSIS_DS_GROUP].descRegion[1].szDesc   = sizeof(Cppi4TeardownDesc),    /* 32 byte sized        */
    .queueMgrInfo[PAL_CPPI41_QUEUE_MGR_PARTITION_DOCSIS_DS_GROUP].descRegion[1].numDesc  = PAL_CPPI41_NUM_TD_DESC,       /* Have to serve 1 DMA  */

    /************************************************************************/
    /*                                                                      */
    /************************************************************************/

    .bufMgrBase[PAL_CPPI41_BUF_MGR_PARTITION_DOCSIS_DS_GROUP]    = (CSL_BufMgr_RegsOvly) AVALANCHE_DOCSIS_SS_DS_GROUP1_BMGR_BASE,

    /************************************************************************/
    /*                                                                      */
    /************************************************************************/

    .dmaBlock[PAL_CPPI41_DMA_BLOCK0].globalCtrlBase     = (Ptr) AVALANCHE_DOCSIS_SS_DS_GROUP1_DMA_GBLCFG_BASE,
    .dmaBlock[PAL_CPPI41_DMA_BLOCK0].chCtrlStatusBase   = (Ptr) AVALANCHE_DOCSIS_SS_DS_GROUP1_DMA_CHNCFG_BASE,
    .dmaBlock[PAL_CPPI41_DMA_BLOCK0].schedCtrlBase      = (Ptr) AVALANCHE_DOCSIS_SS_DS_GROUP1_DMA_SCHEDCFG_BASE,
    .dmaBlock[PAL_CPPI41_DMA_BLOCK0].schedTableBase     = (Ptr) AVALANCHE_DOCSIS_SS_DS_GROUP1_DMA_SCHEDTBL_BASE,

    .dmaBlock[PAL_CPPI41_DMA_BLOCK0].tdFQueue.qMgr      = 0,
    .dmaBlock[PAL_CPPI41_DMA_BLOCK0].tdFQueue.qNum      = 59,

    .dmaBlock[PAL_CPPI41_DMA_BLOCK0].schedTable.numEntries  = 32,
    .dmaBlock[PAL_CPPI41_DMA_BLOCK0].schedTable.entry =
    {
        PAL_CPPI41_DMA_CH_CONFIG( PAL_CPPI41_DMA_TX_CH, 16 ),
        PAL_CPPI41_DMA_CH_CONFIG( PAL_CPPI41_DMA_RX_CH, 0 ),
        PAL_CPPI41_DMA_CH_CONFIG( PAL_CPPI41_DMA_TX_CH, 16 ),
        PAL_CPPI41_DMA_CH_CONFIG( PAL_CPPI41_DMA_RX_CH, 1 ),
        PAL_CPPI41_DMA_CH_CONFIG( PAL_CPPI41_DMA_TX_CH, 16 ),
        PAL_CPPI41_DMA_CH_CONFIG( PAL_CPPI41_DMA_RX_CH, 2 ),
        PAL_CPPI41_DMA_CH_CONFIG( PAL_CPPI41_DMA_TX_CH, 16 ),
        PAL_CPPI41_DMA_CH_CONFIG( PAL_CPPI41_DMA_RX_CH, 3 ),
        PAL_CPPI41_DMA_CH_CONFIG( PAL_CPPI41_DMA_TX_CH, 16 ),
        PAL_CPPI41_DMA_CH_CONFIG( PAL_CPPI41_DMA_RX_CH, 4 ),
        PAL_CPPI41_DMA_CH_CONFIG( PAL_CPPI41_DMA_TX_CH, 16 ),
        PAL_CPPI41_DMA_CH_CONFIG( PAL_CPPI41_DMA_RX_CH, 5 ),
        PAL_CPPI41_DMA_CH_CONFIG( PAL_CPPI41_DMA_TX_CH, 16 ),
        PAL_CPPI41_DMA_CH_CONFIG( PAL_CPPI41_DMA_RX_CH, 6 ),
        PAL_CPPI41_DMA_CH_CONFIG( PAL_CPPI41_DMA_TX_CH, 16 ),
        PAL_CPPI41_DMA_CH_CONFIG( PAL_CPPI41_DMA_RX_CH, 7 ),
        PAL_CPPI41_DMA_CH_CONFIG( PAL_CPPI41_DMA_TX_CH, 16 ),
        PAL_CPPI41_DMA_CH_CONFIG( PAL_CPPI41_DMA_RX_CH, 8 ),
        PAL_CPPI41_DMA_CH_CONFIG( PAL_CPPI41_DMA_TX_CH, 16 ),
        PAL_CPPI41_DMA_CH_CONFIG( PAL_CPPI41_DMA_RX_CH, 9 ),
        PAL_CPPI41_DMA_CH_CONFIG( PAL_CPPI41_DMA_TX_CH, 16 ),
        PAL_CPPI41_DMA_CH_CONFIG( PAL_CPPI41_DMA_RX_CH, 10 ),
        PAL_CPPI41_DMA_CH_CONFIG( PAL_CPPI41_DMA_TX_CH, 16 ),
        PAL_CPPI41_DMA_CH_CONFIG( PAL_CPPI41_DMA_RX_CH, 11 ),
        PAL_CPPI41_DMA_CH_CONFIG( PAL_CPPI41_DMA_TX_CH, 16 ),
        PAL_CPPI41_DMA_CH_CONFIG( PAL_CPPI41_DMA_RX_CH, 12 ),
        PAL_CPPI41_DMA_CH_CONFIG( PAL_CPPI41_DMA_TX_CH, 16 ),
        PAL_CPPI41_DMA_CH_CONFIG( PAL_CPPI41_DMA_RX_CH, 13 ),
        PAL_CPPI41_DMA_CH_CONFIG( PAL_CPPI41_DMA_TX_CH, 16 ),
        PAL_CPPI41_DMA_CH_CONFIG( PAL_CPPI41_DMA_RX_CH, 14 ),
        PAL_CPPI41_DMA_CH_CONFIG( PAL_CPPI41_DMA_TX_CH, 16 ),
        PAL_CPPI41_DMA_CH_CONFIG( PAL_CPPI41_DMA_RX_CH, 15 ),
    },

    .debugToolBind = puma6_cppi_docsis_ds1_proc_init,

};



static Cppi4InitCfg cppi4_DOCSIS_US_InitCfg_g =
{
    .resetLine              = 0,

    /************************************************************************/
    /*                                                                      */
    /************************************************************************/

    .queueMgrInfo[PAL_CPPI41_QUEUE_MGR_PARTITION_DOCSIS].queueMgrRgnBase        = (Ptr) AVALANCHE_DOCSIS_SS_QMGR_RGN_BASE,
    .queueMgrInfo[PAL_CPPI41_QUEUE_MGR_PARTITION_DOCSIS].descMemRgnBase         = (Ptr) AVALANCHE_DOCSIS_SS_DESCMEM_RGN_BASE,
    .queueMgrInfo[PAL_CPPI41_QUEUE_MGR_PARTITION_DOCSIS].queueMgmtRgnBase       = (Ptr) AVALANCHE_DOCSIS_SS_QMGMT_RGN_BASE,
    .queueMgrInfo[PAL_CPPI41_QUEUE_MGR_PARTITION_DOCSIS].queueStatusRgnBase     = (Ptr) AVALANCHE_DOCSIS_SS_QSTATUS_RGN_BASE,
    .queueMgrInfo[PAL_CPPI41_QUEUE_MGR_PARTITION_DOCSIS].totalQNum              = PAL_CPPI41_DOCSIS_US_QMGR_TOTAL_Q_COUNT,

    .queueMgrInfo[PAL_CPPI41_QUEUE_MGR_PARTITION_DOCSIS].LinkingRAM0Base        = IO_VIRT2PHY( AVALANCHE_DOCSIS_SS_LINKING_RAM_BASE ),
    .queueMgrInfo[PAL_CPPI41_QUEUE_MGR_PARTITION_DOCSIS].LinkingRAM0Size        = AVALANCHE_DOCSIS_SS_LINKING_RAM_MAX_ENTRIES,
    .queueMgrInfo[PAL_CPPI41_QUEUE_MGR_PARTITION_DOCSIS].LinkingRAM1Base        = 0,



    .queueMgrInfo[PAL_CPPI41_QUEUE_MGR_PARTITION_DOCSIS].descRegion[0].base     = (Ptr) IO_VIRT2PHY( AVALANCHE_DOCSIS_SS_COP_US_PACKET_RAM_BASE ),
    .queueMgrInfo[PAL_CPPI41_QUEUE_MGR_PARTITION_DOCSIS].descRegion[0].szDesc   = PAL_CPPI41_DOCSIS_US_FD_EMB_DESC_SIZE,
    .queueMgrInfo[PAL_CPPI41_QUEUE_MGR_PARTITION_DOCSIS].descRegion[0].numDesc  = PAL_CPPI41_DOCSIS_US_FD_EMB_DESC_COUNT,
    .queueMgrInfo[PAL_CPPI41_QUEUE_MGR_PARTITION_DOCSIS].descRegion[0].isOnChip = 1,
                                                                                  /* Docsis MAC US internal memory */
    .queueMgrInfo[PAL_CPPI41_QUEUE_MGR_PARTITION_DOCSIS].descRegion[1].base     = (Ptr) IO_VIRT2PHY( AVALANCHE_DOCSIS_SS_US_PACKET_RAM_BASE ),
    .queueMgrInfo[PAL_CPPI41_QUEUE_MGR_PARTITION_DOCSIS].descRegion[1].szDesc   = 128,
    .queueMgrInfo[PAL_CPPI41_QUEUE_MGR_PARTITION_DOCSIS].descRegion[1].numDesc  = 32,
    .queueMgrInfo[PAL_CPPI41_QUEUE_MGR_PARTITION_DOCSIS].descRegion[1].isOnChip = 1,



    /************************************************************************/
    /*                                                                      */
    /************************************************************************/

    .bufMgrBase[PAL_CPPI41_BUF_MGR_PARTITION_DOCSIS]    = (CSL_BufMgr_RegsOvly) AVALANCHE_DOCSIS_SS_BMGR_BASE,

    /************************************************************************/
    /*                                                                      */
    /************************************************************************/

    .dmaBlock[PAL_CPPI41_DMA_BLOCK0].globalCtrlBase     = NULL,
    .dmaBlock[PAL_CPPI41_DMA_BLOCK1].globalCtrlBase     = NULL,
    .dmaBlock[PAL_CPPI41_DMA_BLOCK2].globalCtrlBase     = NULL,
    .dmaBlock[PAL_CPPI41_DMA_BLOCK3].globalCtrlBase     = NULL,

    .debugToolBind = puma6_cppi_docsis_us_proc_init,

};



/************************************************************************/
/*                                                                      */
/*      DOCSIS Buffer Manager specific definition                       */
/*                                                                      */
/************************************************************************/
typedef struct
{
    Cppi4EmbdDesc   hw;
    Uint32          psi[5];
}
docsisCppiDsDescType_t;


#define DS_EMBEDDED_DESC_BPI_CMD_TEMPLATE           0x21800000
#define DS_EMBEDDED_DESC_CRC_CMD_TEMPLATE           0x41800000
#define DS_EMBEDDED_DESC_PHS_CMD_TEMPLATE           0x11800000
#define DS_EMBEDDED_DESC_RX_FIFO_0_CMD_TEMPLATE     0x72000000
#define DS_EMBEDDED_DESC_RX_FIFO_1_CMD_TEMPLATE     0x080404C1



int docsis_cppi_MNG_Resources_init (PAL_Handle palHandle)
{
    Cppi4Queue          rxFreeQ;
    Cppi4Queue          rxQ;
    PAL_Cppi4QueueHnd   rxfdQueueHdl;
    int                 cnt;
    Cppi4HostDesc*      currBD;
    Ptr                 ptrBDregion;
    Ptr                 currBuffer;
    Cppi4RxChInitCfg    rxCh;
    PAL_Cppi4RxChHnd    rxChHandle;
    Cppi4Queue          txFreeQ;
    Cppi4Queue          txQ;
    PAL_Cppi4QueueHnd   txfdQueueHdl;

    /*  open a queue objects one for free descriptor queue
     *  and one for rx queue
     */

    rxQ.qMgr = PAL_CPPI41_QUEUE_MGR_PARTITION_SR;
    rxQ.qNum = PAL_CPPI41_SR_DOCSIS_MGMT_HOST_RX_Q_NUM;

    if (NULL == PAL_cppi4QueueOpen( palHandle, rxQ ))
    {
        return -1;
    }

    rxFreeQ.qMgr = PAL_CPPI41_QUEUE_MGR_PARTITION_SR;
    rxFreeQ.qNum = PAL_CPPI41_SR_DOCSIS_MGMT_RX_FD_HOST_Q_NUM;

    if (NULL == (rxfdQueueHdl = PAL_cppi4QueueOpen( palHandle, rxFreeQ )))
    {
        return -1;
    }

    txQ.qMgr = PAL_CPPI41_QUEUE_MGR_PARTITION_SR;
    txQ.qNum = PAL_CPPI41_SR_DOCSIS_TX_MGMT_Q_NUM;

    if (NULL == PAL_cppi4QueueOpen( palHandle, txQ ))
    {
        return -1;
    }

    txFreeQ.qMgr = PAL_CPPI41_QUEUE_MGR_PARTITION_SR;
    txFreeQ.qNum = PAL_CPPI41_SR_DOCSIS_MGMT_TX_FD_HOST_Q_NUM;

    if (NULL == (txfdQueueHdl = PAL_cppi4QueueOpen(palHandle, txFreeQ)))
    {
        return -1;
    }

    /*
     * get a pointer to the BD region
     * the region is already preallocated in the PAL init so just
     * get a pointer to the bd area
     */
    if ((ptrBDregion = PAL_cppi4AllocDesc( palHandle, PAL_CPPI41_QUEUE_MGR_PARTITION_SR,
                                                      PAL_CPPI41_SR_DOCSIS_MGMT_RX_FD_HOST_DESC_COUNT + PAL_CPPI41_SR_DOCSIS_MGMT_TX_FD_HOST_DESC_COUNT,
                                                      PAL_CPPI41_SR_DOCSIS_MGMT_RX_FD_HOST_DESC_SIZE )) == NULL)
    {
        printk( " %s : Failed to allocate CNI Management Rx descriptors\n", __FUNCTION__);
        return -1;
    }

    currBD = (Cppi4HostDesc*) ptrBDregion;

    /* allocate a memory for the buffers */

    if (PAL_osMemAlloc(0, PAL_CPPI41_SR_DOCSIS_MGMT_RX_FD_HOST_DESC_COUNT * PAL_CPPI41_SR_DOCSIS_MGMT_RX_FD_HOST_BUFF_SIZE, 0, (Ptr *)&currBuffer) != PAL_SOK)
    {
        printk( " %s : Failed to allocate memory for Management buffer queue",__FUNCTION__);

        PAL_cppi4DeallocDesc( palHandle, PAL_CPPI41_QUEUE_MGR_PARTITION_SR, ptrBDregion );

        return -1;
    }

    PAL_osMemSet(currBuffer,0,PAL_CPPI41_SR_DOCSIS_MGMT_RX_FD_HOST_DESC_COUNT * PAL_CPPI41_SR_DOCSIS_MGMT_RX_FD_HOST_BUFF_SIZE);


    /* need to consider the number of actually allocated buffers !!!!*/
    /* we don't really need 32 pair for management channel, do we? */
    for (cnt = 0; cnt < PAL_CPPI41_SR_DOCSIS_MGMT_RX_FD_HOST_DESC_COUNT; cnt++)
    {
        /* Update the hardware descriptor */
        currBD->descInfo    = ((PAL_CPPI4_HOSTDESC_DESC_TYPE_HOST    << PAL_CPPI4_HOSTDESC_DESC_TYPE_SHIFT));
        currBD->tagInfo     = 0;
        currBD->pktInfo     =
                              (PAL_CPPI4_HOSTDESC_PKT_TYPE_ETH       << PAL_CPPI4_HOSTDESC_PKT_TYPE_SHIFT)
                            | (PAL_CPPI4_HOSTDESC_PKT_RETPLCY_LINKED << PAL_CPPI4_HOSTDESC_PKT_RETPLCY_SHIFT)
                            | (PAL_CPPI4_HOSTDESC_DESC_LOC_OFFCHIP   << PAL_CPPI4_HOSTDESC_DESC_LOC_SHIFT)
                            | (rxFreeQ.qMgr                          << PAL_CPPI4_HOSTDESC_PKT_RETQMGR_SHIFT)
                            | (rxFreeQ.qNum                          << PAL_CPPI4_HOSTDESC_PKT_RETQNUM_SHIFT);

        currBD->buffLen     = 0;
        currBD->bufPtr      = 0;
        currBD->nextBDPtr   = 0;
        currBD->orgBuffLen  = PAL_CPPI41_SR_DOCSIS_MGMT_RX_FD_HOST_BUFF_SIZE;
        currBD->orgBufPtr   = PAL_CPPI4_VIRT_2_PHYS(currBuffer);

        PAL_CPPI4_CACHE_WRITEBACK( currBD, PAL_CPPI41_SR_DOCSIS_MGMT_RX_FD_HOST_DESC_SIZE );

        PAL_cppi4QueuePush ( rxfdQueueHdl,
                             (Ptr) PAL_CPPI4_VIRT_2_PHYS((Uint32)currBD),
                             PAL_CPPI4_DESCSIZE_2_QMGRSIZE(PAL_CPPI41_SR_DOCSIS_MGMT_RX_FD_HOST_DESC_SIZE),
                             0 );

        currBD      = (Cppi4HostDesc*)  ((Uint32)currBD     + PAL_CPPI41_SR_DOCSIS_MGMT_RX_FD_HOST_DESC_SIZE);
        currBuffer  = (Ptr)             ((Uint32)currBuffer + PAL_CPPI41_SR_DOCSIS_MGMT_RX_FD_HOST_BUFF_SIZE);
    }

    /* allocate a memory for the buffers */

    if (PAL_osMemAlloc(0, PAL_CPPI41_SR_DOCSIS_MGMT_TX_FD_HOST_DESC_COUNT * PAL_CPPI41_SR_DOCSIS_MGMT_TX_FD_HOST_BUFF_SIZE, 0, (Ptr *)&currBuffer) != PAL_SOK)
    {
        printk( " %s : Failed to allocate memory for Management buffer queue",__FUNCTION__);

        PAL_cppi4DeallocDesc( palHandle, PAL_CPPI41_QUEUE_MGR_PARTITION_SR, ptrBDregion );

        return -1;
    }

    PAL_osMemSet(currBuffer,0,PAL_CPPI41_SR_DOCSIS_MGMT_TX_FD_HOST_DESC_COUNT * PAL_CPPI41_SR_DOCSIS_MGMT_TX_FD_HOST_BUFF_SIZE);

    /* need to consider the number of actually allocated buffers !!!!*/
    /* we don't really need 32 pair for management channel, do we? */
    for (cnt = 0; cnt < PAL_CPPI41_SR_DOCSIS_MGMT_TX_FD_HOST_DESC_COUNT; cnt++)
    {
        /* Update the hardware descriptor */
        currBD->descInfo    = ( (PAL_CPPI4_HOSTDESC_DESC_TYPE_HOST     << PAL_CPPI4_HOSTDESC_DESC_TYPE_SHIFT));
        currBD->tagInfo     = 0;
        currBD->pktInfo     =
                                (PAL_CPPI4_HOSTDESC_PKT_TYPE_GENERIC    << PAL_CPPI4_HOSTDESC_PKT_TYPE_SHIFT)
                              | (PAL_CPPI4_HOSTDESC_PKT_RETPLCY_LINKED  << PAL_CPPI4_HOSTDESC_PKT_RETPLCY_SHIFT)
                              | (PAL_CPPI4_HOSTDESC_DESC_LOC_OFFCHIP    << PAL_CPPI4_HOSTDESC_DESC_LOC_SHIFT)
                              | (txFreeQ.qMgr                           << PAL_CPPI4_HOSTDESC_PKT_RETQMGR_SHIFT)
                              | (txFreeQ.qNum                           << PAL_CPPI4_HOSTDESC_PKT_RETQNUM_SHIFT);

        currBD->buffLen     = 0;
        currBD->bufPtr      = 0;
        currBD->nextBDPtr   = 0;
        currBD->orgBuffLen  = PAL_CPPI41_SR_DOCSIS_MGMT_TX_FD_HOST_BUFF_SIZE;
        currBD->orgBufPtr   = PAL_CPPI4_VIRT_2_PHYS(currBuffer);

        PAL_CPPI4_CACHE_WRITEBACK(currBD, PAL_CPPI41_SR_DOCSIS_MGMT_TX_FD_HOST_DESC_SIZE);

        PAL_cppi4QueuePush ( txfdQueueHdl,
                             (Ptr) PAL_CPPI4_VIRT_2_PHYS((Uint32)currBD),
                             PAL_CPPI4_DESCSIZE_2_QMGRSIZE(PAL_CPPI41_SR_DOCSIS_MGMT_TX_FD_HOST_DESC_SIZE),
                             0 );

        currBD      = (Cppi4HostDesc*)    ((Uint32)currBD     + PAL_CPPI41_SR_DOCSIS_MGMT_TX_FD_HOST_DESC_SIZE);
        currBuffer  = (Ptr)               ((Uint32)currBuffer + PAL_CPPI41_SR_DOCSIS_MGMT_TX_FD_HOST_BUFF_SIZE);
    }

    /* configure the DMA channels of both DS Groups to use this queue */

    rxCh.chNum =        PAL_CPPI41_SR_DOCSIS_DS_CoP_MGMT_DMA01_RX_CH_NUM;
    rxCh.dmaNum =       PAL_CPPI41_DMA_BLOCK0;
    rxCh.defDescType =  CPPI41_DESC_TYPE_HOST;
    rxCh.sopOffset =    0;
    rxCh.rxCompQueue =  rxQ;
    rxCh.u.hostPktCfg.fdbQueue[0] = rxFreeQ;
    rxCh.u.hostPktCfg.fdbQueue[1] = rxFreeQ;
    rxCh.u.hostPktCfg.fdbQueue[2] = rxFreeQ;
    rxCh.u.hostPktCfg.fdbQueue[3] = rxFreeQ;


    if (NULL == (rxChHandle = PAL_cppi4RxChOpen (palHandle, &rxCh, NULL)))
    {
        printk( " %s : Failed to open Management DMA channel",__FUNCTION__);
        return(-1);
    }

    PAL_cppi4EnableRxChannel (rxChHandle, NULL);

    rxCh.dmaNum =       PAL_CPPI41_DMA_BLOCK1;

    if (NULL == (rxChHandle = PAL_cppi4RxChOpen (palHandle, &rxCh, NULL)))
    {
        printk( " %s : Failed to open Management DMA channel",__FUNCTION__);
        return(-1);
    }

    PAL_cppi4EnableRxChannel (rxChHandle, NULL);


    return 0;
}


int docsis_cppi_DS_GROUP_init( PAL_Handle  palHandle, int groupIndex )
{
    Cppi4BufPool        tmpBufPool; /* Used for Init calls */
    int                 channel;
    Uint32              buffersBase;
    Uint32              descBase;

    printk(" Entered %s \n", __FUNCTION__);

    switch (groupIndex)
    {
        case 0:
        {
            buffersBase = AVALANCHE_DOCSIS_SS_DS_GROUP0_BUFFERS_RAM_BASE;
            descBase    = AVALANCHE_DOCSIS_SS_DS_GROUP0_PACKET_RAM_BASE;
            break;
        }

        case 1:
        {
            buffersBase = AVALANCHE_DOCSIS_SS_DS_GROUP1_BUFFERS_RAM_BASE;
            descBase    = AVALANCHE_DOCSIS_SS_DS_GROUP1_PACKET_RAM_BASE;
            break;
        }

        default:
        printk ("%s: FAILED - wrong group (%d).\n",__FUNCTION__, groupIndex);
        return -1;
    }

    /************************************************************************/
    /*  Buffer Pool Initialization                                          */
    /************************************************************************/
    tmpBufPool.bMgr     = BUF_POOL_DS_GROUP_MGR;
    tmpBufPool.bPool    = PAL_CPPI41_BMGR_POOL0;

    if ((PAL_cppi4BufPoolDirectInit(palHandle, tmpBufPool,
        BMGR_DS_GROUP_POOL00_REF_CNT,
        BMGR_DS_GROUP_POOL00_BUF_SIZE,
        BMGR_DS_GROUP_POOL00_BUF_COUNT,
        (Ptr)IO_VIRT2PHY_DSGx( buffersBase ) )) == NULL)
    {
        printk ("%s: for pool %d FAILED.\n",__FUNCTION__, tmpBufPool.bPool);
        return -1;
    }
    /************************************************************************/


    /************************************************************************/
    /*  Format descriptors and fill in the free queue for DOCSIS Downstream */
    /************************************************************************/
    {
        docsisCppiDsDescType_t*     currBD;
        docsisCppiDsDescType_t*     currBDphysical;

        int bd_cnt;
        PAL_Cppi4QueueHnd tmpQHnd;
        Cppi4Queue tmpQ;

        currBD          = (docsisCppiDsDescType_t *) descBase;
        currBDphysical  = (docsisCppiDsDescType_t *) IO_VIRT2PHY_DSGx( descBase );

        memset((void *)currBD, 0, PAL_CPPI41_DOCSIS_DS_FD_EMB_DESC_SIZE * PAL_CPPI41_DOCSIS_DS_FD_EMB_DESC_COUNT);

        tmpQ.qMgr = PAL_CPPI41_QUEUE_MGR_PARTITION_DOCSIS_DS_GROUP;
        tmpQ.qNum = PAL_CPPI41_DOCSIS_DS_FD_EMB_Q;
        tmpQHnd = PAL_cppi4QueueOpen ( palHandle, tmpQ );

        for (bd_cnt = 0; bd_cnt < PAL_CPPI41_DOCSIS_DS_FD_EMB_DESC_COUNT; bd_cnt++)
        {
            currBD->hw.descInfo    =
                  CPPI41_EM_DESCINFO_DTYPE_EMBEDDED
                /*| CPPI41_EM_DESCINFO_SLOTCNT_MYCNT */
                | (5 << CPPI41_EM_DESCINFO_PSWSIZE_SHIFT);
            currBD->hw.tagInfo     = 0;
            currBD->hw.pktInfo     =
                 (1                << CPPI41_EM_PKTINFO_RETPOLICY_SHIFT)
                |(1                << CPPI41_EM_PKTINFO_ONCHIP_SHIFT)
                |(tmpQ.qMgr        << PAL_CPPI4_HOSTDESC_PKT_RETQMGR_SHIFT)
                |(tmpQ.qNum        << PAL_CPPI4_HOSTDESC_PKT_RETQNUM_SHIFT);

            currBD->psi[0]  = DS_EMBEDDED_DESC_BPI_CMD_TEMPLATE;
            currBD->psi[1]  = DS_EMBEDDED_DESC_CRC_CMD_TEMPLATE;
            currBD->psi[2]  = DS_EMBEDDED_DESC_PHS_CMD_TEMPLATE;
            currBD->psi[3]  = DS_EMBEDDED_DESC_RX_FIFO_0_CMD_TEMPLATE;
            currBD->psi[4]  = DS_EMBEDDED_DESC_RX_FIFO_1_CMD_TEMPLATE;

            PAL_CPPI4_CACHE_WRITEBACK(currBD, PAL_CPPI41_DOCSIS_DS_FD_EMB_DESC_SIZE);

            PAL_cppi4QueuePush (tmpQHnd, (Ptr)currBDphysical, PAL_CPPI4_DESCSIZE_2_QMGRSIZE(PAL_CPPI41_DOCSIS_DS_FD_EMB_DESC_SIZE), 0);

            currBD          = (docsisCppiDsDescType_t*)((Uint32)currBD          + PAL_CPPI41_DOCSIS_DS_FD_EMB_DESC_SIZE);
            currBDphysical  = (docsisCppiDsDescType_t*)((Uint32)currBDphysical  + PAL_CPPI41_DOCSIS_DS_FD_EMB_DESC_SIZE);
        }
    }
    /************************************************************************/

    /************************************************************************/
    /*  DMA Rx Channels initialization.                                     */
    /************************************************************************/
    for (channel=0; channel<16; channel++)
    {
        Cppi4RxChInitCfg rxCh;
        PAL_Cppi4RxChHnd cppi4RxChHnd;

        /* Set up Rx channel */
        rxCh.chNum              = channel;
        rxCh.dmaNum             = PAL_CPPI41_DMA_BLOCK0;
        rxCh.defDescType        = CPPI41_DESC_TYPE_EMBEDDED;
        rxCh.sopOffset          = 0;
        rxCh.rxCompQueue.qMgr   = PAL_CPPI41_QUEUE_MGR_PARTITION_DOCSIS_DS_GROUP;
        rxCh.rxCompQueue.qNum   = 0;
        rxCh.u.embeddedPktCfg.fdQueue.qMgr = PAL_CPPI41_QUEUE_MGR_PARTITION_DOCSIS_DS_GROUP;
        rxCh.u.embeddedPktCfg.fdQueue.qNum = PAL_CPPI41_DOCSIS_DS_FD_EMB_Q;
        rxCh.u.embeddedPktCfg.numBufSlot = (EMSLOTCNT-1);
        rxCh.u.embeddedPktCfg.sopSlotNum = 0;
        rxCh.u.embeddedPktCfg.fBufPool[0] = tmpBufPool;
        rxCh.u.embeddedPktCfg.fBufPool[1] = tmpBufPool;
        rxCh.u.embeddedPktCfg.fBufPool[2] = tmpBufPool;
        rxCh.u.embeddedPktCfg.fBufPool[3] = tmpBufPool;
        cppi4RxChHnd        = PAL_cppi4RxChOpen( palHandle, &rxCh, NULL );

        /* Enable Rx channel */
        PAL_cppi4EnableRxChannel (cppi4RxChHnd, NULL);
    }
    /************************************************************************/


    /************************************************************************/
    /*  DMA Tx Channel initialization.                                      */
    /************************************************************************/
    {
        Cppi4TxChInitCfg txCh;
        PAL_Cppi4TxChHnd cppi4TxChHnd;

        /* Set up Tx channel */
        txCh.chNum          = 0;
        txCh.dmaNum         = PAL_CPPI41_DMA_BLOCK0;
        txCh.tdQueue.qMgr   = PAL_CPPI41_QUEUE_MGR_PARTITION_DOCSIS_DS_GROUP;
        txCh.tdQueue.qNum   = 59,// CNI_CPPI4x_DOCSIS_DS_CoP_Q;

        cppi4TxChHnd        = PAL_cppi4TxChOpen( palHandle, &txCh, NULL );

        /* Enable Rx channel */
        PAL_cppi4EnableTxChannel (cppi4TxChHnd, NULL);
    }
    /************************************************************************/

    return (0);
}


int docsis_cppi_DS_SR_init( PAL_Handle  palHandle, channel_info_t *  info )
{
    volatile Cppi4RxChInitCfg *rxChInfo;
    int dmaNum;
    int rxChNum;

    /***************************************************************
     * Prepare DOCSIS DS Data CoP channels
     * ====================
     */
    
    if (PAL_osMemAlloc(0, sizeof(Cppi4RxChInitCfg), sizeof(int), (Ptr *)&rxChInfo) != PAL_SOK)
    {
        printk(KERN_ERR "%s : Failed to allocate memory for rxChInfo", __FUNCTION__);
        return -1;
    }
    PAL_osMemSet((Ptr)rxChInfo, 0, sizeof(Cppi4RxChInitCfg));

    /* Common initializations */
    rxChInfo->sopOffset = 128;
    rxChInfo->retryOnStarvation = 0;
    rxChInfo->defDescType = CPPI41_DESC_TYPE_EMBEDDED;
    rxChInfo->u.embeddedPktCfg.numBufSlot = CPPI41_EM_DESCINFO_SLOTCNT_PP;
    rxChInfo->u.embeddedPktCfg.sopSlotNum = 0;
    rxChInfo->u.embeddedPktCfg.fBufPool[0].bMgr = BUF_POOL_MGR0;
    rxChInfo->u.embeddedPktCfg.fBufPool[1].bMgr = BUF_POOL_MGR0;
    rxChInfo->u.embeddedPktCfg.fBufPool[2].bMgr = BUF_POOL_MGR0;
    rxChInfo->u.embeddedPktCfg.fBufPool[3].bMgr = BUF_POOL_MGR0;
    rxChInfo->rxCompQueue.qMgr = PAL_CPPI41_QUEUE_MGR_PARTITION_SR;
    rxChInfo->u.embeddedPktCfg.fdQueue.qMgr = PAL_CPPI41_QUEUE_MGR_PARTITION_SR;

    for (rxChNum = 0; rxChNum < PAL_CPPI41_SR_DOCSIS_DS_CoP_DATA_NUM_RX_CH; rxChNum++)
    {
        if (-1 != info[rxChNum].dmaChannel)
        {
    PAL_Cppi4RxChHnd          rxChHdl;

            rxChInfo->chNum = info[rxChNum].dmaChannel;
            rxChInfo->rxCompQueue.qNum = info[rxChNum].outputQueue.qNum;
            rxChInfo->u.embeddedPktCfg.fdQueue.qNum = info[rxChNum].freeQueue.qNum;
            rxChInfo->u.embeddedPktCfg.fBufPool[0].bPool = info[rxChNum].bufPoolNum_s0;
            rxChInfo->u.embeddedPktCfg.fBufPool[1].bPool = info[rxChNum].bufPoolNum_s0;
            rxChInfo->u.embeddedPktCfg.fBufPool[2].bPool = info[rxChNum].bufPoolNum_s0;
            rxChInfo->u.embeddedPktCfg.fBufPool[3].bPool = info[rxChNum].bufPoolNum_s0;

    for (dmaNum = PAL_CPPI41_DMA_BLOCK0; dmaNum <= PAL_CPPI41_DMA_BLOCK1; dmaNum++)
    {
                rxChInfo->dmaNum = dmaNum;
                rxChHdl = PAL_cppi4RxChOpen(palHandle, (Cppi4RxChInitCfg*)rxChInfo, NULL);
        if(rxChHdl == NULL)
        {
                    printk("ERROR in %s: Unable to open dmaNum=%d, rxChNum=%d\n", __FUNCTION__, dmaNum, rxChInfo->chNum);
            return (-1);
        }

                PAL_cppi4EnableRxChannel(rxChHdl, NULL);

                DPRINTK("%s: Open and enable dmaNum=%d, rxChNum=%d\n", __FUNCTION__, dmaNum, rxChInfo->chNum);
    }
        }
    }

    return (0);
}



/*
 * Prepare CNI Infrastrucuter channels
 * ===================================
 */
int docsis_cppi_cni_infrastructure_init(PAL_Handle palHandle)
{
    int iInfraChan;

    /************************************************************************/
    /*********** Setup Free Host descriptors        *************************/
    /************************************************************************/

    /************************************************/
    /*      Allocate region                         */
    /*                                              */
    docsisHostBDCfg_g.buffDescRegionPtr =
        PAL_cppi4AllocDesc( palHandle, docsisHostBDCfg_g.qMgr,
        docsisHostBDCfg_g.numDesc,
        docsisHostBDCfg_g.szDesc );

    if (!docsisHostBDCfg_g.buffDescRegionPtr)
    {
        printk("%s: Host descriptor region allocation FAILED\n", __FUNCTION__);
        return -1;
    }
    /************************************************/

    {
        int i;
        Cppi4HostDesc*      currBD;

        currBD = (Cppi4HostDesc*)docsisHostBDCfg_g.buffDescRegionPtr;

        for (i = 0; i < docsisHostBDCfg_g.numBlks; i++)
        {
            BDBlkInfo* BDBlk = &docsisHostBDCfg_g.BDBlk[i];
            int bd_cnt;
            PAL_Cppi4QueueHnd   tmpQHnd;
            Cppi4Queue          tmpQ;

            tmpQ.qMgr = docsisHostBDCfg_g.qMgr;
            tmpQ.qNum = BDBlk->qNum;
            tmpQHnd = PAL_cppi4QueueOpen (palHandle, tmpQ);

            for (bd_cnt = 0; bd_cnt < BDBlk->numDesc; bd_cnt++)
            {
                PAL_osMemSet(currBD, 0, docsisHostBDCfg_g.szDesc);

                currBD->descInfo    = (PAL_CPPI4_HOSTDESC_DESC_TYPE_HOST << PAL_CPPI4_HOSTDESC_DESC_TYPE_SHIFT);
                currBD->tagInfo     = 0x3FFF;
                currBD->pktInfo     =
                         (BDBlk->pktType                        << PAL_CPPI4_HOSTDESC_PKT_TYPE_SHIFT)
                        |(PAL_CPPI4_HOSTDESC_PKT_RETPLCY_LINKED << PAL_CPPI4_HOSTDESC_PKT_RETPLCY_SHIFT)
                        |(PAL_CPPI4_HOSTDESC_DESC_LOC_OFFCHIP   << PAL_CPPI4_HOSTDESC_DESC_LOC_SHIFT)
                        |(docsisHostBDCfg_g.qMgr                << PAL_CPPI4_HOSTDESC_PKT_RETQMGR_SHIFT)
                        |(BDBlk->qNum                           << PAL_CPPI4_HOSTDESC_PKT_RETQNUM_SHIFT);

                if ( BDBlk->buffSize )
                {
                    struct sk_buff* skb = dev_alloc_skb( BDBlk->buffSize );

                    if (NULL == skb)
                    {
                        printk("%s: The SKB allocation FAILED\n", __FUNCTION__);
                        return -1;
                    }

                    skb_reserve (skb, NET_IP_ALIGN);    /* 16 bit align the IP fields. */
                    currBD->orgBuffLen  = BDBlk->buffSize - NET_IP_ALIGN;
                    currBD->orgBufPtr   = PAL_CPPI4_VIRT_2_PHYS(skb->data);
                    ((Cppi4HostDescLinux *)(currBD))->skb   = skb;
                }

                PAL_CPPI4_CACHE_WRITEBACK(currBD, docsisHostBDCfg_g.szDesc);

                PAL_cppi4QueuePush (tmpQHnd,
                                    (Ptr)PAL_CPPI4_VIRT_2_PHYS((Uint32)currBD),
                                    PAL_CPPI4_DESCSIZE_2_QMGRSIZE(docsisHostBDCfg_g.szDesc),
                                    0/*!@@*/);

                currBD = (Cppi4HostDesc*)((Uint32)currBD + docsisHostBDCfg_g.szDesc);
            }
        }
    }

    /********************** Free Host desc setup Done  ******************/


    /*
     * Open CNI Infrastructure Tx channels
     * -----------------------------------
     */
    for (iInfraChan = 0; iInfraChan<PAL_CPPI41_SR_CNI_INFRA_DMA_CH_COUNT; iInfraChan++)
    {
        volatile Cppi4TxChInitCfg InfraTxChInfo;
        volatile Cppi4RxChInitCfg InfraRxChInfo;
        PAL_Cppi4TxChHnd InfraTxChHdl;
        PAL_Cppi4RxChHnd InfraRxChHdl;

        InfraTxChInfo.chNum         = rxInfra[iInfraChan].dmaChannel;
        InfraTxChInfo.dmaNum        = rxInfra[iInfraChan].dmaBlock;
        InfraTxChInfo.tdQueue.qMgr  = PAL_CPPI41_QUEUE_MGR_PARTITION_SR;
        InfraTxChInfo.tdQueue.qNum  = PAL_CPPI41_SR_DMA_FD_TEARDOWN_Q_NUM;
        InfraTxChInfo.defDescType   = CPPI41_DESC_TYPE_EMBEDDED;

        DPRINTK(" Call PAL_cppi4TxChOpen channel=%d\n", InfraTxChInfo.chNum);

        InfraTxChHdl = PAL_cppi4TxChOpen ( palHandle, (Cppi4TxChInitCfg *)(&InfraTxChInfo), NULL);
        if(InfraTxChHdl == NULL)
        {
            printk("%s: Unable to open dma=%d, channel=%d\n", __FUNCTION__, InfraTxChInfo.dmaNum, InfraTxChInfo.chNum);
            return (-1);
        }
        PAL_cppi4EnableTxChannel (InfraTxChHdl, NULL);

    /*
     * Prepare CNI Infrastructure rx queues
     * ------------------------------------
     */
        if (NULL == PAL_cppi4QueueOpen( palHandle, rxInfra[iInfraChan].outputQueue))
        {
            printk("%s: Unable to open queue %d \n", __FUNCTION__, rxInfra[iInfraChan].outputQueue.qNum);
            return (-1);
        }

    /*
     * Open CNI Infrastructure Rx channels
     * -----------------------------------
     * Free queues are from Q[128+8]
     * Output queues are    Q[100+5]
     */

        InfraRxChInfo.chNum            = rxInfra[iInfraChan].dmaChannel;
        InfraRxChInfo.dmaNum           = rxInfra[iInfraChan].dmaBlock;
        InfraRxChInfo.rxCompQueue.qMgr = rxInfra[iInfraChan].outputQueue.qMgr;
        InfraRxChInfo.rxCompQueue.qNum = rxInfra[iInfraChan].outputQueue.qNum;
        InfraRxChInfo.sopOffset = 0; // SOF skip=0
        InfraRxChInfo.defDescType = CPPI41_DESC_TYPE_HOST;
        InfraRxChInfo.retryOnStarvation = 0;
        InfraRxChInfo.u.hostPktCfg.fdbQueue[0] = rxInfra[iInfraChan].freeQueue;
        InfraRxChInfo.u.hostPktCfg.fdbQueue[1] = rxInfra[iInfraChan].freeQueue;
        InfraRxChInfo.u.hostPktCfg.fdbQueue[2] = rxInfra[iInfraChan].freeQueue;
        InfraRxChInfo.u.hostPktCfg.fdbQueue[3] = rxInfra[iInfraChan].freeQueue;

        DPRINTK(" Call PAL_cppi4RxChOpen channel=%d\n", InfraRxChInfo.chNum);

        InfraRxChHdl = PAL_cppi4RxChOpen ( palHandle, (Cppi4RxChInitCfg *)(&InfraRxChInfo), NULL);
        if(InfraRxChHdl == NULL)
        {
            printk("%s: Unable to open dma=%d, channel=%d \n", __FUNCTION__, InfraRxChInfo.dmaNum, InfraRxChInfo.chNum);
            return (-1);
        }
        PAL_cppi4EnableRxChannel (InfraRxChHdl, NULL);
    }

    return (0);
}




int docsis_cppi_init(void)
{
    PAL_Handle          palHandle;
    int                 retCode;
    int                 i;

    printk(" Entered %s \n", __FUNCTION__);

    /***********************************************************************************/
    /***********************************************************************************/
    palHandle = PAL_cppi4Init( &cppi4_DSG0_InitCfg_g, (Ptr)CPPI41_DOMAIN_PRIMARY_DOCSIS_DSG0 );

    retCode = docsis_cppi_DS_GROUP_init(palHandle,0);

    if (retCode)
    {
        return (retCode);
    }
    /***********************************************************************************/

    /***********************************************************************************/
    /***********************************************************************************/
    palHandle = PAL_cppi4Init( &cppi4_DSG1_InitCfg_g, (Ptr)CPPI41_DOMAIN_PRIMARY_DOCSIS_DSG1 );

    retCode = docsis_cppi_DS_GROUP_init(palHandle,1);

    if (retCode)
    {
        return (retCode);
    }
    /***********************************************************************************/

    palHandle = PAL_cppi4Init( NULL, CPPI41_DOMAIN_PRIMARY_SR );


    do
    {
        unsigned int    paramVal;
        bool            singleDma = False;

        /* Chexk silicon stepping*/
        SRAM_Get_Boot_Param(SILICON_STEPPING_ID, &paramVal);
        if ( paramVal < SILICON_STEPPING_ID_C_1 )
        {
            singleDma = True;
        }

        retCode = 0;


        /****************************************************/
        /*                                                  */
        /*  Open DOCSIS Upstream MAC/PHY DMA channels       */
        /*                                                  */
        /****************************************************/
        for (i = 0; i < PAL_CPPI41_SR_DOCSIS_TX_DMA_CH_COUNT; i++)
        {
            volatile Cppi4TxChInitCfg  txchInfo;
            PAL_Cppi4TxChHnd           txchHdl;

            txchInfo.chNum          = PAL_CPPI41_SR_DOCSIS_TX_DMA_CH_NUM(i);

            if (singleDma)
            {
                txchInfo.dmaNum     = PAL_CPPI41_DMA_BLOCK2;
            }
            else
            {
            txchInfo.dmaNum         = PAL_CPPI41_SR_DOCSIS_TX_DMA_NUM(i);
            }

            txchInfo.tdQueue.qMgr   = PAL_CPPI41_QUEUE_MGR_PARTITION_SR;
            txchInfo.tdQueue.qNum   = PAL_CPPI41_SR_DMA_FD_TEARDOWN_Q_NUM;

            if (NULL == (txchHdl = PAL_cppi4TxChOpen( palHandle, (Cppi4TxChInitCfg *)(&txchInfo), NULL )))
            {
                printk("%s: Unable to open dma=%d, channel=%d\n", __FUNCTION__, txchInfo.dmaNum, txchInfo.chNum);
                retCode = -1;
                break;
            }

            PAL_cppi4EnableTxChannel (txchHdl, NULL);
        }
        /****************************************************/

        /****************************************************/
        /*                                                  */
        /*  Open DOCSIS Upstream CoProcessor DMA channels   */
        /*                                                  */
        /****************************************************/
        {
             volatile Cppi4RxChInitCfg  rxchInfo;
             PAL_Cppi4RxChHnd           rxchHdl;
             volatile Cppi4TxChInitCfg  txchInfo;
             PAL_Cppi4TxChHnd           txchHdl;
             Cppi4Queue                 fdQueue = {PAL_CPPI41_QUEUE_MGR_PARTITION_DOCSIS , PAL_CPPI41_DOCSIS_US_FD_EMB_Q};

             /* init co-proc TX channel */
            txchInfo.chNum          = PAL_CPPI41_SR_DOCSIS_TX_COP_DMA_TX_CH_NUM;
            txchInfo.dmaNum         = PAL_CPPI41_DMA_BLOCK2;
            txchInfo.tdQueue.qMgr   = PAL_CPPI41_QUEUE_MGR_PARTITION_SR;
            txchInfo.tdQueue.qNum   = PAL_CPPI41_SR_DMA_FD_TEARDOWN_Q_NUM;

            if (NULL == (txchHdl = PAL_cppi4TxChOpen( palHandle, (Cppi4TxChInitCfg *)(&txchInfo), NULL )))
            {
                printk("%s: Unable to open dma=%d, channel=%d\n", __FUNCTION__, txchInfo.dmaNum, txchInfo.chNum);
                retCode = -1;
                break;
            }

            PAL_cppi4EnableTxChannel (txchHdl, NULL);

            /* init co-proc RX channel qman 1 queue */
            rxchInfo.chNum          = PAL_CPPI41_SR_DOCSIS_TX_COP_DMA_RX_CH_NUM;
            rxchInfo.dmaNum         = PAL_CPPI41_DMA_BLOCK2;
            rxchInfo.defDescType    = CPPI41_DESC_TYPE_EMBEDDED;
            rxchInfo.sopOffset=0;
            rxchInfo.retryOnStarvation = 0;

            rxchInfo.u.embeddedPktCfg.fdQueue = fdQueue;
            rxchInfo.u.embeddedPktCfg.numBufSlot = (0);
            rxchInfo.u.embeddedPktCfg.sopSlotNum = 0;
            rxchInfo.u.embeddedPktCfg.fBufPool[0].bMgr  = BUF_POOL_MGR1;
            rxchInfo.u.embeddedPktCfg.fBufPool[0].bPool = PAL_CPPI41_BMGR_POOL0;
            rxchInfo.u.embeddedPktCfg.fBufPool[1].bMgr  = BUF_POOL_MGR1;
            rxchInfo.u.embeddedPktCfg.fBufPool[1].bPool = PAL_CPPI41_BMGR_POOL0;
            rxchInfo.u.embeddedPktCfg.fBufPool[2].bMgr  = BUF_POOL_MGR1;
            rxchInfo.u.embeddedPktCfg.fBufPool[2].bPool = PAL_CPPI41_BMGR_POOL0;
            rxchInfo.u.embeddedPktCfg.fBufPool[3].bMgr  = BUF_POOL_MGR1;
            rxchInfo.u.embeddedPktCfg.fBufPool[3].bPool = PAL_CPPI41_BMGR_POOL0;

            if (NULL == (rxchHdl = PAL_cppi4RxChOpen( palHandle, (Cppi4RxChInitCfg *)(&rxchInfo), NULL )))
            {
                printk("%s: Unable to open dma=%d, channel=%d\n", __FUNCTION__, rxchInfo.dmaNum, rxchInfo.chNum);
                retCode = -1;
                break;
            }

            PAL_cppi4EnableRxChannel (rxchHdl, NULL);
        }
        /****************************************************/

    } while (0);

    retCode |= docsis_cppi_DS_SR_init( palHandle, dsCopDmaChannels );

    retCode |= docsis_cppi_cni_infrastructure_init( palHandle );

    retCode |= docsis_cppi_MNG_Resources_init( palHandle );

    PAL_cppi4Exit( palHandle, CPPI41_DOMAIN_PRIMARY_SR );

    if (retCode)
    {
        return (retCode);
    }

    palHandle = PAL_cppi4Init( &cppi4_DOCSIS_US_InitCfg_g, (Ptr)CPPI41_DOMAIN_PRIMARY_DOCSIS );

    /************************************************************************/
    /*  Initialize the buffers for DOCSIS Upstream CoProcessor              */
    /************************************************************************/
    {
        Cppi4BufPool tmpBufPool;
        tmpBufPool.bMgr     = BUF_POOL_MGR1;
        tmpBufPool.bPool    = PAL_CPPI41_BMGR_POOL0;
        if ((PAL_cppi4BufPoolInit(palHandle, tmpBufPool,
                            BMGR1_POOL00_REF_CNT,
                            BMGR1_POOL00_BUF_SIZE,
                            BMGR1_POOL00_BUF_COUNT )) == NULL)
        {
            printk ("%s: PAL_cppi4BufPoolInit for pool %d FAILED\n", __FUNCTION__, tmpBufPool.bPool);
            return -1;
        }
    }
    /************************************************************************/


    /************************************************************************/

    return (0);
}


/****************************************************************************************************/
/*                                                                                                  */
/*                                                                                                  */
/*                                                                                                  */
/*                                                                                                  */
/*                        DEBUGGING PROCS                                                           */
/*                                                                                                  */
/*                                                                                                  */
/*                                                                                                  */
/*                                                                                                  */
/****************************************************************************************************/

#define PUMA6_PROC_FS_BUFF_SZ   (512*128)
static char puma6_proc_fs_buffer[ PUMA6_PROC_FS_BUFF_SZ ];

/****************************************************************************************************/

typedef struct FDqueue
{
    unsigned int    id;
    unsigned int    amount;
}
FDqueue_t;

FDqueue_t gFDqueues_docsis_us[] =
{
    {   .id = PAL_CPPI41_DOCSIS_US_FD_EMB_Q,        .amount = PAL_CPPI41_DOCSIS_US_FD_EMB_DESC_COUNT    },
    {   .id = PAL_CPPI41_DOCSIS_US_FD_MONO_Q,       .amount = 16    },
};

FDqueue_t gFDqueues_docsis_ds[] =
{
    {   .id = PAL_CPPI41_DOCSIS_DS_FD_EMB_Q,        .amount = PAL_CPPI41_DOCSIS_DS_FD_EMB_DESC_COUNT    },
};

static PAL_Handle puma6palHnd_docsis_us;
static PAL_Handle puma6palHnd_docsis_ds0;
static PAL_Handle puma6palHnd_docsis_ds1;

/****************************************************************************************************/

PAL_CPPI41_DOCSIS_US_QMGR_QUEUES_STR(us_qname);

/****************************************************************************************************/
int puma6_cppi_docsis_us_dump_all_stats(char* buf, char **start, off_t offset, int count, int *eof, void *data)
{
    int queue = 0;
    int len = 0;
    int i;
    unsigned int pktCount;
    unsigned int expectedCount;
    unsigned int byteCount;
    unsigned int byteCountHead;
    Cppi4Queue  cppiQueue;
    static   int buff_size;

    if (0 == offset)
    {
        for (queue = 0; queue < PAL_CPPI41_DOCSIS_US_QMGR_TOTAL_Q_COUNT; queue++)
        {
            cppiQueue.qMgr = PAL_CPPI41_QUEUE_MGR_PARTITION_DOCSIS;
            cppiQueue.qNum = queue ;

            PAL_cppi4Control(puma6palHnd_docsis_us, PAL_CPPI41_IOCTL_GET_QUEUE_ENTRY_COUNT,   &cppiQueue, &pktCount);

            expectedCount = 0;

            for (i=0; i<ARRAY_SIZE(gFDqueues_docsis_us); i++)
            {
                if (gFDqueues_docsis_us[i].id == queue)
                {
                    expectedCount = gFDqueues_docsis_us[i].amount;
                    break;
                }
            }

            if (pktCount != expectedCount)
            {
                PAL_cppi4Control(puma6palHnd_docsis_us, PAL_CPPI41_IOCTL_GET_QUEUE_BYTE_COUNT,    &cppiQueue, &byteCount);
                PAL_cppi4Control(puma6palHnd_docsis_us, PAL_CPPI41_IOCTL_GET_QUEUE_HEAD_PKT_SIZE, &cppiQueue, &byteCountHead);

                len += sprintf (&puma6_proc_fs_buffer[len], "Queue: %5d %-65s : %10d pkts [%5d] %10d bytes %10d head\n",
                                queue, us_qname[queue], pktCount, expectedCount, byteCount, byteCountHead);
            }

            if (len + 128 > PUMA6_PROC_FS_BUFF_SZ)
            {
                sprintf(&puma6_proc_fs_buffer[len - 6],"\n...\n");
                break ;
            }
        }

        buff_size = len;
    }
    else
    {
        len = buff_size - offset;
    }

    if (len > count)
    {
        len = count;
    }

    memcpy(buf, &puma6_proc_fs_buffer[offset], len);

    return len;
}
/****************************************************************************************************/

PAL_CPPI41_DOCSIS_DS_QMGR_QUEUES_STR(ds_qname);

/****************************************************************************************************/
int puma6_cppi_docsis_ds_dump_all_stats(char* buf, char **start, off_t offset, int count, int *eof, void *data)
{
    int queue = 0;
    int len = 0;
    int i;
    unsigned int pktCount;
    unsigned int expectedCount;
    unsigned int byteCount;
    unsigned int byteCountHead;
    Cppi4Queue  cppiQueue;
    static   int buff_size;
    PAL_Handle  handle = puma6palHnd_docsis_ds0;

    if (data)
    {
        handle = puma6palHnd_docsis_ds1;
    }

    if (0 == offset)
    {

        for (queue = 0; queue < PAL_CPPI41_DOCSIS_DS_QMGR_TOTAL_Q_COUNT; queue++)
        {
            cppiQueue.qMgr = PAL_CPPI41_QUEUE_MGR_PARTITION_DOCSIS_DS_GROUP;
            cppiQueue.qNum = queue ;

            PAL_cppi4Control(handle, PAL_CPPI41_IOCTL_GET_QUEUE_ENTRY_COUNT,   &cppiQueue, &pktCount);

            expectedCount = 0;

            for (i=0; i<ARRAY_SIZE(gFDqueues_docsis_ds); i++)
            {
                if (gFDqueues_docsis_ds[i].id == queue)
                {
                    expectedCount = gFDqueues_docsis_ds[i].amount;
                    break;
                }
            }

            if (pktCount != expectedCount)
            {
                PAL_cppi4Control(handle, PAL_CPPI41_IOCTL_GET_QUEUE_BYTE_COUNT,    &cppiQueue, &byteCount);
                PAL_cppi4Control(handle, PAL_CPPI41_IOCTL_GET_QUEUE_HEAD_PKT_SIZE, &cppiQueue, &byteCountHead);

                len += sprintf (&puma6_proc_fs_buffer[len], "Queue: %5d DS%d %-62s : %10d pkts [%5d] %10d bytes %10d head\n",
                                queue, (unsigned int)data, ds_qname[queue], pktCount, expectedCount, byteCount, byteCountHead);
            }

            if (len + 128 > PUMA6_PROC_FS_BUFF_SZ)
            {
                sprintf(&puma6_proc_fs_buffer[len - 6],"\n...\n");
                break ;
            }
        }

        buff_size = len;
    }
    else
    {
        len = buff_size - offset;
    }

    if (len > count)
    {
        len = count;
    }

    memcpy(buf, &puma6_proc_fs_buffer[offset], len);

    return len;
}
/****************************************************************************************************/


/****************************************************************************************************/
static PAL_Result puma6_cppi_docsis_us_proc_init (Ptr hnd, Ptr param)
{
    struct proc_dir_entry * dir_1 = (struct proc_dir_entry *)param;

    puma6palHnd_docsis_us = (PAL_Handle)hnd;

    if (NULL == (dir_1 = proc_mkdir("docsis_us",dir_1))) {   printk("%s:%d ERROR ....\n",__FUNCTION__,__LINE__); return -1;  }
    if (NULL == (dir_1 = proc_mkdir("stats",    dir_1))) {   printk("%s:%d ERROR ....\n",__FUNCTION__,__LINE__); return -1;  }
    if (NULL == (create_proc_read_entry( "all" , 0, dir_1, puma6_cppi_docsis_us_dump_all_stats, NULL ))) {   printk("%s:%d ERROR ....\n",__FUNCTION__,__LINE__); return -1;  }

    return (0);
}
/****************************************************************************************************/

/****************************************************************************************************/
static PAL_Result puma6_cppi_docsis_ds0_proc_init (Ptr hnd, Ptr param)
{
    struct proc_dir_entry * dir_1 = (struct proc_dir_entry *)param;

    puma6palHnd_docsis_ds0 = (PAL_Handle)hnd;

    if (NULL == (dir_1 = proc_mkdir("docsis_ds0",dir_1))) {   printk("%s:%d ERROR ....\n",__FUNCTION__,__LINE__); return -1;  }
    if (NULL == (dir_1 = proc_mkdir("stats",    dir_1))) {   printk("%s:%d ERROR ....\n",__FUNCTION__,__LINE__); return -1;  }
    if (NULL == (create_proc_read_entry( "all" , 0, dir_1, puma6_cppi_docsis_ds_dump_all_stats, (void *)0 ))) {   printk("%s:%d ERROR ....\n",__FUNCTION__,__LINE__); return -1;  }

    return (0);
}
/****************************************************************************************************/

/****************************************************************************************************/
static PAL_Result puma6_cppi_docsis_ds1_proc_init (Ptr hnd, Ptr param)
{
    struct proc_dir_entry * dir_1 = (struct proc_dir_entry *)param;

    puma6palHnd_docsis_ds1 = (PAL_Handle)hnd;

    if (NULL == (dir_1 = proc_mkdir("docsis_ds1",dir_1))) {   printk("%s:%d ERROR ....\n",__FUNCTION__,__LINE__); return -1;  }
    if (NULL == (dir_1 = proc_mkdir("stats",    dir_1))) {   printk("%s:%d ERROR ....\n",__FUNCTION__,__LINE__); return -1;  }
    if (NULL == (create_proc_read_entry( "all" , 0, dir_1, puma6_cppi_docsis_ds_dump_all_stats, (void *)1 ))) {   printk("%s:%d ERROR ....\n",__FUNCTION__,__LINE__); return -1;  }

    return (0);
}
/****************************************************************************************************/

