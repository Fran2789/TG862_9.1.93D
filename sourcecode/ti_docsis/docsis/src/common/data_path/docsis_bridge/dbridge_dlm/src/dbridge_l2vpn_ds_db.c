/*
 *
 * dbridge_l2vpn_ds_db.c
 * Description:
 * DOCSIS bridge L2VPN DS Forwarding database implementation
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

#define _DBRIDGE_L2VPN_DS_DB_C_

/*! \file dbridge_l2vpn_ds_db.c
    \brief the Docsis Bridge L2VPN DS Forwarding data base support
*/

/**************************************************************************/
/*      INCLUDES:                                                         */
/**************************************************************************/
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/list.h>
#include "dbridge_l2vpn_ds_db.h"
#include "dbridge_l2vpn_ds_ctl.h"
#include "dfltr_class_utils.h"
#include "dbridge_db.h"
#include "dbridge_common.h"

/**************************************************************************/
/*      EXTERNS Declaration:                                              */
/**************************************************************************/
 
/**************************************************************************/
/*      DEFINES:                                                          */
/**************************************************************************/

#define DBRIDGE_L2VPN_DS_DB_DEBUG

#ifdef DBRIDGE_L2VPN_DS_DB_DEBUG
/* note: prints function name for you */
#ifdef DBRIDGE_LOG
#  define DPRINTK(fmt, args...) \
        if(dbridgeDbgLevel & DBRIDGE_DBG_L2VPN_DS) printk(KERN_INFO "%s: " fmt, __FUNCTION__ , ## args)
#else  // DBRIDGE_LOG
#  define DPRINTK(fmt, args...)
#endif // DBRIDGE_LOG
#else  // DBRIDGE_L2VPN_DS_DB_DEBUG
#  define DPRINTK(fmt, args...)
#endif // DBRIDGE_L2VPN_DS_DB_DEBUG

#define DBRIDGE_L2VPN_DS_MAX_NUM_OF_SAID 24

/*! \var typedef struct DbridgeL2vpnDsDb DbridgeL2vpnDsDb_t
    \brief Structure contains information about the DOCSIS bridge L2VPN DS Data Base.
*/
typedef struct DbridgeL2vpnDsDb
{
    Bool   l2vpnRelated;
    unsigned long long linuxCmim;
    Uint32 floodMap;
}DbridgeL2vpnDsDb_t;

/*! \var typedef struct DbridgeL2vpnDsDutDb DbridgeL2vpnDsDutDb_t
    \brief Structure contains information about the DOCSIS bridge L2VPN DS DUT Data Base.
*/
typedef struct DbridgeL2vpnDsDutDb
{
    Bool   isConfigured;
    unsigned long long linuxCmim;
    Uint32 floodMap;
}DbridgeL2vpnDsDutDb_t;

/**************************************************************************/
/*      LOCAL DECLARATIONS:                                               */
/**************************************************************************/

 
/**************************************************************************/
/*      LOCAL VARIABLES:                                                  */
/**************************************************************************/

/* structure which keeps track of all the information for the DOCSIS Bridge L2VPN DS */
static DbridgeL2vpnDsDb_t    dbridge_l2vpn_ds_db[DBRIDGE_L2VPN_DS_MAX_NUM_OF_SAID];
static DbridgeL2vpnDsDutDb_t dbridge_l2vpn_ds_dut_db;

/**************************************************************************/
/*      INTERFACE FUNCTIONS Implementation:                               */
/**************************************************************************/

/**************************************************************************/
/*! \fn int DbridgeL2vpnDsDb_Init(void)                                     
 **************************************************************************
 *  \brief Docsis bridge L2VPN DS data base initialization.
 *  \param[in] no input.
 *  \param[out] no output.
 *  \return OK or error status.
 **************************************************************************/
int DbridgeL2vpnDsDb_Init(void)
{
	memset((void *)&dbridge_l2vpn_ds_db, 0, sizeof(DbridgeL2vpnDsDb_t) * DBRIDGE_L2VPN_DS_MAX_NUM_OF_SAID);
    dbridge_l2vpn_ds_dut_db.isConfigured = False;

    DPRINTK(KERN_INFO"\nInit Dbridge L2VPN DS DB done\n");
    
	return 0;
}


/**************************************************************************/
/*! \fn int DbridgeL2vpnDsDb_SetSaid(Uint32 saidIndex, Uint32 docsisCmim)                                   
 **************************************************************************
 *  \brief Set Docsis Bridge L2VPN related SAID with its CMIM.
 *  \param[in] saidIndex - SAID index
 *  \param[in] docsisCmim - interface mask
 *  \param[out] no output.
 *  \return OK or error status.
 **************************************************************************/
int DbridgeL2vpnDsDb_SetSaid(Uint32 saidIndex, Uint32 docsisCmim)
{
    unsigned long long linuxCmim = 0;
    Uint32 floodMap = 0;

    if (saidIndex >= DBRIDGE_L2VPN_DS_MAX_NUM_OF_SAID)
    {
        printk(KERN_ERR "%s: saidIndex %d out of range (max value is %d)\n", __FUNCTION__, saidIndex, DBRIDGE_L2VPN_DS_MAX_NUM_OF_SAID);
        return -1;
    }

    DPRINTK(KERN_INFO "Set L2VPN DS SAID %d DOCSIS CMIM 0x%x\n", saidIndex, docsisCmim);

    /* translate the LINUX CMIM to Flood Map */
    if (DbridgeCommon_TransDocsisCmimToLinuxCmim(docsisCmim, &linuxCmim) < 0)
    {
        printk(KERN_ERR "%s: Call to DbridgeCommon_TransDocsisCmimToLinuxCmim failed\n", __FUNCTION__);
        return -2;
    }

    /* translate the DOCSIS CMIM to Flood Map */
    if (DbridgeCommon_TransDocsisCmimToFloodMap(docsisCmim, &floodMap) < 0)
    {
        printk(KERN_ERR "%s: Call to DbridgeCommon_TransDocsisCmimToFloodMap failed\n", __FUNCTION__);
        return - 3;
    }

    dbridge_l2vpn_ds_db[saidIndex].l2vpnRelated = True;
    dbridge_l2vpn_ds_db[saidIndex].linuxCmim = linuxCmim;
    dbridge_l2vpn_ds_db[saidIndex].floodMap = floodMap;
    
    DPRINTK(KERN_INFO "Set L2VPN DS SAID %u, flood map 0x%x, LINUX CMIM 0x%llx\n", saidIndex, floodMap, linuxCmim);

    return 0;
}

/**************************************************************************/
/*! \fn int DbridgeL2vpnDsDb_DelSaid(Uint32 saidIndex)                                   
 **************************************************************************
 *  \brief Delete Docsis Bridge L2VPN related SAID
 *  \param[in] saidIndex - SAID index
 *  \param[out] no output.
 *  \return OK or error status.
 **************************************************************************/
int DbridgeL2vpnDsDb_DelSaid(Uint32 saidIndex)
{
    if (saidIndex >= DBRIDGE_L2VPN_DS_MAX_NUM_OF_SAID)
    {
        printk(KERN_ERR "%s: saidIndex %d out of range (max value is %d)\n", __FUNCTION__, saidIndex, DBRIDGE_L2VPN_DS_MAX_NUM_OF_SAID);
        return -1;
    }

    DPRINTK(KERN_INFO "Delete L2VPN DS SAID %d\n", saidIndex);

    dbridge_l2vpn_ds_db[saidIndex].l2vpnRelated = False;

    return 0;
}


/**************************************************************************/
/*! \fn int DbridgeL2vpnDsDb_SetDut(Bool isConfigured, Uint32 docsisCmim)                                   
 **************************************************************************
 *  \brief Set Docsis Bridge L2VPN DUT with its CMIM.
 *  \param[in] isConfigured - True-configured, False-not configured
 *  \param[in] docsisCmim - interface mask
 *  \param[out] no output.
 *  \return OK or error status.
 **************************************************************************/
int DbridgeL2vpnDsDb_SetDut(Bool isConfigured, Uint32 docsisCmim)
{
    unsigned long long linuxCmim = 0;
    Uint32 floodMap = 0;

    DPRINTK(KERN_INFO "Set L2VPN DS DUT isConfigured %d DOCSIS CMIM 0x%x\n", isConfigured, docsisCmim);

    dbridge_l2vpn_ds_dut_db.isConfigured = isConfigured;
    
    if (isConfigured)
    {
        /* translate the DOCSIS CMIM to LINUX CMIM */
        if (DbridgeCommon_TransDocsisCmimToLinuxCmim(docsisCmim, &linuxCmim) < 0)
        {
            printk(KERN_ERR "%s: Call to DbridgeCommon_TransDocsisCmimToLinuxCmimTransDocsisCmimToLinuxCmim failed\n", __FUNCTION__);
            return -1;
        }

        /* translate the DOCSIS CMIM to Flood Map */
        if (DbridgeCommon_TransDocsisCmimToFloodMap(docsisCmim, &floodMap) < 0)
        {
            printk(KERN_ERR "%s: Call to DbridgeCommon_TransDocsisCmimToFloodMap failed\n", __FUNCTION__);
            return - 2;
        }
        
        dbridge_l2vpn_ds_dut_db.linuxCmim = linuxCmim;
        dbridge_l2vpn_ds_dut_db.floodMap = floodMap;
    }

    return 0;
}

/**************************************************************************/
/*! \fn int DbridgeL2vpnDsDb_Print(void)                                     
 **************************************************************************
 *  \brief Print Docsis bridge L2VPN data base.
 *  \param[in] no input.
 *  \param[out] no output.
 *  \return OK or error status.
 **************************************************************************/
int DbridgeL2vpnDsDb_Print(void)
{
    Uint32 i;
    
    printk("*************************************************************************************\n");
    printk("*                             DOCSIS Bridge L2VPN DS DB                             *\n");
    printk("*************************************************************************************\n");

    for (i = 0; i < DBRIDGE_L2VPN_DS_MAX_NUM_OF_SAID; i++)
    {
        if (dbridge_l2vpn_ds_db[i].l2vpnRelated)
        {
            printk("SAID = %d, LINUX CMIM=0x%llx, flood map=0x%x\n", i, 
                dbridge_l2vpn_ds_db[i].linuxCmim, dbridge_l2vpn_ds_db[i].floodMap);
        }
    }

    if (dbridge_l2vpn_ds_dut_db.isConfigured)
    {
        printk("DUT Configured. LINUX CMIM=0x%llx, flood map=0x%x\n", 
                dbridge_l2vpn_ds_dut_db.linuxCmim, dbridge_l2vpn_ds_dut_db.floodMap);
    }

    printk("*************************************************************************************\n");
	printk("\n");

    return 0;
}



/**************************************************************************/
/*! \fn int DbridgeL2vpnDb_GetSaidBitmaps(Uint32 saidIndex, Uint32* linuxCmim, Uint32* floodMap)                                     
 **************************************************************************
 *  \brief Get the CMIM and flood map bitmaps according to the SAID index.
 *  \param[in] saidIndex - SAID index value.
 *  \param[out] linuxCmim - Linux CMIM value to be returned.
 *  \param[out] floodMap - Flood Map value to be returned.
 *  \return OK or error status.
 **************************************************************************/
int DbridgeL2vpnDb_GetSaidBitmaps(Uint32 saidIndex, unsigned long long* linuxCmim, Uint32* floodMap)
{

    if (saidIndex >= DBRIDGE_L2VPN_DS_MAX_NUM_OF_SAID)
    {
        printk(KERN_ERR "%s: saidIndex %d out of range (max value is %d)\n", __FUNCTION__, saidIndex, DBRIDGE_L2VPN_DS_MAX_NUM_OF_SAID);
        return -1;
    }
    
    if (linuxCmim == NULL || floodMap == NULL)
    {
        printk(KERN_ERR "%s: Got NULL output parameter\n", __FUNCTION__);
        return -2;
    }
    
    if (False == dbridge_l2vpn_ds_db[saidIndex].l2vpnRelated)
    {
        printk(KERN_ERR "%s: Request of SAID bitmaps for none L2VPN SAID index %d\n", __FUNCTION__, saidIndex);
        return -3;
    }
    

    *linuxCmim = dbridge_l2vpn_ds_db[saidIndex].linuxCmim;
    *floodMap = dbridge_l2vpn_ds_db[saidIndex].floodMap;

    DPRINTK(KERN_INFO"\nGot L2VPN Linux CMIM 0x%llx and Flood map 0x%x for SAID index %u\n", *linuxCmim, *floodMap, saidIndex);
    
    return 0;
}

/**************************************************************************/
/*! \fn int DbridgeL2vpnDb_GetDutConfigured(Bool *dutConfigured)
 **************************************************************************
 *  \brief Get DUT configured or not
 *  \param[out] dutConfigured - True=DUT configured, False=DUT not configured
 *  \return OK or error status.
 **************************************************************************/
int DbridgeL2vpnDb_GetDutConfigured(Bool *dutConfigured)
{
    if (dutConfigured == NULL)
    {
        printk(KERN_ERR "%s: Got NULL output parameter\n", __FUNCTION__);
        return -1;
    }
    
    *dutConfigured = dbridge_l2vpn_ds_dut_db.isConfigured;

    return 0;
}


/**************************************************************************/
/*! \fn int DbridgeL2vpnDb_GetDutBitmaps(Bool *dutConfigured, Uint32* linuxCmim, Uint32* floodMap)
 **************************************************************************
 *  \brief Get the CMIM and flood map bitmaps according for DUT
 *  \param[out] linuxCmim - Linux CMIM value to be returned.
 *  \param[out] floodMap - Flood Map value to be returned.
 *  \return OK or error status.
 **************************************************************************/
int DbridgeL2vpnDb_GetDutBitmaps(unsigned long long* linuxCmim, Uint32* floodMap)
{
    if (linuxCmim == NULL || floodMap == NULL)
    {
        printk(KERN_ERR "%s: Got NULL output parameter\n", __FUNCTION__);
        return -1;
    }
    if (False == dbridge_l2vpn_ds_dut_db.isConfigured)
    {
        printk(KERN_ERR "%s: Request of unconfigured DUT bitmaps\n", __FUNCTION__);
        return -2;
    }

    *linuxCmim = dbridge_l2vpn_ds_dut_db.linuxCmim;
    *floodMap = dbridge_l2vpn_ds_dut_db.floodMap;

    DPRINTK(KERN_INFO"\nGot L2VPN Linux CMIM 0x%llx and Flood map 0x%x for DUT\n", *linuxCmim, *floodMap);
    
    return 0;
}


