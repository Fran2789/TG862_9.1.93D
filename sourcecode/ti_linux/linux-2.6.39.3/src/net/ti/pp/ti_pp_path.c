    /*
 * Copyright (C) <2008>, Texas Instruments, Incorporated
 *
 *  This program is free software; you can distribute it and/or modify it
 *  under the terms of the GNU General Public License (Version 2) as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *  for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
 */

#define _HIL_PP_PATH_C_

/*! \file ti_pp_path.c
    \brief
        This file contains the implementation of the PP Path counters (PPP).
        The PPP provides a service targeted at virtual network drives.
        It provides counters (In/Out Packets/Octets) for a virtual network
        drivers situated between accelerated physical network drivers.
*/

/**************************************************************************/
/*      INCLUDES:                                                         */
/**************************************************************************/

#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/list.h>
#include <linux/string.h>
#include <linux/if.h>
#include <linux/proc_fs.h>
#include <asm-arm/arch-avalanche/generic/_tistdtypes.h>
#include <asm-arm/arch-avalanche/generic/pal.h>
#include "linux/ti_pp_path.h"

#ifdef CONFIG_MACH_PUMA6
#include <asm-arm/arch-avalanche/generic/avalanche_pp_api.h>
#else
#include "linux/ti_ppm.h"
#endif



/**************************************************************************/
/*      EXTERNS Declaration:                                              */
/**************************************************************************/

/**************************************************************************/
/*      DEFINES:                                                          */
/**************************************************************************/

#define DLOG(fmt, ...) if (PPP_enableDebug) printk(KERN_NOTICE "PPP: %s - %d: " fmt "\n", __func__, __LINE__, ## __VA_ARGS__)
#define WLOG(fmt, ...) printk(KERN_WARNING "PPP: %s - %d: " fmt "\n", __func__, __LINE__, ## __VA_ARGS__)
#define ELOG(fmt, ...) printk(KERN_ERR "PPP: %s - %d: "  fmt "\n", __func__, __LINE__, ## __VA_ARGS__)

#ifdef CONFIG_MACH_PUMA6
    #define PPP_MAX_SESSIONS    AVALANCHE_PP_MAX_ACCELERATED_SESSIONS
#else
    #define PPP_MAX_SESSIONS    TI_PP_MAX_ACCLERABLE_SESSIONS
#endif 

#define PPP_IS_LEGAL_SESSION_HANDLE(s) ((s) < PPP_MAX_SESSIONS)

#define PPP_IS_LEGAL_PATHDIR(p) (((p) == PPP_PATHDIR_IN) || ((p) == PPP_PATHDIR_OUT))
#define PPP_SESSION_HANDLE_FROM_PTR(p) ((p) - PPP_sessions)

#define TOKENIZE(varN, sepS, startP, endP, errS) \
    /* Parse line */                             \
    varN = startP;                               \
    /* Skip sep */                               \
    varN += strspn(varN, sepS);                  \
    /* Find end of token */                      \
    endP = varN + strcspn(varN, sepS);           \
    if (varN == endP)                            \
    {                                            \
        ELOG(errS);                              \
        return -EFAULT;                          \
    }                                            \
    /* Terminate */                              \
    *endP = '\0';                                \
    DLOG("param : \"%s\"", varN);

/*! \var define PPP_PROT_DCL(_sav)
    \brief Declare a variable to hold context for multitasking protection
    \param[in] _sav: Name of context variable
*/
#define PPP_PROT_DCL(_sav) Uint32 _sav

/*! \var define PPP_PROT_ON(_sav)
    \brief Start DBR ALT multitasking protection
    \param[out] _sav: Place to save context to restore, must be declared using PPP_PROT_DCL
*/
#define PPP_PROT_ON(_sav) \
	PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &(_sav))

/*! \var define PPP_PROT_OFF(_sav)
    \brief End DBR ALT multitasking protection
    \param[in] _sav: Same variable used by the previous PPP_PROT_ON
*/
#define PPP_PROT_OFF(_sav) \
    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, _sav)

/**************************************************************************/
/*      LOCAL DECLARATIONS:                                               */
/**************************************************************************/

typedef unsigned int sessionHandle_t;

/*! \var typedef struct PPP_session_t
    \brief Session
*/
typedef struct
{
    PPP_pathDir_e dir;
    Bool multicast;
} PPP_session_t;

/*! \var typedef struct PPP_virtualDev_t
    \brief DB of (via) virtual devices
*/
typedef struct
{
    char virtualDev[IFNAMSIZ];
    struct net_device *viaDev;
    PPP_counters_t deadSessionCtr;
    PPP_session_t sessions[ PPP_MAX_SESSIONS ];
    struct list_head listVirtualDevs;           /* Element */
} PPP_virtualDev_t;

/*! \var typedef struct PPP_path_t
    \brief DB of paths
*/
typedef struct
{
    char rxFromDev[IFNAMSIZ];
    char txToDev[IFNAMSIZ];
    PPP_pathDir_e pathDir;
    PPP_session_t sessions[ PPP_MAX_SESSIONS ];
    PPP_virtualDev_t *virtualDev;
    struct list_head listPaths;                 /* Element */
} PPP_path_t;

/**************************************************************************/
/*      LOCAL VARIABLES:                                                  */
/**************************************************************************/

/* Root of list of paths */
static LIST_HEAD(PPP_headPaths);

/* Root of list of virtual devices */
static LIST_HEAD(PPP_headVirtualDevs);

/* PP event CB */
static unsigned int PPP_event_handler_handle;

/* procs */
static struct proc_dir_entry *pppProc;
static struct proc_dir_entry *pppProcDbg;

/* Debug */
static Bool PPP_enableDebug = False;

/* Local functions */
static PPP_path_t * PPP_GetPath(char *rxFrom, char *txTo, char *via);
static PPP_virtualDev_t * PPP_GetVirtDev(char *viaVirtualDev);
static int PPP_AddSession(sessionHandle_t sessionHandle, struct sk_buff *skb);
static int PPP_DelSession(sessionHandle_t sessionHandle, Uint64 sessionBytes, Uint32 sessionPkts);
static void PPP_event_handler(unsigned int event_id, unsigned int param1, unsigned int param2);
static int PPP_Show(char* buf, char **start, off_t offset, int count, int *eof, void *data);
static int PPP_WrProc (struct file *file, const char *buffer, unsigned long count, void *data);
static int PPP_TraverseVirtDev(char *viaVirtualDev, PPP_counters_t *outPppCtrs, int limitLen, char *outBuf);
static int PPP_WrProcDbg (struct file *file, const char *buffer, unsigned long count, void *data);
static int PPP_RdProcDbg(char* page, char **start, off_t offset, int count, int *eof, void *data);
static PPP_virtualDev_t *PPP_AddVirtDev(char *viaVirtualDev);
static int PPP_DelVirtDev(PPP_virtualDev_t *virtDev);
static int PPP_SumSession(Uint32 currSessionHandle, PPP_pathDir_e dir, Bool mcast, PPP_counters_t *localPppCtrs, int limitLen, char *outBuf, int *inLen);

/**************************************************************************/
/*      INTERFACE FUNCTIONS Implementation:                               */
/**************************************************************************/

/**************************************************************************/
/*! \fn int PPP_Init(void)
 **************************************************************************
 *  \brief Init the PPP module
 *  \return 0 (OK) / 1 (NOK)
 **************************************************************************/
int PPP_Init(void)
{
    /* Disable debug */
    PPP_enableDebug = False;

    /* Create procs */
    pppProc = create_proc_entry("ti_pp_path" ,0644, init_net.proc_net);
    if (pppProc == NULL)
    {
        ELOG("Unable to create the PPP proc entries");
        return -1;
    }
    pppProc->data = NULL;
    pppProc->read_proc = PPP_Show;
    pppProc->write_proc = PPP_WrProc;


    pppProcDbg = create_proc_entry("ti_pp_path_dbg" ,0644, init_net.proc_net);
    if (pppProcDbg == NULL)
    {
        ELOG("Unable to create the PPP DBG proc entries");
        return -1;
    }
    pppProcDbg->data = NULL;
    pppProcDbg->read_proc = PPP_RdProcDbg;
    pppProcDbg->write_proc = PPP_WrProcDbg;

    /* Register event */
#ifdef CONFIG_MACH_PUMA6
    if ( PP_RC_SUCCESS != avalanche_pp_event_handler_register( &PPP_event_handler_handle, PPP_event_handler ) )
#else
    PPP_event_handler_handle = ti_ppm_register_event_handler (PPP_event_handler);
    if (PPP_event_handler_handle == 0)
#endif
    {
        ELOG("Could not register PP event handler");
        return -1;
    }

    return 0;
}

/**************************************************************************/
/*! \fn int PPP_AddPath(char *rxFrom, char *txTo, char *viaVirtualDev, PPP_pathDir_e pathDir)
 **************************************************************************
 *  \brief Add a path
 *  \param[in] rxFrom - input device
 *  \param[in] txTo - output device
 *  \param[in] viaVirtualDev - virtual device through which the traffic flows
 *  \param[in] pathDir - does the count belong to the IN (to the box) or OUT (of the box)
 *  \return 0 (OK) / err (NOK)
 **************************************************************************/
int PPP_AddPath(char *rxFrom, char *txTo, char *viaVirtualDev, PPP_pathDir_e pathDir)
{
    PPP_PROT_DCL(lockKey);
    PPP_path_t *currPath;
    PPP_virtualDev_t *currVirtDev;
    Bool newPath = False;
    Bool newVirtDev = False;

    if ((rxFrom == NULL) || (txTo == NULL) || (viaVirtualDev == NULL) || !PPP_IS_LEGAL_PATHDIR(pathDir))
    {
        ELOG("Illegal param: rxFrom %p, txTo %p, viaVirtualDev %p, pathDir %d",
             rxFrom, txTo, viaVirtualDev, pathDir);
        return -1;
    }

    DLOG("Add path %s -> %s -> %s (%s)",
         rxFrom, viaVirtualDev, txTo,
         pathDir == PPP_PATHDIR_OUT ? "OUT" : "IN");

    PPP_PROT_ON(lockKey);

    /* Does the virtual device exist? */
    currVirtDev = PPP_GetVirtDev(viaVirtualDev);
    if (currVirtDev == NULL)
    {
        /* Virtual device does not exist, add */
        DLOG("virtDev does not exist, creating...");

        currVirtDev = PPP_AddVirtDev(viaVirtualDev);
        if (currVirtDev == NULL)
        {
            /* If created the path here, remove it */
            if (newPath)
            {
                /* Could not create complete path, cleanup */
                PPP_DelPath(rxFrom, txTo, viaVirtualDev);
            }
            PPP_PROT_OFF(lockKey);
            WLOG("Could not create virtual dev : rxFrom %s, txTo %s, virtDev %s", rxFrom, txTo, viaVirtualDev);
            return ENOMEM; 
        }

        newVirtDev = True;
    }

    /* Does the path exist? */
    currPath = PPP_GetPath(rxFrom, txTo, viaVirtualDev);
    if (currPath == NULL)
    {
        /* Path does not exist, add a new path */
        DLOG("Path does not exist, creating...");

        currPath = kmalloc(sizeof(*currPath), GFP_KERNEL);
        if (currPath == NULL)
        {
            if (newVirtDev)
            {
                PPP_DelVirtDev(currVirtDev);
            }
            PPP_PROT_OFF(lockKey); 
            /* Cannot alloc mem, reject */
            WLOG("Cannot kmalloc(%d, GPF_KERNEL) for path : rxFrom %s, txTo %s, virtDev %s",
                 sizeof(*currPath), rxFrom, txTo, viaVirtualDev);
            return ENOMEM;
        }

        /* Init path */
        strncpy(currPath->rxFromDev, rxFrom, sizeof(currPath->rxFromDev));
        strncpy(currPath->txToDev, txTo, sizeof(currPath->txToDev));
        currPath->pathDir = pathDir;
        memset(currPath->sessions, 0, sizeof(currPath->sessions));
        currPath->virtualDev = currVirtDev;

        /* Connect path to path list */
        list_add(&currPath->listPaths, &PPP_headPaths);

        newPath = True;
    }

    PPP_PROT_OFF(lockKey);

    /* Add path to virtual device */
    DLOG("Added path %s -> %s -> %s (%s)",
         rxFrom, viaVirtualDev, txTo,
         pathDir == PPP_PATHDIR_OUT ? "OUT" : "IN");

    return 0;
}

/**************************************************************************/
/*! \fn int PPP_DelPath(char *rxFrom, char *txTo, char *viaVirtualDev)
 **************************************************************************
 *  \brief Delete a path
 *  \param[in] rxFrom - device used as IN
 *  \param[in] txTo - device used as OUT
 *  \param[in] viaVirtualDev - virtual device through which the traffic flows
 *  \return 0 (OK) / 1 (NOK)
 **************************************************************************/
int PPP_DelPath(char *rxFrom, char *txTo, char *viaVirtualDev)
{
    PPP_PROT_DCL(lockKey);
    PPP_path_t *currPath;
    Uint32 currSessionHandle;
    PPP_virtualDev_t *currVirtDev;
    Bool used = False;

    if ((rxFrom == NULL) || (txTo == NULL) || (viaVirtualDev == NULL))
    {
        ELOG("Illegal param: rxFrom %p, txTo %p, viaVirtualDev %p",
             rxFrom, txTo, viaVirtualDev);
        return -1;
    }

    DLOG("Del path %s -> %s -> %s", rxFrom, viaVirtualDev, txTo);

    PPP_PROT_ON(lockKey);

    /* Does the path exist? */
    currPath = PPP_GetPath(rxFrom, txTo, viaVirtualDev);
    if (currPath != NULL)
    {
        /* Get virtual dev */
        currVirtDev = currPath->virtualDev; /* PPP_GetVirtDev(viaVirtualDev); */
        if (currVirtDev != NULL)
        {
            /* Go over all sessions in the path:
                - Sum into dead ctr
                - Remove from virtDev.
                    This assumes a session going thru a virtDev cannot belong to more than a single path going thru this virt device
                    This is true since a session's endpoints + the virtDev define the path
            */
            for (currSessionHandle = 0; currSessionHandle < ARRAY_SIZE(currPath->sessions); currSessionHandle++)
            {
                if (currPath->sessions[currSessionHandle].dir != PPP_PATHDIR_NONE)
                {
                    /* Sum */
                    PPP_SumSession(currSessionHandle, currVirtDev->sessions[currSessionHandle].dir, currVirtDev->sessions[currSessionHandle].multicast, &currVirtDev->deadSessionCtr, 0, NULL, NULL);
                    /* Remove from virtDev */
                    currVirtDev->sessions[currSessionHandle].dir = PPP_PATHDIR_NONE;
                    /* Remove from path */
                    currPath->sessions[currSessionHandle].dir = PPP_PATHDIR_NONE;
                }
            }
        }
        else
        {
            ELOG("%s(%s, %s, %s) --> Virtual device %s not connected to path %s - %s",
                 __func__, rxFrom, txTo, viaVirtualDev, viaVirtualDev, rxFrom, txTo);
            /* Continue anyway, since the rest is only cleanup */
        }

        /* Remove the path */
        /* DisConnect path from path list */
            list_del(&currPath->listPaths);
        /* Remove memory */
            kfree(currPath);

        /* Check if we can del the virt dev */
        list_for_each_entry(currPath, &PPP_headPaths, listPaths)
        {
            if ((currPath->virtualDev != NULL) && (strncmp(currPath->virtualDev->virtualDev, viaVirtualDev, sizeof(currPath->virtualDev->virtualDev)) == 0))
            {
                /* Cannot delete virt dev, still exists on path */
                DLOG("Cannot delete virt dev, still exists on path %s -> %s -> %s", currPath->rxFromDev, currPath->txToDev, currPath->virtualDev->virtualDev);
                used = True;
                break;
            }
        }
        if (!used)
        {
            /* No more paths via this dev, it can be removed */
            DLOG("No more paths via %s, delete", viaVirtualDev);
            PPP_DelVirtDev(currVirtDev);
        }

    }
    else
    {
        DLOG("Path does not exist: %s -> %s -> %s", rxFrom, viaVirtualDev, txTo);
    }

    PPP_PROT_OFF(lockKey);

    return 0;
}

/**************************************************************************/
/*! \fn int PPP_ReadCounters(char *viaVirtualDev, PPP_counters_t *pppCtrs)
 **************************************************************************
 *  \brief Read a path's counters
 *  \param[in] viaVirtualDev - virtual device through which the traffic flows
 *  \param[out] pppCtrs - Counters of PP traffic going through the virtual device
 *              in/out are provided according to the paths requested when
 *              adding the paths.
 *  \return 0 (OK) / 1 (NOK)
 **************************************************************************/
int PPP_ReadCounters(char *viaVirtualDev, PPP_counters_t *pppCtrs)
{
    PPP_PROT_DCL(lockKey);
    Bool savEnableDebug;

    if ((viaVirtualDev == NULL) || (pppCtrs == NULL))
    {
        ELOG("Illegal param: viaVirtualDev %p, pppCtrs %p", viaVirtualDev, pppCtrs);
        return -1;
    }

    PPP_PROT_ON(lockKey);

    /* Temporarily stop standard logs */
    savEnableDebug = PPP_enableDebug;
    PPP_enableDebug = False;

    PPP_TraverseVirtDev(viaVirtualDev, pppCtrs, 0, NULL);

    /* Restore logs */
    PPP_enableDebug = savEnableDebug;

    PPP_PROT_OFF(lockKey);

    return 0;
}

/**************************************************************************/
/*! \fn static int PPP_MarkPacket(struct sk_buff *skb, int dev)
 **************************************************************************
 *  \brief Mark (in the skb) that the packet flowed via the device
 *          Handles skb, no protection required
 *  \param[in] skb - skb that started the session
 *  \param[in] dev - the device index through which the packet is flowing
 *  \return 0 (OK) / !=1 (NOK)
 **************************************************************************/

int PPP_MarkPacket(struct sk_buff *skb, int dev)
{
    int currDevId = dev;
    struct net_device *currDev;

    if (skb == NULL)
    {
        ELOG("Illegal param: skb == NULL");
        return -1;
    }

    if (skb->pp_packet_info.ppp_packet_info.num_devs >= ARRAY_SIZE(skb->pp_packet_info.ppp_packet_info.devices))
    {
        /* No room for additional device */
        currDevId = dev;
        currDev = dev_get_by_index(&init_net, currDevId);
        ELOG("cannot add dev %d (%s) to skb (%p), ppp_packet_info.num_devs == %d would exceed max %d", 
             currDevId, currDev ? currDev->name : NULL, skb,
             skb->pp_packet_info.ppp_packet_info.num_devs, ARRAY_SIZE(skb->pp_packet_info.ppp_packet_info.devices));
        dev_put(currDev);
#ifdef PPP_DEBUG
        {
            int i;
            unsigned long long markTime = 0;
            unsigned long markTimeSec = 0;
            unsigned long markTimeUSec = 0;

            if (skb->pp_packet_info.ppp_packet_info.num_devs == ARRAY_SIZE(skb->pp_packet_info.ppp_packet_info.devices))
            {
                if (PPP_LogPacketCb)
                {
                    PPP_LogPacketCb(skb);
                }
                ELOG("ppp_packet_info.num_clones == %d", skb->pp_packet_info.ppp_packet_info.num_clones); 
                ELOG("ppp_packet_info.num_copies == %d", skb->pp_packet_info.ppp_packet_info.num_copies);
                #ifdef CONFIG_FTRACE
                    markTime = skb->pp_packet_info.ppp_packet_info.allocTime;
                #else
                    markTime = 0;
                #endif
                markTimeSec = markTime;
                markTimeUSec = do_div(markTimeSec, NSEC_PER_SEC);
                markTimeUSec /= 1000;
                ELOG("ppp_packet_info.allocTime == %lu.%06lu", markTimeSec, markTimeUSec);
                for (i = 0; i < ARRAY_SIZE(skb->pp_packet_info.ppp_packet_info.devices); i++)
                {
                    currDevId = skb->pp_packet_info.ppp_packet_info.devices[i];
                    currDev = dev_get_by_index(&init_net, currDevId);
                    {
                        #ifdef CONFIG_FTRACE
                            markTime = skb->pp_packet_info.ppp_packet_info.markTime[i];
                        #else
                            markTime = 0;
                        #endif
                        markTimeSec = markTime;
                        markTimeUSec = do_div(markTimeSec, NSEC_PER_SEC);
                        markTimeUSec /= 1000;
                        ELOG("Device[%d]: id %d, name %s, time %lu.%06lu", 
                             i, currDevId, currDev ? currDev->name : NULL,
                             markTimeSec, markTimeUSec);
                    }
                    dev_put(currDev);
                }
            }
        }
        /* See how many times we actually go thru here */
        skb->pp_packet_info.ppp_packet_info.num_devs++;
#endif
        return -1; 
    }

    /* Add device */
    /* Workaround for problem of MTA DHCP packets looping thru the system, ignore identical device  */
    if ((skb->pp_packet_info.ppp_packet_info.num_devs == 0) ||
        (skb->pp_packet_info.ppp_packet_info.devices[skb->pp_packet_info.ppp_packet_info.num_devs - 1] != dev))
    {
        /* New device --> add */
        skb->pp_packet_info.ppp_packet_info.devices[skb->pp_packet_info.ppp_packet_info.num_devs] = dev;
    #ifdef PPP_DEBUG
        skb->pp_packet_info.ppp_packet_info.clones[skb->pp_packet_info.ppp_packet_info.num_devs] = skb->pp_packet_info.ppp_packet_info.num_clones;
        skb->pp_packet_info.ppp_packet_info.copies[skb->pp_packet_info.ppp_packet_info.num_devs] = skb->pp_packet_info.ppp_packet_info.num_copies;
    #ifdef CONFIG_FTRACE
        skb->pp_packet_info.ppp_packet_info.markTime[skb->pp_packet_info.ppp_packet_info.num_devs] = sched_clock();
    #endif
        skb->pp_packet_info.ppp_packet_info.num_devs++;
        DLOG("Added dev %d to skb (%p), ppp_packet_info.num_devs == %d, ppp_packet_info.num_clones == %d, copies == %d", 
             currDevId, skb, skb->pp_packet_info.ppp_packet_info.num_devs, 
             skb->pp_packet_info.ppp_packet_info.num_clones,
             skb->pp_packet_info.ppp_packet_info.num_copies);
    #else
        skb->pp_packet_info.ppp_packet_info.num_devs++;
        DLOG("Added dev %d to skb (%p), ppp_packet_info.num_devs == %d", 
             currDevId, skb, skb->pp_packet_info.ppp_packet_info.num_devs);
    #endif
                                                                                                                        
    }
    return 0;
}


/**************************************************************************/
/*      LOCAL FUNCTIONS:                                                  */
/**************************************************************************/

/**************************************************************************/
/*! \fn static Bool PPP_IsExistPath(char *rxFrom, char *txTo, char *via)
 **************************************************************************
 *  \brief Get the requested path
 *
 *      MUST BE CALLED WITH PROTECTION ON
 *
 *  \param[in] rxFrom - input device
 *  \param[in] txTo - output device
 *  \param[in] via - (virtual) device through which the path passes
 *  \return PPP_path_t*, NULL when not exist
 **************************************************************************/
static PPP_path_t * PPP_GetPath(char *rxFrom, char *txTo, char *via)
{
    PPP_path_t *candPath; /* Candidate */

    if ((rxFrom == NULL) || (txTo == NULL) || (via == NULL))
    {
        ELOG("Illegal param: rxFrom %p, txTo %p, via %p", rxFrom, txTo, via);
        return NULL;
    }

    DLOG("Get path %s -> %s -> %s", rxFrom, via, txTo);

    /* Does the path exist? */
    list_for_each_entry(candPath, &PPP_headPaths, listPaths)
    {
        if ((candPath->rxFromDev[0] == '\0') || (candPath->txToDev[0] == '\0') || (candPath->virtualDev == NULL) || (candPath->virtualDev->virtualDev[0] == '\0'))
        {
            /* NULL device, error */
            ELOG("NULL device name on path %p : rxFrom \"%s\", txTo \"%s\", via_dev %p, via \"%s\"",
                 candPath, candPath->rxFromDev, candPath->txToDev, candPath->virtualDev, candPath->virtualDev ? candPath->virtualDev->virtualDev : NULL);
            /* Next */
            continue;
        }

        DLOG("Trying %s -> %s -> %s", candPath->rxFromDev, candPath->virtualDev->virtualDev, candPath->txToDev);

        /* Names seem OK */
        if ((strncmp(rxFrom, candPath->rxFromDev, sizeof(candPath->rxFromDev)) == 0) &&
            (strncmp(via, candPath->virtualDev->virtualDev, sizeof(candPath->virtualDev->virtualDev)) == 0) && 
            (strncmp(txTo, candPath->txToDev, sizeof(candPath->txToDev)) == 0))
        {
            /* Found existing path */
            DLOG("Found path %s -> %s -> %s : %p", candPath->rxFromDev, candPath->virtualDev->virtualDev, candPath->txToDev, candPath);
            return candPath;
        }
    }

    /* Path does not exist */
    DLOG("No such path %s -> %s -> %s", rxFrom, via, txTo);
    return NULL;
}

/**************************************************************************/
/*! \fn static PPP_virtualDev_t * PPP_AddVirtDev(char *viaVirtualDev)
 **************************************************************************
 *  \brief Create new virtual dev
 *
 *      MUST BE CALLED WITH PROTECTION ON
 *
 *  \param[in] viaVirtualDev - virtual device name
 *  \return PPP_virtualDev_t*, NULL when not exist
 **************************************************************************/
static PPP_virtualDev_t *PPP_AddVirtDev(char *viaVirtualDev)
{
    PPP_virtualDev_t *currVirtDev;
    struct net_device *viaDev;

    currVirtDev = kmalloc(sizeof(*currVirtDev), GFP_KERNEL);
    if (currVirtDev == NULL)
    {
        /* Cannot alloc mem, reject */
        WLOG("Cannot kmalloc(%d, GPF_KERNEL) for virtual device %s", sizeof(*currVirtDev), viaVirtualDev);
        return NULL;
    }
    viaDev = dev_get_by_name(&init_net, viaVirtualDev);
    if (viaDev == NULL)
    {
        WLOG("Virtual dev %s does not exist", viaVirtualDev);
        kfree(currVirtDev);
        return NULL;
    }

    /* Created virtual device, init and connect */
    strncpy(currVirtDev->virtualDev, viaVirtualDev, sizeof(currVirtDev->virtualDev));
    currVirtDev->viaDev = viaDev;
    PPP_ZERO_CTR(&currVirtDev->deadSessionCtr);
    memset(currVirtDev->sessions, 0, sizeof(currVirtDev->sessions));

    /* Add to list of virtual devices */
    list_add(&currVirtDev->listVirtualDevs, &PPP_headVirtualDevs);

    DLOG("Added virtual dev %s", viaVirtualDev);

    return currVirtDev;
}

/**************************************************************************/
/*! \fn static int PPP_DelVirtDev(PPP_virtualDev_t *virtDev)
 **************************************************************************
 *  \brief Delete virtual dev
 *
 *      MUST BE CALLED WITH PROTECTION ON
 *
 *  \param[in] virtDev - virtual device 
 *  \return PPP_virtualDev_t*, NULL when not exist
 **************************************************************************/
static int PPP_DelVirtDev(PPP_virtualDev_t *virtDev)
{
    if (virtDev == NULL)
    {
        WLOG("Illegal param: vrtDev == NULL");
        return -1;
    }

    DLOG("Deleting virtual dev %s", virtDev->virtualDev);

    /* Remove from list of virtual devs */
    list_del(&virtDev->listVirtualDevs);

    dev_put(virtDev->viaDev);

    /* Free element */
    kfree(virtDev);

    return 0;
}

/**************************************************************************/
/*! \fn static PPP_virtualDev_t * PPP_GetVirtDev(char *viaVirtualDev)
 **************************************************************************
 *  \brief Get the requested virtual dev
 *
 *      MUST BE CALLED WITH PROTECTION ON
 *
 *  \param[in] viaVirtualDev - virtual device name
 *  \return PPP_virtualDev_t*, NULL when not exist
 **************************************************************************/
static PPP_virtualDev_t *PPP_GetVirtDev(char *viaVirtualDev)
{
    PPP_virtualDev_t *candVirtDev; /* Candidate */

    if (viaVirtualDev == NULL)
    {
        ELOG("Illegal param: viaVirtualDev %p", viaVirtualDev);
        return NULL;
    }

    DLOG("Get virtDev %s", viaVirtualDev);

    list_for_each_entry(candVirtDev, &PPP_headVirtualDevs, listVirtualDevs)
    {
        if (candVirtDev->virtualDev[0] == '\0')
        {
            /* NULL device, error */
            ELOG("NULL device name on virtual device list element %p", candVirtDev);

            /* Next */
            continue;
        }

        DLOG("Trying %s", candVirtDev->virtualDev);

        /* Names seem OK */
        if (strncmp(viaVirtualDev, candVirtDev->virtualDev, sizeof(candVirtDev->virtualDev)) == 0)
        {
            /* Found existing path */
            DLOG("Found virtDev %s : %p", viaVirtualDev, candVirtDev);
            return candVirtDev;
        }
    }

    /* Device does not exist */
    DLOG("No such virtDev %s", viaVirtualDev);
    return NULL;
}

/**************************************************************************/
/*! \fn static int PPP_AddSession(sessionHandle, skb)
 **************************************************************************
 *  \brief Add a session
 *  \param[in] sessionHandle - Handle to the created session
 *  \param[in] skb - skb that started the session
 *  \return 0 (OK) / 1 (NOK)
 **************************************************************************/
static int PPP_AddSession(sessionHandle_t sessionHandle, struct sk_buff *skb)
{
    PPP_PROT_DCL(lockKey);
    PPP_path_t *currPath = NULL;
    struct net_device *input_dev = NULL;
    Bool multicast;
    int ret_val;
    int i;
    struct net_device *curr_virt_dev;

    if (!PPP_IS_LEGAL_SESSION_HANDLE(sessionHandle) || (skb == NULL))
    {
        ELOG("Illegal param: sessionHandler %d, skb %p", sessionHandle, skb);
        return -1;
    }

    if (skb->skb_iif == 0)
    {
        ELOG("Session %d not added: Missing info in skb: skb->iif is NULL", sessionHandle);
        return -1;
    }

    DLOG("Add session %d skb %p", sessionHandle, skb);

    PPP_PROT_ON(lockKey);

#ifdef CONFIG_MACH_PUMA6
    if (skb->pp_packet_info.pp_session.ingress.lookup.LUT1.u.fields.L2.entry_type != AVALANCHE_PP_LUT_ENTRY_L2_ETHERNET)
    {
        PPP_PROT_OFF(lockKey);
        ELOG("Session %d not added : Ingress packet type (%d) is not AVALANCHE_PP_LUT_ENTRY_L2_ETHERNET (%d), cannot determine Multicast", 
             sessionHandle, skb->pp_packet_info.pp_session.ingress.lookup.LUT1.u.fields.L2.entry_type, AVALANCHE_PP_LUT_ENTRY_L2_ETHERNET);
        return -1;
    }
#else
    if (skb->pp_packet_info.ti_session.ingress.l2_packet.packet_type != TI_PP_ETH_TYPE)
    {
        PPP_PROT_OFF(lockKey);
        ELOG("Session %d not added : Ingress packet type (%d) is not TI_PP_ETH_TYPE (%d), cannot determine Multicast", 
             sessionHandle, skb->pp_packet_info.ti_session.ingress.l2_packet.packet_type, TI_PP_ETH_TYPE);
        return -1;
    }
#endif

    input_dev = dev_get_by_index(&init_net, skb->skb_iif);

    /* Is there a path that matches this session? */
    if (input_dev)
    {
        /* Multicast? */
#ifdef CONFIG_MACH_PUMA6
        if ((skb->pp_packet_info.pp_session.ingress.lookup.LUT1.u.fields.L2.dstmac[0] & 0x01) != 0)
#else
        if ((skb->pp_packet_info.ti_session.ingress.l2_packet.u.eth_desc.dstmac[0]    & 0x01) != 0)
#endif 
        {
            /* Multicast! */
            multicast = True;
        }
        else
        {
            /* Unicast! */
            multicast = False;
        }

        /* Get paths through all devices in the skb */
        for (i = 0; i < skb->pp_packet_info.ppp_packet_info.num_devs; i++)
        {
            curr_virt_dev = dev_get_by_index(&init_net, skb->pp_packet_info.ppp_packet_info.devices[i]);
            if (curr_virt_dev == NULL)
            {
                ELOG("Net device %d marked in skb but does not exist", skb->pp_packet_info.ppp_packet_info.devices[i]);
                continue;
            }

            /* Get path */
            currPath = PPP_GetPath(input_dev->name, skb->dev->name, curr_virt_dev->name);
            if (currPath != NULL)
            {
        /* Path exists */

                /* Add session to lists */
                currPath->sessions[sessionHandle].dir = currPath->pathDir;
                currPath->sessions[sessionHandle].multicast = multicast;
                currPath->virtualDev->sessions[sessionHandle].dir = currPath->pathDir;
                currPath->virtualDev->sessions[sessionHandle].multicast = multicast; 

                DLOG("Added session %d on path %s --> %s --> %s dir %d", sessionHandle, input_dev->name, curr_virt_dev->name, skb->dev->name, currPath->pathDir);
            }
            else
            {
                DLOG("Session %d (skb %p, devid %d) not added on path, no such path: %s --> %s --> %s", 
                     sessionHandle, skb, skb->pp_packet_info.ppp_packet_info.devices[i], input_dev->name, curr_virt_dev->name, skb->dev->name);
            }

            dev_put(curr_virt_dev);
        }
    }

    if (input_dev)
    {
        dev_put(input_dev);
    }

    PPP_PROT_OFF(lockKey);

    return 0;
}

/**************************************************************************/
/*! \fn static int PPP_DelSession(sessionHandle, sessionBytes, sessionPkts)
 **************************************************************************
 *  \brief Add a session
 *  \param[in] sessionHandle - Handle to the created session
 *  \param[in] sessionBytes, sessionPkts - counetrs of deleted session
 *  \return 0 (OK) / 1 (NOK)
 **************************************************************************/
static int PPP_DelSession(sessionHandle_t sessionHandle, Uint64 sessionBytes, Uint32 sessionPkts)
{
    PPP_PROT_DCL(lockKey);
    PPP_path_t *currPath;
    PPP_virtualDev_t *currVirtualDev;

    if (!PPP_IS_LEGAL_SESSION_HANDLE(sessionHandle))
    {
        ELOG("Illegal param: sessionHandler %d", sessionHandle);
        return -1;
    }

    DLOG("Del session %d", sessionHandle);

    PPP_PROT_ON(lockKey);

    /* Look for session in all paths, if found:
        - proceed to via, update dead counters and remove
        - remove from path 
    */
    list_for_each_entry(currPath, &PPP_headPaths, listPaths)
    {
        /* Does session exist on this path */
        if (currPath->sessions[sessionHandle].dir != PPP_PATHDIR_NONE)
        {
            /* Exists */

            /* Handle via */
            currVirtualDev = currPath->virtualDev;
            /* Update dead ctr */
            PPP_ADD_1CTR_oct_pckt(PPP_1CTR(&currVirtualDev->deadSessionCtr, currPath->sessions[sessionHandle].dir, currPath->sessions[sessionHandle].multicast), sessionBytes, sessionPkts);
            /* Remove from via */
            currVirtualDev->sessions[sessionHandle].dir = PPP_PATHDIR_NONE;

            /* Remove from path */
            currPath->sessions[sessionHandle].dir = PPP_PATHDIR_NONE;
        }
    }

    PPP_PROT_OFF(lockKey);

    return 0;
}

/**************************************************************************/
/*! \fn static void PPP_event_handler(unsigned int event_id, unsigned int param1, unsigned int param2)
 **************************************************************************
 *  \brief Handle PP events
 *  \param[in] event id
 *  \param[in] param1, param2 - change according to the event type.
 *  \return 0 (OK) / 1 (NOK)
 **************************************************************************/
static void PPP_event_handler(unsigned int event_id, unsigned int param1, unsigned int param2)
{
    unsigned int sessionHandle;
    struct sk_buff *skb;

    switch (event_id)
    {
#ifdef CONFIG_MACH_PUMA6
        case PP_EV_SESSION_CREATED:
#else
        case TI_PPM_SESSION_CREATED:
#endif
        {
            /* param1 - sessionHandle, param2 - skb */
            sessionHandle = param1;
            skb = (struct sk_buff*)param2;
            PPP_AddSession(sessionHandle, skb);
            break;
        }

#ifdef CONFIG_MACH_PUMA6
        case PP_EV_SESSION_DELETED:
        {
            AVALANCHE_PP_SESSION_STATS_t *  ppStats = (AVALANCHE_PP_SESSION_STATS_t *)param2;

            /* param1 - sessionHandle, param2 - ppStats */
            sessionHandle = param1;

            PPP_DelSession( sessionHandle, ppStats->bytes_forwarded, ppStats->packets_forwarded );
            break;
        }
#else
        case TI_PPM_SESSION_DELETED:
        {
            TI_PP_SESSION_STATS *ppStats = (TI_PP_SESSION_STATS*)param2;
            Uint64 uint64var;

            /* param1 - sessionHandle, param2 - ppStats */
            sessionHandle = param1;

            /* Bytes forwarded */
            uint64var = ppStats->bytes_forwarded_hi;
            uint64var <<= 32;
            uint64var += ppStats->bytes_forwarded_lo;

            PPP_DelSession( sessionHandle, uint64var, ppStats->packets_forwarded );
            break;
        }
#endif
        default:
        {
            /* Silently ignore everything else */
            break;
        }
    }

    /* Our work is done. */
    return;
}

/**************************************************************************/
/*! \fn static int PPP_SumSession(Uint32 currSessionHandle, PPP_pathDir_e dir, Bool mcast, PPP_counters_t *localPppCtrs, int limitLen, char *outBuf, int *inLen)
 **************************************************************************
 *  \brief Sdd session counters
 *  \param[in] currSessionHandle - session handle
 *  \param[in] dir - direction of session
 *  \param[in] mcast - Multicast
 *  \param[in,out] localPppCtrs - counters, add to these
 *  \param[in] limitLen - max len allowed in outBuf
 *  \param[out] outBuf - output buffer to write output
 *  \param[in,out] inLen - in: start point in outBuf, out: end point in outBuf
 *  \return 0 - OK, -1 - NOK
 **************************************************************************/
static int PPP_SumSession(Uint32 currSessionHandle, PPP_pathDir_e dir, Bool mcast, PPP_counters_t *localPppCtrs, int limitLen, char *outBuf, int *inLen)
{
    Uint64 var64;
#ifdef CONFIG_MACH_PUMA6
    AVALANCHE_PP_SESSION_STATS_t    sessionStats;
#else
    TI_PP_SESSION_STATS sessionStats;
#endif
    int len;

    if (!PPP_IS_LEGAL_SESSION_HANDLE(currSessionHandle) || !PPP_IS_LEGAL_PATHDIR(dir) || (localPppCtrs == NULL))
    {
        ELOG("Illegal params: currSessionHandle %d, dir %d, localPppCtrs %p", currSessionHandle, dir, localPppCtrs);
        return -1;
    }

    /* Get running session counters */
#ifdef CONFIG_MACH_PUMA6
    if (PP_RC_SUCCESS != avalanche_pp_get_stats_session(currSessionHandle, &sessionStats))
#else
    if (ti_ppm_get_session_stats(currSessionHandle, &sessionStats) < 0)
#endif
    {
        /* Something is wrong */
        ELOG("Could not get stats for session %d", currSessionHandle);
        return -1;
    }

    DLOG("Add live session %d", currSessionHandle);

#ifdef CONFIG_MACH_PUMA6
    var64 = sessionStats.bytes_forwarded;
#else
    var64 = sessionStats.bytes_forwarded_hi;
    var64 <<= 32;
    var64 += sessionStats.bytes_forwarded_lo;
#endif

    PPP_ADD_1CTR_oct_pckt(PPP_1CTR(localPppCtrs, dir, mcast), var64, sessionStats.packets_forwarded);

    if ((outBuf != NULL) && (inLen != NULL))
    {
        len = *inLen;
        len += snprintf(outBuf+len, limitLen-len, "\t\tSession %d: " PPP_DISPLAY_FORMAT_1CTR "\n", 
                        currSessionHandle, PPP_DISPLAY_ELEMENTS_1CTR(PPP_1CTR(localPppCtrs, dir, mcast)));
        *inLen = len;
    }

    return 0;
}

/**************************************************************************/
/*! \fn static int PPP_SumSessionsVirtDev(PPP_virtualDev_t *currVirtDev, PPP_counters_t *localPppCtrs, int limitLen, char *outBuf, int *inLen)
 **************************************************************************
 *  \brief Sdd session counters
 *  \param[in] currVirtDev - Virtual dev to sum
 *  \param[in,out] localPppCtrs - counters, add to these
 *  \param[in] limitLen - max len allowed in outBuf
 *  \param[out] outBuf - output buffer to write output
 *  \param[in,out] inLen - in: start point in outBuf, out: end point in outBuf
 *  \return 0 - OK, -1 - NOK
 **************************************************************************/
static int PPP_SumSessionsVirtDev(PPP_virtualDev_t *currVirtDev, PPP_counters_t *localPppCtrs, int limitLen, char *outBuf, int *inLen)
{
    Uint32 currSessionHandle;

    if (localPppCtrs == NULL)
    {
        ELOG("Illegal params: localPppCtrs %p", localPppCtrs);
        return -1;
    }

    for (currSessionHandle = 0; currSessionHandle < ARRAY_SIZE(currVirtDev->sessions); currSessionHandle++)
    {
        if (currVirtDev->sessions[currSessionHandle].dir != PPP_PATHDIR_NONE)
        {
            PPP_SumSession(currSessionHandle, currVirtDev->sessions[currSessionHandle].dir, currVirtDev->sessions[currSessionHandle].multicast, localPppCtrs, limitLen, outBuf, inLen);
        }
    }

    return 0;
}



/**************************************************************************/
/*! \fn static int PPP_TraverseVirtDev(char *viaVirtualDev, PPP_counters_t *outPppCtrs, int limitLen, char *outBuf)
 **************************************************************************
 *  \brief Accumulate counters for a virtual dev
 *          Output can be as counters and/or printout
 *  \param[in] viaVirtualDev - virtual device through which the traffic flows
 *  \param[out] outPppCtrs - Counters of PP traffic going through the virtual device
 *              in/out are provided according to the paths requested when
 *              adding the paths.
 *
 *      MUST BE CALLED WITH PROTECTION ON
 *
 *  \param[in] limitLen - max len allowed in outBuf
 *  \param[out] outBuf - output buffer to write output
 *  \return 0 (OK) / <1 NOK / >1 len (when using outBuf)
 **************************************************************************/
static int PPP_TraverseVirtDev(char *viaVirtualDev, PPP_counters_t *outPppCtrs, int limitLen, char *outBuf)
{
    PPP_virtualDev_t *currVirtDev;
    PPP_counters_t localPppCtrs;
    int len;
    int currSess;

    if ((viaVirtualDev == NULL) ||
        ((outPppCtrs == NULL) && ((outBuf == NULL) || (limitLen == 0))) )
    {
        ELOG("Illegal param: viaVirtualDev %p, pppCtrs %p, buf %p, limit len %d", viaVirtualDev, outPppCtrs, outBuf, limitLen);
        return -1;
    }

    DLOG("Read counters for virtDev %s", viaVirtualDev);

        /* Init val to 0 */
    PPP_ZERO_CTR(&localPppCtrs);

    len = 0;

    /* Get the device */
    currVirtDev = PPP_GetVirtDev(viaVirtualDev);
    if (currVirtDev != NULL)
    {
        /* Dev exists */
        DLOG("Found virtDev %s : %p", viaVirtualDev, currVirtDev);

        if (outBuf != NULL)
        {
            len += snprintf(outBuf+len, limitLen - len, "virtDev : %s\n", currVirtDev->virtualDev);
        }

        /* Sum all sessions on this device */
        PPP_SumSessionsVirtDev(currVirtDev, &localPppCtrs, limitLen, outBuf, &len); 

            /* Dead sessions */
        PPP_ADD_CTRS(&localPppCtrs, &currVirtDev->deadSessionCtr);

            if (outBuf != NULL)
            {
            len += snprintf(outBuf+len, limitLen-len, "\t\tDead sessions: " PPP_DISPLAY_FORMAT_CTRS("\t\t\t") "\n", 
                            PPP_DISPLAY_ELEMENTS_CTRS(&currVirtDev->deadSessionCtr));

            len += snprintf(outBuf+len, limitLen-len, "\t\tTotal: " PPP_DISPLAY_FORMAT_CTRS("\t\t\t") "\n", PPP_DISPLAY_ELEMENTS_CTRS(&localPppCtrs));
            /* Sessions */
            len += snprintf(outBuf+len, limitLen-len, "\t\t\tSessions: ");
            for (currSess = 0; currSess < ARRAY_SIZE(currVirtDev->sessions); currSess++)
            {
                if (currVirtDev->sessions[currSess].dir != PPP_PATHDIR_NONE)
                {
                    len += snprintf(outBuf+len, limitLen-len, "%d ", currSess);
                }
            }
            len += snprintf(outBuf+len, limitLen-len, "\n");
        }
    }

    if (outPppCtrs != NULL)
    {
        *outPppCtrs = localPppCtrs;
    }

    DLOG("Read virDev %s - done", viaVirtualDev);

    return len;
}

/**************************************************************************/
/*! \fn static int PPP_Show(char* buf, char **start, off_t offset, int count, int *eof, void *data)
 **************************************************************************
 *  \brief Show PPP DB
 *  \param[in] read proc params
 *  \return len of data / -1 (NOK)
 **************************************************************************/
static int PPP_Show(char* page, char **start, off_t offset, int count, int *eof, void *data)
{
    PPP_PROT_DCL(lockKey);
    int len=0;
    PPP_path_t *currPath;
    PPP_virtualDev_t *currVirtDev;
    int limit = count;
    char* buf = page + offset;
    int currSess;

    PPP_PROT_ON(lockKey);

    /* Print all paths */
    len += snprintf(buf+len, limit-len, "%-15s %-15s %-15s %s\n", "rxFrom", "via", "txTo", "sessions");
    len += snprintf(buf+len, limit-len, "%-15s %-15s %-15s %s\n", "---------------", "---------------", "---------------", "---------------...");
    list_for_each_entry(currPath, &PPP_headPaths, listPaths)
    {
        /* Path */
        len += snprintf(buf+len, limit-len, "%-15s %-15s %-15s ", currPath->rxFromDev, currPath->virtualDev->virtualDev, currPath->txToDev);
        /* Sessions */
        for (currSess = 0; currSess < ARRAY_SIZE(currPath->sessions); currSess++)
        {
            if (currPath->sessions[currSess].dir != PPP_PATHDIR_NONE)
            {
                len += snprintf(buf+len, limit-len, "%d ", currSess);
            }
        }
        len += snprintf(buf+len, limit-len, "\n");
    }
    len += snprintf(buf+len, limit-len, "\n");

    /* Print all virt devs */
    list_for_each_entry(currVirtDev, &PPP_headVirtualDevs, listVirtualDevs)
    {
        len += PPP_TraverseVirtDev(currVirtDev->virtualDev, NULL, limit-len, buf+len);
    }

    PPP_PROT_OFF(lockKey);

    *eof = 1;
    return len;
}

/**************************************************************************/
/*! \fn static int PPP_WrProc (struct file *file, const char *buffer, unsigned long count, void *data)
 **************************************************************************
 *  \brief Handle WR proc
 *  \param[in] write proc parsm
 *              buffer: add/del rxFromDev txToDev viaVirtualDev
 *  \return count / -(1) (NOK)
 **************************************************************************/
static int PPP_WrProc (struct file *file, const char *buffer, unsigned long count, void *data)
{
    char pppCmd[100]; /* add/del==3 + \b + 3 * (IFNAMSIZ + \b) + in/out==3 ==> 55 */
    static const char *sep = " \t,\n";
    char *cmd;
    char *rxFrom;
    char *txTo;
    char *viaVirtualDev;
    char *dirStr;
    int  ret;
    char *endtok;
    PPP_pathDir_e pathDir;

    ret = count;

    /* Validate the length of data passed. */
    if (count > (sizeof(pppCmd) - 1))
        count = sizeof(pppCmd) - 1;

    /* Initialize the buffer before using it. */
    memset(pppCmd, 0, sizeof(pppCmd));

    /* Copy from user space. */
    if (copy_from_user(pppCmd, buffer, count))
        return -EFAULT;
    pppCmd[count] = '\0';

    /* Parse line */
    /* cmd */
    TOKENIZE(cmd, sep, pppCmd, endtok, "No command [add/del]");
    /* rxFrom */
    TOKENIZE(rxFrom, sep, endtok + 1, endtok, "No rxFrom device");
    /* txTo */
    TOKENIZE(txTo, sep, endtok + 1, endtok, "No txTo device");
    /* virtDev */
    TOKENIZE(viaVirtualDev, sep, endtok + 1, endtok, "No virtual device");

    /* Command Handlers */
    if (strcmp(cmd, "add") == 0)
    {
        /* direction */
        TOKENIZE(dirStr, sep, endtok + 1, endtok, "No direction");
        if (strcmp(dirStr, "in") == 0)
        {
            pathDir = PPP_PATHDIR_IN;
        }
        else if (strcmp(dirStr, "out") == 0)
        {
            pathDir = PPP_PATHDIR_OUT;
        }
        else
        {
            ELOG("Illegal pathDir \"%s\" [in/out]", dirStr);
            return -EFAULT;
        }

        PPP_AddPath(rxFrom, txTo, viaVirtualDev, pathDir);
        return ret;
    }
    else if (strcmp(cmd, "del") == 0)
    {
        PPP_DelPath(rxFrom, txTo, viaVirtualDev);
        return ret;
    }
    else
    {
        ELOG("Illegal command %s [add/del]", cmd);
        return -EFAULT;
    }
}

/**************************************************************************/
/*! \fn static int PPP_WrProcDbg (struct file *file, const char *buffer, unsigned long count, void *data)
 **************************************************************************
 *  \brief Enable/Disable debug
 *  \param[in] write proc parsm
 *  \return count / -(1) (NOK)
 **************************************************************************/
static int PPP_WrProcDbg (struct file *file, const char *buffer, unsigned long count, void *data)
{
    char pppDbg[2]; /* 1/0 */
    int  ret;

    ret = count;

    /* Validate the length of data passed. */
    if (count > (sizeof(pppDbg) - 1))
        count = sizeof(pppDbg) - 1;

    /* Initialize the buffer before using it. */
    memset(pppDbg, 0, sizeof(pppDbg));

    /* Copy from user space. */
    if (copy_from_user(pppDbg, buffer, count))
        return -EFAULT;
    pppDbg[count] = '\0';

    if (pppDbg[0] == '1')
    {
        /* Enable debug */
        PPP_enableDebug = True;
    }
    else
    {
        /* Enable debug */
        PPP_enableDebug = False;
    }

    return ret;
}

/**************************************************************************/
/*! \fn static int PPP_RdProcDbg(char* buf, char **start, off_t offset, int count, int *eof, void *data)
 **************************************************************************
 *  \brief Show PPP Debug
 *  \param[in] read proc params
 *  \return len of data / -1 (NOK)
 **************************************************************************/
static int PPP_RdProcDbg(char* page, char **start, off_t offset, int count, int *eof, void *data)
{
    int len=0;
    int limit = count;
    char* buf = page + offset;

    len += snprintf(buf+len, limit-len, "PPP Debug : %s (%d)\n", PPP_enableDebug ? "Enabled" : "Disabled", PPP_enableDebug);

    *eof = 1;
    return len;
}

EXPORT_SYMBOL(PPP_AddPath);
EXPORT_SYMBOL(PPP_DelPath);
EXPORT_SYMBOL(PPP_ReadCounters);
EXPORT_SYMBOL(PPP_MarkPacket);

