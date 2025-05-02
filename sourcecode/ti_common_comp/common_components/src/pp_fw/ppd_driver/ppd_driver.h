/*
*
*  GPL LICENSE SUMMARY
*
*  Copyright(c) 2014 Intel Corporation. All rights reserved.
*
*  This program is free software; you can redistribute it and/or modify
*  it under the terms of version 2 of the GNU General Public License as
*  published by the Free Software Foundation.
*
*  This program is distributed in the hope that it will be useful, but
*  WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
*  General Public License for more details.
*
*  You should have received a copy of the GNU General Public License
*  along with this program; if not, write to the Free Software
*  Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
*  The full GNU General Public License is included in this distribution
*  in the file called LICENSE.GPL.
*
*  Contact Information:
*    Intel Corporation
*    2200 Mission College Blvd.
*    Santa Clara, CA  97052
*
*/

#include <linux/ioctl.h>

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
#define PPD_DRIVER_MODULE_ID                    (0xBA)

#define PPD_DRIVER_INIT_PPD                     _IO  (PPD_DRIVER_MODULE_ID, 1)
#define PPD_DRIVER_INIT_PREFETCHER              _IO  (PPD_DRIVER_MODULE_ID, 2)




