/*
 * (C) Copyright 2008-2011 Freescale Semiconductor, Inc.
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

 /* 
 * Includes Intel Corporation's changes/modifications dated: 2011. 
 * Changed/modified portions - Copyright © 2011 , Intel Corporation.   
 */ 

/* #define DEBUG */

#include <common.h>
#include <command.h>
#include <environment.h>
#include <linux/stddef.h>
#include <malloc.h>
#include <mmc.h>

#ifdef CONFIG_GENERIC_MMC
#ifdef CONFIG_ENV_IS_IN_MMC


/* references to names in env_common.c */
extern uchar default_environment[];
extern int default_environment_size;

char *env_mmc_name_spec = "MMC";

extern env_t *env_ptr;

/* Function that updates CRC of the enironment */
extern void env_crc_update (void);

/* local functions */
#if !defined(ENV_IS_EMBEDDED)
static void use_default(void);
#endif

DECLARE_GLOBAL_DATA_PTR;


#if !defined(CFG_ENV_OFFSET)
#define CFG_ENV_OFFSET 0
#endif

static int __mmc_get_env_addr(struct mmc *mmc, u32 *env_addr)
{
	*env_addr = CFG_ENV_OFFSET;
	return 0;
}
__attribute__((weak, alias("__mmc_get_env_addr")))
int mmc_get_env_addr(struct mmc *mmc, u32 *env_addr);


uchar env_mmc_get_char_spec(int index)
{
	return *((uchar *)(gd->env_addr + index));
}

int env_mmc_init(void)
{
    flash_addr_init(0,0); /* TODO: should be moved to board_init() */

    /* use default */
	gd->env_addr = (ulong)&default_environment[0];
	gd->env_valid = 1;

	return 0;
}

int init_mmc_for_env(struct mmc *mmc)
{
	if (!mmc) {
		puts("No MMC card found\n");
		return -1;
	}

	if (mmc_init(mmc)) {
		puts("MMC init failed\n");
		return  -1;
	}

	return 0;
}

inline int write_env(struct mmc *mmc, unsigned long size,
			unsigned long offset, const void *buffer)
{
	uint blk_start, blk_cnt, n;

    if ((offset & (mmc->write_bl_len-1)) != 0 )
        printf("Warning: write_env: offset [0x%X] is not align to %d \n", offset,mmc->write_bl_len);

    if ((size & (mmc->write_bl_len-1)) != 0 )
        printf("Warning: write_env: size [0x%X] is not align to %d \n", size,mmc->write_bl_len);

	blk_start = ALIGN(offset, mmc->write_bl_len) / mmc->write_bl_len;
	blk_cnt   = ALIGN(size, mmc->write_bl_len) / mmc->write_bl_len;

    n = mmc->block_dev.block_write(CONFIG_SYS_MMC_ENV_DEV, blk_start,
					blk_cnt, (u_char *)buffer);

	return (n == blk_cnt) ? 0 : -1;
}

int env_mmc_saveenv(void)
{
    struct mmc *mmc = find_mmc_device(CONFIG_SYS_MMC_ENV_DEV);
	u32 offset;

	if (init_mmc_for_env(mmc))
		return 1;

	if(mmc_get_env_addr(mmc, &offset))
		return 1;

	printf("Writing to MMC(%d)... \n", CONFIG_SYS_MMC_ENV_DEV);
	if (write_env(mmc, CFG_ENV_SIZE, offset, (u_char *)env_ptr)) {
		puts("failed\n");
		return 1;
	}

	puts("done\n");
	return 0;
}

inline int read_env(struct mmc *mmc, unsigned long size,
			unsigned long offset, const void *buffer)
{
	uint blk_start, blk_cnt, n;

    if ((offset & (mmc->read_bl_len-1)) != 0 )
        printf("Warning: read_env: offset [0x%X] is not align to %d \n", offset,mmc->read_bl_len);

    if ((size & (mmc->read_bl_len-1)) != 0 )
        printf("Warning: read_env: size [0x%X] is not align to %d \n", size,mmc->read_bl_len);

	blk_start = ALIGN(offset, mmc->read_bl_len) / mmc->read_bl_len;
	blk_cnt   = ALIGN(size, mmc->read_bl_len) / mmc->read_bl_len;

	n = mmc->block_dev.block_read(CONFIG_SYS_MMC_ENV_DEV, blk_start,
					blk_cnt, (unsigned long *)buffer);

	return (n == blk_cnt) ? 0 : -1;
}

void env_mmc_relocate_spec(void)
{
#if !defined(ENV_IS_EMBEDDED) 

	struct mmc *mmc = find_mmc_device(CONFIG_SYS_MMC_ENV_DEV);
	u32 offset;

	if (init_mmc_for_env(mmc)) {
        puts ("MMC - Failed to init enc on DDR\n");
		use_default();
		return;
	}

	if(mmc_get_env_addr(mmc, &offset)) {
        puts ("MMC - Failed to get env addess on DDR\n");
		use_default();
		return ;
	}

    if (read_env(mmc, CFG_ENV_SIZE, offset, (const void*)env_ptr)) {
        puts ("MMC - Failed to read from eMMC\n");
		use_default();
		return;
	}

    /* check crc */
    if (env_ptr->crc != crc32(0, env_ptr->data, ENV_SIZE))
    {
        puts ("MMC - Bad CRC\n");
        use_default();
		return;
    }

#endif
}

#if !defined(ENV_IS_EMBEDDED)
static void use_default()
{
	
    puts ("Using default environment\n\n");

    if (default_environment_size > ENV_SIZE)
    {
        puts ("*** Error - default environment is too large\n\n");
        return;
    }

    memset (env_ptr, 0, CFG_ENV_SIZE);
    memcpy (env_ptr->data,default_environment,default_environment_size);

#ifdef CFG_REDUNDAND_ENVIRONMENT
	env_ptr->flags = 0xFF;
#endif

	env_crc_update ();

    gd->env_valid = 1;

}
#endif
#endif /* CONFIG_ENV_IS_IN_MMC*/
#endif /* CONFIG_GENERIC_MMC */
