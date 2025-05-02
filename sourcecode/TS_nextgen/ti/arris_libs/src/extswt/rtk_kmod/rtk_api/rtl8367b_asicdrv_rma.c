/*
 * Copyright (C) 2013 Realtek Semiconductor Corp.
 * All Rights Reserved.
 *
 * Unless you and Realtek execute a separate written software license
 * agreement governing use of this software, this software is licensed
 * to you under the terms of the GNU General Public License version 2,
 * available at https://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
 *
 * $Revision: 14202 $
 * $Date: 2010-11-16 15:13:00 +0800 (週二, 16 十一月 2010) $
 *
 * Purpose : RTL8367B switch high-level API for RTL8367B
 * Feature : RMA related functions
 *
 */
#include <rtl8367b_asicdrv_rma.h>
/* Function Name:
 *      rtl8367b_setAsicRma
 * Description:
 *      Set reserved multicast address for CPU trapping
 * Input:
 *      index   - reserved multicast LSB byte, 0x00~0x2F is available value
 *      pRmacfg     - type of RMA for trapping frame type setting
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK       - Success
 *      RT_ERR_SMI      - SMI access error
 *      RT_ERR_RMA_ADDR - Invalid RMA address index
 * Note:
 *      None
 */
ret_t rtl8367b_setAsicRma(rtk_uint32 index, rtl8367b_rma_t* pRmacfg)
{
    rtk_uint32 regData;
    ret_t retVal;

    if(index > RTL8367B_RMAMAX)
        return RT_ERR_RMA_ADDR;

    regData = *(rtk_uint16*)pRmacfg;

    if( (index >= 0x4 && index <= 0x7) || (index >= 0x9 && index <= 0x0C) || (0x0F == index))
        index = 0x04;
    else if((index >= 0x13 && index <= 0x17) || (0x19 == index) || (index >= 0x1B && index <= 0x1f))
        index = 0x13;
    else if(index >= 0x22 && index <= 0x2F)
        index = 0x22;

    retVal = rtl8367b_setAsicRegBits(RTL8367B_REG_RMA_CTRL00, RTL8367B_TRAP_PRIORITY_MASK, pRmacfg->trap_priority);
    if(retVal != RT_ERR_OK)
        return retVal;

    return rtl8367b_setAsicReg(RTL8367B_REG_RMA_CTRL00+index, regData);
}
/* Function Name:
 *      rtl8367b_getAsicRma
 * Description:
 *      Get reserved multicast address for CPU trapping
 * Input:
 *      index   - reserved multicast LSB byte, 0x00~0x2F is available value
 *      rmacfg  - type of RMA for trapping frame type setting
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK       - Success
 *      RT_ERR_SMI      - SMI access error
 *      RT_ERR_RMA_ADDR - Invalid RMA address index
 * Note:
 *      None
 */
ret_t rtl8367b_getAsicRma(rtk_uint32 index, rtl8367b_rma_t* pRmacfg)
{
    ret_t retVal;
    rtk_uint32 regData;

    if(index > RTL8367B_RMAMAX)
        return RT_ERR_RMA_ADDR;

    if( (index >= 0x4 && index <= 0x7) || (index >= 0x9 && index <= 0x0C) || (0x0F == index))
        index = 0x04;
    else if((index >= 0x13 && index <= 0x17) || (0x19 == index) || (index >= 0x1B && index <= 0x1f))
        index = 0x13;
    else if(index >= 0x22 && index <= 0x2F)
        index = 0x22;

    retVal = rtl8367b_getAsicReg(RTL8367B_REG_RMA_CTRL00+index, &regData);
    if(retVal != RT_ERR_OK)
        return retVal;

    *pRmacfg = *(rtl8367b_rma_t*)(&regData);

    retVal = rtl8367b_getAsicRegBits(RTL8367B_REG_RMA_CTRL00, RTL8367B_TRAP_PRIORITY_MASK, &regData);
    if(retVal != RT_ERR_OK)
        return retVal;

    pRmacfg->trap_priority = regData;

    return RT_ERR_OK;
}

