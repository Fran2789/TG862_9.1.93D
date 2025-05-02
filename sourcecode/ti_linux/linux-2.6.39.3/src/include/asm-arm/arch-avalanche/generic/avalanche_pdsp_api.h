/* 
  This file is provided under a dual BSD/GPLv2 license.  When using or 
  redistributing this file, you may do so under either license.

  GPL LICENSE SUMMARY

  Copyright(c) 2014 Intel Corporation.

  This program is free software; you can redistribute it and/or modify 
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation.

  This program is distributed in the hope that it will be useful, but 
  WITHOUT ANY WARRANTY; without even the implied warranty of 
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU 
  General Public License for more details.

  You should have received a copy of the GNU General Public License 
  along with this program; if not, write to the Free Software 
  Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
  The full GNU General Public License is included in this distribution 
  in the file called LICENSE.GPL.


  Contact Information:
  Intel Corporation
  2200 Mission College Blvd.
  Santa Clara, CA  97052

  BSD LICENSE 

  Copyright(c) 2014 Intel Corporation. All rights reserved.

  Redistribution and use in source and binary forms, with or without 
  modification, are permitted provided that the following conditions 
  are met:

    * Redistributions of source code must retain the above copyright 
      notice, this list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above copyright 
      notice, this list of conditions and the following disclaimer in 
      the documentation and/or other materials provided with the 
      distribution.

    * Neither the name of Intel Corporation nor the names of its 
      contributors may be used to endorse or promote products derived 
      from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR 
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY 
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef _AVALANCHE_PDSP_H
#define _AVALANCHE_PDSP_H

#include <asm-arm/arch-avalanche/generic/_tistdtypes.h>
#include <linux/ioctl.h>

#ifdef __KERNEL__ 
    #if defined (CONFIG_MACH_PUMA6)
        #define AVALANCHE_PDSP_H_PUMA6
	#else
		#undef  AVALANCHE_PDSP_H_PUMA6
    #endif
#else
#include <puma_autoconf.h> 
    #if PUMA6_OR_NEWER_SOC_TYPE
        #define AVALANCHE_PDSP_H_PUMA6
    #else
	    #undef  AVALANCHE_PDSP_H_PUMA6
    #endif
#endif


/* PDSP commands */
#define PDSP_CMD_OPEN                      0x80
#define PDSP_CONFIG_PREFETCH               0x84
#define PDSP_ENABLE_PREFETCH               0x83
#define PDSP_PREFATCHER_CONFIG_TDQ         0x87
#define PDSP_SETPSM                        0x88
#define PDSP_SET_ACK_SUPP                  0x8C

typedef enum
{
    PDSP_ID_Prefetcher,   //  _PPDSP     
    PDSP_ID_Classifier,   //  _CPDSP1
#ifdef AVALANCHE_PDSP_H_PUMA6
    PDSP_ID_Classifier2,  //  _CPDSP2 only in P6 
#endif  
    PDSP_ID_Modifier,     //  _MPDSP     
    PDSP_ID_QoS,          //  _QPDSP  
#ifdef AVALANCHE_PDSP_H_PUMA6  
    PDSP_ID_LAN_Proxy,    //  _PrxPDSP only in P6 
    PDSP_ID_CoE,          //  _CoePDSP only in P6   
#endif
    PDSP_ID_US_Prefetch,  //  _UsPrefPDSP
    PDSP_ID_MAX
} 
pdsp_id_t;

typedef Uint32 pdsp_cmd_t;

typedef struct
{
    pdsp_id_t       pdsp_id;
    pdsp_cmd_t      cmd;
    Uint32          params_len;
    Uint32          params[64];
}
pdsp_cmd_params_t;

/********************************************************************************************************/
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
#define PDSP_DRIVER_MODULE_ID                   (0xDE)

#define PDSP_DRIVER_RESET_PDSP                  _IOW (PDSP_DRIVER_MODULE_ID, 1, pdsp_id_t) 
#define PDSP_DRIVER_START_PDSP                  _IOW (PDSP_DRIVER_MODULE_ID, 2, pdsp_id_t)
#define PDSP_DRIVER_DOWNLOAD_START              _IOW (PDSP_DRIVER_MODULE_ID, 3, pdsp_id_t)
#define PDSP_DRIVER_DOWNLOAD_FINISH             _IOW (PDSP_DRIVER_MODULE_ID, 4, pdsp_id_t)
#define PDSP_DRIVER_TEST_IRAM                   _IOW (PDSP_DRIVER_MODULE_ID, 5, pdsp_id_t)
#define PDSP_DRIVER_PUT_CMD                     _IOWR(PDSP_DRIVER_MODULE_ID, 6, pdsp_cmd_params_t)



#ifdef __KERNEL__

/* Success Code */
#define SR_RETCODE_SUCCESS          1

/* PDSP error codes */
#define SRPDSP_ENORES                   -1
#define SRPDSP_EINVCMD                  -2
#define SRPDSP_EINVOPT                  -3
#define SRPDSP_EINVINDEX                -4
#define SRPDSP_EALREADYOPEN             -5
#define SRPDSP_ENOTOPEN                 -6
#define SRPDSP_EMAPERROR                -7
#define SRPDSP_EINVPORT                 -8
#define SRPDSP_EINVPID                  -9
#define SRPDSP_EPAUSELIMITEXCEED        -10
#define SRPDSP_ESESSIONNOTPAUSED        -11
#define SRPDSP_ESESSIONPAUSED           -12
#define SRPDSP_EREOPENINVALID           -13
#define SRPDSP_EINTERROR                -99




Int32 pdsp_cmd_send    (pdsp_id_t               id,
                        pdsp_cmd_t              cmd_word, 
                        void *wr_ptr,   Uint32  wr_word, 
                        void *rd_ptr,   Uint32  rd_word);

typedef enum
{
    PDSPCTRL_HLT      ,
    PDSPCTRL_STEP     ,
    PDSPCTRL_FREERUN  ,
    PDSPCTRL_RESUME   ,
    PDSPCTRL_RST      ,
    PDSPCTRL_START    ,
}
pdsp_ctrl_op_t;
/*
 * pdsp_control -
 *
 * Description: This API provides interface to control PDSPs. Following
 * operations are supported :-
 *  PDSPCTRL_HLT
 *      HALT pdsp execution
 *  PDSPCTRL_STEP
 *      Set PDSP Single step mode. This option halts the PDSP and successive
 *      RESUMEs are carried as single steps.
 *  PDSPCTRL_FREERUN
 *      Set free running mode, i.e., disable single step. PDSP execution is
 *      implicitly RESUMEd as a result of this command.
 *  PDSPCTRL_RESUME
 *      RESUME pdsp execution
 *  PDSPCTRL_RST
 *      RESET PDSP and start execution from specified program counter. The
 *      16-bit program counter shoule be passed by ctl_data pointer.
 *  PDSPCTRL_PSM
 *      Enable or Disable PSM mode. ctl_data should be passed as pointer to
 *      boolean (32-bit integer) flag indicating desired enable (!0) or disable
 *      (0) status of PSM. Note that pdsp_id value is ignored for this option.
 *
 * Note:
 *   Setting option PDSPCTRL_STEP just sets the PDSP in single step
 *  mode and halts its execution, actual single stepping should be performed by
 *  calling this API with PDSPCTRL_RESUME option per step till free
 *  running is enabled explicitly with option PDSPCTRL_FREERUN
 *  single step or halting the pdsp
 *
 * Precondition:
 *  -   ti_ppd_init
 *
 * Parameters:
 *  id (IN)         - Id of PDSP to control: CPDSP(0), MPDSP(1), QPDSP(2).
 *  ctl_op (IN)     - Eiter of the PDSP control options as explained above.
 *  ctl_data (IN)   - Pointer to data corresponding the pdsp control option.
 *
 * Return:
 *  0 on Success, <0 on error.
 */
Int32 pdsp_control (pdsp_id_t   pdsp_id, Uint32 ctl_op, Ptr ctl_data);

#endif

#endif
