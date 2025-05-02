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

#ifndef _RTL8367B_ASICDRV_STORM_H_
#define _RTL8367B_ASICDRV_STORM_H_

#include <rtl8367b_asicdrv.h>

extern ret_t rtl8367b_setAsicStormFilterBroadcastEnable(rtk_uint32 port, rtk_uint32 enabled);
extern ret_t rtl8367b_getAsicStormFilterBroadcastEnable(rtk_uint32 port, rtk_uint32 *pEnabled);
extern ret_t rtl8367b_setAsicStormFilterBroadcastMeter(rtk_uint32 port, rtk_uint32 meter);
extern ret_t rtl8367b_getAsicStormFilterBroadcastMeter(rtk_uint32 port, rtk_uint32 *pMeter);
extern ret_t rtl8367b_setAsicStormFilterMulticastEnable(rtk_uint32 port, rtk_uint32 enabled);
extern ret_t rtl8367b_getAsicStormFilterMulticastEnable(rtk_uint32 port, rtk_uint32 *pEnabled);
extern ret_t rtl8367b_setAsicStormFilterMulticastMeter(rtk_uint32 port, rtk_uint32 meter);
extern ret_t rtl8367b_getAsicStormFilterMulticastMeter(rtk_uint32 port, rtk_uint32 *pMeter);
extern ret_t rtl8367b_setAsicStormFilterUnknownMulticastEnable(rtk_uint32 port, rtk_uint32 enabled);
extern ret_t rtl8367b_getAsicStormFilterUnknownMulticastEnable(rtk_uint32 port, rtk_uint32 *pEnabled);
extern ret_t rtl8367b_setAsicStormFilterUnknownMulticastMeter(rtk_uint32 port, rtk_uint32 meter);
extern ret_t rtl8367b_getAsicStormFilterUnknownMulticastMeter(rtk_uint32 port, rtk_uint32 *pMeter);
extern ret_t rtl8367b_setAsicStormFilterUnknownUnicastEnable(rtk_uint32 port, rtk_uint32 enabled);
extern ret_t rtl8367b_getAsicStormFilterUnknownUnicastEnable(rtk_uint32 port, rtk_uint32 *pEnabled);
extern ret_t rtl8367b_setAsicStormFilterUnknownUnicastMeter(rtk_uint32 port, rtk_uint32 meter);
extern ret_t rtl8367b_getAsicStormFilterUnknownUnicastMeter(rtk_uint32 port, rtk_uint32 *pMeter);

#endif /*_RTL8367B_ASICDRV_STORM_H_*/


