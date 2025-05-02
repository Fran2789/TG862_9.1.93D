/*
 * GPL LICENSE SUMMARY
 *
 *  Copyright(c) 2012 Intel Corporation.
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

#include <mmc_utils.h>
#include <common.h>
#include <command.h>
#include <environment.h>
#include <uImage.h>
#include <mmc.h>
#include <rtc.h>


/* Read <size> bytes from eMMC <srcAddr> to <buff> */
int mmc_read_buff(unsigned int srcAddr, unsigned int* buffer, unsigned int size){
    int curr_device = 0;
	struct mmc *mmc;
    //block_dev_desc_t *mmc_dev;
    unsigned long * malloc_buff = NULL;

    if (get_mmc_num() > 0)
        curr_device = CONFIG_SYS_MMC_IMG_DEV;
    else {
        printf("\nError: no MMC device available.\n");
        return STATUS_NOK;
    }
    
    mmc = find_mmc_device(curr_device);

	if (mmc)
	{
		mmc_init(mmc);

        uint blk_start, blk_cnt, n;

        if ((srcAddr & (mmc->read_bl_len-1)) != 0 )
            printf("Warning: mmc_read_buff: address [0x%X] is not align to %d.\n", srcAddr,mmc->read_bl_len);

        //if ((size & (mmc->read_bl_len-1)) != 0 )
        //    printf("Warning: mmc_read_buff: size [0x%X] is not align to %d.\n", size,mmc->read_bl_len);

        blk_start = ALIGN(srcAddr, mmc->read_bl_len) / mmc->read_bl_len;
        blk_cnt   = ALIGN(size, mmc->read_bl_len) / mmc->read_bl_len;

        malloc_buff = (unsigned long *)malloc(blk_cnt* mmc->read_bl_len);

        n = mmc->block_dev.block_read(curr_device, blk_start,blk_cnt, malloc_buff);

        if ((n == blk_cnt) && (buffer != NULL))
            memcpy(buffer,malloc_buff,size);           

        free(malloc_buff);
        return (n == blk_cnt) ? STATUS_OK : STATUS_NOK;
    } 
    else
	{
		printf("\nError: no mmc device in slot %x\n", curr_device);
		return STATUS_NOK;
	}
}

/* Write <length> bytes, from <srcAddr> in RAM, to <dstAddr> in flash, with/without verbose */
int flash_write_mmc(unsigned int srcAddr, unsigned int dstAddr, unsigned int length, int verbose)
{
    struct mmc *mmc;
    unsigned int  blk_start;
    unsigned int  blk_cnt;
    unsigned int  blk_n;

    if (verbose == VERBOSE_ENABLE)
    {
        printf("Program to eMMC from:0x%0.8X to:0x%0.8X length: %d\n",srcAddr, dstAddr,length);
    }

    /* Get MMC device */
    mmc = find_mmc_device(CONFIG_SYS_MMC_IMG_DEV);

    if (!mmc)
	{
		printf("No MMC card found\n");
		return STATUS_NOK;
	}

	if (mmc_init(mmc))
	{
		printf("MMC init failed\n");
		return  STATUS_NOK;
	}

    if ((dstAddr & (mmc->write_bl_len-1)) != 0 )
        printf("Warning: flash_write_mmc: dest [0x%X] is not align to %d.\n", dstAddr,mmc->write_bl_len);

    //if ((length & (mmc->write_bl_len-1)) != 0 )
    //    printf("Warning: flash_write_mmc: length [0x%X] is not align to %d.\n", length,mmc->write_bl_len);

    /* Convert to blocks */
    blk_start = ALIGN(dstAddr, mmc->write_bl_len)   / mmc->write_bl_len;
	blk_cnt   = ALIGN(length, mmc->write_bl_len) / mmc->write_bl_len;

	printf("Writing to MMC(%d)..\n", CONFIG_SYS_MMC_IMG_DEV);
    	
    blk_n = mmc->block_dev.block_write(CONFIG_SYS_MMC_IMG_DEV, blk_start, blk_cnt, (u_char *)srcAddr);

	if (blk_n != blk_cnt)
    {
        printf("\nError: failed to write to mmc.\n");
        return STATUS_NOK;
    }

    printf("Done.\n");
    return STATUS_OK;
}
