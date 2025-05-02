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

#include <common.h>
#include <spi.h>
#include "spi_sfi.h"

#define SPI_SFI_RESET_BIT          		2
#define SPI_SFI_TIMEOUT  				100
#define MAX_SPI_FRAME_LENGTH 			4096

#define SPI_SFI_BASE_ADDR 				0x08612500
#define SPI_SFI_HW_MODULE_REV         	0x80010000
#define NUM_MMSPI_REGS					4


struct spi_sfi_cntrl_t {
	volatile unsigned int rev_id;    /* MODULE ADDRESS + 0x00 */
	volatile unsigned int clk_ctrl;  /* MODULE ADDRESS + 0x04  */
	volatile unsigned int dev_cfg;   /* MODULE ADDRESS + 0x08  */
	volatile unsigned int cmd;       /* MODULE ADDRESS + 0x0C  */
	volatile unsigned int status;    /* MODULE ADDRESS + 0x10  */
	volatile unsigned int data;      /* MODULE ADDRESS + 0x14  */
	volatile unsigned int mm_spi_setup[NUM_MMSPI_REGS];/* MODULE ADDRESS + 0x18,0x1c,0x20,x024 */
	volatile unsigned int mm_spi_switch; /* MODULE ADDRESS + 0x28 */
};

/*----------------------------CONTROLLER REG FLAGS ---------------------------*/
/* SPI clk Control Reg FLags */
#define SPI_SFI_CLOCK_DIV_MAX			255

#define SPI_SFI_CLK_EN         		 	(1 << 31)
#define SPI_SFI_CLK_DIS         		(0 << 31)
/* SPI Device control Reg Flags */
#define SET_DD0                			0x00
#define SET_DD1                  		0x00
#define SET_DD2                			0x00
#define SET_DD3                  		0x00
#define SET_CKPOL_SLAVE_0       		(0x01)
#define SET_CKPHA_SLAVE_0       		(0x01 << 2)
#define SET_CKPOL_SLAVE_1      			(0x01 << 8)
#define SET_CKPHA_SLAVE_1      			(0x01 << 10)
#define SET_CKPOL_SLAVE_2      			(0x01 << 16)
#define SET_CKPHA_SLAVE_2       		(0x01 << 18)
#define SET_CKPOL_SLAVE_3      			(0x01 << 24)
#define SET_CKPHA_SLAVE_3       		(0x01 << 26)
/* SPI Command Reg Flags */
#define ENABLE_CHIP_SELECT_0			(0x0 << 28)
#define ENABLE_CHIP_SELECT_1			(0x1 << 28)
#define ENABLE_CHIP_SELECT_2			(0x2 << 28)
#define ENABLE_CHIP_SELECT_3			(0x3 << 28)
#define SET_CMD_MODE_3					(1 << 18 )
#define SET_CMD_MODE_4					(0 << 18 )
/*16th and 17th bits of SPI command register*/
#define SPI_CMD_READ            		(0x01 << 16)
#define SPI_CMD_WRITE           		(0x02 << 16)
#define SPI_CMD_DUAL_READ       		(0x03 << 16)
#define SPMODE_LEN(word_len)    		((word_len) << 19)

#define MMSPI_READ_CMD_POS          	0
#define NUM_MMSPI_REGS              	4
#define MMSPI_NUM_ADDR_BYTES_POS    	8
#define MMSPI_NUM_DUMMY_BYTES_POS   	10
#define MMSPI_DUAL_READ_POS         	12
#define MMSPI_WRITE_CMD_POS         	16
#define ENABLE_SFI_MODE					0x1
#define DISABLE_SFI_MODE				0x0
#define SPI_MAX_FLEN					4096

#undef DEBUG

#if defined(DEBUG)
#define DEBUG(fmt,arg...)  printf(fmt, ##arg)
#else
#define DEBUG(fmt,arg...)
#endif



static void spi_chipsel_0(int cs);
static void spi_chipsel_1(int cs);
static void spi_chipsel_2(int cs);
static void spi_chipsel_3(int cs);


static const spi_chipsel_type spi_chipsel[]= {
    spi_chipsel_0,
    spi_chipsel_1,
    spi_chipsel_2,
    spi_chipsel_3,
};

static int setup_sfi_xfer(uchar cs);
static int spi_setup_mode(struct spi_client_info *spi_client);
static int setup_spi_xfer (struct spi_client_info *spi_client);

static unsigned int CMD;
static unsigned int FLEN;
static unsigned char SPI_CMD_MODE;
static unsigned char NUM_BYTES_PER_WORD;

volatile static struct spi_sfi_cntrl_t *spi_sfi_cntrl;
extern struct sfi_client_info puma5_sfi_client[];
extern struct spi_client_info puma5_spi_client[];

/* functions to deal with different sized buffers */
#define avalanche_SPI_RX_BUF(type)                    \
static inline void avalanche_spi_rx_buf_##type(u32 data, void *rx_buff) \
{                                     \
    type * rx = rx_buff;                    \
    *rx++ = (type)data;                       \
    rx_buff = rx;                       \
}

#define avalanche_SPI_TX_BUF(type)              \
static inline u32 avalanche_spi_tx_buf_##type(const void *tx_buff)  \
{                               \
    u32 data;                       \
    const type * tx = (type *)tx_buff;            \
    data = *tx++;                       \
    tx_buff = tx;                 \
    return data;                        \
}

avalanche_SPI_RX_BUF(u8);
avalanche_SPI_RX_BUF(u16);
avalanche_SPI_RX_BUF(u32);
avalanche_SPI_TX_BUF(u8);
avalanche_SPI_TX_BUF(u16);
avalanche_SPI_TX_BUF(u32);


/* put data into buffer */
typedef  void (*GET_rx) (u32 rx_data, void  *rx_buff);

/* get data from buffer */
typedef  u32(*GET_tx) (const void *tx_buffer);

GET_rx get_rx = NULL;
GET_tx get_tx = NULL;


static int dump_spi_regs(unsigned long spi_master )
{
#if 0
    printf("\n SPI Master Reg Contents\n");
    printf("The rev_id   = 0x%x\n",*(volatile unsigned int*)(spi_master + 0x00));
    printf("The clk_ctrl = 0x%x\n",*(volatile unsigned int*)(spi_master + 0x04));
    printf("The dev_cfg  = 0x%x\n",*(volatile unsigned int*)(spi_master + 0x08));
    printf("The cmd      = 0x%x\n",*(volatile unsigned int*)(spi_master + 0x0C));
    printf("The status   = 0x%x\n",*(volatile unsigned int*)(spi_master + 0x10));
    printf("The data     = 0x%x\n",*(volatile unsigned int*)(spi_master + 0x14));
    printf("The mm_spi_setup_reg0 = 0x%x\n",*(volatile unsigned int*)(spi_master + 0x18));
    printf("The mm_spi_setup_reg1 = 0x%x\n",*(volatile unsigned int*)(spi_master + 0x1C));
    printf("The mm_spi_setup_reg2 = 0x%x\n",*(volatile unsigned int*)(spi_master + 0x20));
    printf("The mm_spi_setup_reg3 = 0x%x\n",*(volatile unsigned int*)(spi_master + 0x24));
    printf("The mm_spi_switch = 0x%x\n",*(volatile unsigned int*)(spi_master + 0x28));
    printf("The cmd      = 0x%x\n",*(volatile unsigned int*)(spi_master + 0x0C));
    printf("The mm_spi_switch = 0x%x\n",*(volatile unsigned int*)(spi_master + 0x28));
#endif
	return 0;
};



static void spi_chipsel_0(int cs)
{
	/* Here we can not disable the Chip select */
	 CMD &= 0; /* Reset the previous cmd */
	if( setup_spi_xfer( &puma5_spi_client[0] ) == 0) {
		CMD |= ENABLE_CHIP_SELECT_0;
		DEBUG("CMD val after setup_spi_xfer is 0x%x\n",CMD);
	}else{
		CMD =-1;
		printf(" setup_spi_xfer FAILED for cs 0\n");
	}
	DEBUG("CMD val at  spi_chipsel_0 is 0x%x\n",CMD);
}

static void spi_chipsel_1(int cs)
{
	/* Here we can not disable the Chip select */
	 CMD &= 0;
	if(setup_spi_xfer(&puma5_spi_client[1]) == 0) {
		CMD |= ENABLE_CHIP_SELECT_1;
	}else{
		CMD = -1;
		printf(" setup_spi_xfer FAILED for cs 1\n");
	}

	DEBUG("CMD val at  spi_chipsel_1 is 0x%x\n",CMD);
}
static void spi_chipsel_2(int cs)
{
	/* Here we can not disable the Chip select */
	 CMD &= 0;
	if(setup_spi_xfer(&puma5_spi_client[2]) == 0) {
		CMD |= ENABLE_CHIP_SELECT_2;
	}else{
		CMD  = -1;
		printf("setup_spi_xfer FAILED for cs 2\n");
	}
	DEBUG("CMD val at  spi_chipsel_2 is 0x%x\n",CMD);
}

static void spi_chipsel_3(int cs)
{
	/* Here we can not disable the Chip select */
	CMD &= 0;
	if(setup_spi_xfer(&puma5_spi_client[3]) == 0) {
		CMD |= ENABLE_CHIP_SELECT_3;
	}else{
		CMD = -1;
		printf(" setup_spi_xfer FAILED for cs 3\n");
	}
	DEBUG("CMD val at  spi_chipsel_3 is 0x%x\n",CMD);
}


static int spi_setup_mode(struct spi_client_info *spi_client )
{
	unsigned int regval = 0;

	if(spi_client == NULL) {
		printf(" NULL Argumnet passed to spi_setup function \n");
		return -1;
	}

	SPI_CMD_MODE = spi_client->spi_cmd_mode;
	NUM_BYTES_PER_WORD = spi_client->num_bits_per_word/8;
	/* Set SPI CMD mode 3 or 4 line mode  */

	if(SPI_CMD_MODE == SPI_4_PIN_CMD_MODE)	{
		DEBUG(" Setting 4 Line SPI mode \n");
		CMD |= SET_CMD_MODE_4;
	}else{
		DEBUG(" Setting 3 line SPI mode \n");
		CMD |= SET_CMD_MODE_3;
	}

    /* Assign function pointer to appropriate transfer method 8bit/16bit or
	 * 32bit transfer
	 */
    if (NUM_BYTES_PER_WORD == 1)
    {
		DEBUG("Setting get_tx get_rx for NUM_BYTES_PER_WORD = %d \n",NUM_BYTES_PER_WORD);
        get_rx = avalanche_spi_rx_buf_u8;
        get_tx = avalanche_spi_tx_buf_u8;
    } else if (NUM_BYTES_PER_WORD == 2) {
		DEBUG("Setting get_tx get_rx for NUM_BYTES_PER_WORD = %d \n",NUM_BYTES_PER_WORD);
        get_rx = avalanche_spi_rx_buf_u16;
        get_tx = avalanche_spi_tx_buf_u16;
    } else if (NUM_BYTES_PER_WORD == 4) {
		DEBUG("Setting get_tx get_rx for NUM_BYTES_PER_WORD = %d \n",NUM_BYTES_PER_WORD);
        get_rx = avalanche_spi_rx_buf_u32;
        get_tx = avalanche_spi_tx_buf_u32;
    }else {
        return -1;
    }
	/*
	 * The combination of [CKP, CKPH] creates the “SPI mode”. Most serial
	 * Flash devices only support SPI modes 0 and 3. SPI devices transmit
	 * and receive data on opposite edge’s of the SPI clock.
	 */

	switch( spi_client->cs )
    {
        case 0: /* for Slave device 0 */
            if (spi_client->cmd_mode & SPI_CPHA)
                regval |= SET_CKPHA_SLAVE_0;

            if (spi_client->cmd_mode & SPI_CPOL)
                regval |= SET_CKPOL_SLAVE_0;

            if( spi_client->pol )
                regval |= (1<<1);

			/* only required if operating at higher freq. than SPI controller */
            regval |= SET_DD0;
            break;

        case 1: /* for Slave device 1 */
            if (spi_client->cmd_mode & SPI_CPHA)
                regval |= SET_CKPHA_SLAVE_1;

            if (spi_client->cmd_mode & SPI_CPOL)
                regval |= SET_CKPOL_SLAVE_1;

            if(spi_client->pol)
             	regval |= (1<<9);


            regval |= SET_DD1;

            break;

        case 2: /* for Slave device 2 */
            if (spi_client->cmd_mode & SPI_CPHA)
                regval |= SET_CKPHA_SLAVE_2;

            if (spi_client->cmd_mode & SPI_CPOL)
                regval |= SET_CKPOL_SLAVE_2;

            if(spi_client->pol)
                regval |= (1<<17);

            regval |= SET_DD2;

            break;

        case 3: /* for Slave device 3 */
            if (spi_client->cmd_mode & SPI_CPHA)
                regval |= SET_CKPHA_SLAVE_3;

            if (spi_client->cmd_mode & SPI_CPOL)
                regval |= SET_CKPOL_SLAVE_3;

            if(spi_client->pol)
                regval |= (1<<25);

            regval |= SET_DD3;

            break;

        default:
            break;
    }
	spi_sfi_cntrl->mm_spi_switch &= DISABLE_SFI_MODE;
	spi_sfi_cntrl->dev_cfg = regval;
	DEBUG("Device Configuration Reg val = 0x%x\n",spi_sfi_cntrl->dev_cfg);
	return 0;
}

static int spi_set_clk_freq ( unsigned short clk_div )
{
	/* Disable and enable the CLK */
	spi_sfi_cntrl->clk_ctrl &= SPI_SFI_CLK_DIS;

	if(clk_div > 0 && clk_div <= SPI_SFI_CLOCK_DIV_MAX ){
		spi_sfi_cntrl->clk_ctrl = (SPI_SFI_CLK_EN | clk_div);
	}else {
		DEBUG(" Clk divide Setting to Default \n");
		spi_sfi_cntrl->clk_ctrl |= SPI_SFI_CLK_EN;
		spi_sfi_cntrl->clk_ctrl &= 0x80000000;
	}

	return 0;
}

/* **************************************************************************
 *
 *  Function:    setup_spi_xfer
 *
 *  Description: Setup  SPI-Controller for Xfer
 *
 *  return:      0 on success negetive vale on failure
 *
 * *********************************************************************** */

static int setup_spi_xfer (struct spi_client_info *spi_client)
{
	if( spi_client != NULL ){

		if( spi_set_clk_freq(spi_client->clk_div) == 0)
		{
			if(spi_setup_mode( spi_client ) == 0)
			{
				DEBUG(" SPI setup Success\n");
				DEBUG("Setting word len spi_client->num_bits_per_word -1 = %d\n",(spi_client->num_bits_per_word -1));
				CMD |= SPMODE_LEN( (spi_client->num_bits_per_word -1 ));
				DEBUG("After setting word length CMD val = 0x%x\n",CMD);
			}else{
				printf("SPI Set up failed \n");
				return -1;
			}
		}else{
			printf("spi_set_clk_freq Failed\n");
			return -1;
		}
		DEBUG("CMD val at  setup_spi_xfer is 0x%x\n",CMD);
		return 0;
	}
	else {
		printf("Invalid argument passed for %s",__FUNCTION__ );
		return -1;
	}
}

#if 0
static int wait_for_xfer(void)
{
//	unsigned int timeout = SPI_SFI_TIMEOUT;
	unsigned int timeout =1000;
	/* make sure CNTRL is in SPI mode */
	do{
		if( (spi_sfi_cntrl->status & 0x3) == 0x2)
		{
			return 0;
		}
                printf("[%08x] ", spi_sfi_cntrl->status);
		udelay(10000);
	}while(timeout--);
	return 1;
}
#endif

static int wait_for_xfer(void)
{
	unsigned int timeout = SPI_SFI_TIMEOUT;
	volatile unsigned int stat = 0;

	do{
		stat = spi_sfi_cntrl->status;
		if( (stat & 0x3) == 0x2)
		{
			return 0;
		}
		DEBUG("[%08x]", stat);
	}while(timeout--);

    return 1;
}


static int setup_sfi_xfer( uchar cs )
{
	if( cs < PUMA5_NUM_SFI_CLIENTS )
	{
		spi_sfi_cntrl->mm_spi_switch |= ENABLE_SFI_MODE;
		spi_set_clk_freq(puma5_sfi_client[cs].clk_div);

		if(puma5_sfi_client[cs].initialized == 0 )
		{
			DEBUG(" For the cs = %d Settig up SFI CMDS\n",cs);

			spi_sfi_cntrl->mm_spi_setup[cs] =
				puma5_sfi_client[cs].write_cmd << MMSPI_WRITE_CMD_POS;
			spi_sfi_cntrl->mm_spi_setup[cs] |=
				puma5_sfi_client[cs].dual_read<< MMSPI_DUAL_READ_POS;
			spi_sfi_cntrl->mm_spi_setup[cs] |=
				puma5_sfi_client[cs].num_dummy_bytes << MMSPI_NUM_DUMMY_BYTES_POS;
			spi_sfi_cntrl->mm_spi_setup[cs] |=
				puma5_sfi_client[cs].num_addr_bytes << MMSPI_NUM_ADDR_BYTES_POS;
			spi_sfi_cntrl->mm_spi_setup[cs] |=
				puma5_sfi_client[cs].read_cmd << MMSPI_READ_CMD_POS;
			puma5_sfi_client[cs].initialized = 1;
			DEBUG("spi_sfi_cntrl->mm_spi_setup[%d] = 0x%x\n",cs,spi_sfi_cntrl->mm_spi_setup[cs]);
		}else {
			printf("Already setup_sfi_xfer done for cs = %d\n",cs);
		}
	}else {
		printf(" For the cs = %d SFI Memory Map not enabled in platform \n",cs);
		return -1;
	}
	return 0;
}


int spi_write_then_read( int bank, uchar *tx_data, int tx_len, uchar *rx_data, int rx_len)
{
	unsigned int flen = 0;
	FLEN = 0;

	DEBUG("Inside function %s tx_data = 0x%x tx_len = %d  rx_data = 0x%x  rx_len =%d\n",
			__FUNCTION__, tx_data,tx_len,rx_data,rx_len);

	DEBUG(" FLEN = %d\n",FLEN);

	if(tx_len && tx_data)
		flen = tx_len;

	if(rx_len && rx_data)
		flen += rx_len;

	FLEN = flen -1;

	DEBUG(" after assigning flen = %d  FLEN = %d\n",flen,FLEN);
	if(tx_len && tx_data){
		if(spi_xfer(spi_chipsel[bank], (tx_len * 8) /* byte to bit */, tx_data, NULL) !=(tx_len * 8))
		{
			printf("spi_xfer failed in TX \n");
			return -1;
		}
	}
	if(rx_len && rx_data){
		if (spi_xfer(spi_chipsel[bank], (rx_len * 8), NULL, rx_data)!=(rx_len * 8))
		{
			printf("spi_xfer failed in RX \n");
			return -1;
		}
	}
	return (int)(flen);
}




/* **************************************************************************
 *
 *  Function:    spi_ini
 *
 *  Description: Init SPI-Controller (Nothing much to be done reset all regs )
 *
 *  return:      ---
 *
 * *********************************************************************** */

void spi_init( void )
{

	unsigned short i;
	DEBUG("Inside Function spi_init\n");

	/* Initilise the Controller */
	spi_sfi_cntrl = (struct spi_sfi_cntrl_t *)SPI_SFI_BASE_ADDR;
	DEBUG("Inside Function spi_init spi_sfi_cntrl = 0x%8x\n",spi_sfi_cntrl);

	spi_sfi_cntrl->clk_ctrl = 0;
	spi_sfi_cntrl->dev_cfg 	= 0;
	spi_sfi_cntrl->cmd 		= 0;
	spi_sfi_cntrl->data 	= 0;

	for(i = 0 ;i < PUMA5_NUM_SFI_CLIENTS; i++){
		setup_sfi_xfer(i);
	}
	/* Initilize to SFI mode first */
	spi_sfi_cntrl->clk_ctrl |= SPI_SFI_CLK_EN;
	spi_sfi_cntrl->mm_spi_switch |= ENABLE_SFI_MODE;
	DEBUG("After init SPI regs are \n");
	dump_spi_regs(SPI_SFI_BASE_ADDR);
}




/* **************************************************************************
 *
 *  Function:    spi_xfer
 *
 *  Description: Function to transfer data through SPI
 *				 Sets up SPI clock, mode and cmd and performs data transfer
 *
 *  return:		0 on Success , -ve value on ERROR
 *
 * *********************************************************************** */
int  spi_xfer(spi_chipsel_type chipsel, int bitlen, uchar *d_out, uchar *d_in)
{
	short byte_cnt = bitlen/8;
	uchar *dout = d_out;
	uchar *din = d_in;
	unsigned int rcmd = 0;
	unsigned int wcmd = 0;
	int retval =0;
	spi_sfi_cntrl->mm_spi_switch = DISABLE_SFI_MODE;

	DEBUG("NUM_BYTES_PER_WORD = %d\n",NUM_BYTES_PER_WORD);

	if(chipsel != NULL)
		chipsel(1);	/* This calls setup_spi_xfer */

	if( bitlen == 0){
		printf(" Invalid Argument in function spi_xfer \n");
		retval =-1;
		goto END;
	}

	dump_spi_regs(SPI_SFI_BASE_ADDR);
	DEBUG("In spi_xfer NUM_BYTES_PER_WORD = %d  bitlen =%d dout = 0x%x  \
	din = 0x%x byte_cnt %d \n",NUM_BYTES_PER_WORD,bitlen,dout,din,byte_cnt);

	CMD |= FLEN;

	/* Either Read/Write CMD initiates transfer */
	if(dout)
		wcmd = (CMD | SPI_CMD_WRITE);

	if(din)
		rcmd = (CMD | SPI_CMD_READ);

	DEBUG(" byte_cnt = %d\n",byte_cnt);
	DEBUG("CMD val at  before assigning is 0x%x\n",CMD);

	/* Let us latch the data If we don't need read data just ignore it*/
	while(byte_cnt)
	{
		DEBUG("Inside While loop  for data transfer of byte_cnt = %d\n",byte_cnt);

		if(dout)
		{
			DEBUG("Writing 0x%x to data reg with CMD 0x%x\n", get_tx(dout),wcmd);
			spi_sfi_cntrl->data = get_tx(dout);
			spi_sfi_cntrl->cmd = wcmd;
			if( wait_for_xfer()) {
				printf(" Xfer not complete for long time \n ");
				retval = -1;
				goto END;
			}
			dout += NUM_BYTES_PER_WORD;
		}

		if( din )
		{
			DEBUG("Reading with CMD 0x%x\n",rcmd);
			spi_sfi_cntrl->cmd = rcmd;
			if( wait_for_xfer() ) {
				printf(" Xfer not complete for long time \n");
				retval = -1;
				goto END;
			}
			get_rx(spi_sfi_cntrl->data, din);
			DEBUG("\n data READ is 0x%x\n", *din);
			din += NUM_BYTES_PER_WORD;
		}
		byte_cnt -= NUM_BYTES_PER_WORD;
		if(byte_cnt < 0)
			break;
	}
	/* If Xfer is Successful */
	if(byte_cnt <= 0){
		retval = bitlen;
	}
END:
	spi_sfi_cntrl->mm_spi_switch = ENABLE_SFI_MODE;
	return retval;
}
