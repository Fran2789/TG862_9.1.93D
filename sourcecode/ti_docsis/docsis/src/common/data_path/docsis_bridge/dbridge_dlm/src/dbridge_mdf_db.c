/*
 *
 * dbridge_mdf_db.c
 * Description:
 * DOCSIS bridge Multicast DSID Forwarding database implementation
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

#define _DBRIDGE_MDF_DB_C_

/*! \file dbridge_mdf_db.c
    \brief the Docsis Bridge Multicast DSID Forwarding data base support
*/

/**************************************************************************/
/*      INCLUDES:                                                         */
/**************************************************************************/
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/list.h>
#include "dbridge_mdf_db.h"
#include "dbridge_mdf_ctl.h"
#include "dfltr_class_utils.h"
#include "dbridge_db.h"
#include "pal.h"
#include "dbridge_common.h"


/**************************************************************************/
/*      DEFINES:                                                          */
/**************************************************************************/

#define DBRIDGE_MDF_DB_DEBUG

#ifdef DBRIDGE_MDF_DB_DEBUG
/* note: prints function name for you */
#  define DPRINTK(fmt, args...) printk(KERN_INFO "%s: " fmt, __FUNCTION__ , ## args)
#else
#  define DPRINTK(fmt, args...)
#endif

/* Multicast Client MAC Address Action defines */
#define DBRIDGE_MDF_CLIENT_MAC_ADDR_ACTION_ADD     0
#define DBRIDGE_MDF_CLIENT_MAC_ADDR_ACTION_DELETE  1

#define DBR_MAX_NET_DEV_TYPES_ENTRY 8
#define DBR_MCAST_LINUX_CMIM_IP_STACK 1

#ifndef DOCS_CONF_CODE_SUCCESS
#define DOCS_CONF_CODE_SUCCESS      0
#endif

#ifndef DOCS_CONF_CODE_UNKNOWN_CLIENT_MAC_ADDR
#define DOCS_CONF_CODE_UNKNOWN_CLIENT_MAC_ADDR      42
#endif

/*! \var typedef struct DbridgeMcastClientsEntry DbridgeMcastClientsEntry_t
    \brief Structure defines the format of the Multicast Client MAC Address linked list entry. 
*/
typedef struct DbridgeMcastClientsEntry 
{
    struct list_head list;

    /* DOCSIS Bridge ALT entry pointer on which the dev interface index can be found */
    DbridgeAltEntry_t* dbridgeAltEntry;

}DbridgeMcastClientsEntry_t;


/*! \var typedef struct DbridgeDmtEntry DbridgeDmtEntry_t
    \brief Structure defines the format of the DSID mapping table entry. 
     The maaping is from DSID index to Linux interfaces.
*/
typedef struct DbridgeDmtEntry 
{
    /* inducates if this entry is occupied */
    Bool isDsidIndexValid;  

    /************************/
    /*     Flood bitmap     */
    /************************/
    /* network device types bitmap for mapping the destanation index the packet is send to */
    /* a bit mask representing the docsis interface type - CPE eSAFE CABLE WAN IP or LAN IP */
    Uint32 mcastFloodMap;

    /* Translation of the CMIM interfaces indexed (in DOCSIS numeration) bitmap into 
       network device types bitmap */
    Uint32 mcastDocsisCmimToFloodMap;

    /* network device types bitmap of all the Multicast Client MAC Addresses */
    Uint32 mcastClientsFloodMap;   

    /************************/
    /*  Linux CMIM bitmap   */
    /************************/
    /* CMIM interfaces indexed (in Linux Kernel numeration) bitmap */
    /* a bit mask representing the Linux interfaces of the CM to which the
      CM is to forward multicast traffic associated with the DSID*/
    unsigned long long mcastLinuxCmim;   

    /* Translation of the CMIM interfaces indexed (in DOCSIS numeration) bitmap into
       CMIM interfaces indexed (in Linux Kernel numeration) bitmap */
    unsigned long long mcastDocsisToLinuxCmim;  

    /* CMIM interfaces indexed (in Linux Kernel numeration) bitmap of all the Multicast Client MAC Addresses */
    unsigned long long mcastClientsLinuxCmim;   

    /************************/
    /*   Ref count arrays   */
    /************************/
    /* reference count for all network device types */
    Uint32 ifNetDevTypeRefCount[DBR_MAX_NET_DEV_TYPES_ENTRY]; 

    /* reference count for all Linux interfaces */
    Uint32 ifDevIndexRefCount[DBR_MAX_ALL_INTERFACE]; 

    /************************/
    /*  Mcast Client List   */
    /************************/
    /* number of current Multicast Client MAC Addresses in the mcastClientsList */
    Uint32 mcastClientsNum;

    /* linked list which representing all the Multicast Client MAC Addresses */
    struct list_head mcastClientsList;

}DbridgeDmtEntry_t;


/*! \var typedef struct DbridgeMdfDataBase DbridgeMdfDataBase_t
    \brief Structure contains information about the DOCSIS bridge MDF Data Base.
*/
typedef struct DbridgeMdfDataBase
{
    /* Docsis confirmation/error code */
    Uint8 mdfConfCode;

    /* Docsis bride MDF mode */
    Uint32 mdfMode;

    /* current DSID indexes configured in the dmtTable table */
    Uint32 dmtCurrDsidNum;

    /* Mapping DSID index table which contains all the corresponding Linux interfaces */
    DbridgeDmtEntry_t dmtTable[DBR_MDF_MAX_DSIDS];

}DbridgeMdfDataBase_t;


/**************************************************************************/
/*      LOCAL DECLARATIONS:                                               */
/**************************************************************************/
static int AddPreRegDsid(Uint32 dsidIndex);
static int DeletePreRegDsid(Uint32 dsidIndex);

static int AddMcastDsid(Uint32 dsidIndex, DbridgeMcastEncodings_t* buf);
static int DeleteMcastDsid(Uint32 dsidIndex);
static int UpdateMcastDsid(Uint32 dsidIndex, DbridgeMcastEncodings_t* buf);

static int UpdateMcastDocsisCmim(Uint32 dsidIndex, Uint32 mcastDocsisCmim);
static int AddMcastClientMacAddress(Uint32 dsidIndex, Uint8* clientMacAddress);
static int DeleteMcastClientMacAddress(Uint32 dsidIndex, Uint8* clientMacAddress);

static int CreateNewDbridgeMcastClientsEntry(DbridgeMcastClientsEntry_t **entry, DbridgeAltEntry_t* dbridgeAltEntry);
static int GetDbridgeMcastClientsEntryByMac(Uint32 dsidIndex, Uint8* clientMacAddress, DbridgeMcastClientsEntry_t **entry);
 
/**************************************************************************/
/*      LOCAL VARIABLES:                                                  */
/**************************************************************************/

/*  structure which keeps track of all the information for the DOCSIS Bridge MDF. */
static DbridgeMdfDataBase_t dbridge_mdf_db;

/**************************************************************************/
/*      INTERFACE FUNCTIONS Implementation:                               */
/**************************************************************************/

/**************************************************************************/
/*! \fn int DbridgeMdfDb_Init(void)                                     
 **************************************************************************
 *  \brief Docsis bridge MDF data base initialization.
 *  \param[in] no input.
 *  \param[out] no output.
 *  \return OK or error status.
 **************************************************************************/
int DbridgeMdfDb_Init(void)
{
    Uint32 i;

	memset((void *)&dbridge_mdf_db, 0, sizeof(DbridgeMdfDataBase_t));

    for (i = 0; i < DBR_MDF_MAX_DSIDS; i++)
    {
        INIT_LIST_HEAD(&(dbridge_mdf_db.dmtTable[i].mcastClientsList));
    }

#ifdef DBRIDGE_LOG
    if(dbridgeDbgLevel & DBRIDGE_DBG_MDF)
    {
        DPRINTK(KERN_INFO"\nInit Dbridge MDF DB done\n");
    }
#endif
    
	return 0;
}

/**************************************************************************/
/*! \fn int DbridgeMdfDb_ReInit(void)                                     
 **************************************************************************
 *  \brief Docsis bridge MDF data base re-initialization.
 *  \param[in] no input.
 *  \param[out] no output.
 *  \return OK or error status.
 **************************************************************************/
int DbridgeMdfDb_ReInit(void)
{
    Uint32 i = 0;

    /* for each DSID index - free Multicast Clients linked List */
    for (i = 0; i < DBR_MDF_MAX_DSIDS; i++)
    {
        struct list_head *pos;
        DbridgeMcastClientsEntry_t *curr = NULL;
        Uint32 lockKey;
    
        PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);

        /* free list */
        pos = dbridge_mdf_db.dmtTable[i].mcastClientsList.next;

        while(pos != &dbridge_mdf_db.dmtTable[i].mcastClientsList)
        {
            curr = list_entry(pos, DbridgeMcastClientsEntry_t, list);
            pos = pos->next;
            PAL_osMemFree(0, curr, sizeof(DbridgeMcastClientsEntry_t));
        }

        PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
    }

	memset((void *)&dbridge_mdf_db, 0, sizeof(DbridgeMdfDataBase_t));

    for (i = 0; i < DBR_MDF_MAX_DSIDS; i++)
    {
        INIT_LIST_HEAD(&(dbridge_mdf_db.dmtTable[i].mcastClientsList));
    }

#ifdef DBRIDGE_LOG
    if(dbridgeDbgLevel & DBRIDGE_DBG_MDF)
    {
        DPRINTK(KERN_INFO"\nRe-Init Dbridge MDF DB done\n");
    }
#endif
    
	return 0;
}

/**************************************************************************/
/*! \fn int DbridgeMdfDb_SetMdfMode(int subtype, Uint32 param1)                                   
 **************************************************************************
 *  \brief Set Docsis Bridge MDF Mode.
 *  \param[in] subtype - command subtype ID.
 *  \param[in] param1 - generic integer parameter.
 *  \param[out] no output.
 *  \return OK or error status.
 **************************************************************************/
int DbridgeMdfDb_SetMdfMode(int subtype, Uint32 param1)
{
    if (param1 == DBR_MDF_ENABLE_MODE)
    {
#ifdef DBRIDGE_LOG
        if(dbridgeDbgLevel & DBRIDGE_DBG_MDF)
        {
            DPRINTK(KERN_INFO"\nSet MDF Mode DBR_MDF_ENABLE_MODE\n");
        }
#endif
    }

    else if (param1 == DBR_MDF_DISABLE_MODE)
    {
#ifdef DBRIDGE_LOG
        if(dbridgeDbgLevel & DBRIDGE_DBG_MDF)
        {
            DPRINTK(KERN_INFO"\nSet MDF Mode DBR_MDF_DISABLE_MODE\n");
        }
#endif
    }

    else
    {
#ifdef DBRIDGE_LOG
        if(dbridgeDbgLevel & DBRIDGE_DBG_MDF)
        {
            DPRINTK(KERN_ERR"\nInvalid MDF Mode %u\n", param1);
        }
#endif
    }

    if (dbridge_mdf_db.mdfMode != param1)
    {
        DbridgeMdfDb_ReInit();
    }

    /* set MDF mode */
    dbridge_mdf_db.mdfMode = param1;

    /* reset MDF confirmation code */
    dbridge_mdf_db.mdfConfCode = DOCS_CONF_CODE_SUCCESS;

#ifdef DBRIDGE_LOG
    if(dbridgeDbgLevel & DBRIDGE_DBG_MDF)
    {
        DPRINTK(KERN_INFO"\nSet Docsis Bridge MDF Mode %u done\n", param1);
    }
#endif

    return 0;
}

/**************************************************************************/
/*! \fn int DbridgeMdfDb_SetPreRegDsid(int subtype, Uint32 param1)                                     
 **************************************************************************
 *  \brief Set Multicast Pre-Registration DSID index.
 *  \param[in] subtype - command subtype ID.
 *  \param[in] param1 - generic integer parameter.
 *  \param[out] no output.
 *  \return OK or error status.
 **************************************************************************/
int DbridgeMdfDb_SetPreRegDsid(int subtype, Uint32 param1)
{
    int ret = 0;

    switch (subtype)
    {
    case MDF_IOCTL_S_PREREGDSID_SUBTYPE_ADD:
        ret = AddPreRegDsid(param1);
        break;
    case MDF_IOCTL_S_PREREGDSID_SUBTYPE_DEL:
        ret = DeletePreRegDsid(param1);
        break;
    default:
        DPRINTK(KERN_ERR"\nInvalid subtype %u\n", subtype);
        return -EINVAL;
    }

#ifdef DBRIDGE_LOG
    if(dbridgeDbgLevel & DBRIDGE_DBG_MDF)
    {
        DPRINTK(KERN_INFO"\nSet Multicast Pre-Registration DSID index %u done\n", param1);
    }
#endif
    
    return ret;
}

/**************************************************************************/
/*! \fn int DbridgeMdfDb_SetMcastDsid(int subtype, Uint32 param1, void *buf)                                   
 **************************************************************************
 *  \brief Set Multicast DSID Encodings.
 *  \param[in] subtype - command subtype ID.
 *  \param[in] param1 - generic integer parameter.
 *  \param[in/out] buf - input/output buffer.
 *  \return OK or error status.
 **************************************************************************/
int DbridgeMdfDb_SetMcastDsid(int subtype, Uint32 param1, void *buf)
{
    int ret = 0;

    switch (subtype)
    {
    case MDF_IOCTL_S_MCASTDSID_SUBTYPE_ADD:
        if (buf)
        {
            ret = AddMcastDsid(param1, (DbridgeMcastEncodings_t *)buf);
        }
        else
        {
            return -EFAULT;
        }
        break;
    case MDF_IOCTL_S_MCASTDSID_SUBTYPE_DEL:
        ret = DeleteMcastDsid(param1);
        break;
    case MDF_IOCTL_S_MCASTDSID_SUBTYPE_UPD:
        if (buf)
        {
            ret = UpdateMcastDsid(param1, (DbridgeMcastEncodings_t *)buf);
        }
        else
        {
            return -EFAULT;
        }
        break;
    default:
        DPRINTK(KERN_ERR"\nInvalid subtype %u\n", subtype);
        return -EINVAL;
    }

#ifdef DBRIDGE_LOG
    if(dbridgeDbgLevel & DBRIDGE_DBG_MDF)
    {
        DPRINTK(KERN_INFO"\nSet Multicast DSID index %u done\n", param1);
    }
#endif
    
    return ret;
}

/**************************************************************************/
/*! \fn int DbridgeMdfDb_GetMcastDocsisClientCmim(int subtype, Uint32 param1, void *buf)                                    
 **************************************************************************
 *  \brief Get Multicast Linux CMIM.
 *  \param[in] subtype - command subtype ID.
 *  \param[in] param1 - generic integer parameter.
 *  \param[in/out] buf - input/output buffer.
 *  \return OK or error status.
 **************************************************************************/
int DbridgeMdfDb_GetMcastDocsisClientCmim(int subtype, Uint32 param1, void *buf)
{
    Uint32 dsidIndex = param1;
    Uint32 cmim;

    if (dbridge_mdf_db.dmtTable[dsidIndex].isDsidIndexValid == False)
    {
        DPRINTK(KERN_ERR"\nUnknown DSID index %u - Can't get Multicast Linux CMIM!!!\n", dsidIndex);
        return -1;
    }

    if (DbridgeCommon_TransLinuxCmimToDocsisCmim(dbridge_mdf_db.dmtTable[dsidIndex].mcastClientsLinuxCmim, &cmim) == -1)
    {
        DPRINTK(KERN_ERR"\nfail translate client linux CMIM to docsis CMIM\n");
    }

    ((DbridgeMcastEncodings_t*)buf)->mcastDocsisCmim = cmim;

#ifdef DBRIDGE_LOG
    if(dbridgeDbgLevel & DBRIDGE_DBG_MDF)
    {
        DPRINTK(KERN_INFO"\nGot Multicast Linux CMIM 0x%llx for DSID index %u\n", 
                dbridge_mdf_db.dmtTable[dsidIndex].mcastLinuxCmim, , dsidIndex);
    }
#endif                                              
    
    return 0;
}

/**************************************************************************/
/*! \fn int DbridgeMdfDb_GetMcastFloodMap(int subtype, Uint32 param1, void *buf)                                    
 **************************************************************************
 *  \brief Get Multicast Flood Map.
 *  \param[in] subtype - command subtype ID.
 *  \param[in] param1 - generic integer parameter.
 *  \param[in/out] buf - input/output buffer.
 *  \return OK or error status.
 **************************************************************************/
int DbridgeMdfDb_GetMcastFloodMap(int subtype, Uint32 param1, void *buf)
{
    Uint32 dsidIndex = param1;

    if (dbridge_mdf_db.dmtTable[dsidIndex].isDsidIndexValid == False)
    {
        DPRINTK(KERN_ERR"\nUnknown DSID index %u - Can't get Multicast Flood Map!!!\n", dsidIndex);
        return -1;
    }

    ((DbridgeMcastEncodings_t*)buf)->mcastFloodMap = dbridge_mdf_db.dmtTable[dsidIndex].mcastFloodMap;

#ifdef DBRIDGE_LOG
    if(dbridgeDbgLevel & DBRIDGE_DBG_MDF)
    {
        DPRINTK(KERN_INFO"\nGot Multicast Flood Map 0x%x for DSID index %u\n", 
                dbridge_mdf_db.dmtTable[dsidIndex].mcastFloodMap, , dsidIndex);
    }
#endif                                              
    
    return 0;
}

/**************************************************************************/
/*! \fn int DbridgeMdfDb_GetMcastDsidConfCode(int subtype, Uint32 param1, void *buf)                                     
 **************************************************************************
 *  \brief Get Multicast DSID confirmation/error code.
 *  \param[in] subtype - command subtype ID.
 *  \param[in] param1 - generic integer parameter.
 *  \param[in/out] buf - input/output buffer.
 *  \return OK or error status.
 **************************************************************************/
int DbridgeMdfDb_GetMcastDsidConfCode(int subtype, Uint32 param1, void *buf)
{
    ((DbridgeMcastEncodings_t*)buf)->mcastConfCode = dbridge_mdf_db.mdfConfCode;

#ifdef DBRIDGE_LOG
    if(dbridgeDbgLevel & DBRIDGE_DBG_MDF)
    {
        DPRINTK(KERN_INFO"\nGot Multicast DSID confirmation code %u\n", dbridge_mdf_db.mdfConfCode);
    }
#endif                                              

    return 0;
}

/**************************************************************************/
/*! \fn int DbridgeMdfDb_SetMcastDsidConfCode(int subtype, Uint32 param1)
 **************************************************************************
 *  \brief Get Multicast DSID confirmation/error code.
 *  \param[in] subtype - command subtype ID.
 *  \param[in] param1 - generic integer parameter.
 *  \return OK or error status.
 **************************************************************************/
int DbridgeMdfDb_SetMcastDsidConfCode(int subtype, Uint32 param1)
{
    dbridge_mdf_db.mdfConfCode = param1;

#ifdef DBRIDGE_LOG
    if(dbridgeDbgLevel & DBRIDGE_DBG_MDF)
    {
        DPRINTK(KERN_INFO"\nSet Multicast DSID confirmation code %u\n", dbridge_mdf_db.mdfConfCode);
    }
#endif                                              

    return 0;
}

/**************************************************************************/
/*! \fn int DbridgeMdfDb_GetAltMemeberExist(int subtype, void *buf)
 **************************************************************************
 *  \brief Does ALT member exist, key is MAC address
 *  \param[in] subtype - command subtype ID.
 *  \param[in/out] buf - input/output buffer.
 *  \return OK or error status.
 **************************************************************************/
int DbridgeMdfDb_GetAltMemeberExist(int subtype, void *buf)
{
    DbridgeGetAltMemberExist_t *p = buf;
    Uint8* clientMacAddress;
    DbridgeAltEntry_t* dbridgeAltEntry = NULL;
    DBRALT_PROT_DCL(lockKey);

    if (!buf)
    {
        #ifdef DBRIDGE_LOG
            if(dbridgeDbgLevel & DBRIDGE_DBG_MDF)
            {
                DPRINTK(KERN_INFO"\nFailed to Get ALT member --> NULL pointer\n");
            }
        #endif                                              
        return -EFAULT;
    }

    clientMacAddress = p->clientMacAddress;

#ifdef DBRIDGE_LOG
    if(dbridgeDbgLevel & DBRIDGE_DBG_MDF)
    {
        DPRINTK(KERN_INFO"\nGet ALT member by MAC Address %.2x-%.2x-%.2x-%.2x-%.2x-%.2x\n", 
                clientMacAddress[0],
                clientMacAddress[1],
                clientMacAddress[2],
                clientMacAddress[3],
                clientMacAddress[4],
                clientMacAddress[5]);
    }
#endif

    DBRALT_PROT_ON(lockKey);
    dbridgeAltEntry = DbridgeDb_AltGetMemberByMac(clientMacAddress);

    /* client MAC address not found in ALT table */
    if (dbridgeAltEntry == NULL)
    {
        p->found = False;
    }
    else
    {
        p->found = True;
    }
    DBRALT_PROT_OFF(lockKey);

    return 0;
}

/**************************************************************************/
/*! \fn int DbridgeMdfDb_Print(void)                                     
 **************************************************************************
 *  \brief Print Docsis bridge MDF data base.
 *  \param[in] no input.
 *  \param[out] no output.
 *  \return OK or error status.
 **************************************************************************/
int DbridgeMdfDb_Print(void)
{
    Uint32 i = 0;
    Uint32 j = 0;
    struct list_head *ptr = NULL;
    DbridgeMcastClientsEntry_t *curr = NULL;

    printk("*************************************************************************************\n");
    printk("*                          DOCSIS Bridge MDF DB                                     *\n");
    printk("*************************************************************************************\n");

	printk("MDF Mode = %u\n", dbridge_mdf_db.mdfMode);
	printk("dmtCurrDsidNum = %u\n", dbridge_mdf_db.dmtCurrDsidNum);
    printk("mdfConfCode = %u\n", dbridge_mdf_db.mdfConfCode);
    printk("\n");

    for (i = 0; i < DBR_MDF_MAX_DSIDS; i++)
    {
        if (dbridge_mdf_db.dmtTable[i].isDsidIndexValid)
        {
            printk("===============================================================\n");
            printk("                        DSID Index %d                          \n", i);
            printk("===============================================================\n");

            printk("dmtTable[%d].mcastLinuxCmim = 0x%llx\n", i, dbridge_mdf_db.dmtTable[i].mcastLinuxCmim);
            printk("dmtTable[%d].mcastDocsisToLinuxCmim = 0x%llx\n", i, dbridge_mdf_db.dmtTable[i].mcastDocsisToLinuxCmim);
            printk("dmtTable[%d].mcastClientsLinuxCmim = 0x%llx\n", i, dbridge_mdf_db.dmtTable[i].mcastClientsLinuxCmim);
    
            printk("dmtTable[%d].mcastFloodMap = 0x%x\n", i, dbridge_mdf_db.dmtTable[i].mcastFloodMap);
            printk("dmtTable[%d].mcastDocsisCmimToFloodMap = 0x%x\n", i, dbridge_mdf_db.dmtTable[i].mcastDocsisCmimToFloodMap);
            printk("dmtTable[%d].mcastClientsFloodMap = 0x%x\n", i, dbridge_mdf_db.dmtTable[i].mcastClientsFloodMap);

            printk("dmtTable[%d].mcastClientsNum = %u\n", i, dbridge_mdf_db.dmtTable[i].mcastClientsNum);

            if (dbridge_mdf_db.dmtTable[i].mcastClientsNum != 0)
            {
                list_for_each(ptr, &(dbridge_mdf_db.dmtTable[i].mcastClientsList))
                {
                    curr = list_entry(ptr, DbridgeMcastClientsEntry_t, list);
    
                    printk("---------------------------------------------------------------\n");
                    printk("dmtTable[%d].mcastClientsList->dbridgeAltEntry->macAddress = "
                           "[%.2x-%.2x-%.2x-%.2x-%.2x-%.2x]\n", i, 
                           curr->dbridgeAltEntry->macAddress[0],
                           curr->dbridgeAltEntry->macAddress[1],
                           curr->dbridgeAltEntry->macAddress[2],
                           curr->dbridgeAltEntry->macAddress[3],
                           curr->dbridgeAltEntry->macAddress[4],
                           curr->dbridgeAltEntry->macAddress[5]);

                    switch(curr->dbridgeAltEntry->macLearningFrom)
                    {
                    case DBRIDGE_INIT:
                        printk("dmtTable[%d].mcastClientsList->dbridgeAltEntry->macLearningFrom = Init\n", i);
                        break;
                    case CPE_STATIC:
                        printk("dmtTable[%d].mcastClientsList->dbridgeAltEntry->macLearningFrom = Static\n", i);
                        break;
                    case CPE_DYNAMIC:
                        printk("dmtTable[%d].mcastClientsList->dbridgeAltEntry->macLearningFrom = Dynamic\n", i);
                        break;
                    default:
                        printk("dmtTable[%d].mcastClientsList->dbridgeAltEntry->macLearningFrom = Unknown\n", i);
                    }

                    printk("dmtTable[%d].mcastClientsList->dbridgeAltEntry->cpe_number = %u\n", 
                           i, curr->dbridgeAltEntry->cpe_number);

                    if (curr->dbridgeAltEntry->sourceInterface)
                    {
                        printk("dmtTable[%d].mcastClientsList->dbridgeAltEntry->sourceInterface->name = %s\n", 
                               i, curr->dbridgeAltEntry->sourceInterface->name);
                        printk("dmtTable[%d].mcastClientsList->dbridgeAltEntry->sourceInterface->ifindex = %u\n", 
                               i, curr->dbridgeAltEntry->sourceInterface->ifindex);
                    }

                    if (curr->dbridgeAltEntry->dbridgeDevice)            
                    {
                        printk("dmtTable[%d].mcastClientsList->dbridgeAltEntry->dbridgeDevice->ptr_interface->name = %s\n", 
                               i, curr->dbridgeAltEntry->dbridgeDevice->ptr_interface->name);
                        printk("dmtTable[%d].mcastClientsList->dbridgeAltEntry->dbridgeDevice->ptr_interface->ifindex = %u\n", 
                               i, curr->dbridgeAltEntry->dbridgeDevice->ptr_interface->ifindex);
                        printk("dmtTable[%d].mcastClientsList->dbridgeAltEntry->dbridgeDevice->net_dev_type = %u\n", 
                               i, curr->dbridgeAltEntry->dbridgeDevice->net_dev_type);
                    }
         
                    printk("---------------------------------------------------------------\n");
                }
            }

            for (j = 0; j < DBR_MAX_ALL_INTERFACE; j++)
            {
                if (dbridge_mdf_db.dmtTable[i].ifDevIndexRefCount[j] != 0)
                {
                    printk("dmtTable[%d].ifDevIndexRefCount[%d] = %u\n", i, j, dbridge_mdf_db.dmtTable[i].ifDevIndexRefCount[j]);
                }
            }

            for (j = 0; j < DBR_MAX_NET_DEV_TYPES_ENTRY; j++)
            {
                if (dbridge_mdf_db.dmtTable[i].ifNetDevTypeRefCount[j] != 0)
                {
                    printk("dmtTable[%d].ifNetDevTypeRefCount[%d] = %u\n", i, j, dbridge_mdf_db.dmtTable[i].ifNetDevTypeRefCount[j]);
                }
            }
        }
    }

    printk("*************************************************************************************\n");
	printk("\n");

    return 0;
}

/**************************************************************************/
/*! \fn int DbridgeMdfDb_GetMdfMode(Uint32* mdfMode)                                     
 **************************************************************************
 *  \brief Get Docsis Bridge MDF Mode.
 *  \param[in] no input.
 *  \param[out] mdfMode - the Docsis Bridge MDF Mode to be returned.
 *  \return OK or error status.
 **************************************************************************/
int DbridgeMdfDb_GetMdfMode(Uint32* mdfMode)
{
    *mdfMode = dbridge_mdf_db.mdfMode;

    return 0;
}

/**************************************************************************/
/*! \fn int DbridgeMdfDb_GetMcastBitmaps(Uint32 dsidIndex, Uint32* mcastLinuxCmim, Uint32* mcastFloodMap)                                     
 **************************************************************************
 *  \brief Get the Multicast bitmaps according to the dsid index.
 *  \param[in] dsidIndex - dsid index value.
 *  \param[out] mcastLinuxCmim - Multicast Linux CMIM value to be returned.
 *  \param[out] mcastFloodMap - Multicast Flood Map value to be returned.
 *  \return OK or error status.
 **************************************************************************/
int DbridgeMdfDb_GetMcastBitmaps(Uint32 dsidIndex, unsigned long long* mcastLinuxCmim, Uint32* mcastFloodMap)
{
    if (dbridge_mdf_db.dmtTable[dsidIndex].isDsidIndexValid == False)
    {
        DPRINTK(KERN_ERR"\nUnknown DSID index %u - Can't forward the packet!!!\n", dsidIndex);
        return -1;
    }

    *mcastLinuxCmim = dbridge_mdf_db.dmtTable[dsidIndex].mcastLinuxCmim;
    *mcastFloodMap = dbridge_mdf_db.dmtTable[dsidIndex].mcastFloodMap;

#ifdef DBRIDGE_LOG
    if(dbridgeDbgLevel & DBRIDGE_DBG_MDF)
    {
        DPRINTK(KERN_INFO"\nGot Multicast Linux CMIM 0x%llx and Multicast Flood map 0x%x for DSID index %u\n", 
                *mcastLinuxCmim, *mcastFloodMap, dsidIndex);
    }
#endif                                              
    
    return 0;
}


/**************************************************************************/
/*      LOCAL FUNCTIONS:                                                  */
/**************************************************************************/


/**************************************************************************/
/*! \fn static int AddPreRegDsid(Uint32 dsidIndex)                                    
 **************************************************************************
 *  \brief Add Pre-Registration DSID to Docsis bridge MDF DB.
 *  \param[in] dsidIndex - the DSID index value.
 *  \param[out] no output.
 *  \return OK or error status.
 **************************************************************************/
static int AddPreRegDsid(Uint32 dsidIndex)
{
    /* check if Dbridge MDF DB is full */
    if ((dbridge_mdf_db.dmtCurrDsidNum >= DBR_MDF_MAX_DSIDS) || (dbridge_mdf_db.dmtCurrDsidNum != 0))
    {
        DPRINTK(KERN_ERR"\nFailed to add Pre-Registration DSID index %u - Dbridge MDF DB Current DSIDs num is %d\n", 
                dsidIndex, dbridge_mdf_db.dmtCurrDsidNum);
        return -1;
    }

    /* check if DSID index exist */
    if (dbridge_mdf_db.dmtTable[dsidIndex].isDsidIndexValid == True)
    {
        DPRINTK(KERN_ERR"\nFailed to add Pre-Registration DSID - DSID index %u alreay exist\n", dsidIndex);
        return -1;
    }

    /* set the interface bit in the Multicast Linux CMIM bitmask */
    dbridge_mdf_db.dmtTable[dsidIndex].mcastLinuxCmim = DBR_MCAST_LINUX_CMIM_IP_STACK;

    /* set the interface bit in the Multicast Flood Map */
    dbridge_mdf_db.dmtTable[dsidIndex].mcastFloodMap |= DBR_WAN_IP_NET_DEV_TYPE;

    /* mark the dsid index as valid */
    dbridge_mdf_db.dmtTable[dsidIndex].isDsidIndexValid = True;

    /* increase the number of DSID indexes */
    dbridge_mdf_db.dmtCurrDsidNum++;

#ifdef DBRIDGE_LOG
    if(dbridgeDbgLevel & DBRIDGE_DBG_MDF)
    {
        DPRINTK(KERN_INFO"\nUpdate Multicast Linux CMIM 0x%llx for Pre-Registration DSID index %u done\n", 
                dbridge_mdf_db.dmtTable[dsidIndex].mcastLinuxCmim, dsidIndex);
        DPRINTK(KERN_INFO"\nUpdate Multicast Flood Map 0x%x for Pre-Registration DSID index %u done\n", 
                dbridge_mdf_db.dmtTable[dsidIndex].mcastFloodMap, dsidIndex);
        DPRINTK(KERN_INFO"\nAdd Multicast Pre-registration DSID index %u done\n", dsidIndex);
    }
#endif
    
    return 0;
}

/**************************************************************************/
/*! \fn static int DeletePreRegDsid(Uint32 dsidIndex)                                   
 **************************************************************************
 *  \brief delete Pre-Registration DSID from Docsis bridge MDF DB.
 *  \param[in] dsidIndex - the DSID index value.
 *  \param[out] no output.
 *  \return OK or error status.
 **************************************************************************/
static int DeletePreRegDsid(Uint32 dsidIndex)
{
    /* check if Dbridge MDF DB is empty */
    if (dbridge_mdf_db.dmtCurrDsidNum != 1)
    {
        DPRINTK(KERN_ERR"\nFailed to delete Pre-Registration DSID index %u - Dbridge MDF DB Current DSIDs num is %d\n", 
                dsidIndex, dbridge_mdf_db.dmtCurrDsidNum);
        return -1;
    }

    /* check if DSID index exist */
    if (dbridge_mdf_db.dmtTable[dsidIndex].isDsidIndexValid == False)
    {
        DPRINTK(KERN_ERR"\nFailed to delete Pre-Registration DSID - DSID index %u not found\n", dsidIndex);
        return -1;
    }

    /* un-set the interface bit in the Multicast Linux CMIM bitmask */
    dbridge_mdf_db.dmtTable[dsidIndex].mcastLinuxCmim = 0;

    /* un-set the interface bit in the Multicast Flood Map */
    dbridge_mdf_db.dmtTable[dsidIndex].mcastFloodMap = 0;

    /* mark the dsid index as invalid */
    dbridge_mdf_db.dmtTable[dsidIndex].isDsidIndexValid = False;

    /* decrease the number of DSID indexes */
    dbridge_mdf_db.dmtCurrDsidNum--;

#ifdef DBRIDGE_LOG
    if(dbridgeDbgLevel & DBRIDGE_DBG_MDF)
    {
        DPRINTK(KERN_INFO"\nUpdate Multicast Linux CMIM 0x%llx for Pre-Registration DSID index %u done\n", 
                dbridge_mdf_db.dmtTable[dsidIndex].mcastLinuxCmim, dsidIndex);
        DPRINTK(KERN_INFO"\nUpdate Multicast Flood Map 0x%x for Pre-Registration DSID index %u done\n", 
                dbridge_mdf_db.dmtTable[dsidIndex].mcastFloodMap, dsidIndex);
        DPRINTK(KERN_INFO"\nDelete Multicast Pre-registration DSID index %u done\n", dsidIndex);
    }
#endif
    
    return 0;
}

/**************************************************************************/
/*! \fn static int AddMcastDsid(Uint32 dsidIndex, DbridgeMcastEncodings_t* buf)                                  
 **************************************************************************
 *  \brief Add Multicast DSID to Docsis bridge MDF DB.
 *  \param[in] dsidIndex - the DSID index value.
 *  \param[in] buf - the Docsis Bridge Multicast Encodings structure.
 *  \param[out] no output.
 *  \return OK or error status.
 **************************************************************************/
static int AddMcastDsid(Uint32 dsidIndex, DbridgeMcastEncodings_t* buf)
{
    Uint32 i = 0;

    /* check if Dbridge MDF DB is full */
    if (dbridge_mdf_db.dmtCurrDsidNum >= DBR_MDF_MAX_DSIDS)
    {
        DPRINTK(KERN_ERR"\nFailed to add Multicast DSID index %u - Dbridge MDF DB is full (Max-Dsid = %d)\n", dsidIndex, DBR_MDF_MAX_DSIDS);
        return -1;
    }

    /* check if DSID index exist */
    if (dbridge_mdf_db.dmtTable[dsidIndex].isDsidIndexValid == True)
    {
        DPRINTK(KERN_ERR"\nFailed to add Multicast DSID - DSID index %u alreay exist\n", dsidIndex);
        return -1;
    }

#ifdef DBRIDGE_LOG
    if(dbridgeDbgLevel & DBRIDGE_DBG_MDF)
    {
        DPRINTK(KERN_INFO"\ngotMcastDocsisCmim = %u\n",buf->gotMcastDocsisCmim);
    }
#endif
    
    /* if mcastDocsisCmim has changed */
    if (buf->gotMcastDocsisCmim)
    {
#ifdef DBRIDGE_LOG
    if(dbridgeDbgLevel & DBRIDGE_DBG_MDF)
    {
        DPRINTK(KERN_INFO"\nUpdate Multicast Docsis CMIM 0x%llx\n", buf->mcastDocsisCmim);
    }
#endif
        /* update Multicast Docsis CMIM */
        UpdateMcastDocsisCmim(dsidIndex, buf->mcastDocsisCmim);
    }

#ifdef DBRIDGE_LOG
    if(dbridgeDbgLevel & DBRIDGE_DBG_MDF)
    {
        DPRINTK(KERN_INFO"\nMulticast Client Mac Address Num is %u\n", buf->clientMacAddrNum);
    }
#endif
    
    /* for all client MAC address encodings */
    for (i = 0; i < buf->clientMacAddrNum; i++)
    {
        /* add Multicast client MAC address */
        if (AddMcastClientMacAddress(dsidIndex, buf->clientMacAddrEncodings[i].clientMacAddress) == -1)
        {
            DPRINTK(KERN_ERR"\nFailed to Add Multicast client MAC Address  "
                            "%.2x-%.2x-%.2x-%.2x-%.2x-%.2x for DSID index %u\n",
                    buf->clientMacAddrEncodings[i].clientMacAddress[0],
                    buf->clientMacAddrEncodings[i].clientMacAddress[1],
                    buf->clientMacAddrEncodings[i].clientMacAddress[2],
                    buf->clientMacAddrEncodings[i].clientMacAddress[3],
                    buf->clientMacAddrEncodings[i].clientMacAddress[4],
                    buf->clientMacAddrEncodings[i].clientMacAddress[5], dsidIndex);
            return -1;
        }
    }

    /* mark the dsid index as valid */
    dbridge_mdf_db.dmtTable[dsidIndex].isDsidIndexValid = True;

    /* increase the number of DSID indexes */
    dbridge_mdf_db.dmtCurrDsidNum++;

    /* update Multicast Linux CMIM parameter in Dbridge MDF DB */
    dbridge_mdf_db.dmtTable[dsidIndex].mcastLinuxCmim = (dbridge_mdf_db.dmtTable[dsidIndex].mcastDocsisToLinuxCmim |
        dbridge_mdf_db.dmtTable[dsidIndex].mcastClientsLinuxCmim);

    /* update Multicast Flood Map parameter in Dbridge MDF DB */
    dbridge_mdf_db.dmtTable[dsidIndex].mcastFloodMap = (dbridge_mdf_db.dmtTable[dsidIndex].mcastDocsisCmimToFloodMap |
        dbridge_mdf_db.dmtTable[dsidIndex].mcastClientsFloodMap);

#ifdef DBRIDGE_LOG
    if(dbridgeDbgLevel & DBRIDGE_DBG_MDF)
    {
        DPRINTK(KERN_INFO"\nUpdate Multicast Linux CMIM 0x%llx for DSID index %u done\n", 
                dbridge_mdf_db.dmtTable[dsidIndex].mcastLinuxCmim, dsidIndex);
        DPRINTK(KERN_INFO"\nUpdate Multicast Flood Map 0x%x for DSID index %u done\n", 
                dbridge_mdf_db.dmtTable[dsidIndex].mcastFloodMap, dsidIndex);
        DPRINTK(KERN_INFO"\nAdd Multicast DSID index %u done\n", dsidIndex);
    }
#endif
    
    return 0;
}

/**************************************************************************/
/*! \fn static int DeleteMcastDsid(Uint32 dsidIndex)                         
 **************************************************************************
 *  \brief Delete Multicast DSID from Docsis bridge MDF DB.
 *  \param[in] dsidIndex - the DSID index value.
 *  \param[out] no output.
 *  \return OK or error status.
 **************************************************************************/
static int DeleteMcastDsid(Uint32 dsidIndex)
{
    struct list_head *pos;
    DbridgeMcastClientsEntry_t *curr = NULL;
    Uint32 lockKey;

    /* check if Dbridge MDF DB is empty */
    if (dbridge_mdf_db.dmtCurrDsidNum == 0)
    {
        DPRINTK(KERN_ERR"\nFailed to delete Multicast DSID index %u - Dbridge MDF DB is empty\n", dsidIndex);
        return -1;
    }

    /* check if DSID index exist */
    if (dbridge_mdf_db.dmtTable[dsidIndex].isDsidIndexValid == False)
    {
        DPRINTK(KERN_ERR"\nFailed to delete Multicast DSID - DSID index %u not found\n", dsidIndex);
        return -1;
    }

    PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);

    /* free list */
    pos = dbridge_mdf_db.dmtTable[dsidIndex].mcastClientsList.next;

    while(pos != &(dbridge_mdf_db.dmtTable[dsidIndex].mcastClientsList))
    {
        curr = list_entry(pos, DbridgeMcastClientsEntry_t, list);
        pos = pos->next;
        PAL_osMemFree(0, curr, sizeof(DbridgeMcastClientsEntry_t));
    }

    /* delete the dsid index entry in Mapping DSID index table */
    memset((void *)&(dbridge_mdf_db.dmtTable[dsidIndex]), 0, sizeof(DbridgeDmtEntry_t));
    INIT_LIST_HEAD(&(dbridge_mdf_db.dmtTable[dsidIndex].mcastClientsList));

    /* mark the dsid index as invalid */
    dbridge_mdf_db.dmtTable[dsidIndex].isDsidIndexValid = False;

    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);

    /* decrease the number of DSID indexes */
    dbridge_mdf_db.dmtCurrDsidNum--;

#ifdef DBRIDGE_LOG
    if(dbridgeDbgLevel & DBRIDGE_DBG_MDF)
    {
        DPRINTK(KERN_INFO"\nDelete Multicast DSID index %u done\n", dsidIndex);
    }
#endif
    
    return 0;
}

/**************************************************************************/
/*! \fn static int UpdateMcastDsid(Uint32 dsidIndex, DbridgeMcastEncodings_t* buf)                                  
 **************************************************************************
 *  \brief Update Multicast DSID in Docsis bridge MDF DB.
 *  \param[in] dsidIndex - the DSID index value.
 *  \param[in] buf - the Docsis Bridge Multicast Encodings structure.
 *  \param[out] no output.
 *  \return OK or error status.
 **************************************************************************/
static int UpdateMcastDsid(Uint32 dsidIndex, DbridgeMcastEncodings_t* buf)
{
    Uint32 i = 0;

    if (dbridge_mdf_db.dmtCurrDsidNum == 0)
    {
        DPRINTK(KERN_ERR"\nFailed to update Multicast DSID index %u - Dbridge MDF DB is empty\n", dsidIndex);
        return -1;
    }

    if (dbridge_mdf_db.dmtTable[dsidIndex].isDsidIndexValid == False)
    {
        DPRINTK(KERN_ERR"\nFailed to update Multicast DSID - DSID index %u not found\n", dsidIndex);
        return -1;
    }

    /* if mcastDocsisCmim has changed */
    if (buf->gotMcastDocsisCmim)
    {
#ifdef DBRIDGE_LOG
    if(dbridgeDbgLevel & DBRIDGE_DBG_MDF)
    {
        DPRINTK(KERN_INFO"\nUpdate Multicast Docsis CMIM 0x%x\n", buf->mcastDocsisCmim);
    }
#endif
        /* update Multicast Docsis CMIM */
        UpdateMcastDocsisCmim(dsidIndex, buf->mcastDocsisCmim);
    }

#ifdef DBRIDGE_LOG
    if(dbridgeDbgLevel & DBRIDGE_DBG_MDF)
    {
        DPRINTK(KERN_INFO"\nMulticast Client Mac Address Num is %u\n", buf->clientMacAddrNum);
    }
#endif

    /* for all client MAC address encodings */
    for (i = 0; i < buf->clientMacAddrNum; i++)
    {
        /* if client MAC Address action is Add */
        if (buf->clientMacAddrEncodings[i].clientMacAddressAction == DBRIDGE_MDF_CLIENT_MAC_ADDR_ACTION_ADD)
        {
            /* add old client MAC Address */
            if (AddMcastClientMacAddress(dsidIndex, buf->clientMacAddrEncodings[i].clientMacAddress) == -1)
            {
                DPRINTK(KERN_ERR"\nFailed to Add Multicast client MAC Address  "
                                "%.2x-%.2x-%.2x-%.2x-%.2x-%.2x for DSID index %u\n",
                        buf->clientMacAddrEncodings[i].clientMacAddress[0],
                        buf->clientMacAddrEncodings[i].clientMacAddress[1],
                        buf->clientMacAddrEncodings[i].clientMacAddress[2],
                        buf->clientMacAddrEncodings[i].clientMacAddress[3],
                        buf->clientMacAddrEncodings[i].clientMacAddress[4],
                        buf->clientMacAddrEncodings[i].clientMacAddress[5], dsidIndex);
                return -1;
            }
        }

        /* if client MAC Address action is Delete */
        else if (buf->clientMacAddrEncodings[i].clientMacAddressAction == DBRIDGE_MDF_CLIENT_MAC_ADDR_ACTION_DELETE)
        {
            /* delete old client MAC Address */
            if (DeleteMcastClientMacAddress(dsidIndex, buf->clientMacAddrEncodings[i].clientMacAddress) == -1)
            {
                DPRINTK(KERN_ERR"\nFailed to Delete Multicast client MAC Address  "
                                "%.2x-%.2x-%.2x-%.2x-%.2x-%.2x for DSID index %u\n",
                        buf->clientMacAddrEncodings[i].clientMacAddress[0],
                        buf->clientMacAddrEncodings[i].clientMacAddress[1],
                        buf->clientMacAddrEncodings[i].clientMacAddress[2],
                        buf->clientMacAddrEncodings[i].clientMacAddress[3],
                        buf->clientMacAddrEncodings[i].clientMacAddress[4],
                        buf->clientMacAddrEncodings[i].clientMacAddress[5], dsidIndex);
                // ARRIS MODIFY Start
                // if we returned with -1 the function it will cause the HandleDsidUpdate to return and not execute the code to handle the lower layers.
                // This will cause the mismatch in the hal_db and dsid_db and Modem will not be able to delete the dsid's and will run of dsid's.
                return 0;
                // ARRIS MODIFY End
            }
        }

        /* invalid client MAC Address action */
        else
        {
            DPRINTK(KERN_ERR"\nInvalid Multicast client Mac Address action %u for DSID index %u\n",
                    buf->clientMacAddrEncodings[i].clientMacAddressAction, dsidIndex);
        }
    }

    /* update Multicast Linux CMIM parameter in Dbridge MDF DB */
    dbridge_mdf_db.dmtTable[dsidIndex].mcastLinuxCmim = (dbridge_mdf_db.dmtTable[dsidIndex].mcastDocsisToLinuxCmim |
        dbridge_mdf_db.dmtTable[dsidIndex].mcastClientsLinuxCmim);

    /* update Multicast Flood Map parameter in Dbridge MDF DB */
    dbridge_mdf_db.dmtTable[dsidIndex].mcastFloodMap = (dbridge_mdf_db.dmtTable[dsidIndex].mcastDocsisCmimToFloodMap |
        dbridge_mdf_db.dmtTable[dsidIndex].mcastClientsFloodMap);

#ifdef DBRIDGE_LOG
    if(dbridgeDbgLevel & DBRIDGE_DBG_MDF)
    {
        DPRINTK(KERN_INFO"\nUpdate Multicast Linux CMIM 0x%llx for DSID index %u done\n", 
                dbridge_mdf_db.dmtTable[dsidIndex].mcastLinuxCmim, dsidIndex);
        DPRINTK(KERN_INFO"\nUpdate Multicast Flood Map 0x%x for DSID index %u done\n", 
                dbridge_mdf_db.dmtTable[dsidIndex].mcastFloodMap, dsidIndex);
        DPRINTK(KERN_INFO"\nUpdate Multicast DSID index %u done\n", dsidIndex);
    }
#endif
    
    return 0;
}


/**************************************************************************/
/*! \fn static int UpdateMcastDocsisCmim(Uint32 dsidIndex, Uint32 mcastDocsisCmim)                                    
 **************************************************************************
 *  \brief Update the Multicast CMIM Docsis interfaces indexed bitmap. 
 *  \param[in] dsidIndex - the DSID index value.
 *  \param[in] mcastDocsisCmim - the Multicast Docsis CMIM value.
 *  \param[out] no output.
 *  \return OK or error status.
 **************************************************************************/
static int UpdateMcastDocsisCmim(Uint32 dsidIndex, Uint32 mcastDocsisCmim)
{
    unsigned long long transMcastLinuxCmim = 0;
    Uint32 transMcastFloodMap = 0;

    /* translate the Multicast Docsis CMIM to Multicast Linux CMIM */
    if (DbridgeCommon_TransDocsisCmimToLinuxCmim(mcastDocsisCmim, &transMcastLinuxCmim) == -1)
    {
        DPRINTK(KERN_INFO"\nFailed to translate Multicast Docsis CMIM %x DSID index %u to Multicast Linux CMIM\n", 
                mcastDocsisCmim, dsidIndex);
        return -1;
    }

    /* update Multicast Docsis to Linux CMIM parameter in Dbridge MDF DB */
    dbridge_mdf_db.dmtTable[dsidIndex].mcastDocsisToLinuxCmim = transMcastLinuxCmim;

#ifdef DBRIDGE_LOG
    if(dbridgeDbgLevel & DBRIDGE_DBG_MDF)
    {
        DPRINTK(KERN_INFO"\nFinish update Multicast Docsis to Linux CMIM 0x%llx for DSID index %u\n", 
                transMcastLinuxCmim, dsidIndex);
    }
#endif
    
    /* translate the Multicast Docsis CMIM to Multicast Flood Map */
    if (DbridgeCommon_TransDocsisCmimToFloodMap(mcastDocsisCmim, &transMcastFloodMap) == -1)
    {
        DPRINTK(KERN_INFO"\nFailed to translate Multicast Docsis CMIM 0x%x DSID index %u to Multicast Flood Map\n", 
                mcastDocsisCmim, dsidIndex);
        return -1;
    }

    /* update Multicast Docsis to Flood Map parameter in Dbridge MDF DB */
    dbridge_mdf_db.dmtTable[dsidIndex].mcastDocsisCmimToFloodMap = transMcastFloodMap;

#ifdef DBRIDGE_LOG
    if(dbridgeDbgLevel & DBRIDGE_DBG_MDF)
    {
        DPRINTK(KERN_INFO"\nFinish update Multicast Docsis CMIM to Flood Map 0x%x for DSID index %u\n", 
                transMcastFloodMap, dsidIndex);
        DPRINTK(KERN_INFO"\nUpdate Multicast Docsis CMIM 0x%llx for DSID index %u done\n", 
                mcastDocsisCmim, dsidIndex);
    }
#endif
    
    return 0;
}

/**************************************************************************/
/*! \fn static int AddMcastClientMacAddress(Uint32 dsidIndex, Uint8* clientMacAddress)                                  
 **************************************************************************
 *  \brief Add Multicast Client MAC address to Docsis bridge MDF DB.
 *  \param[in] dsidIndex - the DSID index value.
 *  \param[in] clientMacAddress - the Multicast client MAC address value.
 *  \param[out] no output.
 *  \return OK or error status.
 **************************************************************************/
static int AddMcastClientMacAddress(Uint32 dsidIndex, Uint8* clientMacAddress)
{
	int	ifindex = 0;
    int net_dev_type = 0;
	Uint32 i = 0;
    Uint32 lockKey = 0;
    DbridgeAltEntry_t* dbridgeAltEntry = NULL;
    DbridgeMcastClientsEntry_t* newEntry = NULL;

#ifdef DBRIDGE_LOG
    if(dbridgeDbgLevel & DBRIDGE_DBG_MDF)
    {
        DPRINTK(KERN_INFO"\nAdd Multicast client MAC Address %.2x-%.2x-%.2x-%.2x-%.2x-%.2x\n", 
                clientMacAddress[0],
                clientMacAddress[1],
                clientMacAddress[2],
                clientMacAddress[3],
                clientMacAddress[4],
                clientMacAddress[5]);
    }
#endif

    dbridgeAltEntry = DbridgeDb_AltGetMemberByMac(clientMacAddress);

    /* client MAC address not found in ALT table */
    if (dbridgeAltEntry == NULL)
    {
        DPRINTK(KERN_ERR"\nDSID Multicast Client MAC address "
                        "%.2x-%.2x-%.2x-%.2x-%.2x-%.2x is not known by the CM\n", 
                clientMacAddress[0],
                clientMacAddress[1],
                clientMacAddress[2],
                clientMacAddress[3],
                clientMacAddress[4],
                clientMacAddress[5]);
        /* Confirmation code 42 - reject-unknown-client-macaddress: */
        /* DSID Multicast Client MAC address is not known by the CM (via REG-ACK and DBC-RSP). */
        dbridge_mdf_db.mdfConfCode = DOCS_CONF_CODE_UNKNOWN_CLIENT_MAC_ADDR;
        return 0;
    }

#ifdef DBRIDGE_LOG
    if(dbridgeDbgLevel & DBRIDGE_DBG_MDF)
    {
        DPRINTK(KERN_INFO"\nDocsis Bridge ALT entry for client MAC address %.2x-%.2x-%.2x-%.2x-%.2x-%.2x is found\n",
                clientMacAddress[0],
                clientMacAddress[1],
                clientMacAddress[2],
                clientMacAddress[3],
                clientMacAddress[4],
                clientMacAddress[5]);
    }
#endif

#ifdef DBRIDGE_LOG
    if(dbridgeDbgLevel & DBRIDGE_DBG_MDF)
    {
        DPRINTK(KERN_INFO"\ndbridgeAltEntry->macLearningFrom = %u\n", dbridgeAltEntry->macLearningFrom);
    }
#endif

    /* get Linux interface index from the ALT entry */
    if (dbridgeAltEntry->sourceInterface)
    {
        ifindex = dbridgeAltEntry->sourceInterface->ifindex;
    }
    else if (dbridgeAltEntry->macLearningFrom == CPE_STATIC)
    {
        ifindex = 0;
    }
    else
    {
        ifindex = dbridgeAltEntry->dbridgeDevice->ptr_interface->ifindex;
    }

#ifdef DBRIDGE_LOG
    if(dbridgeDbgLevel & DBRIDGE_DBG_MDF)
    {
        DPRINTK(KERN_INFO"\nLinux Interface Index (ifindex) = %u (0x%x)\n", ifindex, ifindex);
    }
#endif

    if (ifindex)
    {   
        /* increase device index reference count for this interface */
        dbridge_mdf_db.dmtTable[dsidIndex].ifDevIndexRefCount[ifindex - 1]++;
    
        /* set the interface bit in the Multicast clients Linux CMIM bitmask */
        SET_BIT_MASK_64(dbridge_mdf_db.dmtTable[dsidIndex].mcastClientsLinuxCmim, ifindex - 1);
    }
    else
    {
        /* increase device index reference count for all interfaces */
        for (i = 0; i < DBR_MAX_ALL_INTERFACE; i++)
        {
            dbridge_mdf_db.dmtTable[dsidIndex].ifDevIndexRefCount[i]++;
        }

        /* set all interfaces bits in the Multicast clients Linux CMIM bitmask */
        dbridge_mdf_db.dmtTable[dsidIndex].mcastClientsLinuxCmim = 0;
    }

    /* get network device type from the ALT entry */
    net_dev_type = dbridgeAltEntry->dbridgeDevice->net_dev_type;

#ifdef DBRIDGE_LOG
    if(dbridgeDbgLevel & DBRIDGE_DBG_MDF)
    {
        DPRINTK(KERN_INFO"\nNetwork Device Type (net_dev_type) = %u (0x%x)\n", net_dev_type, net_dev_type);
    }
#endif
    
    /* increase network device type reference count for this interface */
    dbridge_mdf_db.dmtTable[dsidIndex].ifNetDevTypeRefCount[net_dev_type - 1]++;

    /* set the network device type bit in the Multicast clients Flood map */
    dbridge_mdf_db.dmtTable[dsidIndex].mcastClientsFloodMap |= net_dev_type;

    /* add the DOCSIS Bridge ALT entry pointer into the Multicast Clients List */
    /* create new list entry*/
    if (CreateNewDbridgeMcastClientsEntry(&newEntry, dbridgeAltEntry))
    {
        DPRINTK(KERN_ERR"\nFailed to add Multicast client MAC Address  "
                        "%.2x-%.2x-%.2x-%.2x-%.2x-%.2x for DSID index %u\n",
                clientMacAddress[0],
                clientMacAddress[1],
                clientMacAddress[2],
                clientMacAddress[3],
                clientMacAddress[4],
                clientMacAddress[5], dsidIndex);
        DPRINTK(KERN_ERR"\nFailed to to create entry in the list\n");
        return -1;
    }

    /* insert the new entry */
    PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);
    list_add_tail(&newEntry->list, &(dbridge_mdf_db.dmtTable[dsidIndex].mcastClientsList));
    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);

#ifdef DBRIDGE_LOG
    if(dbridgeDbgLevel & DBRIDGE_DBG_MDF)
    {
        DPRINTK(KERN_INFO"\nInsert new entry to Multicast Clients List done\n");
    }
#endif

    /* increase the number of current Multicast Client MAC Addresses in the mcastClientsListof DSID indexes */
    dbridge_mdf_db.dmtTable[dsidIndex].mcastClientsNum++;

#ifdef DBRIDGE_LOG
    if(dbridgeDbgLevel & DBRIDGE_DBG_MDF)
    {
        DPRINTK(KERN_INFO"\nAdd Multicast client MAC Address  "
                "%.2x-%.2x-%.2x-%.2x-%.2x-%.2x for DSID index %u done\n",
                clientMacAddress[0],
                clientMacAddress[1],
                clientMacAddress[2],
                clientMacAddress[3],
                clientMacAddress[4],
                clientMacAddress[5], dsidIndex);
    }
#endif

    return 0;
}

/**************************************************************************/
/*! \fn static int DeleteMcastClientMacAddress(Uint32 dsidIndex, Uint8* clientMacAddress)                                    
 **************************************************************************
 *  \brief Delete Multicast Client MAC address from Docsis bridge MDF DB.
 *  \param[in] dsidIndex - the DSID index value.
 *  \param[in] clientMacAddress - the Multicast client MAC address value.
 *  \param[out] no output.
 *  \return OK or error status.
 **************************************************************************/
static int DeleteMcastClientMacAddress(Uint32 dsidIndex, Uint8* clientMacAddress)
{    
	int	ifindex = 0;
    int net_dev_type = 0;
    Uint32 i = 0;
    Uint32 lockKey = 0;
    DbridgeMcastClientsEntry_t *entry;

    if (dbridge_mdf_db.dmtTable[dsidIndex].mcastClientsNum == 0)
    {
        DPRINTK(KERN_ERR"\nFailed to delete Multicast client MAC Address  "
                        "%.2x-%.2x-%.2x-%.2x-%.2x-%.2x - Multicast Clients List is empty\n", 
                clientMacAddress[0],
                clientMacAddress[1],
                clientMacAddress[2],
                clientMacAddress[3],
                clientMacAddress[4],
                clientMacAddress[5]);
        return -1;
    }

    if (GetDbridgeMcastClientsEntryByMac(dsidIndex, clientMacAddress, &entry) == -1)
    {
        DPRINTK(KERN_ERR"\nMulticast client MAC Address %.2x-%.2x-%.2x-%.2x-%.2x-%.2x - "
                        "not found in Multicast Clients List\n", 
                clientMacAddress[0],
                clientMacAddress[1],
                clientMacAddress[2],
                clientMacAddress[3],
                clientMacAddress[4],
                clientMacAddress[5]);
        return -1;
    }

#ifdef DBRIDGE_LOG
    if(dbridgeDbgLevel & DBRIDGE_DBG_MDF)
    {
        DPRINTK(KERN_INFO"\nentry->dbridgeAltEntry->macLearningFrom = %u\n", entry->dbridgeAltEntry->macLearningFrom);
    }
#endif

    printk("\nentry->dbridgeAltEntry->macLearningFrom = %u\n", entry->dbridgeAltEntry->macLearningFrom);

    /* get Linux interface index from the ALT entry */
    if (entry->dbridgeAltEntry->sourceInterface)
    {
        ifindex = entry->dbridgeAltEntry->sourceInterface->ifindex;
    }
    else if (entry->dbridgeAltEntry->macLearningFrom == CPE_STATIC)
    {
        ifindex = 0;
    }
    else
    {
        ifindex = entry->dbridgeAltEntry->dbridgeDevice->ptr_interface->ifindex;
    }

#ifdef DBRIDGE_LOG
    if(dbridgeDbgLevel & DBRIDGE_DBG_MDF)
    {
        DPRINTK(KERN_INFO"\nLinux Interface Index (ifindex) = %u (0x%x)\n", ifindex, ifindex);
    }
#endif

    if (ifindex)
    {   
        /* decrease device index reference count for this interface */
        dbridge_mdf_db.dmtTable[dsidIndex].ifDevIndexRefCount[ifindex - 1]--;
    
        if (dbridge_mdf_db.dmtTable[dsidIndex].ifDevIndexRefCount[ifindex - 1] == 0)
        {
            /* unset the interface bit in the Multicast clients Linux CMIM bitmask */
            UNSET_BIT_MASK_64(dbridge_mdf_db.dmtTable[dsidIndex].mcastClientsLinuxCmim, ifindex - 1);
        }
    }
    else
    {
        /* decrease device index reference count for all interfaces */
        for (i = 0; i < DBR_MAX_ALL_INTERFACE; i++)
        {
            dbridge_mdf_db.dmtTable[dsidIndex].ifDevIndexRefCount[i]--;
        }

        /* unset all interfaces bits in the Multicast clients Linux CMIM bitmask */
        dbridge_mdf_db.dmtTable[dsidIndex].mcastClientsLinuxCmim = 0;
    }

    /* get network device type from the ALT entry */
    net_dev_type = entry->dbridgeAltEntry->dbridgeDevice->net_dev_type;

#ifdef DBRIDGE_LOG
    if(dbridgeDbgLevel & DBRIDGE_DBG_MDF)
    {
        DPRINTK(KERN_INFO"\nNetwork Device Type (net_dev_type) = %u (0x%x)\n", net_dev_type, net_dev_type);
    }
#endif

    /* decrease network device type reference count for this interface */
    dbridge_mdf_db.dmtTable[dsidIndex].ifNetDevTypeRefCount[net_dev_type - 1]--;

    if (dbridge_mdf_db.dmtTable[dsidIndex].ifNetDevTypeRefCount[net_dev_type - 1] == 0)
    {
        /* unset the network device type bit in the Multicast clients Flood map */
        dbridge_mdf_db.dmtTable[dsidIndex].mcastClientsFloodMap &= ~net_dev_type;
    }

    /* delete the Multicast Clients List entry */
    PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);
    list_del(&entry->list);
    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
    PAL_osMemFree(0, entry, sizeof(DbridgeMcastClientsEntry_t));

    /* decrease the number of current Multicast Client MAC Addresses in the mcastClientsListof DSID indexes */
    dbridge_mdf_db.dmtTable[dsidIndex].mcastClientsNum--;

#ifdef DBRIDGE_LOG
    if(dbridgeDbgLevel & DBRIDGE_DBG_MDF)
    {
        DPRINTK(KERN_INFO"\nDelete Multicast client MAC Address  "
                "%.2x-%.2x-%.2x-%.2x-%.2x-%.2x for DSID index %u done\n",
                clientMacAddress[0],
                clientMacAddress[1],
                clientMacAddress[2],
                clientMacAddress[3],
                clientMacAddress[4],
                clientMacAddress[5], dsidIndex);
    }
#endif
    
    return 0;
}


/**************************************************************************/
/*! \fn static int CreateNewDbridgeMcastClientsEntry(DbridgeMcastClientsEntry_t **entry, 
                                                     DbridgeAltEntry_t* dbridgeAltEntry)
 **************************************************************************
 *  \brief Create new entry in the Multicast Clients List.
 *  \param[out] entry - pointer to the allocated entry.
 *  \param[in] dbridgeAltEntry - pointer to the Docsis Bridge ALY entry.
 *  \return OK or error status.
 **************************************************************************/
static int CreateNewDbridgeMcastClientsEntry(DbridgeMcastClientsEntry_t **entry, DbridgeAltEntry_t* dbridgeAltEntry)
{
    PAL_Result res;

    res = PAL_osMemAlloc(0, sizeof(DbridgeMcastClientsEntry_t), 0, (Ptr *)entry);

    if (res != PAL_SOK) 
    {
        DPRINTK(KERN_ERR"\nFailed to allocate dynamic memory\n");
        return -1;
    }

    (*entry)->dbridgeAltEntry = dbridgeAltEntry;

#ifdef DBRIDGE_LOG
    if(dbridgeDbgLevel & DBRIDGE_DBG_MDF)
    {
        DPRINTK(KERN_INFO"\nCreate new entry in the Multicast Clients List done\n");
    }
#endif

    return 0;
}

/**************************************************************************/
/*! \fn static int GetDbridgeMcastClientsEntryByMac(Uint32 dsidIndex, Uint8* clientMacAddress, 
                                                    DbridgeMcastClientsEntry_t **entry)
 **************************************************************************
 *  \brief Find the Docsis Bridge Multicast Clients enrty according to the client MAC address.
 *  \param[in] dsidIndex - the DSID index value.
 *  \param[in] clientMacAddress - the client MAC address value. 
 *  \param[out] entry - pointer to the found entry.
 *  \return OK or error status.
 **************************************************************************/
static int GetDbridgeMcastClientsEntryByMac(Uint32 dsidIndex, Uint8* clientMacAddress, DbridgeMcastClientsEntry_t **entry)
{
    struct list_head *pos = NULL;
    DbridgeMcastClientsEntry_t *curr = NULL;

    /* find a place in the list according to the MAC address */
    list_for_each(pos, &(dbridge_mdf_db.dmtTable[dsidIndex].mcastClientsList))
    {
        curr = list_entry(pos, DbridgeMcastClientsEntry_t, list);

		if (memcmp ((void *)clientMacAddress, (void *)curr->dbridgeAltEntry->macAddress, MAC_ADDRESS_LEN) == 0)
        {
            break;
        }
    }

    if (pos != &(dbridge_mdf_db.dmtTable[dsidIndex].mcastClientsList)) 
    {
        *entry = curr;

#ifdef DBRIDGE_LOG
    if(dbridgeDbgLevel & DBRIDGE_DBG_MDF)
    {
        DPRINTK(KERN_INFO"\nDocsis Bridge Multicast Clients enrty for client MAC address "
                         "%.2x-%.2x-%.2x-%.2x-%.2x-%.2x is found\n",
                clientMacAddress[0],
                clientMacAddress[1],
                clientMacAddress[2],
                clientMacAddress[3],
                clientMacAddress[4],
                clientMacAddress[5]);
    }
#endif
        return 0;
    }

    return -1;
}

