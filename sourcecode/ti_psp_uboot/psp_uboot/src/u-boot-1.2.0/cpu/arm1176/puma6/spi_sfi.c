/*
 * spi_sfi.c
 * Description:
 * See below.
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


/*
 * This file contains functions to initialize the SPI/SFI related
 * functions.
 * The code in this file assumes that data-cache is disabled.
 *
 *      25 Oct 06 BEGR
 *  - Platform-specific SPI/SFI  constants are now provided externally via the
 *  hardware.inc file. The <platform_name>.inc file is copied to hardware.inc
 *  by the clittle/cbig compile script.
 *
 */


/* 
 * Includes Intel Corporation's changes/modifications dated: 2011. 
 * Changed/modified portions - Copyright © 2011 , Intel Corporation.   
 */ 


#include <common.h>
#include <spi.h>
#ifdef SPI_DEBUG
#include <command.h>
#endif


/*-----------------------------------------------------------------------
 * Definitions
 */

/* Base Address */
#define SPI_F_CSR0_MBAR	0x0FFE0100
#define SPI_F_WIN_MBAR	0x08000000

/* Register offset */
#define MODE_CONTL_REG                          ((SPI_F_CSR0_MBAR) + 0x00)
#define ADDR_SPLIT_REG                          ((SPI_F_CSR0_MBAR) + 0x04)
#define CURRENT_ADDR_REG                        ((SPI_F_CSR0_MBAR) + 0x08)
#define COMMAND_DATA_REG                        ((SPI_F_CSR0_MBAR) + 0x0C)
#define INTERFACE_CONFIG_REG                    ((SPI_F_CSR0_MBAR) + 0x10)
#define HIGH_EFFICIENCY_COMMAND_DATA_REG        ((SPI_F_CSR0_MBAR) + 0x20)
#define HIGH_EFFICIENCY_TRANSACTION_PARAMS_REG  ((SPI_F_CSR0_MBAR) + 0x24)  
#define HIGH_EFFICIENCY_OPCODE_REG              ((SPI_F_CSR0_MBAR) + 0x28)


/* Keep Chip Select Low (Command/Data register) */
#define KEEP_CS_LOW_MASK    0x04000000

#define N_ADDR_BYTE_MASK    0x0000F000
#define N_ADDR_BYTE_3       0x00003000
#define N_ADDR_BYTE_4       0x00004000

#undef DEBUG
//#define DEBUG

#if defined(DEBUG)
#define DEBUG(fmt,arg...)  printf(fmt, ##arg)
#else
#define DEBUG(fmt,arg...)
#endif

/* Private functions prototypes */
void spi_chipsel_0(int cs);
void spi_chipsel_1(int cs);
inline u32 endian_swap(u32 x); 



/* Global table oc chip select callback table */
/* This table can be access from all project  */
spi_chipsel_type spi_chipsel[] = {spi_chipsel_0,spi_chipsel_1};

/* Global Number of SPI chips - need by /common/md_spi */
int spi_chipsel_cnt = 2;

/* Global Addressing Mode */
int spi_addr_width = 0;

#ifdef SPI_DEBUG
int spi_debug = 0;
#endif

/*=====================================================================*/
/*                         Public Functions                            */
/*=====================================================================*/

/*-----------------------------------------------------------------------
 * Initialization
 */

void spi_init (void)
{
    unsigned int CtrlMode = 0;

    /* In Puma6 The Atom initialize the controller, uboot does not need to do anything. */
    /* Since uart is not initialize at this point, yet
       Printf not allow at this function ,in the first run of spi_init,
    */
/* Temp For FPGA testing - normally set by ATOM */
#ifdef ENABLE_SPI_INIT
#warning "ENABLE_SPI_INIT is enabled"
    *((volatile uint32_t *)(MODE_CONTL_REG  )) = endian_swap(0x43454020);
    *((volatile uint32_t *)(ADDR_SPLIT_REG  )) = endian_swap(0x00001A18); // 32MB x 2

#endif

    /* Read Addressing Mode */
    CtrlMode = endian_swap(*((volatile uint32_t *)(MODE_CONTL_REG)));
    if ((CtrlMode & N_ADDR_BYTE_4) == N_ADDR_BYTE_4)
    {
        spi_addr_width = 4;
    }
    else
    {
        spi_addr_width = 3;
    }

#ifdef ENABLE_SPI_INIT
#warning "ENABLE_SPI_INIT is enabled"
    if ((CtrlMode & N_ADDR_BYTE_4) == N_ADDR_BYTE_4)
    {
        unsigned int cmd;

        /* Turn Off Chip Select */
        *((volatile uint32_t *)(COMMAND_DATA_REG)) = 0;

        /* Select Chip#0 */
        /* Clear bits 20:21 , Set Bits 16:17 in Mode Control Register */
        CtrlMode = ( (CtrlMode & 0xFFCFFFFF)  | 0x00010000) ;
        *((volatile uint32_t *)(MODE_CONTL_REG)) = endian_swap(CtrlMode);

        /* Write Enable */
        cmd = (0<<26 | 1<<24 | 0x06 << 16);
        *((volatile uint32_t *)(COMMAND_DATA_REG)) = endian_swap(cmd);

        /* Enter 4 Byte Addressing mode */
        cmd = (0<<26 | 1<<24 | 0xB7 << 16);
        *((volatile uint32_t *)(COMMAND_DATA_REG)) = endian_swap(cmd);

        /* Turn Off Chip Select */
        *((volatile uint32_t *)(COMMAND_DATA_REG)) = 0;
    }
    else
    {
         unsigned int cmd;

        /* Turn Off Chip Select */
        *((volatile uint32_t *)(COMMAND_DATA_REG)) = 0;

        /* Select Chip#0 */
        /* Clear bits 20:21 , Set Bits 16:17 in Mode Control Register */
        CtrlMode = ( (CtrlMode & 0xFFCFFFFF)  | 0x00010000) ;
        *((volatile uint32_t *)(MODE_CONTL_REG)) = endian_swap(CtrlMode);

        /* Write Enable */
        cmd = (0<<26 | 1<<24 | 0x06 << 16);
        *((volatile uint32_t *)(COMMAND_DATA_REG)) = endian_swap(cmd);

        /* Exit 4 Byte Addressing mode */
        cmd = (0<<26 | 1<<24 | 0xE9 << 16);
        *((volatile uint32_t *)(COMMAND_DATA_REG)) = endian_swap(cmd);

        /* Turn Off Chip Select */
        *((volatile uint32_t *)(COMMAND_DATA_REG)) = 0;

    }
#endif

    return;

}

/*-----------------------------------------------------------------------
 * SPI transfer
 *
 * This writes "bitlen" bits out the SPI MOSI port and simultaneously clocks
 * "bitlen" bits in the SPI MISO port.  That's just the way SPI works.
 *
 * The source of the outgoing bits is the "dout" parameter and the
 * destination of the input bits is the "din" parameter.  Note that "dout"
 * and "din" can point to the same memory location, in which case the
 * input data overwrites the output data (since both are buffered by
 * temporary variables, this is OK).
 *
 * If the chipsel() function is not NULL, it is called with a parameter
 * of '1' (chip select active) at the start of the transfer and again with
 * a parameter of '0' at the end of the transfer.
 *
 * If the chipsel() function _is_ NULL, it the responsibility of the
 * caller to make the appropriate chip select active before calling
 * spi_xfer() and making it inactive after spi_xfer() returns.
 *
 * spi_xfer() interface:
 *   chipsel: Routine to call to set/clear the chip select:
 *              if chipsel is NULL, it is not used.
 *              if(cs),  make the chip select active (typically '0').
 *              if(!cs), make the chip select inactive (typically '1').
 *   dout:    Pointer to a string of bits to send out.  The bits are
 *              held in a byte array and are sent MSB first.
 *   din:     Pointer to a string of bits that will be filled in.
 *   bitlen:  How many bits to write and read.
 *
 *   Returns: 0 on success, not 0 on failure
 */

int  spi_xfer(spi_chipsel_type chipsel, int bitlen, uchar *dout, uchar *din)
{
    unsigned int tmpdin  = 0;
	unsigned int tmpdout = 0;
	unsigned int bytelen = 0;
    unsigned int bytes = 0;
    unsigned int bytesToSend = 0;
    unsigned int bytesToRead = 0;
    int i=0;

    /*printf("spi_xfer: chipsel %08X dout %08X din %08X bitlen %d\n",(int)chipsel, (uint *)dout, (uint *)din, bitlen); */

    bytelen = bitlen/8;
    if ((bitlen%8) != 0)
    {
        bytelen++;
    }

    /* select the target chip */
	if(chipsel != NULL) {
		(chipsel)(1);
	}

    /* write bytelen bytes to Flash */
    if (dout != NULL)
    {
        bytes=0; 
        while (bytes < bytelen)
        {
            tmpdout = 0;

            /* Set first byte to send */
            tmpdout = (dout[bytes] << 16);
            bytes++;
            bytesToSend = 1;

            /* Set Second byte to send */
            if (bytes < bytelen)
            {
                tmpdout |= dout[bytes] << 8;
                bytes++;
                bytesToSend = 2;
            }
                
            /* Set third byte to send */
            if (bytes < bytelen)
            {
                tmpdout |= dout[bytes];
                bytes++;
                bytesToSend = 3;       
            }
           
            /* Set number of bytes to send, and set CS to stay low. */
            tmpdout |= (KEEP_CS_LOW_MASK | (bytesToSend & 0x03)<<24 );

            /* Write data to register */
            *((volatile uint32_t *)(COMMAND_DATA_REG)) = endian_swap(tmpdout);
#ifdef SPI_DEBUG
            (spi_debug==1)?printf("Write  : 0x%08X\n",tmpdout):0;
#endif
            
        }
    }

    /* read bytelen bytes from Flash */
    if (din != NULL)
    {
        bytes=0; 
        while (bytes < bytelen)
        {
            if  (bytelen - bytes >= 3) 
            {
                bytesToRead = 3;
            }
            else if (bytelen - bytes >= 2)
            {
                bytesToRead = 2;
            }
            else
            {
                bytesToRead = 1;
            }

            /* Write number of bytes to read */
            tmpdin = (KEEP_CS_LOW_MASK | (bytesToRead & 0x03)<<24);
            *((volatile uint32_t *)(COMMAND_DATA_REG)) = endian_swap(tmpdin);
#ifdef SPI_DEBUG
            (spi_debug==1)?printf("Read(W): 0x%08X\n",tmpdin):0;
#endif
            

           /* Read byte */
            tmpdin = endian_swap(*((volatile uint32_t *)(COMMAND_DATA_REG)));
#ifdef SPI_DEBUG
            (spi_debug==1)?printf("Read(R): 0x%08X\n",tmpdin):0;
#endif
            

            for (i=(bytesToRead-1)*8;i>=0;i-=8)
            {
                din[bytes] = (uchar)(tmpdin>>i & 0x000000FF);
                bytes++;
            } 
        }
    }

    /* deselect the target chip */
	if(chipsel != NULL) {
		(chipsel)(0);	

	}

	return(0);
}

/*=====================================================================*/
/*                         Private Functions                           */
/*=====================================================================*/

void spi_chipsel_0(int cs)
{
    uint32_t modeCtrlReg;

    /* Turn Off Chip Select */
    *((volatile uint32_t *)(COMMAND_DATA_REG)) = 0;

    if (cs == 1)
    {
        /* Select Chip#0 */
        /* Clear bits 20:21 , Set Bits 16:17 in Mode Control Register */
        modeCtrlReg = endian_swap(*((volatile uint32_t *)(MODE_CONTL_REG)));
        modeCtrlReg = ( (modeCtrlReg & 0xFFCFFFFF)  | 0x00010000) ;
        *((volatile uint32_t *)(MODE_CONTL_REG)) = endian_swap(modeCtrlReg);   
    }
    return;
}

void spi_chipsel_1(int cs)
{
    uint32_t modeCtrlReg;

    /* Turn Off Chip Select */
    *((volatile uint32_t *)(COMMAND_DATA_REG)) = 0;

    if (cs == 1)
    {
        /* Select Chip#1 */
        /* Clear bits 16:17, Set Bits 20:21 in Mode Control Register */
        modeCtrlReg = endian_swap(*((volatile uint32_t *)(MODE_CONTL_REG)));
        modeCtrlReg = ( (modeCtrlReg & 0xFFFCFFFF)  | 0x00100000) ;
        *((volatile uint32_t *)(MODE_CONTL_REG)) = endian_swap(modeCtrlReg);
    }
    return;
}

/*-----------------------------------------------------------------------
 *  Helper function for word endian swapping
 */
inline u32 endian_swap(u32 x) 
{ 
    int swp = x;
    return ( ((swp&0x000000FF)<<24) + ((swp&0x0000FF00)<<8 ) +
             ((swp&0x00FF0000)>>8 ) + ((swp&0xFF000000)>>24) );
} 


#ifdef CMD_SPI_TESTS
int do_spim (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
    unsigned int cmd = 0;
    int   mode = 0;
    unsigned int modeCtrl;

    // Get new mode
    if (argc >= 2)
        mode = simple_strtoul(argv[1], NULL, 10);

    /* Toggle mode */
    if (mode == 0)
    {
        mode = 4 - spi_addr_width%3;
    }

    printf("Change mode to %d byte addressing mode\n", mode); 
    /* change mode to 3 byte */
    if (mode == 3)
    {
        // Read Mode Ctrl Register
        modeCtrl = endian_swap(*((volatile uint32_t *)(MODE_CONTL_REG)));
        modeCtrl = (modeCtrl & 0xFFFF0FFF) | (3 << 12);
        *((volatile uint32_t *)(MODE_CONTL_REG  )) = endian_swap(modeCtrl);
        spi_addr_width = 3;
    }

    /* change mode to 4 byte */
    if (mode == 4)
    {
        // Read Mode Ctrl Register
        modeCtrl = endian_swap(*((volatile uint32_t *)(MODE_CONTL_REG)));
        modeCtrl = (modeCtrl & 0xFFFF0FFF) | (4 << 12);
        *((volatile uint32_t *)(MODE_CONTL_REG  )) = endian_swap(modeCtrl);
        spi_addr_width = 4;
    }

	return 0;
}


U_BOOT_CMD(
	spim,	2,	1,	do_spim,
	"spim    - Change SPI and Flash Addressing mode\n",
	"<mode> 3 or 4 addressing mode\n"
);


int do_flmode (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
    unsigned int cmd = 0;
    int   mode = 0;
    unsigned int modeCtrl;

    // Get new mode
    if (argc >= 2)
        mode = simple_strtoul(argv[1], NULL, 10);

    /* Toggle mode */
    if (mode == 0)
    {
        mode = spi_addr_width;
    }

    printf("Change FLASH mode to %d byte addressing mode\n", mode); 
    /* change mode to 3 byte */
    if (mode == 3)
    {
        /* Turn Off Chip Select */
        *((volatile uint32_t *)(COMMAND_DATA_REG)) = 0;

        // Read Mode Ctrl Register
        modeCtrl = endian_swap(*((volatile uint32_t *)(MODE_CONTL_REG)));

        /* Select Chip#0 */
        /* Clear bits 20:21 , Set Bits 16:17 in Mode Control Register */
        modeCtrl = ( (modeCtrl & 0xFFCFFFFF)  | 0x00010000) ;
        *((volatile uint32_t *)(MODE_CONTL_REG)) = endian_swap(modeCtrl);

        /* Write Enable */
        cmd = (0<<26 | 1<<24 | 0x06 << 16);
        *((volatile uint32_t *)(COMMAND_DATA_REG)) = endian_swap(cmd);

        /* Exit 4 Byte Addressing mode */
        cmd = (0<<26 | 1<<24 | 0xE9 << 16);
        *((volatile uint32_t *)(COMMAND_DATA_REG)) = endian_swap(cmd);

        /* Turn Off Chip Select */
        *((volatile uint32_t *)(COMMAND_DATA_REG)) = 0;
    }

    /* change mode to 4 byte */
    if (mode == 4)
    {
        /* Turn Off Chip Select */
        *((volatile uint32_t *)(COMMAND_DATA_REG)) = 0;

        // Read Mode Ctrl Register
        modeCtrl = endian_swap(*((volatile uint32_t *)(MODE_CONTL_REG)));

        /* Select Chip#0 */
        /* Clear bits 20:21 , Set Bits 16:17 in Mode Control Register */
        modeCtrl = ( (modeCtrl & 0xFFCFFFFF)  | 0x00010000) ;
        *((volatile uint32_t *)(MODE_CONTL_REG)) = endian_swap(modeCtrl);

        /* Write Enable */
        cmd = (0<<26 | 1<<24 | 0x06 << 16);
        *((volatile uint32_t *)(COMMAND_DATA_REG)) = endian_swap(cmd);

        /* Enter 4 Byte Addressing mode */
        cmd = (0<<26 | 1<<24 | 0xB7 << 16);
        *((volatile uint32_t *)(COMMAND_DATA_REG)) = endian_swap(cmd);

        /* Turn Off Chip Select */
        *((volatile uint32_t *)(COMMAND_DATA_REG)) = 0;
    }

	return 0;
}


U_BOOT_CMD(
	flmode,	2,	1,	do_flmode,
	"flmode    - Change Flash Addressing mode\n",
	"<mode> 3 or 4 addressing mode\n"
);


int do_spireg (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
    int   cmd = 0;
    int   mode = 0;
    unsigned int reg;

    reg = endian_swap(*((volatile uint32_t *)(MODE_CONTL_REG)));
    printf("Mode Control Reg: ..... 0x%08X\n",reg);
   
	return 0;
}


U_BOOT_CMD(
	spireg,	2,	1,	do_spireg,
	"spireg    - Prints SPI Registers\n",
	"Nothing.\n"
);
#endif
