/*
 * (C) Copyright 2001
 * Kyle Harris, Nexus Technologies, Inc. kharris@nexus-tech.net
 *
 * (C) Copyright 2001-2004
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
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


/* Copyright 2008, Texas Instruments Incorporated
 *
 * This program has been modified from its original operation by Texas Instruments
 * to do the following:
 * 1) Support serial flash using SFI.
 * 2) Support Emulation platforms.
 * 3) Handle Redundant Env case.
 *
 * Derived from board/alaska/flash.c
 *
 * THIS MODIFIED SOFTWARE AND DOCUMENTATION ARE PROVIDED
 * "AS IS," AND TEXAS INSTRUMENTS MAKES NO REPRESENTATIONS
 * OR WARRENTIES, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
 * TO, WARRANTIES OF MERCHANTABILITY OR FITNESS FOR ANY
 * PARTICULAR PURPOSE OR THAT THE USE OF THE SOFTWARE OR
 * DOCUMENTATION WILL NOT INFRINGE ANY THIRD PARTY PATENTS,
 * COPYRIGHTS, TRADEMARKS OR OTHER RIGHTS.
 * See The GNU General Public License for more details.
 *
 * These changes are covered under version 2 of the GNU General Public License,
 * dated June 1991.
 */

 /* 
  * Includes Intel Corporation's changes/modifications dated: 2011. 
  * Changed/modified portions - Copyright © 2011 , Intel Corporation.   
  */ 

#include <common.h>
#include <command.h>
#if !defined CFG_FLASH_CFI_DRIVER && !defined CFG_NO_FLASH

#include <spi.h>
#include <serial_flash.h>
#include <docsis_ip_boot_params.h> 
#include <puma6_hw_mutex.h>

#undef DEBUG
//#define DEBUG

#define FLASH_TEST_CMD


#if defined(DEBUG)
#define DEBUG(fmt,arg...)  printf(fmt, ##arg)
#else
#define DEBUG(fmt,arg...)
#endif



/* Flash info table - must be define according to "flash.h" driver header file */
flash_info_t flash_info[CFG_MAX_FLASH_BANKS];	

/* API External functions prototype */
/* Do not change name */
int  flash_addr_init(int i, int verbose);

/*
 * External table of chip select functions (see the appropriate board
 * support for the actual definition of the table).
 */
extern spi_chipsel_type spi_chipsel[];

/* External Spi/Flash addresing mode , can be 3 or 4 */
extern int spi_addr_width;

#ifdef FLASH_TEST_CMD
extern int spi_debug;
#endif

/* Global variables */
static int          g_serial_flash_init = 0;     /* Global module init */
static int          g_boot_param_num_of_flashes = 0;        /* Number of detected Flash devices */
static unsigned int g_boot_param_ubootbin_base_offset = 0;
static unsigned int g_boot_param_ubootbin_size = 0;
unsigned int        g_uboot_partition_size = 0;
unsigned int        g_boot_param_env1_base_offset = 0;
unsigned int        g_boot_param_env2_base_offset = 0;
unsigned int        g_boot_param_env_size = 0;


/* private global table that holds Flash Commands */
static const sf_cmd_tbl_t cmd_tbl[] = {
    {
        WRITE_EN(0x06), WRITE_DIS(0x04), WRITE_STS(0x01),
        READ_ID(0x9F), READ_STS(0x05), READ_DATA(0x03), READ_FAST(0x0B),
        ERASE_SECT(0xD8), ERASE_BLOCK(0xD8), ERASE_CHIP(0xC7), PROG_PAGE(0x02)
    },

    {
        WRITE_EN(0x06), WRITE_DIS(0x04), WRITE_STS(0x01),
        READ_ID(0x9F), READ_STS(0x05), READ_DATA(0x03), READ_FAST(0x0B),
        ERASE_SECT(0x20), ERASE_BLOCK(0xD8), ERASE_CHIP(0xC7), PROG_PAGE(0x02)
    },
};

/* private global table that holds information on all supported Flash types*/
static const sf_info_t sf_info[] = {

	/* Numonyix  Serial Flashes */
    {   FLASH_N25Q128, MAN_ID(0x20), MEM_TYPE(0xBA), MEM_DENS(0x18), EXD_ID(0x10, 0x1),
        FLASH_SIZE(16*1024*1024), SECT_CNT(256), SECT_SIZE(64*1024), SECT_PER_BLOCK(1), 0x3C, PAGE_SIZE_256,
        &cmd_tbl[0], "Numonyx N25Q128",
        0},
    {   FLASH_N25Q256, MAN_ID(0x20), MEM_TYPE(0xBA), MEM_DENS(0x19), EXD_ID(0x10, 0x0),
        FLASH_SIZE(32*1024*1024), SECT_CNT(512), SECT_SIZE(64*1024), SECT_PER_BLOCK(1), 0x3C, PAGE_SIZE_256,
        &cmd_tbl[0], "Numonyx N25Q256",
        0 },
};



#undef VALIDATE_ERASE
#undef VALIDATE_WRITE




/*-----------------------------------------------------------------------
 * Internal Private Functions Prototypes
 */
static int sf_cmd2buff        (unsigned char* buff, unsigned char cmd, unsigned int addr);
static int sf_probe_flash_idx (int bank, int verbose );
static int sf_write_enable    (flash_info_t* info, const sf_cmd_tbl_t* cmd_tbl  );
static int sf_write_status    (flash_info_t* info, const sf_cmd_tbl_t* cmd_tbl, u8 val);
static int sf_read_status     (flash_info_t* info, const sf_cmd_tbl_t* cmd_tbl, unsigned char *status);
static int sf_wait_till_ready (flash_info_t* info, const sf_cmd_tbl_t* cmd_tbl  );
static int sf_erase_sector    (flash_info_t* info, const sf_cmd_tbl_t* cmd_tbl, unsigned int sect_addr, unsigned int block_erase);
static int sf_unlock          (flash_info_t* info, const sf_cmd_tbl_t* cmd_tbl, unsigned int start_addr, unsigned int end_addr);
static int sf_write           (flash_info_t* info, const sf_info_t* sf_info_p, unsigned int addr, unsigned char *buf, unsigned int len);
static int sf_write_page      (flash_info_t* info, const sf_info_t* sf_info_p, unsigned int addr, unsigned char *buf, unsigned int len);
static unsigned char sf_sector_protected (flash_info_t* info,const  sf_cmd_tbl_t* cmd_tbl, ushort sector);
static int sf_get_flindx_from_flid (unsigned long flash_id);
static void sf_sync_real_protect (flash_info_t* info);
static int sf_write_then_read( int bank, unsigned char *tx_data, int tx_len, unsigned char *rx_data, int rx_len);





/*-----------------------------------------------------------------------
 * API External Driver Functions 
 */


unsigned long flash_init (void)
{
    int i,j;
    int idx = -1;
    unsigned long size = 0;
    unsigned long base =0; 
    unsigned int sector_size = 0;

    if (g_boot_param_num_of_flashes == 0)
    {
        return 0;
    }

    g_serial_flash_init = 0;

    DEBUG("Inside function flash_init \n");
    
    // ARRIS bypass this code (with associated error prints) since we don't use NOR flash
    return 0;
    // END ARRIS

    for (i = 0; i < CFG_MAX_FLASH_BANKS; i++)
    {
        /* Reset flash_info structure */
        memset (&flash_info[i], 0, sizeof (flash_info_t));

        /* check if we need to detect more flash devices */
        if (i >= g_boot_param_num_of_flashes)
        {
            // If no Flash is connected, keep the for loop to clear the rest of flash_info[i] array
            continue;
        }

        /* Lock the HW Mutex */
        if (hw_mutex_lock(HW_MUTEX_NOR_SPI) == 0)
        {
            printf("failed to lock HW Mutex\n",i);
            continue;
        }

        /* Search for Flash Type index in sf_info table*/
        idx = sf_probe_flash_idx(i, 1);

        /* Release HW Mutes */
        hw_mutex_unlock(HW_MUTEX_NOR_SPI);

        if (idx < 0)
        {
            printf("failed to probe Flash (bank %d)\n",i);
            continue;
        }


        /* Update uboot flash_info structure */
        flash_info[i].flash_id     = sf_info[idx].flash_id;
        flash_info[i].sector_count = sf_info[idx].num_sectors;
        flash_info[i].size         = sf_info[idx].flash_size;
        flash_info[i].bank         = i;

        /* Updaet Sectors base address */
        switch (i)
        {
        case 0:
            base = CFG_FLASH_BASE;
            break;
        case 1:
            base = CFG_FLASH_BASE; /* TODO: get the second flash base address from reading the Split Register */
            break;
        default:
            printf("CFG_MAX_FLASH_BANKS is define as %d, while flash %d does not have base address\n",CFG_MAX_FLASH_BANKS,i);
            continue;
        }


        for (j = 0; j < flash_info[i].sector_count; j++)
    	{
            flash_info[i].start[j] = base;
            base += sf_info[idx].sector_size;
    	}

        /* Unlock the whole flash */
        printf("Warning: Unlock the whole flash \n");

        /* Lock the HW Mutex */
        if (hw_mutex_lock(HW_MUTEX_NOR_SPI) == 0)
        {
            printf("Error: Flash not Unlocked - failed to lock HW Mutex\n");
            continue;
        }

        if( sf_unlock(&flash_info[i], (sf_cmd_tbl_t *)sf_info[idx].cmd_tbl, 0x0000, flash_info[i].size)!= 0 )
        {
            printf("Error: Flash not Unlocked \n");
        }

#ifdef SPI_DEBUG
#warning "SPI_DEBUG is defined"
        printf("Flash use %d byte addressing mode\n",spi_addr_width);
#endif 

        /* Release HW Mutes */
        hw_mutex_unlock(HW_MUTEX_NOR_SPI);


        size += flash_info[i].size;

        /* get the h/w and s/w protection status in sync */
        sf_sync_real_protect(&flash_info[i]);

        sector_size = sf_info[idx].sector_size;
    }


    /* If probe failed to all banks */
    if (size == 0)
    {
        printf("Error: failed to probe Flash\n");
        return 0;
    }


    /* Calculate the u-boot partition size */
    g_uboot_partition_size = 0;
    while( g_uboot_partition_size < g_boot_param_ubootbin_size )
    {
        g_uboot_partition_size += sector_size;
    }

   /* 
    * Protect monitor and environment sectors. 
    */
    flash_protect (FLAG_PROTECT_SET, g_boot_param_ubootbin_base_offset, g_boot_param_ubootbin_base_offset + g_uboot_partition_size - 1,   &flash_info[0]);
    flash_protect (FLAG_PROTECT_SET, CFG_ENV_ADDR,        CFG_ENV_ADDR + CFG_ENV_SIZE - 1,               &flash_info[0]);
#ifdef CFG_ENV_ADDR_REDUND
    flash_protect (FLAG_PROTECT_SET, CFG_ENV_ADDR_REDUND, CFG_ENV_ADDR_REDUND + CFG_ENV_SIZE_REDUND - 1, &flash_info[0]);
#endif

    DEBUG("function flash_init - Done\n");
    return size;
}


/*-----------------------------------------------------------------------
 */
void flash_print_info (flash_info_t * info)
{
	int i,j;

	if (info->flash_id == FLASH_UNKNOWN) {
		printf ("Error: Missing or unknown FLASH type\n");
		return;
	}

    if ((i = sf_get_flindx_from_flid (info->flash_id)) >= 0)
        printf ("%s\n", sf_info[i].flash_name);
    else 
    {
        printf ("Error: Missing or unknown FLASH type\n");
        return;
    }

	printf ("Size: %ld MB in %d Sectors\n",
		info->size >> 20, info->sector_count);

	printf ("Sector Start Addresses:\n");
	for (i = 0, j = 0; i < info->sector_count; ++i, j++) {
		if (j > 4) {
			printf ("\n");
            j = 0;
        }
		printf (" %08lX%s",
			info->start[i], info->protect[i] ? " (RO)" : "     ");
	}
	printf ("\n");
	return;
}


/*-----------------------------------------------------------------------
 */
int flash_erase (flash_info_t * info, int s_first, int s_last)
{
    int indx;
    sf_cmd_tbl_t* cmd_tbl;
    int ret;
    int i;

    if ((indx = sf_get_flindx_from_flid (info->flash_id)) < 0)
    {
        printf ("Error: Missing or unknown FLASH type\n");
        return 1;
    }
    cmd_tbl = (sf_cmd_tbl_t*)sf_info[indx].cmd_tbl;


    unsigned int sect_addr = 0;

	if ((s_first < 0) || (s_first > s_last) ||(s_last > info->sector_count)) {
		if (info->flash_id == FLASH_UNKNOWN)
			printf ("- missing\n");
		else
			printf ("- no sectors to erase\n");
		return 1;
	}

		DEBUG(" Inside function %s\n",__FUNCTION__);

		/* Sync up real protect is already called */
		for(i = s_first ; i<= s_last; i++ )
		{
            int block_erase = 0;

            /* Get sector address */
			sect_addr= info->start[i];
		
            /* Calculate sector offset in Flash */
			sect_addr -= info->start[0];
			if( info->protect[i]  == 0 )
			{
                /* For 4KB sectors with block erase option, erase blocks when possible for faster performance */
                if( sf_info[indx].sect_per_block > 1 )
                {
                    if ( ( (i & (sf_info[indx].sect_per_block-1) ) == 0) && ( ( i + sf_info[indx].sect_per_block ) <= s_last ) )
                    {
                        block_erase = 1;
                    }
                }

                /* Lock the HW Mutex */
                if (hw_mutex_lock(HW_MUTEX_NOR_SPI) == 0)
                {
                     printf(" sf_erase_sector failed for sector = %d address = 0x%x - HW  Failed\n",
                            i,info->start[i] );
                     return -1;
                }
                
                /* Erase one sector */
                ret = sf_erase_sector(info, cmd_tbl, sect_addr, block_erase);

                /* Release HW Mutes */
                hw_mutex_unlock(HW_MUTEX_NOR_SPI);

                if (ret < 0 ) 
                {
                    printf(" sf_erase_sector failed for sector = %d address = 0x%x\n",
                            i,info->start[i] );
                    return -1;
                }
				else 
                {
					DEBUG("Erasing sector %d at address 0x%x........ done\n",i,info->start[i]);
                    printf (".");
				}
			}
			else
			{
				printf("Serial Flash Sector %d address = 0x%x is Locked can't be Erased\n",
						i,info->start[i]);
			}
            if(block_erase)
            {
                i+=sf_info[indx].sect_per_block-1;
            }
		}
		printf("\n");
	 	return 0;
}





/*-----------------------------------------------------------------------
 * Set/Clear sector's lock bit, returns:
 * 0 - OK
 * 1 - Error (timeout, voltage problems, etc.)
 */
int flash_real_protect (flash_info_t * info, long sector, int prot)
{

    int rc = 0;

    if( sector <= info->sector_count)
    {
        info->protect[sector] = prot;
    }else{
        printf("Error: Sector amount %d exceding FLASH maximum sectors %d",
                sector,info->sector_count);
        rc = 1;
    }
    return rc;

}


/* Please note:
   This function is beening called before we have initialize the UART.
   So, no prints are allowed in this function
*/
int flash_addr_init(int i, int verbose)
{
    /* Get uboot offset in flash from Docsis IP Boot Params */
    g_boot_param_ubootbin_base_offset = BOOT_PARAM_DWORD_READ(ARM11_UBOOT_OFFSET);

    /* Get uboot size from Docsis IP Boot Params */
    g_boot_param_ubootbin_size = BOOT_PARAM_DWORD_READ(ARM11_UBOOT_SIZE);

    /* Get Env1 offset in flash from Docsis IP Boot Params */
    g_boot_param_env1_base_offset = BOOT_PARAM_DWORD_READ(ARM11_ENV1_OFFSET);

    /* Get Env2 offset in flash from Docsis IP Boot Params */
    g_boot_param_env2_base_offset = BOOT_PARAM_DWORD_READ(ARM11_ENV2_OFFSET);

    /* Get Env1 and Env2 Size (same size) from Docsis IP Boot Params */
    g_boot_param_env_size = BOOT_PARAM_DWORD_READ(ARM11_ENV_SIZE);

    /* Get number of flashes from Docsis IP Boot Params */
    g_boot_param_num_of_flashes = BOOT_PARAM_DWORD_READ(NUMBER_OF_FLASHES); ;

    return 0;
}


/*
 * This function gets the u-boot flash sector protection status
 * (flash_info_t.protect[]) in sync with the sector protection
 * status stored in hardware.
 */
static void sf_sync_real_protect (flash_info_t * info)
{
	int i;

    int indx;
    if ((indx = sf_get_flindx_from_flid (info->flash_id)) < 0)
    {
        printf ("Error: Missing or unknown FLASH type\n");
        return;
    }

    for(i = 0; i < info->sector_count; ++i){
        info->protect[i] = sf_sector_protected(info, (sf_cmd_tbl_t *)(sf_info[indx].cmd_tbl), i);
    }
    g_serial_flash_init++;

}


/*
 * The following code cannot be run from FLASH!
 */
static unsigned char sf_sector_protected(flash_info_t *info, const sf_cmd_tbl_t* cmd_tbl, ushort sector)
{
    /* Initially Flash is unlocked */
    if( g_serial_flash_init == 0 ){
        return 0;
    }else{
        return info->protect[sector];
    }

}


static int  sf_probe_flash_idx( int bank, int verbose )
{
    int i;
    unsigned char  flash_id[5]={0x00,0x00,0x00,0x00,0x00};

    /* TODO: Add support to traverse through complete command table for the
     * case where READ_ID command from preceding command table (currently -
     * first table only) failed because the device didn't support that
     * command.
     */
    unsigned char cmd = cmd_tbl[0].read_id;  /* See TODO comment up.*/

    if(sf_write_then_read( bank, &cmd,1,(unsigned char*)&flash_id,5) < 0)
    {
        if(verbose)
            printf("Error: Serial Flash Read JEDEC Failed\n");
        return -1;
    }
    if(verbose)
        DEBUG("\n Auto detect Flash ID  0x%x  0x%x  0x%x\n",flash_id[0],flash_id[1],flash_id[2]);


    /* Check if second flash is not installed */
    if( ( flash_id[0] == MAN_ID(0xFF) ) && ( bank > 0 ) )
    {
        return -1;
    }


    /* Search for a match FLASH in flash info table */
    for (i = 0; i < (sizeof (sf_info) / sizeof (sf_info_t)); i++)
    {
        if (   (flash_id[0] == sf_info[i].manf_id)
            && (flash_id[1] == sf_info[i].mem_type)
            && (flash_id[2] == sf_info[i].mem_density)
            && (flash_id[3] == sf_info[i].extended_id[0])
            && (flash_id[4] == sf_info[i].extended_id[1]))
        {
            if(verbose)
                printf ("%s flash found\n", sf_info[i].flash_name);

            /* return flash info index */
            return i;
        }
    }

    if(verbose) {
        printf("*** Warning - Unsupported Flash detected, flash is unusable\n\nManufacturer ID: 0x%X\nType: 0x%X\nDensity: 0x%X\nExtended ID: {0x%X, 0x%X}\n",
               (int)flash_id[0], (int)flash_id[1], (int)flash_id[2],
                (int)flash_id[3], (int)flash_id[4]);
    }

    return -1;
}



/*
 * write_status_reg writes the vlaue to status reg.
 * Returns status on success or negetive valueif error.
 * NOTE:  write enable should be called before calling this function.
 */
static int sf_write_status (flash_info_t * info, const sf_cmd_tbl_t* cmd_tbl, u8 val)
{
    u8 write_val[2] = {cmd_tbl->write_sts, val};
    DEBUG("Inside the function %s\n",__FUNCTION__);

    if(sf_write_then_read( info->bank, &write_val[0],sizeof(write_val),NULL,0) != sizeof(write_val) )
    {
        printf("Error: Failed to write FLASH status register\n");
        return -1;
    }
    return 0;
}


/*
 * write_enable perpares the flash state to enable write/erase/lock/unlock
 * Returns 0 on success or negetive valueif error.
 */
static int sf_write_enable(flash_info_t * info, const sf_cmd_tbl_t* cmd_tbl)
{
    u8 code = cmd_tbl->write_en;
    DEBUG("Inside the function %s\n",__FUNCTION__);
    if( sf_write_then_read( info->bank,&code,1,NULL,0) == 1)
    {
        return 0;
    }
    else
    {
        DEBUG("sf_write_then_read failed in function %s\n", __FUNCTION__ );
        return -1;
    }
}


static int sf_unlock (flash_info_t* info, const sf_cmd_tbl_t* cmd_tbl, unsigned int start_addr, unsigned int end_addr)
{
    u8  status = 0;
    s32 retval = 0;

    DEBUG("Inside the function %s with start_addr= 0x%x and end_addr = 0x%x\n",
            __FUNCTION__,start_addr,end_addr);

    /* sanity checks */
    if( (start_addr > end_addr) && (end_addr > info->size ))
    {
        printf("Error: Invalid arguments\n");
        return -1;
    }
    if( sf_write_enable(info, cmd_tbl)!= 0){
        DEBUG("sf_write_enable failed in function %s\n",__FUNCTION__);
        return -1;
    }

    if( sf_read_status(info, cmd_tbl, &status) != 0 ){
        DEBUG(" Read Status Failed in %s\n",__FUNCTION__);
        return -1;
    }

    DEBUG("Status =0x%x\n",status);

    /* Simply Unlock the whole flash */

    status = 0;

    if(sf_wait_till_ready(info, cmd_tbl))
    {
        DEBUG("Flash Busy\n",__FUNCTION__);
        return -1;
    }
    if( sf_write_enable(info, cmd_tbl)!= 0){
        DEBUG("sf_write_enable failed in function %s\n",__FUNCTION__);
        return -1;
    }
    retval = sf_write_status(info, cmd_tbl, status);
    if( retval == 0){
        DEBUG("Sector(s) corrosponding to Address 0x%x to  0x%x is  unlocked\n",
                start_addr,end_addr);
        DEBUG("Return retval = %d  from flash_unlock\n",retval);
    }else{
        DEBUG("In function  %s write_status_reg failed\n",__FUNCTION__);
        return -1;
    }
    return 0;
}

/*
 * sf_read_status return the status of the flash
 * Returns status on success or negetive valueif error.
 */
static int sf_read_status (flash_info_t * info, const sf_cmd_tbl_t* cmd_tbl, unsigned char *status)
{
    u8 cmd = cmd_tbl->read_sts;

    if( status != NULL ) {
        *status =0;
    }else{
        DEBUG("In function %s Invalid argument\n",__FUNCTION__);
        goto read_fail;
    }

    DEBUG("Inside the function %s\n",__FUNCTION__);

    if( sf_write_then_read( info->bank,&cmd, 1, status,1) == 2 ) {
        DEBUG(" Flash Status = 0x%x\n",*status);
        return 0;
    }else{
        DEBUG("sf_write_then_read failed in function %s\n",__FUNCTION__);
        goto read_fail;
    }

    read_fail:
    printf("Error: Failed to read status register\n");
    return -1;
}


/*
 * sf_wait_till_ready to read status register until ready, or timeout occurs.
 * Returns non-zero if error.
 */
static int sf_wait_till_ready(flash_info_t * info, const sf_cmd_tbl_t* cmd_tbl)
{
    u8 status = 0;
    u64 time_out = SERIAL_FLASH_TIMEOUT;

    DEBUG("Inside the function %s\n",__FUNCTION__);
    do{
        if(sf_read_status(info, cmd_tbl, &status) < 0)
        {
            break;
        }else{
            DEBUG(" Serial Flash status = 0x%x\n",status);
            if (!(status & SR_WIP))
            {
                return 0;
            }
        }
    }while( time_out-- );
    printf("Error: Serial FLASH busy\n");
    return 1;
}


static int sf_erase_sector(flash_info_t * info, const sf_cmd_tbl_t* cmd_tbl, unsigned int sect_addr, unsigned int block_erase)
{

    unsigned char cmd[SF_MAX_CMD_SIZE];
    int cmd_size = 0;
#ifdef VALIDATE_ERASE
    int i;
    volatile unsigned char *ptr = NULL;
#endif
    DEBUG("Inside the function sf_erase_sector at sect_addr 0x%08x\n", sect_addr);

    /* Wait until finished previous write command. */
    if( sf_wait_till_ready(info, cmd_tbl) ) {
        DEBUG("sf_wait_till_ready failed in function %s\n",__FUNCTION__);
        return -1;
    }

    /* Send write enable, then erase commands. */
    if( sf_write_enable(info, cmd_tbl)!= 0){
        DEBUG("sf_write_enable failed in function %s\n",__FUNCTION__);
        return -1;
    }

    /* Wait until finished previous write command. */
    if( sf_wait_till_ready(info, cmd_tbl) ) {
        DEBUG("sf_wait_till_ready failed in function %s\n",__FUNCTION__);
        return -1;
    }

    /* Write command and address to buffer */
    cmd_size = sf_cmd2buff(cmd, (block_erase ? cmd_tbl->erase_block : cmd_tbl->erase_sect), sect_addr);
    
    if( sf_write_then_read( info->bank,cmd, cmd_size, NULL, 0) == cmd_size )
    {
#ifdef VALIDATE_ERASE
        /* Validate ERASE */
        ptr =(volatile unsigned char*)(sect_addr + info->start[0]);
        printf("ERASE check from Address 0x%x to 0x%x\n",ptr,(ptr+(info->size/info->sector_count)));
        for(i= 0; i < (info->size/info->sector_count); i++)
        {
            if((*(unsigned char*)(ptr +i))!= 0xFF){
                printf("ERASE IMPROPER Partial Erase at 0x%x\n",(ptr +i));
                return -1;
            }
        }
#endif
        return 0;
    }else{
        DEBUG("sf_write_then_read failed in function %s\n",__FUNCTION__);
        return -1;
    }
}



/*-----------------------------------------------------------------------
 * Copy memory to flash, returns:
 * 0 - OK
 * 1 - write timeout
 * 2 - Flash not erased
 * 4 - Flash not identified
 */

int write_buff (flash_info_t * info, unsigned char * src, ulong addr, ulong cnt)
{
    unsigned int serctor_size = 0,start_sect = 0,end_sect = 0;
    int indx;
    int ret;
    ulong offset;
#ifdef VALIDATE_WRITE
    /* Spansion serial Flash write mechanism */
    volatile unsigned char *ptr = NULL;
    unsigned int i;
#endif

    if (info->flash_id == FLASH_UNKNOWN) {
        return 4;
    }

    if ((indx = sf_get_flindx_from_flid (info->flash_id)) < 0)
    {
        printf ("Error: Missing or unknown FLASH type\n");
        return 4;
    }

        DEBUG("write_buf: src=0x%08X, addr(dest) = 0x%x, cnt=%d\n",src,addr,cnt );

    // Change address to offset in flash
    offset = addr - info->start[0];
    // calculate sector size
    serctor_size = info->size/info->sector_count;
    // calculate the destination first sector number
    start_sect = offset/serctor_size;
    // calculate the destination last sector number
    end_sect   = (offset + cnt-1)/serctor_size;
      
    DEBUG("write_buf:  serctor_size = %d start_sect = %d address 0x%x end_sect = %d address 0x%x \n",
            serctor_size,start_sect,info->start[start_sect],end_sect,info->start[end_sect]);

    DEBUG("write_buf: writing from offset 0x%x to 0x%x\n",offset,offset + cnt);
    /* we are workig im Memory Mapped mode so for actual serial flash in
     * core SPI mode only offset with in flash range is required
     */
  
    if (sf_write(info, &sf_info[indx], offset, src, cnt) == cnt) 
    {
        udelay (100000);
#ifdef VALIDATE_WRITE
        ptr = NULL;
        ptr = (volatile unsigned char*)(addr + info->start[0]);
        DEBUG("Verifying Data written from address 0x%x to 0x%x from Source 0x%x to 0x%x\n",
                ptr,(ptr+cnt),src,(src +cnt));

        for(i = 0;i< cnt; i++)
        {
            if((*(volatile unsigned char*)(ptr +i)) != (*(unsigned char*)(src +i)))
            {
                printf("DATA WRITE MISMATCH at FLash address 0x%x with data 0x%x and Source addr 0x%x with data 0x%x\n",
                        (ptr +i),*(unsigned char*)(ptr+i),(src +i),*(unsigned char*)(src +i));
                return -1;
            }
        }
#endif
        ret = 0;
    }
    else
    {
        ret =  -1;
    }

    return ret;
}

int sf_write_page (flash_info_t * info, const sf_info_t* sf_info_p, unsigned int addr, unsigned char *buf, unsigned int len)
{
    int ret = 0;
    static unsigned char buffer[PAGE_SIZE_256 + SF_MAX_CMD_SIZE];  
    unsigned int page_offset;
    sf_cmd_tbl_t* cmd_tbl = (sf_cmd_tbl_t*)sf_info_p->cmd_tbl;
    int cmd_size = 0;


    page_offset = addr & (sf_info_p->bytes_per_page - 1);

    /* sanity check */
    if (page_offset + len > sf_info_p->bytes_per_page)
    {
        DEBUG("sf_write_page: sanity check failed trying to write out of Page boundary \n");
        return -1;
    }
    
    /* reset buffer */
    memset(buffer,0,sizeof(buffer));

    /* write the next page to flash */
    cmd_size = sf_cmd2buff(buffer,cmd_tbl->prog_page,addr);
    
    memcpy((buffer + cmd_size),buf,len);

    /* Lock the HW Mutex */
    if (hw_mutex_lock(HW_MUTEX_NOR_SPI) == 0)
    {
         printf("HW failed to lock\n");
         return 1;
    }

    if(!ret && sf_wait_till_ready(info, cmd_tbl) )
    {
        printf("sf_write_page: sf_wait_till_ready failed\n");
        ret = -1;
    }

    if(!ret && sf_write_enable(info, cmd_tbl) != 0){
        printf("sf_write_page: sf_write_enable failed in Function %s\n");
        ret = -1;
    }

    if(!ret && sf_wait_till_ready(info, cmd_tbl) )
    {
        printf("sf_write_page: sf_wait_till_ready failed\n");
        ret = -1;
    }

    if (!ret &&  (( sf_write_then_read( info->bank,buffer, (len + cmd_size),NULL, 0)) != (len + cmd_size)))
    {
        printf("sf_write_page: sf_write_then_read failed\n");
    }

    /* Release HW Mutes */
    hw_mutex_unlock(HW_MUTEX_NOR_SPI);

    return len;
}

int sf_write (flash_info_t * info, const sf_info_t* sf_info_p, unsigned int addr, unsigned char *buf, unsigned int len)
{
    
    unsigned long write_size = 0;
    unsigned long progress;
    unsigned int page_offset = addr & (sf_info_p->bytes_per_page - 1);
    int bytes_writen = 0;


    DEBUG("page_offset = %d  FLASH_PAGESIZE = %d \n",page_offset,sf_info_p->bytes_per_page);

    if( (!len) || (( addr + len ) > info->size )){
        printf("Invalid argumnets\n");
        return -1;
    }

    /* Calculate first chunk size */
    if (page_offset + len <= sf_info_p->bytes_per_page)
    {
        write_size = len;
    }
    else
    {
        write_size = sf_info_p->bytes_per_page - page_offset;
    }

    DEBUG("page_offset = %d write_size = %d \n",page_offset,write_size);

    bytes_writen = 0;
    progress = 0;
    while (bytes_writen < len)
    {
        if (sf_write_page(info,sf_info_p,addr+bytes_writen,buf+bytes_writen,write_size)<0)
        {
            printf("sf_write_page: failed\n");
            return -1;
        }

        bytes_writen += write_size;

        /* Calulate next chunk size - if there is less than one page size then reduce the chunk size*/
        if (len - bytes_writen > sf_info_p->bytes_per_page)
            write_size = sf_info_p->bytes_per_page; /* normal page */
        else
            write_size = len - bytes_writen;         /* last page chunk */

        progress++;
        if (progress > 99){  /* 100K bytes with pagesize 256 => 100*(1024/256) */
            printf (".");
            progress = 0;
        }
	}
	DEBUG("write done retlen = %d len = %d \n",retlen,len);
	return bytes_writen;
}


/*
 * sf_get_flindx_from_flid
 *  - Provides index in sf_info structure for the provided flash id. This
 *  function is required since U-Boot global flash_info doesn't have a "private
 *  data " field and we need to traverse to the correct sf_info block to
 *  identify the details about the corresponding serial flash.
 *
 *  Note:  Thould tis is a costly operation, it should not really hamper the
 *  performance as none of the flash APIs in this file are called iterative for
 *  a given flash. For example, flash_erase would be called with start sector to
 *  end sector as arguments, write_buff will be called with a data block.
 */
static int sf_get_flindx_from_flid (unsigned long flash_id)
{
    int i;
    static int last_flash_id = -1; /* Do it once in each run time */

    if( last_flash_id == -1 )
    {
        for (i = 0; i < (sizeof (sf_info) / sizeof (sf_info_t)); i++)
            if (sf_info[i].flash_id == flash_id)
            {
                    last_flash_id = i;
                    break;
            }
    }

    return last_flash_id;
}

int sf_write_then_read( int bank, unsigned char *tx_data, int tx_len, unsigned char *rx_data, int rx_len)
{

	DEBUG("Inside function %s: bank = %d, tx_data = 0x%08X, tx_len = %d,  rx_data = 0x%08X,  rx_len =%d\n",__FUNCTION__, bank, tx_data,tx_len,rx_data,rx_len);
    spi_chipsel[bank](1);
	if((tx_len != 0)  && (tx_data != NULL))
    {
        if(spi_xfer(NULL, (tx_len * 8) /* byte to bit */, tx_data, NULL) != 0 )
		{
			printf("spi_xfer failed in TX \n");
			return -1;
		}
    }

	if((rx_len != 0)  && (rx_data != NULL))
    {
		if (spi_xfer(NULL, (rx_len * 8), NULL, rx_data)!= 0 )
		{
			printf("spi_xfer failed in RX \n");
			return -1;
		}
	}
    spi_chipsel[bank](0);

	return (int)(rx_len+tx_len);
}



static int sf_cmd2buff        (unsigned char* buff, unsigned char cmd, unsigned int addr)
{
    /* opcode */
    buff[0] = cmd;

    /* Address */
    if (spi_addr_width == 4)
    {
        /* 4 byte Addressing mode */
        buff[1] = (char)((addr >> 24) & 0xFF);
        buff[2] = (char)((addr >> 16) & 0xFF);
        buff[3] = (char)((addr >> 8 ) & 0xFF);
        buff[4] = (char)((addr      ) & 0xFF);
    }
    else
    {
        /* 3 byte Addressing Mode*/
        buff[1] = (char)((addr >> 16) & 0xFF);
        buff[2] = (char)((addr >> 8 ) & 0xFF);
        buff[3] = (char)((addr      ) & 0xFF);
    }
    return spi_addr_width + 1;
}

#ifdef FLASH_TEST_CMD
int do_flwr (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
#define MAX_SPI_BYTES 100
    unsigned char dout[MAX_SPI_BYTES];
    unsigned din[MAX_SPI_BYTES];
    int   device;
    int   bytelen_w;
    int   bytelen_r;

	char  *cp = 0;
	unsigned char tmp;
	int   j;
	int   rcode = 0;

    spi_debug = 1;

    // Get device id
    if (argc >= 2)
        device = simple_strtoul(argv[1], NULL, 10);

    // Get number of bytes to write
    if (argc >= 3)
        bytelen_w = simple_strtoul(argv[2], NULL, 10);

    // Get buffer to write
    if (argc >= 4) {
        cp = argv[3];
        for(j = 0; *cp; j++, cp++)
        {
            tmp = *cp - '0';
            if(tmp > 9)
                tmp -= ('A' - '0') - 10;
            if(tmp > 15)
                tmp -= ('a' - 'A');
            if(tmp > 15) {
                printf("Hex conversion error on %c, giving up.\n", *cp);
                return 1;
            }
            if((j % 2) == 0)
                dout[j / 2] = (tmp << 4);
            else
                dout[j / 2] |= tmp;
        }
    }

    // Get number of bytes to read
    if (argc >= 5)
        bytelen_r = simple_strtoul(argv[4], NULL, 10);


	if ((device < 0) || (device >=  2)) {
		printf("Invalid device %d, giving up.\n", device);
		return 1;
	}
	if ((bytelen_w < 0) || (bytelen_w >  MAX_SPI_BYTES)) {
		printf("Invalid bytelen_w %d, giving up.\n", bytelen_w);
		return 1;
	}

    if ((bytelen_r < 0) || (bytelen_r >  MAX_SPI_BYTES)) {
		printf("Invalid bytelen_r %d, giving up.\n", bytelen_r);
		return 1;
	}

    /* Lock the HW Mutex */
    if (hw_mutex_lock(HW_MUTEX_NOR_SPI) == 0)
    {
         printf("HW failed to lock\n");
         return 1;
    }

	if(sf_write_then_read(device, dout, bytelen_w, din, bytelen_r) != bytelen_w + bytelen_r)
    {
		printf("Error with the sf_write_then_read transaction.\n");
		rcode = 1;
	}
    else
    {
		cp = (char *)din;
		for(j = 0; j < bytelen_r ; j++) {
			printf("%02X", *cp++);
		}
		printf("\n");
	}

    /* Release HW Mutes */
    hw_mutex_unlock(HW_MUTEX_NOR_SPI);

    spi_debug = 0;

	return rcode;
}


U_BOOT_CMD(
	flwr,	5,	1,	do_flwr,
	"flwr    - Flash Write and Read utility commands\n",
	"<device> <byte_len_write> <dout> <byte_len_read>- Send <byte_len_write> bytes from <dout> out the Flash\n"
	"<device>  - Identifies the chip select of the device\n"
	"<byte_len_write> - Number of bytess to send (base 10)\n"
	"<dout>    - Hexadecimal string that gets sent\n"
    "<byte_len_read> - Number of bytess to read (base 10)\n"
);


#endif /* FLASH_TEST_CMD */

#endif /* !CFG_FLASH_CFI_DRIVER && !CONFIG_PUMA5_QT_EMU */
