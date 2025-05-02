/*
 * Copyright (C) 2013 Realtek Semiconductor Corp.
 * All Rights Reserved.
 *
 * Unless you and Realtek execute a separate written software license
 * agreement governing use of this software, this software is licensed
 * to you under the terms of the GNU General Public License version 2,
 * available at https://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
 *
 */

#ifndef _RTL8367B_ASICDRV_EEE_H_
#define _RTL8367B_ASICDRV_EEE_H_

#include <rtl8367b_asicdrv.h>

extern ret_t rtl8367b_setAsicEee100M(rtk_uint32 port, rtk_uint32 enable);
extern ret_t rtl8367b_getAsicEee100M(rtk_uint32 port, rtk_uint32 *enable);
extern ret_t rtl8367b_setAsicEeeGiga(rtk_uint32 port, rtk_uint32 enable);
extern ret_t rtl8367b_getAsicEeeGiga(rtk_uint32 port, rtk_uint32 *enable);


#endif /*_RTL8367B_ASICDRV_EEE_H_*/
