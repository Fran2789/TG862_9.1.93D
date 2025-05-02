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

#ifndef _RTL8367B_ASICDRV_CPUTAG_H_
#define _RTL8367B_ASICDRV_CPUTAG_H_

#include <rtl8367b_asicdrv.h>

enum CPUTAG_INSERT_MODE
{
    CPUTAG_INSERT_TO_ALL = 0,
    CPUTAG_INSERT_TO_TRAPPING,
    CPUTAG_INSERT_TO_NO,
    CPUTAG_INSERT_END
};

extern ret_t rtl8367b_setAsicCputagEnable(rtk_uint32 enabled);
extern ret_t rtl8367b_getAsicCputagEnable(rtk_uint32 *pEnabled);
extern ret_t rtl8367b_setAsicCputagTrapPort(rtk_uint32 port);
extern ret_t rtl8367b_getAsicCputagTrapPort(rtk_uint32 *pPort);
extern ret_t rtl8367b_setAsicCputagPortmask(rtk_uint32 portmask);
extern ret_t rtl8367b_getAsicCputagPortmask(rtk_uint32 *pPmsk);
extern ret_t rtl8367b_setAsicCputagInsertMode(rtk_uint32 mode);
extern ret_t rtl8367b_getAsicCputagInsertMode(rtk_uint32 *pMode);
extern ret_t rtl8367b_setAsicCputagPriorityRemapping(rtk_uint32 srcPri, rtk_uint32 newPri);
extern ret_t rtl8367b_getAsicCputagPriorityRemapping(rtk_uint32 srcPri, rtk_uint32 *pNewPri);
extern ret_t rtl8367b_setAsicCputagPosition(rtk_uint32 postion);
extern ret_t rtl8367b_getAsicCputagPosition(rtk_uint32* pPostion);
extern ret_t rtl8367b_setAsicCputagMode(rtk_uint32 mode);
extern ret_t rtl8367b_getAsicCputagMode(rtk_uint32 *pMode);
extern ret_t rtl8367b_setAsicCputagRxMinLength(rtk_uint32 mode);
extern ret_t rtl8367b_getAsicCputagRxMinLength(rtk_uint32 *pMode);

#endif /*#ifndef _RTL8367B_ASICDRV_CPUTAG_H_*/

