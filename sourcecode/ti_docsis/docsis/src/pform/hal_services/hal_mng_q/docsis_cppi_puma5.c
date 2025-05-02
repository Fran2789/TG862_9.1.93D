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
#include <puma5_cppi.h>
#include <linux/skbuff.h>
#include <linux/proc_fs.h>

// #define DOCSIS_CPPI_DEBUG

#ifdef DOCSIS_CPPI_DEBUG
/* note: prints function name for you */
#  define DPRINTK(fmt, args...) printk("%s:%d " fmt, __FUNCTION__ , __LINE__ , ## args)
#else
#  define DPRINTK(fmt, args...)
#endif
static PAL_Result puma5_cppi_docsis_proc_init (Ptr hnd, Ptr param);


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
    BDBlkInfo   BDBlk [PAL_CPPI41_FBD_Q_LAST - PAL_CPPI41_FBD_Q_BASE];
} HostBDCfg;


HostBDCfg docsisHostBDCfg_g =
{
    .qMgr       = PAL_CPPI41_QUEUE_MGR_PARTITION_SR,
    .numDesc    = CPPI4x_CNI_RX_INFRA_FD_HOST_DESC_NUM,                       /* Must be same as in Cppi Cgf structure */
    .szDesc     = CPPI4x_CNI_RX_INFRA_FD_HOST_DESC_SIZE,                      /* Must be same as for specific region   */
    .numBlks    = CPPI4x_CNI_RX_INFRA_CH_COUNT,       /* Change for each block added/removed   */

    /* PP2Host CNI Low */
    .BDBlk[0].numDesc   = CPPI4x_CNI_RX_INFRA_FD_HOST_DESC_NUM_LOW,
    .BDBlk[0].qNum      = CPPI4x_CNI_RX_INFRA_FD_HOST_QNUM( PAL_CPPI4x_PRTY_LOW ),
    .BDBlk[0].pktType   = PAL_CPPI4_HOSTDESC_PKT_TYPE_ETH,
    .BDBlk[0].buffSize  = CPPI4x_CNI_RX_INFRA_FD_HOST_BUFF_SIZE,

    /* PP2Host CNI Medium/High */
    .BDBlk[1].numDesc   = CPPI4x_CNI_RX_INFRA_FD_HOST_DESC_NUM_HIGH,
    .BDBlk[1].qNum      = CPPI4x_CNI_RX_INFRA_FD_HOST_QNUM( PAL_CPPI4x_PRTY_MED ),
    .BDBlk[1].pktType   = PAL_CPPI4_HOSTDESC_PKT_TYPE_ETH,
    .BDBlk[1].buffSize  = CPPI4x_CNI_RX_INFRA_FD_HOST_BUFF_SIZE
};


/************************************************************************/
/*      PP -> HOST  CNI Infrastructure channels information             */
/************************************************************************/
static  channel_info_t  rxInfra[CPPI4x_CNI_RX_INFRA_CH_COUNT]=
{
    {
        .outputQueue.qMgr   = PAL_CPPI41_QUEUE_MGR_PARTITION_SR,
        .outputQueue.qNum   = CPPI4x_CNI_RXCMPL_QNUM( PAL_CPPI4x_PRTY_LOW ),
        .freeQueue.qMgr     = PAL_CPPI41_QUEUE_MGR_PARTITION_SR,
        .freeQueue.qNum     = CPPI4x_CNI_RX_INFRA_FD_HOST_QNUM( PAL_CPPI4x_PRTY_LOW ),
        .dmaChannel         = CPPI4x_CNI_RX_INFRA_CH_NUM(PAL_CPPI4x_PRTY_LOW), /* CNI Low */
        .dmaBlock           = PAL_CPPI41_DMA_BLOCK1,
    },
    {
        .outputQueue.qMgr   = PAL_CPPI41_QUEUE_MGR_PARTITION_SR,
        .outputQueue.qNum   = CPPI4x_CNI_RXCMPL_QNUM( PAL_CPPI4x_PRTY_MED ),
        .freeQueue.qMgr     = PAL_CPPI41_QUEUE_MGR_PARTITION_SR,
        .freeQueue.qNum     = CPPI4x_CNI_RX_INFRA_FD_HOST_QNUM( PAL_CPPI4x_PRTY_MED ),
        .dmaChannel         = CPPI4x_CNI_RX_INFRA_CH_NUM(PAL_CPPI4x_PRTY_MED), /* CNI High */
        .dmaBlock           = PAL_CPPI41_DMA_BLOCK1,
    }
};
/************************************************************************/



/************************************************************************/
/*      DOCSIS -> PP channels information                               */
/************************************************************************/
static  channel_info_t  rxIngressDS [CPPI4x_SR_DOCSIS_RX_DMA_CH_COUNT]=
{
    /*  =========== Odd channels ================ */
    {
        .outputQueue.qMgr   = PAL_CPPI41_QUEUE_MGR_PARTITION_SR,
        .outputQueue.qNum   = PAL_CPPI41_SR_PPDSP_LOW_Q_NUM,
        .freeQueue.qMgr     = PAL_CPPI41_QUEUE_MGR_PARTITION_SR,
        .freeQueue.qNum     = CPPI4x_SR_DOCSIS_RX_FD_EMB_QNUM( PAL_CPPI4x_PRTY_LOW ),
        .dmaChannel         = CPPI4x_SR_DOCSIS_RX_DMA_CHNUM(0),
        .dmaBlock           = PAL_CPPI41_DMA_BLOCK1,
        .bufPoolNum_s0      = BMGR0_POOL00,
        .bufPoolNum_s1      = BMGR0_POOL00,
        .bufPoolNum_s2      = BMGR0_POOL04,
        .bufPoolNum_s3      = BMGR0_POOL00,
    },

    /*  =========== Even channels =============== */
    {
        .outputQueue.qMgr   = PAL_CPPI41_QUEUE_MGR_PARTITION_SR,
        .outputQueue.qNum   = PAL_CPPI41_SR_PPDSP_LOW_Q_NUM,
        .freeQueue.qMgr     = PAL_CPPI41_QUEUE_MGR_PARTITION_SR,
        .freeQueue.qNum     = CPPI4x_SR_DOCSIS_RX_FD_EMB_QNUM( PAL_CPPI4x_PRTY_LOW ),
        .dmaChannel         = CPPI4x_SR_DOCSIS_RX_DMA_CHNUM(1),
        .dmaBlock           = PAL_CPPI41_DMA_BLOCK1,
        .bufPoolNum_s0      = BMGR0_POOL01,
        .bufPoolNum_s1      = BMGR0_POOL01,
        .bufPoolNum_s2      = BMGR0_POOL04,
        .bufPoolNum_s3      = BMGR0_POOL00,
    },

    /*  ======= High Priority channels ========== */
    {
        .outputQueue.qMgr   = PAL_CPPI41_QUEUE_MGR_PARTITION_SR,
        .outputQueue.qNum   = PAL_CPPI41_SR_PPDSP_HIGH_Q_NUM,
        .freeQueue.qMgr     = PAL_CPPI41_QUEUE_MGR_PARTITION_SR,
        .freeQueue.qNum     = CPPI4x_SR_DOCSIS_RX_FD_EMB_QNUM( PAL_CPPI4x_PRTY_HIGH ),
        .dmaChannel         = CPPI4x_SR_DOCSIS_RX_DMA_CHNUM(2),
        .dmaBlock           = PAL_CPPI41_DMA_BLOCK1,
        .bufPoolNum_s0      = BMGR0_POOL05,
        .bufPoolNum_s1      = BMGR0_POOL05,
        .bufPoolNum_s2      = BMGR0_POOL05,
        .bufPoolNum_s3      = BMGR0_POOL00,
    }
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
static Cppi4InitCfg cppi4InitCfg_g =
{
    .resetLine              = 0,

    /************************************************************************/
    /*                                                                      */
    /************************************************************************/

    .queueMgrInfo[PAL_CPPI41_QUEUE_MGR_PARTITION_DOCSIS].queueMgrRgnBase        = (Ptr) AVALANCHE_DOCSIS_SS_QMGR_RGN_BASE,
    .queueMgrInfo[PAL_CPPI41_QUEUE_MGR_PARTITION_DOCSIS].descMemRgnBase         = (Ptr) AVALANCHE_DOCSIS_SS_DESCMEM_RGN_BASE,
    .queueMgrInfo[PAL_CPPI41_QUEUE_MGR_PARTITION_DOCSIS].queueMgmtRgnBase       = (Ptr) AVALANCHE_DOCSIS_SS_QMGMT_RGN_BASE,
    .queueMgrInfo[PAL_CPPI41_QUEUE_MGR_PARTITION_DOCSIS].queueStatusRgnBase     = (Ptr) AVALANCHE_DOCSIS_SS_QSTATUS_RGN_BASE,

    .queueMgrInfo[PAL_CPPI41_QUEUE_MGR_PARTITION_DOCSIS].LinkingRAM0Base        = IO_VIRT2PHY(AVALANCHE_DOCSIS_SS_LINKING_RAM_BASE),
    .queueMgrInfo[PAL_CPPI41_QUEUE_MGR_PARTITION_DOCSIS].LinkingRAM0Size        = 1024,
    .queueMgrInfo[PAL_CPPI41_QUEUE_MGR_PARTITION_DOCSIS].LinkingRAM1Base        = IO_VIRT2PHY(AVALANCHE_DOCSIS_SS_LINKING_RAM_BASE+0x1000),

                                                                                  /* Docsis MAC DS internal memory */
    .queueMgrInfo[PAL_CPPI41_QUEUE_MGR_PARTITION_DOCSIS].descRegion[0].base     = (Ptr) IO_VIRT2PHY( AVALANCHE_DOCSIS_SS_DS_PACKET_RAM_BASE + 0x8000 ),
    .queueMgrInfo[PAL_CPPI41_QUEUE_MGR_PARTITION_DOCSIS].descRegion[0].szDesc   = CNI_CPPI4x_DOCSIS_DS_DESC_SIZE,
    .queueMgrInfo[PAL_CPPI41_QUEUE_MGR_PARTITION_DOCSIS].descRegion[0].numDesc  = CNI_CPPI4x_DOCSIS_DS_DESC_NUM,
    .queueMgrInfo[PAL_CPPI41_QUEUE_MGR_PARTITION_DOCSIS].descRegion[0].isOnChip = 1,

                                                                                  /* Docsis MAC US internal memory */
    .queueMgrInfo[PAL_CPPI41_QUEUE_MGR_PARTITION_DOCSIS].descRegion[1].base     = (Ptr) IO_VIRT2PHY( AVALANCHE_DOCSIS_SS_US_PACKET_RAM_BASE ),
    .queueMgrInfo[PAL_CPPI41_QUEUE_MGR_PARTITION_DOCSIS].descRegion[1].szDesc   = 128,
    .queueMgrInfo[PAL_CPPI41_QUEUE_MGR_PARTITION_DOCSIS].descRegion[1].numDesc  = 32,
    .queueMgrInfo[PAL_CPPI41_QUEUE_MGR_PARTITION_DOCSIS].descRegion[1].isOnChip = 1,

    .queueMgrInfo[PAL_CPPI41_QUEUE_MGR_PARTITION_DOCSIS].descRegion[2].base     = 0,
    .queueMgrInfo[PAL_CPPI41_QUEUE_MGR_PARTITION_DOCSIS].descRegion[2].szDesc   = CNI_CPPI4x_DOCSIS_US_DESC_SIZE,
    .queueMgrInfo[PAL_CPPI41_QUEUE_MGR_PARTITION_DOCSIS].descRegion[2].numDesc  = CNI_CPPI4x_DOCSIS_US_DESC_NUM,
    .queueMgrInfo[PAL_CPPI41_QUEUE_MGR_PARTITION_DOCSIS].descRegion[2].isOnChip = 0,

    .queueMgrInfo[PAL_CPPI41_QUEUE_MGR_PARTITION_DOCSIS].descRegion[3].base     = 0,
    .queueMgrInfo[PAL_CPPI41_QUEUE_MGR_PARTITION_DOCSIS].descRegion[3].szDesc   = sizeof(Cppi4TeardownDesc),    /* 32 byte sized        */
    .queueMgrInfo[PAL_CPPI41_QUEUE_MGR_PARTITION_DOCSIS].descRegion[3].numDesc  = PAL_CPPI41_NUM_TD_DESC * 2,   /* Have to serve 2 DMAs */
    .queueMgrInfo[PAL_CPPI41_QUEUE_MGR_PARTITION_DOCSIS].descRegion[3].isOnChip = 0,

    /************************************************************************/
    /*                                                                      */
    /************************************************************************/

    .bufMgrBase[PAL_CPPI41_BUF_MGR_PARTITION_DOCSIS]    = (CSL_BufMgr_RegsOvly) AVALANCHE_DOCSIS_SS_BMGR_BASE,

    /************************************************************************/
    /*                                                                      */
    /************************************************************************/

    .dmaBlock[PAL_CPPI41_DMA_BLOCK0].globalCtrlBase     = (Ptr) AVALANCHE_DOCSIS_DMA0_GBLCFG_BASE,
    .dmaBlock[PAL_CPPI41_DMA_BLOCK0].chCtrlStatusBase   = (Ptr) AVALANCHE_DOCSIS_DMA0_CHNCFG_BASE,
    .dmaBlock[PAL_CPPI41_DMA_BLOCK0].schedCtrlBase      = (Ptr) AVALANCHE_DOCSIS_DMA0_SCHEDCFG_BASE,
    .dmaBlock[PAL_CPPI41_DMA_BLOCK0].schedTableBase     = (Ptr) AVALANCHE_DOCSIS_DMA0_SCHEDTBL_BASE,

    .dmaBlock[PAL_CPPI41_DMA_BLOCK0].tdFQueue.qMgr      = PAL_CPPI41_QUEUE_MGR_PARTITION_DOCSIS,
    .dmaBlock[PAL_CPPI41_DMA_BLOCK0].tdFQueue.qNum      = 63,

    .dmaBlock[PAL_CPPI41_DMA_BLOCK1].globalCtrlBase     = (Ptr) AVALANCHE_DOCSIS_DMA1_GBLCFG_BASE,
    .dmaBlock[PAL_CPPI41_DMA_BLOCK1].chCtrlStatusBase   = (Ptr) AVALANCHE_DOCSIS_DMA1_CHNCFG_BASE,
    .dmaBlock[PAL_CPPI41_DMA_BLOCK1].schedCtrlBase      = (Ptr) AVALANCHE_DOCSIS_DMA1_SCHEDCFG_BASE,
    .dmaBlock[PAL_CPPI41_DMA_BLOCK1].schedTableBase     = (Ptr) AVALANCHE_DOCSIS_DMA1_SCHEDTBL_BASE,

    .dmaBlock[PAL_CPPI41_DMA_BLOCK1].tdFQueue.qMgr      = PAL_CPPI41_QUEUE_MGR_PARTITION_DOCSIS,
    .dmaBlock[PAL_CPPI41_DMA_BLOCK1].tdFQueue.qNum      = 63,

    .dmaBlock[PAL_CPPI41_DMA_BLOCK0].schedTable.numEntries  = 1,
    .dmaBlock[PAL_CPPI41_DMA_BLOCK0].schedTable.entry =
    {
        PAL_CPPI41_DMA_CH_CONFIG( PAL_CPPI41_DMA_TX_CH, 0 ),
    },

    .dmaBlock[PAL_CPPI41_DMA_BLOCK1].schedTable.numEntries  = 8,
    .dmaBlock[PAL_CPPI41_DMA_BLOCK1].schedTable.entry =
    {
        PAL_CPPI41_DMA_CH_CONFIG( PAL_CPPI41_DMA_RX_CH, 0 ),
        PAL_CPPI41_DMA_CH_CONFIG( PAL_CPPI41_DMA_RX_CH, 1 ),
        PAL_CPPI41_DMA_CH_CONFIG( PAL_CPPI41_DMA_RX_CH, 2 ),
        PAL_CPPI41_DMA_CH_CONFIG( PAL_CPPI41_DMA_RX_CH, 3 ),
        PAL_CPPI41_DMA_CH_CONFIG( PAL_CPPI41_DMA_RX_CH, 4 ),
        PAL_CPPI41_DMA_CH_CONFIG( PAL_CPPI41_DMA_RX_CH, 5 ),
        PAL_CPPI41_DMA_CH_CONFIG( PAL_CPPI41_DMA_RX_CH, 6 ),
        PAL_CPPI41_DMA_CH_CONFIG( PAL_CPPI41_DMA_RX_CH, 7 ),
    },
	.debugToolBind = puma5_cppi_docsis_proc_init,
};

/************************************************************************/
/*                                                                      */
/*      DOCSIS Buffer Manager specific definition                       */
/*                                                                      */
/************************************************************************/
#define DS_EMBEDDED_DESC_RAM_BASE_ADDR_HOST         ((AVALANCHE_DOCSIS_SS_DS_PACKET_RAM_BASE + 0x08000000 + 0x8000)) /* Host addressing */

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
    rxQ.qNum = CPPI4x_SR_DOCSIS_MGMT_RXCMPL_QNUM(0);

    if (NULL == PAL_cppi4QueueOpen( palHandle, rxQ ))
    {
        return -1;
    }

    rxFreeQ.qMgr = PAL_CPPI41_QUEUE_MGR_PARTITION_SR;
    rxFreeQ.qNum = CPPI4x_SR_DOCSIS_MGMT_RX_FD_HOST_QNUM;

    if (NULL == (rxfdQueueHdl = PAL_cppi4QueueOpen( palHandle, rxFreeQ )))
    {
        return -1;
    }

    txQ.qMgr = PAL_CPPI41_QUEUE_MGR_PARTITION_SR;
    txQ.qNum = CPPI4x_SR_DOCSIS_MGMT_TX_QNUM(0);

    if (NULL == PAL_cppi4QueueOpen( palHandle, txQ ))
    {
        return -1;
    }

    txFreeQ.qMgr = PAL_CPPI41_QUEUE_MGR_PARTITION_SR;
    txFreeQ.qNum = CPPI4x_SR_DOCSIS_MGMT_TX_FD_HOST_QNUM;

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
                                                      CPPI4x_SR_DOCSIS_MGMT_RX_FD_HOST_DESC_NUM + CPPI4x_SR_DOCSIS_MGMT_TX_FD_HOST_DESC_NUM,
                                                      CPPI4x_SR_DOCSIS_MGMT_RX_FD_HOST_DESC_SIZE )) == NULL)
    {
        printk( " %s : Failed to allocate CNI Management Rx descriptors\n", __FUNCTION__);
        return -1;
    }

    currBD = (Cppi4HostDesc*) ptrBDregion;

    /* allocate a memory for the buffers */

    if (PAL_osMemAlloc(0, CPPI4x_SR_DOCSIS_MGMT_RX_FD_HOST_DESC_NUM * CPPI4x_SR_DOCSIS_MGMT_RX_FD_HOST_BUFF_SIZE, 0, (Ptr *)&currBuffer) != PAL_SOK)
    {
        printk( " %s : Failed to allocate memory for Management buffer queue",__FUNCTION__);

        PAL_cppi4DeallocDesc( palHandle, PAL_CPPI41_QUEUE_MGR_PARTITION_SR, ptrBDregion );

        return -1;
    }

    PAL_osMemSet(currBuffer,0,CPPI4x_SR_DOCSIS_MGMT_RX_FD_HOST_DESC_NUM * CPPI4x_SR_DOCSIS_MGMT_RX_FD_HOST_BUFF_SIZE);


    /* need to consider the number of actually allocated buffers !!!!*/
    /* we don't really need 32 pair for management channel, do we? */
    for (cnt = 0; cnt < CPPI4x_SR_DOCSIS_MGMT_RX_FD_HOST_DESC_NUM; cnt++)
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
        currBD->orgBuffLen  = CPPI4x_SR_DOCSIS_MGMT_RX_FD_HOST_BUFF_SIZE;
        currBD->orgBufPtr   = PAL_CPPI4_VIRT_2_PHYS(currBuffer);

        PAL_CPPI4_CACHE_WRITEBACK( currBD, CPPI4x_SR_DOCSIS_MGMT_RX_FD_HOST_DESC_SIZE );

        PAL_cppi4QueuePush ( rxfdQueueHdl,
                             (Ptr) PAL_CPPI4_VIRT_2_PHYS((Uint32)currBD),
                             PAL_CPPI4_DESCSIZE_2_QMGRSIZE(CPPI4x_SR_DOCSIS_MGMT_RX_FD_HOST_DESC_SIZE),
                             0 );

        currBD      = (Cppi4HostDesc*)  ((Uint32)currBD     + CPPI4x_SR_DOCSIS_MGMT_RX_FD_HOST_DESC_SIZE);
        currBuffer  = (Ptr)             ((Uint32)currBuffer + CPPI4x_SR_DOCSIS_MGMT_RX_FD_HOST_BUFF_SIZE);
    }

    /* allocate a memory for the buffers */

    if (PAL_osMemAlloc(0, CPPI4x_SR_DOCSIS_MGMT_TX_FD_HOST_DESC_NUM * CPPI4x_SR_DOCSIS_MGMT_TX_FD_HOST_BUFF_SIZE, 0, (Ptr *)&currBuffer) != PAL_SOK)
    {
        printk( " %s : Failed to allocate memory for Management buffer queue",__FUNCTION__);

        PAL_cppi4DeallocDesc( palHandle, PAL_CPPI41_QUEUE_MGR_PARTITION_SR, ptrBDregion );

        return -1;
    }

    PAL_osMemSet(currBuffer,0,CPPI4x_SR_DOCSIS_MGMT_TX_FD_HOST_DESC_NUM * CPPI4x_SR_DOCSIS_MGMT_TX_FD_HOST_BUFF_SIZE);

    /* need to consider the number of actually allocated buffers !!!!*/
    /* we don't really need 32 pair for management channel, do we? */
    for (cnt = 0; cnt < CPPI4x_SR_DOCSIS_MGMT_TX_FD_HOST_DESC_NUM; cnt++)
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
        currBD->orgBuffLen  = CPPI4x_SR_DOCSIS_MGMT_TX_FD_HOST_BUFF_SIZE;
        currBD->orgBufPtr   = PAL_CPPI4_VIRT_2_PHYS(currBuffer);

        PAL_CPPI4_CACHE_WRITEBACK(currBD, CPPI4x_SR_DOCSIS_MGMT_TX_FD_HOST_DESC_SIZE);

        PAL_cppi4QueuePush ( txfdQueueHdl,
                             (Ptr) PAL_CPPI4_VIRT_2_PHYS((Uint32)currBD),
                             PAL_CPPI4_DESCSIZE_2_QMGRSIZE(CPPI4x_SR_DOCSIS_MGMT_TX_FD_HOST_DESC_SIZE),
                             0 );

        currBD      = (Cppi4HostDesc*)    ((Uint32)currBD     + CPPI4x_SR_DOCSIS_MGMT_TX_FD_HOST_DESC_SIZE);
        currBuffer  = (Ptr)               ((Uint32)currBuffer + CPPI4x_SR_DOCSIS_MGMT_TX_FD_HOST_BUFF_SIZE);
    }

    /* configure the DMA channel to use this queue*/

    rxCh.chNum =        CPPI4x_SR_DOCSIS_MGMT_RXCMPL_CHNUM;
    rxCh.dmaNum =       PAL_CPPI41_DMA_BLOCK1;
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

    return 0;
}


int docsis_cppi_DS_GROUP_init( PAL_Handle  palHandle )
{
    Cppi4BufPool        tmpBufPool; /* Used for Init calls */
    int                 channel;

    printk(" Entered %s \n", __FUNCTION__);

    /************************************************************************/
    /*  Buffer Pool Initialization                                          */
    /************************************************************************/
    tmpBufPool.bMgr     = PAL_CPPI41_BUF_MGR_PARTITION_DOCSIS;
    tmpBufPool.bPool    = BMGR1_POOL00;

    if ((PAL_cppi4BufPoolDirectInit(palHandle, tmpBufPool,
        BMGR1_POOL00_REF_CNT,
        BMGR1_POOL00_BUF_SIZE,
        BMGR1_POOL00_BUF_COUNT,
        (Ptr)IO_VIRT2PHY( AVALANCHE_DOCSIS_SS_DS_PACKET_RAM_BASE ) )) == NULL)
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

        currBD          = (docsisCppiDsDescType_t *) DS_EMBEDDED_DESC_RAM_BASE_ADDR_HOST;
        currBDphysical  = (docsisCppiDsDescType_t *) IO_VIRT2PHY( AVALANCHE_DOCSIS_SS_DS_PACKET_RAM_BASE + 0x8000 );

        memset((void *)currBD, 0, CNI_CPPI4x_DOCSIS_DS_DESC_SIZE*CNI_CPPI4x_DOCSIS_DS_DESC_NUM);

        tmpQ.qMgr = PAL_CPPI41_QUEUE_MGR_PARTITION_DOCSIS;
        tmpQ.qNum = CNI_CPPI4x_DOCSIS_DS_FDQ;
        tmpQHnd = PAL_cppi4QueueOpen ( palHandle, tmpQ );

        for (bd_cnt = 0; bd_cnt < CNI_CPPI4x_DOCSIS_DS_DESC_NUM; bd_cnt++)
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

            PAL_CPPI4_CACHE_WRITEBACK(currBD, CNI_CPPI4x_DOCSIS_DS_DESC_SIZE);

            PAL_cppi4QueuePush (tmpQHnd, (Ptr)currBDphysical, PAL_CPPI4_DESCSIZE_2_QMGRSIZE(CNI_CPPI4x_DOCSIS_DS_DESC_SIZE), 0);

            currBD          = (docsisCppiDsDescType_t*)((Uint32)currBD          + CNI_CPPI4x_DOCSIS_DS_DESC_SIZE);
            currBDphysical  = (docsisCppiDsDescType_t*)((Uint32)currBDphysical  + CNI_CPPI4x_DOCSIS_DS_DESC_SIZE);
        }
    }
    /************************************************************************/

    /************************************************************************/
    /*  DMA Rx Channels initialization.                                     */
    /************************************************************************/
    for (channel=0; channel<8; channel++)
    {
        Cppi4RxChInitCfg rxCh;
        PAL_Cppi4RxChHnd cppi4RxChHnd;

        /* Set up Rx channel */
        rxCh.chNum              = channel;
        rxCh.dmaNum             = PAL_CPPI41_DMA_BLOCK1;
        rxCh.defDescType        = CPPI41_DESC_TYPE_EMBEDDED;
        rxCh.sopOffset          = 0;
        rxCh.rxCompQueue.qMgr   = PAL_CPPI41_QUEUE_MGR_PARTITION_DOCSIS;
        rxCh.rxCompQueue.qNum   = 0;
        rxCh.u.embeddedPktCfg.fdQueue.qMgr = PAL_CPPI41_QUEUE_MGR_PARTITION_DOCSIS;
        rxCh.u.embeddedPktCfg.fdQueue.qNum = CNI_CPPI4x_DOCSIS_DS_FDQ;
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
        txCh.tdQueue.qMgr   = PAL_CPPI41_BUF_MGR_PARTITION_DOCSIS;
        txCh.tdQueue.qNum   = 63; 

        cppi4TxChHnd        = PAL_cppi4TxChOpen( palHandle, &txCh, NULL );

        /* Enable Rx channel */
        PAL_cppi4EnableTxChannel (cppi4TxChHnd, NULL);
    }
    /************************************************************************/

    return (0);
}


int docsis_cppi_DS_SR_init( PAL_Handle  palHandle, channel_info_t *  info )
{
    int iCniChan;

    /***************************************************************
     * Prepare CNI Rx channels
     * ====================
     */

    /*
     * Prepare Cni Rx queues
     * ---------------------
     * Free queues are from Q[144+7]..
     * Rx complete queues are from Q[56].. - Prefetcher queues
     */
    for (iCniChan=0; iCniChan<CPPI4x_SR_DOCSIS_RX_DMA_CH_COUNT; iCniChan++)
    {
    /*
     * Open Docsis Cni Rx channels
     * -----------------------------
     * Free queues are from   Q[128+0]
     * Output queues are from Q[100+5]
     */
        if (-1 != info[iCniChan].dmaChannel)
        {
            volatile Cppi4RxChInitCfg CniRxChInfo;
            PAL_Cppi4RxChHnd CniRxChHdl;

            CniRxChInfo.chNum               = info[iCniChan].dmaChannel;
            CniRxChInfo.dmaNum              = info[iCniChan].dmaBlock;
            CniRxChInfo.rxCompQueue.qMgr    = info[iCniChan].outputQueue.qMgr;
            CniRxChInfo.rxCompQueue.qNum    = info[iCniChan].outputQueue.qNum;
            CniRxChInfo.sopOffset           = 0; // SOF skip=0
            CniRxChInfo.retryOnStarvation   = 0;

            CniRxChInfo.defDescType = CPPI41_DESC_TYPE_EMBEDDED;
            CniRxChInfo.u.embeddedPktCfg.fdQueue.qMgr = info[iCniChan].freeQueue.qMgr;
            CniRxChInfo.u.embeddedPktCfg.fdQueue.qNum = info[iCniChan].freeQueue.qNum;
            CniRxChInfo.u.embeddedPktCfg.numBufSlot = (EMSLOTCNT-1);
            CniRxChInfo.u.embeddedPktCfg.sopSlotNum = 1;

            CniRxChInfo.u.embeddedPktCfg.fBufPool[0].bMgr   = BUF_POOL_MGR0;
            CniRxChInfo.u.embeddedPktCfg.fBufPool[1].bMgr   = BUF_POOL_MGR0;
            CniRxChInfo.u.embeddedPktCfg.fBufPool[2].bMgr   = BUF_POOL_MGR0;
            CniRxChInfo.u.embeddedPktCfg.fBufPool[3].bMgr   = BUF_POOL_MGR0;

            CniRxChInfo.u.embeddedPktCfg.fBufPool[0].bPool  = info[iCniChan].bufPoolNum_s0;
            CniRxChInfo.u.embeddedPktCfg.fBufPool[1].bPool  = info[iCniChan].bufPoolNum_s1;
            CniRxChInfo.u.embeddedPktCfg.fBufPool[2].bPool  = info[iCniChan].bufPoolNum_s2;
            CniRxChInfo.u.embeddedPktCfg.fBufPool[3].bPool  = info[iCniChan].bufPoolNum_s3;

            DPRINTK(" Call PAL_cppi4RxChOpen channel=%d\n", CniRxChInfo.chNum);

            CniRxChHdl = PAL_cppi4RxChOpen ( palHandle, (Cppi4RxChInitCfg *)(&CniRxChInfo), NULL);
            if(CniRxChHdl == NULL)
            {
                DPRINTK(" Unable to open %d channel \n", CniRxChInfo.chNum);
                return (-1);
            }

            PAL_cppi4EnableRxChannel ( CniRxChHdl, NULL );

            DPRINTK(" Call PAL_cppi4EnableRxChannel channel=%d Done\n", CniRxChInfo.chNum);
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
        DPRINTK("Host descriptor region allocation FAILED.\n");
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
                        DPRINTK("The SKB allocation FAILED.\n");
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
     * Input queues are predefined from Q[222]..
     * TxCompl queues are from Q[122+2]
     */
    for (iInfraChan=0; iInfraChan < CPPI4x_CNI_RX_INFRA_CH_COUNT; iInfraChan++)
    {
        volatile Cppi4TxChInitCfg InfraTxChInfo;
        volatile Cppi4RxChInitCfg InfraRxChInfo;
        PAL_Cppi4TxChHnd InfraTxChHdl;
        PAL_Cppi4RxChHnd InfraRxChHdl;

        InfraTxChInfo.chNum         = rxInfra[iInfraChan].dmaChannel;
        InfraTxChInfo.dmaNum        = rxInfra[iInfraChan].dmaBlock;
        InfraTxChInfo.tdQueue.qMgr  = DMA1_CPPI4x_FTD_QMGR;
        InfraTxChInfo.tdQueue.qNum  = DMA1_CPPI4x_FTD_QNUM;
        InfraTxChInfo.defDescType   = CPPI41_DESC_TYPE_EMBEDDED;

        DPRINTK(" Call PAL_cppi4TxChOpen channel=%d\n", InfraTxChInfo.chNum);

        InfraTxChHdl = PAL_cppi4TxChOpen ( palHandle, (Cppi4TxChInitCfg *)(&InfraTxChInfo), NULL);
        if(InfraTxChHdl == NULL)
        {
            DPRINTK(" Unable to open %d channel \n", InfraTxChInfo.chNum);
            return (-1);
        }
        PAL_cppi4EnableTxChannel (InfraTxChHdl, NULL);

    /*
     * Prepare CNI Infrastructure rx queues
     * ------------------------------------
     * Free queues are from   Q[128+8]
     * Rx complete queues are Q[100+5]
     */
        if (NULL == PAL_cppi4QueueOpen( palHandle, rxInfra[iInfraChan].outputQueue))
        {
            DPRINTK(" Unable to open queue %d \n", rxInfra[iInfraChan].outputQueue.qNum);
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
            DPRINTK(" Unable to open %d channel \n", InfraRxChInfo.chNum);
            return (-1);
        }
        PAL_cppi4EnableRxChannel (InfraRxChHdl, NULL);
    }

    return (0);
}




int docsis_cppi_init(void)
{
    PAL_Handle          palHandleDocsis;
    PAL_Handle          palHandleSR;
    int                 retCode;
    int                 i;

    printk(" Entered %s \n", __FUNCTION__);

    palHandleSR = PAL_cppi4Init( NULL, CPPI41_DOMAIN_PRIMARY_SR );

    do
    {
        retCode = 0;

        /****************************************************/
        /*                                                  */
        /*  Open DOCSIS Upstream MAC/PHY DMA channels       */
        /*                                                  */
        /****************************************************/
        for (i = 0; i < CPPI4x_SR_DOCSIS_TX_DMA_CH_COUNT; i++)
        {
            volatile Cppi4TxChInitCfg  txchInfo;
            PAL_Cppi4TxChHnd           txchHdl;

            txchInfo.chNum          = CPPI4x_SR_DOCSIS_TX_DMA_CHNUM(i);
            txchInfo.dmaNum         = PAL_CPPI41_DMA_BLOCK1;
            txchInfo.tdQueue.qMgr   = DMA1_CPPI4x_FTD_QMGR;
            txchInfo.tdQueue.qNum   = DMA1_CPPI4x_FTD_QNUM;

            if (NULL == (txchHdl = PAL_cppi4TxChOpen( palHandleSR, (Cppi4TxChInitCfg *)(&txchInfo), NULL )))
            {
                printk("%s: Unable to open %d channel \n",__FUNCTION__, txchInfo.chNum);
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
             Cppi4Queue                 fdQueue = {PAL_CPPI41_QUEUE_MGR_PARTITION_DOCSIS ,CNI_CPPI4x_DOCSIS_US_FDQ};

             /* init co-proc TX channel */
            txchInfo.chNum          = PAL_CPPI41_DMA1_TX_DOCSIS_TX_CoP_CHN_NUM;
            txchInfo.dmaNum         = PAL_CPPI41_DMA_BLOCK1;
            txchInfo.tdQueue.qMgr   = DMA1_CPPI4x_FTD_QMGR;
            txchInfo.tdQueue.qNum   = DMA1_CPPI4x_FTD_QNUM;

            if (NULL == (txchHdl = PAL_cppi4TxChOpen( palHandleSR, (Cppi4TxChInitCfg *)(&txchInfo), NULL )))
            {
                printk("%s: Unable to open %d channel \n",__FUNCTION__, txchInfo.chNum);
                retCode = -1;
                break;
            }

            PAL_cppi4EnableTxChannel (txchHdl, NULL);

            /* init co-proc RX channel qman 1 queue */
            rxchInfo.chNum          = PAL_CPPI41_DMA1_RX_DOCSIS_TX_CoP_CHN_NUM;
            rxchInfo.dmaNum         = PAL_CPPI41_DMA_BLOCK1;
            rxchInfo.defDescType    = CPPI41_DESC_TYPE_EMBEDDED;
            rxchInfo.sopOffset=0;
            rxchInfo.retryOnStarvation = 0;

            rxchInfo.u.embeddedPktCfg.fdQueue = fdQueue;
            rxchInfo.u.embeddedPktCfg.numBufSlot = (EMSLOTCNT-1);
            rxchInfo.u.embeddedPktCfg.sopSlotNum = 0;
            rxchInfo.u.embeddedPktCfg.fBufPool[0].bMgr  = BUF_POOL_MGR0;
            rxchInfo.u.embeddedPktCfg.fBufPool[0].bPool = BMGR0_POOL12;
            rxchInfo.u.embeddedPktCfg.fBufPool[1].bMgr  = BUF_POOL_MGR0;
            rxchInfo.u.embeddedPktCfg.fBufPool[1].bPool = BMGR0_POOL12;
            rxchInfo.u.embeddedPktCfg.fBufPool[2].bMgr  = BUF_POOL_MGR0;
            rxchInfo.u.embeddedPktCfg.fBufPool[2].bPool = BMGR0_POOL12;
            rxchInfo.u.embeddedPktCfg.fBufPool[3].bMgr  = BUF_POOL_MGR0;
            rxchInfo.u.embeddedPktCfg.fBufPool[3].bPool = BMGR0_POOL12;

            if (NULL == (rxchHdl = PAL_cppi4RxChOpen( palHandleSR, (Cppi4RxChInitCfg *)(&rxchInfo), NULL )))
            {
                printk("%s: Unable to open %d channel \n",__FUNCTION__, rxchInfo.chNum);
                retCode = -1;
                break;
            }

            PAL_cppi4EnableRxChannel (rxchHdl, NULL);
        }
        /****************************************************/
    } while (0);

    retCode |= docsis_cppi_DS_SR_init( palHandleSR, rxIngressDS );

    retCode |= docsis_cppi_cni_infrastructure_init( palHandleSR );

    retCode |= docsis_cppi_MNG_Resources_init( palHandleSR );

    PAL_cppi4Exit( palHandleSR, CPPI41_DOMAIN_PRIMARY_SR );

    if (retCode)
    {
        return (retCode);
    }

    palHandleDocsis = PAL_cppi4Init( &cppi4InitCfg_g, (Ptr)CPPI41_DOMAIN_PRIMARY_DOCSIS );

    /************************************************************************/
    retCode = docsis_cppi_DS_GROUP_init( palHandleDocsis );

    if (retCode)
    {
        return (retCode);
    }

    /************************************************************************/

    /************************************************************************/
    /*  Format descriptors and fill in the free queue for DOCSIS Upstream   */
    /************************************************************************/
    {
        Cppi4EmbdDesc* currBD;
        int bd_cnt;
        PAL_Cppi4QueueHnd tmpQHnd;
        Cppi4Queue tmpQ;

        currBD = (Cppi4EmbdDesc*)PAL_cppi4AllocDesc( palHandleDocsis, PAL_CPPI41_QUEUE_MGR_PARTITION_DOCSIS,
                                                     CNI_CPPI4x_DOCSIS_US_DESC_NUM,
                                                     CNI_CPPI4x_DOCSIS_US_DESC_SIZE );

        tmpQ.qMgr = PAL_CPPI41_QUEUE_MGR_PARTITION_DOCSIS;
        tmpQ.qNum = CNI_CPPI4x_DOCSIS_US_FDQ;
        tmpQHnd = PAL_cppi4QueueOpen ( palHandleDocsis, tmpQ );

        for (bd_cnt = 0; bd_cnt < CNI_CPPI4x_DOCSIS_US_DESC_NUM; bd_cnt++)
        {
            currBD->descInfo    = CPPI41_EM_DESCINFO_DTYPE_EMBEDDED | CPPI41_EM_DESCINFO_SLOTCNT_MYCNT;
            currBD->tagInfo     = 0;
            currBD->pktInfo     =
                 (1                << CPPI41_EM_PKTINFO_RETPOLICY_SHIFT)
                |(tmpQ.qMgr        << PAL_CPPI4_HOSTDESC_PKT_RETQMGR_SHIFT)
                |(tmpQ.qNum        << PAL_CPPI4_HOSTDESC_PKT_RETQNUM_SHIFT);

            PAL_CPPI4_CACHE_WRITEBACK(currBD, CNI_CPPI4x_DOCSIS_US_DESC_SIZE);

            PAL_cppi4QueuePush (tmpQHnd, (Ptr)PAL_CPPI4_VIRT_2_PHYS((Uint32)currBD),
                (CNI_CPPI4x_DOCSIS_US_DESC_SIZE-24)/4, 0);

            currBD = (Cppi4EmbdDesc*)((Uint32)currBD + CNI_CPPI4x_DOCSIS_US_DESC_SIZE);
        }

    }
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

#define PUMA5_PROC_FS_BUFF_SZ   (512*128)
static char puma5_proc_fs_buffer[ PUMA5_PROC_FS_BUFF_SZ ];

/****************************************************************************************************/
typedef struct FDqueue
{
    unsigned int    id;
    unsigned int    amount;
}
FDqueue_t;

FDqueue_t gFDqueues_docsis[] =
{
   {   .id = CNI_CPPI4x_DOCSIS_US_FDQ,                      .amount = CNI_CPPI4x_DOCSIS_US_DESC_NUM   }, 
   {   .id = CNI_CPPI4x_DOCSIS_DS_FDQ,                      .amount = CNI_CPPI4x_DOCSIS_DS_DESC_NUM   }, 
   {   .id = PAL_CPPI41_DOCSIS_TX_MONOLITHIC_Q_NUM,         .amount = 16    }, //check
};

// Define QUEUEs Names
PAL_CPPI41_DOCSIS_QMGR_QUEUES_STR(gDOCSIS_MNGRname);


#define PAL_CPPI41_GET_DOCSIS_QNAME(num) (gDOCSIS_MNGRname[(num)])

static PAL_Handle puma5palHnd_docsis;
int puma5_cppi_docsis_dump_all_stats(char* buf, char **start, off_t offset, int count, int *eof, void *data)
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
        for (queue = 0; queue < PAL_CPPI41_DOCSIS_INTERNAL_Q_LAST; queue++)
        {
            cppiQueue.qMgr = PAL_CPPI41_QUEUE_MGR_PARTITION_DOCSIS;
            cppiQueue.qNum = queue ;

            PAL_cppi4Control(puma5palHnd_docsis, PAL_CPPI41_IOCTL_GET_QUEUE_ENTRY_COUNT,   &cppiQueue, &pktCount);

            expectedCount = 0;

            for (i=0; i<ARRAY_SIZE(gFDqueues_docsis); i++)
            {
                if (gFDqueues_docsis[i].id == queue)
                {
                    expectedCount = gFDqueues_docsis[i].amount;
                    break;
                }
            }


            if (pktCount != expectedCount )   
            {
                PAL_cppi4Control(puma5palHnd_docsis, PAL_CPPI41_IOCTL_GET_QUEUE_BYTE_COUNT,    &cppiQueue, &byteCount);
                PAL_cppi4Control(puma5palHnd_docsis, PAL_CPPI41_IOCTL_GET_QUEUE_HEAD_PKT_SIZE, &cppiQueue, &byteCountHead);
                 
                len += sprintf (&puma5_proc_fs_buffer[len], "%5d %-27s : %10d pkts [%5d] %10d bytes %10d head\n",
                                queue, PAL_CPPI41_GET_DOCSIS_QNAME(queue), pktCount, expectedCount, byteCount, byteCountHead);
            }

            if (len + 128 > PUMA5_PROC_FS_BUFF_SZ)
            {
                sprintf(&puma5_proc_fs_buffer[len - 6],"\n...\n");
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

    memcpy(buf, &puma5_proc_fs_buffer[offset], len);

    return len;
}
static PAL_Result puma5_cppi_docsis_proc_init (Ptr hnd, Ptr param)
{
    struct proc_dir_entry * dir_1 = (struct proc_dir_entry *)param;

    puma5palHnd_docsis = (PAL_Handle)hnd;

    if (NULL == (dir_1 = proc_mkdir("docsis_us",dir_1))) {   printk("%s:%d ERROR ....\n",__FUNCTION__,__LINE__); return -1;  }
    if (NULL == (dir_1 = proc_mkdir("stats",    dir_1))) {   printk("%s:%d ERROR ....\n",__FUNCTION__,__LINE__); return -1;  }
    if (NULL == (create_proc_read_entry( "all" , 0, dir_1, puma5_cppi_docsis_dump_all_stats, NULL ))) {   printk("%s:%d ERROR ....\n",__FUNCTION__,__LINE__); return -1;  }

    return (0);
}

