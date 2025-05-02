/*
 *
 * mpeg_out_driver.h
 * Description:
 * declaration of functions and types used to control the MPEG out driver.
 *
 * Copyright (C) 2008-2013 Texas Instruments Incorporated - http://www.ti.com/ 
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

#ifndef _HAL_MPEG_SOC_DRIVER_H_
#define _HAL_MPEG_SOC_DRIVER_H_

#include <asm/ioctl.h>
#include "sys_ptypes.h"
#include "soc_modules.h"
#include <puma_autoconf.h>


/**************************************************************************/
/*      INTERFACE  Defines and Structs                                    */
/**************************************************************************/
#define SOC_MPEG_OUT_DRIVER_DEV_NAME  SOC_INTERFACE_DRIVER_DEV_NAME
/*****************************************/
/* Definitions for MPEG-Out SoC Unit     */
/*****************************************/

/* MPEG Out Control Register */
#if defined (CONFIG_MACH_PUMA6)

typedef struct SoCMPEGOutInterfaceSelect                    
{
    Uint32 IntfcSel;             /* bit 0-11  - Selectors for the 6 muxes of the output.
                                                 Every 2 bits are selector for a mux selecting:
                                                 00 - all outputs are Zeros.
                                                 01 - CPPI 1.
                                                 10 - CPPI 2.
                                                 11 - Phy MPEG. */
}SoCMPEGOutInterfaceSelect_t;



typedef struct SoCMPEGOutGlobalConfig                    
{


   Uint32   cppiMsbFirst;     /* Bit 7 - Bit transmission order - which of the bits is sent first. 
                                      Affects ONLY the MPEG frame generator [default: 1]:
                                      0 – LSB
                                      1 – MSB */

   Uint32   lockRcvrDis;       /* Bit 6 - Lock Fall Recovery Disable. When active, extra features of the P2S modules are disabled:
                                - Complete the frame when lock fall in the middle of it.
                                - Start sending new frame only ay sinc and when lock is active. 
                                1 - Lock Fall Recovery disabled
                                0 - Lock Fall Recovery enabled */

   Uint32   endianSwap;       /* Bit 5 - Swap bytes in the bus going from the Async FIFO to the Frame Generator [default: 0] 
                                      0 – Bus directly connected
                                      1 – Bytes are swapped */

   Uint32   clockInv;         /* Bit 4 - Invert the clock output [default: 0]:
                                      0 – Signals change at rise edge
                                      1 – Signals change at fall edge */

   Uint32   syncPulseWidth;   /* Bit 4 - Sync pulse width. Affect the MPEG frame generator, P2S and S2P [default: 0]:
                                      0 – Bit
                                      1 – Byte */
   Uint32   msbFirstTxOrder;  /* Bit 3 - MSB first Bit transmission order - which of the bits is sent first. 
                                      Affects the MPEG frame generator, P2S and S2P [default: 1]:
                                      0 – LSB
                                      1 – MSB */
   Uint32   syncInv;          /* Bit 1 - Sync Invert Switch the SYNC output to active low mode.
                                      Used for both serial and parallel interfaces [default: 0]:
                                      0 – Active high
                                      1 – Active low */
   Uint32   validInv;         /* Bit 0 - Valid Invert Switch the valid output to active low mode.
                                      Used for both serial and parallel interfaces [default: 0]:
                                      0 – Active high
                                      1 – Active low */
} SoCMPEGOutGlobalConfig_t;

#else
typedef struct SoCMPEGOutGlobalConfig                    
{
   Uint32   cppiMsbFirst;     /* Bit 11 - Bit transmission order - which of the bits is sent first. 
                                      Affects ONLY the MPEG frame generator [default: 1]:
                                      0 – LSB
                                      1 – MSB */


   Uint32   endianSwap;       /* Bit 9 - Swap bytes in the bus going from the Async FIFO to the Frame Generator [default: 0] 
                                      0 – Bus directly connected
                                      1 – Bytes are swapped */
   Uint32   clockInv;         /* Bit 8 - Invert the clock output [default: 0]:
                                      0 – Signals change at rise edge
                                      1 – Signals change at fall edge */
   Uint32   frameCntEn;       /* Bit 7 - Frame counter enable [default: 0]
                                      0 – Disable
                                      1 – Enable */
   Uint32   syncPulseWidth;   /* Bit 6 - Sync pulse width. Affect the MPEG frame generator, P2S and S2P [default: 0]:
                                      0 – Bit
                                      1 – Byte */
   Uint32   msbFirstTxOrder;  /* Bit 5 - MSB first Bit transmission order - which of the bits is sent first. 
                                      Affects the MPEG frame generator, P2S and S2P [default: 1]:
                                      0 – LSB
                                      1 – MSB */
   Uint32   syncInv;          /* Bit 4 - Sync Invert Switch the SYNC output to active low mode.
                                      Used for both serial and parallel interfaces [default: 0]:
                                      0 – Active high
                                      1 – Active low */
   Uint32   validInv;         /* Bit 3 - Valid Invert Switch the valid output to active low mode.
                                      Used for both serial and parallel interfaces [default: 0]:
                                      0 – Active high
                                      1 – Active low */
} SoCMPEGOutGlobalConfig_t;
#endif
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

#define SOC_MPEGO_UNIT_ENABLE         _IO (SOC_MPEG_OUT_MODULE_ID, 1)   
#define SOC_MPEGO_UNIT_DISABLE        _IO (SOC_MPEG_OUT_MODULE_ID, 2)
#define SOC_MPEGO_PORT_ENABLE         _IOW(SOC_MPEG_OUT_MODULE_ID, 3, Uint32) /* input is port index 1-4 (by value) */
#define SOC_MPEGO_PORT_DISABLE        _IOW(SOC_MPEG_OUT_MODULE_ID, 4, Uint32) /* input is port index 1-4 (by value) */
#define SOC_MPEGO_UNIT_GLOBAL_CONFIG  _IOW(SOC_MPEG_OUT_MODULE_ID, 5, SoCMPEGOutGlobalConfig_t) /* input is global config parameters */
#define SOC_MPEGO_PORT_SOURCE         _IOW(SOC_MPEG_OUT_MODULE_ID, 6, Uint32) /* port-1 input source, 0-PHY1/1-CPPI */
#define SOC_MPEGO_CPPI_ENABLE         _IO (SOC_MPEG_OUT_MODULE_ID, 7)
#define SOC_MPEGO_IO_ENABLE           _IO (SOC_MPEG_OUT_MODULE_ID, 8)
/* SOC_MPEGO_IOCTL_MAXNR: In case of adding new MPEG IOCTL commands this number must be update */
#define SOC_MPEGO_IOCTL_MAXNR 8 

/**************************************************************************/
/*! \fn void mpegOutRegisterSoCModule (SoCDriverOperations_t **mpeg_out_operations)
 **************************************************************************
 *  \brief Register the MPEG-Out driver function pointers object.
 *  \param[in] **mpeg_out_operations - pointer to MPEG-Out .
 *  \return none.
 **************************************************************************/
#ifdef __KERNEL__
void mpegOutRegisterSoCModule (SoCDriverOperations_t **mpeg_out_operations_out);

Int32 mpegOutUnitGlobalConfig( SoCMPEGOutGlobalConfig_t *mpegOutConfiguration, socDevice_e device );
Int32 mpegOutPortSet( Uint32 port, Bool enable, socDevice_e device );
Int32 mpegOutUnitEnable( socDevice_e device );
Int32 mpegOutUnitDisable( socDevice_e device );
Int32 mpegOutPort1Source( socDevice_e device, Uint32 source );
#if defined (CONFIG_MACH_PUMA6)
Int32 enableMPEGoutIO();
#endif

#endif /* __KERNEL__ */

#endif  /* _HAL_MPEG_SOC_DRIVER_H_ */
