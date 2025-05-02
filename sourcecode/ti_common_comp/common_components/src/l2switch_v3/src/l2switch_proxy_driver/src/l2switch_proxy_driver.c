/*
 * GPL LICENSE SUMMARY
 *
 *  Copyright(c) 2014 Intel Corporation.
 *
 *  This program is free software; you can redistribute it and/or modify 
 *  it under the terms of version 2 of the GNU General Public License as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful, but 
 *  WITHOUT ANY WARRANTY; without even the implied warranty of 
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU 
 *  General Public License for more details.
 *
 *
 *  You should have received a copy of the GNU General Public License 
 *  along with this program; if not, write to the Free Software 
 *  Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *  The full GNU General Public License is included in this distribution 
 *  in the file called LICENSE.GPL.
 *
 *  Contact Information:
 *  Intel Corporation
 *  2200 Mission College Blvd.
 *  Santa Clara, CA  97052
 */
/*! \file l2switch_proxy_driver.c
    \brief Implementation of l2switch proxy Driver. 
    \code.
*/
/**************************************************************************/
/*      INCLUDES:                                                         */
/**************************************************************************/
#include <linux/mman.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/proc_fs.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/inet_lro.h>
#include <asm-arm/arch-avalanche/puma6/puma6_hardware.h>
#include <asm-arm/arch-avalanche/puma6/puma6.h>
#include <linux/ioctl.h>  
#include <sys_ptypes.h> 
#include "_tistdtypes.h"
#include "l2switch_proxy_driver.h" 
#include "puma6.h"
#include "puma6_cppi.h"
#include "puma6_hardware.h"
#include <linux/swab.h>
#include <linux/cat_l2switch_netdev.h>
 
/**************************************************************************/
/*      LOCAL DECLARATIONS:                                               */
/**************************************************************************/
static long l2switch_proxy_ioctl(struct file *fd, unsigned int cmd, unsigned long arg);
static int prxpdsp_setup();
/**************************************************************************/
/*      STRUCTS:                                                          */
/**************************************************************************/
typedef struct
{
    volatile Uint32 reserved1[2];       /* Offset: 0x00, 0x04 */
    volatile Uint32 currDescr;          /* Offset: 0x08 */
    volatile Uint32 nextDescr;          /* Offset: 0x0C */
    volatile Uint32 srcDmaStart;        /* Offset: 0x10 */
    volatile Uint32 dstDmaStart;        /* Offset: 0x14 */
    volatile Uint32 srcDmaSize;         /* Offset: 0x18 */
    volatile Uint32 flagsMode;          /* Offset: 0x1C */
    volatile Uint32 otherMode;          /* Offset: 0x20 */
    volatile Uint32 reserved2[7];       /* Offset: 0x24 - 0x3F */
}udma_context_regs_t;

typedef struct
{
    udma_context_regs_t docsis_rx_port;         /* Offset: 0x00 - 0x3F */
    udma_context_regs_t docsis_tx_port;         /* Offset: 0x40 - 0x7F */
    volatile Uint32     reserved[2];            /* Offset: 0x80, 0x84 */
    volatile Uint32     docsis_rx_interrupts;   /* Offset: 0x88 */
    volatile Uint32     reserved2;              /* offset: 0x8C */
    volatile Uint32     docsis_tx_interrupts;   /* Offset: 0x90 */
}udma_regs_t;

typedef struct prxpdsp_udma_desc
{
    volatile Uint32 next;
    volatile Uint32 size;
    volatile Uint32 sourceAddress;
    volatile Uint32 destAddress;
    volatile Uint32 flagsMode;
}prxpdsp_udma_desc_t;

typedef struct prxpdsp_txudma_linklist_ram
{
    volatile Uint32         metaData;
    prxpdsp_udma_desc_t     metaDataDesc;
    prxpdsp_udma_desc_t     buffer0Desc;
    prxpdsp_udma_desc_t     buffer1Desc;
    prxpdsp_udma_desc_t     buffer2Desc;
    prxpdsp_udma_desc_t     buffer3Desc;
    prxpdsp_udma_desc_t     crcDesc;
    volatile Uint32         reserved;
}prxpdsp_txudma_linklist_ram_t;

typedef struct prxpdsp_rxudma_linklist_ram
{
    prxpdsp_udma_desc_t     bufferDesc;
    prxpdsp_udma_desc_t     dummyReadDesc;
}prxpdsp_rxudma_linklist_ram_t;



/**************************************************************************/
/*      DEFINES:                                                          */
/**************************************************************************/

#define FIRST_MINOR 0
#define MINOR_CNT 1


#define proxy_os_get_io_virt(addr)        ((void *)IO_ADDRESS((void *)(addr)))
//#define PP_PRXPDSP_COUNTERS_BASE        ((Uint32)proxy_os_get_io_virt(0x03587420))
#define PP_PRXPDSP_COUNTERS_BASE        ((Uint32)IO_ADDRESS((void *)(0x03587420)))

#define L2SWITCH_TX_DEST_ADDRESS                    0xFFFF0000
#define L2SWITCH_RX_SOURCE_ADDRESS                  0xFFFE0000
#define L2SWITCH_RX_DEST_ADDRESS_FOR_DUMMY_READ     0xFFF00000


/* UDMA FLAG register definitons */
#define UDMA_FLAG_DST_LINK_LIST_EN                  (0x1)       /* Destination Link List Enable: “0” – Linked-list mode disabled. “1” – Linked-list mode enabled. */
#define UDMA_FLAG_DST_ADDR_MODE_LINEAR              (0 << 1)    /* Destination Addressing Mode: “00” – Linear- addressing mode. */
#define UDMA_FLAG_DST_ADDR_MODE_FIXED               (2 << 1)    /* Destination Addressing Mode: “10” – Fixed – addressing mode. (transfer to fixed address until DSTDMA_SIZE == 0). */
#define UDMA_FLAG_DST_ADDR_MODE_FIXEDCONTINUOUS     (3 << 1)    /* Destination Addressing Mode: “11” –Fixed Continuous – addressing mode (transfer to a fixed address indefinitely). */
#define UDMA_FLAG_WRITE_EN                          (0x1 << 3)  /* The DMA will read data from the local address space and write it into the global address space (DMA direction is with respect to the Local Agent). */
#define UDMA_FLAG_SRC_LINK_LIST_EN                  (0x1 << 4)  /* Source Link List Enable: “0” – Linked-list mode disabled. “1” – Linked-list mode enabled.*/
#define UDMA_FLAG_SRC_ADDR_MODE_LINEAR              (0 << 5)    /* Source Addressing Mode: “00” – Linear- addressing mode. */
#define UDMA_FLAG_SRC_ADDR_MODE_FIXED               (2 << 5)    /* Source Addressing Mode: “10” – Fixed – addressing mode. (transfer to fixed address until SRCDMA_SIZE == 0). */
#define UDMA_FLAG_SRC_ADDR_MODE_FIXEDCONTINUOUS     (3 << 5)    /* Source Addressing Mode: “11” – Fixed Continuous – addressing mode (transfer to a fixed address indefinitely). */
#define UDMA_FLAG_READ_EN                           (0x1 << 7)  /* The DMA will read data from the global address space and write it into the local address space (DMA direction is with respect to the Local Agent). */
#define UDMA_FLAG_XBURST_SZ_4_BYTES                 (0 << 12)   /* The DMA will transfer data to the Global Agent in bursts of this value: 0000 – 4 bytes */
#define UDMA_FLAG_XBURST_SZ_8_BYTES                 (1 << 12)   /* The DMA will transfer data to the Global Agent in bursts of this value: 0001 – 8 bytes */
#define UDMA_FLAG_XBURST_SZ_16_BYTES                (2 << 12)   /* The DMA will transfer data to the Global Agent in bursts of this value: 0010 – 16 bytes */
#define UDMA_FLAG_XBURST_SZ_32_BYTES                (3 << 12)   /* The DMA will transfer data to the Global Agent in bursts of this value: 0011 – 32 bytes */
#define UDMA_FLAG_XBURST_SZ_64_BYTES                (4 << 12)   /* The DMA will transfer data to the Global Agent in bursts of this value: 0100 – 64 bytes */
#define UDMA_FLAG_XBURST_SZ_128_BYTES               (5 << 12)   /* The DMA will transfer data to the Global Agent in bursts of this value: 0101 – 128 bytes */
#define UDMA_FLAG_XBURST_SZ_256_BYTES               (6 << 12)   /* The DMA will transfer data to the Global Agent in bursts of this value: 0110 – 256 bytes */
#define UDMA_FLAG_XDMA_GAP_0_CLOCKS                 (0 << 16)   /* Every time a burst of data is transferred, the DMA will stop working on the context, swap it out, and then “lock” it for <0000 - 0> clockcycles*/
#define UDMA_FLAG_XDMA_GAP_16_CLOCKS                (1 << 16)   /* Every time a burst of data is transferred, the DMA will stop working on the context, swap it out, and then “lock” it for <0001 – 16> clockcycles*/
#define UDMA_FLAG_XDMA_GAP_64_CLOCKS                (2 << 16)   /* Every time a burst of data is transferred, the DMA will stop working on the context, swap it out, and then “lock” it for <0010 – 64> clockcycles*/
#define UDMA_FLAG_XDMA_GAP_256_CLOCKS               (3 << 16)   /* Every time a burst of data is transferred, the DMA will stop working on the context, swap it out, and then “lock” it for <0011 – 256> clockcycles*/
#define UDMA_FLAG_XDMA_GAP_1024_CLOCKS              (4 << 16)   /* Every time a burst of data is transferred, the DMA will stop working on the context, swap it out, and then “lock” it for <0100 – 1024> clockcycles*/
#define UDMA_FLAG_XDMA_GAP_2048_CLOCKS              (5 << 16)   /* Every time a burst of data is transferred, the DMA will stop working on the context, swap it out, and then “lock” it for <0101 – 2048> clockcycles*/
#define UDMA_FLAG_XDMA_GAP_4096_CLOCKS              (6 << 16)   /* Every time a burst of data is transferred, the DMA will stop working on the context, swap it out, and then “lock” it for <0110 – 4096> clockcycles*/
#define UDMA_FLAG_XDMA_GAP_8192_CLOCKS              (7 << 16)   /* Every time a burst of data is transferred, the DMA will stop working on the context, swap it out, and then “lock” it for <0111 – 8192> clockcycles*/
#define UDMA_FLAG_PACKET_POSITION_CONTINUATION      (0 << 21)   /* Encodes the position of Data transferred with relation to the Packet: 000 - Continuation - Default */
#define UDMA_FLAG_PACKET_POSITION_START             (1 << 21)   /* Encodes the position of Data transferred with relation to the Packet: 001 - This buffer is starting a packet */
#define UDMA_FLAG_PACKET_POSITION_END               (2 << 21)   /* Encodes the position of Data transferred with relation to the Packet: 010 - This buffer is ending a packet */
#define UDMA_FLAG_PACKET_POSITION_START_AND_END     (3 << 21)   /* Encodes the position of Data transferred with relation to the Packet: 011 - This buffer is starting and ending of a packet*/
#define UDMA_FLAG_DST_ENDIANISM_BIG                 (0x1 << 24) /* Destination Endianism: Indicates what the destination endianism is and is used by hardware to generate the appropriate byte enables for non-32bit aligned accesses. ‘1’ – Big Endianism, ‘0’ – Little Endianism. */
#define UDMA_FLAG_SRC_ENDIANISM_BIG                 (0x1 << 25) /* Source Endianism: Indicates what the source endianism is and is used by hardware to generate the appropriate byte enables for non-32bit aligned accesses. ‘1’ – Big Endianism, ‘0’ – Little Endianism. */
#define UDMA_FLAG_TERM                              (0x1 << 28) /* Marks the end of a linked-list chain. Do not fetch the next descriptor when the current transfer has finished (byte counter reaches zero). ‘1’ = terminate, ‘0’ = go read next descriptor. NOTE: (Valid only in linked-list mode). */
#define UDMA_FLAG_DST_INT_EN                        (0x1 << 29) /* Interrupt when the transfer to the destination buffer has finished (destination buffer becomes full). ‘1’ = enable interrupt, ‘0’ = do not interrupt */
#define UDMA_FLAG_SRC_INT_EN                        (0x1 << 30) /* Interrupt when the transfer from the source buffer has finished (source buffer becomes empty). ‘1’ = enable interrupt, ‘0’ = do not interrupt */

#define UDMA_FLAGS_COMMON                           UDMA_FLAG_DST_LINK_LIST_EN | /* = 0x4095 */ \
                                                    UDMA_FLAG_DST_ADDR_MODE_FIXED |             \
                                                    UDMA_FLAG_SRC_LINK_LIST_EN |                \
                                                    UDMA_FLAG_SRC_ADDR_MODE_LINEAR |            \
                                                    UDMA_FLAG_READ_EN |                         \
                                                    UDMA_FLAG_XDMA_GAP_0_CLOCKS |               \
                                                    UDMA_FLAG_XBURST_SZ_128_BYTES

#define UDMA_FLAGS_LINK_LIST_START                  UDMA_FLAG_PACKET_POSITION_START | UDMA_FLAGS_COMMON             /* = 0x204095 */
#define UDMA_FLAGS_LINK_LIST_MIDDLE                 UDMA_FLAG_PACKET_POSITION_CONTINUATION | UDMA_FLAGS_COMMON      /* = 0x4095 */
#define UDMA_FLAGS_LINK_LIST_END                    UDMA_FLAG_PACKET_POSITION_END | UDMA_FLAGS_COMMON | UDMA_FLAG_TERM | UDMA_FLAG_DST_INT_EN   /* = 0x30404095 */

#define UDMA_FLAGS_LINK_LIST_RX_START               UDMA_FLAG_PACKET_POSITION_START |           \
                                                    UDMA_FLAG_DST_LINK_LIST_EN |                \
                                                    UDMA_FLAG_DST_ADDR_MODE_LINEAR |            \
                                                    UDMA_FLAG_WRITE_EN |                        \
                                                    UDMA_FLAG_SRC_LINK_LIST_EN |                \
                                                    UDMA_FLAG_SRC_ADDR_MODE_FIXED |             \
                                                    UDMA_FLAG_XDMA_GAP_0_CLOCKS |               \
                                                    UDMA_FLAG_XBURST_SZ_128_BYTES


#define UDMA_FLAGS_LINK_LIST_END_DUMMY_READ         UDMA_FLAG_DST_LINK_LIST_EN |    /* = 0x304000D1 */  \
                                                    UDMA_FLAG_SRC_LINK_LIST_EN |                        \
                                                    UDMA_FLAG_SRC_ADDR_MODE_FIXED |                     \
                                                    UDMA_FLAG_PACKET_POSITION_END |                     \
                                                    UDMA_FLAG_READ_EN |                                 \
                                                    UDMA_FLAG_TERM |                                    \
                                                    UDMA_FLAG_DST_INT_EN

/* UDMA OTHER_FLAGS register definitons */
#define UDMA_OTHER_FLAGS_STOP                       (0x1)       /* Set to 1 to stop the DMA context. When software sets this bit, the DMA context will stop itself at the first safe moment. When the stop has completed, the DMA will clear this bit and produce a stop interrupt */
#define UDMA_OTHER_FLAGS_LL_PRE_FETCH_DIS           (0x1 << 2)  /* Set this bit to disable linked-list pre-fetching. If this bit is not set, next linked-list descriptor will be fetched before the current transfer completes. */
#define UDMA_OTHER_FLAGS_LL_OWNERSHIP_TAGS          (0x1 << 3) /* Set this bit to enable the linked-list ownership tags feature. */

/* UDMA INTR_MASK register definitions */
#define UDMA_INTR_MASK_DOCSIS_RX_SRC_TRANSFER_FIN   (0x1 << 4)  /* Interrupt when the transfer from the source buffer has finished (source buffer becomes empty). ‘1’ = enable interrupt, ‘0’ = do not interrupt */
#define UDMA_INTR_MASK_DOCSIS_TX_SRC_TRANSFER_FIN   (0x1 << 5)  /* Interrupt when the transfer from the source buffer has finished (source buffer becomes empty). ‘1’ = enable interrupt, ‘0’ = do not interrupt */
#define UDMA_INTR_MASK_DOCSIS_RX_DST_TRANSFER_FIN   (0x1 << 10) /* Interrupt when the transfer to the destination buffer has finished (destination buffer becomes full). ‘1’ = enable interrupt, ‘0’ = do not interrupt */
#define UDMA_INTR_MASK_DOCSIS_TX_DST_TRANSFER_FIN   (0x1 << 11) /* Interrupt when the transfer to the destination buffer has finished (destination buffer becomes full). ‘1’ = enable interrupt, ‘0’ = do not interrupt */
#define UDMA_INTR_MASK_DOCSIS_RX_STOP_COMPLETE      (0x1 << 16) /* Interrupt when the stop has completed, the DMA will clear this bit and produce a stop interrupt. ‘1’ = enable interrupt, ‘0’ = do not interrupt */
#define UDMA_INTR_MASK_DOCSIS_TX_STOP_COMPLETE      (0x1 << 17) /* Interrupt when the stop has completed, the DMA will clear this bit and produce a stop interrupt. ‘1’ = enable interrupt, ‘0’ = do not interrupt */


#define PrxPDSP_TX_UDMA_LINK_LIST_COUNT             2

/**************************************************************************/
/*      LOCAL VARIABLES:                                                  */
/**************************************************************************/

/* The instanse for the creation of the driver */
static dev_t dev;
static struct cdev *c_dev;
static struct class *cl;


/* Used for init the delete the driver */
static struct file_operations FileOpPrxy =
{
    .owner = THIS_MODULE,
    .unlocked_ioctl = l2switch_proxy_ioctl
};


/***************************************************************************************/
/* LOCAL FUNCTIONS                                                                     */
/***************************************************************************************/


/* Power up proxy PDSP using clock gating */
Int32 enable_proxy_pdsp_hw (void)
{
    int reg;
    int* regAddr;

    printk("%s: Run Proxy PDSP\n", __FUNCTION__);

    regAddr = (int*)AVALANCHE_NWSS_GENERAL_MAILBOX_CLK_CTRL_REG;
    reg = *regAddr;
    reg |= (AVALANCHE_NWSS_GENERAL_MAILBOX_CLK_CTRL_PrxPDSP | AVALANCHE_NWSS_GENERAL_MAILBOX_CLK_CTRL_McDMA4);
    *regAddr = reg;

    /*update l2switch driver that proxy psdp is running*/
    set_proxy_pdsp_running(true);

    return 0;
}

/* Power down Proxy PDSP using clock gating */
Int32 disable_proxy_pdsp_hw (void)
{
    int reg;
    int* regAddr;

    printk("%s: Halt Proxy PDSPs\n", __FUNCTION__);

    /*update l2switch driver that proxy psdp is sleeping*/
    set_proxy_pdsp_running(false);

    regAddr = (int*)AVALANCHE_NWSS_GENERAL_MAILBOX_CLK_CTRL_REG;
    reg = *regAddr;
    reg &= ~(AVALANCHE_NWSS_GENERAL_MAILBOX_CLK_CTRL_PrxPDSP | AVALANCHE_NWSS_GENERAL_MAILBOX_CLK_CTRL_McDMA4);
    *regAddr = reg;

    return 0;
}

/* Power up/down the proxy pdsp accurding to onoff param */
Int32 prxpdsp_psm(Uint8     onOff)
{
 if (onOff)
    {
        enable_proxy_pdsp_hw(); 
        
    }
    else
    {
        disable_proxy_pdsp_hw();
    }
}

/* Setup PrxPDSP memory */
static int prxpdsp_setup()
{
    int                 rc = 0;
    Uint32              i;
    prxpdsp_txudma_linklist_ram_t *prxpdsp_txudma_linklist = (prxpdsp_txudma_linklist_ram_t*)AVALANCHE_NWSS_PrxPDSP_TX_UDMA_DESC_RAM_BASE;
    prxpdsp_rxudma_linklist_ram_t *prxpdsp_rxudma_linklist = (prxpdsp_rxudma_linklist_ram_t*)AVALANCHE_NWSS_PrxPDSP_RX_UDMA_DESC_RAM_BASE;
    udma_regs_t         *udma_regs = (udma_regs_t*)AVALANCHE_UDMA_DOCSIS_PORTS_BASE;
    volatile Uint32     avbar;
    Uint32              udmaTxDescBase;

    
    //////////////////////////////////////////////////////////////////////////
    // TBD: DOCSIS MBAR value should be set in the below register by CEFDK
    //      Please remove this line when CEFDK will support this sophisticated feature
    udma_regs->docsis_rx_port.nextDescr = 0x000000C0;
    //////////////////////////////////////////////////////////////////////////

    avbar = (__swab32(udma_regs->docsis_rx_port.nextDescr) & 0xE0000000) | 0x08000000;
    // Initializae UDMA registers (Remember the values are swapped!!!)
    udma_regs->docsis_rx_port.currDescr = 0;
    udma_regs->docsis_tx_port.currDescr = 0;
    udma_regs->docsis_rx_port.srcDmaStart = 0;
    udma_regs->docsis_tx_port.srcDmaStart = 0;
    udma_regs->docsis_rx_port.dstDmaStart = 0;
    udma_regs->docsis_tx_port.dstDmaStart = 0;
    udma_regs->docsis_rx_port.srcDmaSize = 0;
    udma_regs->docsis_tx_port.srcDmaSize = 0;
    udma_regs->docsis_rx_port.otherMode = __swab32(UDMA_OTHER_FLAGS_LL_PRE_FETCH_DIS);
    udma_regs->docsis_tx_port.otherMode = __swab32(UDMA_OTHER_FLAGS_LL_PRE_FETCH_DIS);
    udma_regs->docsis_rx_interrupts =     __swab32(UDMA_INTR_MASK_DOCSIS_RX_DST_TRANSFER_FIN);
    udma_regs->docsis_tx_interrupts =     __swab32(UDMA_INTR_MASK_DOCSIS_TX_DST_TRANSFER_FIN);
    for (i = 0; i < PrxPDSP_TX_UDMA_LINK_LIST_COUNT; i++)
    {
        udmaTxDescBase = IO_VIRT2PHY(AVALANCHE_NWSS_PrxPDSP_TX_UDMA_DESC_RAM_BASE) + (i * sizeof(prxpdsp_txudma_linklist_ram_t)) + avbar;
        prxpdsp_txudma_linklist->metaData = 0;
        prxpdsp_txudma_linklist->metaDataDesc.next =            __swab32(udmaTxDescBase + offsetof(prxpdsp_txudma_linklist_ram_t, buffer0Desc));
        prxpdsp_txudma_linklist->metaDataDesc.size =            __swab32(sizeof(Uint32));
        prxpdsp_txudma_linklist->metaDataDesc.sourceAddress =   __swab32(udmaTxDescBase);
        prxpdsp_txudma_linklist->metaDataDesc.destAddress =     __swab32(L2SWITCH_TX_DEST_ADDRESS);
        prxpdsp_txudma_linklist->metaDataDesc.flagsMode =       __swab32(UDMA_FLAGS_LINK_LIST_START);
        prxpdsp_txudma_linklist->buffer0Desc.destAddress =      __swab32(L2SWITCH_TX_DEST_ADDRESS);
        prxpdsp_txudma_linklist->buffer0Desc.flagsMode =        __swab32(UDMA_FLAGS_LINK_LIST_MIDDLE);
        prxpdsp_txudma_linklist->buffer1Desc.destAddress =      __swab32(L2SWITCH_TX_DEST_ADDRESS);
        prxpdsp_txudma_linklist->buffer1Desc.flagsMode =        __swab32(UDMA_FLAGS_LINK_LIST_MIDDLE);
        prxpdsp_txudma_linklist->buffer2Desc.destAddress =      __swab32(L2SWITCH_TX_DEST_ADDRESS);
        prxpdsp_txudma_linklist->buffer2Desc.flagsMode =        __swab32(UDMA_FLAGS_LINK_LIST_MIDDLE);
        prxpdsp_txudma_linklist->buffer3Desc.next =             __swab32(udmaTxDescBase + offsetof(prxpdsp_txudma_linklist_ram_t, crcDesc));
        prxpdsp_txudma_linklist->buffer3Desc.destAddress =      __swab32(L2SWITCH_TX_DEST_ADDRESS);
        prxpdsp_txudma_linklist->buffer3Desc.flagsMode =        __swab32(UDMA_FLAGS_LINK_LIST_MIDDLE);
        // TBD: since CRC descriptor is not used anymore by the PrxPDSP we can remove the following code
        prxpdsp_txudma_linklist->crcDesc.next = 0;
        prxpdsp_txudma_linklist->crcDesc.size =             __swab32(sizeof(Uint32));
        prxpdsp_txudma_linklist->crcDesc.sourceAddress =    __swab32(udmaTxDescBase);
        prxpdsp_txudma_linklist->crcDesc.destAddress =      __swab32(L2SWITCH_TX_DEST_ADDRESS);
        prxpdsp_txudma_linklist->crcDesc.flagsMode =        __swab32(UDMA_FLAGS_LINK_LIST_END);

        prxpdsp_txudma_linklist++;
    }
    prxpdsp_rxudma_linklist->bufferDesc.next =              __swab32(IO_VIRT2PHY(AVALANCHE_NWSS_PrxPDSP_RX_UDMA_DESC_RAM_BASE) + offsetof(prxpdsp_rxudma_linklist_ram_t, dummyReadDesc) + avbar);
    prxpdsp_rxudma_linklist->bufferDesc.sourceAddress =     __swab32(L2SWITCH_RX_SOURCE_ADDRESS);
    prxpdsp_rxudma_linklist->bufferDesc.flagsMode =         __swab32(UDMA_FLAGS_LINK_LIST_RX_START);
    prxpdsp_rxudma_linklist->dummyReadDesc.next = 0;
    prxpdsp_rxudma_linklist->dummyReadDesc.size =           __swab32(sizeof(Uint32));
    prxpdsp_rxudma_linklist->dummyReadDesc.destAddress =    __swab32(L2SWITCH_RX_DEST_ADDRESS_FOR_DUMMY_READ);
    prxpdsp_rxudma_linklist->dummyReadDesc.flagsMode =      __swab32(UDMA_FLAGS_LINK_LIST_END_DUMMY_READ);


    return 0;
}
/**************************************************************************************/
/*! \fn static int proc_snprintf(char **page, int *page_size, int file_offset, 
 *                               int *curr_offset, const char *format, ...)
 **************************************************************************************
 *  \brief This function checks if the writing to the file is allowed writes 
 *         to the wanted buffer.
 *  \return return the number of bytes wirtten to the buffer or -1 if the 
 *          page_size isn't big enough.
 **************************************************************************************/
static int proc_snprintf(char **page, int *page_size, int file_offset, int *curr_offset, const char *format, ...)
{
    va_list args;
    int ret;
    char buff[200];

    /* va_start sets args to the first optional argument in the list of arguments passed to the function*/
    /* va_start must be used before args is used for the first time*/
    va_start(args, format);

    /* Write formatted output to buff where 200 is the Maximum number of characters to write
       using args pointer to a list of arguments, return the number of characters written if the number of characters to write is less than or equal to 200 */
    ret = vsnprintf (buff, 200, format, args);

    /* After all arguments have been retrieved, va_end resets the pointer to NULL*/
    va_end(args);

    /* If the number of characters to write is greater than count, these functions return -1 indicating that output has been truncated.*/
    if (ret == -1)
    {
        return -1;
    }

    *curr_offset += ret; 

    /* This was written already */
    if (*curr_offset <= file_offset) 
    {
        /* Do nothing */
        return 0;
    }

    if (ret > *page_size) 
    {
        /* Dump */
        return -1;
    }

    memcpy(*page,buff,ret);
    *page += ret;
    *page_size -= ret;

    return ret;
}
/**************************************************************************************/
/*! \fn static int proxy_read_proc (char *page, char **start, 
*                  off_t off, int count,int *eof, void *data)
 **************************************************************************************
 *  \brief This function reads the proxy statistical counters.
 *  \return return the number of bytes wirtten to the buffer.
 **************************************************************************************/
static int proxy_read_proc (char *page, char **start, off_t off, int count, int *eof, void *data)
{
    
    int ret = 0,len = 0, stopWrite= 0;
    int curr_offset = 0;
    int page_size = count;
    char *page_off = page;
    Uint32 *counter = (Uint32*)(PP_PRXPDSP_COUNTERS_BASE);
    
    
    if (off > 0)
    {
        return 0;
    }

    
    len = proc_snprintf(&page_off, &page_size, off, &curr_offset, "\n"); 
    if (len != -1)
       ret += len;
    else
       stopWrite = 1;
     

     if (!stopWrite)
     {
         len = proc_snprintf(&page_off, &page_size, off, &curr_offset, "######################\n"); 
        if (len != -1)
             ret += len;
         else
             stopWrite = 1;
     }

     if (!stopWrite)
     {
         len = proc_snprintf(&page_off, &page_size, off, &curr_offset, "# PrxPDSP statistics #\n"); 
        if (len != -1)
             ret += len;
         else
             stopWrite = 1;
     }

     if (!stopWrite)
     {
         len = proc_snprintf(&page_off, &page_size, off, &curr_offset, "######################\n\n"); 
        if (len != -1)
             ret += len;
         else
             stopWrite = 1;
     }

     if (!stopWrite)
     {
         len = proc_snprintf(&page_off, &page_size, off, &curr_offset, "Pkts popped from ingress queues = %d\n", *counter); 
        if (len != -1)
             ret += len;
         else
             stopWrite = 1;
     }

    counter++;
    if (!stopWrite)
    {
        len = proc_snprintf(&page_off, &page_size, off, &curr_offset, "Pkts forwarded to L2SW          = %d\n", *counter); 
        if (len != -1)
             ret += len;
         else
             stopWrite = 1;
    }

    counter++;
    if (!stopWrite)
    {
        len = proc_snprintf(&page_off, &page_size, off, &curr_offset, "Pkts received from L2SW         = %d\n", *counter); 
        if (len != -1)
             ret += len;
         else
             stopWrite = 1;
    }

    counter++;
    if (!stopWrite)
    {
        len = proc_snprintf(&page_off, &page_size, off, &curr_offset, "Pkts pushed to prefetcher       = %d\n", *counter); 
        if (len != -1)
             ret += len;
         else
             stopWrite = 1;
    }

    counter++;
    if((*counter) && (!stopWrite))
    {
        len = proc_snprintf(&page_off, &page_size, off, &curr_offset, "Buffers starvation count        = %d\n", *counter);
        if (len != -1)
         ret += len;
        else
         stopWrite = 1;

    }

    counter++;
    if((*counter) && (!stopWrite))
    {
        len = proc_snprintf(&page_off, &page_size, off, &curr_offset, "Descriptors starvation count    = %d\n", *counter);
        if (len != -1)
         ret += len;
        else
         stopWrite = 1;
    } 
      
    if (!stopWrite)
    {
        len = proc_snprintf(&page_off, &page_size, off, &curr_offset, "\n"); 
        if (len != -1)
             ret += len;
    }

    /* Set next begin of page */
    *start = page + off;
    *eof = 1; 

    /* Return number of bytes writen */
    return ret; 
    
}

/**************************************************************************/
/*! \fn static int l2switchProxyDriverInit
 **************************************************************************
 *  \brief     driver init routine 
 *  \param[in] none
 *  \return    0  = ok, other = not ok.
 *************************************************************************/
static int l2switchProxyDriverInit(void)
{
     int ret;
    struct device *dev_ret;
    /* The proc file and directory for reading the Proxy PDSP Statisticals */
    struct proc_dir_entry *procFile;
    struct proc_dir_entry *dir = (struct proc_dir_entry *)init_net.proc_net;
    /* ************************************************************************ */
    /*                                                                          */
    /*         Interface device initialization stuff ....                       */
    /*                                                                          */
    /* ************************************************************************ */
#if 0

    /* Registering device */
    if ( register_chrdev ( DEVICE_MAJOR , DEVICE_NAME , &pdspCdevFops ) < 0 )
    {
        printk ( KERN_WARNING "memory: cannot obtain major number %d\n", DEVICE_MAJOR );
        return -EIO;
    }

#else

    /*Allocating the coe_driver */
    if ((ret = alloc_chrdev_region(&dev, FIRST_MINOR, MINOR_CNT, "l2switch_proxy_driver")) < 0)
    {
        return ret;
    }

    if (!(c_dev = cdev_alloc()))
    {
        printk(KERN_ERR "%s:%d Failed to allocate character device l2switch_proxy_driver\n",__FUNCTION__,__LINE__);
        unregister_chrdev_region(dev, MINOR_CNT);
        return (-1);
    }

    /* Init the driver */
    cdev_init(c_dev, &FileOpPrxy);

    if ((ret = cdev_add(c_dev, dev, MINOR_CNT)) < 0)
    {
        printk(KERN_ERR "%s:%d Failed to add character device l2switch_proxy_driver\n",__FUNCTION__,__LINE__);
        cdev_del(c_dev);
        unregister_chrdev_region(dev, MINOR_CNT);
        return ret;
    }

    if (IS_ERR(cl = class_create(THIS_MODULE, "l2switch_proxy_driver")))
    {
        cdev_del(c_dev);
        unregister_chrdev_region(dev, MINOR_CNT);
        return PTR_ERR(cl);
    }

    if (IS_ERR(dev_ret = device_create(cl, NULL, dev, NULL, "l2switch_proxy_driver")))
    {
        class_destroy(cl);
        cdev_del(c_dev);
        unregister_chrdev_region(dev, MINOR_CNT);
        return PTR_ERR(dev_ret);
    }
#endif 

     /* create the proc file in proxy directory */
    if (NULL != (dir = proc_mkdir("proxy",dir)))
    {
        procFile = create_proc_entry(PROXY_PROC_NAME, 0644, dir);
        
        if (procFile == NULL) {
            remove_proc_entry(PROXY_PROC_NAME, NULL);
            printk(KERN_ALERT "Error: Could not initialize /proc/%s\n",PROXY_PROC_NAME);
            return -ENOMEM;
        }

        procFile->read_proc = proxy_read_proc;
        procFile->mode      = S_IFREG | S_IRUGO;
    }
    else
    {
        printk(KERN_ALERT "Error: Could not create /proc/net/proxy directory \n");
        return -ENOMEM;
    }

    /* Setup the proxy pdsp clock */
    enable_proxy_pdsp_hw();

    memset( IO_PHY2VIRT( 0x03208100 ), 0, 0xc0 );
    memset( IO_PHY2VIRT( 0x03401500 ), 0, 0x140 );
    
    return 0;
}


/**************************************************************************/
/*! \fn long l2switch_proxy_ioctl(struct file *fd, unsigned int cmd, unsigned long arg) 
 **************************************************************************
 *  \brief This function is called from the user space to oprate function in the driver.
 *  \param[in] fd - The character device of the l2switch_proxy_driver.
 *  \param[in] cmd - The wanted command.
 *  \param[in] arg - The arguments from and to the user space.
 *  \return    0  = ok, other = not ok.
 *************************************************************************/
static long l2switch_proxy_ioctl(struct file *fd, unsigned int cmd, unsigned long arg)
{
    
    if (fd == NULL)
    {
        printk("Error value\n");
        return -1;
    }

    switch (cmd)
    {

    case  L2SWITCH_PROXY_SETUP:
        prxpdsp_setup();
        break;

    case   L2SWITCH_PROXY_PSM:
        {
            proxy_psm_param_t usr_param; 
            if (copy_from_user(&usr_param, (void __user *)arg, sizeof(usr_param)))
            {
                printk(KERN_ERR"\n%s: failed to copy from user\n", __FUNCTION__);
                return -EFAULT;
            }
            prxpdsp_psm(usr_param);
        }
        break;

    default:
        return -EINVAL;
    }

    return 0;
}



/**************************************************************************/
/*! \fn int l2switchProxyDriverExit(void)
 **************************************************************************
 *  \brief     driver exit routine,currntly not supported. 
 *  \param[in] none
 *  \return    0  = ok, other = not ok.
 *************************************************************************/
static int l2switchProxyDriverExit(void)
{
 
     if (c_dev)
    {
        cdev_del(c_dev);
    }
    unregister_chrdev_region(dev, MINOR_CNT);
    device_destroy(cl, dev); 
    class_destroy(cl);  

     return 0;
}



MODULE_AUTHOR ("Intel Corporation");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("l2switch proxy Driver");

module_init(l2switchProxyDriverInit);
module_exit(l2switchProxyDriverExit);

