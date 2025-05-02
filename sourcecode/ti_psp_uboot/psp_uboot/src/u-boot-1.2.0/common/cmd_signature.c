/* 
 * Written by Or Hedvat - ISR 
 * 
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


#include <common.h>
#include <command.h>
#include <environment.h>
#include <image.h>
#include <uImage.h>
#include <mmc.h>

#include <rtc.h>

#include <signature.h>


/* Parse & execute 'signature' command */
int do_signature(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	unsigned int addr 	= DEFAULT_ADDR;		/* Default source address in SRAM is 0x40900000 */

	int signatureNum 	= NO_VALUE;
	int cnt 			= NO_VALUE;
	int cmd_type 		= NO_VALUE;


	/* Parse */
	if(strcmp(argv[SIGNATURE_CMD_OPERATOR_IDX], "-i") == 0)
		cmd_type = SIGNATURE_CMD_INFO;
	else if(strcmp(argv[SIGNATURE_CMD_OPERATOR_IDX], "-r") == 0)
		cmd_type = SIGNATURE_CMD_READ;
	else if(strcmp(argv[SIGNATURE_CMD_OPERATOR_IDX], "-w") == 0)
		cmd_type = SIGNATURE_CMD_WRITE;
	else if(strcmp(argv[SIGNATURE_CMD_OPERATOR_IDX], "-c") == 0)
		cmd_type = SIGNATURE_CMD_COPY;

	if(cmd_type != NO_VALUE)
	{
		/* Execute */
		switch (cmd_type)
		{
		case SIGNATURE_CMD_INFO :
			if(argc == (SIGNATURE_CMD_OPERATOR_IDX + 1))
			{
				print_signatures_info();
				return STATUS_OK;
			}
		break;
		case SIGNATURE_CMD_READ :
			if(argc == (SIGNATURE_CMD_SIGNATURE_NUM_IDX + 1))
			{
				if(strcmp(argv[SIGNATURE_CMD_SIGNATURE_NUM_IDX], "all") == 0)
				{
					cnt = GET_MAXIMUM_SIGNATURES(); 
					signatureNum = FIRST_SIGNATURE_TO_PRINT;
				}
				else
				{
					signatureNum = simple_strtoul(argv[SIGNATURE_CMD_SIGNATURE_NUM_IDX], NULL, 10);
					cnt = 1;
				}
				if(check_signature_num(signatureNum) == STATUS_NOK)
					return STATUS_NOK;
				for(; cnt>0; --cnt, ++signatureNum)
					if(read_signature(signatureNum) == STATUS_NOK)
						return STATUS_NOK;

				return STATUS_OK;
			}
		break;
		case SIGNATURE_CMD_WRITE :
			if((argc == (SIGNATURE_CMD_SIGNATURE_NUM_IDX + 1)) || (argc == (SIGNATURE_CMD_ADDR_IDX + 1)))
			{
				if(strcmp(argv[SIGNATURE_CMD_SIGNATURE_NUM_IDX], "all") == 0)
				{
					cnt = GET_MAXIMUM_SIGNATURES();
					signatureNum = FIRST_SIGNATURE_TO_PRINT;
				}
				else
				{
					signatureNum = simple_strtoul(argv[SIGNATURE_CMD_SIGNATURE_NUM_IDX], NULL, 10);
					cnt = 1;
				}
				if(argc == (SIGNATURE_CMD_ADDR_IDX + 1))
					addr = (unsigned int)simple_strtoul(argv[SIGNATURE_CMD_ADDR_IDX], NULL, ENV_BASE_HEX);
				if((check_signature_num(signatureNum) == STATUS_NOK))
						return STATUS_NOK;
				for(; cnt>0; --cnt, ++signatureNum, addr += (SINGLE_SIGNATURE_SIZE / sizeof(unsigned int)))
					if(write_signature(addr, signatureNum) == STATUS_NOK)
						return STATUS_NOK;

				return STATUS_OK;
			}
		break;
		case SIGNATURE_CMD_COPY :
			if(argc == (SIGNATURE_CMD_ADDR_IDX + 1))
			{
				signatureNum = simple_strtoul(argv[SIGNATURE_CMD_SIGNATURE_NUM_IDX], NULL, 10);
				addr = (unsigned int)simple_strtoul(argv[SIGNATURE_CMD_ADDR_IDX], NULL, ENV_BASE_HEX);
				if((check_signature_num(signatureNum) == STATUS_NOK))
					return STATUS_NOK;
				if(copy_signature_to_ram(signatureNum, addr) == STATUS_NOK)
					return STATUS_NOK;

				return STATUS_OK;
			}
		break;
		}
	}

	printf("\nError: illegal command use.\n");
	printf ("Usage:\n%s\n", cmdtp->usage);
	return STATUS_NOK;
}

U_BOOT_CMD(
	signature, 10, 0, do_signature,
	"signature\t - Program Puma6 image signatures.\n",
	"[-w <signum>|all [<addr>]] [-r <signum>|all] [-c <signum> <addr>] [-i]\n"
	"'-w <signum>|all [<addr>]' - Write specified/all image signature/s (optional arbitrary address (hex)).\n"
	"'-r <signum>|all'          - Read specified/all image signature/s.\n"
	"'-c <signum> <addr>'       - Copy image signature from flash to RAM.\n"
	"'-i'                       - Print signature information.\n"
);
