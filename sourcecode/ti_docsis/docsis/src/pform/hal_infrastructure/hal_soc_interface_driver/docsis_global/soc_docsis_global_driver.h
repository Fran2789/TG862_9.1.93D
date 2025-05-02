/*
 *
 * soc_docsis_global_driver.h
 * Description:
 * declaration of functions and types used to control the DOCSIS driver.
 *
 * Copyright (C) 2008 Texas Instruments Incorporated - http://www.ti.com/ 
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

#ifndef _HAL_SOC_DOCSIS_GLOBAL_DRIVER_H_
#define _HAL_SOC_DOCSIS_GLOBAL_DRIVER_H_

#include <asm/ioctl.h>
#include "sys_ptypes.h"
#include "soc_modules.h"

/**************************************************************************/
/*      INTERFACE  Defines and Structs                                    */
/**************************************************************************/
#define SOC_DOCSIS_GLOBAL_DRIVER_DEV_NAME  SOC_INTERFACE_DRIVER_DEV_NAME

/* IOCTL commands:

   If you are adding new ioctl's to the kernel, you should use the _IO
   macros defined in <linux/ioctl.h> _IO macros are used to create ioctl numbers:

	_IO(type, nr)         - an ioctl with no parameter. 
   _IOW(type, nr, size)  - an ioctl with write parameters (copy_from_user), kernel would actually read data from user space 
   _IOR(type, nr, size)  - an ioctl with read parameters (copy_to_user), kernel would actually write data to user space 
   _IOWR(type, nr, size) - an ioctl with both write and read parameters 

   'Write' and 'read' are from the user's point of view, just like the
	system calls 'write' and 'read'.  For example, a SET_FOO ioctl would
	be _IOW, although the kernel would actually read data from user space;
	a GET_FOO ioctl would be _IOR, although the kernel would actually write
	data to user space.

	The first argument to _IO, _IOW, _IOR, or _IOWR is an identifying letter
	or number from the SoC_ModuleIds_e enum located in this file. 

	The second argument to _IO, _IOW, _IOR, or _IOWR is a sequence number
	to distinguish ioctls from each other.  

   The third argument to _IOW, _IOR, or _IOWR is the type of the data going
   into the kernel or coming out of the kernel (e.g.  'int' or 'struct foo').

   NOTE!  Do NOT use sizeof(arg) as the third argument as this results in 
   your ioctl thinking it passes an argument of type size_t.

*/

#define	SOC_DOCSIS_GLOBAL_CONFIG      _IOW (SOC_DOCSIS_GLOBAL_MODULE_ID, 1, Uint32)
#define	SOC_DOCSIS_OOB_CONFIG         _IO  (SOC_DOCSIS_GLOBAL_MODULE_ID, 2)	
#define	SOC_DOCSIS_DISABLE_OOB        _IO  (SOC_DOCSIS_GLOBAL_MODULE_ID, 3)
#define	SOC_DOCSIS_MPEG_IN_ENABLE     _IO  (SOC_DOCSIS_GLOBAL_MODULE_ID, 4)
#define	SOC_DOCSIS_SPLITTER_ENABLE    _IOW (SOC_DOCSIS_GLOBAL_MODULE_ID, 5, Uint32)
#define	SOC_MXL261_RESET_CONTROL      _IOW (SOC_DOCSIS_GLOBAL_MODULE_ID, 6, Uint32)
#define	SOC_DOCSIS_BUFFER_CONTROL     _IO  (SOC_DOCSIS_GLOBAL_MODULE_ID, 7)
#define	SOC_MXL261_PULLUP_MPEG_IF_CONTROL    _IOW  (SOC_DOCSIS_GLOBAL_MODULE_ID, 8,Uint32)
#define	SOC_DOCSIS_NB_ADC_CONFIG     _IOW (SOC_DOCSIS_GLOBAL_MODULE_ID, 9, Uint32)
#define	SOC_DOCSIS_HSIF_CONFIG       _IOW (SOC_DOCSIS_GLOBAL_MODULE_ID, 10, Uint32)
#define	SOC_PUMA6_TUNER_RESET_CONTROL _IOW (SOC_DOCSIS_GLOBAL_MODULE_ID, 11, Uint32)

/* SOC_DOCSIS_GLOBAL_IOCTL_MAXNR: In case of adding new DOCSIS Global IOCTL commands this number must be update */
#define SOC_DOCSIS_GLOBAL_IOCTL_MAXNR 11

#define SOC_MXL261_POWER_ON                (1)
#define SOC_MXL261_POWER_OFF               (0)

#define SOC_MXL_HRCLS_POWER_ON             (1)
#define SOC_MXL_HRCLS_POWER_OFF            (0)

#define SOC_NXP18265_POWER_ON                (1)
#define SOC_NXP18265_POWER_OFF               (0)

#define SOC_MXL261_PULL_UP_ON                (0)
#define SOC_MXL261_PULL_UP_OFF               (1)

#define SOC_DOCSIS_RF_SPLITTER_EN          (1)
#define SOC_DOCSIS_RF_SPLITTER_958REV2     (1<<1)

#define SOC_DOCSIS_GLOBAL_NB_MASTER_TUNER_MASK   (1<<0) 
#define SOC_DOCSIS_GLOBAL_MF1_MASK               (1<<1)
#define SOC_DOCSIS_GLOBAL_NB_BABY_TUNER_MASK     (1<<2)
#define SOC_DOCSIS_GLOBAL_PUMA6_US_DAC_MASK      (1<<3)

/* NBADC declarations*/
#define MAX_NBADC_CALIBRATION_OK_WAIT       (1)
#define SOC_DOCSIS_NBADC_POWER_UP           (1)
#define SOC_DOCSIS_NBADC_POWER_DOWN         (2)

#define HSIF_ENABLE_TX_FULL_RATE                  (0x01)
#define HSIF_ENABLE_RX_FULL_RATE                  (0x02)

#ifdef __KERNEL__
/**************************************************************************/
/*! \fn static void socDocsisGlobalConfig(unsigned long param, socDevice_e device)
 **************************************************************************
 *  \brief Perform the required configuration of the SoC DOCSIS Global level.
 *  \      This function is envoked by a specific IOCTL.
 *  \param[in]  param - bitmap that provides the operating-mode flags.
 *  \return none.
 **************************************************************************/
//void socDocsisGlobalConfig(unsigned long param, socDevice_e device);

/**************************************************************************/
/*! \fn void socDocsisGlobalRegisterSoCModule (SoCDriverOperations_t **docsis_global_operations_out)
 **************************************************************************
 *  \brief Register the SoC DOCSIS Global driver function pointers object.
 *  \param[in] **docsis_global_operations_out - pointer to SoC DOCSIS Global.
 *  \return none.
 **************************************************************************/
void socDocsisGlobalRegisterSoCModule (SoCDriverOperations_t **docsis_global_operations_out);

/**************************************************************************/
/*! \fn void socDocsisGlobalConfig(unsigned long param, socDevice_e device)
 **************************************************************************
 *  \brief Perform the required configuration of the SoC DOCSIS Global level.
 *  \      This function is envoked by a specific IOCTL.
 *  \param[in]  param - bitmap that provides the operating-mode flags.
 *  \return none.
 **************************************************************************/
void socDocsisGlobalConfig(unsigned long param, socDevice_e device);

/**************************************************************************/
/*! \fn void socDocsisGlobalInit(Uint32 device)
 **************************************************************************
 *  \brief Perform the required initialization of the SoC DOCSIS Global level.
 *  \      This function is envoked at the installation of the driver.
 *  \param[in]  device - Local for local initialization or Remote for Baby-Puma initialization 
 *  \return none.
 **************************************************************************/
void socDocsisGlobalInit(Uint32 device);

#endif /* __KERNEL__ */ 

#endif  /* _HAL_SOC_DOCSIS_GLOBAL_DRIVER_H_ */
