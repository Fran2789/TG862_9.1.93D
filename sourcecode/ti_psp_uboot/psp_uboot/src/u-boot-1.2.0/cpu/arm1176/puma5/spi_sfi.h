/*
 * spi_sfi.h
 * Description:
 * SPI and SFI client data structures.
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

#ifndef __PUMA5_SPI_H__
#define  __PUMA5_SPI_H__

#define SPI_CPHA    				0x01            /* clock phase */
#define SPI_CPOL    				0x02            /* clock polarity */
#define SPI_MODE_0  				(0|0)           /* (original MicroWire) */
#define SPI_MODE_1  				(0|SPI_CPHA)
#define SPI_MODE_2  				(SPI_CPOL|0)
#define SPI_MODE_3  				(SPI_CPOL|SPI_CPHA)
#define SPI_CS_HIGH 				0x04            /* chipselect active high? */
#define SPI_LSB_FIRST   			0x08            /* per-word bits-on-wire */

#define SPI_POL_ACTIVE_HIGH			1
#define SPI_POL_ACTIVE_LOW			0

#define SPI_4_PIN_CMD_MODE			0
#define SPI_3_PIN_CMD_MODE			1

#define PUMA5_CS_0					0
#define PUMA5_CS_1					1
#define PUMA5_CS_2					2
#define PUMA5_CS_3 					3
#define PUMA5_MAX_CS				4

#define NUM_BITS_PER_WORD_8			8
#define NUM_BITS_PER_WORD_16		16
#define NUM_BITS_PER_WORD_24		24
#define NUM_BITS_PER_WORD_32		32

#define SPI_DUAL_READ_ENABLE		1
#define SPI_DUAL_READ_DISABLE		0

#define PUMA5_MAX_SFI_MM			2
#define PUMA5_MM_BASE_CS_0			0x48000000
#define PUMA5_MM_SIZE_CS_0			0x01000000
#define PUMA5_MM_BASE_CS_1			0x4c000000
#define PUMA5_MM_SIZE_CS_1			0x01000000

#define PUMA5_NUM_SPI_CLIENTS		2
#define PUMA5_NUM_SFI_CLIENTS		2

#define SPI_MAX_FLEN 				4096


struct spi_client_info {
	const char *name;
	unsigned char cs;
	unsigned char cmd_mode;
	unsigned char pol;
	unsigned char num_bits_per_word;
	unsigned char spi_cmd_mode;
	unsigned short clk_div;
};

struct sfi_client_info {
	const char *name;
	unsigned short clk_div;			   /* Clk Div val to be used */
    unsigned char write_cmd;           /* Flash Specific WRITE CMD */
    unsigned char read_cmd;            /* Flash Specific READ/FAST_READ/DUAL_READ CMD */
    unsigned short dual_read;          /* 0 for Normal & Fast Read  1 for Dual read  */
    unsigned short num_dummy_bytes;    /* 0 for Normal read 1 for Fast & Dual Read */
    unsigned short num_addr_bytes;     /* 3 bytes for most flashes */
    unsigned int sfi_base;           /* Memory Mapped address for givn CS */
    unsigned short initialized;        /* Used to avoid CMD initilization repetation */
};

int spi_write_then_read( int bank, uchar *tx_data, int tx_len, uchar *rx_data, int rx_len);

#endif /* __PUMA5_SPI_H__ */
