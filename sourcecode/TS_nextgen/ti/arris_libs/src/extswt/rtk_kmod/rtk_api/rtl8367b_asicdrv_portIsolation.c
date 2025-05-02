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
 * Feature : Port isolation related functions
 *
 */

#include <rtl8367b_asicdrv_portIsolation.h>
/* Function Name:
 *      rtl8367b_setAsicPortIsolationPermittedPortmask
 * Description:
 *      Set permitted port isolation portmask
 * Input:
 *      port            - Physical port number (0~7)
 *      permitPortmask  - port mask
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK           - Success
 *      RT_ERR_SMI          - SMI access error
 *      RT_ERR_PORT_ID      - Invalid port number
 *      RT_ERR_PORT_MASK    - Invalid portmask
 * Note:
 *      None
 */
ret_t rtl8367b_setAsicPortIsolationPermittedPortmask(rtk_uint32 port, rtk_uint32 permitPortmask)
{
    if(port >= RTL8367B_PORTNO)
        return RT_ERR_PORT_ID;

    if( permitPortmask > RTL8367B_PORTMASK)
        return RT_ERR_PORT_MASK;

    return rtl8367b_setAsicReg(RTL8367B_PORT_ISOLATION_PORT_MASK_REG(port), permitPortmask);
}
/* Function Name:
 *      rtl8367b_getAsicPortIsolationPermittedPortmask
 * Description:
 *      Get permitted port isolation portmask
 * Input:
 *      port                - Physical port number (0~7)
 *      pPermitPortmask     - port mask
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK           - Success
 *      RT_ERR_SMI          - SMI access error
 *      RT_ERR_PORT_ID      - Invalid port number
 * Note:
 *      None
 */
ret_t rtl8367b_getAsicPortIsolationPermittedPortmask(rtk_uint32 port, rtk_uint32 *pPermitPortmask)
{
    if(port >= RTL8367B_PORTNO)
        return RT_ERR_PORT_ID;

    return rtl8367b_getAsicReg(RTL8367B_PORT_ISOLATION_PORT_MASK_REG(port), pPermitPortmask);
}
/* Function Name:
 *      rtl8367b_setAsicPortIsolationEfid
 * Description:
 *      Set port isolation EFID
 * Input:
 *      port    - Physical port number (0~7)
 *      efid    - EFID (0~7)
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK           - Success
 *      RT_ERR_SMI          - SMI access error
 *      RT_ERR_PORT_ID      - Invalid port number
 *      RT_ERR_OUT_OF_RANGE - Input parameter out of range
 * Note:
 *      EFID is used in individual learning in filtering database
 */
ret_t rtl8367b_setAsicPortIsolationEfid(rtk_uint32 port, rtk_uint32 efid)
{
    if(port >= RTL8367B_PORTNO)
        return RT_ERR_PORT_ID;

    if( efid > RTL8367B_EFIDMAX)
        return RT_ERR_OUT_OF_RANGE;

    return rtl8367b_setAsicRegBits(RTL8367B_PORT_EFID_REG(port), RTL8367B_PORT_EFID_MASK(port), efid);
}
/* Function Name:
 *      rtl8367b_getAsicPortIsolationEfid
 * Description:
 *      Get port isolation EFID
 * Input:
 *      port    - Physical port number (0~7)
 *      pEfid   - EFID (0~7)
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK           - Success
 *      RT_ERR_SMI          - SMI access error
 *      RT_ERR_PORT_ID      - Invalid port number
 * Note:
 *      None
 */
ret_t rtl8367b_getAsicPortIsolationEfid(rtk_uint32 port, rtk_uint32 *pEfid)
{
    if(port >= RTL8367B_PORTNO)
        return RT_ERR_PORT_ID;

    return rtl8367b_getAsicRegBits(RTL8367B_PORT_EFID_REG(port), RTL8367B_PORT_EFID_MASK(port), pEfid);
}

