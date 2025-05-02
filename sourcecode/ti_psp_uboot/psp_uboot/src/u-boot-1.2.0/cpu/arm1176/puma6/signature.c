/* 
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

#include <signature.h>
#include <common.h>
#include <command.h>
#include <environment.h>
#include <image.h>
#include <uImage.h>
#include <mmc.h>
#include <mmc_utils.h>


/* Check if image signature number is valid */
int check_signature_num(int sigNum)
{
	/* sigNum should be = [1, MAXIMUM_SIGNATURES] */
	if ((1 <= sigNum) && (sigNum <= GET_MAXIMUM_SIGNATURES()))
		return STATUS_OK;
	else
	{
		printf("\nError: image signature number should be within [1, %d].\n", GET_MAXIMUM_SIGNATURES());
		return STATUS_NOK;
	}
}

/* Read (from emmc) & print image signature */
int read_signature(int sigNum)
{
	unsigned int sigAddr = (unsigned int)GET_SIGNATURE_ADDRESS(sigNum);		/* Image signature address in flash */
	unsigned int sigBuffer[SINGLE_SIGNATURE_SIZE / sizeof(unsigned int)];

	int i;

	if(check_signature_address(sigAddr) == STATUS_NOK)
		return STATUS_NOK;

	if (mmc_read_buff(sigAddr, sigBuffer, SINGLE_SIGNATURE_SIZE) != STATUS_OK)
	{
		printf("\nError: failed to read from flash.\n");
		return STATUS_NOK;
	}

	printf("\nImage signature %d.\nAddress in flash = [0x%08x], size = %d bytes.\n\n", sigNum, sigAddr, SINGLE_SIGNATURE_SIZE);

	/* The number <4> is hard-coded because we print 4 integers in a row */
	for (i=0; i < (SINGLE_SIGNATURE_SIZE / sizeof(unsigned int)); i += 4, sigAddr += 4*sizeof(unsigned int*))
		printf("[0x%08p] - %08x %08x %08x %08x\n", sigAddr, mmc_swap_dword(sigBuffer[i]), mmc_swap_dword(sigBuffer[i+1]), mmc_swap_dword(sigBuffer[i+2]), mmc_swap_dword(sigBuffer[i+3]));

	printf("\n");

	return STATUS_OK;
}

int check_signature_address(unsigned int sigAddr)
{
	if(sigAddr == ERROR)
	{
		printf("\nError: signature address is wrong or undefined.\n");
		return STATUS_NOK;
	}
	else
	{
		return STATUS_OK;
	}
}

/*
	Write (to emmc) image signature at given address 
    baseAddr - The address to read the image signature from (must be RAM address).  
	sigNum - The image signature number.
*/ 
int write_signature(unsigned int baseAddr, int sigNum)
{
	unsigned int sigAddr = (unsigned int)GET_SIGNATURE_ADDRESS(sigNum);

	if(check_signature_address(sigAddr) == STATUS_NOK)
		return STATUS_NOK;

	/* Write image signature to mmc */
	printf("\nWriting image signature number %d, from RAM at [0x%08x], to flash at [0x%08x]..\n", sigNum, baseAddr, sigAddr);
	if (flash_write_mmc(baseAddr, sigAddr, SINGLE_SIGNATURE_SIZE, VERBOSE_DISABLE) != STATUS_OK)
	{
		printf("\nError: failed to write to flash.\n");
		return STATUS_NOK;
	}
	else
	{
		printf("Image signature updated successfully.\n\n");
		return STATUS_OK;
	}
}

/* Copy image signature from flash to RAM */
int copy_signature_to_ram(int sigNum, unsigned int dstAddr)
{
	unsigned int* dst = (unsigned int*)dstAddr;
	unsigned int src = GET_SIGNATURE_ADDRESS(sigNum);

	if(check_signature_address(src) == STATUS_NOK)
		return STATUS_NOK;

	printf("\nCopying image signature number %d from flash, to RAM at [0x%08x]..\n", sigNum, dst);

	if(mmc_read_buff(src, dst, SINGLE_SIGNATURE_SIZE) == STATUS_NOK)
	{
		printf("Error: failed to copy signature to RAM.\n");
		return STATUS_NOK;
	}

	printf("\nImage signature copied successfully.\n\n");

	return STATUS_OK;
}

/* Print signatures definitions in header */
void print_signatures_info()
{
	unsigned int sigStartAddr = GET_SIGNATURES_BLOCK_START();

	printf("\nImages Signatures information\n");
	if(check_signature_address(sigStartAddr) == STATUS_NOK)
	{
		return STATUS_NOK;
	}
	else
	{
		printf("Signatures start address - [0x%08x]\n", sigStartAddr);
		printf("Signatures offset source - %s\n", SIGNATURES_OFFSET_SOURCE());
		printf("Signature size           - %d Bytes\n", SINGLE_SIGNATURE_SIZE);
		printf("Signatures count         - %d\n\n", GET_MAXIMUM_SIGNATURES());
	}
}

/* Endian swap for 32bit int */
unsigned int mmc_swap_dword(unsigned int x) 
{ 
    unsigned int swp = x;
    return ( ((swp&0x000000FF)<<24) + ((swp&0x0000FF00)<<8) + ((swp&0x00FF0000)>>8) + ((swp&0xFF000000)>>24));
}
