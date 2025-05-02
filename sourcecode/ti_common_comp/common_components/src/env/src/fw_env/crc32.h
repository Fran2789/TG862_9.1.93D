/* 
  GPL LICENSE SUMMARY

  Copyright(c) 2006-2013 Intel Corporation.

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
*/	

/*! \file crc32.h
    \brief CRC32 implementation header file
*/

#include "sys_types.h"

/**************************************************************************/
/*! \fn Uint32 crc32( Uint32 crc, const Uint8 *buf, Uint32 len )
**************************************************************************
 *  \brief  CRC32 implementation
 *  \param[in] crc: Initial CRC
 *  \param[in] buf: Pointer to data buffer
 *  \param[in] len: Buffer length
 *  \return CRC32 value
  **************************************************************************/
Uint32 crc32( Uint32 crc, const Uint8 *buf, Uint32 len );

