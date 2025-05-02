/*
 * spi_sfi_client_info.c
 * Description:
 * SPI and SFI client info.
 *
 *
 * Copyright (C) 2008, Texas Instruments, Incorporated
 *
 *  This program is free software; you can distribute it and/or modify it
 *  under the terms of the GNU General Public License (Version 2) as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *  for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
 */


#include <common.h>
#include <spi.h>
#include "spi_sfi.h"


#define SPANSION_NUM_DUMMY_BYTES_0	0
#define SPANSION_NUM_DUMMY_BYTES_1	1
#define SPANSION_NUM_ADDR_BYTES		2 /*Count starts form 0, 3 address bytes */

#define SPANSION_READ				0x03
#define SPANSION_FAST_READ			0x0B
#define SPANSION_PAGE_PROGRAM		0x02
#define SPANSION_SPI_CLK_DIV 		0x00


struct spi_client_info puma5_spi_client[PUMA5_NUM_SFI_CLIENTS] = {
	{
		"Spansion",
		PUMA5_CS_0,
		SPI_MODE_0,
		SPI_POL_ACTIVE_LOW,
		NUM_BITS_PER_WORD_8,
		SPI_4_PIN_CMD_MODE,
		SPANSION_SPI_CLK_DIV,
	},
	{
		"Spansion1",
		PUMA5_CS_1,
		SPI_MODE_0,
		SPI_POL_ACTIVE_LOW,
		NUM_BITS_PER_WORD_8,
		SPI_4_PIN_CMD_MODE,
		SPANSION_SPI_CLK_DIV,
	},
};


struct sfi_client_info puma5_sfi_client[PUMA5_NUM_SFI_CLIENTS] =  {
	{
		"Spansion",
		SPANSION_SPI_CLK_DIV,
		SPANSION_PAGE_PROGRAM,
		SPANSION_FAST_READ,
		SPI_DUAL_READ_DISABLE,
		SPANSION_NUM_DUMMY_BYTES_1,/* Fast Read */
		SPANSION_NUM_ADDR_BYTES,
		PUMA5_MM_BASE_CS_0,
		0,
	},
	{
		"Spansion1",
		SPANSION_SPI_CLK_DIV,
		SPANSION_PAGE_PROGRAM,
		SPANSION_FAST_READ,
		SPI_DUAL_READ_DISABLE,
		SPANSION_NUM_DUMMY_BYTES_1,/* Fast Read */
		SPANSION_NUM_ADDR_BYTES,
		PUMA5_MM_BASE_CS_0,
		0,
	},
};
