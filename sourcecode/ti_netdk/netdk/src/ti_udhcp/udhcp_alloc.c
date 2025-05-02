/*
 * udhcp_alloc.c
 *
 * The file contains a wrapper for memory allocation functions
 *
 * Copyright (C) 2008 Texas Instruments Incorporated - http://www.ti.com/
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as 
 * published by the Free Software Foundation version 2.
 *
 * This program is distributed “as is” WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "udhcp_alloc.h"

void *udhcp_alloc(size_t size)
{
	/*
	right now just call malloc
	*/
	return (malloc(size));
}

void udhcp_free(void *ptr)
{
	free(ptr);
}
