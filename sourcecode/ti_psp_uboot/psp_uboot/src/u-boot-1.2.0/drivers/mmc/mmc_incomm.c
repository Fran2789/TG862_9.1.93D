/*
 * Copyright 2008, Freescale Semiconductor, Inc
 * Andy Fleming
 *
 * Based vaguely on the Linux code
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
 */

/* 
 * Includes Intel Corporation's changes/modifications dated: 2011. 
 * Changed/modified portions - Copyright © 2012 , Intel Corporation.   
 */ 

#include <config.h>
#include <common.h>
#include <command.h>
#include <mmc.h>
#include <malloc.h>
#include <linux/list.h>

#if defined(CONFIG_USE_HW_MUTEX)
#include <puma6_hw_mutex.h>
#endif


#define ENV_BASE_DEC    10
#define ENV_BASE_HEX    16

#define HAS_RESPONSE    1
#define NO_RESPONSE     0

/* eMMC manufacture commands */
#define MMC_CMD_60      60
#define MMC_CMD_61      61
#define MMC_CMD_62      62
#define MMC_CMD_63      63

/* InComm argumants opcode*/
#define READ_ROM_VER         0x08f70001
#define READ_FW_VER          0x08f70002
#define READ_FLASH_TYPE      0x08f70003
#define READ_BAD_BLOCKS      0x08f70004
#define READ_ERASE_STAT      0x07f80005
#define READ_PCODE_STAT      0x01FE0000
#define PCODE_ERASE_STAT     0x02FD0000
#define GO_TO_DOWNLOAD_PROC  0x00010307
#define GO_TO_MAM            0x00003233
#define GO_TO_MAM_WRITE      0x10000000
#define GO_TO_CID_WRITE      0x03FC694E
#define WRITE_PCODE          0x01FE0000
#define WRITE_STRUCT         0x08F70001
#define WRITE_CID            0x05FA0000
#define SET_WIDE_BUS         0xFF02D084
#define FULL_ERASE           0x07f80005
#define PCODE_ERASE          0x02FD0000


#define LENGTH_CTRL_VER   16
#define LENGTH_FW_VER     17
#define LENGTH_FLASH_TYPE 14
#define LENGTH_BAD_BLOCKS 16
#define LENGTH_PCODE_STAT 4
#define LENGTH_ERASE_STAT 4
#define LENGTH_PCODE_ERASE_STAT     512
#define FLASH_STRUCTURE_STRING_SIZE 512
#define FLASH_NMAE_STRING_SIZE 256

//#define INCOMM_DEBUG 

#ifdef  INCOMM_DEBUG
#define DEBUGF(fmt,args...) printf(fmt ,##args)
#else
#define DEBUGF(fmt,args...)
#endif


/* helper functions */
static void dump_hex(char *version,int len);

/* mmc commands */
static int mmc_send_op_cond_only(struct mmc *mmc);
static int mmc_send_all_cid(struct mmc *mmc);
static int mmc_set_relative_addr(struct mmc *mmc);
static int mmc_select_card(struct mmc *mmc);
static int mmc_stop_transmission(struct mmc *mmc, int has_response);
static int mmc_write_tcode(struct mmc *mmc,lbaint_t blkcnt, void *src);
static int mmc_go_to_download_procedure(struct mmc *mmc);
static int mmc_go_mam(struct mmc *mmc);
static int mmc_read_status(struct mmc *mmc,void *dst, unsigned int type, int length,  unsigned int has_response);
static int mmc_write_pcode(struct mmc *mmc, void *src, int length);
static int mmc_write_flash_structure(struct mmc *mmc, void *src);
static int mmc_go_mam_write(struct mmc *mmc);
static int mmc_set_wide_bus(struct mmc *mmc);
static int mmc_full_erase(struct mmc *mmc);
static int mmc_pcode_erase(struct mmc *mmc);
static int mmc_enter_to_tran_state(struct mmc *mmc);

/* incomm procedure */
static int download_tcode(struct mmc *mmc, void* src, int length);
static int read_flash_type(struct mmc *mmc, char *flash_type);
static int read_rom_version(struct mmc *mmc, char *ctrl_version);
static int read_fw_version(struct mmc *mmc, char *fw_version);
static int write_flash_structure(struct mmc *mmc,char *flash_string);
static int bad_blocks_check(struct mmc *mmc, int* over_limit, int* bad_sectors_count);
static int full_erase(struct mmc *mmc);
static int mmc_power_on(struct mmc *mmc, int pwr);
static int mmc_host_ctrl_init(struct mmc *mmc);
static int setcid(struct mmc *mmc);
static int pcode_erase(struct mmc *mmc);

/* user commands */
static void do_init_tcode_handshake(char *src, unsigned int length);
static void do_download_pcode(char *src, unsigned int length);
static void do_read_fw_version(void);
static void do_read_bad_blocks_status(void);
static void do_full_erase(void);
static void do_setcid(void);



/*******************************************************************************************************************/
/**       auxiliary helpers functions                                                                              */
/*******************************************************************************************************************/

/* Dump Hex string to screen*/
static void dump_hex(char *version,int len)
{
    int i=0;

    for (i=0;i<len;i++)
    {
        printf("%02X ",version[i]);
    }
    printf(" \n");
}


/*******************************************************************************************************************/
/**       eMMC commands functions                                                                                  */
/*******************************************************************************************************************/


/*  CMD1 - Send OCR (Operation Condition Register) */
static int mmc_send_op_cond_only(struct mmc *mmc)
{
	int timeout = 10000;
	struct mmc_cmd cmd;

 	/* Asking to the card its capabilities */
 	cmd.cmdidx = MMC_CMD_SEND_OP_COND;
 	cmd.resp_type = MMC_RSP_R3;
 	cmd.cmdarg = 0;
 	cmd.flags = 0;

    DEBUGF("[DEBUG] - mmc_send_op_cond_only: Send Cmd %d, arg 0x%X, \n",cmd.cmdidx,cmd.cmdarg);
    if (mmc_send_cmd(mmc, &cmd, NULL) != 0)
    {
        printf("Error: Failed to send Cmd %d, arg 0x%X, \n",cmd.cmdidx,cmd.cmdarg);
        return -1;
    }

 	udelay(1000);

	do {
		cmd.cmdidx = MMC_CMD_SEND_OP_COND;
		cmd.resp_type = MMC_RSP_R3;
		cmd.cmdarg = (mmc_host_is_spi(mmc) ? 0 :
				(mmc->voltages &
				(cmd.response[0] & OCR_VOLTAGE_MASK)) |
				(cmd.response[0] & OCR_ACCESS_MODE));

		if (mmc->host_caps & MMC_MODE_HC)
			cmd.cmdarg |= OCR_HCS;

		cmd.flags = 0;

        DEBUGF("[DEBUG] - mmc_send_op_cond_only: Send Cmd %d, arg 0x%X, \n",cmd.cmdidx,cmd.cmdarg);
        if (mmc_send_cmd(mmc, &cmd, NULL) != 0)
        {
            printf("Error: Failed to send Cmd %d, arg 0x%X, \n",cmd.cmdidx,cmd.cmdarg);
            return -1;
        }

		udelay(1000);
	} while (!(cmd.response[0] & OCR_BUSY) && timeout--);

    if (timeout <= 0)
    {
        printf("Error: Send OP Cond no  response on time-out\n");
    }

    mmc->version = MMC_VERSION_UNKNOWN;
	mmc->ocr = cmd.response[0];

	mmc->high_capacity = ((mmc->ocr & OCR_HCS) == OCR_HCS);
	mmc->rca = 1;

    return 0;
}

/*  CMD2 - Send CID */
static int mmc_send_all_cid(struct mmc *mmc)
{
	struct mmc_cmd cmd;

	/* Put the Card in Identify Mode */
	cmd.cmdidx = MMC_CMD_ALL_SEND_CID; 
	cmd.resp_type = MMC_RSP_R2;
	cmd.cmdarg = 0;
	cmd.flags = 0;

    DEBUGF("[DEBUG] - mmc_send_all_cid: Send Cmd %d, arg 0x%X, \n",cmd.cmdidx,cmd.cmdarg);
    if (mmc_send_cmd(mmc, &cmd, NULL) != 0)
    {
        printf("Error: Failed to send Cmd %d, arg 0x%X, \n",cmd.cmdidx,cmd.cmdarg);
        return -1;
    }

	memcpy(mmc->cid, cmd.response, 16);

    return 0;
}

/*  CMD3 -  Set Card address */
static int mmc_set_relative_addr(struct mmc *mmc)
{
    struct mmc_cmd cmd;

    cmd.cmdidx = SD_CMD_SEND_RELATIVE_ADDR;
    cmd.cmdarg = mmc->rca << 16;
    cmd.resp_type = MMC_RSP_R6;
    cmd.flags = 0;

    DEBUGF("[DEBUG] - mmc_set_relative_addr:Send Cmd %d, arg 0x%X, \n",cmd.cmdidx,cmd.cmdarg);
    if (mmc_send_cmd(mmc, &cmd, NULL) != 0)
    {
        printf("Error: Failed to send Cmd %d, arg 0x%X, \n",cmd.cmdidx,cmd.cmdarg);
        return -1;
    }

    return 0;
}

/* CMD7 - Select Card */
static int mmc_select_card(struct mmc *mmc)
{
    struct mmc_cmd cmd;

    cmd.cmdidx = MMC_CMD_SELECT_CARD;
    cmd.resp_type = MMC_RSP_R1;
    cmd.cmdarg = mmc->rca << 16;
    cmd.flags = 0;
    DEBUGF("[DEBUG] - mmc_select_card: Send Cmd %d, arg 0x%X, \n",cmd.cmdidx,cmd.cmdarg);
    if (mmc_send_cmd(mmc, &cmd, NULL) != 0)
    {
        printf("Error: Failed to send Cmd %d, arg 0x%X, \n",cmd.cmdidx,cmd.cmdarg);
        return -1;
    }

    return 0;
}


// CMD12 - Stop Transmition, and CMD13 - Send Status 
static int mmc_stop_transmission(struct mmc *mmc, int has_response)
{
    struct mmc_cmd cmd;
	int timeout = 1000;

    cmd.cmdidx = MMC_CMD_STOP_TRANSMISSION;
	cmd.cmdarg = 0;
    if (has_response == NO_RESPONSE)
    {
        cmd.resp_type = MMC_RSP_NONE;
    }
    else
    {
        cmd.resp_type = MMC_RSP_R1b;
    }
	cmd.flags = 0;
    DEBUGF("[DEBUG] - mmc_stop_transmission: Send Cmd %d, arg 0x%X, \n",cmd.cmdidx,cmd.cmdarg);
	if (mmc_send_cmd(mmc, &cmd, NULL) != 0) {
		printf("Error: Failed to send Cmd %d, arg 0x%X, \n",cmd.cmdidx,cmd.cmdarg);
        return -1;
    }

    if (has_response == NO_RESPONSE)
        return 0;

    /* Waiting for the ready status */
    if (mmc_send_status(mmc, timeout) != 0)
    {
        printf("Error: Failed to get eMMC Status \n",cmd.cmdidx,cmd.cmdarg);
        return -1;
    }

    return 0;
}


/* CMD25 - write new tCoce image */
static int mmc_write_tcode(struct mmc *mmc,lbaint_t blkcnt, void *src)
{
    struct mmc_cmd cmd;
    struct mmc_data data;

	cmd.cmdidx = MMC_CMD_WRITE_MULTIPLE_BLOCK;
	cmd.resp_type = MMC_RSP_R1;
	cmd.cmdarg = 0;
	cmd.flags = 0;

	data.src = src;
	data.blocks = blkcnt;
	data.blocksize = mmc->write_bl_len;
	data.flags = MMC_DATA_WRITE;

    DEBUGF("[DEBUG] - mmc_write_tcode: Send Cmd %d, arg 0x%X, \n",cmd.cmdidx,cmd.cmdarg);
	if (mmc_send_cmd(mmc, &cmd, &data) != 0)
    {
        printf("Error: Failed to send Cmd %d, arg 0x%X, \n",cmd.cmdidx,cmd.cmdarg);
        return -1;
    }

    return 0;
}

/* CMD60 - go to download procedure */
static int mmc_go_to_download_procedure(struct mmc *mmc)
{
    struct mmc_cmd cmd;

	cmd.cmdidx = MMC_CMD_60;
	cmd.resp_type = MMC_RSP_R3;
	cmd.cmdarg = GO_TO_DOWNLOAD_PROC;
	cmd.flags = 0;

    DEBUGF("[DEBUG] - mmc_go_to_download_procedure: Send Cmd %d, arg 0x%X, \n",cmd.cmdidx,cmd.cmdarg);
	if (mmc_send_cmd(mmc, &cmd, NULL) != 0)
    {
        printf("Error: Failed to send Cmd %d, arg 0x%X, \n",cmd.cmdidx,cmd.cmdarg);
        return -1;
    }

    return 0;
}

/* CMD60 - Enter 'Memory Access Mode' - MAM */
static int mmc_go_mam(struct mmc *mmc)
{
    struct mmc_cmd cmd;

	cmd.cmdidx = MMC_CMD_60;
	cmd.resp_type = MMC_RSP_R1;
	cmd.cmdarg = GO_TO_MAM;
	cmd.flags = 0;

    DEBUGF("[DEBUG] - mmc_go_mam: Send Cmd %d, arg 0x%X, \n",cmd.cmdidx,cmd.cmdarg);
    if (mmc_send_cmd(mmc, &cmd, NULL) != 0) {
		printf("Error: Failed to send Cmd %d, arg 0x%X, \n",cmd.cmdidx,cmd.cmdarg);
        return -1;
    }
    
    return 0;
}

/* CMD61 - Read Status command */
static int mmc_read_status(struct mmc *mmc,void *dst, unsigned int type, int length,  unsigned int has_response)
{
    struct mmc_cmd cmd;
    struct mmc_data data;

	cmd.cmdidx = MMC_CMD_61;
	cmd.resp_type = (has_response == HAS_RESPONSE) ? MMC_RSP_R1 : MMC_RSP_NONE;
    cmd.cmdarg = type;
	cmd.flags = 0;

    data.dest = dst;
	data.blocks = 1;
	data.blocksize = length;
	data.flags = MMC_DATA_READ;

    DEBUGF("[DEBUG] - mmc_read_status: Send Cmd %d, arg 0x%X, \n",cmd.cmdidx,cmd.cmdarg);
    if (mmc_send_cmd(mmc, &cmd, &data) != 0)
    {
        printf("Error: Failed to send Cmd %d, arg 0x%X, \n",cmd.cmdidx,cmd.cmdarg);
        return -1;
    }

    return 0;
}



/* CMD62 - write new pCoce image */
static int mmc_write_pcode(struct mmc *mmc, void *src, int length)
{
    struct mmc_cmd cmd;
    struct mmc_data data;
    lbaint_t blkcnt;
    
    blkcnt = ALIGN(length, mmc->write_bl_len) / mmc->write_bl_len;


	cmd.cmdidx = MMC_CMD_62;
	cmd.resp_type = MMC_RSP_NONE;
	cmd.cmdarg = (WRITE_PCODE | blkcnt);
	cmd.flags = 0;

	data.src = src;
	data.blocks = blkcnt;
	data.blocksize = mmc->write_bl_len;
	data.flags = MMC_DATA_WRITE;

    DEBUGF("[DEBUG] - mmc_write_pcode: Send Cmd %d, arg 0x%X, \n",cmd.cmdidx,cmd.cmdarg);
	if (mmc_send_cmd(mmc, &cmd, &data) != 0)
    {
        printf("Error: Failed to send Cmd %d, arg 0x%X, \n",cmd.cmdidx,cmd.cmdarg);
        return -1;
    }

    return 0;
}


// Send CMD 62 - Send Flash Structure String
static int mmc_write_flash_structure(struct mmc *mmc, void *src)
{
    struct mmc_cmd cmd;
    struct mmc_data data;

	cmd.cmdidx = MMC_CMD_62;
	cmd.resp_type = MMC_RSP_NONE;
	cmd.cmdarg = WRITE_STRUCT;
	cmd.flags = 0;

	data.src = src;
	data.blocks = 1;
	data.blocksize = FLASH_STRUCTURE_STRING_SIZE;
	data.flags = MMC_DATA_WRITE;

    DEBUGF("[DEBUG] - mmc_write_flash_structure: Send Cmd %d, arg 0x%X, \n",cmd.cmdidx,cmd.cmdarg);
	if (mmc_send_cmd(mmc, &cmd, &data) != 0)
    {
        printf("Error: Failed to send Cmd %d, arg 0x%X, \n",cmd.cmdidx,cmd.cmdarg);
        return -1;
    }

    return 0;
}

// Send CMD 62 - Send CID Structure
static int mmc_write_cid_structure(struct mmc *mmc, void *src)
{
    struct mmc_cmd cmd;
    struct mmc_data data;

	cmd.cmdidx = MMC_CMD_62;
	cmd.resp_type = MMC_RSP_NONE;
	cmd.cmdarg = WRITE_CID;
	cmd.flags = 0;

	data.src = src;
	data.blocks = 1;
	data.blocksize = 152;
	data.flags = MMC_DATA_WRITE;

    DEBUGF("[DEBUG] - mmc_write_cid_structure: Send Cmd %d, arg 0x%X, \n",cmd.cmdidx,cmd.cmdarg);
	if (mmc_send_cmd(mmc, &cmd, &data) != 0)
    {
        printf("Error: Failed to send Cmd %d, arg 0x%X, \n",cmd.cmdidx,cmd.cmdarg);
        return -1;
    }

    return 0;
}

// Send CMD 62 - Erase pCode
static int mmc_pcode_erase(struct mmc *mmc)
{
    struct mmc_cmd cmd;
    struct mmc_data data;
    char src[] = "INSystem";

    cmd.cmdidx = MMC_CMD_62;
    cmd.resp_type = MMC_RSP_NONE;
    cmd.cmdarg = PCODE_ERASE;
    cmd.flags = 0;

    data.src = src;
    data.blocks = 1;
    data.blocksize = 8;
    data.flags = MMC_DATA_WRITE;

    DEBUGF("[DEBUG] - mmc_write_flash_structure: Send Cmd %d, arg 0x%X, string = %s\n",cmd.cmdidx,cmd.cmdarg,src);
    if (mmc_send_cmd(mmc, &cmd, &data) != 0)
    {
        printf("Error: Failed to send Cmd %d, arg 0x%X, \n",cmd.cmdidx,cmd.cmdarg);
        return -1;
    }

    return 0;
}


/* Send CMD63 - Enter 'Memory Access Write Mode' - MAM Write */
static int mmc_go_mam_write(struct mmc *mmc)
{
    struct mmc_cmd cmd;

	cmd.cmdidx = MMC_CMD_63;
	cmd.resp_type = MMC_RSP_NONE;
	cmd.cmdarg = GO_TO_MAM_WRITE;
	cmd.flags = 0;

    DEBUGF("[DEBUG] - mmc_go_mam_write: Send Cmd %d, arg 0x%X, \n",cmd.cmdidx,cmd.cmdarg);
    if (mmc_send_cmd(mmc, &cmd, NULL) != 0)
    {
        printf("Error: Failed to send Cmd %d, arg 0x%X, \n",cmd.cmdidx,cmd.cmdarg);
        return -1;
    }

    return 0; 
}

/* Send CMD 63 - Set Wide bus */
static int mmc_set_wide_bus(struct mmc *mmc)
{
    struct mmc_cmd cmd;

	cmd.cmdidx = MMC_CMD_63;
	cmd.resp_type = MMC_RSP_NONE;
	cmd.cmdarg = SET_WIDE_BUS;
	cmd.flags = 0;

    DEBUGF("[DEBUG] - mmc_set_wide_bus: Send Cmd %d, arg 0x%X, \n",cmd.cmdidx,cmd.cmdarg);
	if (mmc_send_cmd(mmc, &cmd, NULL) != 0)
    {
        printf("Error: Failed to send Cmd %d, arg 0x%X, \n",cmd.cmdidx,cmd.cmdarg);
        return -1;
    }

    return 0;
}

/* Send CMD 63 - Perform full erase */
static int mmc_full_erase(struct mmc *mmc)
{
     struct mmc_cmd cmd;

	cmd.cmdidx = MMC_CMD_63;
	cmd.resp_type = MMC_RSP_NONE;
	cmd.cmdarg = FULL_ERASE;
	cmd.flags = 0;

    DEBUGF("[DEBUG] - mmc_full_erase: Send Cmd %d, arg 0x%X, \n",cmd.cmdidx,cmd.cmdarg);
	if (mmc_send_cmd(mmc, &cmd, NULL) != 0)
    {
        printf("Error: Failed to send Cmd %d, arg 0x%X, \n",cmd.cmdidx,cmd.cmdarg);
        return -1;
    }

    return 0;
}

/* Send CMD 63 - Enter CID write mode */
static int mmc_cid_write_mode(struct mmc *mmc)
{
    struct mmc_cmd cmd;

	cmd.cmdidx = MMC_CMD_63;
	cmd.resp_type = MMC_RSP_NONE;
	cmd.cmdarg = GO_TO_CID_WRITE;
	cmd.flags = 0;

    DEBUGF("[DEBUG] - mmc_cid_write_mode: Send Cmd %d, arg 0x%X, \n",cmd.cmdidx,cmd.cmdarg);
	if (mmc_send_cmd(mmc, &cmd, NULL) != 0)
    {
        printf("Error: Failed to send Cmd %d, arg 0x%X, \n",cmd.cmdidx,cmd.cmdarg);
        return -1;
    }

    return 0;
}

/*******************************************************************************************************************/
/**       eMMC InCOMM procedure functions                                                                          */
/*******************************************************************************************************************/


/* Initialize eMMC card - go to trans state */
static int mmc_enter_to_tran_state(struct mmc *mmc)
{
    int err;

    /* Send CMD0 - Enter to Idle state*/
    DEBUGF("Enter to Idle state\n");
    mmc_go_idle(mmc);

    /* Send CMD1 - Send OP Cond */
    DEBUGF("Send OCR\n");
    err = mmc_send_op_cond_only(mmc);
	if (err) {
		printf("Card did not respond to voltage select!\n");
        return -1;
    }
 
    /* Send CMD2 - All Send CID */
    DEBUGF("Send CID\n");
    mmc_send_all_cid(mmc);

    /* Send CMD3 - Send Relative Address */
    DEBUGF("Send RCA\n");
    mmc_set_relative_addr(mmc);

    /* Send CMD7 - Select Card */
    DEBUGF("Select card\n");
    mmc_select_card(mmc);

	return 0;
}


/* Write tCode to flash */
static int download_tcode(struct mmc *mmc, void* src, int length)
{
    
    lbaint_t blkcnt;
    
    blkcnt = ALIGN(length, mmc->write_bl_len) / mmc->write_bl_len;

    /* Send CMD0 - Go to idle state */
    DEBUGF("Enter idle state");
    mmc_go_idle(mmc);

    /* CMD60 - Enter 'Memory Access Mode' - MAM */
    DEBUGF("Enter 'Memory Access Mode' - MAM\n");
    mmc_go_mam(mmc);

    /* CMD63 - Enter 'Memory Access Write Mode' - MAM Write */
    DEBUGF("Enter 'Memory Access Write Mode' - MAM Write\n");
    mmc_go_mam_write(mmc);

    /* CMD63 - Set Wide bus */
    DEBUGF("Set wide bus - 4 bits\n");
    mmc_set_wide_bus(mmc);

    /* Set host controller to 4 bit */
    DEBUGF("Set bus width to 4\n");
    mmc_set_bus_width(mmc, 4);

    /* Send CMD16 - Set Block len to 512 bytes */
    DEBUGF("Set Block length to %d bytes\n", mmc->write_bl_len);
    mmc_set_blocklen(mmc, mmc->write_bl_len);

    /* Send CMD25 - Write tCode */
    DEBUGF("Write tCode to flash\n");
    mmc_write_tcode(mmc, blkcnt, src);

    /* Send CMD12 - Stop Transfer */
    DEBUGF("Stop Transfer\n");
    mmc_stop_transmission(mmc,0);

    return 0;
}


// write pCode to flash
static int download_pcode(struct mmc *mmc, void* src, int length)
{

    char response_version[LENGTH_PCODE_STAT+1] = {0};
    
    /* Send CMD6 - Set bus width to 4 */
    DEBUGF("Set mmc bus width to 4\n");
    mmc_switch(mmc, EXT_CSD_CMD_SET_NORMAL,EXT_CSD_BUS_WIDTH, 1);

    /* Set controller bus width to 4 */
    DEBUGF("Set controller bus width to 4\n");
    mmc_set_bus_width(mmc,4);

    /* Send CMD16 - Set Block len to 512 bytes */
    DEBUGF("Set Block length to %d bytes\n",mmc->write_bl_len);
    mmc_set_blocklen(mmc, mmc->write_bl_len);
 
    /* Send CMD62 - Write pCode to flash */
    DEBUGF("Download pCode\n");
    mmc_write_pcode(mmc,src,length);

    /* Send CMD12 - Stop Transfer */
    DEBUGF("Stop Transfer\n");
    mmc_stop_transmission(mmc,1);

    /* Send CMD16 - Set Block len to 4 bytes */
    DEBUGF("Set Block length to %d bytes\n",LENGTH_PCODE_STAT);
    mmc_set_blocklen(mmc, LENGTH_PCODE_STAT);

    /* Send CMD61 - Read download status */
    DEBUGF("Read download status\n");
    mmc_read_status(mmc,response_version,READ_PCODE_STAT,LENGTH_PCODE_STAT,NO_RESPONSE);

    /* Send CMD12 - Stop Transfer */
    DEBUGF("Stop Transfer\n");
    mmc_stop_transmission(mmc,1);

    printf("pCode download response status: ");
    dump_hex(response_version,LENGTH_PCODE_STAT);
    
    if ((response_version[0] == 0x01) && (response_version[1] == 0x00))
    {
        printf("Response OK\n");
        return 0;
    }
    printf("Error: Response is not OK\n");
    return -1;

}

static int pcode_erase(struct mmc *mmc)
{
    char response_erase  [LENGTH_PCODE_ERASE_STAT+1] = {0};
    
    /* Send CMD6 - Set bus width to 4 */
    DEBUGF("Set mmc bus width to 4\n");
    mmc_switch(mmc, EXT_CSD_CMD_SET_NORMAL,EXT_CSD_BUS_WIDTH, 1);

    /* Set controller bus width to 4 */
    DEBUGF("Set controller bus width to 4\n");
    mmc_set_bus_width(mmc,4);

    /* Send CMD16 - Set Block len to 512 bytes */
    DEBUGF("Set Block length to %d bytes\n",mmc->write_bl_len);
    mmc_set_blocklen(mmc, mmc->write_bl_len);

    /* Send CMD61 - Erase pCode from flash and read erase status */
    DEBUGF("Erase pCode\n");
    mmc_read_status(mmc,response_erase,PCODE_ERASE_STAT,LENGTH_PCODE_ERASE_STAT,HAS_RESPONSE);

    //printf("InComm eMMC erase response: ");
    //dump_hex(response_erase,LENGTH_PCODE_ERASE_STAT);

    DEBUGF("InComm eMMC erase response: %s\n",(response_erase+0x04));

    /* Send CMD12 - Stop Transfer */
    DEBUGF("Stop Transfer\n");
    mmc_stop_transmission(mmc,1);

    if (strncmp(response_erase+20,"INSystem",8) != 0)
    {
        printf("Error: InComm eMMC erase response: %s\n",(response_erase+0x04));
        return -1;
    }


     /* Send CMD16 - Set Block len to 512 bytes */
    DEBUGF("Set Block length to %d bytes\n",8);
    mmc_set_blocklen(mmc, 8);


    /* Send CMD62 - Erase pCode */
    mmc_pcode_erase(mmc);


    /* Send CMD12 - Stop Transfer */
    DEBUGF("Stop Transfer\n");
    mmc_stop_transmission(mmc,1);


    return 0;
}

/* Read NAND flash type */
static int read_flash_type(struct mmc *mmc, char *flash_type)
{

    /* Send CMD16 - Set Block len to 14 bytes */
    DEBUGF("Set Block len to %d bytes\n",LENGTH_FLASH_TYPE);
    mmc_set_blocklen(mmc,LENGTH_FLASH_TYPE);

    /* Send CMD61 - Start Reading Controller Version */
    DEBUGF("Start Reading flash type\n");
    mmc_read_status(mmc,flash_type,READ_FLASH_TYPE,LENGTH_FLASH_TYPE,HAS_RESPONSE);

    /* Send CMD12 - Stop Transfer */
    DEBUGF("Stop Transfer\n");
    mmc_stop_transmission(mmc,1);

    return 0;
}

/* Read ROM version - bCode version */
static int read_rom_version(struct mmc *mmc, char *ctrl_version)
{
    /* Send CMD16 - Set Block len to 16 bytes */
    DEBUGF("Set Block len to %d bytes\n",LENGTH_CTRL_VER);
    mmc_set_blocklen(mmc,LENGTH_CTRL_VER);

    /* Send CMD61 - Start Reading Controller Version */
    DEBUGF("Start Reading Controller Version\n");
    mmc_read_status(mmc,ctrl_version, READ_ROM_VER,LENGTH_CTRL_VER,HAS_RESPONSE);

    /* Send CMD12 - Stop Transfer */
    DEBUGF("Stop Transfer\n");
    mmc_stop_transmission(mmc,1);
    
	return 0;
}

/* Read FW version */
static int read_fw_version(struct mmc *mmc, char *fw_version)
{
    /* Send CMD16 - Set Block len to 20 bytes */
    DEBUGF("Set Block len to %d bytes\n",LENGTH_FW_VER);
    mmc_set_blocklen(mmc,LENGTH_FW_VER);

    /* Send CMD61 - Start Reading FW Version */
    DEBUGF("Start Reading FW Version\n");
    mmc_read_status(mmc,(void*)fw_version,READ_FW_VER,LENGTH_FW_VER,HAS_RESPONSE);
 
    /* Send CMD12 - Stop Transfer */
    DEBUGF("Stop Transfer\n");
    mmc_stop_transmission(mmc,1);

    /* Send CMD16 - Set Block len to 512 bytes */
    DEBUGF("Set Block len to %d bytes\n",mmc->write_bl_len);
    mmc_set_blocklen(mmc,mmc->write_bl_len);

	return 0;
}


/* upload Flash type structure */
static int write_flash_structure(struct mmc *mmc,char *flash_string)
{
    
    /* Send CMD6 - Set bus width to 4 */
    DEBUGF("Set mmc bus width to 4\n");
    mmc_switch(mmc, EXT_CSD_CMD_SET_NORMAL,EXT_CSD_BUS_WIDTH, 1);

    /*Set controller bus width to 4 */
    DEBUGF("Set controller bus width to 4\n");
    mmc_set_bus_width(mmc,4);

    /* Send CMD16 - Set Block len to 16 bytes */
    DEBUGF("Set Block length to %d bytes\n",FLASH_STRUCTURE_STRING_SIZE);
    mmc_set_blocklen(mmc, FLASH_STRUCTURE_STRING_SIZE);

    /* Send CMD62 - Send Flash Structure String */
    DEBUGF("Write flash_structure string\n");
    mmc_write_flash_structure(mmc,flash_string);
 
    /* Send CMD12 - Stop Transfer */
    DEBUGF("Stop Transfer\n");
    mmc_stop_transmission(mmc,1);


	return 0;
}

/* Read Bad Blocks status */
static int bad_blocks_check(struct mmc *mmc, int* over_limit, int* bad_sectors_count)
{
    char bad_blocks[LENGTH_BAD_BLOCKS+1] = {0};

    /* Send CMD6 - Set bus width to 4 */
    DEBUGF("Set mmc bus width to 4\n");
    mmc_switch(mmc, EXT_CSD_CMD_SET_NORMAL,EXT_CSD_BUS_WIDTH, 1);

    /* Set controller bus width to 4 */
    DEBUGF("Set controller bus width to 4\n");
    mmc_set_bus_width(mmc,4);

    /* Send CMD16 - Set Block len to 16 bytes */
    DEBUGF("Send CMD16 - Set Block len to %d bytes\n",LENGTH_BAD_BLOCKS);
    mmc_set_blocklen(mmc,LENGTH_BAD_BLOCKS);

    /* Send CMD61 - Start Reading Bad Blocks info */
    DEBUGF("Send CMD61 - Start Reading Bad Blocks info\n");
    mmc_read_status(mmc,bad_blocks, READ_BAD_BLOCKS,LENGTH_BAD_BLOCKS,HAS_RESPONSE);

    /* Send CMD12 - Stop Transfer */
    DEBUGF("Send CMD12 - Stop Transfer\n");
    mmc_stop_transmission(mmc,1);
    
    /* Dump bad-blocks info */
    printf("Bad blocks status:");
    dump_hex(bad_blocks,LENGTH_BAD_BLOCKS);

    /* Set output - bad block over the limit */
    *over_limit = ((bad_blocks[4]>>7)&0x01);

    /* Set output - number bad block */
    *bad_sectors_count = ((bad_blocks[2]<<8) | bad_blocks[3]);
    
    return 0;
}

/* Perform Full Erase */
static int full_erase(struct mmc *mmc)
{
    char full_erase_status[LENGTH_ERASE_STAT+1] = {0};
	int timeout = 1000;

    // Send CMD16 - Set Block len to 4 bytes
    DEBUGF("Set Block len to %d bytes\n",LENGTH_ERASE_STAT);
    mmc_set_blocklen(mmc,LENGTH_ERASE_STAT);

    // Send CMD6 - Set bus width to 4
    DEBUGF("Set mmc bus width to 4\n");
    mmc_switch(mmc, EXT_CSD_CMD_SET_NORMAL,EXT_CSD_BUS_WIDTH, 1);

    // Set controller bus width to 4
    DEBUGF("Set controller bus width to 4\n");
    mmc_set_bus_width(mmc,4);

    // Send CMD63 - Perform full erase
    DEBUGF("Perform full erase\n");
    mmc_full_erase(mmc);

    // Send CMD13 - Waiting for the ready status
    DEBUGF("Wait for ready status\n");
    mmc_send_status(mmc, timeout);

    // Send CMD61 - Read response value
    DEBUGF("Check full erase status\n");
    mmc_read_status(mmc,full_erase_status, READ_ERASE_STAT,LENGTH_ERASE_STAT,HAS_RESPONSE);

    // Send CMD12 - Stop Transfer
    DEBUGF("Stop Transfer\n");
    mmc_stop_transmission(mmc,1);

    printf("Status:");
    dump_hex(full_erase_status,LENGTH_ERASE_STAT);

    if (full_erase_status[0] == 0x07 && full_erase_status[1] == 0x00)
    {
        return 0;
    }

    return -1;
}


/* Power on/off the eMMC card */
static int mmc_power_on(struct mmc *mmc, int pwr)
{
    /* Power On/Off the eMMC Incomm Controller */
    switch(pwr)
    {
    case 0:
        mmc->ioctl(mmc,MMC_IOCTL_HOST_POWER_OFF,0);
        break;
    case 1:
        mmc->ioctl(mmc,MMC_IOCTL_HOST_POWER_ON,0);
        break;
    }

    /* 100ms delay */
    udelay (10000);  //100ms

    return 0;
}


/* SDHCI Host controller initialization */
static int mmc_host_ctrl_init(struct mmc *mmc)
{
	int err = 0;

    DEBUGF("[DEBUG] - mmc_host_ctrl_init: SDHCI Init\n");
	err = mmc->init(mmc);

	if (err)
    {
        printf("Error: SDHCI init failed\b");
		return err;
    }

    DEBUGF("[DEBUG] - mmc_host_ctrl_init: Set bus width to 1\n");
    mmc_set_bus_width(mmc, 1);

    DEBUGF("[DEBUG] - mmc_host_ctrl_init: Set clock to 400000\n");
	mmc_set_clock(mmc, 400000);

	return err;
}

/* Write CID */
static int setcid(struct mmc *mmc)
{
    char cid_full[152] = {0xFF};
    char cid[16]       = {0xC7,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x31,0x01,0x00,0x00,0x00,0x03,0xBF,0xFF};

    /* Prepare CID structure */
    memset(cid_full,0xFF,152);
    memcpy(cid_full,cid,16);

    /* Send CMD16 - Set Block len to 152 bytes */
    DEBUGF("Set Block len to %d bytes\n",152);
    mmc_set_blocklen(mmc,152);


    /* Send CMD 63 - Enter CID write mode */
    mmc_cid_write_mode(mmc);

    /* Send CMD62 - Send */
    DEBUGF("Send CID.\n");
    mmc_write_cid_structure(mmc,cid_full);

     // Send CMD12 - Stop Transfer
    DEBUGF("Stop Transfer\n");
    mmc_stop_transmission(mmc,1);

    /* Send CMD16 - Set Block len to 512 bytes */
    DEBUGF("Set Block length to %d bytes\n", mmc->write_bl_len);
    mmc_set_blocklen(mmc, mmc->write_bl_len);

    return 0;
}


/*******************************************************************************************************************/
/**       eMMC user commands functions                                                                             */
/*******************************************************************************************************************/

/* Init the InCOMM test mode, download ETcode image, startup the eMMC, and perform handshake */
static void do_init_tcode_handshake(char *src, unsigned int length)
{
    struct mmc *mmc;
    int dev_num = 0;

    char ctrl_version[LENGTH_CTRL_VER+1] = {0};
    char flash_type[LENGTH_FLASH_TYPE+1] = {0};

    DEBUGF("Initializing test mode.\n");
    mmc = find_mmc_device(dev_num);
	if (!mmc)
    {
        printf("Error: fail to find mmc device #%d\n",dev_num);
		return;
    }

    printf("Initializing InComm to Test Mode...\n");

    /* Turn off eMMC card */
    DEBUGF("Turn OFF eMMC InComm Controller\n");
    mmc_power_on(mmc,0);
    
    /* Turn on eMMC card */
    DEBUGF("Turn ON eMMC InComm Controller\n");
    mmc_power_on(mmc,1);

    /* Initialize host controller */
    DEBUGF("Initialize host controller\n");
    mmc_host_ctrl_init(mmc);

    /* Enter to test mode */
    DEBUGF("Enter to boot loader mode\n");
    mmc_go_to_download_procedure(mmc);

    /* Start up the emmc card */
    DEBUGF("Startup eMMC card\n");
    mmc_enter_to_tran_state(mmc);


    read_rom_version(mmc,ctrl_version);
    printf("InComm controller ROM Version: %s\n",ctrl_version);

    printf("Downloading tCode.\n");

    /* Download tCode to flash */
    DEBUGF("Download tCode\n");
    download_tcode(mmc,src+FLASH_STRUCTURE_STRING_SIZE, length-FLASH_STRUCTURE_STRING_SIZE);
    printf("Complete.\n");

    /* Initialize host controller */
    DEBUGF("Initialize host controller\n");
    mmc_host_ctrl_init(mmc);


    /* Start up the emmc card */
    DEBUGF("Startup eMMC card\n");
    mmc_enter_to_tran_state(mmc);
    
    printf("eMMC is ready. \n");

    DEBUGF("Read NAND ID\n");
    read_flash_type(mmc,flash_type);

    printf("NAND ID: ");
    dump_hex(flash_type,LENGTH_FLASH_TYPE);


    DEBUGF("Writing structure string\n");
    write_flash_structure(mmc,src);

    printf("Test mode ready.\n");

    return;
}

/* Download InComm FW version to eMMC */
static void do_download_pcode(char *src, unsigned int length)
{
    struct mmc *mmc;
    int dev_num = 0;

    DEBUGF("Download new InComm FW version to eMMC\n");
    mmc = find_mmc_device(dev_num);
	if (!mmc)
    {
        printf("Error: fail to find mmc device #%d\n",dev_num);
		return;
    }


    printf("Erasing old InComm FW...\n");
    if (pcode_erase(mmc)  == 0)
    {
        printf("Done.\n");
    }
    else
    {
        printf("Failed.\n");
        return;
    }

    printf("Downloading new InComm FW...\n");

    /* download and program new pCoce version */
    if (download_pcode(mmc,src,length) == 0)
    {
        printf("Complete.\n");
    }
    else
    {
        printf("Failed.\n");
        return;
    }

    return;
}


/* Read NAND type */
static void do_read_flash_type(void)
{
    struct mmc *mmc;
    int dev_num = 0;

    char flash_type[LENGTH_FLASH_TYPE+1] = {0};

    DEBUGF("Print NAND flash type.\n");
    mmc = find_mmc_device(dev_num);
	if (!mmc)
    {
        printf("Error: fail to find mmc device #%d",dev_num);
		return;
    }

    /* Read flash type ID */
    DEBUGF("Read NAND ID\n");
    read_flash_type(mmc,flash_type);

    printf("NAND ID: ");
    dump_hex(flash_type,LENGTH_FLASH_TYPE);
    
    return;
}

/* Read FW version */
static void do_read_fw_version()
{
    struct mmc *mmc;
    int dev_num = 0;
    char fw_version[LENGTH_FW_VER+1] = {0};

    DEBUGF("Read InComm FW version\n");

    mmc = find_mmc_device(dev_num);
	if (!mmc)
    {
        printf("Error: fail to find mmc device #%d",dev_num);
		return;
    }

    read_fw_version(mmc,fw_version);
    printf("InComm eMMC FW Version: %s\n",fw_version);

    return;
}


/* Read Bad Blocks Status */
static void do_read_bad_blocks_status()
{
    struct mmc *mmc;
    int dev_num = 0;
    int over_limit = 0;
    int bad_sectors_count = 0;
  
    DEBUGF("Read Bad blocks status.\n");

    mmc = find_mmc_device(dev_num);
	if (!mmc)
    {
        printf("Error: fail to find mmc device #%d",dev_num);
		return;
    }

    bad_blocks_check(mmc,&over_limit,&bad_sectors_count);
    printf("NAND have %d bad blocks. Over the limit: %s.\n",bad_sectors_count,((over_limit==1)?"YES":"NO"));


    return;
}

/* Perform Full Erase */
static void do_full_erase()
{
    struct mmc *mmc;
    int dev_num = 0;
  
    DEBUGF("Perfrom eMMC full erase.\n");

    mmc = find_mmc_device(dev_num);
	if (!mmc)
    {
        printf("Error: fail to find mmc device #%d",dev_num);
		return;
    }

    printf("Ereasing eMMC..\n");

    /* Erase flash */
    if (full_erase(mmc) == 0)
    {
        printf("Done - OK.\n");
    }
    else
    {
        printf("Failed.\n");
    }


    return;
}

/* Write CID */
static void  do_setcid()
{
    struct mmc *mmc;
    int dev_num = 0;
  
    DEBUGF("Set CID.\n");

    mmc = find_mmc_device(dev_num);
	if (!mmc)
    {
        printf("Error: fail to find mmc device #%d",dev_num);
		return;
    }

    printf("CID \n");
    setcid(mmc);

return;
}



/*******************************************************************************************************************/
/**       Main shell command functions                                                                             */
/*******************************************************************************************************************/

int do_incomm_test (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
    int i=0;
   
#if defined(CONFIG_USE_HW_MUTEX)
    /* Lock the HW Mutex */
    if (hw_mutex_lock(HW_MUTEX_EMMC) == 0)
    {
        printf("Error: failed to lock HW Mutex\n");
        return 0;
    }
    DEBUGF("hw mutex is locked \n");
#endif

    /* Parse Command Line   */
    /* -------------------- */
    for (i=0;i<argc;i++)
    {
        if (strcmp(argv[i], "fw") == 0)
        {
            /* Run Test initialization */
            do_read_fw_version();
        }

        if (strcmp(argv[i], "testmode") == 0)
        {
            /* Get source adddress */
            void *src = (void*)simple_strtoul(argv[i+1], NULL, ENV_BASE_HEX);

            /* Get length */
            unsigned int length = (unsigned int)simple_strtoul(argv[i+2], NULL, ENV_BASE_HEX);

            if (src == 0)
            {
                printf("Error: source is zero.\n");
                printf ("Usage:\n%s\n", cmdtp->usage);
                return 1;
            }
            if (src < ((void *)0x40000000) || src > ((void *)0x80000000))
            {
                printf("Error: source is not in range off RAM.\n");
                printf ("Usage:\n%s\n", cmdtp->usage);
                return 1;
            } 
            if (length == 0)
            {
                printf("Error: length is zero.\n");
                printf ("Usage:\n%s\n", cmdtp->usage);
                return 1;
            } 
       
            /* Download tCode */
            do_init_tcode_handshake(src,length);
        }

        if (strcmp(argv[i], "pcode") == 0)
        {
            /* Get source adddress */
            void *src = (void*)simple_strtoul(argv[i+1], NULL, ENV_BASE_HEX);

            /* Get length */
            unsigned int length = (unsigned int)simple_strtoul(argv[i+2], NULL, ENV_BASE_HEX);

            if (src == 0)
            {
                printf("Error: source is zero.\n");
                printf ("Usage:\n%s\n", cmdtp->usage);
                return 1;
            }
            if (src < ((void *)0x40000000) || src > ((void *)0x80000000))
            {
                printf("Error: source is not in range off RAM.\n");
                printf ("Usage:\n%s\n", cmdtp->usage);
                return 1;
            } 
            if (length == 0)
            {
                printf("Error: length is zero.\n");
                printf ("Usage:\n%s\n", cmdtp->usage);
                return 1;
            } 

            /* Downloadig pCode */
            do_download_pcode(src,length);
        }

        if (strcmp(argv[i], "status") == 0)
        {
            /* Read Bad Block status */
            do_read_bad_blocks_status();
        }

        if (strcmp(argv[i], "erase") == 0)
        {
            /* Perform Full erase */
            do_full_erase();
        }

        if (strcmp(argv[i], "cid") == 0)
        {
            /* Perform Full erase */
            do_setcid();
        }
    }

#if defined(CONFIG_USE_HW_MUTEX)
    /* Release HW Mutes */
    hw_mutex_unlock(HW_MUTEX_EMMC);
#endif
    return 0;
}


U_BOOT_CMD(
    incomm, 10, 0, do_incomm_test,
    "incomm\t - InComm test.\n",
    "[fw|testmode|pcdoe|status|erase|cid]\n"
    "fw          - Display FW version.\n"
    "testmode    - download tCode and enter to Test Mode\n"
    "pcode       - Download pCode.\n"
    "status      - Read bad blocks status.\n"
    "erase  -    - Perform Full Erase.\n"
    "cid         - Program CID.\n"
    
);



