/*
 * udhcp_alloc.h
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

#ifndef UDHCP_ALLOC
#define UDHCP_ALLOC
#include <stdlib.h>
void *udhcp_alloc(size_t size);
void udhcp_free(void *ptr);
#endif
