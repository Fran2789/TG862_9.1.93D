/*
 *
 * soc_docsis_global_driver.c
 * Description:
 * SoC docsis Global Driver implementation
 *
 * Copyright (C) 2008-2012 Texas Instruments Incorporated - http://www.ti.com/ 
 * 
 * 
 *  Redistribution and use in source and binary forms, with or without 
 *  modification, are permitted provided that the following conditions 
 *  are met:
 *
 *    Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer.
 *
 *    Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the 
 *    documentation and/or other materials provided with the   
 *    distribution.
 *
 *    Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
*/

/*! \file soc_docsis_global_driver.c
    \brief Implementation of SoC docsis Global Driver. 
*/


#include <linux/module.h>	/* module */
#include <linux/bitops.h>	/* module */
#include <asm/uaccess.h>
#include <puma.h>
#include <hardware.h>
#include "pal.h"
#include "soc_interface_driver.h"
#include "soc_docsis_global_driver.h"
#if defined (CONFIG_MACH_PUMA6)
#include "iosfsb_api.h"
#endif

#if defined (CONFIG_MACH_PUMA5)
/**********************************************/
/* Definitions for SoC DOCSIS Global Unit     */
/**********************************************/

#define PUMA5_DOCSIS_ANALOG_CR_0_OFFSET         (0x11B44)
#define PUMA5_DOCSIS_ANALOG_CR_1_OFFSET         (0x11B48)
#define PUMA5_DOCSIS_ANALOG_CR_2_OFFSET         (0x11B4C)
#define PUMA5_DOCSIS_ANALOG_CR_3_OFFSET         (0x11B50)

#define PUMA5_DOCSIS_ANALOG_CR_0_REG( device )       GET_SOC_REG_ADDR( device, PUMA5_DOCSIS_ANALOG_CR_0_OFFSET )
#define PUMA5_DOCSIS_ANALOG_CR_1_REG( device )       GET_SOC_REG_ADDR( device, PUMA5_DOCSIS_ANALOG_CR_1_OFFSET )
#define PUMA5_DOCSIS_ANALOG_CR_2_REG( device )       GET_SOC_REG_ADDR( device, PUMA5_DOCSIS_ANALOG_CR_2_OFFSET )
#define PUMA5_DOCSIS_ANALOG_CR_3_REG( device )       GET_SOC_REG_ADDR( device, PUMA5_DOCSIS_ANALOG_CR_3_OFFSET )


#define PUMA5_ANALOG_CR0_REG1_PLL_VAL    0x5
#define PUMA5_ANALOG_CR0_REG1_PLL_MASK   0x7
#define PUMA5_ANALOG_CR0_REG1_PLL_SHIFT  0x1

/* For Narrow Band tuner archtecture only */
#define PUMA5_ANALOG_CR0_REG3_PLL_VAL    0x2
#define PUMA5_ANALOG_CR0_REG3_PLL_MASK   0x3
#define PUMA5_ANALOG_CR0_REG3_PLL_SHIFT  0x10

#define PUMA5_ANALOG_CR0_REG4_PLL_PWDN_150_VAL     0x1
#define PUMA5_ANALOG_CR0_REG4_PLL_PWDN_75_VAL      0x1
#define PUMA5_ANALOG_CR0_REG4_PLL_PWDN_150_MASK    0x1
#define PUMA5_ANALOG_CR0_REG4_PLL_PWDN_75_MASK     0x1
#define PUMA5_ANALOG_CR0_REG4_PLL_PWDN_150_SHIFT   30
#define PUMA5_ANALOG_CR0_REG4_PLL_PWDN_75_SHIFT    29

#define PUMA5_ANALOG_CR1_WB_DATA_EN_VAL   0x1
#define PUMA5_ANALOG_CR1_WB_DATA_EN_SHIFT 0x1F
#define PUMA5_ANALOG_CR1_WB_DATA_EN_MASK  0x1


#define PUMA5_ANALOG_CR3_REG_PLL_VAL     0x1
#define PUMA5_ANALOG_CR3_REG_PLL_MASK    0x1
#define PUMA5_ANALOG_CR3_REG_PLL_SHIFT   0x0

#define PUMA5_ANALOG_CONFIG_DELAY_MSEC   1

#define PUMA5_DOCSIS_MAC_CR_OFFSET               (0x11B68)
#define PUMA5_DOCSIS_MAC_CR_REG( device )        GET_SOC_REG_ADDR( device, PUMA5_DOCSIS_MAC_CR_OFFSET )

#define PUMA5_DOCSIS_MAC_CR_VALUE_MF_0   0x501
#define PUMA5_DOCSIS_MAC_CR_VALUE_MF_1   0x701


#define PUMA5_DOCSIS_PSC_VAL             0x107 /* Including ARM9 clock */
#define PUMA5_ETH_RESET_AUX_GPIO            (0+AVALANCHE_MAX_PRIMARY_GPIOS) /* auxiliary GPIOs start after primary GPIOs */
#define PUMA5_BUFFER_CONTROL_GPIO           (13)
#define PUMA5_RF_SPLITTER_AUX_GPIO_REV1     (27+AVALANCHE_MAX_PRIMARY_GPIOS)
#define PUMA5_RF_SPLITTER_AUX_GPIO_REV2     (29+AVALANCHE_MAX_PRIMARY_GPIOS)
#define PUMA5_MXL261_RESET_PIN              (31+AVALANCHE_MAX_PRIMARY_GPIOS) /* ARRIS */

/* AUX GPIOs #27 #28 and #29 are used for AGC PWM for Tuners #2 #3 and #4 */
#define PUMA5_GPIO_MASK_BIT_27_28_29     (0xC7FFFFFF) 
/* GPIOs #29 and #30 are used for OOB */
#define PUMA5_GPIO_MASK_BIT_29_30        (0x9FFFFFFF) 

#define PUMA5_PINMUX0_OOB_EN              (0x1)
#endif

#if defined (CONFIG_MACH_PUMA6)
#define NBADC_RESET_RELEASE                 BIT(1)
#define NBADC_RESET_ACTIVATE                (0)

#define HAL_NBADC_IDA_REG01       0x0000
#define HAL_NBADC_IDA_REG03       0x0002
#define HAL_NBADC_OAD_RDREG02     0x001f
#define HAL_NBADC_IDA_REG28       0x001b 


#define OAD_RDREG02_SDADC_STAGE1_DONE       BIT(7)
#define OAD_RDREG02_SDADC_STAGE2_DONE       BIT(6)
#define OAD_RDREG02_SDADC_CAL_DONE          BIT(5)


#define PUMA6_TUNER_RESET_PIN               (97)
/* Boot config register values*/
#define HSIF_RX_FULL_RATE_BIT        (0x01)
#define HSIF_TX_FULL_RATE_BIT        (0x02)
#define HSIF_RX_TX_FULL_RATE_VALUE   (0x00)
#define HSIF_RX_TX_HALF_RATE_VALUE   (0x03)

#define ENABLE_SIGMA_DELTA_ADC_AND_BIASGEN_BLOCK        (BIT(6)|BIT(7))
#define DISABLE_SIGMA_DELTA_ADC_AND_BIASGEN_BLOCK       (0)
#define ENABLE_INPUT_BUFFER_AND_VCTOP                   (BIT(6)|BIT(7))
#define HAL_NBADC_IDA_REG28_DOUBLE_SAMPLING             (0x38)        
#define DISABLE_INPUT_BUFFER_AND_VCTOP                  (0)

#define HSIF_ENABLE_TX_FULL_RATE                  (0x01)
#define HSIF_ENABLE_RX_FULL_RATE                  (0x02)

#endif



static long socDocsisGlobalDriverIoctl(struct file *file, 
                                       unsigned int ioctl_num,unsigned long ioctl_param, struct semaphore *soc_sem);
static void socDocsisGlobalDriverInit(struct semaphore *);
static void socDocsisOOBConfig(Bool oobEn, socDevice_e device );

#if defined (CONFIG_MACH_PUMA5)
static void socDocsisRFSplitterConfig(unsigned long splitterEnParams);
static void socDocsisMxL261Reset(unsigned long resetPinVal /* 0 or 1*/);
static void socDocsisBufferControlConfig(void);
static void socDocsis261PullUpControl(unsigned long resetPinVal);
static void socDocsisMpegInEnable(void);
static void socDocsisConfigNBADCsupport(unsigned long param, socDevice_e device);
#endif

#if defined (CONFIG_MACH_PUMA6)
static void socDocsisNBADC_Init_puma6(unsigned long PowerType);
static void socDocsisPuma6TunerReset(unsigned long resetPinVal /* 0 or 1*/);
static void socDocsisHSIF_InitRate(unsigned long InitType);
#endif

static SoCDriverOperations_t docsis_global_operations =  
{
   .socModuleID   = SOC_DOCSIS_GLOBAL_MODULE_ID,
   .soc_ioctl     = &socDocsisGlobalDriverIoctl, 
   .soc_release   = NULL,
   .soc_open      = NULL,
   .soc_cleanup   = NULL,
   .soc_init      = &socDocsisGlobalDriverInit
};

/**************************************************************************/
/*! \fn void socDocsisGlobalRegisterSoCModule (SoCDriverOperations_t **docsis_global_operations_out)
 **************************************************************************
 *  \brief Register the SoC DOCSIS Global driver function pointers object.
 *  \param[in] **docsis_global_operations_out - pointer to SoC DOCSIS Global.
 *  \return none.
 **************************************************************************/
void socDocsisGlobalRegisterSoCModule (SoCDriverOperations_t **docsis_global_operations_out)
{
   *docsis_global_operations_out  =  &docsis_global_operations;
}

/**************************************************************************/
/*! \fn static long socDocsisGlobalDriverIoctl(struct file *file,
                                              unsigned int ioctl_num, unsigned long ioctl_param)
 **************************************************************************
 *  \brief This function is called whenever a process tries to do an ioctl to our
 * SoC driver file. We get two extra parameters (additional to the inode and file
 * structures, which all device functions get): the number of the ioctl called
 * and the parameter given to the ioctl function.
 *
 * If the ioctl is write or read/write (meaning output is returned to the
 * calling process), the ioctl call returns the output of this function.
 * _IOC macros are used to decode ioctl numbers:
 *    _IOC_DIR(nr)  - get direction number 
 *    _IOC_TYPE(nr) - get type number 
 *    _IOC_NR(nr)   - get ordinal (sequential) number 
 *    _IOC_SIZE(nr) - get size number 
 *  \return 0 on success otherwise ioctl error code.
 **************************************************************************/
static long  socDocsisGlobalDriverIoctl(struct file *file,
                                        unsigned int ioctl_num,
                                        unsigned long ioctl_param,
                                        struct semaphore *soc_sem)
{
   /*
    * extract the type and number bitfields, and don't decode
    * wrong cmds: return ENOTTY (inappropriate ioctl) before access_ok(  )
    */
   if (_IOC_TYPE(ioctl_num) != SOC_DOCSIS_GLOBAL_MODULE_ID) 
      return -ENOTTY;
   if (_IOC_NR(ioctl_num) > SOC_DOCSIS_GLOBAL_IOCTL_MAXNR) 
      return -ENOTTY;

   /* */
   down(soc_sem);
   /* 
	 * Switch according to the ioctl called 
	 */
	switch (ioctl_num) {
    case SOC_DOCSIS_GLOBAL_CONFIG:
        socDocsisGlobalConfig(ioctl_param, SOC_DEVICE_LOCAL);
        break;
    case SOC_DOCSIS_OOB_CONFIG:
        socDocsisOOBConfig(True, SOC_DEVICE_LOCAL);
        break;
    case SOC_DOCSIS_DISABLE_OOB:
        socDocsisOOBConfig(False, SOC_DEVICE_LOCAL);
        break;
#if defined (CONFIG_MACH_PUMA5)
    case SOC_DOCSIS_MPEG_IN_ENABLE:
        socDocsisMpegInEnable();
        break;
    case SOC_DOCSIS_SPLITTER_ENABLE:
        socDocsisRFSplitterConfig(ioctl_param);
        break;
    case SOC_MXL261_RESET_CONTROL:
		socDocsisMxL261Reset(ioctl_param);
		break;
    case SOC_DOCSIS_BUFFER_CONTROL:
        socDocsisBufferControlConfig();
        break;
    case SOC_MXL261_PULLUP_MPEG_IF_CONTROL:
        socDocsis261PullUpControl(ioctl_param);
        break;
#endif
    case SOC_DOCSIS_NB_ADC_CONFIG:
#if defined (CONFIG_MACH_PUMA6)
       socDocsisNBADC_Init_puma6(ioctl_param);
#else
       socDocsisConfigNBADCsupport(ioctl_param, SOC_DEVICE_LOCAL);
#endif
       break;
#if defined (CONFIG_MACH_PUMA6)
	case SOC_PUMA6_TUNER_RESET_CONTROL:
		socDocsisPuma6TunerReset(ioctl_param);
		break;
    case SOC_DOCSIS_HSIF_CONFIG:
        socDocsisHSIF_InitRate(ioctl_param);
        break;
#endif
   default:  /* redundant, as cmd was checked against MAXNR */
      up(soc_sem);
      return -ENOTTY;
	}

   up(soc_sem);
	return 0;
}


/**************************************************************************/
/*! \fn void socDocsisGlobalInit(Uint32 device)
 **************************************************************************
 *  \brief Perform the required initialization of the SoC DOCSIS Global level.
 *  \      This function is envoked at the installation of the driver.
 *  \param[in]  device - Local for local initialization or Remote for Baby-Puma initialization 
 *  \return none.
 **************************************************************************/
void socDocsisGlobalInit(Uint32 device)
{
#if defined (CONFIG_MACH_PUMA6)

    printk(KERN_INFO "enable CRU_NUM_DOCSIS_MAC0 \n");
    PAL_sysResetCtrl(CRU_NUM_DOCSIS_MAC0, OUT_OF_RESET);

    printk(KERN_INFO "enable CRU_NUM_DOCSIS_MAC1 \n");
    PAL_sysResetCtrl(CRU_NUM_DOCSIS_MAC1, OUT_OF_RESET);

    printk(KERN_INFO "enable CRU_NUM_DOCSIS_PHY0 \n");
    PAL_sysResetCtrl(CRU_NUM_DOCSIS_PHY0, OUT_OF_RESET);

    printk(KERN_INFO "enable CRU_NUM_DOCSIS_PHY1 \n");
    PAL_sysResetCtrl(CRU_NUM_DOCSIS_PHY1, OUT_OF_RESET);

    printk(KERN_INFO "enable CRU_NUM_DOCSIS_PHY2 \n");
    PAL_sysResetCtrl(CRU_NUM_DOCSIS_PHY2, OUT_OF_RESET);

#else


   printk(KERN_INFO "Kick the BootCfg lock \n");
   socWriteRegister((PUMA5_BOOTCFG_KICK_0_REG( device )), PUMA5_BOOTCFG_KICK_0_VAL, True);
   socWriteRegister((PUMA5_BOOTCFG_KICK_1_REG( device )), PUMA5_BOOTCFG_KICK_1_VAL, True);
   
   /* Add delay */
   PAL_osWaitMsecs(PUMA5_ANALOG_CONFIG_DELAY_MSEC);
   
   printk(KERN_INFO "Configure DOCSIS PSC \n");
   /* Configure DOCSIS PSC */
   socWriteRegister((PUMA5_DOCSIS_PSC_REG( device )), PUMA5_DOCSIS_PSC_VAL, True);
   
   printk(KERN_INFO "Configure PSC Go \n");
   /* Configure DOCSIS PSC */
   socWriteRegister((PUMA5_PSC_GO_REG( device )), PUMA5_PSC_GO_VAL, True);

#endif
   return;
}

/**************************************************************************/
/*! \fn static void socDocsisGlobalDriverInit(struct semaphore *soc_sem)
 **************************************************************************
 *  \brief Perform the required initialization of the SoC DOCSIS Global level.
 *  \      This function is envoked at the installation of the driver.
 *  \param[in]  *soc_sem - pointer to the driver's semaphore.
 *  \return none.
 **************************************************************************/
static void socDocsisGlobalDriverInit(struct semaphore *soc_sem)
{
    down(soc_sem);

    socDocsisGlobalInit(SOC_DEVICE_LOCAL);

    up(soc_sem);
    return;
}

/**************************************************************************/
/*! \fn static void socDocsisGlobalConfig(unsigned long param, socDevice_e device)
 **************************************************************************
 *  \brief Perform the required configuration of the SoC DOCSIS Global level.
 *  \      This function is envoked by a specific IOCTL.
 *  \param[in]  param - bitmap that provides the operating-mode flags.
 *  \return none.
 **************************************************************************/
void socDocsisGlobalConfig(unsigned long param, socDevice_e device)
{

#if defined (CONFIG_MACH_PUMA6)
    if ( (param & SOC_DOCSIS_GLOBAL_PUMA6_US_DAC_MASK) && (device == SOC_DEVICE_LOCAL) )
    {
        printk(KERN_INFO "enable CRU_NUM_DAC \n");
        PAL_sysResetCtrl(CRU_NUM_DAC, OUT_OF_RESET); 
    }
#endif    

#if defined (CONFIG_MACH_PUMA5)

    Uint32  regContent;
    Bool supportNB = False;
    int multFactMode = 0;
    Uint32  regGpioAux;

    printk(KERN_INFO " socDocsisGlobalConfig - param = %ld\n", param);
    if(((param & SOC_DOCSIS_GLOBAL_NB_MASTER_TUNER_MASK) && (device == SOC_DEVICE_LOCAL)) ||
	   ((param & SOC_DOCSIS_GLOBAL_NB_BABY_TUNER_MASK) && (device == SOC_DEVICE_REMOTE)))
        supportNB = True;
    if(param & SOC_DOCSIS_GLOBAL_MF1_MASK)
        multFactMode = 1;

    printk(KERN_INFO " socDocsisGlobalConfig - supportNB = %d device %d \n", supportNB,device);
    printk(KERN_INFO " socDocsisGlobalConfig - multFactMode = %d \n", multFactMode);

    printk(KERN_INFO "Kick the BootCfg lock \n");
    socWriteRegister((PUMA5_BOOTCFG_KICK_0_REG( device )), PUMA5_BOOTCFG_KICK_0_VAL, True);
    socWriteRegister((PUMA5_BOOTCFG_KICK_1_REG( device )), PUMA5_BOOTCFG_KICK_1_VAL, True);

    printk(KERN_INFO "Configure Analog CR 0 \n");
    /* Configure Analog CR 0*/
    socReadRegister(PUMA5_DOCSIS_ANALOG_CR_0_REG( device ), &regContent, True);
    regContent &= ~(PUMA5_ANALOG_CR0_REG1_PLL_MASK << PUMA5_ANALOG_CR0_REG1_PLL_SHIFT);
    regContent |=  (PUMA5_ANALOG_CR0_REG1_PLL_VAL << PUMA5_ANALOG_CR0_REG1_PLL_SHIFT); 
    socWriteRegister((PUMA5_DOCSIS_ANALOG_CR_0_REG( device )), regContent, True);

    /* Disable/Enable the NB ADC */
    socDocsisConfigNBADCsupport(param,device);

    printk(KERN_INFO "Configure Analog CR 1 \n");
    /* Configure Analog CR 3 */
    socReadRegister(PUMA5_DOCSIS_ANALOG_CR_1_REG( device ), &regContent, True);
    regContent &= ~(PUMA5_ANALOG_CR1_WB_DATA_EN_MASK << PUMA5_ANALOG_CR1_WB_DATA_EN_SHIFT);
    regContent |=  (PUMA5_ANALOG_CR1_WB_DATA_EN_VAL << PUMA5_ANALOG_CR1_WB_DATA_EN_SHIFT); 
    socWriteRegister((PUMA5_DOCSIS_ANALOG_CR_1_REG( device )), regContent, True);

    printk(KERN_INFO "Configure Analog CR 3 \n");
    /* Configure Analog CR 3 */
    socReadRegister(PUMA5_DOCSIS_ANALOG_CR_3_REG( device ), &regContent, True); 
    regContent &= ~(PUMA5_ANALOG_CR3_REG_PLL_MASK << PUMA5_ANALOG_CR3_REG_PLL_SHIFT);
    regContent |=  (PUMA5_ANALOG_CR3_REG_PLL_VAL << PUMA5_ANALOG_CR3_REG_PLL_SHIFT); 
    socWriteRegister((PUMA5_DOCSIS_ANALOG_CR_3_REG( device )), regContent, True);
    
    /* Add delay */
    PAL_osWaitMsecs(PUMA5_ANALOG_CONFIG_DELAY_MSEC);

    printk(KERN_INFO "Configure DOCSIS MAC CR \n");
    /* Configure DOCSIS MAC CR */
    if(multFactMode == 1)
    {   /* MULT-FACT = 1*/
        socWriteRegister((PUMA5_DOCSIS_MAC_CR_REG( device )), PUMA5_DOCSIS_MAC_CR_VALUE_MF_1, True);
    }
    else
    {   /* MULT-FACT = 0*/
        socWriteRegister((PUMA5_DOCSIS_MAC_CR_REG( device )), PUMA5_DOCSIS_MAC_CR_VALUE_MF_0, True);
    }    

    /* Add delay */
    PAL_osWaitMsecs(PUMA5_ANALOG_CONFIG_DELAY_MSEC);
	/* PUMA6 TBD*/
    if (supportNB == True)
    {
       /* Configure DOCSIS AUX GPIO for Narrow Band Tuner archtecture:             */
       /* Disable AUX GPIOs in order to allow the AGC PWM for Tuners #2 #3 #4 to   */ 
       /* be muxed out of the AGCS pins the relevant AUX GPIOs are #27 / #28 / #29 */
// ARRIS remove TI-specific GPIO configuration
//       socReadRegister((PUMA5_DOCSIS_AUX_GPIO_REG( device )),&regGpioAux,True);
//       regGpioAux = ( regGpioAux & PUMA5_GPIO_MASK_BIT_27_28_29 );
//       socWriteRegister((PUMA5_DOCSIS_AUX_GPIO_REG( device )), regGpioAux, True);

       /* TNETC950 (NB) ETH_RST pin is connected to AUX_GPIO #0 (ASR_CS1_N).        */
       /* In TNETC550 (WB) this pin is connected to GPIO #14.                       */
       /* The current PSP drop we are using configure GPIO #14 only                 */
       /* via  puma5_ext_phy_reset(). We need to enable AUX_GPIO #0 and configure   */ 
       /* it to output 1 manually.                                                  */
// ARRIS remove TI-specific GPIO configuration
//        PAL_sysGpioCtrl(PUMA5_ETH_RESET_AUX_GPIO, GPIO_PIN, GPIO_OUTPUT_PIN);    
//       PAL_sysGpioOutBit(PUMA5_ETH_RESET_AUX_GPIO, 1 );
    }
#endif 
    return;
}
/**************************************************************************/
/*! \fn static void socDocsisOOBConfig(Bool oobEn)
 **************************************************************************
 *  \brief Perform the required configuration of the SoC DOCSIS for OOB mode.
 *  \      This function is envoked by a specific IOCTL.
 *  \param[in]  Bool oobEn - If true the turn OOB ON else OFF.
 *  \return none.
 **************************************************************************/
static void socDocsisOOBConfig(Bool oobEn, socDevice_e device)
{
  
#if defined (CONFIG_MACH_PUMA6)
    
    if (oobEn == True)
    {
       printk(KERN_INFO "socDocsisOOBConfig ::Enable OOB io\n");
       PAL_sysBootCfgCtrl_DocsisIo_OOB(BOOTCFG_IO_ENABLE);
    }
    else
    {
       printk(KERN_INFO "socDocsisOOBConfig ::Disable OOB io\n");
       PAL_sysBootCfgCtrl_DocsisIo_OOB(BOOTCFG_IO_DISABLE);

    }

#else

    Uint32  regValue;
    Uint32  regGpio;

    printk(KERN_INFO "socDocsisOOBConfig :: Kick the BootCfg lock \n");
    socWriteRegister((PUMA5_BOOTCFG_KICK_0_REG( device )), PUMA5_BOOTCFG_KICK_0_VAL, True);
    socWriteRegister((PUMA5_BOOTCFG_KICK_1_REG( device )), PUMA5_BOOTCFG_KICK_1_VAL, True);

    if (oobEn == True)
    {
       /* Configure DOCSIS GPIO for OOB: */
       socReadRegister((PUMA5_DOCSIS_GPIO_REG( device )),&regGpio,True);
       regGpio = ( regGpio & PUMA5_GPIO_MASK_BIT_29_30 );
       socWriteRegister((PUMA5_DOCSIS_GPIO_REG( device )), regGpio, True);

       /* Enable OOB mode */
       socReadRegister((PUMA5_PINMUX0_REG( device )),&regValue,True);
       regValue |= PUMA5_PINMUX0_OOB_EN;
       socWriteRegister((PUMA5_PINMUX0_REG( device )), regValue, True);
    }
    else
    {
       /* TBD -- Do we need to Enable the GPIOs ?? */

       /* Disable OOB mode */
       socReadRegister((PUMA5_PINMUX0_REG( device )),&regValue,True);
       regValue &= ~PUMA5_PINMUX0_OOB_EN;
       socWriteRegister((PUMA5_PINMUX0_REG( device )), regValue, True);
    }

#endif
}

#if defined (CONFIG_MACH_PUMA5)

/**************************************************************************/
/*! \fn static void socDocsisMxL261Reset(unsigned long resetPinVal)
 **************************************************************************
 *  \brief Perform RF-Splitter configuration.
 *  \      This function is invoked by a specific IOCTL.
 *  \param[in]  splitterEn - 1 - for RF splitter ON  and 0 - for OFF.
 *  \return none.
 **************************************************************************/
static void socDocsisMxL261Reset(unsigned long resetPinVal /* 0 or 1*/)
{
    int ret = 0;
    unsigned int gpio_pin1 = 31; // PUMA5_MXL261_RESET_PIN;  ARRIS TEMP change for proto1 822
    unsigned int gpio_pin = PUMA5_MXL261_RESET_PIN;

    printk(KERN_INFO "Configure MxL261 Reset PIN (AUX-GPIO %d) %d \n",(gpio_pin-AVALANCHE_MAX_PRIMARY_GPIOS), (int)resetPinVal);

    ret  = PAL_sysGpioCtrl(gpio_pin, GPIO_PIN, GPIO_OUTPUT_PIN);    
    ret |= PAL_sysGpioCtrl(gpio_pin1, GPIO_PIN, GPIO_OUTPUT_PIN);    // ARRIS TEMP

    if (resetPinVal == 1)
    {
        /* Power up MxL261 */
        ret |= PAL_sysGpioOutBit(gpio_pin, 1);
        ret |= PAL_sysGpioOutBit(gpio_pin1, 1);  // ARRIS TEMP
    }
    else if (resetPinVal == 0)
    {
        /* Power down MxL261 */
        ret |= PAL_sysGpioOutBit(gpio_pin, 0);
        ret |= PAL_sysGpioOutBit(gpio_pin1, 0);  // ARRIS TEMP
    }
    
    if (ret)
    {
       printk("Fail to set MxL261 Reset pin (AUX GPIO %d (%d)) ret = %d\n", (gpio_pin-AVALANCHE_MAX_PRIMARY_GPIOS), gpio_pin, ret);
    }
}

/**************************************************************************/
/*! \fn static void socDocsisRFSplitterConfig(unsigned long splitterEn)
 **************************************************************************
 *  \brief Perform RF-Splitter configuration.
 *  \      This function is invoked by a specific IOCTL.
 *  \param[in]  splitterEn - 1 - for RF splitter ON  and 0 - for OFF.
 *  \return none.
 **************************************************************************/
static void socDocsisRFSplitterConfig(unsigned long splitterEnParams)
{
    int ret = 0;
    int enVal;
    unsigned int gpio_pin = PUMA5_RF_SPLITTER_AUX_GPIO_REV1;

    if (splitterEnParams & SOC_DOCSIS_RF_SPLITTER_958REV2)
    {
        /* 958 rev02 */
        gpio_pin = PUMA5_RF_SPLITTER_AUX_GPIO_REV2;
    }
    enVal = splitterEnParams & SOC_DOCSIS_RF_SPLITTER_EN;
    
    printk(KERN_INFO "Configure RF-Splitter (AUX-GPIO %d) %d \n",(gpio_pin-AVALANCHE_MAX_PRIMARY_GPIOS), (int)splitterEnParams);

    /* For 958 and 958W boards */
// ARRIS remove TI-specific GPIO configuration
//    ret  = PAL_sysGpioCtrl(PUMA5_RF_SPLITTER_AUX_GPIO, GPIO_PIN, GPIO_OUTPUT_PIN);    

    if (enVal)
    {
// ARRIS remove TI-specific GPIO configuration
//        ret |= PAL_sysGpioOutBit(PUMA5_RF_SPLITTER_AUX_GPIO, 1 );
    }
    else
    {
// ARRIS remove TI-specific GPIO configuration
//        ret |= PAL_sysGpioOutBit(PUMA5_RF_SPLITTER_AUX_GPIO, 0 );
    }
}

/**************************************************************************/
/*! \fn static void socDocsisBufferControlConfig(void)
 **************************************************************************
 *  \brief Perform Buffer Control configuration For 958 and 958W REV2 boards.
 *  \      This function is invoked by a specific IOCTL.
 *  \return none.
 **************************************************************************/
static void socDocsisBufferControlConfig(void)
{
    int ret = 0;

    printk(KERN_INFO "Configure Buffer Control (GPIO 13)\n");

    /* Allocate GPIO#13 to buffer control */
    ret  = PAL_sysGpioCtrl(PUMA5_BUFFER_CONTROL_GPIO, GPIO_PIN, GPIO_OUTPUT_PIN);

    /* At initialization GPIO needs to be set to 0 */
    ret |= PAL_sysGpioOutBit(PUMA5_BUFFER_CONTROL_GPIO, 0 );

    if (ret)
    {
       printk("Fail to set Buffer Control (GPIO 13), ret = %d\n", ret);
    }
}
/**************************************************************************/
/*! \fn static void socDocsisConfigNBADCsupport(unsigned long param, socDevice_e device)
 **************************************************************************
 *  \brief config NB ADC (power and clock).
 *  \      This function is envoked by a specific IOCTL.
 *  \param[in]  param - bitmap that provides the operating-mode flags.
 *                     supportNBAdc - If True  - we will Power up NB ADC and config the clock.
 *                                  - If False - we will Shut down NB ADC.
 *                     multFactMode - The FactMode 1 or 0.
 *  \return none.
 **************************************************************************/
static void socDocsisConfigNBADCsupport(unsigned long param, socDevice_e device)
{
    Bool   supportNBAdc = False;
    Uint32 multFactMode = 0;
    Uint32 regContent   = 0;

    /* Get the input from the user */
    if(((param & SOC_DOCSIS_GLOBAL_NB_MASTER_TUNER_MASK) && (device == SOC_DEVICE_LOCAL)) ||
	   ((param & SOC_DOCSIS_GLOBAL_NB_BABY_TUNER_MASK) && (device == SOC_DEVICE_REMOTE)))
        supportNBAdc = True;
    if(param & SOC_DOCSIS_GLOBAL_MF1_MASK)
        multFactMode = 1;

    printk(KERN_INFO " socDocsisConfigNBsupport - supportNB = %d device %d \n", supportNBAdc,device);
    printk(KERN_INFO " socDocsisConfigNBsupport - multFactMode = %d \n", multFactMode);

    printk(KERN_INFO "Kick the BootCfg lock \n");
    socWriteRegister((PUMA5_BOOTCFG_KICK_0_REG( device )), PUMA5_BOOTCFG_KICK_0_VAL, True);
    socWriteRegister((PUMA5_BOOTCFG_KICK_1_REG( device )), PUMA5_BOOTCFG_KICK_1_VAL, True);

    /* Configure Analog CR 0*/
    socReadRegister(PUMA5_DOCSIS_ANALOG_CR_0_REG( device ), &regContent, True);
    if (supportNBAdc == True) 
    {
       /* Power up NB ADC clock */
       regContent &=  ~(PUMA5_ANALOG_CR0_REG4_PLL_PWDN_150_VAL << PUMA5_ANALOG_CR0_REG4_PLL_PWDN_150_SHIFT); 
       regContent &=  ~(PUMA5_ANALOG_CR0_REG4_PLL_PWDN_75_VAL << PUMA5_ANALOG_CR0_REG4_PLL_PWDN_75_SHIFT); 

       if (multFactMode == 1)
       {  /* TBD : need to review this code when the SW will support Mult Fact! */
          /* For Narrow Band Tuner archtecture with MultFact 1.                 */
          /* This will config the ADC to work with 60Mhz clock                  */
          regContent &= ~(PUMA5_ANALOG_CR0_REG3_PLL_MASK << PUMA5_ANALOG_CR0_REG3_PLL_SHIFT);
          regContent |=  (PUMA5_ANALOG_CR0_REG3_PLL_VAL << PUMA5_ANALOG_CR0_REG3_PLL_SHIFT); 
       }
       else
       {
          regContent &= ~(PUMA5_ANALOG_CR0_REG3_PLL_MASK << PUMA5_ANALOG_CR0_REG3_PLL_SHIFT);
       }
    }
    else
    {
        /* Shut down NB ADC clock */
        regContent |=  (PUMA5_ANALOG_CR0_REG4_PLL_PWDN_150_VAL << PUMA5_ANALOG_CR0_REG4_PLL_PWDN_150_SHIFT); 
        regContent |=  (PUMA5_ANALOG_CR0_REG4_PLL_PWDN_75_VAL << PUMA5_ANALOG_CR0_REG4_PLL_PWDN_75_SHIFT); 
    }
    socWriteRegister((PUMA5_DOCSIS_ANALOG_CR_0_REG( device )), regContent, True);
    return;
}

/**************************************************************************/
/*! \fn static void babyPumaLocalMpegInConfig(void)
 **************************************************************************
 *  \brief Perform the required initialization of the Puma's MPEG-In interface.
 *  \return none.
 **************************************************************************/
static void socDocsisMpegInEnable(void)
{
   Uint32 regValue;

   printk(KERN_INFO "socDocsisMpegInEnable:: Kick the BootCfg lock \n");
   socWriteRegister((PUMA5_BOOTCFG_KICK_0_REG( SOC_DEVICE_LOCAL )), PUMA5_BOOTCFG_KICK_0_VAL, True);
   socWriteRegister((PUMA5_BOOTCFG_KICK_1_REG( SOC_DEVICE_LOCAL )), PUMA5_BOOTCFG_KICK_1_VAL, True);

   /* Bit 1 "MPEG In EN". */
   printk(KERN_INFO "socDocsisMpegInEnable:: Enable MPEG In unit. \n");
   socReadRegister((PUMA5_PINMUX0_REG( SOC_DEVICE_LOCAL )),&regValue,True);
   regValue |= PUMA5_PINMUX0_MPEG_IN_EN;
   socWriteRegister((PUMA5_PINMUX0_REG( SOC_DEVICE_LOCAL )), regValue, True);

   /* The default value of MAC-PHY Interface Global Control Register (0903:A818) is: */
   /* MAC-1 connected to PHY-1 ... MAC-8 connected to PHY-8                          */
   /* So in a wonderful life we have nothing to do...                                */
}


/**************************************************************************/
/*! \fn static void socDocsis261PullUpControl(void)
 **************************************************************************
 *  \brief The MPEG IN PIN connected to PULL UP registor - 
 *   MUST to set it down before getting out of reset the 261, otherwise 261 will boot in test mode.
 *  This function set/Clear BIT0 of register 0861:1b60 
 *  In pullUpVal - 1 indicates to turn it off - 0 turn it on
 *  \return none.
 **************************************************************************/
static void socDocsis261PullUpControl(unsigned long pullUpVal /* 0 or 1*/)
{
   Uint32 regValue;

   socReadRegister((PUMA5_BOOTCFG_PUDCR0_REG(SOC_DEVICE_LOCAL)),&regValue,True);

   if( pullUpVal == SOC_MXL261_PULL_UP_OFF)
   {
       printk(KERN_INFO "socDocsis261PullUpControl:: Kick the Pulll Up/Down to inactive  \n");
       /* Bit 1 Pull up turn off - set it to 1*/
       regValue |= PUMA5_PUDCR0_REG_VALUE;
   }
   else
   {
       printk(KERN_INFO "socDocsis261PullUpControl:: Kick the Pulll Up/Down to active  \n");
       /* Bit 1 Pull up turn ON set to zero */
       regValue &= ~(PUMA5_PUDCR0_REG_VALUE);
   }

   socWriteRegister((PUMA5_BOOTCFG_PUDCR0_REG( SOC_DEVICE_LOCAL )),regValue , True);
}

#endif

#if defined (CONFIG_MACH_PUMA6)

/**************************************************************************/
/*! \fn static void socDocsisPuma6TunerReset(unsigned long resetPinVal)
 **************************************************************************
 *  \brief Perform RF-Splitter configuration.
 *  \      This function is invoked by a specific IOCTL.
 *  \param[in]  splitterEn - 1 - for RF splitter ON  and 0 - for OFF.
 *  \return none.
 **************************************************************************/
static void socDocsisPuma6TunerReset(unsigned long resetPinVal /* 0 or 1*/)
{
    int ret = 0;
    unsigned int gpio_pin = PUMA6_TUNER_RESET_PIN;

    printk(KERN_INFO "Configure PUMA6 Tuner Reset PIN (GPIO %d) %d \n",(gpio_pin), (int)resetPinVal);

	ret |= PAL_sysGpioOutBit(gpio_pin, resetPinVal);
    
    if (ret)
    {
       printk("Fail to set PUMA6 Tuner Reset pin (GPIO %d (%d)) ret = %d\n", (gpio_pin), gpio_pin, ret);
    }
}

/**************************************************************************/
/*! \fn  static void socDocsisHSIF_InitRate(unsigned long InitType)
 **************************************************************************
 *  \brief
 *  \param[in] - unsigned long InitType = indicates the init scenarios 
 *  \param[out]
 *  \return 
 **************************************************************************/
static void socDocsisHSIF_InitRate(unsigned long InitType)
{
    Uint32 regValue = 0x03;
    Uint32 rateMask = (InitType & (HSIF_ENABLE_TX_FULL_RATE |HSIF_ENABLE_RX_FULL_RATE)); 

    switch (rateMask)
    {
        case 0:                                          
            regValue = HSIF_RX_TX_HALF_RATE_VALUE;            
            break;                                       
        case 1:                                         
            regValue &= (~(HSIF_TX_FULL_RATE_BIT));            
            break;                                       
        case 2:                                          
            regValue &= (~(HSIF_RX_FULL_RATE_BIT));            
            break;                                       
        case 3:                                          
            regValue = HSIF_RX_TX_FULL_RATE_VALUE;            
            break;                                       
    }
    PAL_sysBootCfgCtrl_WriteReg(PHY_CONTROL_REGISTER, regValue);
}

/**************************************************************************/
/*! \fn static void socDocsisNBADC_Init_puma6(unsigned long PowerType)
 **************************************************************************
 *  \brief
 *  \param[in] - unsigned long PowerType = NBADC_POWER_UP or NBADC_POWER_DOWN 
 *  \param[out]
 *  \return 
 **************************************************************************/
static void socDocsisNBADC_Init_puma6(unsigned long PowerType)
{
    Uint32 WriteVal;
    Uint32 regValue;

    if (PowerType == SOC_DOCSIS_NBADC_POWER_UP) 
    {
         /* enable NBADC CRU*/
        PAL_sysResetCtrl(AVALANCHE_NBADC_RESET, OUT_OF_RESET); 

        WriteVal = ENABLE_SIGMA_DELTA_ADC_AND_BIASGEN_BLOCK;
        
        if ( iosfsb_reg_read_modify_write( IOSFSB_DOCSIS_NB_ADC_PORT,HAL_NBADC_IDA_REG01, BIT(6)|BIT(7), WriteVal))
        { 
           printk(KERN_ERR "Iosfsb modify IDA_REG1 in socDocsisNBADC_Init_puma6 failed\n" );                                                                                                                    
        }                                                                                                                                                                                                                                                                                                                                                                                               
                                                                                                                                                                                                     
        WriteVal = ENABLE_INPUT_BUFFER_AND_VCTOP;
        if ( iosfsb_reg_read_modify_write ( IOSFSB_DOCSIS_NB_ADC_PORT,HAL_NBADC_IDA_REG03, BIT(6)|BIT(7), WriteVal))
        { 
           printk(KERN_ERR "Iosfsb modify IDA_REG03 in socDocsisNBADC_Init_puma6 failed\n" );                                                                                                                   
        }                                                                                                                                                                                            
       
        WriteVal = HAL_NBADC_IDA_REG28_DOUBLE_SAMPLING;
        if ( iosfsb_write ( IOSFSB_DOCSIS_NB_ADC_PORT,HAL_NBADC_IDA_REG28, WriteVal,IOSFSB_REG_WRITE ))
        { 
           printk(KERN_ERR "Iosfsb write IDA_REG28 in socDocsisNBADC_Init_puma6 failed\n" );                                                                                                                   
        } 

        /* take NBADC out of reset*/            
        WriteVal = NBADC_RESET_RELEASE;
        if ( iosfsb_reg_read_modify_write ( IOSFSB_CPUNIT_PORT,IOSFSB_CPUNIT_MSIP_CTRL_REGISTER, BIT(1), WriteVal))
        { 
           printk(KERN_ERR "Iosfsb modify MSIP_CTRL_REGISTER in socDocsisNBADC_Init_puma6 failed\n" );                                                                                                                   
        } 
                                                                                                                                                                                                      
        /* check if the calibration was successful by checking bits oad_reg02<5,6,7> are equal to "1" (wait at least 10 us). */        
        
         printk(KERN_INFO "check if the calibration was successful - destport=%x, offset=%x\n",IOSFSB_DOCSIS_NB_ADC_PORT,HAL_NBADC_OAD_RDREG02);                                           
         if ( iosfsb_read (IOSFSB_DOCSIS_NB_ADC_PORT,HAL_NBADC_OAD_RDREG02,&regValue,IOSFSB_REG_READ))                                                                        
         {                                                                                                                                                                             
             printk(KERN_ERR "Iosfsb read IOSFSB_DOCSIS_NB_ADC_PORT in socDocsisNBADC_Init_puma6 failed in poll status\n" );                                                                                                               
         }                                                                                                                                                                                                                                                                                                                                                                   
         regValue &= (OAD_RDREG02_SDADC_STAGE1_DONE | OAD_RDREG02_SDADC_STAGE2_DONE | OAD_RDREG02_SDADC_CAL_DONE);                                                                                                                                                                                                                  
         if (regValue != (OAD_RDREG02_SDADC_STAGE1_DONE | OAD_RDREG02_SDADC_STAGE2_DONE | OAD_RDREG02_SDADC_CAL_DONE))
         {
             printk(KERN_ERR "socDocsisNBADC_Init_puma6 failed calibrating\n" );                                                                                                               
         }
    }
    else
    {

       WriteVal = DISABLE_SIGMA_DELTA_ADC_AND_BIASGEN_BLOCK;
       if ( iosfsb_reg_read_modify_write ( IOSFSB_DOCSIS_NB_ADC_PORT,HAL_NBADC_IDA_REG01, BIT(6)|BIT(7), WriteVal))
       { 
          printk(KERN_ERR "Iosfsb modify IDA_REG1 in socDocsisNBADC_Init_puma6 failed\n" );                                                                                                                    
       }                                                                                                                                                                                            


       WriteVal = DISABLE_INPUT_BUFFER_AND_VCTOP;
       if ( iosfsb_reg_read_modify_write ( IOSFSB_DOCSIS_NB_ADC_PORT,HAL_NBADC_IDA_REG03, BIT(6)|BIT(7), WriteVal))
       { 
          printk(KERN_ERR "Iosfsb modify IDA_REG03 in socDocsisNBADC_Init_puma6 failed\n" );                                                                                                                   
       }                                                                                                                                                                                            

        /* put NBADC in reset*/  
        WriteVal = NBADC_RESET_ACTIVATE;
        if ( iosfsb_reg_read_modify_write ( IOSFSB_CPUNIT_PORT,IOSFSB_CPUNIT_MSIP_CTRL_REGISTER, BIT(1), WriteVal))
        { 
           printk(KERN_ERR "Iosfsb modify MSIP_CTRL_REGISTER in socDocsisNBADC_Init_puma6 failed\n" );                                                                                                                   
        } 
        /* reset NBADC CRU*/
        PAL_sysResetCtrl(AVALANCHE_NBADC_RESET, IN_RESET); 
    }

}

#endif
EXPORT_SYMBOL( socDocsisGlobalConfig );
EXPORT_SYMBOL( socDocsisGlobalInit );
