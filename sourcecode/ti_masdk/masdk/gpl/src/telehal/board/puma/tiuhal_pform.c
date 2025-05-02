/*
 * File name: tiuhal_pform.c
 *
 * Description: Linux kernel Telephony Hardware module (Platform specifc).
 * Provides board specifc routines inside Linux. Any board specific code  
 * should be placed here.
 *
 * Copyright (C) 2011 Texas Instruments, Incorporated
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation version 2.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any kind,
 * whether express or implied; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/version.h>
#include <asm-arm/arch-avalanche/generic/pal.h>
#include <asm-arm/arch-avalanche/generic/pal_sys.h>
#include <asm-arm/arch-avalanche/generic/avalanche_intc.h>
#if defined(CONFIG_MACH_PUMA6)
#include <asm-arm/arch-avalanche/puma6/puma6_bootcfg_ctrl.h>
#endif
#include "tiuhalcfg.h"
#include "rtos_hal.h"
#include "tiuhal.h"

#define DEX_TIU_IND_TRANSITION_EVT  0x0800 /* NOTE: this is to get rid of a userspace include see modules/dex/dimexeif.h to ensure proper value*/

#if defined(CONFIG_MACH_PUMA6)
#define TELE_IRQ                    15      /* External Slac interrupt */
#elif defined(CONFIG_MACH_PUMA5)
#define TELE_IRQ                    0      /* External interrupt 0 */
#else
#error No Architecture specified
#endif

#if defined(CONFIG_MACH_PUMA5)
#define pCODEC_SPI_CORSTCR_REG			((volatile UINT32 *)(IO_ADDRESS(0x08611B5CUL)))
#define CODEC_SPI_CORSTCR_REG			(*pCODEC_SPI_CORSTCR_REG)
#define 	CODEC_SPI_CODEC_OUT_OF_RESET        (0xAUL)
#define 	CODEC_SPI_CODEC_IN_RESET            (0x0UL)
#endif

extern TIUHAL_API_T tiuhal_spi_api;
TIUHAL_API_T *tiuhw_api;
TIUHW_CHIPSEL_T tiuhal_chipselect[TIUHW_MAX_TCIDS];

static void tiuhw_int_disconnect(void);
static int tiuhw_int_connect(void);


/*******************************************************************************
* FUNCTION:    tele_int_handler
*
********************************************************************************
*
* DESCRIPTION: This function is the interrupt handler for the Telephony
*              hardware chipsets found on a SPI bus (typically). It basically
*              posts an event to the MXP DEX process so it can then service
*              the interrupt outside the interrupt context.
* 
* RETURNS:     irqreturn_t
*
*******************************************************************************/

#ifdef TIUHAL_SUPPORT_TID_INTERRUPTS

#if  LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19)
static irqreturn_t tele_int_handler(int irq, void *dev_id, struct pt_regs *regs)
#else
static irqreturn_t tele_int_handler(int irq, void *dev_id)
#endif
{
	static int dex_tid = -1;

	extern int tcb_by_name(char *name);
	extern int mxp_ev_post_by_tid(int tid, unsigned long event);

	if (dex_tid < 0)
		dex_tid = tcb_by_name("DEX");

	if (dex_tid >= 0)
	{
		/* Just post interrupt event to DEX Task */
		mxp_ev_post_by_tid(dex_tid, DEX_TIU_IND_TRANSITION_EVT);
		DRV_TRACE_MSG("tele_interrupt DEX_TIU_IND_TRANSITION_EVT\n");
	}
	else
	{
		DRV_TRACE_MSG("tele_interrupt:No DEX task to deliver interrupt to\n");
	}
	return IRQ_HANDLED; /* For all cases, we'll assume the interrupt is handled */
}
#endif

/*******************************************************************************
* FUNCTION:   tiuhw_int_connect  
*
********************************************************************************
*
* DESCRIPTION:  This function connects the kernel ISR to the TIUHAL handler.
* 
* RETURNS:     int - 0 = OK
*
*******************************************************************************/
static int tiuhw_int_connect()
{
#ifdef TIUHAL_SUPPORT_TID_INTERRUPTS
	int ret, ret2;
	int polarity=0;  /* 0 - active low, 1 - active high */
	int type = 1;   /* 0 – level , 1 – edge/pulse. */

	ret = avalanche_intc_get_interrupt_polarity(TELE_IRQ); 
	ret2 = avalanche_intc_get_interrupt_type(TELE_IRQ);

	DRV_DEBUG_MSG(0, "TELE_IRQ Polarity %x, Type %x\n", ret, ret2);

	DRV_DEBUG_MSG(0, "TELE_IRQ Setting Polarity %x, Type %x\n",
		polarity, type);

	ret = avalanche_intc_set_interrupt_polarity(TELE_IRQ, polarity);

	if (ret == -1)
	{
		DRV_DEBUG_MSG(0,"Failed to set interrupt polarity");
	} 

	ret = avalanche_intc_set_interrupt_type(TELE_IRQ, type);
	if (ret == -1)
	{
		DRV_DEBUG_MSG(0,"Failed to set interrupt type");
	} 

	ret = avalanche_intc_get_interrupt_polarity(TELE_IRQ);
	ret2 = avalanche_intc_get_interrupt_type(TELE_IRQ);
	DRV_DEBUG_MSG(0, "TELE_IRQ Polarity %x, Type %x\n", ret, ret2);

	/* NOTE: we only have 1 interrupt pin generating interrupts, so we don't
     * need to pass a device structure 
     */
	if(request_irq(TELE_IRQ,tele_int_handler,SA_INTERRUPT|IRQF_SAMPLE_RANDOM,"ti_tele", NULL) != 0)
	{
		DRV_DEBUG_MSG(0,"failed to initialize TELE_IRQ");
		return 1;
	}

#endif
    return 0;
}

/*******************************************************************************
* FUNCTION:   tiuhw_int_disconnect  
*
********************************************************************************
*
* DESCRIPTION:  This function disconnects the kernel ISR from the TIUHAL handler.
* 
* RETURNS:      void
*
*******************************************************************************/
static void tiuhw_int_disconnect()
{
#ifdef TIUHAL_SUPPORT_TID_INTERRUPTS
	free_irq(TELE_IRQ, NULL);
#endif
}



/*******************************************************************************
 * Function:  tiuhw_select_tid
 *******************************************************************************
 * DESCRIPTION: This is the board specific code to select the correct TID.
 *
 ******************************************************************************/
int  tiuhw_select_tid(TCID id)
{
	return(tiuhal_chipselect[id]);
}

/*******************************************************************************
 * Function:  tiuhw_deselect_tid
 *******************************************************************************
 * DESCRIPTION:  Deselect the TID
 *
 *
 ******************************************************************************/
void tiuhw_deselect_tid(TCID id)
{
	/* Nothing to do */
}

/*******************************************************************************
* FUNCTION:    tiuhw_init_hal 
*
********************************************************************************
* DESCRIPTION:    Initializes the voice interface (general entry point).
*                 Normally, this will be the place were a FPGA image is to be
*                 downloaded or a subsystem is setup and the API table is 
*                 populated.
*
*******************************************************************************/

BOOL tiuhw_init_hal(TIUHAL_CAP_T *capabilities, void *options)
{

	/* Codec Reset control Register in Boot Cnfg */ 
#if defined(CONFIG_MACH_PUMA6)
    PAL_sysBootCfgCtrl_ReadModifyWriteReg(BOOTCFG_GPCR, BOOTCFG_CODEC_RESET_MASK, BOOTCFG_CODEC_OUT_OF_RESET_VAL);
    PAL_sysBootCfgCtrl_DocsisIo_CODEC_RESET(BOOTCFG_IO_ENABLE);
    /* **FIXME - Need to reset other TID device when LPC enabled */
    //PAL_sysBootCfgCtrl_ReadModifyWriteReg(BOOTCFG_GPCR, BOOTCFG_ZDS_RESET_MASK, BOOTCFG_ZDS_OUT_OF_RESET_VAL);
    //PAL_sysBootCfgCtrl_DocsisIo_ZDS(BOOTCFG_IO_ENABLE);

#elif defined(CONFIG_MACH_PUMA5)
	CODEC_SPI_CORSTCR_REG = CODEC_SPI_CODEC_OUT_OF_RESET;
#endif

	return((tiuhw_api->init)(capabilities, options));
}

/*******************************************************************************
* FUNCTION:      tiuhw_reset_tid()
*
********************************************************************************
* DESCRIPTION:   Perform any resets based on which TID.
*                Board specific. NOTE: this is only used right now by the internal
*                TID HAL and not by the FPGA implementation. We're assuming the TELE_RESET
*                pin was configured in hal initialization code.
*
*******************************************************************************/
UINT32 tiuhw_reset_tid(UINT8 slot, BOOL in_reset)
{
    DRV_DEBUG_MSG(0,"tiuhw_reset_tid: %u %u\n",slot,in_reset);

	/* NOTE: we're slot agnostic in our hardware */
	if(in_reset)
	{
#if defined(CONFIG_MACH_PUMA6)
        PAL_sysBootCfgCtrl_ReadModifyWriteReg(BOOTCFG_GPCR, BOOTCFG_CODEC_RESET_MASK, BOOTCFG_CODEC_OUT_OF_RESET_VAL);
        PAL_sysBootCfgCtrl_DocsisIo_CODEC_RESET(BOOTCFG_IO_DISABLE);
        /* **FIXME - Need to reset other TID device when LPC enabled */
        //PAL_sysBootCfgCtrl_ReadModifyWriteReg(BOOTCFG_GPCR, BOOTCFG_ZDS_RESET_MASK, BOOTCFG_ZDS_OUT_OF_RESET_VAL);
        //PAL_sysBootCfgCtrl_DocsisIo_ZDS(BOOTCFG_IO_DISABLE);

#elif defined(CONFIG_MACH_PUMA5)
        CODEC_SPI_CORSTCR_REG = CODEC_SPI_CODEC_IN_RESET;
#endif
		udelay(10); /* Ensure reset is complete for the h/w*/
	}
	else
  	{
#if defined(CONFIG_MACH_PUMA6)
        PAL_sysBootCfgCtrl_ReadModifyWriteReg(BOOTCFG_GPCR, BOOTCFG_CODEC_RESET_MASK, BOOTCFG_CODEC_OUT_OF_RESET_VAL);
        PAL_sysBootCfgCtrl_DocsisIo_CODEC_RESET(BOOTCFG_IO_ENABLE);
        /* **FIXME - Need to reset other TID device when LPC enabled */
        //PAL_sysBootCfgCtrl_ReadModifyWriteReg(BOOTCFG_GPCR, BOOTCFG_ZDS_RESET_MASK, BOOTCFG_ZDS_OUT_OF_RESET_VAL);
        //PAL_sysBootCfgCtrl_DocsisIo_ZDS(BOOTCFG_IO_ENABLE);

#elif defined(CONFIG_MACH_PUMA5)
        CODEC_SPI_CORSTCR_REG = CODEC_SPI_CODEC_OUT_OF_RESET;
#endif
		udelay(10); /* Ensure reset is complete for the h/w*/ 
	}
	return 0;
}

/*******************************************************************************
 * Function:  Initialize the Telephony module.
 *******************************************************************************
 * DESCRIPTION:
 *
 ******************************************************************************/
int tiuhal_board_init(void)
{
	int i;

    if((tiuhal_spi_api.init)(NULL, NULL) == TIUHAL_INIT_OK)
	{
		tiuhw_api = &tiuhal_spi_api;	

		for(i = 0; i < TIUHW_MAX_TCIDS; i++)
		{
			tiuhal_chipselect[i] = TIUHAL_SPI_MAX_CS;
		}
		if( tiuhw_int_connect() != 0)
		{
			return -EIO;
		}
	}	
	else
	{
		return -EIO;
	}

	return 0;
}

/*******************************************************************************
* FUNCTION:      cleanup_module
*
********************************************************************************
* DESCRIPTION:  Exit from module (normaly called part of rmmod)  
*               
*
*******************************************************************************/
void tiuhal_board_teardown(void)
{
	tiuhw_int_disconnect();

		tiuhw_api->teardown(NULL);
  	DRV_DEBUG_MSG(0,"TIUHW module unloaded\n");
}
