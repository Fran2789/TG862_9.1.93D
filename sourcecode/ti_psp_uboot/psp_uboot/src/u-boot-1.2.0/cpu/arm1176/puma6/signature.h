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

/* image signature read and write file */

#ifndef _SIGNATURE_H_
#define _SIGNATURE_H_

#include <docsis_ip_boot_params.h>

#define FIRST_SIGNATURE_TO_PRINT	1

#define ENV_BASE_HEX    		16

/* Command types */
#define SIGNATURE_CMD_INFO 		1
#define SIGNATURE_CMD_READ		2
#define SIGNATURE_CMD_WRITE		3
#define SIGNATURE_CMD_COPY		4

/* Command arguments indices */
#define SIGNATURE_CMD_OPERATOR_IDX			1
#define SIGNATURE_CMD_SIGNATURE_NUM_IDX		2
#define SIGNATURE_CMD_ADDR_IDX				3

#define NO_VALUE				-1
#define STATUS_OK       		0
#define STATUS_NOK      		1

#define VERBOSE_ENABLE  		1
#define VERBOSE_DISABLE 		0

/* Default source address */
#define DEFAULT_ADDR			0x40900000

/* In old CEFDK version, SIGNATURE_OFFSET will be set to zero. In order to know the real value of the offset, we need it hard-coded */
#define GS_CR_SIGNATURE_OFFSET		(0x002d8000)	/* Old CEFDK signature offset in Golden-Springs & Cat-River */
#define HP_HPMG_SIGNATURE_OFFSET	(0x001d8000)	/* Old CEFDK signature offset in Harbor-Park & Harbor-Park MG */

/* Sizes of signatures block and a single signature */
#define SIGNATURES_BLOCK_SIZE		(32768)		/* 32KB */
#define SINGLE_SIGNATURE_SIZE		(1024)		/* 1KB */

/* Board id as defined in docsis_ip_boot_params.h */
#define BOARD_ID								(BOOT_PARAM_DWORD_READ(BOARD_TYPE))

#define ERROR									(-1)

#define IS_GS_CR_BOARD()						((BOARD_ID == PUMA6_GS_BOARD_ID) || (BOARD_ID == PUMA6_CR_BOARD_ID))
#define IS_HP_HPMG_BOARD()						((BOARD_ID == PUMA6_HP_BOARD_ID) || (BOARD_ID == PUMA6_HP_MG_BOARD_ID))
#define GET_HARD_CODED_OFFSET()					(IS_GS_CR_BOARD() ? GS_CR_SIGNATURE_OFFSET : (IS_HP_HPMG_BOARD() ? HP_HPMG_SIGNATURE_OFFSET : ERROR))

/* Macro to get offset source */
#define SIGNATURES_OFFSET_SOURCE()				(BOOT_PARAM_DWORD_READ(SIGNATURE1_OFFSET) != 0x0 ? "CEFDK Boot Params" : "Hard-Coded")

/* Macro to get signatures block offset */
#define GET_SIGNATURES_BLOCK_START()			(BOOT_PARAM_DWORD_READ(SIGNATURE1_OFFSET) != 0x0 ? BOOT_PARAM_DWORD_READ(SIGNATURE1_OFFSET) : GET_HARD_CODED_OFFSET())

/* Macro to get signature address */
#define GET_SIGNATURE_ADDRESS(sigNum)			(GET_SIGNATURES_BLOCK_START() != ERROR ? (GET_SIGNATURES_BLOCK_START() + (((sigNum) - 1) * SINGLE_SIGNATURE_SIZE)) : ERROR)

/* Macro to get maximum number of signatures */
#define GET_MAXIMUM_SIGNATURES()				(SIGNATURES_BLOCK_SIZE / SINGLE_SIGNATURE_SIZE)

/* Functions */
int check_signature_num(int signum);
int check_signature_address(unsigned int sigAddr);
int read_signature(int signum);
int write_signature(unsigned int baseaddr, int signum);
int copy_signature_to_ram(int signum, unsigned int dstaddr);
void print_signatures_info();
unsigned int mmc_swap_dword(unsigned int x);

#endif /* _SIGNATURE_H_ */
