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
 * Feature : Field selector related functions
 *
 */
#include <rtl8367b_asicdrv_hsb.h>
/* Function Name:
 *      rtl8367b_setAsicFieldSelector
 * Description:
 *      Set user defined field selectors in HSB
 * Input:
 *      index       - index of field selector 0-15
 *      format      - Format of field selector
 *      offset      - Retrieving data offset
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK           - Success
 *      RT_ERR_SMI          - SMI access error
 *      RT_ERR_OUT_OF_RANGE - input parameter out of range
 * Note:
 *      System support 16 user defined field selctors.
 *      Each selector can be enabled or disable. User can defined retrieving 16-bits in many predefiend
 *      standard l2/l3/l4 payload.
 */
ret_t rtl8367b_setAsicFieldSelector(rtk_uint32 index, rtk_uint32 format, rtk_uint32 offset)
{
    rtk_uint32 regData;

    if(index > RTL8367B_FIELDSEL_FORMAT_NUMBER)
        return RT_ERR_OUT_OF_RANGE;

    if(format >= FIELDSEL_FORMAT_END)
        return RT_ERR_OUT_OF_RANGE;

    regData = (((format << RTL8367B_FIELD_SELECTOR_FORMAT_OFFSET) & RTL8367B_FIELD_SELECTOR_FORMAT_MASK ) |
               ((offset << RTL8367B_FIELD_SELECTOR_OFFSET_OFFSET) & RTL8367B_FIELD_SELECTOR_OFFSET_MASK ));

    return rtl8367b_setAsicReg(RTL8367B_FIELD_SELECTOR_REG(index), regData);
}
/* Function Name:
 *      rtl8367b_getAsicFieldSelector
 * Description:
 *      Get user defined field selectors in HSB
 * Input:
 *      index       - index of field selector 0-15
 *      pFormat     - Format of field selector
 *      pOffset     - Retrieving data offset
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK           - Success
 *      RT_ERR_SMI          - SMI access error
 * Note:
 *      None
 */
ret_t rtl8367b_getAsicFieldSelector(rtk_uint32 index, rtk_uint32* pFormat, rtk_uint32* pOffset)
{
    ret_t retVal;
    rtk_uint32 regData;

    retVal = rtl8367b_getAsicReg(RTL8367B_FIELD_SELECTOR_REG(index), &regData);
    if(retVal != RT_ERR_OK)
        return retVal;

    *pFormat    = ((regData & RTL8367B_FIELD_SELECTOR_FORMAT_MASK) >> RTL8367B_FIELD_SELECTOR_FORMAT_OFFSET);
    *pOffset    = ((regData & RTL8367B_FIELD_SELECTOR_OFFSET_MASK) >> RTL8367B_FIELD_SELECTOR_OFFSET_OFFSET);

    return RT_ERR_OK;
}
