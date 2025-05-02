/*
 *
 * dbridge_netdev.h
 * Description:
 * Declaration of DOCSIS bridge networking related functions and types.
 *
 * Copyright (C) 2008 Texas Instruments Incorporated - http://www.ti.com/ 
 * 
 * 
 *  Redistribution and use in source and binary forms, with or without 
 *  modification, are permitted provided that the following conditions 
 *  are met:
 *
 *    Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer.
 *
 *    Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the 
 *    documentation and/or other materials provided with the   
 *    distribution.
 *
 *    Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
*/

#ifndef _DBRIDGE_NERDEV_H_
#define _DBRIDGE_NERDEV_H_


/**************************************************************************/
/*      INCLUDES                                                          */
/**************************************************************************/

#include <puma_autoconf.h>

#include <linux/netdevice.h>
#include <linux/kernel.h>
#include <linux/etherdevice.h>

#ifdef CONFIG_CAT_L2SWITCH_NID
#include <linux/cat_l2switch_netdev.h>
#endif

/**************************************************************************/
/*  Various defines                                                       */
/**************************************************************************/
#define CABLE_DEV_NAME              "cni0"
#ifdef CONFIG_CAT_L2SWITCH_NID
#define ETH_DEV_NAME                L2SW_NETDEV_DATA0
#else
#define ETH_DEV_NAME                "eth0"
#endif
#define USB_DEV_NAME                "usb0"
#define LBRIGE_CONNECTION_DEV_NAME  "lbr0"
#define LAN_IP_DEV_NAME             "lan0"
#define WAN_IP_DEV_NAME             "wan0"
#define EROUTER_NET_DEV_NAME        "erouter0"
#ifdef DSG
#define LBRIGE_CONNECTION_4DSGI_DEV_NAME "lbr1"
#define ESTB_DSG_DEV_NAME            "dsgtn0"
#define ESTB_IP_DEV_NAME            "dsgip0"
#endif
typedef struct DbridgeNetDev
{
    /* Pointer to the network device. */
    struct net_device*          ptr_device;

    /* Keep track of the statistics. */
    struct net_device_stats     stats;

    /* ARRIS ADD : allow upstream dbridge bypass for speedtest */
    unsigned int                Ipv4FilterAddress;
    unsigned int                Ipv4FilterPort;
    int                         DbridgeBypass;
    /* END ARRIS */

}   DbridgeNetDev_t;


/**************************************************************************/
/*      INTERFACE  Defines and Structs                                    */
/**************************************************************************/


/**************************************************************************/
/*      Default Values                                          */
/**************************************************************************/



/**************************************************************************/
/*      INTERFACE FUNCTIONS Prototypes:                                   */


/**************************************************************************/

/*! \fn int Dbridge_NetDevInit(void)                                     
 *  \brief DOCSIS net device initialization.
 *  \param[in] no input.
 *  \param[out] no output.
 *  \return 0=OK or error status.
 */

int Dbridge_NetDevInit(void);

/*! \fn int Dbridge_NetDevCreate(void)                                     
 *  \brief alloc and register net device for docsis bridge.
 *  \param[in] device name.
 *  \param[in] device setup function.
 *  \return net_device.
 */
struct net_device*  Dbridge_NetDevCreate(char* name,void (*setup)(struct net_device *));


/*! \fn void Dbridge_NetDevSetup(struct net_device *ptr_netdev)                                     
 *  \brief Initialize and override the Lbridge Connection Dev network structure.
 *  \param[in] pointer to Lbridge Connection Device.
 *  \return NONE.
 */
void Dbridge_NetDevSetup(struct net_device *ptr_netdev);

#endif

