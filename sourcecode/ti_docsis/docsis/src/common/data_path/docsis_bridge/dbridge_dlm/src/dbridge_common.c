/*
 *
 * dbridge_common.c
 * Description:
 * DOCSIS bridge Common implementation
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

#define _DBRIDGE_COMMON_C_

/*! \file dbridge_common.c
    \brief the Docsis Bridge common functions support
*/

/**************************************************************************/
/*      INCLUDES:                                                         */
/**************************************************************************/
#include <linux/kernel.h>
// #include <linux/module.h>
// #include <linux/list.h>
#include "dfltr_class_utils.h"
#include "dbridge_db.h"
#include "dbridge_common.h"
#include "dbridge_l2vpn_ds_db.h"
#include "dbridge_netdev.h"

/**************************************************************************/
/*      EXTERNS Declaration:                                              */
/**************************************************************************/
int (*DbridgeCommon_GetSysIfIndexByDocsisCB)(Uint32 docsisIfIndex, Uint32 *sysIfIndex) = NULL;
int (*DbridgeCommon_GetDocsisIfIndexBySysIndexCB)(Uint32 sysIfIndex, Uint32 *docsisIfIndex) = NULL;
#ifdef DBRIDGE_LOG
extern unsigned int dbridgeDbgLevel;
#endif

/**************************************************************************/
/*      DEFINES:                                                          */
/**************************************************************************/

/**************************************************************************/
/*      LOCAL DECLARATIONS:                                               */
/**************************************************************************/
 
/**************************************************************************/
/*      LOCAL VARIABLES:                                                  */
/**************************************************************************/
struct net_device *gCni0Netdev = 0;
EXPORT_SYMBOL(gCni0Netdev);

/**************************************************************************/
/*      LOCAL FUNCTIONS:                                                  */
/**************************************************************************/

/**************************************************************************/
/*      INTERFACE FUNCTIONS implementation:                               */
/**************************************************************************/

/**************************************************************************/
/*! \fn int DbridgeCommon_TransDocsisCmimToLinuxCmim(Uint32 docsisCmim, Uint32* linuxCmim)
 **************************************************************************
 *  \brief Translate the Docsis CMIM to Linux CMIM.
 *  \param[in] docsisCmim - the Docsis CMIM value.
 *  \param[out] linuxCmim - the Linux CMIM value to be returned.
 *  \return OK or error status.
 **************************************************************************/
int DbridgeCommon_TransDocsisCmimToLinuxCmim(Uint32 docsisCmim, unsigned long long* linuxCmim)
{
	Uint32 sysIfIndex = 0;
	Uint32 i = 0;
    *linuxCmim = 0;
	/* translate the CMIM to system interface numbering */
	for (i = DOCSIS_MIN_IF_INDEX; i <= DOCSIS_MAX_IF_CMIM_INDEX; i++) 
	{
		if (BIT_SHIFT_INVERT(i) & docsisCmim) 
		{
            /* if callback is NULL */
            if (DbridgeCommon_GetSysIfIndexByDocsisCB == NULL)
            {
				printk(KERN_ERR "DbridgeCommon_GetSysIfIndexByDocsisCB is NULL\n");
				return -1;
            }

			/* translate interface index from Docsis numbering to Kernel numbering */
			if (DbridgeCommon_GetSysIfIndexByDocsisCB(i, &sysIfIndex) == -1) 
			{
				printk(KERN_ERR "Failed to translate Docsis interface index %u to system one\n", i);
				return -1;
			} 
// ARRIS Add Start
#ifdef INCLUDE_SERCOMM_GW         
            // The bit Position of 1 in the docsis CMIM is for primary CPE interface
            // So if this bit is set in docsis CMIM we also have to add the linux mask for lbr0 because this is also primary
            // CPE interface. By doing this we wont drop the packets destined to Gateway Interface(When Bsod is enabled).  
            if (1 == i) 
            {
             *linuxCmim |= DbridgeDb_Getlbr0InterfaceMask();                     
              printk(KERN_INFO "Adding lbr0 interface mask to linuxCmim for bit position in Docsis CMIM \n");
            }
#endif
// ARRIS Add End
            /* set the Linux kernel interface bit in the bitmask */
            SET_BIT_MASK_64(*linuxCmim, (sysIfIndex - 1));
        }
	}
    return 0;
}

/**************************************************************************/
/*! \fn int DbridgeCommon_TransLinuxCmimToDocsisCmim(Uint32 linuxCmim, Uint32* docsisCmim)
 **************************************************************************
 *  \brief Translate the Linux CMIM to Docsis CMIM.
 *  \param[in] linuxCmim - the linux CMIM value.
 *  \param[out] docsisCmim - the docsis CMIM value to be returned.
 *  \return OK or error status.
 **************************************************************************/
int DbridgeCommon_TransLinuxCmimToDocsisCmim(unsigned long long linuxCmim, Uint32* docsisCmim)
{
	Uint32 docsisIfIndex = 0;
	Uint32 i = 0;
// ARRIS Add Start
#ifdef INCLUDE_SERCOMM_GW 
     // Removing the lbr0 Interface mask from linux CMIM
    // If we do not remove this mask DbridgeCommon_GetDocsisIfIndexBySysIndexCB will not complete the translation
     linuxCmim &= (~DbridgeDb_Getlbr0InterfaceMask());
#endif
// ARRIS Add End
    *docsisCmim = 0;
	/* translate the CMIM to system interface numbering */
	for (i = LINUX_MIN_IF_INDEX; i <= LINUX_MAX_IF_INDEX; i++) 
	{
		if (BIT_SHIFT_64(i) & linuxCmim) 
		{
            /* if callback is NULL */
            if (DbridgeCommon_GetDocsisIfIndexBySysIndexCB == NULL)
            {
				printk(KERN_ERR "DbridgeCommon_GetDocsisIfIndexBySysIndexCB is NULL\n");
				return -1;
            }

			/* translate interface index from Kernel numbering to Docsis numbering */
			if (DbridgeCommon_GetDocsisIfIndexBySysIndexCB((i+1), &docsisIfIndex) == -1) 
			{
				printk(KERN_ERR "Failed to translate Docsis interface index %u to system one\n", i);
				return -1;
			}

            /* set the docsis interface bit in the bitmask */
            SET_BIT_MASK(*docsisCmim, (DOCSIS_MAX_IF_CMIM_INDEX - docsisIfIndex));
        }
	}

    return 0;
}

/**************************************************************************/
/*! \fn int DbridgeCommon_TransDocsisCmimToFloodMap(Uint32 docsisCmim, Uint32* floodMap)                                    
 **************************************************************************
 *  \brief Translate the Docsis CMIM to Flood Map (network device type numbering).
 *  \param[in] docsisCmim - the Docsis CMIM value.
 *  \param[out] floodMap - the Flood Map value to be returned.
 *  \param[out] no output.
 *  \return OK or error status.
 **************************************************************************/
int DbridgeCommon_TransDocsisCmimToFloodMap(Uint32 docsisCmim, Uint32* floodMap)
{
    *floodMap = 0;

    /* Primary interface and CMCI interface*/
    if(DbridgeDb_Erouter_GetMode())
    {
        /*SERCOMM ADD*/
        if ((IsPrimaryInterface(docsisCmim))||(IsCmciInterface(docsisCmim)))
            *floodMap |= DBR_CMCI_NET_DEV_TYPE;
        /*SERCOMM ADD END*/

        if (IsPrimaryInterface(docsisCmim))
            *floodMap |= DBR_ESAFE_NET_DEV_TYPE;
		if(IsCmciInterface(docsisCmim))
			*floodMap |= DBR_CMCI_NET_DEV_TYPE;

    }
    else
    {
        if ((IsPrimaryInterface(docsisCmim))||(IsCmciInterface(docsisCmim)))
            *floodMap |= DBR_CMCI_NET_DEV_TYPE;
    }

    /* CABLE interface */
    if (IsCableInterface(docsisCmim))
    {
        /* set the network device type bit in the bitmask */
        *floodMap |= DBR_CABLE_NET_DEV_TYPE;
    }

    /* eSafe interface */
    if (IsEsafeInterface(docsisCmim))
    {
        /* set the network device type bit in the bitmask */
        *floodMap |= DBR_ESAFE_NET_DEV_TYPE;
    }

    /* WAN-IP interface */
    if (IsWanIpInterface(docsisCmim))
    {
        /* set the network device type bit in the bitmask */
        *floodMap |= DBR_WAN_IP_NET_DEV_TYPE;
    }

    return 0;
}

/**************************************************************************/
/*! \fn int DbridgeCommon_register_GetSysIfIndexByDocsisCB(int (*Dbridge_GetSysIfIndexByDocsis)(Uint32 docsisIfIndex, Uint32 *sysIfIndex))
 **************************************************************************
 *  \brief Register the L2VPN DS data callback function
 *  \param[in] - the L2VPN DS callback function.
 *  \return OK or error status.
 **************************************************************************/
int DbridgeCommon_register_GetSysIfIndexByDocsisCB(int (*Dbridge_GetSysIfIndexByDocsis)(Uint32 docsisIfIndex, Uint32 *sysIfIndex))
{   
    unsigned long flags;

    local_irq_save(flags);
    DbridgeCommon_GetSysIfIndexByDocsisCB = Dbridge_GetSysIfIndexByDocsis;
    local_irq_restore(flags);

    return 0;
}
EXPORT_SYMBOL(DbridgeCommon_register_GetSysIfIndexByDocsisCB); 

/**************************************************************************/
/*! \fn int DbridgeCommon_register_GetDocsisIfIndexBySysIndexCB
 **************************************************************************
 *  \brief register CB for converting linux ifIndex to docsis ifIndex
 *  \param[in] - CB
 *  \return OK or error status.
 **************************************************************************/
int DbridgeCommon_register_GetDocsisIfIndexBySysIndexCB(int (*Dbridge_GetDocsisIfIndexBySysIndex)(Uint32 sysIfIndex, Uint32 *docsisIfIndex))
{   
    unsigned long flags;

    local_irq_save(flags);
    DbridgeCommon_GetDocsisIfIndexBySysIndexCB = Dbridge_GetDocsisIfIndexBySysIndex;
    local_irq_restore(flags);

    return 0;
}
EXPORT_SYMBOL(DbridgeCommon_register_GetDocsisIfIndexBySysIndexCB); 

/**************************************************************************/
/*! \fn Bool DbridgeCommon_ReceiveFilterL2VPNorDUT(struct sk_buff *skb, unsigned int outDev)
 **************************************************************************
 *  \brief Filters L2VPN DS related or DUT traffic coming from CNI
 *  \param[in] skb - skb pointer
 *  \param[in] outDev - The output network device type (CMCI, ESAFE or WAN)
 *  \return True if need to drop the packet
 **************************************************************************/
Bool DbridgeCommon_ReceiveFilterL2VPNorDUT(struct sk_buff *skb, unsigned int outDev)
{
    Bool dropPkt = False;
    unsigned long long linuxCmim;
    unsigned int floodMap;

    /* If unencrypted packet & DUT enabled, filter according to DUT CMIM */
    if (!(skb->ti_meta_info & CNID_ENCRYPTED))
    {
        /* This is an unencrypted packet - check if DUT is enabled */
        Bool dutConfigured;
        
        DbridgeL2vpnDb_GetDutConfigured(&dutConfigured);  

        if (dutConfigured)
        { 
            DbridgeL2vpnDb_GetDutBitmaps(&linuxCmim, &floodMap);
            
            if (outDev == DBR_CMCI_NET_DEV_TYPE)
            {
                skb->ti_selective_fwd_dev_info &= linuxCmim;
                if (skb->ti_selective_fwd_dev_info == 0)
                {
                    /* Drop the packet as this interface is disabled by DUT CMIM */
#ifdef DBRIDGE_LOG
                    if(dbridgeDbgLevel & DBRIDGE_DBG_L2VPN_DS) 
                    {
                        printk(KERN_INFO "Drop the packet as this device(0x%x) is disabled by DUT CMIM\n", outDev);
                    }
#endif
                    dropPkt = True;
                }
            }
            else
            {
                if ((floodMap & outDev) == 0)
                {
                    /* Drop the packet as this interface is disabled by DUT CMIM */
#ifdef DBRIDGE_LOG
                    if(dbridgeDbgLevel & DBRIDGE_DBG_L2VPN_DS) 
                    {
                        printk(KERN_INFO "Drop the packet as this device(0x%x) is disabled by DUT CMIM\n", outDev);
                    }
#endif
                    dropPkt = True;
                }
            }
        }
    }
    /* If Unicast L2VPN related packet */
    else if ((skb->ti_meta_info & CNID_L2VPN_RELATED) && !(skb->ti_meta_info & CNID_MULTICAST))
    {
        unsigned int saidIndex = skb->ti_meta_info2 & CNID_L2VPN_RELATED_SAID_MASK;
        DbridgeL2vpnDb_GetSaidBitmaps(saidIndex, &linuxCmim, &floodMap);
    
        if (outDev == DBR_CMCI_NET_DEV_TYPE)
        {
            skb->ti_selective_fwd_dev_info &= linuxCmim;
            if (skb->ti_selective_fwd_dev_info == 0)
            {
                /* Drop the packet as this interface is disabled by DUT CMIM */
#ifdef DBRIDGE_LOG
                if(dbridgeDbgLevel & DBRIDGE_DBG_L2VPN_DS) 
                {
                    printk(KERN_INFO "Drop the packet as this device(0x%x) is disabled by L2VPN CMIM\n", outDev);
                }
#endif
                dropPkt = True;
            }
        }
        else
        {
            if ((floodMap & outDev) == 0)
            {
                /* Drop the packet as this interface is disabled by DUT CMIM */
#ifdef DBRIDGE_LOG
                if(dbridgeDbgLevel & DBRIDGE_DBG_L2VPN_DS) 
                {
                    printk(KERN_INFO "Drop the packet as this device(0x%x) is disabled by L2VPN CMIM\n", outDev);
                }
#endif
                dropPkt = True;
            }
        }
    }
    /* For Multicast L2VPN the floodMap and CMIM are already correct here 
       since it was done earlier by DBridge_Multicast_L2VPN_handler */
    
    if (dropPkt)
    {
        DbridgeNetDev_t* priv;
        priv = netdev_priv(skb->dev);
        priv->stats.rx_dropped++;
        kfree_skb (skb);
        return True;
    }
    return False;
}
