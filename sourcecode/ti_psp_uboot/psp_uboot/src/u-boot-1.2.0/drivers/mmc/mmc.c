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
 * Changed/modified portions - Copyright © 2011 - 2012 , Intel Corporation.   
 */ 

#include <config.h>
#include <common.h>
#include <command.h>
#include <mmc.h>
#include <part.h>
#include <malloc.h>
#include <linux/list.h>
#include <div64.h>

#if defined(CONFIG_USE_HW_MUTEX)
#include <puma6_hw_mutex.h>
#define NOT_LOCKED 0
#define LOCKED     1
#endif

/* Set block count limit because of 16 bit register limit on some hardware*/
#ifndef CONFIG_SYS_MMC_MAX_BLK_COUNT
#define CONFIG_SYS_MMC_MAX_BLK_COUNT 65535
#endif

//#define DEBUGF(fmt,args...) printf(fmt ,##args)
//#define CONFIG_MMC_TRACE 1

static struct list_head mmc_devices;
static int cur_dev_num = -1;

int __board_mmc_getcd(u8 *cd, struct mmc *mmc) {
	return -1;
}

int board_mmc_getcd(u8 *cd, struct mmc *mmc)__attribute__((weak,
	alias("__board_mmc_getcd")));

int mmc_send_cmd(struct mmc *mmc, struct mmc_cmd *cmd, struct mmc_data *data)
{
#ifdef CONFIG_MMC_TRACE
	int ret;
	int i;
	u8 *ptr;

	printf("CMD_SEND:%d\n", cmd->cmdidx);
	printf("\t\tARG\t\t\t 0x%08X\n", cmd->cmdarg);
	printf("\t\tFLAG\t\t\t %d\n", cmd->flags);
	ret = mmc->send_cmd(mmc, cmd, data);
	switch (cmd->resp_type) {
		case MMC_RSP_NONE:
			printf("\t\tMMC_RSP_NONE\n");
			break;
		case MMC_RSP_R1:
			printf("\t\tMMC_RSP_R1,5,6,7 \t 0x%08X \n",
				cmd->response[0]);
			break;
		case MMC_RSP_R1b:
			printf("\t\tMMC_RSP_R1b\t\t 0x%08X \n",
				cmd->response[0]);
			break;
		case MMC_RSP_R2:
			printf("\t\tMMC_RSP_R2\t\t 0x%08X \n",
				cmd->response[0]);
			printf("\t\t          \t\t 0x%08X \n",
				cmd->response[1]);
			printf("\t\t          \t\t 0x%08X \n",
				cmd->response[2]);
			printf("\t\t          \t\t 0x%08X \n",
				cmd->response[3]);
			printf("\n");
			printf("\t\t\t\t\tDUMPING DATA\n");
			for (i = 0; i < 4; i++) {
				int j;
				printf("\t\t\t\t\t%03d - ", i*4);
				ptr = &cmd->response[i];
				ptr += 3;
				for (j = 0; j < 4; j++)
					printf("%02X ", *ptr--);
				printf("\n");
			}
			break;
		case MMC_RSP_R3:
			printf("\t\tMMC_RSP_R3,4\t\t 0x%08X \n",
				cmd->response[0]);
			break;
		default:
			printf("\t\tERROR MMC rsp not supported\n");
			break;
	}
	return ret;
#else
	return mmc->send_cmd(mmc, cmd, data);
#endif
}

int mmc_send_status(struct mmc *mmc, int timeout)
{
	struct mmc_cmd cmd;
	int err;
#ifdef CONFIG_MMC_TRACE
	int status;
#endif

	cmd.cmdidx = MMC_CMD_SEND_STATUS;
	cmd.resp_type = MMC_RSP_R1;
	if (!mmc_host_is_spi(mmc))
		cmd.cmdarg = mmc->rca << 16;
	cmd.flags = 0;

	do {
        DEBUGF("[DEBUG] - mmc_send_status: Send Cmd %d (MMC_CMD_SEND_STATUS), timeout=%d \n",cmd.cmdidx,timeout);
		err = mmc_send_cmd(mmc, &cmd, NULL);
        DEBUGF("[DEBUG] - mmc_send_status: response = 0x%0.8X\n",cmd.response[0]);
		if (err)
			return err;
		else if (cmd.response[0] & MMC_STATUS_RDY_FOR_DATA)
			break;

		udelay(1000);

		if (cmd.response[0] & MMC_STATUS_MASK) {
			printf("Status Error: 0x%08X\n", cmd.response[0]);
			return COMM_ERR;
		}
	} while (timeout--);

#ifdef CONFIG_MMC_TRACE
	status = (cmd.response[0] & MMC_STATUS_CURR_STATE) >> 9;
	printf("CURR STATE:%d\n", status);
#endif
	if (!timeout) {
		printf("Timeout waiting card ready\n");
		return TIMEOUT;
	}

	return 0;
}

int mmc_set_blocklen(struct mmc *mmc, int len)
{
    return 0;
/*	struct mmc_cmd cmd;

    if ((mmc->high_capacity == 1) || (mmc->ddr_enable == 1 ))
    {
        return 0;
    }

	cmd.cmdidx = MMC_CMD_SET_BLOCKLEN;
	cmd.resp_type = MMC_RSP_R1;
	cmd.cmdarg = len;
	cmd.flags = 0;

    DEBUGF("[DEBUG] - mmc_set_blocklen: Send Cmd %d (MMC_CMD_SET_BLOCKLEN), \n",cmd.cmdidx);
	return mmc_send_cmd(mmc, &cmd, NULL);
*/
}

struct mmc *find_mmc_device(int dev_num)
{
	struct mmc *m;
	struct list_head *entry;

	list_for_each(entry, &mmc_devices) {
		m = list_entry(entry, struct mmc, link);

		if (m->block_dev.dev == dev_num)
			return m;
	}

	printf("MMC Device %d not found\n", dev_num);

	return NULL;
}

static ulong mmc_erase_t(struct mmc *mmc, ulong start, lbaint_t blkcnt)
{
	struct mmc_cmd cmd;
	ulong end;
	int err, start_cmd, end_cmd;

	if (mmc->high_capacity)
		end = start + blkcnt - 1;
	else {
		end = (start + blkcnt - 1) * mmc->write_bl_len;
		start *= mmc->write_bl_len;
	}

	if (IS_SD(mmc)) {
		start_cmd = SD_CMD_ERASE_WR_BLK_START;
		end_cmd = SD_CMD_ERASE_WR_BLK_END;
	} else {
		start_cmd = MMC_CMD_ERASE_GROUP_START;
		end_cmd = MMC_CMD_ERASE_GROUP_END;
	}

	cmd.cmdidx = start_cmd;
	cmd.cmdarg = start;
	cmd.resp_type = MMC_RSP_R1;
	cmd.flags = 0;

    DEBUGF("[DEBUG] - mmc_erase_t: Send Cmd %d (MMC_CMD_ERASE_GROUP_START), \n",cmd.cmdidx);
	err = mmc_send_cmd(mmc, &cmd, NULL);
	if (err)
		goto err_out;

	cmd.cmdidx = end_cmd;
	cmd.cmdarg = end;

    DEBUGF("[DEBUG] - mmc_erase_t: Send Cmd %d (MMC_CMD_ERASE_GROUP_END), \n",cmd.cmdidx);
	err = mmc_send_cmd(mmc, &cmd, NULL);
	if (err)
		goto err_out;

	cmd.cmdidx = MMC_CMD_ERASE;
	cmd.cmdarg = SECURE_ERASE;
	cmd.resp_type = MMC_RSP_R1b;

    DEBUGF("[DEBUG] - mmc_erase_t: Send Cmd %d (MMC_CMD_ERASE), \n",cmd.cmdidx);
	err = mmc_send_cmd(mmc, &cmd, NULL);
	if (err)
		goto err_out;

	return 0;

err_out:
	puts("mmc erase failed\n");
	return err;
}

static unsigned long
mmc_berase(int dev_num, unsigned long start, lbaint_t blkcnt)
{
	int err = 0;
	struct mmc *mmc = find_mmc_device(dev_num);
	lbaint_t blk = 0, blk_r = 0;
#if defined(CONFIG_USE_HW_MUTEX)
    int is_hw_locked = 0;
#endif

	if (!mmc)
        return -1;
	
	if ((start % mmc->erase_grp_size) || (blkcnt % mmc->erase_grp_size))
    {
		printf("\n\nCaution! Your devices Erase group is 0x%x\n"
            "The erase range would be change to 0x%lx~0x%lx\n\n",
               mmc->erase_grp_size, start & ~(mmc->erase_grp_size - 1),
               ((start + blkcnt + mmc->erase_grp_size)
               & ~(mmc->erase_grp_size - 1)) - 1);
        printf("Error: erase range is not aligned with Erase Group (%d blocks)\n",mmc->erase_grp_size);
        return 0;
    }

#if defined(CONFIG_USE_HW_MUTEX)
    /* Lock the HW Mutex */
    if (hw_mutex_is_locked(HW_MUTEX_EMMC) == NOT_LOCKED)
    {
        if (hw_mutex_lock(HW_MUTEX_EMMC) == 0)
        {
            printf("Error: mmc_berase - failed to lock HW Mutex\n");
            return 0;
        }
        DEBUGF("[DEBUG] - mmc_berase: hw mutex is locked \n");
        is_hw_locked = 1;            /* Hw mutex lock by this function */
    }
    else
    {
        DEBUGF("[DEBUG] - mmc_berase: hw mutex is already locked - skip locking\n");
    }
#endif

	while (blk < blkcnt) {
		blk_r = ((blkcnt - blk) > mmc->erase_grp_size) ?
			mmc->erase_grp_size : (blkcnt - blk);
		err = mmc_erase_t(mmc, start + blk, blk_r);
		if (err)
			break;

		blk += blk_r;
	}

#if defined(CONFIG_USE_HW_MUTEX)
    /* Release HW Mutes */
    if (is_hw_locked == 1) /* if lock by this funection then release, otherwise keep it locked */
    {
        hw_mutex_unlock(HW_MUTEX_EMMC);
    }
#endif

	return blk;
}


static ulong
mmc_write_blocks(struct mmc *mmc, ulong start, lbaint_t blkcnt, const void*src)
{
	struct mmc_cmd cmd;
	struct mmc_data data;
	int timeout = 1000;

#ifndef CONFIG_MMC_SKIP_INIT 
/* In case of Skip Boot is enable, then we do not know the Flash size,
   So, we skip this boundary check */
	if ((start + blkcnt) > mmc->block_dev.lba) {
		printf("MMC: block number 0x%lx exceeds max(0x%lx)\n",
			start + blkcnt, mmc->block_dev.lba);
		return 0;
	}
#endif

	if (blkcnt > 1)
		cmd.cmdidx = MMC_CMD_WRITE_MULTIPLE_BLOCK;
	else
		cmd.cmdidx = MMC_CMD_WRITE_SINGLE_BLOCK;

	if (mmc->high_capacity)
		cmd.cmdarg = start;
	else
		cmd.cmdarg = start * mmc->write_bl_len;

	cmd.resp_type = MMC_RSP_R1;
	cmd.flags = 0;

	data.src = src;
	data.blocks = blkcnt;
	data.blocksize = mmc->write_bl_len;
	data.flags = MMC_DATA_WRITE;

    DEBUGF("[DEBUG] - mmc_write_blocks: Send Cmd %d (MMC_CMD_WRITE_SINGLE/MULTIPLE_BLOCK), \n",cmd.cmdidx);
	if (mmc_send_cmd(mmc, &cmd, &data)) {
		printf("mmc write failed\n");
		return 0;
	}
    DEBUGF("[DEBUG] - mmc_write_blocks: response 0x%X,0x%X,0x%X,0x%X \n",cmd.response[0],cmd.response[1],cmd.response[2],cmd.response[3]);

	/* SPI multiblock writes terminate using a special
	 * token, not a STOP_TRANSMISSION request.
	 */
	if (!mmc_host_is_spi(mmc) && blkcnt > 1) {
		cmd.cmdidx = MMC_CMD_STOP_TRANSMISSION;
		cmd.cmdarg = 0;
		cmd.resp_type = MMC_RSP_R1b;
		cmd.flags = 0;
		if (mmc_send_cmd(mmc, &cmd, NULL)) {
			printf("mmc fail to send stop cmd\n");
			return 0;
		}

		/* Waiting for the ready status */
		mmc_send_status(mmc, timeout);
	}

	return blkcnt;
}

static ulong
mmc_bwrite(int dev_num, ulong start, lbaint_t blkcnt, const void*src)
{
	lbaint_t cur, blocks_todo = blkcnt;
    ulong ret = 0;
#if defined(CONFIG_USE_HW_MUTEX)
    int is_hw_locked = 0;
#endif

	struct mmc *mmc = find_mmc_device(dev_num);
	
	DEBUGF("[DEBUG] - mmc_bwrite: start\n");
    // printf("*** EMMC WRITE from [0x%0.8X] to [0x%0.8X], length:%d ***\n",start*512,(start+((ulong)blkcnt))*512,(ulong)blkcnt*512);
	
	if (!mmc)
		return 0;

#if defined(CONFIG_USE_HW_MUTEX)
    /* Lock the HW Mutex */
    if (hw_mutex_is_locked(HW_MUTEX_EMMC) == NOT_LOCKED)
    {
        if (hw_mutex_lock(HW_MUTEX_EMMC) == 0)
        {
            printf("Error: mmc_bwrite - failed to lock HW Mutex\n");
            return 0;
        }
        DEBUGF("[DEBUG] - mmc_bwrite: hw mutex is locked \n");
        is_hw_locked = 1;            /* Hw mutex lock by this function */
    }
    else
    {
        DEBUGF("[DEBUG] - mmc_bwrite: hw mutex is already locked - skip locking\n");
    }
#endif
	if (mmc_set_blocklen(mmc, mmc->write_bl_len))
		goto bwrite_done;

	do {
		cur = (blocks_todo > mmc->b_max) ?  mmc->b_max : blocks_todo;
		if(mmc_write_blocks(mmc, start, cur, src) != cur)
			goto bwrite_done;
		blocks_todo -= cur;
		start += cur;
		src += cur * mmc->write_bl_len;
	} while (blocks_todo > 0);

    ret = blkcnt;

bwrite_done:
#if defined(CONFIG_USE_HW_MUTEX)
    /* Release HW Mutes */
    if (is_hw_locked == 1) /* if lock by this funection then release, otherwise keep it locked */
    {
        hw_mutex_unlock(HW_MUTEX_EMMC);
        DEBUGF("[DEBUG] - mmc_bwrite: hw mutex unlocked\n");
    }
#endif

	return ret;
}

int mmc_read_blocks(struct mmc *mmc, void *dst, ulong start, lbaint_t blkcnt)
{
	struct mmc_cmd cmd;
	struct mmc_data data;
	int timeout = 1000;

	if (blkcnt > 1)
		cmd.cmdidx = MMC_CMD_READ_MULTIPLE_BLOCK;
	else
		cmd.cmdidx = MMC_CMD_READ_SINGLE_BLOCK;

	if (mmc->high_capacity)
		cmd.cmdarg = start;
	else
		cmd.cmdarg = start * mmc->read_bl_len;

	cmd.resp_type = MMC_RSP_R1;
	cmd.flags = 0;

	data.dest = dst;
	data.blocks = blkcnt;
	data.blocksize = mmc->read_bl_len;
	data.flags = MMC_DATA_READ;

    DEBUGF("[DEBUG] - Send Cmd %d (%s), count=%d\n",cmd.cmdidx,(blkcnt > 1?"MMC_CMD_READ_MULTIPLE_BLOCK":"MMC_CMD_READ_SINGLE_BLOCK"),blkcnt);
	if (mmc_send_cmd(mmc, &cmd, &data))
		return 0;

	if (blkcnt > 1) {
		cmd.cmdidx = MMC_CMD_STOP_TRANSMISSION;
		cmd.cmdarg = 0;
		cmd.resp_type = MMC_RSP_R1b;
		cmd.flags = 0;
		if (mmc_send_cmd(mmc, &cmd, NULL)) {
			printf("mmc fail to send stop cmd\n");
			return 0;
		}

		/* Waiting for the ready status */
		mmc_send_status(mmc, timeout);
	}

	return blkcnt;
}

static ulong mmc_bread(int dev_num, ulong start, lbaint_t blkcnt, void *dst)
{
	lbaint_t cur, blocks_todo = blkcnt;
    struct mmc *mmc;
    ulong ret = 0;
#if defined(CONFIG_USE_HW_MUTEX)
    int is_hw_locked = 0;
#endif

    DEBUGF("[DEBUG] - mmc_bread: start\n");
    // printf("*** EMMC READ from [0x%0.8X] to [0x%0.8X], length:%d ***\n",start*512,(start+((ulong)blkcnt))*512,(ulong)blkcnt*512);

	if (blkcnt == 0)
		return 0;

	mmc = find_mmc_device(dev_num);
	if (!mmc)
		return 0;

#if defined(CONFIG_USE_HW_MUTEX)
    /* Lock the HW Mutex */
    if (hw_mutex_is_locked(HW_MUTEX_EMMC) == NOT_LOCKED)
    {
        if (hw_mutex_lock(HW_MUTEX_EMMC) == 0)
        {
            printf("Error: mmc_bread - failed to lock HW Mutex\n");
            return 0;
        }
        DEBUGF("[DEBUG] - mmc_bread: hw mutex is locked \n");
        is_hw_locked = 1;            /* Hw mutex lock by this function */
    }
    else
    {
        DEBUGF("[DEBUG] - mmc_bread: hw mutex is already locked - skip locking\n");
    }
#endif

#ifndef CONFIG_MMC_SKIP_INIT
/* In case of Skip Boot is enable, then we do not know the Flash size,
   So, we skip this boundary check */
    if ((start + blkcnt) > mmc->block_dev.lba) {
		printf("MMC: block number 0x%lx exceeds max(0x%lx)\n",
			start + blkcnt, mmc->block_dev.lba);
		goto bread_done;
	}
#endif
	if (mmc_set_blocklen(mmc, mmc->read_bl_len))
		goto bread_done;

	do {
		cur = (blocks_todo > mmc->b_max) ?  mmc->b_max : blocks_todo;
		if(mmc_read_blocks(mmc, dst, start, cur) != cur)
			goto bread_done;
		blocks_todo -= cur;
		start += cur;
		dst += cur * mmc->read_bl_len;
	} while (blocks_todo > 0);

	ret = blkcnt;
bread_done:
#if defined(CONFIG_USE_HW_MUTEX)
    /* Release HW Mutes */
    if (is_hw_locked == 1) /* if lock by this funection then release, otherwise keep it locked */
    {
        hw_mutex_unlock(HW_MUTEX_EMMC);
        DEBUGF("[DEBUG] - mmc_bread: hw mutex unlocked\n");
    }
#endif
	return ret;
}

uint mmc_read(int dev_num, uint src, uchar *dst, int size)
{
    uint blk_start, blk_cnt, n;
    struct mmc *mmc;

    mmc = find_mmc_device(dev_num);
	if (!mmc)
		return 0;

   if ((src & (mmc->read_bl_len-1)) != 0 )
       printf("Warning: mmc_read: address [0x%X] is not align to %d \n", src,mmc->read_bl_len);

   //if ((size & (mmc->read_bl_len-1)) != 0 )
   //    printf("Warning: mmc_read: size [0x%X] is not align to %d \n", size,mmc->read_bl_len);

	blk_start = ALIGN(src,  mmc->read_bl_len) / mmc->read_bl_len;
	blk_cnt   = ALIGN(size, mmc->read_bl_len) / mmc->read_bl_len;

    n = mmc_bread(dev_num, (ulong)blk_start,(lbaint_t)blk_cnt, (void *)dst);

	return (n == blk_cnt) ? 0 : -1;
}

int mmc_go_idle(struct mmc* mmc)
{
	struct mmc_cmd cmd;
	int err;

	udelay(1000);

	cmd.cmdidx = MMC_CMD_GO_IDLE_STATE;
	cmd.cmdarg = 0;
	cmd.resp_type = MMC_RSP_NONE;
	cmd.flags = 0;

    DEBUGF("[DEBUG] - mmc_go_idle: Send Cmd %d (MMC_CMD_GO_IDLE_STATE), \n",cmd.cmdidx);
	err = mmc_send_cmd(mmc, &cmd, NULL);
	if (err)
		return err;

	udelay(2000);

	return 0;
}

int
sd_send_op_cond(struct mmc *mmc)
{
	int timeout = 1000;
	int err;
	struct mmc_cmd cmd;

	do {
		cmd.cmdidx = MMC_CMD_APP_CMD;
		cmd.resp_type = MMC_RSP_R1;
		cmd.cmdarg = 0;
		cmd.flags = 0;

		err = mmc_send_cmd(mmc, &cmd, NULL);
		if (err)
			return err;

		cmd.cmdidx = SD_CMD_APP_SEND_OP_COND;
		cmd.resp_type = MMC_RSP_R3;

		/*
		 * Most cards do not answer if some reserved bits
		 * in the ocr are set. However, Some controller
		 * can set bit 7 (reserved for low voltages), but
		 * how to manage low voltages SD card is not yet
		 * specified.
		 */
		cmd.cmdarg = mmc_host_is_spi(mmc) ? 0 :
			(mmc->voltages & 0xff8000);

		if (mmc->version == SD_VERSION_2)
			cmd.cmdarg |= OCR_HCS;

		err = mmc_send_cmd(mmc, &cmd, NULL);
		if (err)
			return err;

		udelay(1000);
	} while ((!(cmd.response[0] & OCR_BUSY)) && timeout--);

	if (timeout <= 0)
		return UNUSABLE_ERR;

	if (mmc->version != SD_VERSION_2)
		mmc->version = SD_VERSION_1_0;

	if (mmc_host_is_spi(mmc)) { /* read OCR for spi */
		cmd.cmdidx = MMC_CMD_SPI_READ_OCR;
		cmd.resp_type = MMC_RSP_R3;
		cmd.cmdarg = 0;
		cmd.flags = 0;

		err = mmc_send_cmd(mmc, &cmd, NULL);
		if (err)
			return err;
	}

	mmc->ocr = cmd.response[0];

	mmc->high_capacity = ((mmc->ocr & OCR_HCS) == OCR_HCS);
	mmc->rca = 0;

	return 0;
}

int mmc_send_op_cond(struct mmc *mmc)
{
	int timeout = 10000;
	struct mmc_cmd cmd;
	int err;

	/* Some cards seem to need this */
	mmc_go_idle(mmc);

 	/* Asking to the card its capabilities */
 	cmd.cmdidx = MMC_CMD_SEND_OP_COND;
 	cmd.resp_type = MMC_RSP_R3;
 	cmd.cmdarg = 0;
 	cmd.flags = 0;

 	err = mmc_send_cmd(mmc, &cmd, NULL);
 	if (err)
 		return err;

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

		err = mmc_send_cmd(mmc, &cmd, NULL);
		if (err)
			return err;

		udelay(1000);
	} while (!(cmd.response[0] & OCR_BUSY) && timeout--);

	if (timeout <= 0)
		return UNUSABLE_ERR;

	if (mmc_host_is_spi(mmc)) { /* read OCR for spi */
		cmd.cmdidx = MMC_CMD_SPI_READ_OCR;
		cmd.resp_type = MMC_RSP_R3;
		cmd.cmdarg = 0;
		cmd.flags = 0;

		err = mmc_send_cmd(mmc, &cmd, NULL);
		if (err)
			return err;
	}

	mmc->version = MMC_VERSION_UNKNOWN;
	mmc->ocr = cmd.response[0];

	mmc->high_capacity = ((mmc->ocr & OCR_HCS) == OCR_HCS);
	mmc->rca = 1;

	return 0;
}


int mmc_send_ext_csd(struct mmc *mmc, char *ext_csd)
{
	struct mmc_cmd cmd;
	struct mmc_data data;
	int err;

	/* Get the Card Status Register */
	cmd.cmdidx = MMC_CMD_SEND_EXT_CSD;
	cmd.resp_type = MMC_RSP_R1;
	cmd.cmdarg = 0;
	cmd.flags = 0;

	data.dest = ext_csd;
	data.blocks = 1;
	data.blocksize = 512;
	data.flags = MMC_DATA_READ;

    DEBUGF("[DEBUG] - mmc_send_ext_csd: Send Cmd %d (MMC_CMD_SEND_EXT_CSD), \n",cmd.cmdidx);
	err = mmc_send_cmd(mmc, &cmd, &data);
	if (err)
		return err;

   /* update ext_csd register */
    memcpy(mmc->ext_csd,ext_csd,512);
    return 0;
}


int mmc_switch(struct mmc *mmc, u8 set, u8 index, u8 value)
{
	struct mmc_cmd cmd;
	int timeout = 1000;
	int ret;

    DEBUGF("[DEBUG] - mmc_switch: index=%d  value=%d\n",index,value);

	cmd.cmdidx = MMC_CMD_SWITCH;
	cmd.resp_type = MMC_RSP_R1b;
	cmd.cmdarg = (MMC_SWITCH_MODE_WRITE_BYTE << 24) |
				 (index << 16) |
				 (value << 8);
	cmd.flags = 0;

    DEBUGF("[DEBUG] - mmc_switch: Send Cmd %d (MMC_CMD_SWITCH), \n",cmd.cmdidx);
	ret = mmc_send_cmd(mmc, &cmd, NULL);

	/* Waiting for the ready status */
	mmc_send_status(mmc, timeout);

	return ret;

}

int mmc_change_freq(struct mmc *mmc)
{
	ALLOC_CACHE_ALIGN_BUFFER(char, ext_csd, 512);
	char cardtype;
	int err;

	mmc->card_caps = 0;

	if (mmc_host_is_spi(mmc))
		return 0;

    /* Only version 4 supports high-speed */
	if (mmc->version < MMC_VERSION_4)
		return 0;

	err = mmc_send_ext_csd(mmc, ext_csd);
	if (err)
		return err;

	cardtype = ext_csd[EXT_CSD_CARD_TYPE] & 0xf;

	err = mmc_switch(mmc, EXT_CSD_CMD_SET_NORMAL, EXT_CSD_HS_TIMING, 1);

	if (err)
		return err;

	/* Now check to see that it worked */
	err = mmc_send_ext_csd(mmc, ext_csd);
	if (err)
		return err;

	/* No high-speed support */
	if (!ext_csd[EXT_CSD_HS_TIMING])
		return 0;

    cardtype = ext_csd[EXT_CSD_CARD_TYPE] & 0xf;

    /* High Speed is set, there are four types: 52MHz DDR 1.8v/3v 52MHz DDR 1.2.v , 52MHz and 26MHz  */
    if ((cardtype & EXT_CSD_CARD_TYPE_26) != 0)
    {
        mmc->card_caps |= MMC_MODE_HS;
        DEBUGF("[DEBUG] - mmc_startup: Card type: Support High-Speed MultiMediaCard @ 26MHz \n");
    }
    if ((cardtype & EXT_CSD_CARD_TYPE_52) != 0)
    {
        mmc->card_caps |= (MMC_MODE_HS_52MHz | MMC_MODE_HS);
        DEBUGF("[DEBUG] - mmc_startup: Card type: Support High-Speed MultiMediaCard @ 52MHz \n");
    }
    if ((cardtype & EXT_CSD_CARD_TYPE_52_DDR_1_8) != 0)
    {
        mmc->card_caps |= (MMC_MODE_DDR | MMC_MODE_HS_52MHz | MMC_MODE_HS);
        DEBUGF("[DEBUG] - mmc_startup: Card type: Support High-Speed Dual Data Rate MultimediaCard @ 52MHz - 1.8V or 3V \n");
    }
    if ((cardtype & EXT_CSD_CARD_TYPE_52_DDR_1_2) != 0)
    {
        mmc->card_caps |= MMC_MODE_DDR | MMC_MODE_HS_52MHz |MMC_MODE_HS;
        DEBUGF("[DEBUG] - mmc_startup: Card type: Support High-Speed Dual Data Rate MultimediaCard @ 52MHz - 1.2V \n");
    }
	return 0;
}

int mmc_switch_part(int dev_num, unsigned int part_num)
{
	struct mmc *mmc = find_mmc_device(dev_num);

	if (!mmc)
		return -1;

	return mmc_switch(mmc, EXT_CSD_CMD_SET_NORMAL, EXT_CSD_PART_CONF,
			  (mmc->part_config & ~PART_ACCESS_MASK)
			  | (part_num & PART_ACCESS_MASK));
}

int sd_switch(struct mmc *mmc, int mode, int group, u8 value, u8 *resp)
{
	struct mmc_cmd cmd;
	struct mmc_data data;

	/* Switch the frequency */
	cmd.cmdidx = SD_CMD_SWITCH_FUNC;
	cmd.resp_type = MMC_RSP_R1;
	cmd.cmdarg = (mode << 31) | 0xffffff;
	cmd.cmdarg &= ~(0xf << (group * 4));
	cmd.cmdarg |= value << (group * 4);
	cmd.flags = 0;

	data.dest = (char *)resp;
	data.blocksize = 64;
	data.blocks = 1;
	data.flags = MMC_DATA_READ;

	return mmc_send_cmd(mmc, &cmd, &data);
}


int sd_change_freq(struct mmc *mmc)
{
	int err;
	struct mmc_cmd cmd;
	ALLOC_CACHE_ALIGN_BUFFER(uint, scr, 2);
	ALLOC_CACHE_ALIGN_BUFFER(uint, switch_status, 16);
	struct mmc_data data;
	int timeout;

	mmc->card_caps = 0;

	if (mmc_host_is_spi(mmc))
		return 0;

	/* Read the SCR to find out if this card supports higher speeds */
	cmd.cmdidx = MMC_CMD_APP_CMD;
	cmd.resp_type = MMC_RSP_R1;
	cmd.cmdarg = mmc->rca << 16;
	cmd.flags = 0;

    DEBUGF("[DEBUG] - sd_change_freq: Send Cmd %d (MMC_CMD_APP_CMD), \n",cmd.cmdidx);
	err = mmc_send_cmd(mmc, &cmd, NULL);
	if (err)
		return err;

	cmd.cmdidx = SD_CMD_APP_SEND_SCR;
	cmd.resp_type = MMC_RSP_R1;
	cmd.cmdarg = 0;
	cmd.flags = 0;

	timeout = 3;

retry_scr:
	data.dest = (char *)scr;
	data.blocksize = 8;
	data.blocks = 1;
	data.flags = MMC_DATA_READ;

    DEBUGF("[DEBUG] - sd_change_freq: Send Cmd %d (SD_CMD_APP_SEND_SCR), \n",cmd.cmdidx);
	err = mmc_send_cmd(mmc, &cmd, &data);
	if (err) {
		if (timeout--)
			goto retry_scr;

		return err;
	}

	mmc->scr[0] = __be32_to_cpu(scr[0]);
	mmc->scr[1] = __be32_to_cpu(scr[1]);

	switch ((mmc->scr[0] >> 24) & 0xf) {
		case 0:
			mmc->version = SD_VERSION_1_0;
			break;
		case 1:
			mmc->version = SD_VERSION_1_10;
			break;
		case 2:
			mmc->version = SD_VERSION_2;
			break;
		default:
			mmc->version = SD_VERSION_1_0;
			break;
	}

	if (mmc->scr[0] & SD_DATA_4BIT)
		mmc->card_caps |= MMC_MODE_4BIT;

	/* Version 1.0 doesn't support switching */
	if (mmc->version == SD_VERSION_1_0)
		return 0;

	timeout = 4;
	while (timeout--) {
		err = sd_switch(mmc, SD_SWITCH_CHECK, 0, 1,
				(u8 *)switch_status);

		if (err)
			return err;

		/* The high-speed function is busy.  Try again */
		if (!(__be32_to_cpu(switch_status[7]) & SD_HIGHSPEED_BUSY))
			break;
	}

	/* If high-speed isn't supported, we return */
	if (!(__be32_to_cpu(switch_status[3]) & SD_HIGHSPEED_SUPPORTED))
		return 0;

	err = sd_switch(mmc, SD_SWITCH_SWITCH, 0, 1, (u8 *)switch_status);

	if (err)
		return err;

	if ((__be32_to_cpu(switch_status[4]) & 0x0f000000) == 0x01000000)
		mmc->card_caps |= MMC_MODE_HS;

	return 0;

}

/* frequency bases */
/* divided by 10 to be nice to platforms without floating point */
static const int fbase[] = {
	10000,
	100000,
	1000000,
	10000000,
};

/* Multiplier values for TRAN_SPEED.  Multiplied by 10 to be nice
 * to platforms without floating point.
 */
static const int multipliers[] = {
	0,	/* reserved */
	10,
	12,
	13,
	15,
	20,
	26,
	30,
	35,
	40,
	45,
	52,
	55,
	60,
	70,
	80,
};

void mmc_set_ios(struct mmc *mmc)
{
	mmc->set_ios(mmc);
}

void mmc_set_clock(struct mmc *mmc, uint clock)
{
	if (clock > mmc->f_max)
		clock = mmc->f_max;

	if (clock < mmc->f_min)
		clock = mmc->f_min;

	mmc->clock = clock;

	mmc_set_ios(mmc);
}

void mmc_set_bus_width(struct mmc *mmc, uint width)
{
	mmc->bus_width = width;

	mmc_set_ios(mmc);
}

void mmc_set_ddr(struct mmc *mmc, uint ddr)
{
	mmc->ddr_enable = ddr;

	mmc_set_ios(mmc);
}


int mmc_partial_startup(struct mmc *mmc)
{
	int err;
	uint mult, freq;
	uint cmult = 0; 
    u64 csize = 0;
    uint capacity = 0;
	struct mmc_cmd cmd;
	ALLOC_CACHE_ALIGN_BUFFER(char, ext_csd, 512);
	int timeout = 1000;
    char cardtype;

    /* Waiting for the ready status */
	err = mmc_send_status(mmc, timeout);
    if (err)
		return err;

    /* Send CMD7 return to Standby Mode */
    /* Select the card, and put it into Standby mode */
    cmd.cmdidx = MMC_CMD_SELECT_CARD;
	cmd.resp_type = MMC_RSP_NONE;
	cmd.cmdarg = 0; // to put back to Standby, arg must not be rca */ 
	cmd.flags = 0;
    DEBUGF("[DEBUG] - mmc_partial_startup: Send Cmd %d (MMC_CMD_SELECT_CARD) (arg=%p)\n",cmd.cmdidx,cmd.cmdarg);
	err = mmc_send_cmd(mmc, &cmd, NULL);
	if (err)
		return err;

    /* Read CID */
	cmd.cmdidx = MMC_CMD_SEND_CID; /* cmd not supported in spi */
	cmd.resp_type = MMC_RSP_R2;
	cmd.cmdarg = mmc->rca << 16;;
	cmd.flags = 0;

    DEBUGF("[DEBUG] - mmc_startup: Send Cmd %d (MMC_CMD_SEND_CID), \n",cmd.cmdidx);
	err = mmc_send_cmd(mmc, &cmd, NULL);
	if (err)
    {
        printf("Error: MMC_CMD_SEND_CID failed\n" );
		return err;
    }

	memcpy(mmc->cid, cmd.response, 16);



	/* Get the Card-Specific Data */
    cmd.cmdidx = MMC_CMD_SEND_CSD;
	cmd.resp_type = MMC_RSP_R2;
	cmd.cmdarg = mmc->rca << 16;
	cmd.flags = 0;

    DEBUGF("[DEBUG] - mmc_partial_startup: Send Cmd %d (MMC_CMD_SEND_CSD), \n",cmd.cmdidx);
	err = mmc_send_cmd(mmc, &cmd, NULL);
    if (err)
		return err;

	/* Waiting for the ready status */
	err = mmc_send_status(mmc, timeout);
	if (err)
		return err;

	mmc->csd[0] = cmd.response[0];
	mmc->csd[1] = cmd.response[1];
	mmc->csd[2] = cmd.response[2];
	mmc->csd[3] = cmd.response[3];

    DEBUGF("[DEBUG] - mmc_partial_startup: mmc->csd[0] = 0x%0.8X \n",mmc->csd[0]);
    DEBUGF("[DEBUG] - mmc_partial_startup: mmc->csd[1] = 0x%0.8X \n",mmc->csd[1]);
    DEBUGF("[DEBUG] - mmc_partial_startup: mmc->csd[2] = 0x%0.8X \n",mmc->csd[2]);
    DEBUGF("[DEBUG] - mmc_partial_startup: mmc->csd[3] = 0x%0.8X \n",mmc->csd[3]);

	if (mmc->version == MMC_VERSION_UNKNOWN) {
		int version = (cmd.response[0] >> 26) & 0xf;

		switch (version) {
			case 0:
				mmc->version = MMC_VERSION_1_2;
				break;
			case 1:
				mmc->version = MMC_VERSION_1_4;
				break;
			case 2:
				mmc->version = MMC_VERSION_2_2;
				break;
			case 3:
				mmc->version = MMC_VERSION_3;
				break;
			case 4:
				mmc->version = MMC_VERSION_4;
				break;
			default:
				mmc->version = MMC_VERSION_1_2;
				break;
		}
	}

    DEBUGF("[DEBUG] - mmc_partial_startup: mmc version  = 0x%0.8X \n",mmc->version );

	/* divide frequency by 10, since the mults are 10x bigger */
	freq = fbase[(cmd.response[0] & 0x7)];
	mult = multipliers[((cmd.response[0] >> 3) & 0xf)];

    /* Non 'high speed' transaction speed */ 
	mmc->tran_speed = freq * mult;
    DEBUGF("[DEBUG] - mmc_partial_startup: Max clock frequency when not in high speed mode = %d Htz\n",mmc->tran_speed);

	mmc->read_bl_len = 1 << ((cmd.response[1] >> 16) & 0xf);
    DEBUGF("[DEBUG] - mmc_partial_startup: Max. read data block length = %d bytes\n",mmc->read_bl_len);

    mmc->write_bl_len = 1 << ((cmd.response[3] >> 22) & 0xf);
    DEBUGF("[DEBUG] - mmc_partial_startup: Max. write data block length = %d bytes\n",mmc->write_bl_len);


    if (mmc->high_capacity) 
    {
		csize = ((mmc->csd[1] & 0x3f) << 16) | ((mmc->csd[2] & 0xffff0000) >> 16);
		cmult = 8;
	} 
    else 
    {
        csize = ((mmc->csd[1] & 0x3ff) << 2) | ((mmc->csd[2] & 0xc0000000) >> 30);
		cmult = (mmc->csd[2] & 0x00038000) >> 15;
	}
	mmc->capacity = (csize + 1) << (cmult + 2);
	mmc->capacity *= mmc->read_bl_len;
    DEBUGF("[DEBUG] - mmc_partial_startup: CSD Card Capacity = %qu, bytes \n",mmc->capacity);


	if (mmc->read_bl_len > 512)
		mmc->read_bl_len = 512;

	if (mmc->write_bl_len > 512)
		mmc->write_bl_len = 512;

    

	/* Select the card, and put it into Transfer Mode */
    cmd.cmdidx = MMC_CMD_SELECT_CARD;
	cmd.resp_type = MMC_RSP_R1;
	cmd.cmdarg = mmc->rca << 16;
	cmd.flags = 0;
    DEBUGF("[DEBUG] - mmc_partial_startup: Send Cmd %d (MMC_CMD_SELECT_CARD), \n",cmd.cmdidx);
	err = mmc_send_cmd(mmc, &cmd, NULL);
	if (err)
		return err;

    /*
	 * For SD, its erase group is always one sector
	 */
	mmc->erase_grp_size = 1;
	mmc->part_config = MMCPART_NOAVAILABLE;

    if (mmc->version >= MMC_VERSION_4) 
    {
        DEBUGF("[DEBUG] - mmc_partial_startup: Read ext_csd register, \n");

		/* check ext_csd version and capacity */
		err = mmc_send_ext_csd(mmc, ext_csd);
        if (!err & (ext_csd[EXT_CSD_REV] >= 2)) {
			/*
			 * According to the JEDEC Standard, the value of
			 * ext_csd's capacity is valid if the value is more
			 * than 2GB
			 */
			capacity = (ext_csd[EXT_CSD_SEC_CNT] << 0) | (ext_csd[EXT_CSD_SEC_CNT + 1] << 8) | (ext_csd[EXT_CSD_SEC_CNT + 2] << 16) | (ext_csd[EXT_CSD_SEC_CNT + 3] << 24);
			capacity *= 512;
			if ((capacity >> 20) > 2 * 1024)
            {
				mmc->capacity = capacity;
                DEBUGF("[DEBUG] - mmc_partial_startup: EXT_CSD Card Capacity = %qu bytes  (High-Capacity = %s)\n",mmc->capacity,(mmc->high_capacity==1?"True":"False"));
            }
		}

		/*
		 * Check whether GROUP_DEF is set, if yes, read out
		 * group size from ext_csd directly, or calculate
		 * the group size from the csd value.
		 */
		if (ext_csd[EXT_CSD_ERASE_GROUP_DEF])
			mmc->erase_grp_size = ext_csd[EXT_CSD_HC_ERASE_GRP_SIZE] * 512 * 1024;
		else {
			int erase_gsz, erase_gmul;
			erase_gsz = (mmc->csd[2] & 0x00007c00) >> 10;
			erase_gmul = (mmc->csd[2] & 0x000003e0) >> 5;
			mmc->erase_grp_size = (erase_gsz + 1)
				* (erase_gmul + 1);
		}

		/* store the partition info of emmc */
		if (ext_csd[EXT_CSD_PARTITIONING_SUPPORT] & PART_SUPPORT)
			mmc->part_config = ext_csd[EXT_CSD_PART_CONF];

        cardtype = ext_csd[EXT_CSD_CARD_TYPE] & 0xf;

        DEBUGF("[DEBUG] - mmc_partial_startup: cardtype = %d \n",cardtype);

        if ((cardtype & EXT_CSD_CARD_TYPE_26) != 0)
        {
            mmc->card_caps |= MMC_MODE_HS;
            DEBUGF("[DEBUG] - mmc_partial_startup: Card type: Support High-Speed MultiMediaCard @ 26MHz \n");
        }
        if ((cardtype & EXT_CSD_CARD_TYPE_52) != 0)
        {
            mmc->card_caps |= (MMC_MODE_HS_52MHz | MMC_MODE_HS);
            DEBUGF("[DEBUG] - mmc_partial_startup: Card type: Support High-Speed MultiMediaCard @ 52MHz \n");
        }
        if ((cardtype & EXT_CSD_CARD_TYPE_52_DDR_1_8) != 0)
        {
            mmc->card_caps |= (MMC_MODE_DDR | MMC_MODE_HS_52MHz | MMC_MODE_HS);
            DEBUGF("[DEBUG] - mmc_partial_startup: Card type: Support High-Speed Dual Data Rate MultimediaCard @ 52MHz - 1.8V or 3V \n");
        }
        if ((cardtype & EXT_CSD_CARD_TYPE_52_DDR_1_2) != 0)
        {
            mmc->card_caps |= (MMC_MODE_DDR | MMC_MODE_HS_52MHz | MMC_MODE_HS);
            DEBUGF("[DEBUG] - mmc_partial_startup: Card type: Support High-Speed Dual Data Rate MultimediaCard @ 52MHz - 1.2V \n");
        }
   
        /* high-speed support */
        if (ext_csd[EXT_CSD_HS_TIMING] == 1)
        {
            DEBUGF("[DEBUG] - mmc_partial_startup: High Speed is set \n");
            mmc->card_caps |= MMC_MODE_HS;
        }
	}

	/* Restrict card's capabilities by what the host can do */
	mmc->card_caps &= mmc->host_caps;


    /* Set working bus width */
    if ((mmc->host_caps & MMC_MODE_8BIT) == MMC_MODE_8BIT)
    {
		mmc->bus_width = 8;
		mmc->card_caps |= MMC_MODE_8BIT;
	}
	else if ((mmc->host_caps & MMC_MODE_4BIT) == MMC_MODE_4BIT)
    {
		mmc->bus_width = 4;
		mmc->card_caps |= MMC_MODE_4BIT;
	}
	else 
	{
		mmc->bus_width = 1;

    }

    if (mmc->card_caps & MMC_MODE_HS) {
        if (mmc->card_caps & MMC_MODE_HS_52MHz)
            mmc->clock = 52000000;
        else
            mmc->clock = 26000000;
    } else
        mmc->clock = mmc->tran_speed;
		
    /* adjust max clock */
    if (mmc->clock > mmc->f_max)
		mmc->clock = mmc->f_max;
;

    
    DEBUGF("[DEBUG] - mmc_partial_startup: clock = %d \n",mmc->clock);

    DEBUGF("[DEBUG] - mmc_partial_startup: ddr enabled = %s \n",(mmc->ddr_enable == 1)?"True":"False");

	/* fill in device description */
	mmc->block_dev.lun = 0;
	mmc->block_dev.type = 0;
	mmc->block_dev.blksz = mmc->read_bl_len;
	mmc->block_dev.lba = lldiv(mmc->capacity, mmc->read_bl_len);
	sprintf(mmc->block_dev.vendor, "Man %06x Snr %08x", mmc->cid[0] >> 8,(mmc->cid[2] << 8) | (mmc->cid[3] >> 24));
	sprintf(mmc->block_dev.product, "%c%c%c%c%c", mmc->cid[0] & 0xff,(mmc->cid[1] >> 24), (mmc->cid[1] >> 16) & 0xff,(mmc->cid[1] >> 8) & 0xff, mmc->cid[1] & 0xff);
	sprintf(mmc->block_dev.revision, "%d.%d", mmc->cid[2] >> 28,(mmc->cid[2] >> 24) & 0xf);
#ifdef CONFIG_PARTITIONS	
	init_part(&mmc->block_dev);
#endif
	return 0;
}

int mmc_startup(struct mmc *mmc)
{
	int err, width,ddr_width = 0;
	uint mult, freq;
	u64 cmult, csize, capacity;
	struct mmc_cmd cmd;
	ALLOC_CACHE_ALIGN_BUFFER(char, ext_csd, 512);
	ALLOC_CACHE_ALIGN_BUFFER(char, test_csd, 512);
	int timeout = 1000;

#ifdef CONFIG_MMC_SPI_CRC_ON
	if (mmc_host_is_spi(mmc)) { /* enable CRC check for spi */
		cmd.cmdidx = MMC_CMD_SPI_CRC_ON_OFF;
		cmd.resp_type = MMC_RSP_R1;
		cmd.cmdarg = 1;
		cmd.flags = 0;
		err = mmc_send_cmd(mmc, &cmd, NULL);
		if (err)
			return err;
	}
#endif

	/* Put the Card in Identify Mode */
	cmd.cmdidx = mmc_host_is_spi(mmc) ? MMC_CMD_SEND_CID :
		MMC_CMD_ALL_SEND_CID; /* cmd not supported in spi */
	cmd.resp_type = MMC_RSP_R2;
	cmd.cmdarg = 0;
	cmd.flags = 0;

    DEBUGF("[DEBUG] - mmc_startup: Send Cmd %d (MMC_CMD_ALL_SEND_CID), \n",cmd.cmdidx);
	err = mmc_send_cmd(mmc, &cmd, NULL);
	if (err)
		return err;

	memcpy(mmc->cid, cmd.response, 16);

	/*
	 * For MMC cards, set the Relative Address.
	 * For SD cards, get the Relatvie Address.
	 * This also puts the cards into Standby State
	 */
	if (!mmc_host_is_spi(mmc)) { /* cmd not supported in spi */
		cmd.cmdidx = SD_CMD_SEND_RELATIVE_ADDR;
		cmd.cmdarg = mmc->rca << 16;
		cmd.resp_type = MMC_RSP_R6;
		cmd.flags = 0;

        DEBUGF("[DEBUG] - mmc_startup: Send Cmd %d (SD_CMD_SEND_RELATIVE_ADDR), \n",cmd.cmdidx);
		err = mmc_send_cmd(mmc, &cmd, NULL);
		if (err)
			return err;

		if (IS_SD(mmc))
			mmc->rca = (cmd.response[0] >> 16) & 0xffff;
	}

	/* Get the Card-Specific Data */
	cmd.cmdidx = MMC_CMD_SEND_CSD;
	cmd.resp_type = MMC_RSP_R2;
	cmd.cmdarg = mmc->rca << 16;
	cmd.flags = 0;

    DEBUGF("[DEBUG] - mmc_startup: Send Cmd %d (MMC_CMD_SEND_CSD), \n",cmd.cmdidx);
	err = mmc_send_cmd(mmc, &cmd, NULL);
    if (err)
		return err;

	/* Waiting for the ready status */
	err = mmc_send_status(mmc, timeout);
	if (err)
		return err;

	mmc->csd[0] = cmd.response[0];
	mmc->csd[1] = cmd.response[1];
	mmc->csd[2] = cmd.response[2];
	mmc->csd[3] = cmd.response[3];

    DEBUGF("[DEBUG] - mmc_startup: mmc->csd[0] = 0x%0.8X \n",mmc->csd[0]);
    DEBUGF("[DEBUG] - mmc_startup: mmc->csd[1] = 0x%0.8X \n",mmc->csd[1]);
    DEBUGF("[DEBUG] - mmc_startup: mmc->csd[2] = 0x%0.8X \n",mmc->csd[2]);
    DEBUGF("[DEBUG] - mmc_startup: mmc->csd[3] = 0x%0.8X \n",mmc->csd[3]);


	if (mmc->version == MMC_VERSION_UNKNOWN) {
		int version = (cmd.response[0] >> 26) & 0xf;

		switch (version) {
			case 0:
				mmc->version = MMC_VERSION_1_2;
				break;
			case 1:
				mmc->version = MMC_VERSION_1_4;
				break;
			case 2:
				mmc->version = MMC_VERSION_2_2;
				break;
			case 3:
				mmc->version = MMC_VERSION_3;
				break;
			case 4:
				mmc->version = MMC_VERSION_4;
				break;
			default:
				mmc->version = MMC_VERSION_1_2;
				break;
		}
	}

	/* divide frequency by 10, since the mults are 10x bigger */
	freq = fbase[(cmd.response[0] & 0x7)];
	mult = multipliers[((cmd.response[0] >> 3) & 0xf)];

	mmc->tran_speed = freq * mult;

	mmc->read_bl_len = 1 << ((cmd.response[1] >> 16) & 0xf);

	if (IS_SD(mmc))
		mmc->write_bl_len = mmc->read_bl_len;
	else
		mmc->write_bl_len = 1 << ((cmd.response[3] >> 22) & 0xf);

	if (mmc->high_capacity) {
		csize = (mmc->csd[1] & 0x3f) << 16
			| (mmc->csd[2] & 0xffff0000) >> 16;
		cmult = 8;
	} else {
		csize = (mmc->csd[1] & 0x3ff) << 2
			| (mmc->csd[2] & 0xc0000000) >> 30;
		cmult = (mmc->csd[2] & 0x00038000) >> 15;
	}

	mmc->capacity = (csize + 1) << (cmult + 2);
	mmc->capacity *= mmc->read_bl_len;

	if (mmc->read_bl_len > 512)
		mmc->read_bl_len = 512;

	if (mmc->write_bl_len > 512)
		mmc->write_bl_len = 512;

	/* Select the card, and put it into Transfer Mode */
	if (!mmc_host_is_spi(mmc)) { /* cmd not supported in spi */
		cmd.cmdidx = MMC_CMD_SELECT_CARD;
		cmd.resp_type = MMC_RSP_R1;
		cmd.cmdarg = mmc->rca << 16;
		cmd.flags = 0;
        DEBUGF("[DEBUG] - mmc_startup: Send Cmd %d (MMC_CMD_SELECT_CARD) \n",cmd.cmdidx);
		err = mmc_send_cmd(mmc, &cmd, NULL);
		if (err)
			return err;
	}


	/*
	 * For SD, its erase group is always one sector
	 */
	mmc->erase_grp_size = 1;
	mmc->part_config = MMCPART_NOAVAILABLE;
	if (!IS_SD(mmc) && (mmc->version >= MMC_VERSION_4)) {
		/* check  ext_csd version and capacity */
		err = mmc_send_ext_csd(mmc, ext_csd);
		if (!err & (ext_csd[EXT_CSD_REV] >= 2)) {
			/*
			 * According to the JEDEC Standard, the value of
			 * ext_csd's capacity is valid if the value is more
			 * than 2GB
			 */
			capacity = ext_csd[EXT_CSD_SEC_CNT] << 0
					| ext_csd[EXT_CSD_SEC_CNT + 1] << 8
					| ext_csd[EXT_CSD_SEC_CNT + 2] << 16
					| ext_csd[EXT_CSD_SEC_CNT + 3] << 24;
			capacity *= 512;
			if ((capacity >> 20) > 2 * 1024)
				mmc->capacity = capacity;
		}

		/*
		 * Check whether GROUP_DEF is set, if yes, read out
		 * group size from ext_csd directly, or calculate
		 * the group size from the csd value.
		 */
		if (ext_csd[EXT_CSD_ERASE_GROUP_DEF])
			mmc->erase_grp_size =
			      ext_csd[EXT_CSD_HC_ERASE_GRP_SIZE] * 512 * 1024;
		else {
			int erase_gsz, erase_gmul;
			erase_gsz = (mmc->csd[2] & 0x00007c00) >> 10;
			erase_gmul = (mmc->csd[2] & 0x000003e0) >> 5;
			mmc->erase_grp_size = (erase_gsz + 1)
				* (erase_gmul + 1);
		}

		/* store the partition info of emmc */
		if (ext_csd[EXT_CSD_PARTITIONING_SUPPORT] & PART_SUPPORT)
			mmc->part_config = ext_csd[EXT_CSD_PART_CONF];
	}

	if (IS_SD(mmc))
		err = sd_change_freq(mmc);
	else
		err = mmc_change_freq(mmc);

	if (err)
		return err;

	/* Restrict card's capabilities by what the host can do */
	mmc->card_caps &= mmc->host_caps;

	if (IS_SD(mmc)) {
		if (mmc->card_caps & MMC_MODE_4BIT) {
			cmd.cmdidx = MMC_CMD_APP_CMD;
			cmd.resp_type = MMC_RSP_R1;
			cmd.cmdarg = mmc->rca << 16;
			cmd.flags = 0;

			err = mmc_send_cmd(mmc, &cmd, NULL);
			if (err)
				return err;

			cmd.cmdidx = SD_CMD_APP_SET_BUS_WIDTH;
			cmd.resp_type = MMC_RSP_R1;
			cmd.cmdarg = 2;
			cmd.flags = 0;
			err = mmc_send_cmd(mmc, &cmd, NULL);
			if (err)
				return err;

			mmc_set_bus_width(mmc, 4);
		}

		if (mmc->card_caps & MMC_MODE_HS)
			mmc_set_clock(mmc, 50000000);
		else
			mmc_set_clock(mmc, 25000000);
	} else {
        for (width = EXT_CSD_BUS_WIDTH_8; width >= 0; width--) {

            /* PUMA6 FIX - if host_caps does not support width 8 or width 4, then
               do not try to configure the mmc to that width, just continue. */
            if   (((mmc->host_caps & (width << 8)) == 0) && (width !=0))
                continue;
        
            /* Enable DDR mode */
            if (((mmc->card_caps & MMC_MODE_DDR) != 0) && ((mmc->card_caps & MMC_MODE_HS) != 0) && (width !=0))
            {
                mmc_set_ddr(mmc,1);
            }
             
            /* if DDR enable, then update bus width value */
            if (mmc->ddr_enable)
            {
                switch (width)
                {
                case 0:
                    printf("Error: bus width of 1, is not legal in DDR mode\n");
                    break;
                case 1:
                    /* change to bus width of 4, with DDR enable */
                    ddr_width = 5;
                    break;
                case 2:
                    /* change to bus width of 8, with DDR enable */
                    ddr_width = 6;
                    break;
                }
            }

            /* Set the card to use 4 bit*/
			err = mmc_switch(mmc, EXT_CSD_CMD_SET_NORMAL,
					EXT_CSD_BUS_WIDTH, (mmc->ddr_enable == 0)? width : ddr_width );

			if (err)
				continue;

			if (!width) {
				mmc_set_bus_width(mmc, 1);
				break;
			} else
				mmc_set_bus_width(mmc, 4 * width);

			err = mmc_send_ext_csd(mmc, test_csd);
			if (!err && ext_csd[EXT_CSD_PARTITIONING_SUPPORT] \
				    == test_csd[EXT_CSD_PARTITIONING_SUPPORT]
				 && ext_csd[EXT_CSD_ERASE_GROUP_DEF] \
				    == test_csd[EXT_CSD_ERASE_GROUP_DEF] \
				 && ext_csd[EXT_CSD_REV] \
				    == test_csd[EXT_CSD_REV]
				 && ext_csd[EXT_CSD_HC_ERASE_GRP_SIZE] \
				    == test_csd[EXT_CSD_HC_ERASE_GRP_SIZE]
				 && memcmp(&ext_csd[EXT_CSD_SEC_CNT], \
					&test_csd[EXT_CSD_SEC_CNT], 4) == 0) {

				mmc->card_caps |= width;
				break;
			}
		}

		if (mmc->card_caps & MMC_MODE_HS) {
			if (mmc->card_caps & MMC_MODE_HS_52MHz)
				mmc_set_clock(mmc, 52000000);
			else
                mmc_set_clock(mmc, 26000000);
        } else
			mmc_set_clock(mmc, mmc->tran_speed);
	}

	/* fill in device description */
	mmc->block_dev.lun = 0;
	mmc->block_dev.type = 0;
	mmc->block_dev.blksz = mmc->read_bl_len;
	mmc->block_dev.lba = lldiv(mmc->capacity, mmc->read_bl_len);
	sprintf(mmc->block_dev.vendor, "Man %06x Snr %08x", mmc->cid[0] >> 8,
			(mmc->cid[2] << 8) | (mmc->cid[3] >> 24));
	sprintf(mmc->block_dev.product, "%c%c%c%c%c", mmc->cid[0] & 0xff,
			(mmc->cid[1] >> 24), (mmc->cid[1] >> 16) & 0xff,
			(mmc->cid[1] >> 8) & 0xff, mmc->cid[1] & 0xff);
	sprintf(mmc->block_dev.revision, "%d.%d", mmc->cid[2] >> 28,
			(mmc->cid[2] >> 24) & 0xf);
#ifdef CONFIG_PARTITIONS	
	init_part(&mmc->block_dev);
#endif
	return 0;
}

int mmc_send_if_cond(struct mmc *mmc)
{
	struct mmc_cmd cmd;
	int err;

	cmd.cmdidx = SD_CMD_SEND_IF_COND;
	/* We set the bit if the host supports voltages between 2.7 and 3.6 V */
	cmd.cmdarg = ((mmc->voltages & 0xff8000) != 0) << 8 | 0xaa;
	cmd.resp_type = MMC_RSP_R7;
	cmd.flags = 0;

	err = mmc_send_cmd(mmc, &cmd, NULL);
	if (err)
		return err;

	if ((cmd.response[0] & 0xff) != 0xaa)
		return UNUSABLE_ERR;
	else
		mmc->version = SD_VERSION_2;

	return 0;
}

int mmc_register(struct mmc *mmc)
{
	/* Setup the universal parts of the block interface just once */
	mmc->block_dev.if_type = IF_TYPE_MMC;
	mmc->block_dev.dev = cur_dev_num++;
	mmc->block_dev.removable = 1;
	mmc->block_dev.block_read = mmc_bread;
	mmc->block_dev.block_write = mmc_bwrite;
	mmc->block_dev.block_erase = mmc_berase;
	if (!mmc->b_max)
		mmc->b_max = CONFIG_SYS_MMC_MAX_BLK_COUNT;

	INIT_LIST_HEAD (&mmc->link);

	list_add_tail (&mmc->link, &mmc_devices);

	return 0;
}

#ifdef CONFIG_PARTITIONS
block_dev_desc_t *mmc_get_dev(int dev)
{
	struct mmc *mmc = find_mmc_device(dev);

	return mmc ? &mmc->block_dev : NULL;
}
#endif

int mmc_init(struct mmc *mmc)
{
	int err;
#if defined(CONFIG_USE_HW_MUTEX)
    int is_hw_locked = 0;
#endif

	if (mmc->has_init)
        return 0;

#if defined(CONFIG_USE_HW_MUTEX)
    /* Lock the HW Mutex */
    if (hw_mutex_is_locked(HW_MUTEX_EMMC) == NOT_LOCKED)
    {
        if (hw_mutex_lock(HW_MUTEX_EMMC) == 0)
        {
            printf("Error: mmc_init - failed to lock HW Mutex\n");
            return 0;
        }
        DEBUGF("[DEBUG] - mmc_init: hw mutex is locked \n");
        is_hw_locked = 1;            /* Hw mutex lock by this function */
    }
    else
    {
        DEBUGF("[DEBUG] - mmc_init: hw mutex is already locked - skip locking\n");
    }
#endif

    if (mmc->has_partial_init == 1)
    {
        DEBUGF("[DEBUG] - mmc_init calling mmc_partial_startup\n");
        mmc->version = MMC_VERSION_UNKNOWN;
        err = mmc_partial_startup(mmc);
        if (err)
            mmc->has_init = 0;
        else
            mmc->has_init = 1;
		mmc->has_partial_init = 0;
        goto init_done;
    }

    DEBUGF("[DEBUG] - mmc_init Init\n");
	err = mmc->init(mmc);

	if (err)
		goto init_done;


    DEBUGF("[DEBUG] - mmc_init: Set bus width to 1\n");
    mmc_set_bus_width(mmc, 1);

    DEBUGF("[DEBUG] - mmc_init: Set clock to 400000\n");
	mmc_set_clock(mmc, 400000);

	DEBUGF("[DEBUG] - mmc_init: Disable DDR\n");
	mmc_set_ddr(mmc,0);

	/* Reset the Card */
    DEBUGF("[DEBUG] - mmc_init: Reset the Card\n");
	err = mmc_go_idle(mmc);

	if (err)
		goto init_done;

	/* The internal partition reset to user partition(0) at every CMD0*/
	mmc->part_num = 0;

	/* Test for SD version 2 */
	//err = mmc_send_if_cond(mmc);  

	/* Now try to get the SD card's operating condition */
	//err = sd_send_op_cond(mmc);  

	/* If the command timed out, we check for an MMC card */
	//if (err == TIMEOUT) {
		err = mmc_send_op_cond(mmc);
		if (err) {
			printf("Card did not respond to voltage select!\n");
            err = UNUSABLE_ERR;
            goto init_done;
        }
	//}

    DEBUGF("[DEBUG] - mmc_init calling mmc_startup\n");
	err = mmc_startup(mmc);
	if (err)
		mmc->has_init = 0;
	else
		mmc->has_init = 1;

init_done:
#if defined(CONFIG_USE_HW_MUTEX)
    /* Release HW Mutes */
    if (is_hw_locked == 1) /* if lock by this funection then release, otherwise keep it locked */
    {
        hw_mutex_unlock(HW_MUTEX_EMMC);
        DEBUGF("[DEBUG] - mmc_init: hw mutex unlocked\n");
    }
#endif
	return err;
}

/*
 * CPU and board-specific MMC initializations.  Aliased function
 * signals caller to move on
 */
static int __def_mmc_init(bd_t *bis)
{
	return -1;
}

int cpu_mmc_init(bd_t *bis) __attribute__((weak, alias("__def_mmc_init")));
int board_mmc_init(bd_t *bis) __attribute__((weak, alias("__def_mmc_init")));

void print_mmc_devices(char separator)
{
	struct mmc *m;
	struct list_head *entry;

	list_for_each(entry, &mmc_devices) {
		m = list_entry(entry, struct mmc, link);

		printf("%s: %d", m->name, m->block_dev.dev);

		if (entry->next != &mmc_devices)
			printf("%c ", separator);
	}

	printf("\n");
}


static void print_mmcinfo(struct mmc *mmc)
{
    printf("MMC info:\n");
	printf("  Manufacturer ID: %x\n", mmc->cid[0] >> 24);
    printf("  OEM ID: %x\n", (mmc->cid[0] >> 8) & 0xff);
	printf("  Name: %c%c%c%c%c%c \n", mmc->cid[0] & 0xff,
                                   (mmc->cid[1] >> 24) & 0xff,
                                   (mmc->cid[1] >> 16) & 0xff,
                                   (mmc->cid[1] >> 8) & 0xff,
                                   (((mmc->cid[1] & 0xff)==0)?' ':(mmc->cid[1] & 0xff)),
                                   ((((mmc->cid[2] >> 24)& 0xff)==0)?' ':((mmc->cid[2]>>24) & 0xff)));
    printf("  MMC version %d.%d\n",(mmc->cid[2] >> 20)  & 0xf,(mmc->cid[2] >> 16) & 0xf);
	printf("  High Capacity: %s\n", mmc->high_capacity ? "Yes" : "No");
	printf("  Dual Data Rate (DDR): %s\n", mmc->ddr_enable ? "Yes" : "No");
	printf("  Bus Width: %d-bit\n", mmc->bus_width);
	printf("  Clock: %u\n",mmc->clock);
    printf("  Rd Block Len: %d\n", mmc->read_bl_len);
	printf("  Capacity: ");
	print_size(mmc->capacity, " ");
	printf("(%qu bytes) \n",mmc->capacity);
	
}


int get_mmc_num(void)
{
	return cur_dev_num;
}

int mmc_initialize(bd_t *bis)
{
    struct mmc *mmc;
	INIT_LIST_HEAD (&mmc_devices);
	cur_dev_num = 0;

#if defined(CONFIG_USE_HW_MUTEX)
    /* Lock the HW Mutex */
    if (hw_mutex_lock(HW_MUTEX_EMMC) == 0)
    {
        printf("Error: mmc_initialize - failed to lock HW Mutex\n");
        return 0;
    }
    DEBUGF("[DEBUG] - mmc_initialize: hw mutex locked\n");
    //g_hw_mutex_look = LOCKED;
#endif

	if (board_mmc_init(bis) < 0)
		cpu_mmc_init(bis);

#if defined(CONFIG_USE_HW_MUTEX)
    /* Release HW Mutes */
    hw_mutex_unlock(HW_MUTEX_EMMC);
    DEBUGF("[DEBUG] - mmc_initialize: hw mutex unlocked\n");
#endif

	print_mmc_devices(',');

    mmc = find_mmc_device(CONFIG_SYS_MMC_ENV_DEV);
    if (!mmc) {
        printf("Error: no mmc device at slot %x\n", CONFIG_SYS_MMC_ENV_DEV);
		return -1;
    }
    if (mmc_init(mmc)) {
        puts("MMC init failed\n");
        return  -1;
    }

    print_mmcinfo(mmc);

	return 0;
}




