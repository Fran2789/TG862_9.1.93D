/*
 * Copyright 2011, Marvell Semiconductor Inc.
 * Lei Wen <leiwen@marvell.com>
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 *
 * Back ported to the 8xx platform (from the 8260 platform) by
 * Murray.Jensen@cmst.csiro.au, 27-Jan-01.
 */

/* 
 * Includes Intel Corporation's changes/modifications dated: 2011. 
 * Changed/modified portions - Copyright © 2011 - 2012 , Intel Corporation.   
 */ 


#include <common.h>
#include <malloc.h>
#include <mmc.h>
#include <sdhci.h>
#include <docsis_ip_boot_params.h>

//#define DEBUGF(fmt,args...) printf(fmt ,##args)

void *aligned_buffer;


char *cmdid2text[] = {
"MMC_CMD_GO_IDLE_STATE",	   /* 0 */
"MMC_CMD_SEND_OP_COND",		   /* 1 */
"MMC_CMD_ALL_SEND_CID",		   /* 2 */
"MMC_CMD_SET_RELATIVE_ADDR",   /* 3 */
"MMC_CMD_SET_DSR",			   /* 4 */
"MMC_CMD_NONE_5",              /* 5 */
"MMC_CMD_SWITCH",              /* 6 */
"MMC_CMD_SELECT_CARD",         /* 7 */
"MMC_CMD_SEND_EXT_CSD",        /* 8 */
"MMC_CMD_SEND_CSD",            /* 9 */
"MMC_CMD_SEND_CID",            /* 10 */
"MMC_CMD_NONE_11",             /* 11 */
"MMC_CMD_STOP_TRANSMISSION",   /* 12 */
"MMC_CMD_SEND_STATUS",         /* 13 */
"MMC_CMD_NONE_14",             /* 14 */
"MMC_CMD_NONE_15",             /* 15 */
"MMC_CMD_SET_BLOCKLEN",        /* 16 */
"MMC_CMD_READ_SINGLE_BLOCK",   /* 17 */
"MMC_CMD_READ_MULTIPLE_BLOCK", /* 18 */
"MMC_CMD_NONE_19",             /* 19 */
"MMC_CMD_NONE_20",             /* 20 */
"MMC_CMD_NONE_21",             /* 21 */
"MMC_CMD_NONE_22",             /* 22 */
"MMC_CMD_NONE_23",             /* 23 */
"MMC_CMD_WRITE_SINGLE_BLOCK",  /* 24 */
"MMC_CMD_WRITE_MULTIPLE_BLOCK",/* 25 */
"MMC_CMD_NONE_26",             /* 26 */
"MMC_CMD_NONE_27",             /* 27 */
"MMC_CMD_NONE_28",             /* 28 */
"MMC_CMD_NONE_29",             /* 29 */
"MMC_CMD_NONE_30",             /* 30 */
"MMC_CMD_NONE_31",             /* 31 */
"MMC_CMD_NONE_32",             /* 32 */
"MMC_CMD_NONE_33",             /* 33 */
"MMC_CMD_NONE_34",             /* 34 */
"MMC_CMD_ERASE_GROUP_START",   /* 35 */
"MMC_CMD_ERASE_GROUP_END",     /* 36 */
"MMC_CMD_NONE_37",             /* 37 */
"MMC_CMD_ERASE",               /* 38 */
"MMC_CMD_NONE_39",             /* 39 */
"MMC_CMD_NONE_40",             /* 40 */
"MMC_CMD_NONE_41",             /* 41 */
"MMC_CMD_NONE_42",             /* 42 */
"MMC_CMD_NONE_43",             /* 43 */
"MMC_CMD_NONE_44",             /* 44 */
"MMC_CMD_NONE_45",             /* 45 */
"MMC_CMD_NONE_46",             /* 46 */
"MMC_CMD_NONE_47",             /* 47 */
"MMC_CMD_NONE_48",             /* 48 */
"MMC_CMD_NONE_49",             /* 49 */
"MMC_CMD_NONE_50",             /* 50 */
"MMC_CMD_NONE_51",             /* 51 */
"MMC_CMD_NONE_52",             /* 52 */
"MMC_CMD_NONE_53",             /* 53 */
"MMC_CMD_NONE_54",             /* 54 */
"MMC_CMD_APP_CMD",             /* 55 */
"MMC_CMD_NONE_56",             /* 56 */
"MMC_CMD_NONE_57",             /* 57 */
"MMC_CMD_SPI_READ_OCR",        /* 58 */
"MMC_CMD_SPI_CRC_ON_OFF",      /* 59 */
"MMC_CMD_MANUFACTURE_60",      /* 60 */
"MMC_CMD_MANUFACTURE_61",      /* 61 */
"MMC_CMD_MANUFACTURE_62",      /* 62 */
"MMC_CMD_MANUFACTURE_63",      /* 63 */
};

static void sdhci_dumpregs(struct sdhci_host *host)
{
	printf(": =========== REGISTER DUMP =================\n");

	printf(": Sys addr: 0x%08x | Version:  0x%08x\n",
		sdhci_readl(host, SDHCI_DMA_ADDRESS),
		sdhci_readw(host, SDHCI_HOST_VERSION));
	printf(": Blk size: 0x%08x | Blk cnt:  0x%08x\n",
		sdhci_readw(host, SDHCI_BLOCK_SIZE),
		sdhci_readw(host, SDHCI_BLOCK_COUNT));
	printf(": Argument: 0x%08x | Trn mode: 0x%08x\n",
		sdhci_readl(host, SDHCI_ARGUMENT),
		sdhci_readw(host, SDHCI_TRANSFER_MODE));
	printf(": Present:  0x%08x | Host ctl: 0x%08x\n",
		sdhci_readl(host, SDHCI_PRESENT_STATE),
		sdhci_readb(host, SDHCI_HOST_CONTROL));
	printf(": Power:    0x%08x | Blk gap:  0x%08x\n",
		sdhci_readb(host, SDHCI_POWER_CONTROL),
		sdhci_readb(host, SDHCI_BLOCK_GAP_CONTROL));
	printf(": Wake-up:  0x%08x | Clock:    0x%08x\n",
		sdhci_readb(host, SDHCI_WAKE_UP_CONTROL),
		sdhci_readw(host, SDHCI_CLOCK_CONTROL));
	printf(": Timeout:  0x%08x | Int stat: 0x%08x\n",
		sdhci_readb(host, SDHCI_TIMEOUT_CONTROL),
		sdhci_readl(host, SDHCI_INT_STATUS));
	printf(": Int enab: 0x%08x | Sig enab: 0x%08x\n",
		sdhci_readl(host, SDHCI_INT_ENABLE),
		sdhci_readl(host, SDHCI_SIGNAL_ENABLE));
	printf(": AC12 err: 0x%08x | Slot int: 0x%08x\n",
		sdhci_readw(host, SDHCI_ACMD12_ERR),
		sdhci_readw(host, SDHCI_SLOT_INT_STATUS));
	printf(": Caps:     0x%08x | Caps_1:   0x%08x\n",
		sdhci_readl(host, SDHCI_CAPABILITIES),
		sdhci_readl(host, SDHCI_CAPABILITIES_1));
	printf(": Cmd:      0x%08x | Max curr: 0x%08x\n",
		sdhci_readw(host, SDHCI_COMMAND),
		sdhci_readl(host, SDHCI_MAX_CURRENT));
    printf(": Host Ctl2 0x%08x | --------------------\n",
		sdhci_readw(host, SDHCI_HOST_CONTROL_2));
    

	printf(": ===========================================\n");
}
static void sdhci_reset(struct sdhci_host *host, u8 mask)
{
    DEBUGF("[DEBUG] - sdhci_reset mask=0x%x\n", mask);	
    unsigned long timeout;

    printf("Warning: SDCHI perfrom SW reset (mask=0x%x)\n", mask);

	/* Wait max 100 ms */
	timeout = 100;
	sdhci_writeb(host, mask, SDHCI_SOFTWARE_RESET);
	while (sdhci_readb(host, SDHCI_SOFTWARE_RESET) & mask) {
		if (timeout == 0) {
			printf("Reset 0x%x never completed.\n", (int)mask);
			sdhci_dumpregs(host);
			return;
		}
		timeout--;
		udelay(1000);
	}
}

static void sdhci_cmd_done(struct sdhci_host *host, struct mmc_cmd *cmd)
{
    DEBUGF("[DEBUG] - sdhci_cmd_done\n");		
    int i;
	if (cmd->resp_type & MMC_RSP_136) {
		/* CRC is stripped so we need to do some shifting. */
		for (i = 0; i < 4; i++) {
			cmd->response[i] = sdhci_readl(host,
					SDHCI_RESPONSE + (3-i)*4) << 8;
			if (i != 3)
				cmd->response[i] |= sdhci_readb(host,
						SDHCI_RESPONSE + (3-i)*4-1);
		}
		DEBUGF("[DEBUG] - sdhci_cmd_done: response: 0x%0.8X,0x%0.8X,0x%0.8X,0x%0.8X\n",cmd->response[0],cmd->response[1],cmd->response[2],cmd->response[3]);
	} else {
		cmd->response[0] = sdhci_readl(host, SDHCI_RESPONSE);
        DEBUGF("[DEBUG] - sdhci_cmd_done: response: 0x%0.8X\n",cmd->response[0]);
	}
}

static void sdhci_transfer_pio(struct sdhci_host *host, struct mmc_data *data)
{
    DEBUGF("[DEBUG] - sdhci_transfer_pio\n");	
    int i;
    unsigned int data32 = 0;
    unsigned int size = 4;
	char *offs;
	for (i = 0; i < data->blocksize; i += 4) {
		offs = data->dest + i;
		if (data->flags == MMC_DATA_READ)
        {
			/* *(u32 *)offs = sdhci_readl(host, SDHCI_BUFFER); */
            data32 = sdhci_readl(host, SDHCI_BUFFER);
            while (size) //copying 4 bytes
            {
                /* Copying buffer */
                *offs = data32 & 0xFF; //masking the upper word
                offs++;
                data32 >>= 8;
                size--;
            }
            size = 4;
        }
		else
        {
            while (size) //copying 4 bytes
            {
                data32 >>= 8;
                data32  |= (unsigned int)*offs << 24;
                offs++;
                size--;
            }
            size = 4;
            /* Writing Data Port Register */
            sdhci_writel(host, data32, SDHCI_BUFFER);
			/* sdhci_writel(host, *(u32 *)offs, SDHCI_BUFFER); */
        }
	}
}

static int sdhci_transfer_data(struct sdhci_host *host, struct mmc_data *data,
				unsigned int start_addr)
{
    DEBUGF("[DEBUG] - sdhci_transfer_data\n");	
    unsigned int stat, rdy, mask, timeout, block = 0;

	timeout = 10000;
	rdy = SDHCI_INT_SPACE_AVAIL | SDHCI_INT_DATA_AVAIL;
	mask = SDHCI_DATA_AVAILABLE | SDHCI_SPACE_AVAILABLE;
	do {
		stat = sdhci_readl(host, SDHCI_INT_STATUS);
		if (stat & SDHCI_INT_ERROR) {
			printf("Error detected in status(0x%X)!\n", stat);
			return -1;
		}
        DEBUGF("[DEBUG] - sdhci_transfer_data - read status (0x%X)!\n", stat);
		if (stat & rdy) {
			if (!(sdhci_readl(host, SDHCI_PRESENT_STATE) & mask))
				continue;
			sdhci_writel(host, rdy, SDHCI_INT_STATUS);

            DEBUGF("[DEBUG] - sdhci_transfer_data: calling sdhci_transfer_pio()\n");
			sdhci_transfer_pio(host, data);
			data->dest += data->blocksize;
			if (++block >= data->blocks)
				break;
		}
#ifdef CONFIG_MMC_SDMA
		if (stat & SDHCI_INT_DMA_END) {
            DEBUGF("[DEBUG] - sdhci_transfer_data: SDHCI_INT_STATUS register => SDHCI_INT_DMA_END\n");
			sdhci_writel(host, SDHCI_INT_DMA_END, SDHCI_INT_STATUS);
			start_addr &= ~(SDHCI_DEFAULT_BOUNDARY_SIZE - 1);
			start_addr += SDHCI_DEFAULT_BOUNDARY_SIZE;
#ifdef CONFIG_PUMA6_ADDRFIX
            sdhci_writel(host, start_addr-DDR_BASE_ADDR, SDHCI_DMA_ADDRESS);
#else
			sdhci_writel(host, start_addr, SDHCI_DMA_ADDRESS);
#endif
		}
#endif
		if (timeout-- > 0)
			udelay(10);
		else {
			printf("Transfer data timeout\n");
			return -1;
		}
	} while (!(stat & SDHCI_INT_DATA_END));
	return 0;
}

int sdhci_send_command(struct mmc *mmc, struct mmc_cmd *cmd,
		       struct mmc_data *data)
{
	DEBUGF("[DEBUG] - sdhci_send_command\n");
    struct sdhci_host *host = (struct sdhci_host *)mmc->priv;
	unsigned int stat = 0;
	int ret = 0;
	int trans_bytes = 0, is_aligned = 1;
	u32 mask, flags, mode;
	unsigned int timeout, start_addr = 0;

    DEBUGF("[DEBUG] - sdhci_send_command:  CMD%d - %s\n",cmd->cmdidx,cmdid2text[cmd->cmdidx]);

	/* Wait max 10 ms */
	timeout = 10;

	sdhci_writel(host, SDHCI_INT_ALL_MASK, SDHCI_INT_STATUS);
	mask = SDHCI_CMD_INHIBIT | SDHCI_DATA_INHIBIT;

	/* We shouldn't wait for data inihibit for stop commands, even
	   though they might use busy signaling */
	if (cmd->cmdidx == MMC_CMD_STOP_TRANSMISSION)
    {
        DEBUGF("[DEBUG] - sdhci_send_command: CommandID == MMC_CMD_STOP_TRANSMISSION, SDHCI_DATA_INHIBIT is removed from mask\n" );
		mask &= ~SDHCI_DATA_INHIBIT;   /* OMER: Enable this line cause to write to fail in Puma6, TODO: chaeck why it fails */ 
    }

	while (sdhci_readl(host, SDHCI_PRESENT_STATE) & mask) {
		if (timeout == 0) {
			printf("Controller never released inhibit bit(s).\n");
			return COMM_ERR;
		}
		timeout--;
		udelay(1000);
	}

	mask = SDHCI_INT_RESPONSE;
	if (!(cmd->resp_type & MMC_RSP_PRESENT))
    {
        DEBUGF("[DEBUG] - sdhci_send_command: MMC_RSP_PRESENT not present, flags = SDHCI_CMD_RESP_NONE \n");
		flags = SDHCI_CMD_RESP_NONE;
    }
	else if (cmd->resp_type & MMC_RSP_136)
    {
        DEBUGF("[DEBUG] - sdhci_send_command: MMC_RSP_136 is present, flags = SDHCI_CMD_RESP_LONG \n");
		flags = SDHCI_CMD_RESP_LONG;
    }
	else if (cmd->resp_type & MMC_RSP_BUSY) 
    {
        DEBUGF("[DEBUG] - sdhci_send_command: MMC_RSP_BUSY is present, flags = SDHCI_CMD_RESP_SHORT_BUSY \n");
		flags = SDHCI_CMD_RESP_SHORT_BUSY;
		mask |= SDHCI_INT_DATA_END;
	} 
    else
    {
		flags = SDHCI_CMD_RESP_SHORT;
        DEBUGF("[DEBUG] - sdhci_send_command: flags = SDHCI_CMD_RESP_SHORT \n");

    }

	if (cmd->resp_type & MMC_RSP_CRC)
    {
        DEBUGF("[DEBUG] - sdhci_send_command: MMC_RSP_CRC is included, flags |= SDHCI_CMD_CRC \n");
		flags |= SDHCI_CMD_CRC;
    }
	if (cmd->resp_type & MMC_RSP_OPCODE)
    {
        DEBUGF("[DEBUG] - sdhci_send_command: MMC_RSP_OPCODE is included, flags |= SDHCI_CMD_INDEX \n");
		flags |= SDHCI_CMD_INDEX;
    }
	if (data)
    {
        DEBUGF("[DEBUG] - sdhci_send_command: flags |= SDHCI_CMD_DATA \n");
		flags |= SDHCI_CMD_DATA;
    }

    if (data != 0) {
        DEBUGF("[DEBUG] - sdhci_send_command: Set Timeout Control register \n");
		sdhci_writeb(host, 0xe, SDHCI_TIMEOUT_CONTROL);
		mode = SDHCI_TRNS_BLK_CNT_EN;
		trans_bytes = data->blocks * data->blocksize;
		if (data->blocks > 1)
        {
            DEBUGF("[DEBUG] - sdhci_send_command: mode |= SDHCI_TRNS_MULTI \n");
			mode |= SDHCI_TRNS_MULTI;
        }
		if (data->flags == MMC_DATA_READ)
        {
            DEBUGF("[DEBUG] - sdhci_send_command: mode |= SDHCI_TRNS_READ \n");
			mode |= SDHCI_TRNS_READ;
        }

#ifdef CONFIG_MMC_SDMA
		if (data->flags == MMC_DATA_READ)
        {
            DEBUGF("[DEBUG] - sdhci_send_command: Read destination address 0x%x \n",(unsigned int)data->dest);
			start_addr = (unsigned int)data->dest;
        }
		else
        {
            DEBUGF("[DEBUG] - sdhci_send_command: Write source address 0x%x \n",(unsigned int)data->dest);
			start_addr = (unsigned int)data->src;
        }
		if ((host->quirks & SDHCI_QUIRK_32BIT_DMA_ADDR) &&
				(start_addr & 0x7) != 0x0) {
			is_aligned = 0;
			start_addr = (unsigned int)aligned_buffer;
			if (data->flags != MMC_DATA_READ)
				memcpy(aligned_buffer, data->src, trans_bytes);
		}

        /* Omer To Do: change address to Atom view */
#ifdef CONFIG_PUMA6_ADDRFIX
        sdhci_writel(host, start_addr-DDR_BASE_ADDR, SDHCI_DMA_ADDRESS);
#else
		sdhci_writel(host, start_addr, SDHCI_DMA_ADDRESS);
#endif
        DEBUGF("[DEBUG] - sdhci_send_command: mode |= SDHCI_TRNS_DMA \n");
		mode |= SDHCI_TRNS_DMA;
#endif

#ifndef CONFIG_SYSTEM_PUMA6_SOC
		sdhci_writew(host, SDHCI_MAKE_BLKSZ(SDHCI_DEFAULT_BOUNDARY_ARG,
				data->blocksize),
				SDHCI_BLOCK_SIZE);
		sdhci_writew(host, data->blocks, SDHCI_BLOCK_COUNT);
#else
        DEBUGF("[DEBUG] - sdhci_send_command: set block cnt reg to: 0x%04X, and block size reg to: 0x%04X \n",data->blocks,data->blocksize);
        sdhci_writel(host, (data->blocks << 16 ) | SDHCI_MAKE_BLKSZ(SDHCI_DEFAULT_BOUNDARY_ARG,data->blocksize),SDHCI_BLOCK_SIZE);
#endif
		sdhci_writew(host, mode, SDHCI_TRANSFER_MODE);
	}

    DEBUGF("[DEBUG] - sdhci_send_command: set arg: 0x%08X \n",cmd->cmdarg);
	sdhci_writel(host, cmd->cmdarg, SDHCI_ARGUMENT);
#ifdef CONFIG_MMC_SDMA
	flush_cache(start_addr, trans_bytes);
#endif
    DEBUGF("[DEBUG] - sdhci_send_command: send command \n");
	sdhci_writew(host, SDHCI_MAKE_CMD(cmd->cmdidx, flags), SDHCI_COMMAND);
	do {
		stat = sdhci_readl(host, SDHCI_INT_STATUS);
		if (stat & SDHCI_INT_ERROR)
        {
            printf("warning: sdhci command return error. STATUS register: 0x%0.8X, PRESENT STATE register: 0x%0.8X\n",stat,sdhci_readl(host, SDHCI_PRESENT_STATE));
            printf(" info: failed on CMD%d\n",cmd->cmdidx);
			break;
        }
	} while ((stat & mask) != mask);

    DEBUGF("[DEBUG] - sdhci_send_command: PRESENT STATE register: 0x%0.8X\n",stat,sdhci_readl(host, SDHCI_PRESENT_STATE));

    if ((stat & (SDHCI_INT_ERROR | mask)) == mask) {
        DEBUGF("[DEBUG] - sdhci_send_command: command done\n");
		sdhci_cmd_done(host, cmd);
		sdhci_writel(host, mask, SDHCI_INT_STATUS);
	}
    else
    {
        printf("Error: sdhci command failed\n");
		ret = -1;
    }

	if (!ret && data)
    {
        DEBUGF("[DEBUG] - sdhci_send_command: calliing sdhci_transfer_data\n");
		ret = sdhci_transfer_data(host, data, start_addr);
    }

	stat = sdhci_readl(host, SDHCI_INT_STATUS);
	sdhci_writel(host, SDHCI_INT_ALL_MASK, SDHCI_INT_STATUS);
	if (!ret) {
		if ((host->quirks & SDHCI_QUIRK_32BIT_DMA_ADDR) &&
				!is_aligned && (data->flags == MMC_DATA_READ))
			memcpy(data->dest, aligned_buffer, trans_bytes);
		return 0;
	}

	sdhci_reset(host, SDHCI_RESET_CMD);
	sdhci_reset(host, SDHCI_RESET_DATA);
	if (stat & SDHCI_INT_TIMEOUT)
		return TIMEOUT;
	else
		return COMM_ERR;
}

static int sdhci_set_clock(struct mmc *mmc, unsigned int clock)
{
	struct sdhci_host *host = (struct sdhci_host *)mmc->priv;
	unsigned int div, clk, timeout;

    DEBUGF("[DEBUG] - sdhci_set_clock: Reset clock\n");
	sdhci_writew(host, 0, SDHCI_CLOCK_CONTROL);

	if (clock == 0)
		return 0;

    if (host->version >= SDHCI_SPEC_300) {
		/* Version 3.00 divisors must be a multiple of 2. */
		if (mmc->f_max <= clock)
			div = 1;
		else {
			for (div = 2; div < SDHCI_MAX_DIV_SPEC_300; div += 2) {
				if ((mmc->f_max / div) <= clock)
					break;
			}
		}
	} else {
		/* Version 2.00 divisors must be a power of 2. */
		for (div = 1; div < SDHCI_MAX_DIV_SPEC_200; div *= 2) {
			if ((mmc->f_max / div) <= clock)
				break;
		}
	}
	div >>= 1;
    clk = (div & SDHCI_DIV_MASK) << SDHCI_DIVIDER_SHIFT;
    clk |= ((div & SDHCI_DIV_HI_MASK) >> SDHCI_DIV_MASK_LEN)
		<< SDHCI_DIVIDER_HI_SHIFT;
	clk |= SDHCI_CLOCK_INT_EN;
    DEBUGF("[DEBUG] - sdhci_set_clock: set clock to 0x%0.4X\n",clk);
	sdhci_writew(host, clk, SDHCI_CLOCK_CONTROL);

	/* Wait max 20 ms */
	timeout = 20;
	while (!((clk = sdhci_readw(host, SDHCI_CLOCK_CONTROL))
		& SDHCI_CLOCK_INT_STABLE)) {
		if (timeout == 0) {
			printf("Internal clock never stabilised.\n");
			return -1;
		}
		timeout--;
		udelay(1000);
	}

	clk |= SDHCI_CLOCK_CARD_EN;
    DEBUGF("[DEBUG] - sdhci_set_clock: set clock to 0x%0.4X (enable)\n",clk);
	sdhci_writew(host, clk, SDHCI_CLOCK_CONTROL);
	return 0;
}

static void sdhci_set_power(struct sdhci_host *host, unsigned short power)
{
	u8 pwr = 0;

	if (power != (unsigned short)-1) {
		switch (1 << power) {
		case MMC_VDD_165_195:
			pwr = SDHCI_POWER_180;
			break;
		case MMC_VDD_29_30:
		case MMC_VDD_30_31:
			pwr = SDHCI_POWER_300;
			break;
		case MMC_VDD_32_33:
		case MMC_VDD_33_34:
			pwr = SDHCI_POWER_330;
			break;
		}
	}

	if (pwr == 0) {
		sdhci_writeb(host, 0, SDHCI_POWER_CONTROL);
		return;
	}

	pwr |= SDHCI_POWER_ON;

    DEBUGF("[DEBUG] - sdhci_set_power: Set Power to 0x%0.2X\n", pwr);
	sdhci_writeb(host, pwr, SDHCI_POWER_CONTROL);
}

void sdhci_set_ios(struct mmc *mmc)
{
	u32 ctrl;
    u16 ctrl2;
	struct sdhci_host *host = (struct sdhci_host *)mmc->priv;

     DEBUGF("[DEBUG] - sdhci_set_ios: Start\n");

    /* Set Clock - if changed */
	if (mmc->clock != host->clock)
    {
		sdhci_set_clock(mmc, mmc->clock);
    }

	/* Set bus width */
	ctrl = sdhci_readb(host, SDHCI_HOST_CONTROL);
    DEBUGF("[DEBUG] - sdhci_set_ios: read HOST_CONTROL = 0x%0.2X\n",ctrl);

	if (mmc->bus_width == 8) {
		ctrl &= ~SDHCI_CTRL_4BITBUS;
		/* if (host->version >= SDHCI_SPEC_300)  OMER: Puma6 support 8 bits, regadless the 'version' number */
		ctrl |= SDHCI_CTRL_8BITBUS;
	} else {
		/* if (host->version >= SDHCI_SPEC_300) OMER: Puma6 support 8 bits, regadless the 'version' number */
		ctrl &= ~SDHCI_CTRL_8BITBUS;
		if (mmc->bus_width == 4)
			ctrl |= SDHCI_CTRL_4BITBUS;
		else
			ctrl &= ~SDHCI_CTRL_4BITBUS;
	}

	if (mmc->clock > 26000000)
		ctrl |= SDHCI_CTRL_HISPD;
	else
		ctrl &= ~SDHCI_CTRL_HISPD;


    sdhci_writeb(host, ctrl, SDHCI_HOST_CONTROL);
    DEBUGF("[DEBUG] - sdhci_set_ios: write HOST_CONTROL = 0x%0.2X\n",ctrl);

    /* Set Dual Data Rate enable */
    if (mmc->ddr_enable != 0)
    {
        ctrl2 = SDHCI_CTRL2_DDR;
    }
    else
    {
        ctrl2 = SDHCI_CTRL2_SDR;
    }
    sdhci_writew(host, ctrl2, SDHCI_HOST_CONTROL_2);
    DEBUGF("[DEBUG] - sdhci_set_ios: write HOST_CONTROL 2 = 0x%0.4X\n",ctrl2);
   
    DEBUGF("[DEBUG] - sdhci_set_ios: Done.\n");
}

int sdhci_init(struct mmc *mmc)
{
	struct sdhci_host *host = (struct sdhci_host *)mmc->priv;


	if ((host->quirks & SDHCI_QUIRK_32BIT_DMA_ADDR) && !aligned_buffer) {
		aligned_buffer = memalign(8, 512*1024);
		if (!aligned_buffer) {
			printf("Aligned buffer alloc failed!!!");
			return -1;
		}
	}

	/* Eable all state */
	sdhci_writel(host, SDHCI_INT_ALL_MASK, SDHCI_INT_ENABLE);
	/* sdhci_writel(host, SDHCI_INT_ALL_MASK, SDHCI_SIGNAL_ENABLE);*/ /* Puma6 uboot does not support interrupts */ 

    DEBUGF("[DEBUG] - sdhci_init: calling sdhci_set_power\n");
	sdhci_set_power(host, fls(mmc->voltages) - 1);

	return 0;
}

/* 
 * sdhci_ioctl - perform various of host controller operations: 
 * ops: 
 *      - Dump registers - dump all host controller registers
 *      - Reset          - perform Host Controller Reset Data, Reset Command or Reset ALL.
 *      - Power          - Power On or Off the MMC
 *      - DDR            - Enable/Disable the DDR (Dual Data Rate) capability
 */
int sdhci_ioctl(struct mmc *mmc,int cmd, unsigned arg)
{
    struct sdhci_host *host = (struct sdhci_host *)mmc->priv;

    switch(cmd)
    {
    case MMC_IOCTL_HOST_DUMP_REGS:
        DEBUGF("[DEBUG] - sdhci_ioctl: Dump controller registers\n");
        sdhci_dumpregs(host);
        break;
    case MMC_IOCTL_HOST_RESET_CMD:
        DEBUGF("[DEBUG] - sdhci_ioctl: calling sdhci_reset to do CMD reset to controller\n");
        sdhci_reset(host, SDHCI_RESET_CMD);
        break;
    case MMC_IOCTL_HOST_RESET_DATA:
        DEBUGF("[DEBUG] - sdhci_ioctl: calling sdhci_reset to do DATA reset to controller\n");
        sdhci_reset(host, SDHCI_RESET_DATA);
        break;
    case MMC_IOCTL_HOST_RESET_ALL:
        DEBUGF("[DEBUG] - sdhci_ioctl: calling sdhci_reset to do ALL reset to controller\n");
        sdhci_reset(host, SDHCI_RESET_ALL);
        break;
    case MMC_IOCTL_HOST_POWER_OFF:
        DEBUGF("[DEBUG] - sdhci_ioctl: calling sdhci_set_power to turn OFF the flash\n");
        sdhci_set_power(host,0);
        break;
    case MMC_IOCTL_HOST_POWER_ON:
        DEBUGF("[DEBUG] - sdhci_ioctl: calling sdhci_set_power to turn ON the flash\n");
        sdhci_set_power(host, fls(mmc->voltages) - 1);
        break;
    case MMC_IOCTL_HOST_DDR_ENABLE:
        DEBUGF("[DEBUG] - sdhci_ioctl: Enable Host DDR capability\n");
        mmc->host_caps |= MMC_MODE_DDR;
        break;
	case MMC_IOCTL_HOST_DDR_DISABLE:
        DEBUGF("[DEBUG] - sdhci_ioctl: Disable Host DDR capability\n");
        mmc->host_caps &= ~MMC_MODE_DDR;
        break;
		
    default:
        printf("Warning: sdhci_ioctl: unknown command\n");
    }
    return 0;
}

int add_sdhci(struct sdhci_host *host, u32 max_clk, u32 min_clk)
{
	struct mmc *mmc;
	unsigned int caps;
    unsigned int ctrl2;

	mmc = malloc(sizeof(struct mmc));
	if (!mmc) {
		printf("mmc malloc fail!\n");
		return -1;
	}

	mmc->priv = host;
	host->mmc = mmc;

	sprintf(mmc->name, "%s", host->name);
	mmc->send_cmd = sdhci_send_command;
	mmc->set_ios = sdhci_set_ios;
	mmc->init = sdhci_init;
    mmc->ioctl = sdhci_ioctl;
    mmc->has_init = 0;
    mmc->ddr_enable = 0;
    mmc->high_capacity = 0;
	mmc->has_partial_init = 0;

#ifdef CONFIG_MMC_SKIP_INIT
/* In case of Skip Boot is enable, then we need to intialize some 
   of the parameters in mmc data structure, manualy */

    mmc->part_num = 0;                          /* set partition number*/
	mmc->has_partial_init = 1;                  /* set "initialize complete" - has partial init*/
    mmc->read_bl_len = CONFIG_MMC_BLOCK_SIZE;   /* set Read block lenth in bytes */
	mmc->write_bl_len = CONFIG_MMC_BLOCK_SIZE;  /* set Read block lenth in bytes */
    mmc->rca = 1;
    mmc->block_dev.part_type = PART_TYPE_UNKNOWN;
	
	 /* Read 'Boot Param' to determinate if the card is high capacity (over 2GB). */
	if(BOOT_PARAM_DWORD_READ(EMMC_FLASH_SIZE) > 0x800) {
		mmc->high_capacity = 1;
	}

    DEBUGF("[DEBUG] - Host Control base addresss = 0x%0.8X\n",host->ioaddr);

    /* Read HostControl2 register to determinate if we are already in ddr mode */
    ctrl2 = sdhci_readw(host, SDHCI_HOST_CONTROL_2); 
    DEBUGF("[DEBUG] - Host Control 2 Register = 0x%0.8X\n",ctrl2);
    if ((ctrl2 & SDHCI_CTRL2_DDR) != 0)
    {
        mmc->ddr_enable = 1;
        DEBUGF("[DEBUG] - Host Dual Data Rate is eanbled (DDR)\n");
    }
#endif
    
    DEBUGF("[DEBUG] - Host Control Ver: 0x%0.4x\n", sdhci_readl(host, SDHCI_SLOT_INT_STATUS)>>16);


	caps = sdhci_readl(host, SDHCI_CAPABILITIES);
	DEBUGF("[DEBUG] - Caps: 0x%0.8x\n", caps);
    caps |= SDHCI_CAN_VDD_330;   /* Puma6 fix */
    DEBUGF("[DEBUG] - Caps: 0x%0.8x (with P6 fixup to support V3.3)\n", caps);

#ifdef CONFIG_MMC_SDMA 
	if (!(caps & SDHCI_CAN_DO_SDMA)) {
		printf("Your controller don't support sdma!!\n");
		return -1;
	}
#endif

    DEBUGF("[DEBUG] - Host Version = %d\n",host->version);

	if (max_clk)
		mmc->f_max = max_clk;
	else {
		if (host->version >= SDHCI_SPEC_300)
			mmc->f_max = (caps & SDHCI_CLOCK_V3_BASE_MASK)
				>> SDHCI_CLOCK_BASE_SHIFT;
		else
			mmc->f_max = (caps & SDHCI_CLOCK_BASE_MASK)
				>> SDHCI_CLOCK_BASE_SHIFT;
		mmc->f_max *= 1000000;
	}
	if (mmc->f_max == 0) {
		printf("Hardware doesn't specify base clock frequency\n");
		return -1;
	}
	if (min_clk)
		mmc->f_min = min_clk;
	else {
		if (host->version >= SDHCI_SPEC_300)
			mmc->f_min = mmc->f_max / SDHCI_MAX_DIV_SPEC_300;
		else
			mmc->f_min = mmc->f_max / SDHCI_MAX_DIV_SPEC_200;
	}

	mmc->voltages = 0;
	if (caps & SDHCI_CAN_VDD_330)
		mmc->voltages |= MMC_VDD_32_33 | MMC_VDD_33_34;
	if (caps & SDHCI_CAN_VDD_300)
		mmc->voltages |= MMC_VDD_29_30 | MMC_VDD_30_31;
	if (caps & SDHCI_CAN_VDD_180)
		mmc->voltages |= MMC_VDD_165_195;

    mmc->host_caps = 0;
    if (caps & SDHCI_CAN_DO_HISPD)
    {
        DEBUGF("[DEBUG] - Caps support High Speed.\n");
		mmc->host_caps |= MMC_MODE_HS | MMC_MODE_HS_52MHz;
    }
    else
    {
        DEBUGF("[DEBUG] - Caps not support High Speed.");

    }
    
    if (caps & SDHCI_CAN_DO_8BIT)
    {
        mmc->host_caps |= (MMC_MODE_8BIT | MMC_MODE_4BIT);
        DEBUGF("[DEBUG] - Caps support 4 bit and 8 bit width\n");
    }
    else
    {
        mmc->host_caps |= MMC_MODE_4BIT;
        DEBUGF("[DEBUG] - Caps support 4 bit width\n");
    }
    if (host->quirks & SDHCI_QUIRK_FORCE_1_BIT_DATA)
    {
        mmc->host_caps &= ~(MMC_MODE_8BIT | MMC_MODE_4BIT);
        DEBUGF("[DEBUG] - Quirks force 1 bit width only\n");
    }

    /* Set hard-coded, that the controller support High-Capacity */
    mmc->host_caps |= MMC_MODE_HC;
    DEBUGF("[DEBUG] - Host support High Capacity\n");

    /* Set hard-coded, that the controller support Dual Data Rate (DDR) */
    mmc->host_caps |= MMC_MODE_DDR;

#ifndef CONFIG_MMC_SKIP_INIT
    DEBUGF("[DEBUG] - Reset ALL\n");
	sdhci_reset(host, SDHCI_RESET_ALL);
#endif
	mmc_register(mmc);

	return 0;
}
