/*
 * udhcpc_cfg.h
 *
 * The file contains the definitions of optional configurations for udhcp.
 * 
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

#ifndef _UDHCPC_CFG_H
#define _UDHCPC_CFG_H

#include <autoconf.h>

#ifndef CONFIG_TI_TIUDHCPC_MAX_OPTION_BUFSIZE
#define CONFIG_TI_TIUDHCPC_MAX_OPTION_BUFSIZE	400
#endif

#ifndef CONFIG_TI_TIUDHCPC_MAX_BACKOFF_TIME
#define CONFIG_TI_TIUDHCPC_MAX_BACKOFF_TIME		64
#endif 

#ifndef CONFIG_TI_TIUDHCPC_MAX_CONFIG_ATTEMPTS_THRESHOLD
#define CONFIG_TI_TIUDHCPC_MAX_CONFIG_ATTEMPTS_THRESHOLD	5
#endif

#ifndef CONFIG_TI_TIUDHCPC_RESTART_DELAY
#define CONFIG_TI_TIUDHCPC_RESTART_DELAY		10
#endif

#endif /* _UDHCPC_CFG_H */

