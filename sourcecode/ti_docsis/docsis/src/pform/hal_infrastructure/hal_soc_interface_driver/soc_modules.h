/*
 *
 * soc_modules.h
 * Description:
 * declaration of functions and types used in the SoC Interface Driver.
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

#ifndef _SOC_MODULES_H_
#define _SOC_MODULES_H_

/**************************************************************************/
/*      INTERFACE  Defines and Structs                                    */
/**************************************************************************/
#define SOC_INTERFACE_DRIVER_DEV_NAME "/dev/soc_if_driver"

typedef enum SoC_ModuleIds
{
    /* Please note that the order of the modules in this enum is also the order of calling */
    /* in the main driver core "soc_interface_driver.c"                                    */
    SOC_MODULE_START              = 0,
    SOC_TOP_MODULE_ID   = SOC_MODULE_START,
    SOC_DOCSIS_GLOBAL_MODULE_ID,
    SOC_MPEG_OUT_MODULE_ID,
    SOC_MPEG_ENCAP_MODULE_ID,
    /* --- Add new SoC Modules here --- */
    SOC_MODULE_EOF  /* Total number of modules (used for soc_driver_operations list size) */

} SoC_ModuleIds_e;

#ifdef __KERNEL__

/* declare a typedef for a SoC driver's services function pointer */
typedef long (*SoC_Ioctl_t)    (struct file *, unsigned int, unsigned long, struct semaphore *);
typedef int  (*SoC_Release_t)  (struct inode *, struct file *, struct semaphore *);
typedef int  (*SoC_Open_t)     (struct inode *, struct file *, struct semaphore *);
typedef void (*SoC_Cleanup_t)  (struct semaphore *);
typedef void (*SoC_init_t)     (struct semaphore *);

/* SoC driver operations - function pointers struct */
typedef struct SoCDriverOperations
{
   SoC_ModuleIds_e   socModuleID;
   SoC_Ioctl_t       soc_ioctl;
   SoC_Release_t     soc_release;
   SoC_Open_t        soc_open;
   SoC_Cleanup_t     soc_cleanup;
   SoC_init_t        soc_init;

}  SoCDriverOperations_t;

#endif

#endif
