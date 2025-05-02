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

#include <common.h>
#if !defined CFG_FLASH_CFI_DRIVER && !defined CONFIG_PUMA5_QT_EMU && !defined CFG_NO_FLASH

#include <linux/byteorder/swab.h>

#ifdef CONFIG_SPI_FLASH
#include <spi.h>
#include <spi_sfi.h>
#include <serial_flash.h>
#include <puma5.h>
#endif

#undef DEBUG

#if defined(DEBUG)
#define DEBUG(fmt,arg...)  printf(fmt, ##arg)
#else
#define DEBUG(fmt,arg...)  do {} while (0)
#endif

flash_info_t flash_info[CFG_MAX_FLASH_BANKS];	/* info for FLASH chips    */
unsigned int CFG_FLASH_SECT_SIZE[] = { 0, 0 };
unsigned int CFG_UBOOT_SECT_SIZE = 0;
unsigned int CFG_FLASH_SECT_RESRV[] = { 0, 0 };
unsigned int DETECTED_SFL_DEVICES = 0;

typedef unsigned long flash_sect_t;
typedef unsigned int FLASH_PORT_WIDTH;
typedef volatile unsigned int FLASH_PORT_WIDTHV;

extern void BoostSpiClock( void );

#define FLASH_TYPE_SERIAL

#define SWAP(x)         (x)

/* Intel-compatible flash ID */
#define INTEL_COMPAT    0x89
#define INTEL_ALT       0xB0

/* Intel-compatible flash commands */
#define INTEL_PROGRAM   0x10
#define INTEL_ERASE     0x00200020
#define INTEL_CLEAR     0x00500050
#define INTEL_LOCKBIT   0x00600060
#define INTEL_PROTECT   0x00010001
#define INTEL_STATUS    0x00700070
#define INTEL_READID    0x00900090
#define INTEL_CONFIRM   0x00D000D0
#define INTEL_RESET     0x00FF00FF

/* Intel-compatible flash status bits */
#define INTEL_FINISHED  0x80
#define INTEL_OK        0x80

#define FPW             FLASH_PORT_WIDTH
#define FPWV            FLASH_PORT_WIDTHV

#define FLASH_CYCLE1    0x0555
#define FLASH_CYCLE2    0x02aa
#define WR_BLOCK        0x10


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

static const sf_info_t sf_info[] = {

    /* Spansion Serial Flashes */
    {   FLASH_S25FL004A, MAN_ID(0x01), MEM_TYPE(0x2), MEM_DENS(0x12), EXD_ID(0x00, 0x00),
        FLASH_SIZE(8*64*1024), SECT_CNT(8), SECT_SIZE(64*1024), SECT_PER_BLOCK(1), 0x1C, 256,
        &cmd_tbl[0], "Spansion S25FL064A", 0   },

    {   FLASH_S25FL008A, MAN_ID(0x01), MEM_TYPE(0x2), MEM_DENS(0x13), EXD_ID(0x00, 0x00),
        FLASH_SIZE(1*1024*1024), SECT_CNT(16), SECT_SIZE(64*1024), SECT_PER_BLOCK(1), 0x1C, 256,
        &cmd_tbl[0], "Spansion S25FL008A", 0   },

    {   FLASH_S25FL016A, MAN_ID(0x01), MEM_TYPE(0x2), MEM_DENS(0x14), EXD_ID(0x00, 0x00),
        FLASH_SIZE(2*1024*1024), SECT_CNT(32), SECT_SIZE(64*1024), SECT_PER_BLOCK(1), 0x1C, 256,
        &cmd_tbl[0], "Spansion S25FL016A", 0   },

    {   FLASH_S25FL032A, MAN_ID(0x01), MEM_TYPE(0x2), MEM_DENS(0x15), EXD_ID(0x00, 0x00),
        FLASH_SIZE(4*1024*1024), SECT_CNT(64), SECT_SIZE(64*1024), SECT_PER_BLOCK(1), 0x1C, 256,
        &cmd_tbl[0], "Spansion S25FL032A", 0   },

    {   FLASH_S25FL064A, MAN_ID(0x01), MEM_TYPE(0x2), MEM_DENS(0x16), EXD_ID(0x00, 0x00),
        FLASH_SIZE(8*1024*1024), SECT_CNT(128), SECT_SIZE(64*1024), SECT_PER_BLOCK(1), 0x1C, 256,
        &cmd_tbl[0], "Spansion S25FL064A", 0   },

    {   FLASH_S25FL128P, MAN_ID(0x01), MEM_TYPE(0x20), MEM_DENS(0x18), EXD_ID(0x03, 0x00),
        FLASH_SIZE(16*1024*1024), SECT_CNT(64), SECT_SIZE(256*1024), SECT_PER_BLOCK(1), 0x1C, 256,
        &cmd_tbl[0], "Spansion S25FL128P", 0   },

    {   FLASH_S25FL128P, MAN_ID(0x01), MEM_TYPE(0x20), MEM_DENS(0x18), EXD_ID(0x03, 0x01),
        FLASH_SIZE(16*1024*1024), SECT_CNT(256), SECT_SIZE(64*1024), SECT_PER_BLOCK(1), 0x3C, 256,
        &cmd_tbl[0], "Spansion S25FL128P", 0   },
#ifdef CONFIG_EXTENDED_FLASH
    {   FLASH_S25FL256S, MAN_ID(0x01), MEM_TYPE(0x02), MEM_DENS(0x19), EXD_ID(0x4D, 0x01),
        FLASH_SIZE(32*1024*1024), SECT_CNT(512), SECT_SIZE(64*1024), SECT_PER_BLOCK(1), 0x3C, 256,
        &cmd_tbl[0], "Spansion S25FL256S", 0 },
#endif
	/* ST/Numonyx Serial Flashes */
    {   FLASH_M25P128, MAN_ID(0x20), MEM_TYPE(0x20), MEM_DENS(0x18), EXD_ID(0x00, 0x00),
        FLASH_SIZE(16*1024*1024), SECT_CNT(64), SECT_SIZE(256*1024), SECT_PER_BLOCK(1), 0x3C, 256,
        &cmd_tbl[0], "Numonyx M25P128", 0  },
    {   FLASH_M25P64, MAN_ID(0x20), MEM_TYPE(0x20), MEM_DENS(0x17), EXD_ID(0x10, 0x00),
        FLASH_SIZE(8*1024*1024), SECT_CNT(128), SECT_SIZE(64*1024), SECT_PER_BLOCK(1), 0x3C, 256,
        &cmd_tbl[0], "Numonyx M25P64", 0 },
#ifdef CONFIG_EXTENDED_FLASH
    {   FLASH_N25Q256, MAN_ID(0x20), MEM_TYPE(0xBA), MEM_DENS(0x19), EXD_ID(0x10, 0x00),
        FLASH_SIZE(32*1024*1024), SECT_CNT(512), SECT_SIZE(64*1024), SECT_PER_BLOCK(1), 0x3C, 256,
        &cmd_tbl[0], "Numonyx N25Q256", 0 },
#endif
	/* Macronix Serial Flashes */
    {   FLASH_MX25l6405D, MAN_ID(0xC2), MEM_TYPE(0x20), MEM_DENS(0x17), EXD_ID(0xC2, 0x20),
        FLASH_SIZE(8*1024*1024), SECT_CNT(2048), SECT_SIZE(4*1024), SECT_PER_BLOCK(16), 0x3C, 256,
        &cmd_tbl[1], "Macronix MX25l6405D", 0  },
    {   FLASH_MX25l12805D, MAN_ID(0xC2), MEM_TYPE(0x20), MEM_DENS(0x18), EXD_ID(0xC2, 0x20),
        FLASH_SIZE(16*1024*1024), SECT_CNT(4096), SECT_SIZE(4*1024), SECT_PER_BLOCK(16), 0x3C, 256,
        &cmd_tbl[1], "Macronix MX25l12805D", 0  },
    /* Winbond Serial Flashes */
    {   FLASH_W25Q64BV, MAN_ID(0xEF), MEM_TYPE(0x40), MEM_DENS(0x17), EXD_ID(0x0, 0x0),
        FLASH_SIZE(8*1024*1024), SECT_CNT(2048), SECT_SIZE(4*1024), SECT_PER_BLOCK(16), 0x3C, 256,
        &cmd_tbl[1], "Winbond W25Q64BV",
        FLASH_FEATURE_DUAL_FAST_READ | FLASH_FEATURE_80MHZ_CLOCK },
    {   FLASH_W25Q128BV, MAN_ID(0xEF), MEM_TYPE(0x40), MEM_DENS(0x18), EXD_ID(0x0, 0x0),
        FLASH_SIZE(16*1024*1024), SECT_CNT(4096), SECT_SIZE(4*1024), SECT_PER_BLOCK(16), 0x3C, 256,
        &cmd_tbl[1], "Winbond W25Q128BV", 
        FLASH_FEATURE_DUAL_FAST_READ | FLASH_FEATURE_80MHZ_CLOCK },
    {   FLASH_N25Q128, MAN_ID(0x20), MEM_TYPE(0xBA), MEM_DENS(0x18), EXD_ID(0x10, 0x1),
        FLASH_SIZE(16*1024*1024), SECT_CNT(256), SECT_SIZE(64*1024), SECT_PER_BLOCK(1), 0x3C, 256,
        &cmd_tbl[0], "Numonyx N25Q128",
        FLASH_FEATURE_DUAL_FAST_READ | FLASH_FEATURE_80MHZ_CLOCK },
    {   FLASH_25VF64, MAN_ID(0xBF), MEM_TYPE(0x25), MEM_DENS(0x4B), EXD_ID(0xBF, 0x25),
        FLASH_SIZE(8*1024*1024), SECT_CNT(2048), SECT_SIZE(4*1024), SECT_PER_BLOCK(16), 0x3C, 256,
        &cmd_tbl[1], "SST 25VF064C",
        FLASH_FEATURE_DUAL_FAST_READ },
};

/*
 * _get_flindx_from_flid
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
static int _get_flindx_from_flid (unsigned long flash_id)
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

#ifdef CONFIG_SPI_FLASH
#undef VALIDATE_ERASE
#undef VALIDATE_WRITE
static int  sf_match_flash_id( int bank, int verbose );
static int sf_get_info(flash_info_t * flash_info, int bank, int verbose );
static int sf_write_enable (flash_info_t * info, sf_cmd_tbl_t* cmd_tbl);
static int sf_write_status (flash_info_t * info, sf_cmd_tbl_t* cmd_tbl, u8 val);
static int sf_read_status (flash_info_t * info, sf_cmd_tbl_t* cmd_tbl, uchar *status);
static int sf_wait_till_ready (flash_info_t * info, sf_cmd_tbl_t* cmd_tbl);
static int sf_erase_sector (flash_info_t * info, sf_cmd_tbl_t* cmd_tbl, uint sect_addr, uint block_erase);
static int sf_unlock (flash_info_t * info, sf_cmd_tbl_t* cmd_tbl, uint start_addr,uint end_addr);
static unsigned char sf_sector_protected (flash_info_t *info, sf_cmd_tbl_t* cmd_tbl, ushort sector);

int sf_write (flash_info_t * info, sf_info_t* sf_info_p, uint addr, uchar *buf, uint len);
void flash_read_jedec_ids (flash_info_t * info, sf_cmd_tbl_t* cmd_tbl);
#endif /* CONFIG_SPI_FLASH */

static int serial_flash_init;

unsigned long SF_ID[] = { 0xFFFF, 0xFFFF };
unsigned int SF_SIZE[] = { 0, 0 };

/*-----------------------------------------------------------------------
 * Functions
 */
#ifndef CONFIG_SPI_FLASH
static ulong flash_get_size (FPW * addr, flash_info_t * info);
static int write_data (flash_info_t * info, ulong dest, FPW data);
static int write_data_block (flash_info_t * info, ulong src, ulong dest);
static unsigned char intel_sector_protected (flash_info_t *info, ushort sector);
static unsigned char same_chip_banks (int bank1, int bank2);
static void flash_get_offsets (ulong base, flash_info_t * info);
#endif
static void flash_sync_real_protect (flash_info_t * info);
void inline spin_wheel (void);

/*-----------------------------------------------------------------------
 */

#ifdef CONFIG_SPI_FLASH
int flash_addr_init(int i, int verbose)
{
    int indx;

    if ((indx = sf_get_info(&flash_info[i], i, verbose) ) < 0)
    {
        flash_info[i].flash_id = FLASH_UNKNOWN;

        if( i == 0 )
        {
            /* Put some default values which are wrong - avoid crashing */
            CFG_FLASH_SECT_SIZE[ i ] = 0x10000; /* 64K */
            CFG_UBOOT_SECT_SIZE = 0x20000; /* 128K */
            CFG_FLASH_SECT_RESRV[ i ] = 6;
        }

        return -1;
    }

    /* Initialize sector size */
    CFG_FLASH_SECT_SIZE[ i ] = sf_info[indx].sector_size;

    /* Calculate the u-boot partition size */
    while( CFG_UBOOT_SECT_SIZE < CFG_UBOOTBIN_SIZE )
    {
        CFG_UBOOT_SECT_SIZE += CFG_FLASH_SECT_SIZE[ i ];
    }

    /*
     * The default flash layout is to have 5 sectors of 64KB or 64 sectors of 4KB
     * reserved for JFFS2 partition at the end of flash.
     * In addition, 1 sector is reserved in case the flash sector size is 64KB.
     */
    if( CFG_FLASH_SECT_SIZE[ i ] == 0x10000 )
    {
        /* Reserve 6 sectors */
        CFG_FLASH_SECT_RESRV[ i ] = 5+1;
    }
    else if( CFG_FLASH_SECT_SIZE[ i ] > 0x10000 )
    {
        /* Reserve 5 sectors (Minimum for JFFS2) */
        CFG_FLASH_SECT_RESRV[ i ] = 5;
    }
    else
    {
        /* Reserve 64 sectors */
        CFG_FLASH_SECT_RESRV[ i ] = 64;
    }

    return indx;
}
#endif

unsigned long flash_init (void)
{
    int i;
    ulong size = 0;
    DEBUG("Inside function flash_init \n");
    serial_flash_init = 0;
#ifdef CONFIG_SPI_FLASH
    int indx;
#endif

#ifdef CONFIG_PUMA5_VOLCANO_EMU
#if (CONFIG_COMMANDS & CFG_CMD_NET)
    extern int lava_nogig (void);
    lava_nogig (); /*!@@@*/
#endif
#endif /* CONFIG_PUMA5_VOLCANO_EMU */

    for (i = 0; i < CFG_MAX_FLASH_BANKS; i++)
    {
        memset (&flash_info[i], 0, sizeof (flash_info_t));

        DEBUG("Inside function flash_init before switch \n");
#ifndef CONFIG_SPI_FLASH
        flash_get_size ((FPW *) CFG_FLASH0_BASE,&flash_info[i]);
        flash_get_offsets (CFG_FLASH0_BASE, &flash_info[i]);
#else
        indx = flash_addr_init(i,1);

        if(indx < 0)
        {
            if( i == 0 )
            {
                /* Flash not supported, return size 0 */
                return 0;
            }

            /* We found a single flash platform */
            break;
        }
        else
        {
            if( i > 0 )
            {
                /* No need to reserve sectors on the first flash device */
                CFG_FLASH_SECT_RESRV[ 0 ] = 0;
            }
        }

        /* Update detected devices */
        DETECTED_SFL_DEVICES++;

        /* Unlock the whole flash */
        if( sf_unlock(&flash_info[i], (sf_cmd_tbl_t *)sf_info[indx].cmd_tbl, 0x0000, flash_info[i].size)!= 0 ){
            printf("Error: Flash not Unlocked \n");
                }
#endif /* CONFIG_SPI_FLASH */

        size += flash_info[i].size;

        /* get the h/w and s/w protection status in sync */
        flash_sync_real_protect(&flash_info[i]);
    }

    /* Protect monitor and environment sectors. They are always in the first device
    */
    flash_protect (FLAG_PROTECT_SET,
            CFG_FLASH1_BASE,
            CFG_FLASH1_BASE + monitor_flash_len - 1,
            &flash_info[0]);
    flash_protect (FLAG_PROTECT_SET,
            CFG_ENV_ADDR,
            CFG_ENV_ADDR + CFG_ENV_SIZE - 1,
            &flash_info[0]);
#ifdef CFG_ENV_ADDR_REDUND
    flash_protect (FLAG_PROTECT_SET,
            CFG_ENV_ADDR_REDUND,
            CFG_ENV_ADDR_REDUND + CFG_ENV_SIZE_REDUND - 1,
            &flash_info[0]);
#endif
    return size;
}


static int  sf_match_flash_id( int bank, int verbose )
{
    int ret = -1, i;
    /* TODO: Add support to traverse through complete command table for the
     * case where READ_ID command from preceding command table (currently -
     * first table only) failed because the device didn't support that
     * command.
     */
    unsigned char cmd = cmd_tbl[0].read_id;

    unsigned char  flash_id[5]={0x00,0x00,0x00,0x00,0x00};


    ret = spi_write_then_read( bank, &cmd,1,(unsigned char*)&flash_id,5);

    if(ret < 0)
    {
        if(verbose)
            printf("Error: Serial Flash Read JEDEC Failed\n");
        return -1;
    }
    if(verbose)
        debug("\n Read Flash ID  0x%x  0x%x  0x%x\n",flash_id[0],flash_id[1],flash_id[2]);


    /* Check if second flash is not installed */
    if( ( flash_id[0] == MAN_ID(0xFF) ) && ( bank > 0 ) )
    {
        return -1;
    }

    for (i = 0; i < (sizeof (sf_info) / sizeof (sf_info_t)); i++)
    {
        if ((flash_id[0] == sf_info[i].manf_id)
                && (flash_id[1] == sf_info[i].mem_type)
                && (flash_id[2] == sf_info[i].mem_density)
                && (flash_id[3] == sf_info[i].extended_id[0])
                && (flash_id[4] == sf_info[i].extended_id[1]))
        {
            if(verbose)
                printf ("%s flash found\n", sf_info[i].flash_name);

            /* Enable Fast Dual-Read for supporting devices */
            if( FLASH_SUPPORTS( FLASH_FEATURE_DUAL_FAST_READ ) ) {
                unsigned int *mm_spi = (unsigned int *)REG_MM_SPI_SETUP0;
                *mm_spi = MM_SPI_SETUP0_DFR_VAL;

                mm_spi = (unsigned int *)REG_MM_SPI_SETUP1;
                *mm_spi = MM_SPI_SETUP1_DFR_VAL;
            }

#ifdef CONFIG_ENABLE_SPI_CLOCK_BOOST
            /* Boost SPI clock for supporting devices */
            if( FLASH_SUPPORTS( FLASH_FEATURE_80MHZ_CLOCK ) ) {
                /* Boost SPI clock to 80MHz for better performance */
                BoostSpiClock();
            }
#endif                

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


static int sf_get_info(flash_info_t * flash_info, int bank, int verbose )
{
	int i;
 	int id = -1;
    unsigned long base = bank == 0 ? CFG_FLASH1_BASE : CFG_FLASH2_BASE;

	/* Read Spansion Flash Table to get info*/
	if ((id = sf_match_flash_id(bank, verbose)) < 0)
            return -1;
    if(verbose)
    {
        debug( "Found Flash ID %d\n",id);

        debug("\nID = 0x%x\nFLASH SIZE =0x%x\n NUM_SECTORS = %d\n SECTOR_SIZE = 0x%x\n",
                  sf_info[id].flash_id,
                              sf_info[id].flash_size,sf_info[id].num_sectors,
                              sf_info[id].sector_size);
    }

	SF_ID[bank] = sf_info[id].flash_id;
	SF_SIZE[bank] = sf_info[id].flash_size;

	flash_info->flash_id = sf_info[id].flash_id;
    flash_info->sector_count = sf_info[id].num_sectors;
	flash_info->size =  sf_info[id].flash_size;
    flash_info->bank = bank;

	/* Base addres of the flash start */
	for (i = 0; i < flash_info->sector_count; i++)
	{
            flash_info->start[i] = base;
            base += sf_info[id].sector_size;
	}

	return id;
}

/*-----------------------------------------------------------------------
 */
#ifndef CONFIG_SPI_FLASH
static void flash_get_offsets (ulong base, flash_info_t * info)
{
	int i;
	if (info->flash_id == FLASH_UNKNOWN)
		return;

	if ((info->flash_id & FLASH_VENDMASK) == FLASH_MAN_INTEL) {
		for (i = 0; i < info->sector_count; i++) {
			info->start[i] = base + (i * CFG_FLASH0_SECT_SIZE);
		}
	}
}
#endif
/*-----------------------------------------------------------------------
 */
void flash_print_info (flash_info_t * info)
{
	int i,j;

	if (info->flash_id == FLASH_UNKNOWN) {
		printf ("Error: Missing or unknown FLASH type\n");
		return;
	}

#ifndef CONFIG_SPI_FLASH
	switch (info->flash_id & FLASH_VENDMASK) {
        case FLASH_MAN_INTEL:
		printf ("INTEL ");
		break;
        default:
		break;
	}

	switch (info->flash_id & FLASH_TYPEMASK) {
        case FLASH_28F128J3A:
		printf ("28F128J3A\n");
		break;
	default:
		printf ("Unknown Chip Type\n");
		break;
	}
#else
        if ((i = _get_flindx_from_flid (info->flash_id)) >= 0)
            printf ("%s\n", sf_info[i].flash_name);
        else {
            printf ("Error: Missing or unknown FLASH type\n");
            return;
        }
#endif

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


#ifndef CONFIG_SPI_FLASH
/*
 * The following code cannot be run from FLASH!
 */
static ulong flash_get_size (FPW * addr, flash_info_t * info)
{
	FPWV value;

#define mb() __asm__ __volatile__ ("" : : : "memory")

	addr[FLASH_CYCLE1] = (FPW) 0x00900090;	/* selects Intel or AMD */
        mb();

	udelay (100);

	switch (addr[0] & 0xff) {

	case (uchar) INTEL_MANUFACT:
		info->flash_id = FLASH_MAN_INTEL;
		value = addr[1];
		break;

	default:
		printf ("***WARNING: Flash manufacturer unknown (%#x)\n", addr[0] & 0xff);
                info->flash_id = FLASH_MAN_INTEL; //!@@@ FLASH_UNKNOWN;
		value = INTEL_ID_28F128J3A;
	}
	switch (value) {

	case (FPW) INTEL_ID_28F128J3A:
		info->flash_id += FLASH_28F128J3A;
		info->sector_count = 128;
		info->size = 128*2*1024 * 128;	/* => 16 MB     */
		break;

	default:
		printf ("***WARNING: Flash id unknown (%#x)\n", value);
		info->flash_id = FLASH_UNKNOWN;
		break;
	}

	if (info->sector_count > CFG_MAX_FLASH_SECT) {
		printf ("***ERROR: sector count %d > max (%d) **\n",
			info->sector_count, CFG_MAX_FLASH_SECT);
		info->sector_count = CFG_MAX_FLASH_SECT;
	}

	if (value == (FPW) INTEL_ID_28F128J3A)
		addr[0] = (FPW) 0x00FF00FF;	/* restore read mode */
	else
		addr[0] = (FPW) 0x00F000F0;	/* restore read mode */

	return (info->size);
}
#endif /* !CONFIG_SPI_FLASH */

#ifdef CONFIG_SPI_FLASH
/*
 * The following code cannot be run from FLASH!
 */
static unsigned char sf_sector_protected(flash_info_t *info, sf_cmd_tbl_t* cmd_tbl, ushort sector)
{
    /* Initially Flash is unlocked */
    if( serial_flash_init == 0 ){
        return 0;
    }else{
        return info->protect[sector];
    }

}
#endif /* CONFIG_SPI_FLASH */

/*
 * This function gets the u-boot flash sector protection status
 * (flash_info_t.protect[]) in sync with the sector protection
 * status stored in hardware.
 */
static void flash_sync_real_protect (flash_info_t * info)
{
	int i;

#ifndef CONFIG_SPI_FLASH
	switch (info->flash_id & FLASH_TYPEMASK) {
	case FLASH_28F128J3A:
		for (i = 0; i < info->sector_count; ++i) {
			info->protect[i] = intel_sector_protected(info, i);
		}
		break;
	default:
		/* no h/w protect support */
		break;
	}
#else
        int indx;
        if ((indx = _get_flindx_from_flid (info->flash_id)) < 0)
        {
            printf ("Error: Missing or unknown FLASH type\n");
            return;
        }

        for(i = 0; i < info->sector_count; ++i){
            info->protect[i] = sf_sector_protected(info, (sf_cmd_tbl_t *)(sf_info[indx].cmd_tbl), i);
        }
        serial_flash_init++;
#endif
}

#ifndef CONFIG_SPI_FLASH
/*
 * checks if "sector" in bank "info" is protected. Should work on intel
 * strata flash chips 28FxxxJ3x in 8-bit mode.
 * Returns 1 if sector is protected (or timed-out while trying to read
 * protection status), 0 if it is not.
 */
static unsigned char intel_sector_protected (flash_info_t *info, ushort sector)
{
	FPWV *addr;
	FPWV *lock_conf_addr;
	ulong start;
	unsigned char ret;

	/*
	 * first, wait for the WSM to be finished. The rationale for
	 * waiting for the WSM to become idle for at most
	 * CFG_FLASH_ERASE_TOUT is as follows. The WSM can be busy
	 * because of: (1) erase, (2) program or (3) lock bit
	 * configuration. So we just wait for the longest timeout of
	 * the (1)-(3), i.e. the erase timeout.
	 */

	/* wait at least 35ns (W12) before issuing Read Status Register */
	udelay(1);
	addr = (FPWV *) info->start[sector];
	*addr = (FPW) INTEL_STATUS;

	start = get_timer (0);
	while ((*addr & (FPW) INTEL_FINISHED) != (FPW) INTEL_FINISHED) {
		if (get_timer (start) > CFG_FLASH_ERASE_TOUT) {
			*addr = (FPW) INTEL_RESET; /* restore read mode */
			printf("WSM busy too long, can't get prot status\n");
			return 1;
		}
	}

	/* issue the Read Identifier Codes command */
	*addr = (FPW) INTEL_READID;

	/* wait at least 35ns (W12) before reading */
	udelay(1);

	/* Intel example code uses offset of 4 for 8-bit flash */
	lock_conf_addr = (FPWV *) info->start[sector] + 4;
	ret = (*lock_conf_addr & (FPW) INTEL_PROTECT) ? 1 : 0;

	/* put flash back in read mode */
	*addr = (FPW) INTEL_RESET;

	return ret;
}


/*
 * Checks if "bank1" and "bank2" are on the same chip.  Returns 1 if they
 * are and 0 otherwise.
 */
static unsigned char same_chip_banks (int bank1, int bank2)
{
	unsigned char same_chip[CFG_MAX_FLASH_BANKS][CFG_MAX_FLASH_BANKS] = {
		{1, 0},
		{0, 1}
	};
	return same_chip[bank1][bank2];
}
#else /* CONFIG_SPI_FLASH */

/*
 * write_status_reg writes the vlaue to status reg.
 * Returns status on success or negetive valueif error.
 * NOTE:  write enable should be called before calling this function.
 */
static int sf_write_status (flash_info_t * info, sf_cmd_tbl_t* cmd_tbl, u8 val)
{
    u8 write_val[2] = {cmd_tbl->write_sts, val};
    DEBUG("Inside the function %s\n",__FUNCTION__);

    if(spi_write_then_read( info->bank, &write_val[0],sizeof(write_val),NULL,0) !=
            sizeof(write_val) )
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
static int sf_write_enable(flash_info_t * info, sf_cmd_tbl_t* cmd_tbl)
{
    u8 code = cmd_tbl->write_en;
    DEBUG("Inside the function %s\n",__FUNCTION__);
    if( spi_write_then_read( info->bank,&code,1,NULL,0) == 1)
    {
        return 0;
    }
    else{
        DEBUG("spi_write_then_read failed in function %s\n", __FUNCTION__ );
        return -1;
    }
}


static int sf_unlock (flash_info_t* info, sf_cmd_tbl_t* cmd_tbl, uint start_addr, uint end_addr)
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
static int sf_read_status (flash_info_t * info, sf_cmd_tbl_t* cmd_tbl, uchar *status)
{
    u8 cmd = cmd_tbl->read_sts;

    if( status != NULL ) {
        *status =0;
    }else{
        DEBUG("In function %s Invalid argument\n",__FUNCTION__);
        goto read_fail;
    }

    DEBUG("Inside the function %s\n",__FUNCTION__);

    if( spi_write_then_read( info->bank,&cmd, 1, status,1) == 2 ) {
        DEBUG(" SPANSION SATAUS = 0x%x\n",*status);
        return 0;
    }else{
        DEBUG("spi_write_then_read failed in function %s\n",__FUNCTION__);
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
static int sf_wait_till_ready(flash_info_t * info, sf_cmd_tbl_t* cmd_tbl)
{
    u8 status = 0;
    u64 time_out = SERIAL_FLASH_TIMEOUT;

    DEBUG("Inside the function %s\n",__FUNCTION__);
    do{
        if(sf_read_status(info, cmd_tbl, &status) < 0)
        {
            break;
        }else{
            DEBUG(" Serial Flash  satus = 0x%x\n",status);
            if (!(status & SR_WIP))
            {
                return 0;
            }
        }
    }while( time_out-- );
    printf("Error: Serial FLASH busy\n");
    return 1;
}


static int sf_erase_sector(flash_info_t * info, sf_cmd_tbl_t* cmd_tbl, uint sect_addr, uint block_erase)
{

    unsigned char cmd[SF_CMD_SIZE];
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

    cmd[0] = block_erase ? cmd_tbl->erase_block : cmd_tbl->erase_sect;
    cmd[1] = sect_addr >> 16;
    cmd[2] = sect_addr >> 8;
    cmd[3] = sect_addr;

    if( spi_write_then_read( info->bank,cmd, SF_CMD_SIZE, NULL, 0) == SF_CMD_SIZE ){
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
        DEBUG("spi_write_then_read failed in function %s\n",__FUNCTION__);
        return -1;
    }
}
#endif /* CONFIG_SPI_FLASH */

/*-----------------------------------------------------------------------
 */
int flash_erase (flash_info_t * info, int s_first, int s_last)
{
#ifndef CONFIG_SPI_FLASH
	int flag, prot, sect;
	ulong type, start, last;
	int rcode = 0, intel = 0;
#else
    int indx;
    sf_cmd_tbl_t* cmd_tbl;
#endif
        int i;

        if ((indx = _get_flindx_from_flid (info->flash_id)) < 0)
        {
            printf ("Error: Missing or unknown FLASH type\n");
            return 1;
        }
        cmd_tbl = (sf_cmd_tbl_t*)sf_info[indx].cmd_tbl;


        uint sect_addr = 0;

	if ((s_first < 0) || (s_first > s_last) ||(s_last > info->sector_count)) {
		if (info->flash_id == FLASH_UNKNOWN)
			printf ("- missing\n");
		else
			printf ("- no sectors to erase\n");
		return 1;
	}

#ifdef CONFIG_SPI_FLASH
		DEBUG(" Inside function %s\n",__FUNCTION__);

		/* Sync up real protect is already called */
		for(i = s_first ; i<= s_last; i++ )
		{
            int block_erase = 0;

			sect_addr= info->start[i];
#ifdef CONFIG_EXTENDED_FLASH
            int switch_res;
            DEBUG("%s %d info->start[i]=0x%x\n", __FUNCTION__, __LINE__, sect_addr);
            switch_res = process_flash_bank(info, sect_addr, 0, NULL, 1);
            if (switch_res == EMM_HIGH)
                sect_addr -= FLASH_BORDER_OFFSET_16MB;  
            DEBUG("%s %d (res=%d) info->start[i]=0x%x\n", __FUNCTION__, __LINE__, switch_res, sect_addr);  
#endif
			/* Addr calculated in Memory Mapped mode, Consider only offset with in
			  the flash address range thst is 0x00000 to 0x800000 */
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

                if (sf_erase_sector(info, cmd_tbl, sect_addr, block_erase) < 0 ) {
                    printf(" sf_erase_sector failed for sector = %d address = 0x%x\n",
                            i,info->start[i] );
                    return -1;
                }else {
					DEBUG("Erasing sector %d at address 0x%x........ done\n",i,info->start[i]);
                                        printf (".");
				}
			}else {
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
#else /* !CONFIG_SPI_FLASH */
	{

	/* Parallel Flash Erase code */
	type = (info->flash_id & FLASH_VENDMASK);
	if ((type != FLASH_MAN_INTEL)) {
                printf ("Can't erase unknown flash type %08lx - aborted\n",
                        info->flash_id);
                return 1;
	}

	if (type == FLASH_MAN_INTEL)
		intel = 1;

	prot = 0;
	for (sect = s_first; sect <= s_last; ++sect) {
		if (info->protect[sect]) {
			prot++;
		}
	}

	if (prot) {
		printf ("- Warning: %d protected sectors will not be erased!\n", prot);
	} else {
		printf ("\n");
	}

	start = get_timer (0);
	last = start;

	/* Disable interrupts which might cause a timeout here */
	flag = disable_interrupts ();

	/* Start erase on unprotected sectors */
	for (sect = s_first; sect <= s_last; sect++) {
		if (info->protect[sect] == 0) {	/* not protected */
			FPWV *addr = (FPWV *) (info->start[sect]);
			FPW status;

			printf ("Erasing sector %2d ... ", sect);

			/* arm simple, non interrupt dependent timer */
			start = get_timer (0);

			if (intel) {
				*addr = (FPW) 0x00500050;	/* clear status register */
				*addr = (FPW) 0x00200020;	/* erase setup */
				*addr = (FPW) 0x00D000D0;	/* erase confirm */
			} else {
                            /* Not reached at this time */
			}

			while (((status =
				 *addr) & (FPW) 0x00800080) !=
			       (FPW) 0x00800080) {
				if (get_timer (start) > CFG_FLASH_ERASE_TOUT) {
					printf ("Timeout\n");
					if (intel) {
						*addr = (FPW) 0x00B000B0;	/* suspend erase     */
						*addr = (FPW) 0x00FF00FF;	/* reset to read mode */
					} else
						*addr = (FPW) 0x00F000F0;	/* reset to read mode */

					rcode = 1;
					break;
				}
			}

			if (intel) {
				*addr = (FPW) 0x00500050;	/* clear status register cmd.   */
				*addr = (FPW) 0x00FF00FF;	/* resest to read mode          */
			} else
				*addr = (FPW) 0x00F000F0;	/* reset to read mode */

			printf (" done\n");
		}
	}
	return rcode;
	}
#endif /* !CONFIG_SPI_FLASH */
}


/*-----------------------------------------------------------------------
 * Copy memory to flash, returns:
 * 0 - OK
 * 1 - write timeout
 * 2 - Flash not erased
 * 4 - Flash not identified
 */

int write_buff (flash_info_t * info, uchar * src, ulong addr, ulong cnt)
{
    uint sector_size = 0,start_sect = 0,end_sect = 0;
#ifdef CONFIG_SPI_FLASH
    int indx;
#ifdef VALIDATE_WRITE
    uint i;
#endif
#endif

    if (info->flash_id == FLASH_UNKNOWN) {
        return 4;
    }

#ifdef CONFIG_SPI_FLASH
    /* Spansion serial Flash write mechanism */
#ifdef VALIDATE_WRITE
    volatile unsigned char *ptr = NULL;
#endif
    if ((indx = _get_flindx_from_flid (info->flash_id)) < 0)
    {
        printf ("Error: Missing or unknown FLASH type\n");
        return 4;
    }

        DEBUG("The address received in write_buf is addr = 0x%x\n",addr);

        addr -= info->start[0];
        sector_size = info->size/info->sector_count;
        start_sect = addr/sector_size;
        end_sect   = (addr + cnt)/sector_size;
        DEBUG(" sector_size = %d start_sect = %d address 0x%x end_sect = %d address 0x%x \n",
                sector_size,start_sect,info->start[start_sect],end_sect,info->start[end_sect]);
        DEBUG("writing from 0x%x to 0x%x\n",addr,addr + cnt);
        /* we are workig im Memory Mapped mode so for actual serial flash in
         * core SPI mode only offset with in flash range is required
         */
        DEBUG("Before calling sf_write addr = 0x%x\n",addr);
        if (sf_write(info, (sf_info_t *)&sf_info[indx], addr, src, cnt) == cnt) {
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
            return 0;
        }else {
            return -1;
        }

#else /* ! CONFIG_SPI_FLASH */
    switch (info->flash_id & FLASH_VENDMASK) {
        case FLASH_MAN_INTEL:
            {
                ulong cp, wp;
                FPW data;
                int count, i, l, rc, port_width;

                /* get lower word aligned address */
                port_width = sizeof (FPW);
                wp = addr & ~(port_width - 1);

                /*
                 * handle unaligned start bytes
                 */
                if ((l = addr - wp) != 0) {
                    data = 0;
                    for (i = 0, cp = wp; i < l; ++i, ++cp) {
                        data = (data << 8) | (*(uchar *) cp);
                    }

                    for (; i < port_width && cnt > 0; ++i) {
                        data = (data << 8) | *src++;
                        --cnt;
                        ++cp;
                    }

                    for (; cnt == 0 && i < port_width; ++i, ++cp)
                        data = (data << 8) | (*(uchar *) cp);

                    if ((rc =
                                write_data (info, wp, SWAP (data))) != 0)
                        return (rc);
                    wp += port_width;
                }

                if (cnt > WR_BLOCK*port_width) {
                    /*
                     * handle word aligned part
                     */
                    count = 0;
                    while (cnt >= WR_BLOCK*port_width) {

                        if ((rc =
                                    write_data_block (info,
                                        (ulong) src,
                                        wp)) != 0)
                            return (rc);

                        wp = (unsigned long)(FPWV *)wp + WR_BLOCK;
                        src = (unsigned char *)(FPWV *)src + WR_BLOCK;
                        cnt = cnt - WR_BLOCK*port_width;

                        if (count++ > 0x800) {
                            spin_wheel ();
                            count = 0;
                        }
                    }
                }

                if (cnt < WR_BLOCK*port_width) {
                    /*
                     * handle word aligned part
                     */
                    count = 0;
                    while (cnt >= port_width) {
                        data = 0;
                        for (i = 0; i < port_width; ++i)
                            data = (data << 8) | *src++;

                        if ((rc =
                                    write_data (info, wp,
                                        SWAP (data))) != 0)
                            return (rc);

                        wp += port_width;
                        cnt -= port_width;
                        if (count++ > 0x800) {
                            spin_wheel ();
                            count = 0;
                        }
                    }
                }

                if (cnt == 0)
                    return (0);

                /*
                 * handle unaligned tail bytes
                 */
                data = 0;
                for (i = 0, cp = wp; i < port_width && cnt > 0;
                        ++i, ++cp) {
                    data = (data << 8) | *src++;
                    --cnt;
                }

                for (; i < port_width; ++i, ++cp)
                    data = (data << 8) | (*(uchar *) cp);

                return (write_data (info, wp, SWAP (data)));
            }		/* case FLASH_MAN_INTEL */

    }			/* switch */
    return (0);

#endif /* !CONFIG_SPI_FLASH */
}

#ifdef CONFIG_SPI_FLASH

int sf_write (flash_info_t * info, sf_info_t* sf_info_p, uint addr, uchar *buf, uint len)
{
#define GET_PAGESIZE_FROM_SFINFO(p)         (p->bytes_per_page)
    unsigned char buffer[300];
    sf_cmd_tbl_t* cmd_tbl = (sf_cmd_tbl_t*)sf_info_p->cmd_tbl;
    unsigned long page_size = 0 ,i =0, progress;
    unsigned int page_offset = addr & ( GET_PAGESIZE_FROM_SFINFO(sf_info_p) - 1 );
    int retlen = 0;

    DEBUG("page_offset = %d  FLASH_PAGESIZE = %d \n",
            page_offset,GET_PAGESIZE_FROM_SFINFO(sf_info_p));

    if( (!len) || (( addr + len ) > info->size )){
        printf("Invalid argumnets\n");
        return retlen;
    }
	/* Wait till previous write/erase is done. */
	if( sf_wait_till_ready(info, cmd_tbl) )
	{
		DEBUG("Function %s Line %d sf_wait_till_ready failed\n",
				__FUNCTION__, __LINE__);
		return retlen;
	}

    if(sf_write_enable(info, cmd_tbl) != 0){
		DEBUG(" sf_write_enable failed in Function %s\n",
				__FUNCTION__);
		return retlen;
	}
	/* Wait till previous write/erase is done. */
	if( sf_wait_till_ready(info, cmd_tbl) )
	{
		DEBUG("Function %s Line %d sf_wait_till_ready failed\n",
				__FUNCTION__, __LINE__);
		return retlen;
	}

	if(page_offset + len <= GET_PAGESIZE_FROM_SFINFO(sf_info_p))
	{
		DEBUG("page_offset = %d\n",page_offset);

		memset(buffer,0,sizeof(buffer));

		DEBUG("Writing with in a Single page\n");

		buffer[0] = cmd_tbl->prog_page;
	    buffer[1] = addr >> 16;
   		buffer[2] = addr >> 8;
	    buffer[3] = addr;
		memcpy((buffer + SF_CMD_SIZE), buf, len);


		retlen = spi_write_then_read( info->bank,buffer,(len + SF_CMD_SIZE), NULL, 0);
		if( retlen != (len + SF_CMD_SIZE) ){
			DEBUG("In Function %s spi_write_then_read failed returned %d\n",
					__FUNCTION__,retlen);
			return 0;
		}else{
			return len;
		}

	}else{

		DEBUG("Spawning multiple pages\n");

		page_size = GET_PAGESIZE_FROM_SFINFO(sf_info_p) - page_offset;

		/* Write to the starting page data */
		memset(buffer,0,sizeof(buffer));
		buffer[0] = cmd_tbl->prog_page;
	    buffer[1] = addr >> 16;
   		buffer[2] = addr >> 8;
	    buffer[3] = addr;
		/* Writing in to First Page */
		page_size = GET_PAGESIZE_FROM_SFINFO(sf_info_p) - page_offset;
		DEBUG("page_offset = %d page_size = %d \n",page_offset,page_size);

		memcpy((buffer + SF_CMD_SIZE), buf, page_size);
		retlen =  spi_write_then_read( info->bank,buffer,(SF_CMD_SIZE + page_size), NULL, 0);

		if( retlen != (SF_CMD_SIZE + page_size)){
			DEBUG("In Function %s spi_write_then_read failed returned %d\n",
					__FUNCTION__,retlen);
			return 0;
		}else{
			retlen = page_size;
		}

		/* write everything in PAGESIZE chunks */
		 for(i = page_size, progress = 0;  i < len ; i += page_size, progress++)
         {
            page_size = len - i;

            if (page_size > GET_PAGESIZE_FROM_SFINFO(sf_info_p))
                page_size = GET_PAGESIZE_FROM_SFINFO(sf_info_p);

			//DEBUG("before writing page_size = %d\n",page_size);

			memset(buffer,0,sizeof(buffer));
            /* write the next page to flash */
            buffer[0] = cmd_tbl->prog_page;
            buffer[1] = (addr + i) >> 16;
            buffer[2] = (addr + i) >> 8;
	        buffer[3] = (addr + i);

			memcpy((buffer + SF_CMD_SIZE),(buf + i),page_size);

            if( sf_wait_till_ready(info, cmd_tbl) )
			{
				DEBUG("Function %s Line %d sf_wait_till_ready failed\n",
					__FUNCTION__, __LINE__);
				return retlen;
			}

		    if(sf_write_enable(info, cmd_tbl) != 0){
				DEBUG(" sf_write_enable failed in Function %s\n",__FUNCTION__);
				return -1;
			}

			if( sf_wait_till_ready(info, cmd_tbl) )
			{
				DEBUG("Function %s Line %d sf_wait_till_ready failed\n",
					__FUNCTION__, __LINE__);
				return retlen;
			}

			if ( ( spi_write_then_read( info->bank,buffer, (page_size + SF_CMD_SIZE),
				 NULL, 0)) != (page_size + SF_CMD_SIZE))
			{
				printf(" spi_write_then_read failed  Partial write done retlen = %d len = %d\n",retlen,len);
				return retlen;
			}else {
				retlen += page_size;
			}

            if (progress > 99){  /* 100K bytes with pagesize 256 => 100*(1024/256) */
                printf (".");
                progress = 0;
            }
        }
                 printf ("\n");
	}
	DEBUG(" write done retlen = %d len = %d \n",retlen,len);
	return retlen;
}

void flash_read_jedec_ids (flash_info_t * info, sf_cmd_tbl_t* cmd_tbl)
{
    info->flash_id = 0xFFFF;
    unsigned char cmd = cmd_tbl->read_id;

    if( spi_write_then_read( info->bank,&cmd,1,(unsigned char*)info->flash_id,
                sizeof(info->flash_id)) != (1 + sizeof(info->flash_id)))
    {
        printf("Serial Flash  Read JEDEC Failed \n");
    }
}

#else /* !CONFIG_SPI_FLASH */
/*-----------------------------------------------------------------------
 * Write a word or halfword to Flash, returns:
 * 0 - OK
 * 1 - write timeout
 * 2 - Flash not erased
 */
static int write_data (flash_info_t * info, ulong dest, FPW data)
{
	FPWV *addr = (FPWV *) dest;
	ulong start;
	int flag;

	/* Check if Flash is (sufficiently) erased */
	if ((*addr & data) != data) {
                printf ("Error: Flash is not erased at %08lx (%lx)\n", (ulong)
                        addr, *addr); return (2);
	}
	/* Disable interrupts which might cause a timeout here */
	flag = disable_interrupts ();

	*addr = (FPW) 0x00400040;	/* write setup */
	*addr = data;

	/* arm simple, non interrupt dependent timer */
	start = get_timer (0);

	/* wait while polling the status register */
	while ((*addr & (FPW) 0x00800080) != (FPW) 0x00800080) {
		if (get_timer (start) > CFG_FLASH_WRITE_TOUT) {
			*addr = (FPW) 0x00FF00FF;	/* restore read mode */
			return (1);
		}
	}

	*addr = (FPW) 0x00FF00FF;	/* restore read mode */

	return (0);
}

/*-----------------------------------------------------------------------
 * Write a word or halfword to Flash, returns:
 * 0 - OK
 * 1 - write timeout
 * 2 - Flash not erased
 */
static int write_data_block (flash_info_t * info, ulong src, ulong dest)
{
	FPWV *srcaddr = (FPWV *) src;
	FPWV *dstaddr = (FPWV *) dest;
	ulong start;
	int flag, i;

	/* Check if Flash is (sufficiently) erased */
	for (i = 0; i < WR_BLOCK; i++)
		if ((*dstaddr++ & 0xff) != 0xff) {
            "Error: Flash is not erased at %08lx (%lx)\n",
                (ulong) dstaddr, *dstaddr);
			return (2);
		}

	dstaddr = (FPWV *) dest;

	/* Disable interrupts which might cause a timeout here */
	flag = disable_interrupts ();

	*dstaddr = (FPW) 0x00e800e8;	/* write block setup */

	/* arm simple, non interrupt dependent timer */
	start = get_timer (0);

	/* wait while polling the status register */
	while ((*dstaddr & (FPW) 0x00800080) != (FPW) 0x00800080) {
		if (get_timer (start) > CFG_FLASH_WRITE_TOUT) {
			*dstaddr = (FPW) 0x00FF00FF;	/* restore read mode */
			return (1);
		}
	}

	*dstaddr = (FPW) 0x000f000f;	/* write 16 words */
	for (i = 0; i < WR_BLOCK; i++)
		*dstaddr++ = *srcaddr++;

	dstaddr -= 1;
	*dstaddr = (FPW) 0x00d000d0;

	/* arm simple, non interrupt dependent timer */
	start = get_timer (0);

	/* wait while polling the status register */
	while ((*dstaddr & (FPW) 0x00800080) != (FPW) 0x00800080) {
		if (get_timer (start) > CFG_FLASH_WRITE_TOUT) {
			*dstaddr = (FPW) 0x00FF00FF;	/* restore read mode */
			return (1);
		}
	}

	*dstaddr = (FPW) 0x00FF00FF;	/* restore read mode */

	return (0);
}


void inline spin_wheel (void)
{
	static int p = 0;
	static char w[] = "\\/-";

	printf ("\010%c", w[p]);
	(++p == 3) ? (p = 0) : 0;
}

#endif /* !CONFIG_SPI_FLASH */


/*-----------------------------------------------------------------------
 * Set/Clear sector's lock bit, returns:
 * 0 - OK
 * 1 - Error (timeout, voltage problems, etc.)
 */
int flash_real_protect (flash_info_t * info, long sector, int prot)
{
#ifndef CONFIG_SPI_FLASH
    ulong start;
    int i, j;
    int curr_bank;
    int bank;
    int flag = disable_interrupts ();
#else
    int indx;
#endif

    int rc = 0;

#ifdef  CFG_FLASH_PROTECTION
    int max_sect = 0;
    ulong end;
    uint sector_size = info->size/info->sector_count;
#endif

#ifdef CONFIG_SPI_FLASH
    if ((indx = _get_flindx_from_flid (info->flash_id)) < 0)
        return 1;

    if( sector <= info->sector_count)
    {
        info->protect[sector] = prot;
    }else{
        printf("Error: Sector amount %d exceding FLASH maximum sectors %d",
                sector,info->sector_count);
        rc = 1;
    }
    return rc;

#else /* !CONFIG_SPI_FLASH */

    FPWV *addr = (FPWV *) (info->start[sector]);
    *addr = INTEL_CLEAR;	/* Clear status register    */
    if (prot) {		/* Set sector lock bit      */
        *addr = INTEL_LOCKBIT;	/* Sector lock bit          */
        *addr = INTEL_PROTECT;	/* set                      */
    } else {		/* Clear sector lock bit    */
        *addr = INTEL_LOCKBIT;	/* All sectors lock bits    */
        *addr = INTEL_CONFIRM;	/* clear                    */
    }

    start = get_timer (0);

    while ((*addr & INTEL_FINISHED) != INTEL_FINISHED) {
        if (get_timer (start) > CFG_FLASH_UNLOCK_TOUT) {
            printf ("Flash lock bit operation timed out\n");
            rc = 1;
            break;
        }
    }

    if (*addr != INTEL_OK) {
        printf ("Flash lock bit operation failed at %08X, CSR=%08X\n",
                (uint) addr, (uint) * addr);
        rc = 1;
    }

    if (!rc)
        info->protect[sector] = prot;

    /*
     * Clear lock bit command clears all sectors lock bits, so
     * we have to restore lock bits of protected sectors.
     */
    if (!prot) {
        /*
         * re-locking must be done for all banks that belong on one
         * FLASH chip, as all the sectors on the chip were unlocked
         * by INTEL_LOCKBIT/INTEL_CONFIRM commands. (let's hope
         * that banks never span chips, in particular chips which
         * support h/w protection differently).
         */

        /* find the current bank number */
        curr_bank = CFG_MAX_FLASH_BANKS + 1;
        for (j = 0; j < CFG_MAX_FLASH_BANKS; ++j) {
            if (&flash_info[j] == info) {
                curr_bank = j;
            }
        }
        if (curr_bank == CFG_MAX_FLASH_BANKS + 1) {
            printf("Error: can't determine bank number!\n");
        }

        for (bank = 0; bank < CFG_MAX_FLASH_BANKS; ++bank) {
            if (!same_chip_banks(curr_bank, bank)) {
                continue;
            }
            info = &flash_info[bank];
            for (i = 0; i < info->sector_count; i++) {
                if (info->protect[i]) {
                    start = get_timer (0);
                    addr = (FPWV *) (info->start[i]);
                    *addr = INTEL_LOCKBIT;	/* Sector lock bit  */
                    *addr = INTEL_PROTECT;	/* set              */
                    while ((*addr & INTEL_FINISHED) !=
                            INTEL_FINISHED) {
                        if (get_timer (start) >
                                CFG_FLASH_UNLOCK_TOUT) {
                            printf ("Error: Flash lock bit operation timed out\n");
                            rc = 1;
                            break;
                        }
                    }
                }
            }
        }

        /*
         * get the s/w sector protection status in sync with the h/w,
         * in case something went wrong during the re-locking.
         */
        flash_sync_real_protect(info); /* resets flash to read  mode */
    }

    if (flag)
        enable_interrupts ();

    *addr = INTEL_RESET;	/* Reset to read array mode */

    return rc;
#endif /* !CONFIG_SPI_FLASH */
}

#ifdef CONFIG_EXTENDED_FLASH

extern uchar serial_was_init;

/* 0 - when used addresses below 16MB */
/* 1 - when used addresses above 16MB */
static int ext_mem_mode[CFG_MAX_FLASH_BANKS] = {0}; 

/* Numonyx */
#define NUMONYX_MAN_ID          0x20

#define NUMONYX_WREN            0x06 /* Write enable */ 
#define NUMONYX_WRNVCR          0xB1 /* Write non-volatile configuration reg. */
#define NUMONYX_RDNVCR          0xB5 /* Read non-volatile configuration reg. */
#define NUMONYX_RDSR            0x05 /* Read status register */
#define NUMONYX_RDEAR           0xC8 /* Read extended address register */
#define NUMONYX_WREAR           0xC5 /* Write extended address register */


int switch_numonyx_bank(flash_info_t *flash_info, sf_info_t* sf_info_p, 
                                                uchar mode, int verbose)
{
    int ret = 0;
    uchar rdear_cmd = NUMONYX_RDEAR, rdear_val[] = {0, 0};
    sf_cmd_tbl_t* cmd_tbl = (sf_cmd_tbl_t*)sf_info_p->cmd_tbl;

    if (serial_was_init)
        DEBUG("%s %d bank=%d mode=%d\n", __FUNCTION__, __LINE__, 
                                         flash_info->bank, mode);

    /* wait for flash need in case when we want to switch beetwen erase or 
    write operations */
    sf_wait_till_ready(flash_info, cmd_tbl); 

#if 0
    uchar rdsr_cmd = NUMONYX_RDSR, rdsr_val[] = {0, 0};    
    ret = spi_write_then_read(flash_info->bank, &rdsr_cmd, 1, (uchar*)&rdsr_val, 1);
    if (serial_was_init)
        DEBUG("Read status register rdsr=0x%x (ret=%d)\n", rdsr_val[0], ret);
#endif


    ret = spi_write_then_read( flash_info->bank, &rdear_cmd, 1, (uchar*)&rdear_val, 2);
    if (serial_was_init)
        DEBUG("Current flash bank is %s (EAR: 0x%x)\n",
        (rdear_val[0] & 0x01) ? "high" : "low", rdear_val[0]);

    if ((rdear_val[0] & 0x01) == (uchar)mode)
    {
        if (rdear_val[0] > 0x1)
        {
            if (serial_was_init)
                printf("error: Read wrong value of EAR (0x%x)\n", rdear_val[0]); 
 
            return -EMM_ERR; 
        }   
  
        /* 
            It means that somewhere Extended memory mode was reset,
            but flash mode not switched!
        */
        if (serial_was_init)
            printf(" error: Nothing to switch!\n [Extended memory mode and "
                "Bank Register are equal (%x)]\n", mode);
        return EMM_OK;
    }

    uchar mode_val = {mode ? 1 : 0};
    uchar cmds[] = { NUMONYX_WREN, NUMONYX_WREAR, mode_val };

    /* Write enable */
    ret = spi_write_then_read( flash_info->bank, &cmds[0], 1, NULL, 0);
    /* Write EAR*/
    ret = spi_write_then_read( flash_info->bank, &cmds[1], sizeof(cmds) - 1, NULL, 0);
    /* Read EAR*/
    ret = spi_write_then_read( flash_info->bank, &rdear_cmd, 1, (uchar*)&rdear_val, 2);
   
    sf_wait_till_ready(flash_info, cmd_tbl);

    /* Read EAR*/
    ret = spi_write_then_read( flash_info->bank, &rdear_cmd, 1, (uchar*)&rdear_val, 2);

    if (rdear_val[0] != (uchar)mode)
    {
        /* 
            It means that somewhere Extended memory mode was reset,
            but flash mode not switched!
        */
        if (serial_was_init)
            printf("error: Flash mode not switched!\nNow it still %x %x\n", 
                                                    rdear_val[0], rdear_val[1]);
        return -EMM_ERR;
    }

    if (serial_was_init)
        DEBUG("Numonyx flash switched to %s (EAR: 0x%x)\n", 
                    (rdear_val[0] & 0x01) ? "high" : "low", rdear_val[0]);

    return EMM_OK; 
}


/* Macronix - not supported */
#define MICRON_MAN_ID 0xC2

int switch_micron_bank(flash_info_t *flash_info, sf_info_t* sf_info_p, 
                                                uchar mode, int verbose)
{
    return -EMM_ERR;
}


/* Spansion */
#define SPANSION_MAN_ID 0x01

#define SPANSION_BRAC               0xB9    /* Bank Register access */
#define SPANSION_WRTR               0x01    /* Write register       */
#define SPANSION_BRRD               0x16    /* Bank Register read   */
#define SPANSION_BRWR               0x17    /* Bank Register write  */


int switch_spansion_bank(flash_info_t *flash_info, sf_info_t* sf_info_p, 
                                                uchar mode, int verbose)
{
    int ret = -1;
    uchar cmds[] = {SPANSION_BRAC, SPANSION_WRTR, mode, SPANSION_BRRD};
    uchar brrd_cmd = SPANSION_BRRD, brrd_val;
    sf_cmd_tbl_t* cmd_tbl = (sf_cmd_tbl_t*)sf_info_p->cmd_tbl;

    if (serial_was_init)
        DEBUG("%s bank=%d mode=%d\n", __FUNCTION__, flash_info->bank, mode);

    /* wait for flash need in case when we want to switch beetwen erase or 
    write operations */
    sf_wait_till_ready(flash_info, cmd_tbl); 

    ret = spi_write_then_read( flash_info->bank, &brrd_cmd, 1, &brrd_val, 1);

    if (serial_was_init)
        DEBUG("(1) received byte from BRRD : 0x%x\n", brrd_val);

    if ((brrd_val  & 0x01) == (uchar)mode)
    {
        if (brrd_val > 0x1)
        {
            if (serial_was_init)
                printf("error: Read wrong value of BRRD (0x%x)\n", brrd_val); 
 
            return -EMM_ERR; 
        }   
        /* 
            It means that somewhere Extended memory mode was reset,
            but flash mode not switched!
        */
        if (serial_was_init)
            printf(" error: Nothing to switch!\n [Extended memory mode and "
                "Bank Register are equal (%x)]\n", mode);
        return EMM_OK;
    }

    /* Write Bank register phase 1 */
    ret = spi_write_then_read( flash_info->bank, &cmds[0], sizeof(cmds), NULL, 0);
    
    sf_wait_till_ready(flash_info, cmd_tbl);
    
    cmds[0] = SPANSION_BRWR;
    cmds[1] = mode;
    /*Write Bank register phase 2 */
    ret = spi_write_then_read( flash_info->bank, &cmds[0], 2, NULL, 0);

    /* Read Bank register new value*/
    ret = spi_write_then_read( flash_info->bank, &brrd_cmd, 1, &brrd_val, 1);

    if (serial_was_init)
         DEBUG("(2) received byte from BRRD : 0x%x\n", brrd_val);

    if (brrd_val != (uchar)mode)
    {
        /* 
            It means that somewhere Extended memory mode was reset,
            but flash mode not switched!
        */
        if (serial_was_init)
            printf("error: Flash mode not switched!\nNow it still %x\n", brrd_val);
        return -EMM_ERR;
    }
    if (serial_was_init)
        DEBUG("Spansion flash switched to %s area\n", mode ? "high" : "low");

    return EMM_OK;
}


/* Winbond - not supported */
#define WINBOND_MAN_ID 0xEF

#define WINBOND_READ_EAR            0xC8 /* Read Extended address register */
#define WINBOND_WRITE_EAR           0xC5 /* Write Extended address register */


int switch_winbond_bank(flash_info_t *flash_info, sf_info_t* sf_info_p, uchar mode, int verbose)
{
    /*DEBUG("switch_winbond_addr \n");
    uchar status;
    u8 cmd = WINBOND_READ_EAR;
    
    spi_write_then_read(bank, &cmd, 1, &status, 1);

    u8 write_val[2] = {WINBOND_WRITE_EAR, mode}; 
    spi_write_then_read(bank,  &write_val[0], sizeof(write_val), &status, 1); */
    return -EMM_ERR;
} 



/* **************************************************************************
 *
 *  Function:    process_flash_bank
 *
 *  Description: checks what address used and switches bank if necessary 
 *				 
 *  params: IN: 
 *          flash_info_t *info, 
 *          ulong addr,     
 *          ulong len,
 *          int verbose - not used, 
 *          ulong *proc_len,         
 *  return:	non-negative value which represent current bank on success,
 *          otherwise negative. In proc_len returned actual len within current 
 *          bank (if len === proc_len, it means that whole block is in single 
 *          flash bank low or high)	
 *              
 *
 * *********************************************************************** */
int process_flash_bank(flash_info_t *info, ulong addr, ulong len, 
                                    ulong *proc_len, int verbose)
{
    int test, res = EMM_OK;
    flash_info_t *flash_info;

    if (serial_was_init)   
        DEBUG("\n%s %d addr=0x%x len=%lu\n", __FUNCTION__, __LINE__, addr, len);

    /*Get inforamtion about our flash*/
    if (info)
        flash_info = info;    
    else
    {
	    flash_info = addr2info (addr);
    }   
    
    /* Determine what the bank will be used: low or high and what is actual len
       within this bank (in case of using both banks */
    test = test_flash_bank(flash_info, addr, len, proc_len, verbose);

    if (test == EMM_LOW)
    {
        /* If current bank is not low */
        if (ext_mem_mode[flash_info->bank] != EMM_LOW_ADDRESS)
        {
            /* Switch it*/
            res = switch_flash_bank(flash_info, EMM_LOW_ADDRESS, verbose);

            if (res == EMM_OK)
            {
                if (serial_was_init)
                    DEBUG("%s %d switched to %s area\n", 
                        __FUNCTION__, __LINE__, 
                        ext_mem_mode[flash_info->bank] ? "high" : "low");
            }
            else
            {
                if (serial_was_init)
                    printf("ERROR: %s %d not switched for add=0x%x len=%lu\n",  
                                            __FUNCTION__, __LINE__, addr, len);
            }
        }
    }
    else if (test == EMM_HIGH)
    {
        /* If current bank is not high */
        if (ext_mem_mode[flash_info->bank] != EMM_HIGH_ADDRESS)
        {
            res = switch_flash_bank(flash_info, EMM_HIGH_ADDRESS, verbose);
            if (res == EMM_OK)
            {
                if (serial_was_init)
                    DEBUG("%s %d switched to %s area\n", 
                        __FUNCTION__, __LINE__, 
                        ext_mem_mode[flash_info->bank] ? "high" : "low");
            }
            else
            {
                if (serial_was_init)
                    printf("ERROR: %s %d not switched for add=0x%x len=%lu\n",  
                                            __FUNCTION__, __LINE__, addr, len);
            }
        }
    }
    else if (test == EMM_NOT_FL)
    {
        return test;
    }
    else
    {
        if (serial_was_init)
            printf("ERROR: %s %d test_flash_bank return err %d\n",  
                                    __FUNCTION__, __LINE__, test);
    }
  
    return res == EMM_OK ? test: res;
}
/* **************************************************************************
 *
 *  Function:    test_flash_bank
 *
 *  Description: return bank mode (low or high) for data block begins from addr
 *               @addr with lenght @len.           
 *				 
 *  params: IN:
 *          flash_info_t *info, 
 *          ulong addr, 
 *          ulong len,
 *          int verbose - not used
 *          OUT:
 *          ulong *proc_len 
 *  return:	non-negative value which represent current bank on success,
 *          otherwise negative. In proc_len returned actual len within current 
 *          bank
 *              
 *
 * *********************************************************************** */
int test_flash_bank(flash_info_t *info, ulong addr, ulong len, ulong *proc_len,
                                                                    int verbose)
{
    flash_info_t *flash_info;

    if (info)
        flash_info = info;    
    else
	    flash_info = addr2info (addr);

    if (!flash_info)
        return EMM_NOT_FL;

    /*If whole address space above 16MB border*/
    if (addr >= flash_info->start[0] + FLASH_BORDER_OFFSET_16MB)
    {
        if (proc_len)
            *proc_len = len;
        if (serial_was_init)
            DEBUG("need high. addr=0x%x start=0x%x\n", addr, flash_info->start[0]);

        return EMM_HIGH;        
    }
    /*If whole address space below 16MB border*/
    else if ((addr >= flash_info->start[0]) && 
        (addr + len < flash_info->start[0] + FLASH_BORDER_OFFSET_16MB))
    {
        if (proc_len)
            *proc_len = len;
        if (serial_was_init)
            DEBUG("need low. addr=0x%x start=0x%x\n", addr, flash_info->start[0]);

        return EMM_LOW;
    }
    /* If address space begins below 16MB border and ends above it */
    else
    {
        if (proc_len)
            *proc_len = flash_info->start[0] + FLASH_BORDER_OFFSET_16MB - addr;
        if (serial_was_init)
            DEBUG("need low (partial)\n");

        return EMM_LOW;               
    }
    return -EMM_ERR;
}

/* **************************************************************************
 *
 *  Function:    switch_flash_bank
 *
 *  Description: switch flash to high or low bank 
 *				 
 *  params: 
 *      flash_info_t *flash_info, 
 *      uchar mode,
 *      int verbose - not used
 *  return:	0 if success, nigative in case of error	
 *              
 *
 * *********************************************************************** */
int switch_flash_bank(flash_info_t *flash_info, uchar mode, int verbose)
{
    
    int res = EMM_OK;   
    int idx;
    if (serial_was_init)
        DEBUG("\n%s %d switch to mode=%d\n", __FUNCTION__, __LINE__, mode);
    
    if (!flash_info)
        return -EMM_ERR; 
   
    idx = _get_flindx_from_flid(flash_info->flash_id);
    if (serial_was_init)
        DEBUG("Flash id %d,  manuf id 0x%x\n",idx, sf_info[idx].manf_id);

    switch (sf_info[idx].manf_id)
    {
        case WINBOND_MAN_ID:
            res = -EMM_ERR;
            /* switch_winbond_bank */
            break;
        case SPANSION_MAN_ID: 
            res = switch_spansion_bank(flash_info, (sf_info_t *)&sf_info[idx],
                                                            mode, verbose);  
            break;
        case NUMONYX_MAN_ID: 
            res = switch_numonyx_bank(flash_info,  (sf_info_t *)&sf_info[idx], 
                                                            mode, verbose);
            break;
        case MICRON_MAN_ID:
            res = -EMM_ERR;
            /* switch_micron_bank */
            break;
        default:
            if (serial_was_init)
                printf("ERROR: Undefined flash\n");
            return -EMM_ERR;
    }
    if (serial_was_init)
        DEBUG("%s %d switch res=%d\n", __FUNCTION__, __LINE__, res);

    if (res == EMM_OK)
    {
        if (serial_was_init)
            DEBUG("ext_mem_mode[%d] value changed from %x to %x\n", 
                    flash_info->bank, ext_mem_mode[flash_info->bank], mode);
        /* Save new mode */
        ext_mem_mode[flash_info->bank] = mode;
    }

    return res;
}
#endif /* CONFIG_EXTENDED_FLASH */

#endif /* !CFG_FLASH_CFI_DRIVER && !CONFIG_PUMA5_QT_EMU */
