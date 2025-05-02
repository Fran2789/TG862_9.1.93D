/*
 * File name: hw_dspmod.c
 *
 * Description: Linux kernel DSP module. Provides DSP hardware routines through 
 *              SYSCALL_DIMHW system call.
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>
#include <asm-arm/arch-avalanche/generic/pal.h>
#include <asm-arm/arch-avalanche/generic/soc.h>

/* **TODO - This is temporary fix for PSM issue which causes system HANG when we do
   TDM IN RESET, noted to fix this issue in Phase 2 */
//#define PSM_TEMP_DISABLE_TDM_SHUTDOWN

#if defined(CONFIG_MACH_PUMA6)
/* **TODO - we need to Remove/replace below headers with common files for Puma */
#include <asm-arm/arch-avalanche/puma6/puma6.h>
#include <asm-arm/arch-avalanche/puma6/puma6_cru_ctrl.h> 
#include <asm-arm/arch-avalanche/puma6/puma6_clk_cntl.h>
#include <asm-arm/arch-avalanche/puma6/p_unit_api.h>

#elif defined(CONFIG_MACH_PUMA5)
/* **TODO - we need to Remove these headers with common files for both p5 and p6 */
#include <asm-arm/arch-avalanche/puma5/puma5.h>
#include <asm-arm/arch-avalanche/generic/pal_sysPsc.h>
#include <asm-arm/arch-avalanche/puma5/puma5_clk_cntl.h>

#else
#error No Architecture specified
#endif


#include "dimhw_gw.h"

#ifndef GG_NUM_DSPS
#define GG_NUM_DSPS                    1 /* This is the maximum value */
#endif

#ifndef GG_DSP_TYPE_DEFINED
#define GG_DSP_TYPE_DEFINED
typedef enum
{
    /* Unknown DSP type */
    GG_DSP_TYPE_UNKNOWN = 0,

    GG_DSP_YAMUNA       = 22,
    GG_DSP_PUMA5        = 24,
    GG_DSP_PUMA6        = 30 

} GG_DSP_TYPE_T;
#endif

#if defined(CONFIG_MACH_PUMA6)
#define C55_CFG1_OFFSET  0x178
#define C55_CFG1_BA_MASK 0x00FF0000
#endif

// ARRIS ADD - to help make ZSI mode run-time instead ofcompile/config flag
static int zsimode = 0;
module_param(zsimode, int, S_IRUGO);
// END ARRIS ADD

/*********************************************************************************
* FUNCTION DESCRIPTION: Reset/Bring out of reset PUMA-X DSP.
* INPUT PARAMS: dsp number                                   (INPUT)
*             status -> TRUE  means reset dsp              (INPUT)
*                       FALSE means bring dsp out of reset (INPUT)
* RETURN VALUE:
**********************************************************************************/
void hwu_lin_puma_dsp_reset(UINT32 dsp, BOOL status)
{
	printk(KERN_WARNING "%d: %s PUMA DSPSS \n", __LINE__, 
        (status) ? "Disabling " : "Enabling ");           

    printk(KERN_WARNING "%d: Taking PUMA DSPSS %s RESET \n", __LINE__,
           (status) ? "IN " : "OUT of ");
    
    /* Now setup DSPSS reset */
    /* PAL_sysResetCtrl(AVALANCHE_C55X_RESET_BIT, 
                     (status) ? IN_RESET : OUT_OF_RESET);*/

	if (status)
	{
#if defined(CONFIG_MACH_PUMA6)

		/* Disable Puma6 DSP Clock and put it in reset */
        PAL_sysResetCtrl(CRU_NUM_C55, IN_RESET);
        PAL_sysResetCtrl(CRU_NUM_DSP_PROXY, IN_RESET);
        PAL_sysResetCtrl(CRU_NUM_DSP_INC, IN_RESET);

#elif defined(CONFIG_MACH_PUMA5)
        /* Disable Puma5 DSP clock and put it in reset */
		if (PAL_sysPscSetModuleState(PSC_DSPSS, PSC_DISABLE) != 0) 
		{ 
		 printk(KERN_WARNING "Set DSPSS in power saving mode error \n"); 
		} 
		mdelay(100);

		REG32_WRITE(0x08621a08, 0x03);
		REG32_WRITE(0x08621120, 0x1);
#endif
	}
	else
	{
#if defined(CONFIG_MACH_PUMA6)

        /* Enable Puma6 DSP Clock and take it out of reset */
        PAL_sysResetCtrl(CRU_NUM_DSP_INC, OUT_OF_RESET);
        PAL_sysResetCtrl(CRU_NUM_DSP_PROXY, OUT_OF_RESET);
        PAL_sysResetCtrl(CRU_NUM_C55, OUT_OF_RESET);
#elif defined(CONFIG_MACH_PUMA5)
        /* Enable Puma5 DSP Clock and take it out of reset */
		if (PAL_sysPscSetModuleState(PSC_DSPSS, PSC_ENABLE) != 0) 
		{ 
		 printk(KERN_WARNING "Set DSPSS in power saving mode error \n"); 
		} 
		mdelay(100);

        REG32_WRITE(0x08621a08, 0x103);
        REG32_WRITE(0x08621120, 0x1);
#endif
    }
}

/*********************************************************************************
* FUNCTION DESCRIPTION: enable/disable DSP NMI interrupt.
* INPUT PARAMS: dsp number                                  (INPUT)
*             status -> TRUE  means disable NMI             (INPUT)
*                       FALSE means enable NMI              (INPUT)
* RETURN VALUE:
**********************************************************************************/
void hwu_lin_puma_dsp_nmi(UINT32 dsp, BOOL status)
{
    printk(KERN_INFO "DSP NMI = %s.\n", status ? 
		"Enabled" : "Disabled");
    
    /* Now set DSP NMI */
#if defined(CONFIG_MACH_PUMA6)
	REG32_WRITE(PUMA6_NMI_REG, status);
#elif defined(CONFIG_MACH_PUMA5)
    REG32_WRITE(PUMAV_NMI_REG, status);
#endif
}

void hwu_lin_puma_dsp_halt(UINT32 dsp_num)
{
	printk(KERN_WARNING "%d: PUMA DSPSS Halt \n", __LINE__);
    hwu_lin_puma_dsp_reset(0, TRUE);
}

#if defined(CONFIG_MACH_PUMA5)
/*********************************************************************************
* FUNCTION DESCRIPTION: Control power in PUMA-V DSP.
* INPUT PARAMS: dsp number                                (INPUT)
*             status -> TRUE  means scale system clock down     (INPUT)
*                       FALSE means scale system clock back      
* RETURN VALUE:
**********************************************************************************/
void hwu_lin_pumav_dsp_power_control(UINT32 dsp, BOOL status)
{
    UINT32 timer_return_value;

    printk(KERN_WARNING "%d: Scaling %s system clock \n", __LINE__,
           (status) ? "down" : "up");
    
    /* Now setup system clock */
    if (status)
    {
        if (PAL_sysClkcSetFreq (PAL_SYS_CLKC_ARM, (AVALANCHE_ARM_FREQ_DEFAULT/2)) != 0)
        {
            printk(KERN_WARNING "DSPSS power control error \n");
        }

        timer_return_value =
            PAL_sysTimer16SetParams(AVALANCHE_TIMER1_BASE, PAL_sysClkcGetFreq(CLKC_VBUS), 
                    TIMER16_CNTRL_AUTOLOAD, 10000, NULL);
        if(timer_return_value == -1)
        {
            printk(KERN_WARNING "Error setting parameters for timer\n");
        }
    }
    else
    {
        if (PAL_sysClkcSetFreq (PAL_SYS_CLKC_ARM, AVALANCHE_ARM_FREQ_DEFAULT) != 0)
        {
            printk(KERN_WARNING "DSPSS power control error \n");
        }

        timer_return_value =
            PAL_sysTimer16SetParams(AVALANCHE_TIMER1_BASE, PAL_sysClkcGetFreq(CLKC_VBUS), 
                    TIMER16_CNTRL_AUTOLOAD, 5000, NULL);
        if(timer_return_value == -1)
        {
            printk(KERN_WARNING "Error setting parameters for timer\n");
        }
     }
}
#endif

/*********************************************************************************
* FUNCTION DESCRIPTION: Reset TDM in PUMA DSP.
* INPUT PARAMS: dsp number                                (INPUT)
*             status -> TRUE  means TDM in reset          (INPUT)
*                       FALSE means TDM out of reset      (INPUT)
* RETURN VALUE:
**********************************************************************************/
void hwu_lin_puma_reset_tdm(UINT32 dsp, BOOL status)
{
    /* Currently this function support only TDM1 */
    /* To support TDM0 we can add a tdm_number argument in this IOCTL */

#if defined(CONFIG_MACH_PUMA6)

/* TDM shutdown is temporarily desabled for Puma-6 for PSM mode */
#if defined(PSM_TEMP_DISABLE_TDM_SHUTDOWN)
    printk(KERN_WARNING "%d: Taking TDM %s RESET \n", __LINE__,
           (status) ? "IN" : "OUT of");

    if (status)
    {
        /* Disable TDM0 and TDM1 Clock and put it in reset */
        PAL_sysResetCtrl(CRU_NUM_TDM00, IN_RESET);
        PAL_sysResetCtrl(CRU_NUM_TDM01, IN_RESET);
        PAL_sysResetCtrl(CRU_NUM_TDM10, IN_RESET);
        PAL_sysResetCtrl(CRU_NUM_TDM11, IN_RESET);
    }
    else
    {
        /* Enable TDM0 and TDM1 Clock and take it out of reset */
        PAL_sysResetCtrl(CRU_NUM_TDM00, OUT_OF_RESET);
        PAL_sysResetCtrl(CRU_NUM_TDM01, OUT_OF_RESET);
        PAL_sysResetCtrl(CRU_NUM_TDM10, OUT_OF_RESET);
        PAL_sysResetCtrl(CRU_NUM_TDM11, OUT_OF_RESET);
    }
#endif

#elif defined(CONFIG_MACH_PUMA5)

/* Puma-5 PSM Fix : MCBSP Synch error Due to TDM IN and OUT OF RESET */
#if defined(PSM_TEMP_DISABLE_TDM_SHUTDOWN)
    printk(KERN_WARNING "%d: Taking TDM %s RESET \n", __LINE__,
           (status) ? "IN" : "OUT of");

	if (status)
	{
		/* Disable TDM Clock and put it in reset */
		if (PAL_sysPscSetModuleState(PSC_TDM, PSC_DISABLE) != 0)
		{
			printk(KERN_WARNING "Set TDM in power saving mode error \n");
		}
	}
	else
	{
		/* Enable TDM Clock and take it out of reset */
		if (PAL_sysPscSetModuleState(PSC_TDM, PSC_ENABLE) != 0)
		{
			printk(KERN_WARNING "Set TDM in power saving mode error \n");
		}
	 }
#endif

#endif

}

/*********************************************************************************
* FUNCTION DESCRIPTION: This is the main entry point for initializing the DSP H/w
* Paramters : None
*
**********************************************************************************/
static int hwu_dsp_init(void)
{
	printk(KERN_DEBUG "Entering hwu_dsp_init (%d)\n", zsimode); // ARRIS CHANGE debugging
    
#if defined(CONFIG_MACH_PUMA6)

	printk( KERN_INFO "%d: PUMA6 DSPSS is %s RESET (pre)\n", __LINE__,
           ((PAL_sysGetResetStatus(CRU_NUM_C55) == IN_RESET) ?
            "IN " : "OUT of "));
	printk( KERN_INFO "%d: PUMA6 TDM00 is %s RESET (pre)\n", __LINE__,
           ((PAL_sysGetResetStatus(CRU_NUM_TDM00) == IN_RESET) ?
            "IN " : "OUT of "));
    printk( KERN_INFO "%d: PUMA6 TDM01 is %s RESET (pre)\n", __LINE__,
           ((PAL_sysGetResetStatus(CRU_NUM_TDM01) == IN_RESET) ?
            "IN " : "OUT of "));
#elif defined(CONFIG_MACH_PUMA5)

	printk( KERN_INFO "%d: PUMAV DSPSS is %s RESET (pre)\n", __LINE__,
           ((PAL_sysGetResetStatus(AVALANCHE_C55X_RESET_BIT) == IN_RESET) ?
            "IN " : "OUT of "));
	printk( KERN_INFO "%d: PUMAV TDM is %s RESET (pre)\n", __LINE__,
           ((PAL_sysGetResetStatus(AVALANCHE_TDM_RESET_BIT) == IN_RESET) ?
            "IN " : "OUT of "));
#endif

#if defined(CONFIG_MACH_PUMA6)

if (zsimode != 1)  // ARRIS CHANGE
{                  // ARRIS CHANGE   
	/* Docsis IOs enable (offset: 0x000C_0144). */
	/* Disable PIN Muxing  IOs of the CODEC0 bus, TDM0 bus, and ZDS */
	/* Clear bit2 (CODEC0), bit4 (TDM0) and bit9 (ZDS) */
    PAL_sysBootCfgCtrl_DocsisIo_CODEC0(BOOTCFG_IO_DISABLE); 
    PAL_sysBootCfgCtrl_DocsisIo_TDM0(BOOTCFG_IO_DISABLE);
    PAL_sysBootCfgCtrl_DocsisIo_ZDS(BOOTCFG_IO_DISABLE);
	
	/* Enable PIN Muxing  IOs of the CODEC1 bus, TDM1 bus */
    /* Set bit3 (CODEC1), bit5 (TDM1) */
	PAL_sysBootCfgCtrl_DocsisIo_CODEC1(BOOTCFG_IO_ENABLE);  
    PAL_sysBootCfgCtrl_DocsisIo_TDM1(BOOTCFG_IO_ENABLE);

	/* Boot Config GPCR register to control devices. Codec reset0 - Write “1” to take TID0 out of reset
	   Tdm1_zds_mode - Should be changed to 1 to disable ZDS, TDM1 port will come functional */
    PAL_sysBootCfgCtrl_ReadModifyWriteReg(BOOTCFG_GPCR, BOOTCFG_ZDS_DISABLE_MASK, BOOTCFG_ZDS_DISABLE_VAL); 
         
    PAL_sysBootCfgCtrl_ReadModifyWriteReg(BOOTCFG_GPCR, BOOTCFG_CODEC_RESET_MASK, BOOTCFG_CODEC_OUT_OF_RESET_VAL); 
	/* Fix: Loop Current drop: enbling Pix muxing after CODEC mask */
    PAL_sysBootCfgCtrl_DocsisIo_CODEC_RESET(BOOTCFG_IO_ENABLE);

    {
        unsigned int resetReason;

        p_unit_get_reset_reason((unsigned long *)&resetReason);
        resetReason &= 0x00FF;

        /* RESET_COLD_BOOT  means AC reboot*/
        if (resetReason == RESET_COLD_BOOT)
        {
            PAL_sysBootCfgCtrl_ReadModifyWriteReg(BOOTCFG_GPCR,
                                                  BOOTCFG_CODEC_RESET_MASK,
                                                  BOOTCFG_CODEC_IN_RESET_VAL);
            /* sleep at least 20 us */
            usleep_range(20, 30);
            PAL_sysBootCfgCtrl_ReadModifyWriteReg(BOOTCFG_GPCR,
                                                  BOOTCFG_CODEC_RESET_MASK,
                                                  BOOTCFG_CODEC_OUT_OF_RESET_VAL);
        }
    }
}     // ARRIS CHANGE
else  // ARRIS CHANGE
{     // ARRIS CHANGE
    /* Disable PIN Muxing IOs of the CODEC0, CODEC1,and TDM0 */
    PAL_sysBootCfgCtrl_DocsisIo_CODEC1     (BOOTCFG_IO_DISABLE);
    PAL_sysBootCfgCtrl_DocsisIo_CODEC0     (BOOTCFG_IO_DISABLE);

    PAL_sysBootCfgCtrl_DocsisIo_TDM0       (BOOTCFG_IO_DISABLE);

    /* Enabling TDM1 in ZSI mode - for enable the Frame Sync clock into TDM1 */
    PAL_sysBootCfgCtrl_DocsisIo_TDM1       (BOOTCFG_IO_ENABLE);

    /* Set 'Boot and Config' register - Enable ZDS Mode */
    PAL_sysBootCfgCtrl_ReadModifyWriteReg(BOOTCFG_GPCR, BOOTCFG_ZDS_DISABLE_MASK, BOOTCFG_ZDS_ENABLE_VAL);    
                
    /* Set 'Boot and Config' register - Digital PLL */
    PAL_sysBootCfgCtrl_ReadModifyWriteReg(BOOTCFG_GPCR, BOOTCFG_DPLL_CLK_RESET_MASK, BOOTCFG_DPLL_CLK_OUT_OF_RESET_VAL); 

    /* Set 'Boot and Config' register - Enable ZDS Fast Clock Divider*/
    PAL_sysBootCfgCtrl_ReadModifyWriteReg(BOOTCFG_GPCR, BOOTCFG_ZDS_DIV_CLK_RESET_MASK, BOOTCFG_ZDS_DIV_CLK_OUT_OF_RESET_VAL);

    /* Set 'Boot and Config' register - Codec0 and Codec1 out of reset */
    PAL_sysBootCfgCtrl_ReadModifyWriteReg(BOOTCFG_GPCR, BOOTCFG_CODEC_RESET_MASK, BOOTCFG_CODEC_OUT_OF_RESET_VAL);

    /* Fix for loop voltage issue on ZSI mode, by blocking ZSI IOs during ZSI reset */
    /* Enable PIN Muxing IOs of the ZDS */  
    PAL_sysBootCfgCtrl_DocsisIo_ZDS        (BOOTCFG_IO_ENABLE); 
    
    /* Fix: Loop Current drop: enbling Pix muxing after CODEC mask */
    PAL_sysBootCfgCtrl_DocsisIo_CODEC_RESET(BOOTCFG_IO_ENABLE);

} /* ARRIS CHANGE - CONFIG_SPI_TI_CODEC_ZSI_MODE  */

    /* Prioritize DSP in the bunit */
    PAL_sysBootCfgCtrl_SetDspPriority(BOOTCFG_IO_ENABLE);

    /* Enable C55 Emulation pin IOs */
    PAL_sysBootCfgCtrl_DocsisIo_C55_EMU0(BOOTCFG_IO_ENABLE);
    PAL_sysBootCfgCtrl_DocsisIo_C55_EMU1(BOOTCFG_IO_ENABLE);
	
#endif  /* CONFIG_MACH_PUMA6 */

	/* Bring DSP In reset */
    hwu_lin_puma_dsp_reset(0, TRUE);

    mdelay(100);

    return 0;
}

static int dspmod_device_open = 0;

/********************************************************************
 * This is called whenever a process attempts to open the device file
 ********************************************************************/
static int dspmod_open(struct inode *inode, struct file *file)
{
    if (dspmod_device_open)
    {
        return -EBUSY;
    }
    dspmod_device_open++;
    try_module_get(THIS_MODULE);
    return 0;
}

/********************************************************************
 * This is called whenever a process attempts to close the device file
 ********************************************************************/
static int dspmod_release(struct inode *inode, struct file *file)
{
    /*
     * We're now ready for our next caller
     */
    dspmod_device_open--;
    module_put(THIS_MODULE);
    return 0;
}

/*******************************************************************************
 * Function:  dspmod_ioctl
 *******************************************************************************
 * DESCRIPTION: ioctl implementation for dspmod.
 *
 ******************************************************************************/
static long dspmod_ioctl(struct file *file,
                unsigned int ioctl_num,
                unsigned long ioctl_param)
{
    /*
    * Sanity check.
    */
    if ((_IOC_TYPE(ioctl_num) != DSPMOD_IOCTL_MAGIC) 
       || (_IOC_NR(ioctl_num) > DSPMOD_DEV_IOC_MAXNR)) return -ENOTTY;
    
    switch (ioctl_num)
    {
        case DSPMOD_IOCTL_QUERY_DSP_TYPE:
        {
#if defined(CONFIG_MACH_PUMA6)
            return put_user((unsigned long)GG_DSP_PUMA6, (unsigned long __user *)ioctl_param);
#elif defined(CONFIG_MACH_PUMA5)
            return put_user((unsigned long)GG_DSP_PUMA5, (unsigned long __user *)ioctl_param);
#endif
        }

        case DSPMOD_IOCTL_QUERY_DSP_NUM:
        {
            return put_user((unsigned long)GG_NUM_DSPS, (unsigned long __user *)ioctl_param);
        }

        case DSPMOD_IOCTL_DSP_HALT:
        {
            unsigned short dsp_num;
            if(get_user(dsp_num, (unsigned short __user *)ioctl_param))
            {
                return -EFAULT;
            }
            hwu_lin_puma_dsp_halt(dsp_num);
            break;
        }

        case DSPMOD_IOCTL_DSP_RESET:
        {
            GW_DSPMOD_PARAM_T data;
            if(get_user(data.v, (unsigned long __user *)ioctl_param))
            {
                return -EFAULT;
            }
            hwu_lin_puma_dsp_reset(data.u.dsp_num, (data.u.bool_value) ? TRUE : FALSE);
            break;
        }

        case DSPMOD_IOCTL_QUERY_DSP_ADDR:
        {
            unsigned long start_addr;

#if defined(CONFIG_MACH_PUMA6)

			if ( avalanche_alloc_no_OperSys_memory(eNO_OperSys_VDSP, PUMA6_DSP_EXTERNAL_MEM_SIZE, (unsigned int *)&start_addr) != 0)
            {
                printk("DSPMOD query DSP address syscall: Alloc API failed!\n");
                return put_user((unsigned long)0, (unsigned long __user *)ioctl_param);
            }
            else
            {
                printk("DSPMOD query DSP address syscall: DSP Start address = 0x%08X\n", (unsigned int)start_addr);

				/* Set DSP base address  */
                /* First one is C55_CFG1 in BootCfg. It can be used when the DSP is in reset and will override the default internal DSP register */
                PAL_sysBootCfgCtrl_ReadModifyWriteReg(C55_CFG1_OFFSET, C55_CFG1_BA_MASK, ((start_addr&0xFF000000) >> 8)); /* Configure DSP external address */

                /* Bring TDM IN Reset */
				hwu_lin_puma_reset_tdm(0, FALSE);
       
                return put_user((unsigned long)start_addr, (unsigned long __user *)ioctl_param);
            }		
#elif defined(CONFIG_MACH_PUMA5)
            /* Make sure the DSP-SS is out of RESET */
            REG32_WRITE(0x08621a08, 0x03);
            REG32_WRITE(0x08621120, 0x1);

            /* Take DSP CPPI Proxy out of reset */
            REG32_WRITE(0x08621a8c, 0x103);
            REG32_WRITE(0x08621120 , 0x01);

            if ( avalanche_alloc_no_OperSys_memory(eNO_OperSys_VDSP, PUMAV_DSP_EXTERNAL_MEM_SIZE, (unsigned int *)&start_addr) != 0)
            {
                printk("DSPMOD query DSP address syscall: Alloc API failed!\n");
                /* Default 32 MB start_addr = 0x81E00000; */
                return put_user((unsigned long)0, (unsigned long __user *)ioctl_param);
            }
            else
            {
                printk("DSPMOD query DSP address syscall: DSP Start address = 0x%08X\n", (unsigned int)start_addr);
                REG32_WRITE(PUMAV_EXT_ADDR_CFG_REG, start_addr&0xff000000);  /* Configure DSP external address */
                printk("HW_DSP DSP External Config Register set to: 0x%08X, Start: 0x%08X\n",
                        (unsigned int)REG32_DATA(PUMAV_EXT_ADDR_CFG_REG), (unsigned int)start_addr);

                return put_user((unsigned long)start_addr, (unsigned long __user *)ioctl_param);
            }
#endif
        }

        case DSPMOD_IOCTL_DSP_NMI:
        {
            GW_DSPMOD_PARAM_T data;
            if(get_user(data.v, (unsigned long __user *)ioctl_param))
            {
                return -EFAULT;
            }
            hwu_lin_puma_dsp_nmi(data.u.dsp_num, (data.u.bool_value) ? TRUE : FALSE);
            break;
        }

        case DSPMOD_IOCTL_DSP_PWR:
        {
            GW_DSPMOD_PARAM_T data;
            if(get_user(data.v, (unsigned long __user *)ioctl_param))
            {
                return -EFAULT;
            }

#if defined(CONFIG_MACH_PUMA5)
            hwu_lin_pumav_dsp_power_control(data.u.dsp_num, (data.u.bool_value) ? TRUE : FALSE);
#endif
            break;
        }

        case DSPMOD_IOCTL_RESET_TDM:
        {
            GW_DSPMOD_PARAM_T data;
            if(get_user(data.v, (unsigned long __user *)ioctl_param))
            {
                return -EFAULT;
            }
            hwu_lin_puma_reset_tdm(data.u.dsp_num, (data.u.bool_value) ? TRUE : FALSE);
            break;
        }
       
        case DSPMOD_IOCTL_REG_READ:
        {
            unsigned long register_val = 0, register_addr = 0;

	    if(get_user(register_addr, (unsigned long __user *)ioctl_param))
            {
                return -EFAULT;
            }
	    REG32_READ(register_addr, register_val);
	    return put_user(register_val, (unsigned long __user *)ioctl_param);
        }

    default:
        {
            printk("Unknown ioctl (0x%x) called: Type=%d\n", ioctl_num, _IOC_TYPE(ioctl_num));
            return -EINVAL;
        }

    } /* switch (ioctl_num) */
    
    return 0;
}

static ssize_t dspmod_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
	printk(KERN_WARNING "dspmod_read not supported\n");
	return 0;
}

static ssize_t dspmod_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
	printk(KERN_WARNING "dspmod_write not supportedd\n");
	return 0;
}

static const struct file_operations dspmod_fops = {
	.owner		= THIS_MODULE,
    .open       = dspmod_open,
    .release    = dspmod_release,
	.unlocked_ioctl		= dspmod_ioctl,
	.read		= dspmod_read,
	.write		= dspmod_write,
};

static struct miscdevice dspmod_miscdev = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= DSPMOD_DEVICE_NAME,
	.fops	= &dspmod_fops,
};

static int __init dspmod_init_module(void)
{
    int err;
    if((err = hwu_dsp_init()) == 0)
    {
    	err = misc_register(&dspmod_miscdev);
	    if (err) 
        {
	    	printk(KERN_ERR "dspmod: cannot register miscdev err=%d\n", err);
	    }
        printk("HW_DSP module loaded\n");
    }
    return err;
}

static void __exit dspmod_cleanup_module(void)
{
    int err;

    err = misc_deregister(&dspmod_miscdev);
    if(err)
    {
        printk(KERN_ERR "dspmod: cannot de-register miscdev err=%d\n", err);
    }
#if defined(CONFIG_MACH_PUMA6)
	/* Disable C55, TDM0, TDM1 Clocks and put in reset */
    PAL_sysResetCtrl(CRU_NUM_C55, IN_RESET);
    PAL_sysResetCtrl(CRU_NUM_DSP_PROXY, IN_RESET);
    PAL_sysResetCtrl(CRU_NUM_DSP_INC, IN_RESET);
    PAL_sysResetCtrl(CRU_NUM_TDM00, IN_RESET);
    PAL_sysResetCtrl(CRU_NUM_TDM01, IN_RESET);
    PAL_sysResetCtrl(CRU_NUM_TDM10, IN_RESET);
    PAL_sysResetCtrl(CRU_NUM_TDM11, IN_RESET);

#elif defined(CONFIG_MACH_PUMA5)
    /* Put C55 and TDM0 in reset */
    PAL_sysResetCtrl(AVALANCHE_TDM_RESET_BIT,IN_RESET);
    PAL_sysResetCtrl(AVALANCHE_C55X_RESET_BIT,IN_RESET);

#endif

    printk("HW_DSP module unloaded\n");
}

module_init(dspmod_init_module);
module_exit(dspmod_cleanup_module);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Texas Instruments Incorporated");
MODULE_DESCRIPTION("TI Hardware DSP access module.");
MODULE_SUPPORTED_DEVICE("Texas Instruments "DSPMOD_DEVICE_NAME);

