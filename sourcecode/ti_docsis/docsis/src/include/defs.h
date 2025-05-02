/*

Copyright (c) 2002-2016 ARRIS Enterprises, Inc.

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

/* $Header: Z:\\rcs\\d\\ab15c\\lib\\defs.h,v 1.37 2000-07-05 15:28:24-04 wintersd Exp $ */

// WARNING:  Modifying this file can affect the model5/MMVP boot load.
// file defs.h

#ifndef DEFS_H
#define DEFS_H

typedef signed long     sint32;            // signed 4 byte value
typedef signed short    sint16;            // signed 2 byte value
typedef signed char     sint8;             // signed 1 byte value
// 
// 
//typedef unsigned long   uint32;            // unsigned 4 byte value
// *** uint32 is should be defined as an unsigned long, but is defined as an unsigned int for vxWorks compatability...
// typedef unsigned int    UINT32;            // unsigned 4 byte value
typedef unsigned int    uint32;            // unsigned 4 byte value
typedef unsigned short  uint16;            // unsigned 2 byte value
typedef unsigned char   uint8;             // unsigned 1 byte value
//typedef uint8           boolean;           // unsigned 1 byte value which will only be 1 or 0
typedef int             boolean;          // Re-typedef'd for vxWorks compatability.


#endif
