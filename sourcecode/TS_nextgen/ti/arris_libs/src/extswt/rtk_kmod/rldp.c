/*

Copyright (c) 2013-2017 ARRIS Enterprises, LLC

All rights reserved.

Redistribution and use in source and binary forms, with or without 
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, 
   this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice, 
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.
3. The name of the author may not be used to endorse or promote products 
   derived from this software without specific prior written permission.


Alternatively, this software may be distributed under the terms of the
GNU General Public License ("GPL") version 2 as published by the Free
Software Foundation. 
 

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED 
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. 
IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; 
OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR 
OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF 
ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

GPLv2 license:

This program is free software; you can redistribute it and/or modify it 
under the terms of the GNU General Public License as published by the 
Free Software Foundation; either version 2 of the License, or 
(at your option) any later version.

This program is distributed in the hope that it will be useful, but 
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY 
or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License 
for more details.

You should have received a copy of the GNU General Public License along with 
this program; if not, write to the Free Software Foundation, Inc., 
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

*/

#include <rtl8367b_asicdrv_misc.h>
#include <rtl8367b_asicdrv_rldp.h>
#include <linux/kernel.h>
#include <linux/times.h>
#include <linux/ti_hil.h>
#include "rtk_api_ext.h"

struct timer_list       rtk_rldp_query_timer;

/*
* Query loop timer start
*/
rtk_api_ret_t rtk_rldp_start_query_timer()
{
    mod_timer(&rtk_rldp_query_timer, jiffies + 4*HZ);
    return 0;
}

/*
* Query loop timer expire
*/
static void rtk_rldp_query_timer_expired(unsigned long arg)
{
    rtk_uint32      ret;
    rtk_uint32      loopedPortmask;        
    rtk_uint32      port;
    rtk_port_mac_ability_t  ability;
    rtk_port_linkStatus_t   linkStatus;
    rtk_data_t                      speed, duplex;
    static rtk_uint32      BlockedPortmask = 0;        

    /*If Loop happened, use this API to check loop ports*/
    if ((ret = rtl8367b_getAsicRldpLoopedPortmask(&loopedPortmask))!=RT_ERR_OK)
    {
       printk("rtl8367b_getAsicRldpLoopedPortmask return error %d\n", ret);
       return;
    }
    
    if (loopedPortmask)
    {
        // external port
        for (port=0; port<5; port++) 
        {
            if (loopedPortmask & 1 << port) 
            {
                // link down
                ret = rtk_port_macForceLink_get(port, &ability);
                if ( ret == RT_ERR_OK ) 
                {
                    ability.forcemode = 1;
                    ability.link = DISABLE;
                    rtk_port_macForceLink_set(port, &ability);
                    BlockedPortmask |=  (1 << port);
                    printk("Loop detected, rtk_port_macForceLink_set disable port %d\n", port);        
                }
                else
                {
                    printk("rtk_port_macForceLink_get return error %d\n", ret);        
                }
            }
        }
    }
    else if (BlockedPortmask)
    {
        // external port
        for (port=0; port<5; port++) 
        {
            if (BlockedPortmask & (1 << port))
           {
                ret = rtk_port_phyStatus_get( port, &linkStatus, &speed, &duplex);
                if ( ret != RT_ERR_OK ) 
                {
                    printk("rtk_port_macForceLink_get return error %d\n", ret);        
                    continue;
                }
                
                if ( !linkStatus)
                {
                    // restore disconnected port
                    ret = rtk_port_macForceLink_get(port, &ability);
                    if ( ret == RT_ERR_OK ) 
                    {
                        ability.forcemode=0;
                        ret = rtk_port_macForceLink_set(port, &ability);
                        BlockedPortmask &=  ~(1 << port);
                        printk("Restore from loop state, rtk_port_macForceLink_set enable port %d\n", port);        
                    }
                    else
                    {
                        printk("rtk_port_macForceLink_get return error %d\n", ret);        
                    }
                }
            }
        }
    }

    if (BlockedPortmask)
    {
        rtk_rldp_start_query_timer();
    }
}

rtk_api_ret_t rtk_rldp_init(ether_addr_t* pMac)
{
    static int initOnce = 0;
    ether_addr_t sw_mac; 
    rtk_api_ret_t retVal;
    rtk_uint32 portmask = 0xff;
     
     /*Set Switch MAC*/
     if (!initOnce)
     {
         memcpy(sw_mac.octet, pMac->octet, ETHER_ADDR_LEN);
         initOnce = 1;
     }
     
     if((retVal=rtl8367b_setAsicMacAddress(sw_mac))!=RT_ERR_OK)
         return retVal; 
    
     /*Enable RLDP function*/
     if((retVal = rtl8367b_setAsicRldp(ENABLED)) != RT_ERR_OK)
         return retVal;
         
     /*Enable RLDP tx RLDP packet port mask*/                
     if((retVal = rtl8367b_setAsicRldpTxPortmask(portmask)) != RT_ERR_OK)
         return retVal;   
         
     /*Set check time to 4, period to 2 second*/    
     if ((retVal = rtl8367b_setAsicRldpCheckingStatePara(4, 2000))!=RT_ERR_OK)
         return retVal;
         
     /*Set check time in loop state to 4, period to 2 second*/    
     if ((retVal = rtl8367b_setAsicRldpLoopStatePara(4, 2000))!=RT_ERR_OK)
         return retVal;
    
     /*  Periodically send RLDP packet */
     if ((retVal = rtl8367b_setAsicRldp_mode(RLDP_TRANSMIT_MODE_1))!=RT_ERR_OK)
         return retVal;                           

     setup_timer(&rtk_rldp_query_timer, rtk_rldp_query_timer_expired, 0);
         
     return RT_ERR_OK;
}

rtk_api_ret_t rtk_rldp_enable(int enabled)
{
    rtk_api_ret_t retVal;
    if (enabled)
    {
        /*Enable RLDP function*/
        if((retVal = rtl8367b_setAsicRldp(ENABLED)) != RT_ERR_OK)
            return retVal;
    }
    else
    {
        /*Disable RLDP function*/
        if((retVal = rtl8367b_setAsicRldp(DISABLED)) != RT_ERR_OK)
            return retVal;
    }
    
    return RT_ERR_OK;
}


