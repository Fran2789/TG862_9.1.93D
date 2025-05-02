/*
 * cppi41.h
 * Description:
 * This file contains CPPI 4.1 Data Structures and definitions.
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

#define UINT32 unsigned int

///////////////////////////////////////////////////////////////////////////////////
//
// Host Buffer Descriptor
//
typedef struct _CPPI41_HMPD
{
  UINT32 DescInfo;
#define CPPI41_HM_DESCINFO_DTYPE_SHIFT     27
#define CPPI41_HM_DESCINFO_DTYPE_MASK      (0x1F<<CPPI41_HM_DESCINFO_DTYPE_SHIFT)
#define CPPI41_HM_DESCINFO_PSWSIZE_SHIFT   22
#define CPPI41_HM_DESCINFO_PSWSIZE_MASK    (0x1F<<CPPI41_HM_DESCINFO_PSWSIZE_SHIFT)
#define CPPI41_HM_DESCINFO_PKTLEN_SHIFT    0
#define CPPI41_HM_DESCINFO_PKTLEN_MASK     (0x003FFFFF<<CPPI41_HM_DESCINFO_PKTLEN_SHIFT)
  UINT32 SrcDstTag;
#define CPPI41_HM_SRCDSTTAG_SRCTAG_SHIFT   16
#define CPPI41_HM_SRCDSTTAG_SRCTAG_MASK    (0xFFFF<<CPPI41_HM_SRCDSTTAG_SRCTAG_SHIFT)
#define CPPI41_HM_SRCDSTTAG_DSTTAG_SHIFT   0
#define CPPI41_HM_SRCDSTTAG_DSTTAG_MASK    (0xFFFF<<CPPI41_HM_SRCDSTTAG_DSTTAG_SHIFT)
  UINT32 PktInfo;
#define CPPI41_HM_PKTINFO_PKTERROR_SHIFT   31
#define CPPI41_HM_PKTINFO_PKTERROR_MASK    (1<<CPPI41_HM_PKTINFO_PKTERROR_SHIFT)
#define CPPI41_HM_PKTINFO_PKTTYPE_SHIFT    26
#define CPPI41_HM_PKTINFO_PKTTYPE_MASK     (0x1F<<CPPI41_HM_PKTINFO_PKTTYPE_SHIFT)
#define CPPI41_HM_PKTINFO_RETPOLICY_SHIFT  15
#define CPPI41_HM_PKTINFO_RETPOLICY_MASK   (1<<CPPI41_HM_PKTINFO_RETPOLICY_SHIFT)
#define CPPI41_HM_PKTINFO_ONCHIP_SHIFT     14
#define CPPI41_HM_PKTINFO_ONCHIP_MASK      (1<<CPPI41_HM_PKTINFO_ONCHIP_SHIFT)
#define CPPI41_HM_PKTINFO_RETQMGR_SHIFT    12
#define CPPI41_HM_PKTINFO_RETQMGR_MASK     (0x3<<CPPI41_HM_PKTINFO_RETQMGR_SHIFT)
#define CPPI41_HM_PKTINFO_RETQ_SHIFT       0
#define CPPI41_HM_PKTINFO_RETQ_MASK        (0x7FF<<CPPI41_HM_PKTINFO_RETQ_SHIFT)
  UINT32 DataLength;            // Length of TX or RX data
  UINT32 DataPtr;               // Pointer to TX or RX data
  struct _CPPI41_HMPD *NextDescPtr;
  UINT32 BufLength;             // Length of the physical data buffer
  UINT32 BufPtr;                // Pointer to the physical data buffer
  UINT32 EPI[2];                // Extended Packet Information (from SR)
  UINT32 ProtSpecific[4];
  struct _CPPI41_HMPD *pNextPriv;       // Pointer to next descriptor (SW private use)
  void*  hPkt;                      // Private handle back to our packet type
} CPPI41_HMPD;

/* Commonly used values */
#define CPPI41_HM_DESCINFO_DTYPE_HOST      (16<<CPPI41_HM_DESCINFO_DTYPE_SHIFT)
#define CPPI41_HM_PKTINFO_RETPOLICY_SPLIT  (1<<CPPI41_HM_PKTINFO_RETPOLICY_SHIFT)


///////////////////////////////////////////////////////////////////////////////////
//
// Embedded Buffer Descriptor
//
#define EMSLOTCNT   4

typedef struct _CPPI41_EMPD_BUFF
{
  UINT32 BufInfo;
#define CPPI41_EM_BUFINFO_VALID_SHIFT     31
#define CPPI41_EM_BUFINFO_VALID_MASK      (0x1<<CPPI41_EM_BUFINFO_VALID_SHIFT)
#define CPPI41_EM_BUFINFO_BUFMGR_SHIFT    29
#define CPPI41_EM_BUFINFO_BUFMGR_MASK     (0x3<<CPPI41_EM_BUFINFO_BUFMGR_SHIFT)
#define CPPI41_EM_BUFINFO_BUFPOOL_SHIFT   24
#define CPPI41_EM_BUFINFO_BUFPOOL_MASK    (0x1F<<CPPI41_EM_BUFINFO_BUFPOOL_SHIFT)
#define CPPI41_EM_BUFINFO_BUFLEN_SHIFT    0
#define CPPI41_EM_BUFINFO_BUFLEN_MASK     (0x3FFFFF<<CPPI41_EM_BUFINFO_BUFLEN_SHIFT)
  UINT32 BufPtr;                // Pointer to the physical data buffer
} CPPI41_EMPD_BUFF;

typedef struct _CPPI41_EMPD
{
  UINT32 DescInfo;
#define CPPI41_EM_DESCINFO_DTYPE_SHIFT     30
#define CPPI41_EM_DESCINFO_DTYPE_MASK      (0x3<<CPPI41_EM_DESCINFO_DTYPE_SHIFT)
#define CPPI41_EM_DESCINFO_SLOTCNT_SHIFT   27
#define CPPI41_EM_DESCINFO_SLOTCNT_MASK    (0x7<<CPPI41_EM_DESCINFO_SLOTCNT_SHIFT)
#define CPPI41_EM_DESCINFO_PSWSIZE_SHIFT   22
#define CPPI41_EM_DESCINFO_PSWSIZE_MASK    (0x1F<<CPPI41_EM_DESCINFO_PSWSIZE_SHIFT)
#define CPPI41_EM_DESCINFO_PKTLEN_SHIFT    0
#define CPPI41_EM_DESCINFO_PKTLEN_MASK     (0x003FFFFF<<CPPI41_EM_DESCINFO_PKTLEN_SHIFT)
  UINT32 SrcDstTag;
#define CPPI41_EM_SRCDSTTAG_SRCTAG_SHIFT   16
#define CPPI41_EM_SRCDSTTAG_SRCTAG_MASK    (0xFFFF<<CPPI41_EM_SRCDSTTAG_SRCTAG_SHIFT)
#define CPPI41_EM_SRCDSTTAG_DSTTAG_SHIFT   0
#define CPPI41_EM_SRCDSTTAG_DSTTAG_MASK    (0xFFFF<<CPPI41_EM_SRCDSTTAG_DSTTAG_SHIFT)
  UINT32 PktInfo;
#define CPPI41_EM_PKTINFO_PKTERROR_SHIFT   31
#define CPPI41_EM_PKTINFO_PKTERROR_MASK    (1<<CPPI41_EM_PKTINFO_PKTERROR_SHIFT)
#define CPPI41_EM_PKTINFO_PKTTYPE_SHIFT    26
#define CPPI41_EM_PKTINFO_PKTTYPE_MASK     (0x1F<<CPPI41_EM_PKTINFO_PKTTYPE_SHIFT)
#define CPPI41_EM_PKTINFO_EOPIDX_SHIFT     20
#define CPPI41_EM_PKTINFO_EOPIDX_MASK      (0x7<<CPPI41_EM_PKTINFO_EOPIDX_SHIFT)
#define CPPI41_EM_PKTINFO_PROTSPEC_SHIFT   16
#define CPPI41_EM_PKTINFO_PROTSPEC_MASK    (0xF<<CPPI41_EM_PKTINFO_PROTSPEC_SHIFT)
#define CPPI41_EM_PKTINFO_RETPOLICY_SHIFT  15
#define CPPI41_EM_PKTINFO_RETPOLICY_MASK   (1<<CPPI41_EM_PKTINFO_RETPOLICY_SHIFT)
#define CPPI41_EM_PKTINFO_ONCHIP_SHIFT     14
#define CPPI41_EM_PKTINFO_ONCHIP_MASK      (1<<CPPI41_EM_PKTINFO_ONCHIP_SHIFT)
#define CPPI41_EM_PKTINFO_RETQMGR_SHIFT    12
#define CPPI41_EM_PKTINFO_RETQMGR_MASK     (0x3<<CPPI41_EM_PKTINFO_RETQMGR_SHIFT)
#define CPPI41_EM_PKTINFO_RETQ_SHIFT       0
#define CPPI41_EM_PKTINFO_RETQ_MASK        (0x7FF<<CPPI41_EM_PKTINFO_RETQ_SHIFT)
  CPPI41_EMPD_BUFF Buf[EMSLOTCNT];
  UINT32 EPI[2];                // Extended Packet Information (from SR)
  UINT32 ProtSpecific[3];
} CPPI41_EMPD;


/* Commonly used values */
#define CPPI41_EM_DESCINFO_SLOTCNT_MYCNT   (EMSLOTCNT<<CPPI41_EM_DESCINFO_SLOTCNT_SHIFT)
#define CPPI41_EM_DESCINFO_DTYPE_EMBEDDED  (0<<CPPI41_EM_DESCINFO_DTYPE_SHIFT)
#define CPPI41_EM_PKTINFO_RETPOLICY_RETURN (1<<CPPI41_EM_PKTINFO_RETPOLICY_SHIFT)



///////////////////////////////////////////////////////////////////////////////////
//
// Queue Manager
//
#define QM_REV                  (*(volatile UINT32 *)(0x0306a000))
#define QM_DIVERSION            (*(volatile UINT32 *)(0x0306a008))
#define QM_LINKRAM_RGN0BASE     (*(volatile UINT32 *)(0x0306a080))
#define QM_LINKRAM_RGN0SIZE     (*(volatile UINT32 *)(0x0306a084))
#define QM_LINKRAM_RGN1BASE     (*(volatile UINT32 *)(0x0306a088))
#define QM_DESCRAM_BASE(n)      (*(volatile UINT32 *)(0x0306b000+(16*n)))
#define QM_DESCRAM_CTRL(n)      (*(volatile UINT32 *)(0x0306b004+(16*n)))
#define QM_RXFQ_STARVECNT(n)    (*(volatile  UINT8 *)(0x0306a020+n))

#define QMQBASE                 0x03070000

// Generic Push and Pop
#define Cppi41PushQ(ptr,qnum)   *(volatile UINT32 *)(QMQBASE+(qnum*16+12))=(UINT32)ptr
#define Cppi41PopQ(qnum)        *(volatile UINT32 *)(QMQBASE+(qnum*16+12))

// Queue Number Assignments (flexible)
#define QM_TXCQ(num)            (96+num)
#define QM_RXQ(num)             (100+num)
#define QM_RXFQ(num)            (128+num)

// DMA Fixed Queue Number Assignments
#define QM_INF0TXQ(pri)         (196+pri)     // pri = 0 to 3, with 0 highest
#define QM_EMAC0TXQ(pri)        (212+pri)     // pri = 0 to 1, with 0 highest

// Map CPPI4 macros to CPPI4.1
#define Cppi4PushRXFQ(v,num)                Cppi41PushQ(v,QM_RXFQ(num))
#define Cppi4PopRXFQ(num)                   Cppi41PopQ(QM_RXFQ(num))
#define Cppi4PushRXQ(v,num)                 Cppi41PushQ(v,QM_RXQ(num))
#define Cppi4PopRXQ(num)                    Cppi41PopQ(QM_RXQ(num))
#define Cppi4PushTXCQ(v,num)                Cppi41PushQ(v,QM_TXCQ(num))
#define Cppi4PopTXCQ(num)                   Cppi41PopQ(QM_TXCQ(num))

#define Cppi4PushINF0TXQ(v,priority)        Cppi41PushQ(v,QM_INF0TXQ(priority))
#define Cppi4PushEMAC0TXQ(v,priority)       Cppi41PushQ(v,QM_EMAC0TXQ(priority))
#define Cppi4PopEMAC0TXQ(priority)          Cppi41PopQ(QM_EMAC0TXQ(priority))



///////////////////////////////////////////////////////////////////////////////////
//
// Buffer Manager
//
#define BM_REV                  (*(volatile UINT32 *)(0x03068000))
#define BM_RESET                (*(volatile UINT32 *)(0x03068004))
#define BM_REFCNT_INCVAL        (*(volatile UINT32 *)(0x03068028))
#define BM_REFCNT_POINTER       (*(volatile UINT32 *)(0x0306802c))
#define BM_FREEBUF_POINTER(n)   (*(volatile UINT32 *)(0x03068100+(8*n)))
#define BM_FREEBUF_SIZE(n)      (*(volatile UINT32 *)(0x03068104+(8*n)))
#define BM_FREEBUF_BASEADDR(n)  (*(volatile UINT32 *)(0x03068600+(8*n)))
#define BM_FREEBUF_CONTROL(n)   (*(volatile UINT32 *)(0x03068604+(8*n)))



///////////////////////////////////////////////////////////////////////////////////
//
// CPPI DMA
//

#define CDMA_CH_INF0                0
#define CDMA_CH_EMAC0               8

#define CDMA_REV                    (*(volatile UINT32 *)(0x0300B000))
#define CDMA_TEARDOWN_DESCQ         (*(volatile UINT32 *)(0x0300B004))

#define CDMACHBASE                  0x0300A000

#define CDMA_CH_TXGLBLCFG(ch)       (*(volatile UINT32 *)(CDMACHBASE+(ch*32)))
#define CDMA_CH_RXGLBLCFG(ch)       (*(volatile UINT32 *)(CDMACHBASE+(ch*32+8)))
#define CDMA_CH_HOSTPKTCFGA(ch)     (*(volatile UINT32 *)(CDMACHBASE+(ch*32+12)))
#define CDMA_CH_HOSTPKTCFGB(ch)     (*(volatile UINT32 *)(CDMACHBASE+(ch*32+16)))
#define CDMA_CH_EMBEDPKTCFGA(ch)    (*(volatile UINT32 *)(CDMACHBASE+(ch*32+20)))
#define CDMA_CH_EMBEDPKTCFGB(ch)    (*(volatile UINT32 *)(CDMACHBASE+(ch*32+24)))
#define CDMA_CH_MONOPKTCFG(ch)      (*(volatile UINT32 *)(CDMACHBASE+(ch*32+28)))

#define CDMASCHEDULER               (*(volatile UINT32 *)0x0300B800)
#define CDMASCHEDULETTABLE          (*(volatile UINT32 *)0x0300BC00)


///////////////////////////////////////////////////////////////////////////////////
//
// ACCUMULATOR, PDSP, AND INTERRUPTS
//
#define PDSPBASE                0x0300D000
#define PDSPIRAMBASE            0x03080000
#define PDSPCONTROL(x)          *(volatile UINT32 *)(PDSPBASE+(x*0x100))
#define PDSPIRAM(x,off)         *(volatile UINT32 *)(PDSPIRAMBASE+(x*0x8000)+(off*4))

#define CPDSP                       0
#define MPDSP                       1
#define QPDSP                       2
#define APDSP                       3

#define PDSP_CONTROL_NRESET         0x0001
#define PDSP_CONTROL_ENABLE         0x0002


/*
// Accumulator PDSP Command Interface
*/
#define APDSP_COMMAND           (*(volatile UINT32 *)(0x03167c00))
#define   ACMD_CHANNEL_SHIFT      0
#define   ACMD_CHANNEL_MASK       (0xFF<<ACMD_CHANNEL_SHIFT)
#define   ACMD_COMMAND_SHIFT      8
#define   ACMD_COMMAND_MASK       (0xFF<<ACMD_COMMAND_SHIFT)
#define   ACMD_RETCODE_SHIFT      24
#define   ACMD_RETCODE_MASK       (0xFF<<ACMD_RETCODE_SHIFT)
#define APDSP_LISTBUFFER        (*(volatile UINT32 *)(0x03167c04))
#define APDSP_CFGA              (*(volatile UINT32 *)(0x03167c08))
#define   ACFGA_QUEUE_SHIFT       0
#define   ACFGA_QUEUE_MASK        (0xFFFF<<ACFGA_QUEUE_SHIFT)
#define   ACFGA_MAXENTRY_SHIFT    16
#define   ACFGA_MAXENTRY_MASK     (0xFFFF<<ACFGA_MAXENTRY_SHIFT)
#define APDSP_CFGB              (*(volatile UINT32 *)(0x03167c0c))
#define   ACFGB_TIMERCNT_SHIFT    0
#define   ACFGB_TIMERCNT_MASK     (0xFFFF<<ACFGB_TIMERCNT_SHIFT)
#define   ACFGB_PAGECNT_SHIFT     16
#define   ACFGB_PAGECNT_MASK      (0x3<<ACFGB_PAGECNT_SHIFT)
#define   ACFGB_ENTRYSIZE_SHIFT   18
#define   ACFGB_ENTRYSIZE_MASK    (0x3<<ACFGB_ENTRYSIZE_SHIFT)
#define   ACFGB_COUNTMODE_SHIFT   20
#define   ACFGB_COUNTMODE_MASK    (0x1<<ACFGB_COUNTMODE_SHIFT)
#define   ACFGB_DELAYSTALL_SHIFT  21
#define   ACFGB_DELAYSTALL_MASK   (0x1<<ACFGB_DELAYSTALL_SHIFT)
#define   ACFGB_PACEMODE_SHIFT    22
#define   ACFGB_PACEMODE_MASK     (0x3<<ACFGB_PACEMODE_SHIFT)

#define ACMD_GET_COMMAND(x)     ((x&ACMD_COMMAND_MASK)>>ACMD_COMMAND_SHIFT)
#define ACMD_GET_RETCODE(x)     ((x&ACMD_RETCODE_MASK)>>ACMD_RETCODE_SHIFT)

#define ACMD_CHANNEL(x)         ((x<<ACMD_CHANNEL_SHIFT)&ACMD_CHANNEL_MASK)
#define ACMD_COMMAND(x)         ((x<<ACMD_COMMAND_SHIFT)&ACMD_COMMAND_MASK)
#define ACFGA_QUEUE(x)          ((x<<ACFGA_QUEUE_SHIFT)&ACFGA_QUEUE_MASK)
#define ACFGA_MAXENTRY(x)       ((x<<ACFGA_MAXENTRY_SHIFT)&ACFGA_MAXENTRY_MASK)
#define ACFGB_TIMERCOUNT(x)     ((x<<ACFGB_TIMERCNT_SHIFT)&ACFGB_TIMERCNT_MASK)
#define ACFGB_MAXPAGE(x)        ((x<<ACFGB_PAGECNT_SHIFT)&ACFGB_PAGECNT_MASK)
#define ACFGB_ENTRYSIZE(x)      ((x<<ACFGB_ENTRYSIZE_SHIFT)&ACFGB_ENTRYSIZE_MASK)
#define ACFGB_COUNTMODE(x)      ((x<<ACFGB_COUNTMODE_SHIFT)&ACFGB_COUNTMODE_MASK)
#define ACFGB_DELAYSTALL(x)     ((x<<ACFGB_DELAYSTALL_SHIFT)&ACFGB_DELAYSTALL_MASK)
#define ACFGB_PACEMODE(x)       ((x<<ACFGB_PACEMODE_SHIFT)&ACFGB_PACEMODE_MASK)

#define CMD_ENABLE              0x81
#define CMD_DISABLE             0x80
#define ENTRYSIZE_D             0
#define ENTRYSIZE_CD            1
#define ENTRYSIZE_ABCD          2
#define COUNTMODE_NULLTERM      0
#define COUNTMODE_COUNT         1
#define DELAYSTALL_NO           0
#define DELAYSTALL_YES          1
#define PACEMODE_NONE           0
#define PACEMODE_LASTINT        1
#define PACEMODE_FIRSTPKT       2
#define PACEMODE_LASTPKT        3

/*
// INTD Interface
*/
#define INTDBASE                0x03064000
#define INTDEOI                 (INTDBASE+0x010)
#define INTDSTATUS              (INTDBASE+0x204)
#define INTDCNTBASE             (INTDBASE+0x300)
#define INTD_EOI                (*(volatile UINT32 *)(INTDEOI))
#define INTD_STATUS             (*(volatile UINT32 *)(INTDSTATUS))
#define INTD_INTCOUNT(idx)      (*(volatile UINT32 *)(INTDCNTBASE+((idx)*4)))

/*
// INTC Interface
*/
#define INTCBASE                0x50000000
#define INTC_STATUS(idx)        (*(volatile UINT32 *)(INTCBASE+0x200+(idx)*4))
#define INTC_CLEAR(idx)         (*(volatile UINT32 *)(INTCBASE+0x280+(idx)*4))
#define INTC_POLARITY(idx)      (*(volatile UINT32 *)(INTCBASE+0xD00+(idx)*4))

#define INTCACCUMBASE           35
/* intc masks */
#define INTCACCUMMASK           0x7FFF8     
#define INTCCPGMACTXMASK        0x10
#define INTCCPGMACRXMASK        0x08

