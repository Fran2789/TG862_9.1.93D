/*

Copyright (c) 2013-2016 ARRIS Enterprises, LLC

All rights reserved.

Redistribution and use in source and binary forms, with or without 
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, 
   this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice, 
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.
3. The name of the author may not be used to endorse or promote products 
   derived from this software without specific prior written permission.


Alternatively, this software may be distributed under the terms of the
GNU General Public License ("GPL") version 2 as published by the Free
Software Foundation. 
 

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED 
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. 
IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; 
OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR 
OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF 
ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

GPLv2 license:

This program is free software; you can redistribute it and/or modify it 
under the terms of the GNU General Public License as published by the 
Free Software Foundation; either version 2 of the License, or 
(at your option) any later version.

This program is distributed in the hope that it will be useful, but 
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY 
or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License 
for more details.

You should have received a copy of the GNU General Public License along with 
this program; if not, write to the Free Software Foundation, Inc., 
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

*/

// #include <_tistdtypes.h>
#ifndef __ARRIS_MOD_H__
#define __ARRIS_MOD_H__

#ifdef __KERNEL__
#include <linux/cdev.h>


/* note: prints function name for you */
extern unsigned int debug_mask;
extern unsigned int debug_level;

#define DEBUG_MOD       (0x00000001)
#define DEBUG_RIP       (0x00000002)

#define DEBUG_NONE      (0)
#define DEBUG_ERROR     (1)
#define DEBUG_INFO      (2)
#define DEBUG_VERBOSE   (3)

#define PRINT_VERBOSE(dbg, fmt, args...)                            \
do {                                                                \
    if ( (dbg & debug_mask) && (debug_level >= DEBUG_VERBOSE) )     \
        printk( KERN_ERR "\t\t%s: " fmt, __FUNCTION__ , ## args);   \
} while(0)

#define PRINT_INFO(dbg, fmt, args...)                               \
do {                                                                \
    if ( (dbg & debug_mask) && (debug_level >= DEBUG_INFO) )        \
        printk( KERN_ERR "\t%s: " fmt, __FUNCTION__ , ## args);     \
} while(0)

#define PRINT_ERROR(dbg, fmt, args...)                              \
do {                                                                \
    if ( (dbg & debug_mask) && (debug_level >= DEBUG_ERROR) )       \
        printk( KERN_ERR "%s: " fmt, __FUNCTION__ , ## args);       \
} while(0)

/************************************************************************/
/*     ARRIS kernel modules driver private data                         */
/************************************************************************/
typedef struct _arris_dev_t {
    struct cdev             cdev;
    char                    name[16];
} arris_dev_t;

#endif /* __KERNEL__ */

/* Device numbers for mknod */
#define ARRIS_MAJOR 114
#define ARRIS_MINOR 0

/* new Ioctl's created - using a value with a base that can be adjusted as per needs */
#define ARRIS_IOCTL_BASE          0

/* IOCTL definitions */
#define ARRIS_ADD_RIP_SUBNET      (ARRIS_IOCTL_BASE + 0)
#define ARRIS_CLEAR_RIP_SUBNETS   (ARRIS_IOCTL_BASE + 1)
#define ARRIS_SET_CERT_FLAG       (ARRIS_IOCTL_BASE + 2)

/* generic command with no parameters */
typedef struct
{
    unsigned int            cmd;
} arris_ioctl_command_t;

/* generic request with no parameters */
typedef struct
{
    unsigned int            req;
    unsigned int            rsp;
} arris_ioctl_request_t;


// TWV ECN7 fixes
typedef struct
{
    unsigned int            addr;
    unsigned int            prefix;
} arris_add_rip_subnet_t;

typedef struct
{
    unsigned int            cmd;
    arris_add_rip_subnet_t  sn;
} arris_ioctl_add_rip_subnet_t;

typedef struct
{
    unsigned int            cmd;
    unsigned long           setting;
} arris_ioctl_set_cert_flag_t;

#endif /* __ARRIS_MOD_H__ */

