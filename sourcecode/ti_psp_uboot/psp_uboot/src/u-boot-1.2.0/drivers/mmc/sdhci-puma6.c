/*
 * sdhci-puma6.c
 *
 *  GPL LICENSE SUMMARY
 *
 *  Copyright(c) 2012 Intel Corporation. All rights reserved.
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
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *  The full GNU General Public License is included in this distribution
 *  in the file called LICENSE.GPL.
 *
 *  Contact Information:
 *    Intel Corporation
 *    2200 Mission College Blvd.
 *    Santa Clara, CA  97052
 *
 */


/*
 * SDHCI support for Puma6 SoC
 */

#include <common.h>
#include <malloc.h>
#include <asm/io.h>
#include <sdhci.h>
#include <puma6_aep.h>

/* Host Controller name */
static char *SDHCI_PUMA6_NAME = "sdhci_puma6";

/* Host controller ops data structure */
static struct sdhci_ops sdhci_puma6_ops;



/* Endian swap for 16bits word */ 
inline unsigned short swap_word(unsigned short x) 
{ 
    unsigned short swp = x;
    return ( ((swp&0x00FF)<<8) + ((swp&0xFF00)>>8) );
}
 

/* Endian swap for 32bits double word */
inline unsigned int swap_dword(unsigned int x) 
{ 
    int swp = x;
    return ( ((swp&0x000000FF)<<24) + ((swp&0x0000FF00)<<8 ) +
             ((swp&0x00FF0000)>>8 ) + ((swp&0xFF000000)>>24) );
}

/* Write 8bit byte to MMC host controller register */
static inline void sdhci_puma6_writeb(struct sdhci_host *host, u8 val, int reg)
{
    int align_reg;
    u32 align_val;

    /*In Puma6 A0 we must write to HC Registers within width of 32 bits (4 bytes alingment) 
      Procedure: Read the 32 bit register, modify the reqiured byte (8 bit), and write back 32 bits
    */

    /* align the register to 32 bit */
    align_reg = reg & 0xFFFFFFFC;      // Clear bit #0,#1

    /* Read */
    align_val = swap_dword(readl(host->ioaddr + align_reg));    
    
    /* Modify */
    align_val = align_val &~ (((u32)0x000000FF) << ((reg & 0x00000003) << 3));  // Reset the 0,1,2 or 3 byte
    align_val = align_val | ((u32)val) << ((reg & 0x00000003) << 3);         // Add the 0,1,2 or 3 byte.

    /* Write */
    writel(swap_dword(align_val), host->ioaddr + align_reg);
}

/* Write 16bit word to MMC host controller register */
static inline void sdhci_puma6_writew(struct sdhci_host *host, u16 val, int reg)
{
    int align_reg;
    u32 align_val;
    static u32 shadow_value;
    static u32 shadow_valid = 0;

    /* In Puma6 we must write to HC Registers within width of 32 bits (4 bytes alingment) 
      Procedure: Read the 32 bit register, modify the High Word (16 bit), or Low Word, and write back 32 bits
    */

    /* In Puma6 A special case is wiriting to TRANSFER MODE register.
      Writing to TRANSFER MODE Register with Read/modify/write solution (as above), will trigger the 
      COMMAND Register, to send a message to eMMC card 
      Solution: write the value of TRANSFER MODE to 'a shadow' register, and next time the user write 
      to COMMAND Register, then write the shadow register to TRANSFER MODE register.
     
      Note: According to spec after each writing to TRANSFER MODE register, and user will also write
      to COMMAND Register. 
    */

    /* align the register to 32 bit */
    align_reg = reg & 0xFFFFFFFC;      // Clear bit #0,#1

    /* Save Transfer Mode to a shadow register */
    if (reg == SDHCI_TRANSFER_MODE)
    {
        shadow_value = val;
        shadow_valid = 1;
        return;
    }

    /* Restore the Transfer Mode from a shadow register */
    if ((reg == SDHCI_COMMAND) && (shadow_valid == 1))
    {
        /* Read from shadow */
        align_val = shadow_value;
        shadow_valid = 0;
    }
    else
    {
        /* Read form Register */
        align_val = swap_dword(readl(host->ioaddr + align_reg)); 
    }

    /* Modify */
    align_val = align_val &~ (((u32)0x0000FFFF) << ((reg & 0x00000003) << 3));  // Reset the first or second word (16 bits)
    align_val = align_val | ((u32)val) << ((reg & 0x00000003) << 3);            // Add the first or second word (16 bits)

    
    /* Write */
    writel(swap_dword(align_val), host->ioaddr + align_reg);
}

/* Write 32bit double word to MMC host controller register */
static inline void sdhci_puma6_writel(struct sdhci_host *host, u32 val, int reg)
{
    writel(swap_dword(val),(void *)((unsigned char*)host->ioaddr + reg));
}

/* Read 8bit byte from MMC host controller register */
static inline u8 sdhci_puma6_readb(struct sdhci_host *host, int reg)
{
    u8 value = readb((unsigned char *)host->ioaddr + reg);
    return value;
}

/* Read 16bit word from MMC host controller register */
static inline u16 sdhci_puma6_readw(struct sdhci_host *host, int reg)
{
    u16 value = swap_word(readw(host->ioaddr + reg));
    return value;
}

/* Read 32bit double word from MMC host controller register */
static inline u32 sdhci_puma6_readl(struct sdhci_host *host, int reg)
{
    u32 value = swap_dword(readl(host->ioaddr + reg));
    return value;
}

/* MMC Host Controller Initialization */
int sdhci_puma6_init(void)
{
    u32 max_clk = 0;
    u32 min_clk = 0;

    struct sdhci_host *host = NULL;

	host = (struct sdhci_host *)malloc(sizeof(struct sdhci_host));
	if (!host) {
		printf("sdh_host malloc fail!\n");
		return 1;
	}
    
#ifdef CONFIG_MMC_FPGA_TEST
/* FPGA debug definitions */
#warning "PUMA6 EMMC CONFIGURATION SET TO FPGA TESTING"
    u32 quirks =  SDHCI_QUIRK_BROKEN_DMA            |
                  SDHCI_QUIRK_BROKEN_ADMA           |
                  SDHCI_QUIRK_FORCE_1_BIT_DATA      ;
#else 

    /* Real Product definitions */
   // u32 quirks =  SDHCI_QUIRK_BROKEN_CARD_DETECTION | /* TODO: need to check if really necessary */
   //               SDHCI_QUIRK_32BIT_DMA_ADDR        | /* TODO: need to check if really necessary */
   //               SDHCI_QUIRK_32BIT_DMA_SIZE        | /* TODO: need to check if really necessary */
   //               SDHCI_QUIRK_32BIT_ADMA_SIZE       ; /* TODO: need to check if really necessary */
    u32 quirks =  0;
#endif

#ifdef CONFIG_MMC_SDHCI_IO_ACCESSORS
	memset(&sdhci_puma6_ops, 0, sizeof(struct sdhci_ops));
    sdhci_puma6_ops.write_b = sdhci_puma6_writeb;
    sdhci_puma6_ops.write_w = sdhci_puma6_writew;
    sdhci_puma6_ops.write_l = sdhci_puma6_writel;
    sdhci_puma6_ops.read_b = sdhci_puma6_readb;
    sdhci_puma6_ops.read_w = sdhci_puma6_readw;
    sdhci_puma6_ops.read_l = sdhci_puma6_readl;
	host->ops = &sdhci_puma6_ops;
#endif

    /* Initialize host data structure */
    host->name = SDHCI_PUMA6_NAME;
    if (aep_is_active() == AEP_NOT_ACTIVE) 
    {
        /* Phisical SDHCI controller base address */
	host->ioaddr = (void *)CONFIG_SYS_MMC_BASE;
    }
    else
    {
        /* Virtual SDHCI controller base address in AEP mode enabled */
        host->ioaddr = (void *)aep_get_emmc_base_address();
    }
    host->quirks = quirks;
    host->version = sdhci_readw(host, SDHCI_HOST_VERSION);
    host->version = (host->version & SDHCI_SPEC_VER_MASK) >> SDHCI_SPEC_VER_SHIFT;

    /* Add new MMC device */
	add_sdhci(host, max_clk, min_clk);
	return 0;
}

