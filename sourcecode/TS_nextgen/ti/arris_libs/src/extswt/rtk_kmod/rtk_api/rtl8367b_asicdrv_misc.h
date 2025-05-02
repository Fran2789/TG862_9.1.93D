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

#ifndef _RTL8367B_ASICDRV_MISC_H_
#define _RTL8367B_ASICDRV_MISC_H_

#include <rtl8367b_asicdrv.h>

extern ret_t rtl8367b_setAsicMacAddress(ether_addr_t mac);
extern ret_t rtl8367b_getAsicMacAddress(ether_addr_t *pMac);
extern ret_t rtl8367b_getAsicDebugInfo(rtk_uint32 port, rtk_uint32 *pDebugifo);
extern ret_t rtl8367b_setAsicPortJamMode(rtk_uint32 mode);
extern ret_t rtl8367b_getAsicPortJamMode(rtk_uint32* pMode);
extern ret_t rtl8367b_setAsicMaxLengthInRx(rtk_uint32 maxLength);
extern ret_t rtl8367b_getAsicMaxLengthInRx(rtk_uint32* pMaxLength);
extern ret_t rtl8367b_setAsicMaxLengthAltTxRx(rtk_uint32 maxLength, rtk_uint32 pmskGiga, rtk_uint32 pmask100M);
extern ret_t rtl8367b_getAsicMaxLengthAltTxRx(rtk_uint32* pMaxLength, rtk_uint32* pPmskGiga, rtk_uint32* pPmask100M);

#endif /*_RTL8367B_ASICDRV_MISC_H_*/

