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

#ifndef _RTL8367B_ASICDRV_INBWCTRL_H_
#define _RTL8367B_ASICDRV_INBWCTRL_H_

#include <rtl8367b_asicdrv.h>

extern ret_t rtl8367b_setAsicPortIngressBandwidth(rtk_uint32 port, rtk_uint32 bandwidth, rtk_uint32 preifg, rtk_uint32 enableFC);
extern ret_t rtl8367b_getAsicPortIngressBandwidth(rtk_uint32 port, rtk_uint32* pBandwidth, rtk_uint32* pPreifg, rtk_uint32* pEnableFC );
extern ret_t rtl8367b_setAsicPortIngressBandwidthBypass(rtk_uint32 enabled);
extern ret_t rtl8367b_getAsicPortIngressBandwidthBypass(rtk_uint32* pEnabled);


#endif /*_RTL8367B_ASICDRV_INBWCTRL_H_*/

