/*
 * eth_cppi41.c
 * Description:
 * Ethernet driver using CPPI 4.1.
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
 */

#include <common.h>

#if (CONFIG_COMMANDS & CFG_CMD_NET)

#include <command.h>
#include <net.h>
#include <malloc.h>

#define bit32u  unsigned int

#include "cppi41.h"

#include "cpemacphy.h"

#define PKT_PREPAD  2       /* !@0  for now */

#define MDIO_PHY_MASK                   0x00000002

/* CPGMAC_F Reg ops */
#define CPGMACF_RX_MBP_ENABLE(base)     (*(volatile unsigned int*)((base)+0x01C))
#define CPGMACF_RX_UNICAST_SET(base)    (*(volatile unsigned int*)((base)+0x020))
#define CPGMACF_MACHASH1(base)          (*(volatile unsigned int*)((base)+0x04c))
#define CPGMACF_MACHASH2(base)          (*(volatile unsigned int*)((base)+0x050))
#define CPGMACF_MACCONTROL(base)        (*(volatile unsigned int*)((base)+0x02C))
#define CPGMACF_MAC_INTMASK_SET(base)   (*(volatile unsigned int*)((base)+0x014))
#define CPGMACF_RX_UNICAST_CLEAR(base)  (*(volatile unsigned int*)((base)+0x024))
#define CPGMACF2_MACADDR_LO(base)       (*(volatile unsigned int*)((base)+0x500))
#define CPGMACF2_MACADDR_HI(base)       (*(volatile unsigned int*)((base)+0x504))
#define CPGMACF2_QUEUEINFO(base)        (*(volatile unsigned int*)((base)+0x508))
#define CPGMACF2_MACINDEX(base)         (*(volatile unsigned int*)((base)+0x50C))
#define CPGMACF_MACSTATUS(base)         (*(volatile unsigned int*)((base)+0x030))


#define CPMAC_RXMBP_BROADEN_SHIFT       13
#define CPMAC_RXMBP_BROADEN_MASK        (0x1 << 13)
#define CPMAC_RXMBP_MULTIEN_SHIFT       5
#define CPMAC_RXMBP_MULTIEN_MASK        (0x1 << 5)

#define RX_BROAD_EN \
    ((1 << CPMAC_RXMBP_BROADEN_SHIFT) & CPMAC_RXMBP_BROADEN_MASK)
#define RX_MULTI_EN \
    ((1 << CPMAC_RXMBP_MULTIEN_SHIFT) & CPMAC_RXMBP_MULTIEN_MASK)

#define MAC_EN      0x00000020
#define GMII_EN     (1<<5)
#define GIG_MODE    (1<<7)

#define uint unsigned int
#define UINT8 unsigned char

#define PKT_MAX_MCAST 12
/*
 * Packet device information
 */
typedef struct _pdinfo {
    uint            PhysIdx;    /* Physical index of this device (0 to n-1) */
    uint            Flags;
#define PDINFO_FLAGS_OPEN       0x00000001
#define PDINFO_FLAGS_LINKMASK   (PDINFO_FLAGS_LINK10|PDINFO_FLAGS_LINK100|PDINFO_FLAGS_LINK1000)
#define PDINFO_FLAGS_LINKFD     0x00000002
#define PDINFO_FLAGS_LINK10     0x00000004
#define PDINFO_FLAGS_LINK100    0x00000008
#define PDINFO_FLAGS_LINK1000   0x00000010
    UINT8           bMacAddr[6];/* MAC Address */
    uint            Filter;     /* Current RX filter */
    uint            MCastCnt;   /* Current MCast Address Countr */
    UINT8           bMCast[6*PKT_MAX_MCAST];
    uint            TxFree;     /* Transmitter "free" flag */
} PDINFO;

#define NUM_PORT            1
#define NUM_RX_BUFFERS      60
#define NUM_TX_DESC         30 /* Actually using only 1 desc for tx */
#define SIZE_SRC_ADDRESS    6
#define SIZE_DEST_ADDRESS   6
#define SIZE_TYPE           2
#define SIZE_PPPOE          8
#define SIZE_PKTBUFFER      1536

#define CPPI4_PKTTYPE_ETHERNET              7

/*
// ** Interrupt Allocation **
//
// Note that we only have interrupts defined for one EMAC
// instance. If there were more EMACs, we'd have to have an
// array of these values.
*/
#define ACHAN_RXINT     0
#define AVECT_RXINT     0
#define ACHAN_TXINT     2
#define AVECT_TXINT     1
#define INTC_RXINT      (INTCACCUMBASE+AVECT_RXINT)
#define INTC_TXINT      (INTCACCUMBASE+AVECT_TXINT)

/*
// ** Queue Allocation **
//
// RXQ Range  : 0 to 15
// RXFQ Range : 0 to 31
// TXCQ Range : 0 to 3
*/
#define RXQHOST       0         /* Host RXQ */
#define RXQHOSTTD     8         /* Host TeardownQ */
#define RXFQHOST      0         /* Host RXFREEQ */
#define TXCQHOST      0         /* Host TXCQ */

/*
// ** Descriptor "hint" size for QM **
//
// The descriptor size code is:
//    Code = (size-24)/4
//
// For host descriptors, we need to read the first 40 bytes, thus
// Code = (40-24)/4 = 4
*/
#define HMPD_SZCODE     4

/*
// ** RX/TX Descriptor Accumulator Page Information **
*/
UINT32 *RxDescPageBuffer[NUM_PORT];
#define RXDESC_NUMPAGES     2
#define RXDESC_NUMENTRIES   16
int RxDescCurrentPage[NUM_PORT];
UINT32 *TxDescPageBuffer[NUM_PORT];
#define TXDESC_NUMPAGES     2
#define TXDESC_NUMENTRIES   16
int TxDescCurrentPage[NUM_PORT];

static void* descMemBase = NULL;
static UINT8 pBufMem[NUM_RX_BUFFERS*SIZE_PKTBUFFER];

PDINFO    *localPDI[NUM_PORT];
PDINFO    pdi;

#define FULL_DUPLEX       0x00000001

void *cpMacMdioState[2];
bit32u cpMacMdioMask[2]={0xaaaaaaaa,0x55555554};

unsigned int BaseAddr[2] = { 0x0304E000, 0x0304E800 };
unsigned int ResetBit[2] = { 17, 21 };

CPPI41_HMPD *bdTxFreeList = 0;

int HwPktInit(void);
void HwPktClose( PDINFO *pi );
uint HwPktOpen( PDINFO *pi );
void HwPktShutdown(void);

#if 1
#define dcache_wbi(address,len)
#define dcache_i(address,len)
#else
void dcache_wbi(void *address, unsigned len);
void dcache_i(void *address, unsigned len);
#endif

void adaptercheck(int inst);
void CpGmacF_Init(unsigned inst, bit32u macbase, int resetbit, bit32u memoffset);
void CppiDMACfg(unsigned inst);
int  PhyCheck(bit32u inst);
void PhyRate(bit32u inst);
int CpEmacTxInt(bit32u inst);
int CpEmacRxInt(bit32u inst);
int  GetBiaMacAddr( UINT8 macindex, UINT8 *macaddrout );

#ifndef CONFIG_PUMA5_VOLCANO_EMU
extern void reset_ether_phy (int);
#endif /* !CONFIG_PUMA5_VOLCANO_EMU */

#define DBG_RX_DESC  0x0001
#define DBG_RX_PKT   0x0002
#define DBG_TX_DESC  0x0010
#define DBG_TX_PKT   0x0020

#ifdef ETH_DEBUG
int dbg_mask_g = 0;

void set_eth_dbg (int dbg_mask)
{
    dbg_mask_g = dbg_mask;
}
#endif

/* eth_init -
 * Initialize and set up thernet device.
 */
int eth_init(bd_t *bis)
{
#ifdef ETH_DEBUG
    printf ("Debug mask @%#x\n", &dbg_mask_g);
#endif

    if (HwPktInit () < 0)
    {
        printf ("HwPktInit () Failed.\n");
        return -1;
    }

    /* Initialize PDINFO for our driver instance */
    pdi.PhysIdx     = 0;
    pdi.MCastCnt    = 0;

    /* Default MAC Address (can be overwritten by HwPktOpen()) */
    pdi.bMacAddr[0] = bis->bi_enetaddr[0];
    pdi.bMacAddr[1] = bis->bi_enetaddr[1];
    pdi.bMacAddr[2] = bis->bi_enetaddr[2];
    pdi.bMacAddr[3] = bis->bi_enetaddr[3];
    pdi.bMacAddr[4] = bis->bi_enetaddr[4];
    pdi.bMacAddr[5] = bis->bi_enetaddr[5];

    if (HwPktOpen (&pdi) != 1)
    {
        printf ("HwPktOpen () Failed.\n");
        return -1;
    }

    udelay (20000);

    /* !@@ Restore the new MAC in board info since we have env at NOWHERE - FIXME */
    /* Default MAC Address (can be overwritten by HwPktOpen()) */
    bis->bi_enetaddr[0] = pdi.bMacAddr[0];
    bis->bi_enetaddr[1] = pdi.bMacAddr[1];
    bis->bi_enetaddr[2] = pdi.bMacAddr[2];
    bis->bi_enetaddr[3] = pdi.bMacAddr[3];
    bis->bi_enetaddr[4] = pdi.bMacAddr[4];
    bis->bi_enetaddr[5] = pdi.bMacAddr[5];

    adaptercheck (0);
    {
        extern int ctrlc (void);
        int timeout = 0x8000, rc;

        do {
        if (ctrlc())
            return (-1);

                udelay (1000);
                rc = PhyCheck (0);
        } while (!rc && --timeout);

        if (!timeout) {
            printf ("Link TIMEOUT.\n");
            return -1;
        }
    }

    udelay (20000);

    return 0;
}

/* eth_send -
 */
s32 eth_send(volatile void *packet, s32 length)
{
    CPPI41_HMPD *bd;
#define CPGMAC_TX_TIMEOUT       50000
#define CPGMAC_LINK_TIMEOUT     0xffff
    unsigned int timeout = CPGMAC_TX_TIMEOUT;

#ifdef ETH_DEBUG
    if (dbg_mask_g & DBG_TX_DESC)
        printf ("%s:%d\n", __FUNCTION__, __LINE__);
#endif

    int rc;

        rc = PhyCheck (0);
    //} while (!rc && --link_timeout);

    //if (!link_timeout)
    //{
    //    printf ("eth_send: LINK Fail\n");
    //    return -1;
    //}
    //printf ("%s: PhyCheck returned %d\n", __FUNCTION__, rc);

    bd = bdTxFreeList;
    if( bd )
    {
        bdTxFreeList = bd->pNextPriv;
    }
    else
    {
        printf("Tx Buffer Descriptors Exhausted\n");
        return -1;
    }

    bd->DescInfo      = CPPI41_HM_DESCINFO_DTYPE_HOST | length;
    bd->DataLength    = length;
    bd->DataPtr       = (UINT32)packet;
    bd->NextDescPtr   = 0;

#ifdef ETH_DEBUG
    if (dbg_mask_g & DBG_TX_DESC)
        printf ("%s: Put BD %#x\n", __FUNCTION__, bd);

    if (dbg_mask_g & DBG_TX_PKT)
    {
        printf ("Pkt len = %d\n", length);
        printf ("PKT:\n");
        {
            int i;
            for (i = 0; (i < length && i < 32); i++)
                    printf ("%x", *((volatile char *)packet++));
        }
    }
#endif

    dcache_wbi((void *)packet, length);
    dcache_wbi((void*)bd,64);
    /* Push buffer onto EMAC0 TXQ */
    Cppi4PushEMAC0TXQ(bd|HMPD_SZCODE,0);

    /* Wait for Tx Complete */
    while (timeout--)
    {
        if(CpEmacTxInt (0) == 0)
            break;
    }

    if (timeout == 0) {
        printf ("ERROR: Tx timed out\n");
        return -1;
    }

    return 0;
}

/*--------------------------------------------------------------------
// HwPktInit()
//
// Initialize Device environment and return instance count
//--------------------------------------------------------------------
*/
int HwPktInit()
{
    UINT32  shift,descpoolsize,descnum;
    UINT32  pRcbMem;
    int     i= 0,iRcbSize;
    CPPI41_HMPD *bd;
    UINT8   *pBufTmp;
    uint    rc=0;
    char * tmp;

    if (NULL != (tmp = getenv("eth0_mdio_phy_addr")))
    {
        int eth_mdio_phy_addr = simple_strtoul(tmp, NULL, 0);
        cpMacMdioMask[i]=  1 << eth_mdio_phy_addr;
    }
    else
    {
        cpMacMdioMask[i]=MDIO_PHY_MASK;
    }

#ifndef CONFIG_PUMA5_VOLCANO_EMU
    reset_ether_phy (0);
#endif /* !CONFIG_PUMA5_VOLCANO_EMU */


    /*--------------------------------------------------------
    //
    // Setup the EMAC queue/descriptor environment
    */

    /* Setup QM to use all of on-chip RAM for linking */
    QM_LINKRAM_RGN0BASE = 0x3160000;
    QM_LINKRAM_RGN0SIZE = 1024;
    QM_LINKRAM_RGN1BASE = 0;

    descnum = NUM_RX_BUFFERS+NUM_TX_DESC;

    /* Setup QM to use all of on-chip RAM for linking */
    QM_LINKRAM_RGN0BASE = 0x3160000;
    QM_LINKRAM_RGN0SIZE = 1024;
    QM_LINKRAM_RGN1BASE = 0;

    /*---------------------------------------------------------------------------
    // Adjust the number of descriptors in the pool to a power of 2, in the
    // range from 32 to 4096. We'll still only initialize (and allocate buffers
    // for) the exact number requested by the caller.
    //---------------------------------------------------------------------------
    */
    descpoolsize = descnum-1;
    for(shift=31; shift; shift--)
        if( descpoolsize&(1<<shift) )
            break;
    shift++;
    if( shift<5 )
        shift=5;
    else if( shift>12 )
        shift=12;
    descpoolsize = 1<<shift;
    if( descnum>descpoolsize )
        { printf("Error %d blocks requested, %d blocks available\n",descnum,descpoolsize); return -1; }

    /*---------------------------------------------------------------------------
    //  Round buffer descriptor size up to nearest multiple of 64 bytes (buffer
    //  descriptors must be 64-bytes aligned). Allocate block of buffer
    //  descriptors, and align to nearest 64-byte boundary.
    //---------------------------------------------------------------------------
    */
    iRcbSize = sizeof(CPPI41_HMPD);
    iRcbSize += (64-1);
    iRcbSize &= ~(64-1);

    pRcbMem = (UINT32)malloc(iRcbSize * (descnum+1));
    if(!pRcbMem)
        { printf("Could not Malloc RCB block"); return -1; }
    descMemBase = (void *)pRcbMem;
    pRcbMem += (64-1);
    pRcbMem &= ~(64-1);

#ifdef ETH_DEBUG
    printf("DescPool: %u desc (poolsize %u) with %u byte descriptors starting @ 0x%08x\n",descnum,descpoolsize,iRcbSize,pRcbMem);
#endif

    /*---------------------------------------------------------------------------
    //  Program the descriptor region 0
    //---------------------------------------------------------------------------
    */
    QM_DESCRAM_BASE(0) = (pRcbMem);
    QM_DESCRAM_CTRL(0) = (1<<8) | (shift-5);

    /*---------------------------------------------------------------------------
    //  Reset the queues we'll be using
    //---------------------------------------------------------------------------
    */
    Cppi4PushRXFQ(0,RXFQHOST);      /* Reset Host Rx Free Queue */
    QM_RXFQ_STARVECNT(RXFQHOST);    /* Reset Host Free Queue Starvation Count */
    Cppi4PushRXQ(0,RXQHOST);        /* Reset Host Rx Queue */
    Cppi4PushRXQ(0,RXQHOSTTD);      /* Reset Host Teardown Queue */
    Cppi4PushTXCQ(0,TXCQHOST);      /* Reset Host Tx Completion Queue */

    Cppi4PushEMAC0TXQ(0,0);         /* Reset EMAC0 Tx Queue Pri 0 */
    Cppi4PushEMAC0TXQ(0,1);         /* Reset EMAC0 Tx Queue Pri 1 */

    /* Get temp pointers */
    pBufTmp = pBufMem;

    /*---------------------------------------------------------------------------
    //  Push RX buffer descriptor/buffer onto Rx Free Queue
    //---------------------------------------------------------------------------
    */
    for(i=0;i<NUM_RX_BUFFERS;i++)
    {
        /* Setup our buffer desc */
        bd = (CPPI41_HMPD *)pRcbMem;

        /* Set these packets to go back to the RX free queue on TX */
        bd->DescInfo      = CPPI41_HM_DESCINFO_DTYPE_HOST;
        bd->SrcDstTag     = 0;
        bd->PktInfo = (CPPI4_PKTTYPE_ETHERNET<<CPPI41_HM_PKTINFO_PKTTYPE_SHIFT) |
                            CPPI41_HM_PKTINFO_RETPOLICY_SPLIT | QM_RXFQ(RXFQHOST);
        bd->DataLength    = 0;
        bd->DataPtr       = 0;
        bd->NextDescPtr   = 0;
        bd->BufPtr        = (UINT32)((pBufTmp) + PKT_PREPAD);
        bd->BufLength     = SIZE_PKTBUFFER - PKT_PREPAD;

        dcache_i( (void *)pBufTmp, SIZE_PKTBUFFER );
        dcache_wbi((void*)pRcbMem,64);
        /* Push buffer descriptor/buffer onto Host RXFQ */
        Cppi4PushRXFQ((bd)|HMPD_SZCODE,RXFQHOST);
        pRcbMem += iRcbSize;                /* Next buffer descriptor */
        pBufTmp += SIZE_PKTBUFFER;
    }

    /*---------------------------------------------------------------------------
    //  Put TX descriptors into a free pool for later use by TX
    //---------------------------------------------------------------------------
    */
    bdTxFreeList = 0;
    for( i=0; i<NUM_TX_DESC; i++ )
    {
        /* Setup our buffer desc */
        bd = (CPPI41_HMPD *)pRcbMem;

        /* Set these packets to go to the TX completion queue on TX */
        bd->DescInfo      = CPPI41_HM_DESCINFO_DTYPE_HOST;
        bd->SrcDstTag     = 0;
        bd->PktInfo = (CPPI4_PKTTYPE_ETHERNET<<CPPI41_HM_PKTINFO_PKTTYPE_SHIFT) |
                            CPPI41_HM_PKTINFO_RETPOLICY_SPLIT | QM_TXCQ(TXCQHOST);
        bd->NextDescPtr   = 0;
        bd->BufPtr        = 0;
        bd->BufLength     = 0;

        bd->pNextPriv = bdTxFreeList;
        bdTxFreeList = bd;
        pRcbMem += iRcbSize;                /* Next buffer descriptor */
    }

    return(rc);
}

/* Halt ethernet engine */
void eth_halt ()
{
    HwPktClose (&pdi);
    HwPktShutdown ();
}

/*--------------------------------------------------------------------
// HwPktShutdown()
//
// Shutdown Device Environment
//--------------------------------------------------------------------
*/
void HwPktShutdown()
{

#ifndef CONFIG_PUMA5_VOLCANO_EMU
    reset_ether_phy (1);
#endif /* !CONFIG_PUMA5_VOLCANO_EMU */


    if (descMemBase) {
        free (descMemBase);
        descMemBase = NULL;
    }

    /* TODO : Reset QM configs */

}

/*--------------------------------------------------------------------
// HwPktOpen()
//
// Open Device Instance
//--------------------------------------------------------------------
*/
uint HwPktOpen( PDINFO *pi )
{
    uint idx = pi->PhysIdx;
    bit32u  HdrSize;
    bit32u  PacketPad;

    localPDI[idx] = pi;

    if( idx!= 0 )
    {
        printf("FATAL ERROR: This HwPktOpen() only supports 1 instance\n");
        return(0);
    }

    if(GetBiaMacAddr( (char)idx, localPDI[idx]->bMacAddr )!=0)
        return 0;

    HdrSize =  SIZE_SRC_ADDRESS + SIZE_DEST_ADDRESS + SIZE_TYPE + SIZE_PPPOE;
    PacketPad = 0;  /* =4 if CRC passed up */

    /*--------------------------------------------------------
    //
    // Startup the EMAC
    */
    CpGmacF_Init(idx,BaseAddr[idx],ResetBit[idx],0);

    localPDI[idx]->Flags &= ~PDINFO_FLAGS_LINKMASK;

    /* Don't allow packet tranmission until link */
    pi->TxFree = 0;

    return(1);
}

/*--------------------------------------------------------------------
// HwPktClose()
//
// Close Device Instance
//--------------------------------------------------------------------
*/
void HwPktClose( PDINFO *pi )
{
    uint idx = pi->PhysIdx;

    if( idx != 0 )
    {
        printf("FATAL ERROR: This HwPktClose() only supports 1 instance\n");
        return;
    }


    localPDI[idx]->Flags &= ~PDINFO_FLAGS_LINKMASK;

    CDMA_CH_HOSTPKTCFGA(0)  = 0;
    CDMA_CH_HOSTPKTCFGB(0)  = 0;
    CDMA_CH_EMBEDPKTCFGA(0) = 0;
    CDMA_CH_EMBEDPKTCFGB(0) = 0;
    CDMA_CH_MONOPKTCFG(0)   = 0;
    CDMA_CH_TXGLBLCFG(0)    = 0;
    CDMA_CH_RXGLBLCFG(0)    = 0;

    udelay (1000);

    CPGMACF_MACCONTROL(BaseAddr[idx]) &= ~MAC_EN;

    CPGMACF_MACCONTROL(BaseAddr[idx]) &= ~GIG_MODE;
    CPGMACF_MACCONTROL(BaseAddr[idx]) &= ~GMII_EN;


    if (cpMacMdioState[idx]) {
        free (cpMacMdioState[idx]);
        cpMacMdioState[idx] = NULL;
    }
}

/*--------------------------------------------------------------------
// _HwPktPoll()
//
// Poll routine - CALLED OUTSIDE OF KERNEL MODE
//
// This function is called at least every 100ms, faster in a
// polling environment. The fTimerTick flag is set only when
// called on a 100ms event.
//--------------------------------------------------------------------
*/
void _HwPktPoll( PDINFO *pi, uint fTimerTick )
{
    uint idx = pi->PhysIdx;

    CpEmacRxInt(idx);
    CpEmacTxInt(idx);

#if 0
    if(fTimerTick)
    {
        // Drive MDIO engine
        if( PhyCheck(idx) )
        {
            while( !pi->TxFree )
                HwPktTxNext( pi, 0 );
        }
    }
#endif
}

void adaptercheck(int inst)
{
    bit32u utmp;
    utmp = CPGMACF_MACSTATUS(BaseAddr[inst]);
#ifdef ETH_DEBUG
    printf("Adaptercheck: %08x\n",utmp);
#endif
}

void CpGmacF_Init(unsigned inst, bit32u macbase, int resetbit, bit32u memoffset)
{
    int cpMacMdioStateSize;
    int  i;
    uint tmp;
    int cpufreq;

#ifdef ETH_INFO
    printf("Init CPGMAC_F: %d ",inst);
    printf("Module:%d, Version:%2d.%02d\n",(*((bit32u*)macbase)>>16)&0x3fff,(*((bit32u*)macbase)>>8)&0x0ff,(*((bit32u*)macbase)&0x0ff));
#endif

    // RX Packet Filtering
    CPGMACF_RX_MBP_ENABLE(macbase) = 0; //!@@ RX_BROAD_EN | RX_MULTI_EN;
    CPGMACF_RX_UNICAST_SET(macbase) = 1;
    CPGMACF_MACHASH1(macbase) = 0;
    CPGMACF_MACHASH2(macbase) = 0;

    CPGMACF2_MACINDEX(macbase) = 0;
    CPGMACF2_QUEUEINFO(macbase) = (3<<14) | 0xFFF;
    tmp=0;
    for( i=3; i>=0; i-- )
    {
        tmp <<= 8;
        tmp |= localPDI[inst]->bMacAddr[i];
    }
    CPGMACF2_MACADDR_HI(macbase) = tmp;
    tmp = (3<<19) | (localPDI[inst]->bMacAddr[5]<<8) | localPDI[inst]->bMacAddr[4];
    CPGMACF2_MACADDR_LO(macbase) = tmp;

    // Init Tx and Rx DMA
    CppiDMACfg(inst);

    // Init MAC
    //CPGMACF_MACCONTROL(macbase) |= GMII_EN;
    CPGMACF_MACCONTROL(macbase) |= GIG_MODE;
    //CPGMACF_MACCONTROL(macbase) |= (1<<15);
    //CPGMACF_MACCONTROL(macbase) |= (1<<16);
    //CPGMACF_MACCONTROL(macbase) |= (1<<17);
    //CPGMACF_MACCONTROL(macbase) |= (1<<18);

    CPGMACF_MACCONTROL(macbase) |= MAC_EN;
    CPGMACF_MAC_INTMASK_SET(macbase) |= (1<<1);     //Enable Adaptercheck Ints

    // Initialize the MDIO module for this EMAC instance
    cpMacMdioStateSize=cpMacMdioGetPhyDevSize();
    cpMacMdioState[inst]=malloc(cpMacMdioStateSize);

#ifdef CONFIG_PUMA5_VOLCANO_EMU
    cpufreq = AVALANCHE_VBUS_CLKC_FREQ *2;// !@@@*/;
    cpMacMdioInit(cpMacMdioState[inst],0x03048000,inst,cpMacMdioMask[inst],0,0x08611600,22,cpufreq,0);
    cpMacMdioSetPhyMode(cpMacMdioState[inst],NWAY_AUTO|NWAY_FD100|NWAY_FD10|NWAY_HD10);
#else
    cpufreq = AVALANCHE_VBUS_CLKC_FREQ;
    cpMacMdioInit(cpMacMdioState[inst],0x03048000,inst,cpMacMdioMask[inst],0,0/*IGNORED*/,0/*IGNORED*/,cpufreq,0/*VERBOSE*/);
    cpMacMdioSetPhyMode(cpMacMdioState[inst],NWAY_AUTO|NWAY_FD1000|NWAY_FD100|NWAY_HD100|NWAY_FD10|NWAY_HD10);
#endif

    // Start mode
#ifdef ETH_DEBUG
    printf("Start MAC: %d\n",inst);
#endif
    CPGMACF_MACCONTROL(macbase) |= FULL_DUPLEX;
}

void CppiDMACfg(unsigned inst)
{
    unsigned int channel;

    if( inst != 0  )
        return;

    CDMA_TEARDOWN_DESCQ = 0;

    channel = CDMA_CH_EMAC0;
    CDMA_CH_HOSTPKTCFGA(channel)  = (QM_RXFQ(RXFQHOST)<<16) | QM_RXFQ(RXFQHOST);
    CDMA_CH_HOSTPKTCFGB(channel)  = (QM_RXFQ(RXFQHOST)<<16) | QM_RXFQ(RXFQHOST);
    CDMA_CH_EMBEDPKTCFGA(channel) = 0;
    CDMA_CH_EMBEDPKTCFGB(channel) = 0;
    CDMA_CH_MONOPKTCFG(channel)   = 0;
    CDMA_CH_TXGLBLCFG(channel)    = (1<<31)| QM_RXQ(RXQHOSTTD);
    CDMA_CH_RXGLBLCFG(channel)    = (1<<31)|(1<<14)|QM_RXQ(RXQHOST);

    CDMASCHEDULER      = 0x80000001;
    CDMASCHEDULETTABLE = 0x00008808;
}

void PhyRate(bit32u inst)
{
    int EmacDuplex,EmacSpeed,PhyNum;

    if (cpMacMdioGetLinked(cpMacMdioState[inst]))
    {
        EmacDuplex=cpMacMdioGetDuplex(cpMacMdioState[inst]);
        EmacSpeed=cpMacMdioGetSpeed(cpMacMdioState[inst]);
        PhyNum=cpMacMdioGetPhyNum(cpMacMdioState[inst]);
#ifdef ETH_INFO
        printf("Emac %d: Ethernet Phy= %d, Speed=%sM bps, Duplex=%s\n",inst,PhyNum,(EmacSpeed==3)? "1000" : (EmacSpeed ? "100":"10"),(EmacDuplex)?"Full":"Half");
#endif

        if (EmacSpeed == 3) {
#ifdef ETH_DEBUG
            printf ("Setting GIG bits...\n");
#endif
            CPGMACF_MACCONTROL(BaseAddr[inst]) |= GIG_MODE;
            CPGMACF_MACCONTROL(BaseAddr[inst]) |= (1<<17);
            //CPGMACF_MACCONTROL(BaseAddr[inst]) |= GMII_EN;
        } else {
#ifdef ETH_DEBUG
                printf ("Clearing GIG bits...\n");
#endif
            CPGMACF_MACCONTROL(BaseAddr[inst]) &= ~GIG_MODE;
            CPGMACF_MACCONTROL(BaseAddr[inst]) &= ~(1<<17);
            //CPGMACF_MACCONTROL(BaseAddr[inst]) &= ~GMII_EN;
        }
    }
    else
    {
        printf("Emac %d: Inactive\n",inst);
    }
}

int PhyCheck(bit32u inst)
{
    int EmacDuplex,EmacSpeed,PhyNum;

    if(cpMacMdioTic(cpMacMdioState[inst]))
    {
        if (cpMacMdioGetLinked(cpMacMdioState[inst]))
        {
            EmacDuplex=cpMacMdioGetDuplex(cpMacMdioState[inst]);
            EmacSpeed=cpMacMdioGetSpeed(cpMacMdioState[inst]);
            if( EmacSpeed == 1 )
                localPDI[inst]->Flags |= PDINFO_FLAGS_LINK100;
            else if( EmacSpeed == 0 )
                localPDI[inst]->Flags |= PDINFO_FLAGS_LINK10;

            PhyNum=cpMacMdioGetPhyNum(cpMacMdioState[inst]);
            if(EmacDuplex)
            {
                CPGMACF_MACCONTROL(BaseAddr[inst]) |= FULL_DUPLEX;
                localPDI[inst]->Flags |= PDINFO_FLAGS_LINKFD;
            }
            else
            {
                CPGMACF_MACCONTROL(BaseAddr[inst]) &= ~FULL_DUPLEX;
                localPDI[inst]->Flags &= ~PDINFO_FLAGS_LINKFD;
            }
            PhyRate(inst);
            return(1);
        }
        else
            localPDI[inst]->Flags &= ~PDINFO_FLAGS_LINKMASK;
    }
    return(0);
}


int CpEmacTxInt(bit32u inst)
{
    UINT32 phys_bd;
    CPPI41_HMPD *bd;
    unsigned char *pDescBuf;

    if ((phys_bd=(UINT32)Cppi4PopTXCQ(TXCQHOST))!=0)
    {
        pDescBuf = (unsigned char *) (phys_bd&~0x1f);

#ifdef ETH_DEBUG
        if (dbg_mask_g & DBG_TX_DESC)
            printf ("\n%s: Got BD %#x\n", __FUNCTION__, pDescBuf);
//        else
//            printf ("#");
#endif

        bd = (CPPI41_HMPD *)pDescBuf;

        bd->pNextPriv = bdTxFreeList;

        bdTxFreeList = bd;

        return 0;
    }
    else
        {
        return -1;
        }
}

int eth_rx (void)
{
    return CpEmacRxInt (0);
}

int CpEmacRxInt(bit32u inst)
{
    UINT32 phys_bd;
    int len = 0;
    CPPI41_HMPD *bd;
    unsigned char *pDescBuf;

    while ((phys_bd=(UINT32)Cppi4PopRXQ(RXQHOST))!=0)
    {
        pDescBuf = (unsigned char *) (phys_bd&~0x1f);
        bd = (CPPI41_HMPD *)pDescBuf;
#ifdef ETH_DEBUG
        if (dbg_mask_g & DBG_RX_DESC)
            printf ("%s: Got BD %#x\n", __FUNCTION__, pDescBuf);
#endif
        dcache_i((void *)bd->DataPtr, len /*SIZE_PKTBUFFER*/ /*!@0*/);
        len = bd->DataLength;

#ifdef ETH_DEBUG
        if (dbg_mask_g & DBG_RX_PKT)
        {
            printf ("Rx Pkt len = %d\n", len);
            printf ("PKT:\n");
            {
                int i;
                volatile char* packet = (volatile char *)(bd->DataPtr);
                for (i = 0; (i < len && i < 32); i++)
                        printf ("%x", *(packet++));
            }
        }
#endif

        /* call back u-boot -- may call eth_send() */
        NetReceive ((u8 *)bd->DataPtr, len);

        /* Return the buffer to the RX free queue */
        dcache_wbi((void*)bd,64);

        // Push buffer descriptor/buffer onto Host RXFQ
        Cppi4PushRXFQ(bd|HMPD_SZCODE,RXFQHOST);
    }

    return len;
}


/*
// Read Mac Address from 'ethaddr' environment setting.
// Return zero on success
*/
char* default_mac = "aabbcc001122";
int GetBiaMacAddr( UINT8 macindex, UINT8 *macaddrout )
{
    char *mac_string=0;
    int i,j;
    UINT8 c;
    UINT8 macaddr[6];

#ifndef CFG_NO_FLASH
    /*  Add MacAddress */
    mac_string=(char *)getenv("ethaddr");

    if(mac_string == NULL) {
        mac_string = default_mac;
        printf("\n*** Warning: 'ethaddr' not set! ***\n\nUsing default ->  aa:bb:cc:00:11:22\n");
    }
#else
    mac_string = default_mac;
#endif

    i = 0;
    j = 0;

    while( i<6 )
    {
        c = *mac_string++;
        if( c==':' || c=='.')
            continue;
        if( c==' ' )
            continue;
        if( c>='0' && c<='9' )
            c -= '0';
        else if( c>='a' && c<='f' )
            c -= 'a' - 10;
        else if( c>='A' && c<='F' )
            c -= 'A' - 10;
        else
            break;
        if( !j )
        {
            macaddr[i] = c * 16;
            j++;
        }
        else
        {
            macaddr[i] += c;
            j=0;
            i++;
        }
    }
    if( i!=6 )
        {
            printf("\n*** Error invalid mac address format 'bia'! ***\n");
            printf("\nUse format xx:xx:xx:xx:xx:xx \n");
            return -1;
        }

    macaddr[5] += macindex;
    memcpy( macaddrout, macaddr, 6 );

    return 0;
}
#endif /* CONFIG_COMMANDS & CFG_CMD_NET */

