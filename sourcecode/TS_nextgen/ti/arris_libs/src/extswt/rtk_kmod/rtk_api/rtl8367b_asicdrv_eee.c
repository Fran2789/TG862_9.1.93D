/*
 * Copyright (C) 2013 Realtek Semiconductor Corp.
 * All Rights Reserved.
 *
 * Unless you and Realtek execute a separate written software license
 * agreement governing use of this software, this software is licensed
 * to you under the terms of the GNU General Public License version 2,
 * available at https://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
 *
 * $Revision: 6665 $
 * $Date: 2009-10-30 15:24:44 +0800 (星期五, 30 十月 2009) $
 *
 * Purpose : RTL8370 switch high-level API for RTL8367B
 * Feature :
 *
 */

#include <rtl8367b_asicdrv_eee.h>

/*
@func ret_t | rtl8367b_setAsicEee100M | Set eee force mode function enable/disable.
@parm rtk_uint32 | port | The port number.
@parm rtk_uint32 | enabled | 1: enabled, 0: disabled.
@rvalue RT_ERR_OK | Success.
@rvalue RT_ERR_SMI | SMI access error.
@rvalue RT_ERR_INPUT | Invalid input parameter.
@comm
    This API set the 100M EEE enable function.

*/
ret_t rtl8367b_setAsicEee100M(rtk_uint32 port, rtk_uint32 enable)
{
    if(port >= RTL8367B_PORTNO)
        return RT_ERR_PORT_ID;

    if (enable > 1)
        return RT_ERR_INPUT;

    return rtl8367b_setAsicRegBit(RTL8367B_PORT_EEE_CFG_REG(port), RTL8367B_PORT_EEE_100M_OFFSET, enable);
}

/*
@func ret_t | rtl8367b_getAsicEee100M | Get 100M eee enable/disable.
@parm rtk_uint32 | port | The port number.
@parm rtk_uint32* | enabled | 1: enabled, 0: disabled.
@rvalue RT_ERR_OK | Success.
@rvalue RT_ERR_SMI | SMI access error.
@rvalue RT_ERR_INPUT | Invalid input parameter.
@comm
    This API get the 100M EEE function.
*/
ret_t rtl8367b_getAsicEee100M(rtk_uint32 port, rtk_uint32 *enable)
{
    if(port >= RTL8367B_PORTNO)
        return RT_ERR_PORT_ID;

    return rtl8367b_getAsicRegBit(RTL8367B_PORT_EEE_CFG_REG(port), RTL8367B_PORT_EEE_100M_OFFSET, enable);
}

/*
@func ret_t | rtl8367b_setAsicEeeGiga | Set eee force mode function enable/disable.
@parm rtk_uint32 | port | The port number.
@parm rtk_uint32 | enabled | 1: enabled, 0: disabled.
@rvalue RT_ERR_OK | Success.
@rvalue RT_ERR_SMI | SMI access error.
@rvalue RT_ERR_INPUT | Invalid input parameter.
@comm
    This API set the 100M EEE enable function.

*/
ret_t rtl8367b_setAsicEeeGiga(rtk_uint32 port, rtk_uint32 enable)
{
    if(port >= RTL8367B_PORTNO)
        return RT_ERR_PORT_ID;

    if (enable > 1)
        return RT_ERR_INPUT;

    return rtl8367b_setAsicRegBit(RTL8367B_PORT_EEE_CFG_REG(port), RTL8367B_PORT_EEE_GIGA_OFFSET, enable);
}

/*
@func ret_t | rtl8367b_getAsicEeeGiga | Get 100M eee enable/disable.
@parm rtk_uint32 | port | The port number.
@parm rtk_uint32* | enabled | 1: enabled, 0: disabled.
@rvalue RT_ERR_OK | Success.
@rvalue RT_ERR_SMI | SMI access error.
@rvalue RT_ERR_INPUT | Invalid input parameter.
@comm
    This API get the 100M EEE function.
*/
ret_t rtl8367b_getAsicEeeGiga(rtk_uint32 port, rtk_uint32 *enable)
{
    if(port >= RTL8367B_PORTNO)
        return RT_ERR_PORT_ID;

    return rtl8367b_getAsicRegBit(RTL8367B_PORT_EEE_CFG_REG(port), RTL8367B_PORT_EEE_GIGA_OFFSET, enable);
}
