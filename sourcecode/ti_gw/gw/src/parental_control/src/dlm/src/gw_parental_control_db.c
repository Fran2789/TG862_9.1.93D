/*
 *
 * gw_parental_control_db.c
 * Description:
 * GW parental control database implementation
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

#define _GW_PARENTAL_CONTROL_DB_C_

/*! \file gw_parental_control_db.c
    \brief GW parental control DLM DB
*/

/**************************************************************************/
/*      INCLUDES:                                                         */
/**************************************************************************/
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/list.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>

#include "pal.h"
#include "gw_parental_control_db.h"


/**************************************************************************/
/*      EXTERNS Declaration:                                              */
/**************************************************************************/
 
/**************************************************************************/
/*      DEFINES:                                                          */
/**************************************************************************/
#define GW_PRCTL_DEVICE_MAX_NUM (12)
#define GW_PRCTL_DEV_NAME_MAX_LEN (16)
#define GW_PRCTL_MAC_ADDR_LEN    (6)
#define GW_PRCTL_CPE_MAC_FILTER_DEV_MAX_NUM (256)

typedef struct gwMacAddr 
{
    Uint8 hw[GW_PRCTL_MAC_ADDR_LEN];

} gwMacAddr_t;


/*! \var typedef struct trustedDeviceMacAddrRow
    \brief Structure contains information about a GW trusted device MAC.
*/
typedef struct trustedDeviceMacAddrRow 
{
    gwMacAddr_t     mac;
    Uint32          passthroughPackets;
} trustedDeviceMacAddrRow_t;

/*! \var typedef struct filteredCpeMacAddrRow
    \brief Structure contains information about a CPE MAC filtered from the WAN.
*/
typedef struct filteredCpeMacAddrRow 
{
    gwMacAddr_t     mac;
    Uint32          droppedPackets;
} filteredCpeMacAddrRow_t;

/*! \var typedef struct GwPcTrustedDeviceDb
    \brief Structure contains information about the GW trusted device.
*/
typedef struct GwPcTrustedDeviceDb
{
    Bool trustedDevicePcEnable;
    trustedDeviceMacAddrRow_t deviceMacTable[GW_PRCTL_DEVICE_MAX_NUM];
    Uint32 currentDeviceNum;
}GwPcTrustedDeviceDb_t;

/*! \var typedef struct GwCpeMacFilteringDb
    \brief Structure contains information about CPE MAC addresses to be blocked from the WAN.
*/
typedef struct GwCpeMacFilteringDb
{
    Bool cpeMacFilteringEnable;
    filteredCpeMacAddrRow_t deviceMacTable[GW_PRCTL_CPE_MAC_FILTER_DEV_MAX_NUM];
    Uint32 currentDeviceNum;
}GwCpeMacFilteringDb_t;

/*! \var typedef struct GwParentalControlDb
    \brief Structure contains information about the GW parental control.
*/
typedef struct GwParentalControlDb
{
    Uint32 pcKeywordDropPackets;
    GwPcTrustedDeviceDb_t trustedDevices;
    GwCpeMacFilteringDb_t cpeMacFilteringDevices;
}GwParentalControlDb_t;


/**************************************************************************/
/*      LOCAL DECLARATIONS:                                               */
/**************************************************************************/
#ifdef CONFIG_TI_GW_EGRESS_HOOK
static int GwEgressHook(struct sk_buff* skb);
static Bool GwApplyCpeMacFiltering(struct sk_buff* skb);
static Bool GwApplyKeywordFiltering(struct sk_buff* skb);
static int configureGwEgressHookHandler(Bool reg, Char* interface);
#endif
 
/**************************************************************************/
/*      LOCAL VARIABLES:                                                  */
/**************************************************************************/

/* parental control DB parameter */
static GwParentalControlDb_t gwParentalControlDb;

/**************************************************************************/
/*      INTERFACE FUNCTIONS Implementation:                               */
/**************************************************************************/

/**************************************************************************/
/*! \fn int GwParentalControlDb_Init(void)                                     
 **************************************************************************
 *  \brief GW parental control data base initialization.
 *  \param[in] no input.
 *  \param[out] no output.
 *  \return OK or error status.
 **************************************************************************/
int GwParentalControlDb_Init(void)
{
    Uint32 lockKey;

    PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);

    memset((void *)&gwParentalControlDb, 0, sizeof(GwParentalControlDb_t));

    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);

    printk(KERN_INFO "\nInit GW parental control DB done\n");
    
    return 0;
}

/**************************************************************************/
/*! \fn int GwParentalControlDb_SetEnableStatus(Uint32 status)                                   
 **************************************************************************
 *  \brief Set GW parental control enable status.
 *  \param[in] status -  enable status
 *  \param[out] no output.
 *  \return OK or error status.
 **************************************************************************/
int GwParentalControlDb_SetEnableStatus(Uint32 status, Uint8* buf, int bufLen)
{
    Uint32 lockKey;
    Char interface[GW_PRCTL_DEV_NAME_MAX_LEN] = {0};
    Bool plantHook = False;

    PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);

    if (status & PARENTAL_CONTROL_DEV_HOOK_KEYWORD_FILT)
    {
        plantHook = True;
    }
    
    if (status & PARENTAL_CONTROL_DEV_HOOK_TRUSTED_MAC)
    {
        gwParentalControlDb.trustedDevices.trustedDevicePcEnable = True;
    }
            
    if (status & PARENTAL_CONTROL_DEV_HOOK_CPE_MAC_FILT)
    {
        gwParentalControlDb.cpeMacFilteringDevices.cpeMacFilteringEnable = True;
        plantHook = True;
    }

    if (status == PARENTAL_CONTROL_DEV_HOOK_NONE)
    {
        gwParentalControlDb.cpeMacFilteringDevices.cpeMacFilteringEnable = False;
        gwParentalControlDb.trustedDevices.trustedDevicePcEnable = False;
    }

#ifdef CONFIG_TI_GW_EGRESS_HOOK
    if (bufLen < GW_PRCTL_DEV_NAME_MAX_LEN)
    {
        memcpy(interface, buf, bufLen);
        configureGwEgressHookHandler(plantHook, interface);
    }
#endif

    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
    return 0;
}

/**************************************************************************/
/*! \fn int GwParentalControlDb_AddMac(Uint8 *buf, int bufLen)                                   
 **************************************************************************
 *  \brief Set trusted mac address.
 *  \param[in] buf - mac address
 *  \param[in] bufLen - buffer len
 *  \param[out] no output.
 *  \return OK or error status.
 **************************************************************************/
int GwParentalControlDb_AddMac(Uint8 *buf, int bufLen)
{
    Uint32 index;
    Uint8 *mac;
    Uint32 lockKey;

    if ((!buf) || (bufLen != GW_PRCTL_MAC_ADDR_LEN))
    {
        return -1;
    }

    if (gwParentalControlDb.trustedDevices.currentDeviceNum >= GW_PRCTL_DEVICE_MAX_NUM)
    {
        return -2;
    }
    
    PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);

    index = gwParentalControlDb.trustedDevices.currentDeviceNum;
    mac = gwParentalControlDb.trustedDevices.deviceMacTable[index].mac.hw;
    memcpy(mac, buf, bufLen);
    gwParentalControlDb.trustedDevices.currentDeviceNum++;

    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
    return 0;
}

/**************************************************************************/
/*! \fn int GwParentalControlDb_DelMac(Uint8 *buf, int bufLen)                                  
 **************************************************************************
 *  \brief delete trusted mac address.
 *  \param[in] buf - mac address
 *  \param[in] bufLen - buffer len
 *  \param[out] no output.
 *  \return OK or error status.
 **************************************************************************/
int GwParentalControlDb_DelMac(Uint8 *buf, int bufLen)
{
    Uint32 i;
    Uint32 devicesNum;
    Uint8* mac;
    trustedDeviceMacAddrRow_t* currentRow;
    trustedDeviceMacAddrRow_t* nextRow;
    Uint32 rowsNum;
    Uint32 lockKey;

    if ((!buf) || (bufLen != GW_PRCTL_MAC_ADDR_LEN))
    {
        return -1;
    }

    devicesNum = gwParentalControlDb.trustedDevices.currentDeviceNum;
    
    PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);

    for (i=0; i < devicesNum; i++)
    {
        mac = gwParentalControlDb.trustedDevices.deviceMacTable[i].mac.hw;

        if (memcmp(buf, mac, bufLen) == 0)
        {
            currentRow = &(gwParentalControlDb.trustedDevices.deviceMacTable[i]);
            
            memset(currentRow, 0, sizeof(trustedDeviceMacAddrRow_t));
            if ((i+1) < GW_PRCTL_DEVICE_MAX_NUM)
            {
                nextRow = &(gwParentalControlDb.trustedDevices.deviceMacTable[i+1]);
                rowsNum = (GW_PRCTL_DEVICE_MAX_NUM - i - 1);
                memmove(currentRow, nextRow, sizeof(trustedDeviceMacAddrRow_t)*rowsNum);
            }
            
            gwParentalControlDb.trustedDevices.currentDeviceNum--;
            PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
            return 0;
        }
    }
    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
    return 0;
}

/**************************************************************************/
/*! \fn int GwParentalControlDb_Print(void)                                     
 **************************************************************************
 *  \brief Print GW parental control data base.
 *  \param[in] no input.
 *  \param[out] no output.
 *  \return OK or error status.
 **************************************************************************/
int GwParentalControlDb_Print(void)
{
    Uint32 i;
    Uint8* buf;
    Uint32 devicesNum = gwParentalControlDb.trustedDevices.currentDeviceNum;
    Uint32 packet;
    Uint32 lockKey;

    PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);
           
    printk("*************************************************************************************\n");
    printk("*                             GW Parental Control DB                                *\n");
    printk("*************************************************************************************\n");
    printk("Droped Packets = %d\n",gwParentalControlDb.pcKeywordDropPackets);
    for (i=0; i<devicesNum; i++)
    {
        buf = gwParentalControlDb.trustedDevices.deviceMacTable[i].mac.hw;
        packet = gwParentalControlDb.trustedDevices.deviceMacTable[i].passthroughPackets;

        printk("\t%02X:%02X:%02X:%02X:%02X:%02X\tpassthrough packets = %d\n",buf[0],buf[1],buf[2],buf[3],buf[4],buf[5],packet);
    }   
    printk("\n");

    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
    return 0;
}

/**************************************************************************/
/*! \fn int GwParentalControlDb_FlushAllMac(void)                                     
 **************************************************************************
 *  \brief delete all trusted mac addresses from data base.
 *  \param[in] no input.
 *  \param[out] no output.
 *  \return OK or error status.
 **************************************************************************/
int GwParentalControlDb_FlushAllMac(void)
{   
    trustedDeviceMacAddrRow_t* deviceRow;
    Uint32 lockKey;

    PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);

    deviceRow = gwParentalControlDb.trustedDevices.deviceMacTable;

    memset(deviceRow, 0, sizeof(trustedDeviceMacAddrRow_t)*GW_PRCTL_DEVICE_MAX_NUM);
    gwParentalControlDb.trustedDevices.currentDeviceNum = 0;

    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
    return 0;
}

/**************************************************************************/
/*! \fn int GwCpeMacFilteringDb_AddMac(Uint8 *buf, int bufLen)                                   
 **************************************************************************
 *  \brief Adds a MAC address to the WAN filtering DB.
 *  \param[in] buf - mac address
 *  \param[in] bufLen - buffer len
 *  \param[out] no output.
 *  \return OK or error status.
 **************************************************************************/
int GwCpeMacFilteringDb_AddMac(Uint8 *buf, int bufLen)
{
    Uint32 index;
    Uint8 *mac;
    Uint32 lockKey;

    if ((!buf) || (bufLen != GW_PRCTL_MAC_ADDR_LEN))
    {
        return -1;
    }

    if (gwParentalControlDb.cpeMacFilteringDevices.currentDeviceNum >= GW_PRCTL_CPE_MAC_FILTER_DEV_MAX_NUM)
    {
        return -2;
    }
    
    PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);

    index = gwParentalControlDb.cpeMacFilteringDevices.currentDeviceNum;
    mac = gwParentalControlDb.cpeMacFilteringDevices.deviceMacTable[index].mac.hw;
    memcpy(mac, buf, bufLen);
    gwParentalControlDb.cpeMacFilteringDevices.currentDeviceNum++;

    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
    return 0;
}

/**************************************************************************/
/*! \fn int GwCpeMacFilteringDb_DelMac(Uint8 *buf, int bufLen)                                  
 **************************************************************************
 *  \brief Delete a MAC address from the WAN filtering DB.
 *  \param[in] buf - mac address
 *  \param[in] bufLen - buffer len
 *  \param[out] no output.
 *  \return OK or error status.
 **************************************************************************/
int GwCpeMacFilteringDb_DelMac(Uint8 *buf, int bufLen)
{
    Uint32 i;
    Uint32 devicesNum;
    Uint8* mac;
    filteredCpeMacAddrRow_t* currentRow;
    filteredCpeMacAddrRow_t* nextRow;
    Uint32 rowsNum;
    Uint32 lockKey;

    if ((!buf) || (bufLen != GW_PRCTL_MAC_ADDR_LEN))
    {
        return -1;
    }

    devicesNum = gwParentalControlDb.cpeMacFilteringDevices.currentDeviceNum;
    
    PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);

    for (i=0; i < devicesNum; i++)
    {
        mac = gwParentalControlDb.cpeMacFilteringDevices.deviceMacTable[i].mac.hw;

        if (memcmp(buf, mac, bufLen) == 0)
        {
            currentRow = &(gwParentalControlDb.cpeMacFilteringDevices.deviceMacTable[i]);
            
            memset(currentRow, 0, sizeof(filteredCpeMacAddrRow_t));
            if ((i+1) < GW_PRCTL_CPE_MAC_FILTER_DEV_MAX_NUM)
            {
                nextRow = &(gwParentalControlDb.cpeMacFilteringDevices.deviceMacTable[i+1]);
                rowsNum = (GW_PRCTL_DEVICE_MAX_NUM - i - 1);
                memmove(currentRow, nextRow, sizeof(filteredCpeMacAddrRow_t)*rowsNum);
            }
            
            gwParentalControlDb.cpeMacFilteringDevices.currentDeviceNum--;
            PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
            return 0;
        }
    }

    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
    return 0;
}

/**************************************************************************/
/*! \fn int GwCpeMacFilteringDb_FlushAllMac(void)                                     
 **************************************************************************
 *  \brief delete all mac addresses from the WAN filtering DB.
 *  \param[in] no input.
 *  \param[out] no output.
 *  \return OK or error status.
 **************************************************************************/
int GwCpeMacFilteringDb_FlushAllMac(void)
{   
    filteredCpeMacAddrRow_t* deviceRow;
    Uint32 lockKey;

    PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);

    deviceRow = gwParentalControlDb.cpeMacFilteringDevices.deviceMacTable;

    memset(deviceRow, 0, sizeof(filteredCpeMacAddrRow_t)*GW_PRCTL_CPE_MAC_FILTER_DEV_MAX_NUM);
    gwParentalControlDb.cpeMacFilteringDevices.currentDeviceNum = 0;

    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
    return 0;
}

/**************************************************************************/
/*! \fn int GwCpeMacFilteringDb_Print(void)                                     
 **************************************************************************
 *  \brief Print GW parental control data base.
 *  \param[in] no input.
 *  \param[out] no output.
 *  \return OK or error status.
 **************************************************************************/
int GwCpeMacFilteringDb_Print(void)
{
    Uint32 i;
    Uint8* buf;
    Uint32 devicesNum = gwParentalControlDb.cpeMacFilteringDevices.currentDeviceNum;
    Uint32 packet;
    Uint32 lockKey;

    printk("*************************************************************************************\n");
    printk("*                        GW Parental Control CPE MAC FILTERING                      *\n");
    printk("*************************************************************************************\n");
           
    PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);
    for (i=0; i<devicesNum; i++)
    {
        buf = gwParentalControlDb.cpeMacFilteringDevices.deviceMacTable[i].mac.hw;
        packet = gwParentalControlDb.cpeMacFilteringDevices.deviceMacTable[i].droppedPackets;
        printk("\t%02X:%02X:%02X:%02X:%02X:%02X\tdropped packets = %d\n",buf[0],buf[1],buf[2],buf[3],buf[4],buf[5],packet);
    }   

    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
    printk("\n");

    return 0;
}

/**************************************************************************/
/*      LOCAL FUNCTIONS Implementation:                                   */
/**************************************************************************/
#ifdef CONFIG_TI_GW_EGRESS_HOOK

/**************************************************************************/
/*! \fn int configureGwEgressHookHandler(Bool reg)                                    
 **************************************************************************
 *  \brief configure GW hook.
 *  \param[in] reg - register or deregister indication.
 *  \param[out] no output.
 *  \return OK or error status.
 **************************************************************************/
static int configureGwEgressHookHandler(Bool reg, Char* interface)
{
    struct net_device *netDev = NULL;

    {
        extern int ti_register_gw_egress_hook_handler (struct net_device* dev, int (*gw_egress_hook)(struct sk_buff *skb));
        extern int ti_deregister_gw_egress_hook_handler (struct net_device* dev);

        if (interface)
        {
            netDev = dev_get_by_name(&init_net, interface); // UNIHAN MOD
        }

        if(netDev)
        {
            if(reg)
            {
                if (ti_register_gw_egress_hook_handler(netDev, GwEgressHook) == 0)
                    printk (KERN_INFO "register ingress Hook on  %s \n",netDev->name);
                dev_put(netDev);
            }
            else
            {
                if (ti_deregister_gw_egress_hook_handler(netDev) == 0)
                    printk (KERN_INFO "deregister ingress Hook on  %s \n",netDev->name);
                dev_put(netDev);
                netDev = NULL;
            }
        }
        else
        {
            printk (KERN_ERR "warning: configure gw ingress hook fail no dev\n");
        }
    }

    return 0;
}

/**************************************************************************/
/*! \fn int GwEgressHook(struct sk_buff *skb)
 **************************************************************************
 *  \brief  GW egress hook implementaion
 *  \param[in] SKB.
 *  \return 
 **************************************************************************/
static int GwEgressHook(struct sk_buff* skb)
{
    Bool packetDropped = False;

    packetDropped = GwApplyKeywordFiltering(skb);

    if (packetDropped)
    {
        return -1;
    }

    packetDropped = GwApplyCpeMacFiltering(skb);

    if (packetDropped)
    {
        return -1;
    }

    return 0;
}

/**************************************************************************/
/*! \fn Bool GwApplyCpeMacFiltering(struct sk_buff* skb)
 **************************************************************************
 *  \brief  GW egress hook - CPE MAC filtering implementation
 *  \param[in] SKB.
 *  \return True/False
 **************************************************************************/
static Bool GwApplyCpeMacFiltering(struct sk_buff* skb)
{
    struct ethhdr* ptr_ethhdr = NULL;
    Uint32 i;
    Bool packetDropped = False;
    Bool packetMatchFilteredMac = False;
    Uint32 devicesNum;
    filteredCpeMacAddrRow_t* macRow;
    Uint32 lockKey;

    if ((skb->ti_gw_meta) & (PARENTAL_CONTROL_DEV_HOOK_CPE_MAC_FILT))
    {
        PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);
    
        /*looking for trusted device mac address*/
        if (gwParentalControlDb.cpeMacFilteringDevices.cpeMacFilteringEnable)
        {
            ptr_ethhdr = eth_hdr(skb); // UNIHAN MOD
            if (ptr_ethhdr)
            {
                devicesNum = gwParentalControlDb.cpeMacFilteringDevices.currentDeviceNum;
                for (i=0; i < devicesNum; i++)
                {   
                    macRow = &(gwParentalControlDb.cpeMacFilteringDevices.deviceMacTable[i]);       
                    if (memcmp(macRow->mac.hw, ptr_ethhdr->h_dest, GW_PRCTL_MAC_ADDR_LEN) == 0)
                    {
                        packetMatchFilteredMac = True;
                        break;
                    }
                }
            }
        }
    
        /*droping packet*/
        if (packetMatchFilteredMac)
        {
            macRow->droppedPackets++;
        }

        PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);

        if (packetMatchFilteredMac)
        {
            dev_kfree_skb_any(skb);
            packetDropped = True;
        }
    }

    return packetDropped;
}

/**************************************************************************/
/*! \fn Bool GwApplyKeywordFiltering(struct sk_buff* skb)
 **************************************************************************
 *  \brief  GW egress hook - KeyWord filtering implementation
 *  \param[in] SKB.
 *  \return True/False
 **************************************************************************/
static Bool GwApplyKeywordFiltering(struct sk_buff* skb)
{
    struct ethhdr* ptr_ethhdr = NULL;
    Uint32 i;
    Bool foundTrustedDevice = False;
    Uint32 devicesNum;
    trustedDeviceMacAddrRow_t* macRow;
    Uint32 lockKey;
    Bool packetDropped = False;

    /*check does the packet should be dropped*/
    if ((skb->ti_gw_meta) & (PARENTAL_CONTROL_DEV_HOOK_KEYWORD_FILT))
    {
        PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);
    
        /*looking for trusted device mac address*/
        if (gwParentalControlDb.trustedDevices.trustedDevicePcEnable)
        {
            ptr_ethhdr = eth_hdr(skb); // UNIHAN MOD
            if (ptr_ethhdr)
            {
                devicesNum = gwParentalControlDb.trustedDevices.currentDeviceNum;
                for (i=0; i < devicesNum; i++)
                {   
                    macRow = &(gwParentalControlDb.trustedDevices.deviceMacTable[i]);       
                    if (memcmp(macRow->mac.hw, ptr_ethhdr->h_dest, GW_PRCTL_MAC_ADDR_LEN) == 0)
                    {
                        foundTrustedDevice = True;
                        break;
                    }
                }
            }
        }
    
        /*passthrough packet*/
        if (foundTrustedDevice)
        {
            macRow->passthroughPackets++;
        }
        else
        {
            gwParentalControlDb.pcKeywordDropPackets++;
        }
    
        PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);

        /*droping packet*/
        if (!foundTrustedDevice)
        {
            dev_kfree_skb_any(skb);
            packetDropped = True;
        }
    }

    return packetDropped;
}

#endif /*CONFIG_TI_GW_EGRESS_HOOK*/
