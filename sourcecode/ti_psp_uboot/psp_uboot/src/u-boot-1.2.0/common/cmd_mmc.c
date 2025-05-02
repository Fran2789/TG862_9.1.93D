/*
 * (C) Copyright 2003
 * Kyle Harris, kharris@nexus-tech.net
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
 * Changed/modified portions - Copyright © 2011 - 2012, Intel Corporation.   
 */ 

#include <common.h>
#include <command.h>


#ifdef CONFIG_GENERIC_MMC
#if (CONFIG_COMMANDS & CFG_CMD_MMC)

#include <mmc.h>

/* #define CONFIG_MMC_ERASE_ENABLE */

static int curr_device = -1;

enum mmc_state {
	MMC_INVALID,
	MMC_READ,
	MMC_WRITE,
	MMC_ERASE,
};


/* Dump EXT_CSD register */
static void dump_ext_csd(struct mmc *mmc)
{
    printf("EXT_CSD Register: \n");
    
    printf("ext_csd[504] 'Supported Command Sets'........................................... [0x%0.2X] \n",mmc->ext_csd[504]);
    printf("ext_csd[503] 'HPI features' .................................................... [0x%0.2X] \n",mmc->ext_csd[503]);
    printf("ext_csd[502] 'Background operations support' ................................... [0x%0.2X] \n",mmc->ext_csd[502]);
    printf("ext_csd[246] 'Background operations status' .................................... [0x%0.2X] \n",mmc->ext_csd[246]);
    printf("ext_csd[245:242] 'Number of correctly programmed sectors' ...................... [0x%0.8X] \n",(((int)mmc->ext_csd[245])<<24)+(((int)mmc->ext_csd[244])<<16)+(((int)mmc->ext_csd[243])<<8)+mmc->ext_csd[242]);
    printf("ext_csd[241] '1st initialization time after partitioning' ...................... [0x%0.2X] \n",mmc->ext_csd[241]);
    printf("ext_csd[239] 'Power class for 52MHz, DDR at 3.6V' .............................. [0x%0.2X] \n",mmc->ext_csd[239]);
    printf("ext_csd[238] 'Power class for 52MHz, DDR at 1.95V' ............................. [0x%0.2X] \n",mmc->ext_csd[238]);
    printf("ext_csd[235] 'Minimum Write Performance for 8bit at 52MHz in DDR mode' ......... [0x%0.2X] \n",mmc->ext_csd[235]);
    printf("ext_csd[234] 'Minimum Read Performance for 8bit at 52MHz in DDR mode' .......... [0x%0.2X] \n",mmc->ext_csd[234]);
    printf("ext_csd[232] 'TRIM Multiplier' ................................................. [0x%0.2X] \n",mmc->ext_csd[232]);
    printf("ext_csd[231] 'Secure Feature support' .......................................... [0x%0.2X] \n",mmc->ext_csd[231]);
    printf("ext_csd[230] 'Secure Erase Multiplier' ......................................... [0x%0.2X] \n",mmc->ext_csd[230]);
    printf("ext_csd[229] 'Secure TRIM Multiplier' .......................................... [0x%0.2X] \n",mmc->ext_csd[229]);
    printf("ext_csd[228] 'Boot information' ................................................ [0x%0.2X] \n",mmc->ext_csd[228]);
    printf("ext_csd[226] 'Boot partition size' ............................................. [0x%0.2X] \n",mmc->ext_csd[226]);
    printf("ext_csd[225] 'Access size' ..................................................... [0x%0.2X] \n",mmc->ext_csd[225]);
    printf("ext_csd[224] 'High-capacity erase unit size' ................................... [0x%0.2X] \n",mmc->ext_csd[224]);
    printf("ext_csd[223] 'High-capacity erase timeout' ..................................... [0x%0.2X] \n",mmc->ext_csd[223]);
    printf("ext_csd[222] 'Reliable write sector count' ..................................... [0x%0.2X] \n",mmc->ext_csd[222]);
    printf("ext_csd[221] 'High-capacity write protect group size' .......................... [0x%0.2X] \n",mmc->ext_csd[221]);
    printf("ext_csd[220] 'Sleep current (VCC)' ............................................. [0x%0.2X] \n",mmc->ext_csd[220]);
    printf("ext_csd[219] 'Sleep current (VCCQ)' ............................................ [0x%0.2X] \n",mmc->ext_csd[219]);
    printf("ext_csd[217] 'Sleep/awake timeout' ............................................. [0x%0.2X] \n",mmc->ext_csd[217]);
    printf("ext_csd[215:212] 'Sector Count' ................................................ [0x%0.8X] \n",(((int)mmc->ext_csd[245])<<24)+(((int)mmc->ext_csd[244])<<16)+(((int)mmc->ext_csd[243])<<8)+mmc->ext_csd[242]);
    printf("ext_csd[210] 'Minimum Write Performance for 8bit at 52MHz' ..................... [0x%0.2X] \n",mmc->ext_csd[210]);
    printf("ext_csd[209] 'Minimum Read Performance for 8bit at 52MHz' ...................... [0x%0.2X] \n",mmc->ext_csd[209]);
    printf("ext_csd[208] 'Minimum Write Performance for 8bit at 26MHz, for 4bit at 52MHz' .. [0x%0.2X] \n",mmc->ext_csd[208]);
    printf("ext_csd[207] 'Minimum Read Performance for 8bit at 26MHz, for 4bit at 52MHz' ... [0x%0.2X] \n",mmc->ext_csd[207]);
    printf("ext_csd[206] 'Minimum Write Performance for 4bit at 26MHz' ..................... [0x%0.2X] \n",mmc->ext_csd[206]);
    printf("ext_csd[205] 'Minimum Read Performance for 4bit at 26MHz' ...................... [0x%0.2X] \n",mmc->ext_csd[205]);
    printf("ext_csd[203] 'Power class for 26MHz at 3.6V' ................................... [0x%0.2X] \n",mmc->ext_csd[203]);
    printf("ext_csd[202] 'Power class for 52MHz at 3.6V' ................................... [0x%0.2X] \n",mmc->ext_csd[202]);
    printf("ext_csd[201] 'Power class for 26MHz at 1.95V' .................................. [0x%0.2X] \n",mmc->ext_csd[201]);
    printf("ext_csd[200] 'Power class for 52MHz at 1.95V' .................................. [0x%0.2X] \n",mmc->ext_csd[200]);
    printf("ext_csd[199] 'Partition switching timing' ...................................... [0x%0.2X] \n",mmc->ext_csd[199]);
    printf("ext_csd[198] 'Out-of-interrupt busy timing' .................................... [0x%0.2X] \n",mmc->ext_csd[198]);
    printf("ext_csd[196] 'Card type' ....................................................... [0x%0.2X] \n",mmc->ext_csd[196]);
    printf("ext_csd[194] 'CSD structure version' ........................................... [0x%0.2X] \n",mmc->ext_csd[194]);
    printf("ext_csd[192] 'Extended CSD revision' ........................................... [0x%0.2X] \n",mmc->ext_csd[192]);
    printf("ext_csd[191] 'Modes Segment Command set' ....................................... [0x%0.2X] \n",mmc->ext_csd[191]);
    printf("ext_csd[189] 'Command set revision' ............................................ [0x%0.2X] \n",mmc->ext_csd[189]);
    printf("ext_csd[187] 'Power class POWER_CLASS' ......................................... [0x%0.2X] \n",mmc->ext_csd[187]);
    printf("ext_csd[185] 'High-speed interface timing' ..................................... [0x%0.2X] \n",mmc->ext_csd[185]);
    printf("ext_csd[181] 'Erased memory content' ........................................... [0x%0.2X] \n",mmc->ext_csd[181]);
    printf("ext_csd[179] 'Partition configuration' ......................................... [0x%0.2X] \n",mmc->ext_csd[179]);
    printf("ext_csd[178] 'Boot config protection' .......................................... [0x%0.2X] \n",mmc->ext_csd[178]);
    printf("ext_csd[177] 'Boot bus width' .................................................. [0x%0.2X] \n",mmc->ext_csd[177]);
    printf("ext_csd[175] 'High-density erase group definition' ............................. [0x%0.2X] \n",mmc->ext_csd[175]);
    printf("ext_csd[173] 'Boot area write protection register' ............................. [0x%0.2X] \n",mmc->ext_csd[173]);
    printf("ext_csd[171] 'User area write protection register' ............................. [0x%0.2X] \n",mmc->ext_csd[171]);
    printf("ext_csd[169] 'FW configuration' ................................................ [0x%0.2X] \n",mmc->ext_csd[169]);
    printf("ext_csd[168] 'RPMB Size' ....................................................... [0x%0.2X] \n",mmc->ext_csd[168]);
    printf("ext_csd[167] 'Write reliability setting register' .............................. [0x%0.2X] \n",mmc->ext_csd[167]);
    printf("ext_csd[166] 'Write reliability parameter register' ............................ [0x%0.2X] \n",mmc->ext_csd[166]);
    printf("ext_csd[163] 'Enable background operations handshake' .......................... [0x%0.2X] \n",mmc->ext_csd[163]);
    printf("ext_csd[162] 'H/W reset function' .............................................. [0x%0.2X] \n",mmc->ext_csd[162]);
    printf("ext_csd[161] 'HPI management' .................................................. [0x%0.2X] \n",mmc->ext_csd[161]);
    printf("ext_csd[160] 'Partitioning Support' ............................................ [0x%0.2X] \n",mmc->ext_csd[160]);
    printf("ext_csd[159:157] 'Max Enhanced Area Size' ...................................... [0x%0.6X] \n",(((int)mmc->ext_csd[159])<<16)+(((int)mmc->ext_csd[158])<<8)+mmc->ext_csd[157]);
    printf("ext_csd[156] 'Partitions attribute' ............................................ [0x%0.2X] \n",mmc->ext_csd[156]);
    printf("ext_csd[155] 'Paritioning Setting' ............................................. [0x%0.2X] \n",mmc->ext_csd[155]);
    printf("ext_csd[154] 'General Purpose Partition Size 1 2' .............................. [0x%0.2X] \n",mmc->ext_csd[154]);
    printf("ext_csd[153] 'General Purpose Partition Size 1 1' .............................. [0x%0.2X] \n",mmc->ext_csd[153]);
    printf("ext_csd[152] 'General Purpose Partition Size 1 0' .............................. [0x%0.2X] \n",mmc->ext_csd[152]);
    printf("ext_csd[151] 'General Purpose Partition Size 1 2' .............................. [0x%0.2X] \n",mmc->ext_csd[151]);
    printf("ext_csd[150] 'General Purpose Partition Size 1 1' .............................. [0x%0.2X] \n",mmc->ext_csd[150]);
    printf("ext_csd[149] 'General Purpose Partition Size 1 0' .............................. [0x%0.2X] \n",mmc->ext_csd[149]);
    printf("ext_csd[148] 'General Purpose Partition Size 1 2' .............................. [0x%0.2X] \n",mmc->ext_csd[148]);
    printf("ext_csd[147] 'General Purpose Partition Size 1 1' .............................. [0x%0.2X] \n",mmc->ext_csd[147]);
    printf("ext_csd[146] 'General Purpose Partition Size 1 0' .............................. [0x%0.2X] \n",mmc->ext_csd[146]);
    printf("ext_csd[145] 'General Purpose Partition Size 1 2' .............................. [0x%0.2X] \n",mmc->ext_csd[145]);
    printf("ext_csd[144] 'General Purpose Partition Size 1 1' .............................. [0x%0.2X] \n",mmc->ext_csd[144]);
    printf("ext_csd[143] 'General Purpose Partition Size 1 0' .............................. [0x%0.2X] \n",mmc->ext_csd[143]);
    printf("ext_csd[142:140] 'Enhanced User Data Area Size' ................................ [0x%0.6X] \n",(((int)mmc->ext_csd[142])<<16)+(((int)mmc->ext_csd[141])<<8)+mmc->ext_csd[140]);
    printf("ext_csd[139:136] 'Enhanced User Data Start Address' ............................ [0x%0.8X] \n",(((int)mmc->ext_csd[139])<<24)+(((int)mmc->ext_csd[138])<<16)+(((int)mmc->ext_csd[137])<<8)+mmc->ext_csd[136]);
    printf("ext_csd[134] 'Bad Block Management mode' ....................................... [0x%0.2X] \n",mmc->ext_csd[134]);

    return;
}

/* Dump CSD register */
static void dump_csd(struct mmc *mmc)
{
    printf("CSD Register: \n");
    printf("[CSD] - CSD structure                 : [0x%X] \n",((mmc->csd[0] >> 30) & 0x3));
    printf("[CSD] - System specification version  : [0x%X] \n",((mmc->csd[0] >> 26) & 0xf));
    printf("[CSD] - Reserved                      : [0x%X] \n",((mmc->csd[0] >> 24) & 0x3));
    printf("[CSD] - TAAC                          : [0x%X] \n",((mmc->csd[0] >> 16) & 0xff));
    printf("[CSD] - NSAC                          : [0x%X] \n",((mmc->csd[0] >> 8)  & 0xff));
    printf("[CSD] - Max. bus clock frequency      : [0x%X] \n",((mmc->csd[0] >> 0)  & 0xff));
    printf("[CSD] - CCC                           : [0x%X] \n",((mmc->csd[1] >> 20) & 0xfff));
    printf("[CSD] - Max. read data block length   : [0x%X] \n",((mmc->csd[1] >> 16) & 0xf));
    printf("[CSD] - Partial blocks read allowed   : [0x%X] \n",((mmc->csd[1] >> 15) & 0x1));
    printf("[CSD] - Write block misalignment      : [0x%X] \n",((mmc->csd[1] >> 14) & 0x1));
    printf("[CSD] - Read block misalignment       : [0x%X] \n",((mmc->csd[1] >> 13) & 0x1));
    printf("[CSD] - DSR implemented               : [0x%X] \n",((mmc->csd[1] >> 12) & 0x1));
    printf("[CSD] - Reserved                      : [0x%X] \n",((mmc->csd[1] >> 10) & 0x3));
    printf("[CSD] - Device size                   : [0x%X] \n",((mmc->csd[1] << 2 | mmc->csd[2] >> 30) & 0xFFF));
    printf("[CSD] - Max. read current @ VDD min   : [0x%X] \n",((mmc->csd[2] >> 27) & 0x7));
    printf("[CSD] - Max. read current @ VDD max   : [0x%X] \n",((mmc->csd[2] >> 24) & 0x7));
    printf("[CSD] - Max. write current @ VDD min  : [0x%X] \n",((mmc->csd[2] >> 21) & 0x7));
    printf("[CSD] - Max. write current @ VDD max  : [0x%X] \n",((mmc->csd[2] >> 18) & 0x7));
    printf("[CSD] - Device size multiplier        : [0x%X] \n",((mmc->csd[2] >> 15) & 0x7));
    printf("[CSD] - Erase group size              : [0x%X] \n",((mmc->csd[2] >> 10) & 0x1f));
    printf("[CSD] - Erase group size multiplier   : [0x%X] \n",((mmc->csd[2] >> 5)  & 0x1f));
    printf("[CSD] - Write protect group size      : [0x%X] \n",((mmc->csd[2] >> 0)  & 0x1f));
    printf("[CSD] - Write protect group enable    : [0x%X] \n",((mmc->csd[3] >> 31) & 0x1));
    printf("[CSD] - Manufacturer default ECC      : [0x%X] \n",((mmc->csd[3] >> 29) & 0x3));
    printf("[CSD] - Write speed factor            : [0x%X] \n",((mmc->csd[3] >> 26) & 0x7));
    printf("[CSD] - Max. write data block length  : [0x%X] \n",((mmc->csd[3] >> 22) & 0xf));
    printf("[CSD] - Partial blocks write allowed  : [0x%X] \n",((mmc->csd[3] >> 21) & 0x1));
    printf("[CSD] - Content protection app        : [0x%X] \n",((mmc->csd[3] >> 16) & 0x1));
    printf("[CSD] - File format group             : [0x%X] \n",((mmc->csd[3] >> 15) & 0x1));
    printf("[CSD] - Copy flag (OTP)               : [0x%X] \n",((mmc->csd[3] >> 14) & 0x1));
    printf("[CSD] - Permanent write protection    : [0x%X] \n",((mmc->csd[3] >> 13) & 0x1));
    printf("[CSD] - Temporary write protection    : [0x%X] \n",((mmc->csd[3] >> 12) & 0x1));
    printf("[CSD] - File format                   : [0x%X] \n",((mmc->csd[3] >> 10) & 0x3));
    printf("[CSD] - ECC code                      : [0x%X] \n",((mmc->csd[3] >> 8)  & 0x3));
    printf("[CSD] - CRC                           : [0x%X] \n",((mmc->csd[3] >> 1)  & 0x7F));
    printf("[CSD] - Not used, always 1            : [0x%X] \n",((mmc->csd[3] >> 0)  & 0x1));

    return;
}

/* Dump CID register */
static void dump_cid(struct mmc *mmc)
{
    char product_name[7];

    product_name[0] = ((char)((mmc->cid[0]>>0)  & 0xFF));
    product_name[1] = ((char)((mmc->cid[1]>>24) & 0xFF));
    product_name[2] = ((char)((mmc->cid[1]>>26) & 0xFF));
    product_name[3] = ((char)((mmc->cid[1]>>8)  & 0xFF));
    product_name[4] = ((char)((mmc->cid[1]>>0)  & 0xFF));
    product_name[5] = ((char)((mmc->cid[2]>>24) & 0xFF));
    product_name[6] = 0;

    printf("CID Register: \n");
    printf("  Manufacturer ID ..... 0x%X\n",(char)((mmc->cid[0]>>24) & 0xFF));
    printf("  Card/BGA ............ 0x%X\n",(char)((mmc->cid[0]>>16) & 0x03));
    printf("  OEM/Application ID .. 0x%X\n",(char)((mmc->cid[0]>>8)  & 0xFF));
    printf("  Product name ........ %s\n",product_name);
    printf("  Product revision .... %d.%d\n",((mmc->cid[2]>>20) & 0x0000000F), ((mmc->cid[2]>>16) & 0x0000000F));
    printf("  Serial number ....... 0x%X\n" ,((mmc->cid[3]>>16) & 0xFFFF)    + ((mmc->cid[2] & 0x0000FFFF)<<16));
    printf("  Manufacturing date .. %d/%d\n",((mmc->cid[3]>>12) & 0x0F),       ((mmc->cid[3]>>8) & 0x0F)+ 1997);
    printf("  CRC ................. 0x%X\n",(mmc->cid[3]>>1) & 0x7F);

    return;
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

int do_mmcinfo (cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	struct mmc *mmc;

	if (curr_device < 0) {
		if (get_mmc_num() > 0)
			curr_device = 0;
		else {
			puts("No MMC device available\n");
			return 1;
		}
	}

	mmc = find_mmc_device(curr_device);

	if (mmc) {
		mmc_init(mmc);

		print_mmcinfo(mmc);
		return 0;
	} else {
		printf("no mmc device at slot %x\n", curr_device);
		return 1;
	}
}

U_BOOT_CMD(
	mmcinfo, 1, 0, do_mmcinfo,
	"mmcinfo\t- display MMC info\n",
	"    - device number of the device to dislay info of\n"
	""
);

int do_mmcops(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	enum mmc_state state;

	if (argc < 2)
    {
		printf ("Usage:\n%s\n", cmdtp->usage);
		return 1;
    }

	if (curr_device < 0) {
		if (get_mmc_num() > 0)
			curr_device = 0;
		else {
			puts("No MMC device available\n");
			return 1;
		}
	}
    /* Send Switch Command (CMD6) to MMC */
    if (strcmp(argv[1], "switch") == 0) {
        unsigned char field_id = 0;
        unsigned char value = 0;

		struct mmc *mmc = find_mmc_device(curr_device);

		if (!mmc) {
			printf("no mmc device at slot %x\n", curr_device);
			return 1;
		}

        if (argc == 4)
        {
            /* Get 'index' for CMD6 */
            field_id = simple_strtoul(argv[2], NULL, 10);

            /* Get 'value' for CMD6 */
            value = simple_strtoul(argv[3], NULL, 16);

            printf("MMC set switch [%d] to 0x%X\n",field_id,value);
            mmc_switch(mmc,EXT_CSD_CMD_SET_NORMAL,field_id,value);
        }
        else
        {
            printf("Error: please set an 'index' and 'value'\n");
            return 1;
        }

        printf("Done.\n");
        return 0;
	}

    /* Enable/Disable DDR (Dual Data Rate) Host Controller capability */
    if (strcmp(argv[1], "ddr") == 0) {

		struct mmc *mmc = find_mmc_device(curr_device);

		if (!mmc) {
			printf("no mmc device at slot %x\n", curr_device);
			return 1;
		}

        if (argc != 3)
        {
            printf("Error: no enable/disable parameter\n");
            return 1;
        }

        if (strcmp(argv[2], "enable") == 0)
        {
            printf("Enable DDR Host Controller capability...\n");
            mmc->ioctl(mmc,MMC_IOCTL_HOST_DDR_ENABLE,0);
        }
        else if (strcmp(argv[2], "disable") == 0) 
        {
            printf("Disable DDR Host Controller capability ...\n");
            mmc->ioctl(mmc,MMC_IOCTL_HOST_DDR_DISABLE,0);
        }

        printf("Please do 'mmc rescan' to activate new mode.\n");
        return 0;
	}

    /* Perform various on MMC registers dump for debugging */
    if (strcmp(argv[1], "dump") == 0) {
       
		struct mmc *mmc = find_mmc_device(curr_device);

		if (!mmc) {
			printf("no mmc device at slot %x\n", curr_device);
			return 1;
		}

        if (argc != 3)
        {
            printf("Error: no dump parameter\n");
            return 1;
        }

        /* Dump MMC EXT_CSD Register */
        if (strcmp(argv[2], "ext_csd") == 0) {
            dump_ext_csd(mmc);
        }

        /* Dump MMC CSD Register */
        if (strcmp(argv[2], "csd") == 0) {
            dump_csd(mmc);
        }

        if (strcmp(argv[2], "cid") == 0) {
            dump_cid(mmc);
        }

        /* Dump Host Controller Registers */
        if (strcmp(argv[2], "regs") == 0) {
            mmc->ioctl(mmc,MMC_IOCTL_HOST_DUMP_REGS,0);
        }

        printf("MMC device is ready.\n");
        return 0;
	}

    /* Perform Host controller Reset */
    if (strcmp(argv[1], "reset") == 0) {
       
		struct mmc *mmc = find_mmc_device(curr_device);

		if (!mmc) {
			printf("no mmc device at slot %x\n", curr_device);
			return 1;
		}

        if (argc == 3)
        {
            /* Perfrom Host controller DATA reset */
            if (strcmp(argv[2], "data") == 0) 
            {
                mmc->ioctl(mmc,MMC_IOCTL_HOST_RESET_DATA,0);
            }
            /* Perfrom Host controller CMD reset */
            else if (strcmp(argv[2], "cmd") == 0) 
            {
                mmc->ioctl(mmc,MMC_IOCTL_HOST_RESET_CMD,0);
            }
            /* Perfrom Host controller ALL reset */
            else if (strcmp(argv[2], "all") == 0) 
            {
                mmc->ioctl(mmc,MMC_IOCTL_HOST_RESET_ALL,0);
            }
            else
            {
                 printf("Error: unkonwn reset type '%s'.\n",argv[2]);
                 return 1;
            }
        }
        /* By default, perform Reset ALL */
        else
        {
            mmc->ioctl(mmc,MMC_IOCTL_HOST_RESET_ALL,0);
        }

        printf("MMC controller is Reset, please do 'mmc rescan'.\n");
        return 0;
	}

    /* Re-initialize the MMC card, with optional power cycle to MMC */
	if (strcmp(argv[1], "rescan") == 0) {
        int off_delay_ms = -1;   /* Optional delay ofter Power off in MS */
        int on_delay_ms = -1;    /* Optional delay ofter Power on in MS */ 

		struct mmc *mmc = find_mmc_device(curr_device);

		if (!mmc) {
			printf("no mmc device at slot %x\n", curr_device);
			return 1;
		}

        if (argc >= 3)
        {
            off_delay_ms = simple_strtoul(argv[2], NULL, 10);
            printf("MMC power cycle with power-off delay off %d ms\n",off_delay_ms);
        }

        if (argc >= 4)
        {
            on_delay_ms = simple_strtoul(argv[3], NULL, 10);
            printf("MMC power cycle with power-on  delay off %d ms\n",on_delay_ms);
           
        }

        if (off_delay_ms !=-1)
        {
            mmc->ioctl(mmc,MMC_IOCTL_HOST_POWER_OFF,0);
            udelay (off_delay_ms * 1000); 
            if (on_delay_ms !=-1)
            {
                 mmc->ioctl(mmc,MMC_IOCTL_HOST_POWER_ON,0);
                 udelay (on_delay_ms * 1000);  
            }
        }

		mmc->has_init = 0;

		if (mmc_init(mmc))
        {
            printf("failed to initialize the mmc device.\n");
			return 1;
        }
		else
        {
            printf("MMC device is ready.\n");
			return 0;
        }
#ifdef CONFIG_PARTITIONS
	} else if (strcmp(argv[1], "part") == 0) {
		block_dev_desc_t *mmc_dev;
		struct mmc *mmc = find_mmc_device(curr_device);

		if (!mmc) {
			printf("no mmc device at slot %x\n", curr_device);
			return 1;
		}
		mmc_init(mmc);
		mmc_dev = mmc_get_dev(curr_device);
        if (mmc_dev != NULL)
        {
            init_part(mmc_dev);
    		if (mmc_dev->type != DEV_TYPE_UNKNOWN) {
                if (argc == 4)
                {
                    ulong dest;
                    int buff_size = 0;
                    /* Get Source/Destination address on RAM/DDR */
                    dest = simple_strtoul(argv[3], NULL, 16);
                    if (dest == 0)
                    {
                        printf("No valid destination address in DDR\n");
                        printf ("Usage:\n%s\n", cmdtp->usage);
                        return 1;
                    }
                    /* Backup partition table from RAM/DDR */
                    if (strcmp(argv[2], "backup") == 0)
                    {
                        printf("Backup partitions table into 0x%08X\n",dest);
                        backup_part (mmc_dev,&buff_size, dest);
                        printf("Table size = %u bytes \n",buff_size);
                        return 0;

                    }
                    /* Restore partition table from RAM/DDR */
                    else if (strcmp(argv[2], "restore") == 0)
                    {
                        printf("Restore partitions table from 0x%08X\n",dest);
                        restore_part (mmc_dev,&buff_size, dest);
                        printf("Table size = %u bytes \n",buff_size);
                        return 0;
                    }
                }

                /* Print partition list */
                print_part(mmc_dev);
                return 0;
            }
            puts("unknown partition type\n");
		}
		puts("get mmc type error!\n");
		return 1;     
#endif
	} else if (strcmp(argv[1], "list") == 0) {
		print_mmc_devices('\n');
		return 0;
	} else if (strcmp(argv[1], "dev") == 0) {
		int dev, part = -1;
		struct mmc *mmc;

		if (argc == 2)
			dev = curr_device;
		else if (argc == 3)
			dev = simple_strtoul(argv[2], NULL, 10);
		else if (argc == 4) {
			dev = (int)simple_strtoul(argv[2], NULL, 10);
			part = (int)simple_strtoul(argv[3], NULL, 10);
			if (part > PART_ACCESS_MASK) {
				printf("#part_num shouldn't be larger"
					" than %d\n", PART_ACCESS_MASK);
				return 1;
			}
		} else {
            printf ("Usage:\n%s\n", cmdtp->usage);
            return 1;
        }

		mmc = find_mmc_device(dev);
		if (!mmc) {
			printf("no mmc device at slot %x\n", dev);
			return 1;
		}

		mmc_init(mmc);
		if (part != -1) {
			int ret;
			if (mmc->part_config == MMCPART_NOAVAILABLE) {
				printf("Card doesn't support part_switch\n");
				return 1;
			}

			if (part != mmc->part_num) {
				ret = mmc_switch_part(dev, part);
				if (!ret)
					mmc->part_num = part;

				printf("switch to partions #%d, %s\n",
						part, (!ret) ? "OK" : "ERROR");
			}
		}
		curr_device = dev;
		if (mmc->part_config == MMCPART_NOAVAILABLE)
			printf("mmc%d is current device\n", curr_device);
		else
			printf("mmc%d(part %d) is current device\n",
				curr_device, mmc->part_num);

		return 0;
	} else if (strcmp(argv[1], "on") == 0) {
        struct mmc *mmc;
        int dev_num = 0;

        mmc = find_mmc_device(dev_num);
    	if (!mmc)
        {
            printf("Error: fail to find mmc device #%d",dev_num);
    		return 0;
        }

        /* Power On the eMMC Incomm Controller */
        mmc->ioctl(mmc,MMC_IOCTL_HOST_POWER_ON,0);

        /* 100ms delay */
        udelay (100*1000);  //100ms

        printf("MMC power is ON\n");

        return 0;
    } else if (strcmp(argv[1], "off") == 0) {
        struct mmc *mmc;
        int dev_num = 0;

        mmc = find_mmc_device(dev_num);
    	if (!mmc)
        {
            printf("Error: fail to find mmc device #%d",dev_num);
    		return 0;
        }

        /* Power Off the eMMC Incomm Controller */
        mmc->ioctl(mmc,MMC_IOCTL_HOST_POWER_OFF,0);

        /* 100ms delay */
        udelay (100*1000);  //100ms

        printf("MMC power is OFF\n");

        return 0;
    }


	if (strcmp(argv[1], "read") == 0)
		state = MMC_READ;
	else if (strcmp(argv[1], "write") == 0)
		state = MMC_WRITE;
#ifdef CONFIG_MMC_ERASE_ENABLE
	else if (strcmp(argv[1], "erase") == 0)
		state = MMC_ERASE;
#endif
	else
		state = MMC_INVALID;

	if (state != MMC_INVALID) {
		struct mmc *mmc = find_mmc_device(curr_device);
		int idx = 2;
		u32 blk, cnt, n;
		void *addr;

		if (state != MMC_ERASE) {
			addr = (void *)simple_strtoul(argv[idx], NULL, 16);
			++idx;
		} else
			addr = 0;
		blk = simple_strtoul(argv[idx], NULL, 16);
		cnt = simple_strtoul(argv[idx + 1], NULL, 16);

		if (!mmc) {
			printf("no mmc device at slot %x\n", curr_device);
			return 1;
		}

		printf("\nMMC %s: dev # %d, block # %d, count %d ... ",
				argv[1], curr_device, blk, cnt);

		mmc_init(mmc);

		switch (state) {
		case MMC_READ:
			n = mmc->block_dev.block_read(curr_device, blk,
						      cnt, addr);
			/* flush cache after read */
			flush_cache((ulong)addr, cnt * 512); /* FIXME */
			break;
		case MMC_WRITE:
			n = mmc->block_dev.block_write(curr_device, blk,
						      cnt, addr);
			break;
		case MMC_ERASE:
			n = mmc->block_dev.block_erase(curr_device, blk, cnt);
			break;
		default:
			BUG();
		}

		printf("%d blocks %s: %s\n",
				n, argv[1], (n == cnt) ? "OK" : "ERROR");
		return (n == cnt) ? 0 : 1;
	}

	printf ("Usage:\n%s\n", cmdtp->usage);
    return 1;
}

U_BOOT_CMD(
	mmc, 6, 0, do_mmcops,
	"mmc\t- MMC subsystem commands\n",
	"mmc read addr blk# cnt            - read from mmc  - addr: destination address on memory, blk#: source address in mmc in blocks, cnt#: number of block to read.\n"
	"mmc write addr blk# cnt           - write to mmc   - addr: source address on memory, blk#: destination address in mmc in blocks, cnt#: number of block to read.\n"
#ifdef CONFIG_MMC_ERASE_ENABLE
	"mmc erase blk# cnt                - erase from mmc - blk#: destination address in mmc in blocks, cnt#: number of block to read.\n"
#endif 
    "mmc rescan                        - initialize the mmc without optional power cycle. \n"
	"mmc rescan [off delay] [on delay] - initialize the mmc with power cycle and delay in ms after off an on. \n"
#ifdef CONFIG_PARTITIONS
	"mmc part                          - lists available partition on current mmc device\n"
    "mmc part [backup|restore] [addr]  - backup and restore partitions table to RAM\n"
#endif
	"mmc dev [dev] [part]              - show or set current mmc device [partition]\n"
	"mmc list                          - lists available devices\n"
    "mmc [off|on]                      - switch mmc power on/off \n"
    "mmc dump [cid|csd|ext_csd|regs]   - dump cid, csd, ext_csd or host controller registers\n"   
    "mmc reset [all|data|cmd]          - reset the host controller\n"
    "mmc switch index value            - send CMD6 with index and value\n"
    "mmc ddr [enable|disable]          - enable or disable DDR (Dual Data Rate) capability on Host controller\n");


int do_mmcaddr2blk (cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	struct mmc *mmc;
    block_dev_desc_t *mmc_dev;
    int addr = 0;
    char text_val[20];
    int blksize = 0;

    if (argc < 2)
    {
		printf ("Usage:\n%s\n", cmdtp->usage);
		return 1;
    }

	if (curr_device < 0) {
		if (get_mmc_num() > 0)
			curr_device = 0;
		else {
			puts("No MMC device available\n");
			return 1;
		}
	}

    addr = (int)simple_strtoul(argv[1], NULL, 16);
    if (addr < 0) {
        printf("address must be larger than 0\n", addr);
        return 1;
    }

    mmc = find_mmc_device(curr_device);
	if (mmc) {
		mmc_init(mmc);
        mmc_dev = mmc_get_dev(curr_device);
        
        if (mmc_dev != NULL)
        {
            blksize = addr / mmc->write_bl_len;
            if (blksize * mmc->write_bl_len < addr)  /* add one block for round up*/
            {
                blksize++;
            }
            sprintf (text_val, "0x%X", blksize);
            setenv("blocksize", text_val);

            return 0;
        }
		puts("get mmc type error!\n");
		return 1;
	} 
    printf("no mmc device at slot %x\n", curr_device);
	return 1;
}

U_BOOT_CMD(
	mmcaddr2blk, 2, 0, do_mmcaddr2blk,
	"mmcaddr2blk\t- convert address to blocks, save results in 'blocksize' \n",
	"addr  - address to convert\n"
	""
);

#ifdef CONFIG_PARTITIONS
int do_mmcpart (cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	struct mmc *mmc;
    block_dev_desc_t *mmc_dev;
    int part = 0;
    disk_partition_t info = {0};
    char text_val[20];

    if (argc < 2)
    {
		printf ("Usage:\n%s\n", cmdtp->usage);
		return 1;
    }

	if (curr_device < 0) {
		if (get_mmc_num() > 0)
			curr_device = 0;
		else {
			puts("No MMC device available\n");
			return 1;
		}
	}

    part = (int)simple_strtoul(argv[1], NULL, 10);
    if (part > PART_ACCESS_MASK) {
        printf("#part_num shouldn't be larger than %d\n", PART_ACCESS_MASK);
        return 1;
    }

	mmc = find_mmc_device(curr_device);

	if (mmc) {
		mmc_init(mmc);
        mmc_dev = mmc_get_dev(curr_device);
        if (mmc_dev != NULL)
        {
            init_part(mmc_dev);
    		if (mmc_dev->type != DEV_TYPE_UNKNOWN) {
                if (get_partition_info (mmc_dev, part, &info) == 0){
                    sprintf (text_val, "0x%X", info.start*info.blksz);
                    setenv("partaddr", text_val);
                    sprintf (text_val, "0x%X", info.start);
                    setenv("partaddrblk", text_val);
                    sprintf (text_val, "0x%X", info.size*info.blksz);
                    setenv("partsize", text_val);
                    sprintf (text_val, "0x%X", info.size);
                    setenv("partsizeblk", text_val);
                    return 0;
                }
                
                printf("partition %d not found\n",part);
                return 1;
    		}
            printf("unknown partition type\n");
            return 1;
        }
		puts("get mmc type error!\n");
		return 1;
	} else {
		printf("no mmc device at slot %x\n", curr_device);
		return 1;
	}
}

U_BOOT_CMD(
	mmcpart, 2, 0, do_mmcpart,
	"mmcpart\t- set MMC partition info to environment variables\n",
	"part#  - partiton number\n"
	""
);

#endif /* CONFIG_PARTITIONS */


#endif /* CFG_CMD_MMC */
#endif /* CONFIG_GENERIC_MMC */
