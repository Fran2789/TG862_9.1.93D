/*
 *
 * soc_interface_driver.h
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

#ifndef _SOC_INTERFACE_DRIVER_H_
#define _SOC_INTERFACE_DRIVER_H_

#include <asm/ioctl.h>
#include "_tistdtypes.h"
#include "sys_ptypes.h"
#include "soc_modules.h"


/**************************************************************************/
/*      INTERFACE  Defines and Structs                                    */
/**************************************************************************/
#define SOC_INTERFACE_DRIVER_NAME     "soc_if_driver"

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
typedef struct socTopParams_t
{
    Uint32      regAddr;
    Uint32      regValue;
}
socTopParams_t;


#define SOC_TOP_REGISTER_GET          _IOR (SOC_TOP_MODULE_ID, 1, socTopParams_t)
#define SOC_TOP_REGISTER_SET          _IOW (SOC_TOP_MODULE_ID, 2, socTopParams_t)

#define SOC_TOP_IOCTL_MAXNR         2

/*****************************************/
/* Definitions                           */
/*****************************************/
#if defined (CONFIG_MACH_PUMA6)

//#define BIT(i)                              ((1 << (i)))
/* DS address space                         */
#define HAL_PHY_BASE_OFFSET                 (0x01450000)
#define HAL_PHY_ADDRESS_SPACE_SIZE          (0x8000)
#define MAC_KERNEL_BASE_ADDRESS             (0xD1000000)

/* Phy register address shift value          */
#define HAL_PHY_REGISTER_SHIFT              (2)
/* Phy register address from docsis HAL code */
#define PHY_GENERAL_RESET_REGISTER          (0x1F03)
#define PHY_GENERAL_CLOCK_GATING_REGISTER    (0x1f0b)

#endif
/*****************************************/
/* Definitions for DOCSIS SoC-Level Init */
/*****************************************/

#define PUMA5_SOC_BASE_ADDRESS                  IO_PHY2VIRT(0x08600000)
#define GET_SOC_REG_ADDR( device, offset )      ((Uint32) (PUMA5_SOC_BASE_ADDRESS + offset))

/* SoC MPEG Out Registers */
#define PUMA5_MPEG_OUT_BASE_ADDR               IO_PHY2VIRT(0x06000000)
#define GET_MPEG_OUT_REG_ADDR( device, offset ) ((Uint32) (PUMA5_MPEG_OUT_BASE_ADDR + offset))

#define PUMA5_BOOTCFG_KICK_0                    IO_PHY2VIRT(0x08611A38)
#define PUMA5_BOOTCFG_KICK_1                    IO_PHY2VIRT(0x08611A3C)

#define PUMA5_BOOTCFG_KICK_0_VAL                (0x83E70B13)
#define PUMA5_BOOTCFG_KICK_1_VAL                (0x95A4F1E0)

#define PUMA5_PSC_GO                            IO_PHY2VIRT(0x08621120)
#define PUMA5_PSC_GO_VAL                        (0x00000001)

#define PUMA5_BOOTCFG_KICK_0_OFFSET             (0x11A38)
#define PUMA5_BOOTCFG_KICK_1_OFFSET             (0x11A3C)

#define PUMA5_PUDCR0_REG_OFFSER                 (0x11B60)
#define PUMA5_PUDCR0_REG_VALUE                  (0x01)

#define PUMA5_BOOTCFG_PUDCR0_REG( device )      GET_SOC_REG_ADDR( device, PUMA5_PUDCR0_REG_OFFSER )

#define PUMA5_BOOTCFG_KICK_0_REG( device )      GET_SOC_REG_ADDR( device, PUMA5_BOOTCFG_KICK_0_OFFSET )
#define PUMA5_BOOTCFG_KICK_1_REG( device )      GET_SOC_REG_ADDR( device, PUMA5_BOOTCFG_KICK_1_OFFSET )

#define PUMA5_DOCSIS_PSC_OFFSET                 (0x21A2C) /* 0x08621a2c */
#define PUMA5_DOCSIS_PSC_REG( device )          GET_SOC_REG_ADDR( device, PUMA5_DOCSIS_PSC_OFFSET )

#define PUMA5_PSC_GO_OFFSET                     (0x21120) /* 0x08621120 */
#define PUMA5_PSC_GO_REG( device )              GET_SOC_REG_ADDR( device, PUMA5_PSC_GO_OFFSET )

#define PUMA5_PINMUX0_OFFSET                    (0x11B10)
#define PUMA5_PINMUX0_REG( device )             GET_SOC_REG_ADDR( device, PUMA5_PINMUX0_OFFSET )

#define PUMA5_BOOTCFG_CPPI_SEL_OFFSET           (0x11B98)
#define PUMA5_BOOTCFG_CPPI_SEL_REG( device )    GET_SOC_REG_ADDR( device, PUMA5_BOOTCFG_CPPI_SEL_OFFSET )


/**********************/
/* MPEG-IN definitions*/
/**********************/
#define PUMA5_PINMUX0_MPEG_IN_EN          (0x2)



#define PUMA5_DOCSIS_AUX_GPIO_OUT_VAL_OFFSET    (0x1092C)
#define PUMA5_DOCSIS_AUX_GPIO_DIR_OFFSET        (0x10930)
#define PUMA5_DOCSIS_AUX_GPIO_OFFSET            (0x10934)

#define PUMA5_DOCSIS_GPIO_OUT_VAL_OFFSET        (0x10904)
#define PUMA5_DOCSIS_GPIO_DIR_OFFSET            (0x10908)
#define PUMA5_DOCSIS_GPIO_OFFSET                (0x1090c)


#define PUMA5_DOCSIS_AUX_GPIO_OUT_VAL_REG( device ) GET_SOC_REG_ADDR( device, PUMA5_DOCSIS_AUX_GPIO_OUT_VAL_OFFSET )
#define PUMA5_DOCSIS_AUX_GPIO_DIR_REG( device )     GET_SOC_REG_ADDR( device, PUMA5_DOCSIS_AUX_GPIO_DIR_OFFSET )
#define PUMA5_DOCSIS_AUX_GPIO_REG( device )         GET_SOC_REG_ADDR( device, PUMA5_DOCSIS_AUX_GPIO_OFFSET )

#define PUMA5_DOCSIS_GPIO_OUT_VAL_REG( device )     GET_SOC_REG_ADDR( device, PUMA5_DOCSIS_GPIO_OUT_VAL_OFFSET )
#define PUMA5_DOCSIS_GPIO_DIR_REG( device )         GET_SOC_REG_ADDR( device, PUMA5_DOCSIS_GPIO_DIR_OFFSET )
#define PUMA5_DOCSIS_GPIO_REG( device )             GET_SOC_REG_ADDR( device, PUMA5_DOCSIS_GPIO_OFFSET )


typedef enum
{
    SOC_DEVICE_LOCAL = 0,
    SOC_DEVICE_REMOTE = 1,

} socDevice_e;

/**************************************************************************/
/*! \fn void socWritePhysRegister (unsigned long  addr, unsigned long  value)
 **************************************************************************
 *  \brief Write SoC Register.
 *  \param[in] addr    - Register addr.
 *  \param[in] value   - Value to be written.
 *  \return none.
 **************************************************************************/
void socWritePhysRegister (Uint32  addr, Uint32  value);

/**************************************************************************/
/*! \fn void socReadPhysRegister(unsigned long  addr, volatile unsigned long *value)
 **************************************************************************
 *  \brief Read SoC Register.
 *  \param[in] addr    - Register addr.
 *  \param[in] *value  - Pointer for returned register value.
 *  \return none.
 **************************************************************************/
void socReadPhysRegister (Uint32  addr, Uint32 *value);

/**************************************************************************/
/*! \fn void socWriteRegister (unsigned long  addr, unsigned long  value, bool printEn)
 **************************************************************************
 *  \brief Write SoC Register.
 *  \param[in] addr    - Register addr.
 *  \param[in] value   - Value to be written.
 *  \param[in] printEn - If true a debug print is enable.
 *  \return none.
 **************************************************************************/
void socWriteRegister (Uint32  addr, Uint32  value, Bool printEn);

/**************************************************************************/
/*! \fn void socReadRegister(unsigned long  addr, volatile unsigned long *value, bool printEn)
 **************************************************************************
 *  \brief Read SoC Register.
 *  \param[in] addr    - Register addr.
 *  \param[in] *value  - Pointer for returned register value.
 *  \param[in] printEn - If true a debug print is enable.
 *  \return none.
 **************************************************************************/
void socReadRegister (Uint32  addr, Uint32 *value, Bool printEn);

/**************************************************************************/
/*! \fn void socBitField32Set(Uint32  *addr, Uint32  data, Uint32  offset, Uint32  width)
 **************************************************************************
 *  \brief Set specific bits in a 32 bit variable.
 *  \param[in] *addr   - pointer to 32 bit variable to change.
 *  \param[in] data    - Data to write.
 *  \param[in] offset  - Write from this offset.
 *  \param[in] width   - Write width bits.
 *  \return none.
 **************************************************************************/
void socBitField32Set(Uint32  *addr, Uint32  data, Uint32  offset,Uint32  width);


#if defined (CONFIG_MACH_PUMA6)
/**************************************************************************/
/*! \fn Uint32 BuildPhyAddr(Uint32 regOffset)
 **************************************************************************
 *  \brief Convert register offset into PHY memory address
 *  \param[in]: UINT32 regOffset - The register offset from phy base address
 *  \param[out]: UINT32 - the phy virtual address
 **************************************************************************/
Uint32 BuildPhyAddr(Uint32 regOffset);
#endif


#endif
