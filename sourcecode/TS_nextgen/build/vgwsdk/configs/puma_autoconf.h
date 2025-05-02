/*
 
  BSD LICENSE 
 
  Copyright(c) 2011-2014 Intel Corporation. All rights reserved.
 
  Redistribution and use in source and binary forms, with or without 
  modification, are permitted provided that the following conditions 
  are met:
 
    * Redistributions of source code must retain the above copyright 
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright 
      notice, this list of conditions and the following disclaimer in 
      the documentation and/or other materials provided with the 
      distribution.
 
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

/**************************************************************************/
/*                                                                        */
/*   MODULE:  puma_autoconf.h                                             */
/*   PURPOSE: To detect the right SoC of this build.                      */
/*                                                                        */
/**************************************************************************/

#ifndef _PUMA_AUTOCONF_H
#define _PUMA_AUTOCONF_H

#ifndef __KERNEL__
#include <autoconf.h>  /* In Kernel 2.6.39 no need to include autoconf.h - only user-space need */
#endif

#define SYSTEM_PUMA5_SOC_ID    (50)
#define SYSTEM_PUMA6_SOC_ID    (60)

#ifndef __KERNEL__
/* If SoC ID is Puma5 */
#define PUMA5_SOC_TYPE  (CONFIG_SYSTEM_PUMA_SOC_ID == SYSTEM_PUMA5_SOC_ID)

/* If SoC ID is Puma6 */
#define PUMA6_SOC_TYPE  (CONFIG_SYSTEM_PUMA_SOC_ID == SYSTEM_PUMA6_SOC_ID)

/* If SoC ID is Puma6 or newer (ex. Puma6MG, Puma7...) */
#define PUMA6_OR_NEWER_SOC_TYPE  (CONFIG_SYSTEM_PUMA_SOC_ID >= SYSTEM_PUMA6_SOC_ID)
#endif

#ifdef __KERNEL__
/* If SoC ID is Puma5 */
#define PUMA5_SOC_TYPE  (defined (CONFIG_MACH_PUMA5))

/* If SoC ID is Puma6 */
#define PUMA6_SOC_TYPE  (defined (CONFIG_MACH_PUMA6))

/* If SoC ID is Puma6 or newer (ex. Puma6, Puma7...) */
#define PUMA6_OR_NEWER_SOC_TYPE  (defined (CONFIG_MACH_PUMA6))
#endif

#endif
