/*
  GPL LICENSE SUMMARY

  Copyright(c) 2014 Intel Corporation.

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
  The full GNU General Public License is included in this distribution
  in the file called LICENSE.GPL.

  Contact Information:
    Intel Corporation
    2200 Mission College Blvd.
    Santa Clara, CA  97052
*/

#include <asm-arm/arch-avalanche/generic/avalanche_pp_api.h>
#include <asm-arm/arch-avalanche/generic/avalanche_pdsp_api.h>
#include <asm-arm/arch-avalanche/puma6/puma6.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/list.h>
#include <linux/in.h>
#include "pp_db.h"
#include "pp_hal.h"
#include <pal.h>

/* ARRIS ADD START */
#ifndef CONFIG_MACH_PUMA5
#include <arris/arris_mod_api.h>

ARRISMOD_DEFINE( int, arris_rip_create_pp_session_hook_v2, AVALANCHE_PP_SESSION_INFO_t*, void* );
ARRISMOD_EXTERN( int, arris_rip_create_pp_session_hook_v2, AVALANCHE_PP_SESSION_INFO_t*, void* );
#endif
/* ARRIS ADD END */


static AVALANCHE_PP_RET_e    __avalanche_pp_pid_delete          ( Uint8     pid_handle );
static AVALANCHE_PP_RET_e    __avalanche_pp_vpid_delete         ( Uint8     vpid_handle );
static AVALANCHE_PP_RET_e    __avalanche_pp_session_delete      ( Uint32    session_handle,  AVALANCHE_PP_SESSION_STATS_t *  ptr_session_stats );
static AVALANCHE_PP_RET_e    __avalanche_pp_counter64_read      ( Uint64 *  dest,  volatile Ptr src );
static AVALANCHE_PP_RET_e    __avalanche_pp_flush_single_session( AVALANCHE_PP_SESSION_INFO_t *  ptr_session, Ptr     data );

#define PP_DB_CHECK_ACTIVE_UNDER_LOCK()         \
    if (PP_DB.status != PP_DB_STATUS_ACTIVE)    \
    {                                           \
        printk("ERROR: PP Operation %s cannot be accomplished while PP status is %s\n", __FUNCTION__, PP_DB.status == PP_DB_STATUS_UNINITIALIZED ? "INACTIVE" : "PPM_PSM");     \
        PP_DB_UNLOCK();                         \
        return (PP_RC_FAILURE);                 \
    }                                           


#define PP_DB_CHECK_ACTIVE()            \
{                                       \
    PP_DB_LOCK();                       \
    PP_DB_CHECK_ACTIVE_UNDER_LOCK();    \
    PP_DB_UNLOCK();                     \
}


#define PRECMD_INDEX_SHIFT        0
#define PRECMD_COMMAND_SHIFT      8

#define PRECMD_INDEX_MASK         (0xFFu << PRECMD_INDEX_SHIFT)
#define PRECMD_COMMAND_MASK       (0xFFu << PRECMD_COMMAND_SHIFT)
#define PRECMD_COMMAND(x)         (((x) << PRECMD_COMMAND_SHIFT) & PRECMD_COMMAND_MASK)
#define PRECMD_INDEX(x)           (((x) << PRECMD_INDEX_SHIFT)& PRECMD_INDEX_MASK)

#define WORD_S0_B1_0(s0, b1, b0)  (((Uint16)s0 << 16) | ((Uint8)b1 <<  8)| ((Uint8)b0))


/* ========================================================================================= */
/*                                                                                           */
/*                                                                                           */
/*                    EVENT POLL TIMER                                                       */
/*                                                                                           */
/*                                                                                           */
/* ========================================================================================= */
#define PP_TDOX_EVALUATION_PERIOD_IN_SEC           (1)
#define PP_TDOX_EVALUATION_ONCE_IN_X_TIMER         ( PP_TDOX_EVALUATION_PERIOD_IN_SEC * 1000 / gPpPollTimer.timer_poll_time_msec )    /* Divide by EVENT_POLLTIME_MSECS to get X */

typedef struct
{
    PAL_OsTimerHandle       timer_handle;
    Uint32                  timer_poll_time_msec;
}
AVALANCHE_PP_TIMER_t;

static AVALANCHE_PP_TIMER_t     gPpPollTimer;

/**************************************************************************
 * FUNCTION NAME : void avalanche_pp_timer_handler ( Uint32 param )
 **************************************************************************
 * DESCRIPTION   :
 *  The function is the dispatcher code that passes events
 *  to the registered event handler.
 **************************************************************************/
void    avalanche_pp_timer_handler( Uint32 param )
{
    static Uint32 evaluationIteration = 0;
    Uint16 eventIdxFW   = (Uint16)*(Uint32*)IO_PHY2VIRT(PP_HAL_EVENTS_PP_INDEX_BASE_PHY);
    Uint16 eventIdxHOST = (Uint16)*(Uint32*)IO_PHY2VIRT(PP_HAL_EVENTS_HOST_INDEX_BASE_PHY);

    while( eventIdxHOST != eventIdxFW )
    {
        Uint16 eventCode;
        Uint32 eventAddress = IO_PHY2VIRT( PP_HAL_EVENTS_BASE_PHY ) + ( eventIdxHOST & 0xFFF );

        eventCode = *(Uint16*)(eventAddress);

        avalanche_pp_event_report( PP_EV_SESSION_EXPIRED, 
                                   (Uint32)(eventCode >> PP_HAL_EVENT_DATA_SHIFT) & PP_HAL_EVENT_DATA_MASK, 
                                   (Uint32)0 );
                
        eventIdxHOST += PP_HAL_EVENTS_ENTRY_SIZE_BYTES;
    }
    *(Uint32*) IO_PHY2VIRT(PP_HAL_EVENTS_HOST_INDEX_BASE_PHY) = eventIdxHOST;


    if (++evaluationIteration >= PP_TDOX_EVALUATION_ONCE_IN_X_TIMER)
    {
        /* TDOX sessions evaluation */
        avalanche_pp_event_report( PP_EV_MISC_TRIGGER_TDOX_EVALUATION, 0, 0 );
        evaluationIteration = 0;
    }

    PAL_osTimerStart( gPpPollTimer.timer_handle, gPpPollTimer.timer_poll_time_msec );
}

/**************************************************************************
 * FUNCTION NAME : AVALANCHE_PP_RET_e avalanche_pp_event_poll_timer_init( void )
 **************************************************************************
 * DESCRIPTION   :
 *  The function initiolaized the timer.
 * RETURNS       :
 *      0  -   Success
 *      1  -   Error
 **************************************************************************/
AVALANCHE_PP_RET_e    avalanche_pp_event_poll_timer_init( void )
{
    gPpPollTimer.timer_poll_time_msec = 100;

    if (PAL_osTimerCreate( avalanche_pp_timer_handler, &gPpPollTimer, &gPpPollTimer.timer_handle ))
    {
        return (PP_RC_FAILURE);
    }

    if (PAL_osTimerStart( gPpPollTimer.timer_handle, gPpPollTimer.timer_poll_time_msec ))
    {
        return (PP_RC_FAILURE);
    }

    return (PP_RC_SUCCESS);
}
/* ========================================================================================= */


/* ******************************************************************** */
/*                                                                      */
/*                      ____ ___ ____                                   */
/*                     |  _ \_ _|  _ \                                  */
/*                     | |_) | || | | |                                 */
/*                     |  __/| || |_| |                                 */
/*                     |_|  |___|____/                                  */
/*                                                                      */
/*                                                                      */
/* ******************************************************************** */
    /* ****************************************** */
    /*                           _                */
    /*        ___ _ __ ___  __ _| |_ ___          */
    /*       / __| '__/ _ \/ _` | __/ _ \         */
    /*      | (__| | |  __/ (_| | ||  __/         */
    /*       \___|_|  \___|\__,_|\__\___|         */
    /*                                            */
    /* ****************************************** */

/**************************************************************************
 * FUNCTION NAME : AVALANCHE_PP_RET_e avalanche_pp_pid_create ( AVALANCHE_PP_PID_t * ptr_pid, void * ptr_netdev )
 **************************************************************************
 * DESCRIPTION   :
 *  The function uses the information passed to create a PID in the PP.
 *  param[in] ptr_pid - pointer to pid information
 *  param[in] ptr_netdev - pointer to network device information
 * RETURNS       :
 *      0  -   Success
 *      >0  -   Error
 **************************************************************************/
AVALANCHE_PP_RET_e    avalanche_pp_pid_create            ( AVALANCHE_PP_PID_t * ptr_pid, void * ptr_netdev )
{
    PP_DB_PID_Entry_t *     ptr_pid_db;
    AVALANCHE_PP_RET_e      rc =  PP_RC_SUCCESS;

    if (ptr_pid->pid_handle >= AVALANCHE_PP_MAX_PID)
    {
        return (PP_RC_INVALID_PARAM);
    }

    ptr_pid_db = &PP_DB.repository_PIDs[ ptr_pid->pid_handle ];

    PP_DB_LOCK();

    PP_DB_CHECK_ACTIVE_UNDER_LOCK();

    memcpy(&ptr_pid_db->pid, ptr_pid, sizeof(*ptr_pid)); 
    INIT_LIST_HEAD( &ptr_pid_db->pid_link_head );
    ptr_pid_db->status = PP_DB_STATUS_ACTIVE;
    PP_DB.stats.active_PIDs++;

    rc = pp_hal_pid_create( &ptr_pid_db->pid );

    PP_DB_UNLOCK();

    if (PP_RC_SUCCESS != rc)
    {
        __avalanche_pp_pid_delete( ptr_pid_db->pid.pid_handle );
        return (rc);
    }
    // Send event
    avalanche_pp_event_report( PP_EV_PID_CREATED, 
                               (Uint32)ptr_pid_db->pid.pid_handle, 
                               (Uint32)ptr_netdev );
    return (PP_RC_SUCCESS);
}
    /* ****************************************** */


    /* ****************************************** */
    /*           _      _      _                  */
    /*        __| | ___| | ___| |_ ___            */
    /*       / _` |/ _ \ |/ _ \ __/ _ \           */
    /*      | (_| |  __/ |  __/ ||  __/           */
    /*       \__,_|\___|_|\___|\__\___|           */
    /*                                            */
    /* ****************************************** */

/**************************************************************************
 * FUNCTION NAME : AVALANCHE_PP_RET_e avalanche_pp_pid_delete ( Uint8 pid_handle )
 **************************************************************************
 * DESCRIPTION   :
 *  The function deletes the PID in the PP.
 *  param[in] pid_handle - handle of pid to delete
 * RETURNS       :
 *      0  -   Success
 *      >0  -   Error
 **************************************************************************/
AVALANCHE_PP_RET_e    avalanche_pp_pid_delete            ( Uint8     pid_handle )
{
    AVALANCHE_PP_RET_e      rc = __avalanche_pp_pid_delete( pid_handle );

    if (PP_RC_SUCCESS == rc)
    {
        // Send event
        avalanche_pp_event_report( PP_EV_PID_DELETED, (Uint32)pid_handle, 0 );
    }

    return (rc);
}

static AVALANCHE_PP_RET_e    __avalanche_pp_pid_delete          ( Uint8     pid_handle )
{
    PP_DB_PID_Entry_t *     ptr_pid_db;
    struct list_head *      pos = NULL;
    AVALANCHE_PP_RET_e      rc = PP_RC_SUCCESS;

    if (pid_handle >= AVALANCHE_PP_MAX_PID)
    {
        return (PP_RC_INVALID_PARAM);
    }

    ptr_pid_db = &PP_DB.repository_PIDs[ pid_handle ];

    PP_DB_LOCK();
   
    PP_DB_CHECK_ACTIVE_UNDER_LOCK();

    // Go over all related VPIDs and delete them
    list_for_each( pos, &ptr_pid_db->pid_link_head )
    {
        PP_DB_VPID_Entry_t * entry;
        entry = list_entry(pos, PP_DB_VPID_Entry_t, pid_link);

        rc |= avalanche_pp_vpid_delete( entry->handle );
    }

    rc |= pp_hal_pid_delete( pid_handle );

    ptr_pid_db->status = PP_DB_STATUS_INITIALIZED;
    PP_DB.stats.active_PIDs--;

    PP_DB_UNLOCK();

    return (rc);
}

/**************************************************************************
 * FUNCTION NAME : AVALANCHE_PP_RET_e avalanche_pp_pid_config_range ( AVALANCHE_PP_PID_RANGE_t * pid_range )
 **************************************************************************
 * DESCRIPTION   :
 *  The function uses the information passed to config PID range in the PDSP.
 *  param[in] pid_range - pointer to pid_range struct
 * RETURNS       :
 *      0  -   Success
 *      >0  -   Error
 **************************************************************************/
AVALANCHE_PP_RET_e    avalanche_pp_pid_config_range      ( AVALANCHE_PP_PID_RANGE_t *    pid_range )
{
    PP_DB_CHECK_ACTIVE();
    return pp_hal_pid_range_create( pid_range );
}

/**************************************************************************
 * FUNCTION NAME : AVALANCHE_PP_RET_e avalanche_pp_pid_remove_range ( Uint32 port_num )
 **************************************************************************
 * DESCRIPTION   :
 *  The function uses the information passed to remove PID range in the PDSP.
 *  param[in] port_num - number of port to remove
 * RETURNS       :
 *      0  -   Success
 *      >0  -   Error
 **************************************************************************/
AVALANCHE_PP_RET_e    avalanche_pp_pid_remove_range      ( Uint32    port_num )
{
    PP_DB_CHECK_ACTIVE_UNDER_LOCK();
    return pp_hal_pid_range_delete( port_num );
}

/**************************************************************************
 * FUNCTION NAME : AVALANCHE_PP_RET_e avalanche_pp_pid_set_flags ( Uint8 pid_handle, Uint32 new_flags )
 **************************************************************************
 * DESCRIPTION   :
 *  The function uses the information passed to modify the PID flags in the PP.
 *  param[in] pid_handle - handle of pid
 *  param[in] new_flags - new flags to set 
 * RETURNS       :
 *      0  -   Success
 *      >0  -   Error
 **************************************************************************/
AVALANCHE_PP_RET_e    avalanche_pp_pid_set_flags         ( Uint8     pid_handle,     Uint32  new_flags )
{
    PP_DB_PID_Entry_t *     ptr_pid_db;
    AVALANCHE_PP_RET_e      rc = PP_RC_SUCCESS;

    if (pid_handle >= AVALANCHE_PP_MAX_PID)
    {
        return (PP_RC_INVALID_PARAM);
    }

    ptr_pid_db = &PP_DB.repository_PIDs[ pid_handle ];

    PP_DB_LOCK();

    PP_DB_CHECK_ACTIVE_UNDER_LOCK();

    ptr_pid_db->pid.priv_flags = (Uint8) new_flags;
    rc = pp_hal_pid_flags_set( &ptr_pid_db->pid );

    PP_DB_UNLOCK();

    return (rc);
}

/**************************************************************************
 * FUNCTION NAME : AVALANCHE_PP_RET_e avalanche_pp_pid_get_list ( Uint8 * num_entries, AVALANCHE_PP_PID_t ** pid_list )
 **************************************************************************
 * DESCRIPTION   :
 *  The function is used to get the PID list from PP DB and the number of active PID's
 *  param[in] num_entries - pointer to set number of active PID's
 *  param[in] pid_list - pointer to PID_type list 
 * RETURNS       :
 *      0  -   Success
 *      >0  -   Error
 **************************************************************************/
AVALANCHE_PP_RET_e    avalanche_pp_pid_get_list          ( Uint8 *   num_entries, AVALANCHE_PP_PID_t ** pid_list )
{
    Uint8   pid_handle = 0;

    PP_DB_LOCK();

    for (pid_handle = 0; pid_handle < AVALANCHE_PP_MAX_PID; pid_handle++)
    {
        if (PP_DB.repository_PIDs[ pid_handle ].status > PP_DB_STATUS_INITIALIZED )
        {
            *pid_list++ = &PP_DB.repository_PIDs[ pid_handle ].pid;
        }
    }

    *num_entries = (Uint8) PP_DB.stats.active_PIDs;

    PP_DB_UNLOCK();
    
    return (PP_RC_SUCCESS);
}

/**************************************************************************
 * FUNCTION NAME : AVALANCHE_PP_RET_e avalanche_pp_pid_get_info ( Uint8 pid_handle,  AVALANCHE_PP_PID_t ** ptr_pid )
 **************************************************************************
 * DESCRIPTION   :
 *  The function is used to get the PID Information block given a handle.
 *  param[in] pid_handle - handle of PID
 *  param[in] ptr_pid - pointer to set PID info from DB 
 * RETURNS       :
 *      0  -   Success
 *      >0  -   Error
 **************************************************************************/
AVALANCHE_PP_RET_e    avalanche_pp_pid_get_info          ( Uint8     pid_handle,  AVALANCHE_PP_PID_t ** ptr_pid )
{
    if (pid_handle >= AVALANCHE_PP_MAX_PID)
    {
        return (PP_RC_INVALID_PARAM);
    }
    if (NULL == ptr_pid)
    {
        return (PP_RC_INVALID_PARAM);
    }

    *ptr_pid = &PP_DB.repository_PIDs[ pid_handle ].pid ;
    
    return (PP_RC_SUCCESS);
}



/* ******************************************************************** */
/*                                                                      */
/*                __     ______ ___ ____                                */
/*                \ \   / /  _ \_ _|  _ \                               */
/*                 \ \ / /| |_) | || | | |                              */
/*                  \ V / |  __/| || |_| |                              */
/*                   \_/  |_|  |___|____/                               */
/*                                                                      */
/*                                                                      */
/* ******************************************************************** */
    /* ****************************************** */
    /*                           _                */
    /*        ___ _ __ ___  __ _| |_ ___          */
    /*       / __| '__/ _ \/ _` | __/ _ \         */
    /*      | (__| | |  __/ (_| | ||  __/         */
    /*       \___|_|  \___|\__,_|\__\___|         */
    /*                                            */
    /* ****************************************** */

/**************************************************************************
 * FUNCTION NAME : AVALANCHE_PP_RET_e avalanche_pp_vpid_create ( AVALANCHE_PP_VPID_INFO_t * ptr_vpid )
 **************************************************************************
 * DESCRIPTION   :
 *  The function uses the information passed to create a VPID in the PP.
 *  param[in] ptr_vpid - pointer to VPIT information
 * RETURNS       :
 *      0  -   Success
 *      >0  -   Error
 **************************************************************************/
AVALANCHE_PP_RET_e    avalanche_pp_vpid_create           ( AVALANCHE_PP_VPID_INFO_t *    ptr_vpid )
{
    PP_DB_VPID_Entry_t *        ptr_vpid_db;
    struct list_head *          pos = NULL;
    AVALANCHE_PP_RET_e          rc = PP_RC_SUCCESS;

    if (NULL == ptr_vpid)
    {
        return (PP_RC_INVALID_PARAM);
    }

    if (ptr_vpid->parent_pid_handle >= AVALANCHE_PP_MAX_PID)
    {
        return (PP_RC_INVALID_PARAM);
    }

    PP_DB_LOCK();
    
    PP_DB_CHECK_ACTIVE_UNDER_LOCK();

    if (list_empty( &PP_DB.pool_VPIDs[ PP_DB_POOL_FREE ] ))
    {
        PP_DB_UNLOCK();
        return (PP_RC_OUT_OF_MEMORY);
    }

    pos = PP_DB.pool_VPIDs[ PP_DB_POOL_FREE ].next;

    ptr_vpid_db = list_entry( pos, PP_DB_VPID_Entry_t, link );

    list_move( pos, &PP_DB.pool_VPIDs[ PP_DB_POOL_BUSY ] );

    memcpy( &ptr_vpid_db->vpid, ptr_vpid, sizeof( ptr_vpid_db->vpid ) );

    ptr_vpid_db->vpid.vpid_handle   = ptr_vpid_db->handle;
    ptr_vpid->vpid_handle           = ptr_vpid_db->handle;

    PP_DB.stats.active_VPIDs++;

    list_add( &ptr_vpid_db->pid_link, &PP_DB.repository_PIDs[ ptr_vpid_db->vpid.parent_pid_handle ].pid_link_head );

    // HAL add VPID
    rc = pp_hal_vpid_create( &ptr_vpid_db->vpid );

    PP_DB_UNLOCK();

    if (PP_RC_SUCCESS != rc)
    {
        __avalanche_pp_vpid_delete( ptr_vpid_db->vpid.vpid_handle );
        return (rc);
    }

    // Send event
    avalanche_pp_event_report( PP_EV_VPID_CREATED, (Uint32)ptr_vpid_db->handle, 0 );

    return (PP_RC_SUCCESS);
}

    /* ****************************************** */
                           
    /* ****************************************** */
    /*           _      _      _                  */
    /*        __| | ___| | ___| |_ ___            */
    /*       / _` |/ _ \ |/ _ \ __/ _ \           */
    /*      | (_| |  __/ |  __/ ||  __/           */
    /*       \__,_|\___|_|\___|\__\___|           */
    /*                                            */
    /* ****************************************** */

/**************************************************************************
 * FUNCTION NAME : AVALANCHE_PP_RET_e avalanche_pp_vpid_delete ( Uint8 vpid_handle )
 **************************************************************************
 * DESCRIPTION   :
 *  The function deletes the VPID in the PP.
 *  param[in] vpid_handle - handle of VPID to delete 
 * RETURNS       :
 *      0  -   Success
 *      >0  -   Error
 **************************************************************************/
AVALANCHE_PP_RET_e    avalanche_pp_vpid_delete           ( Uint8     vpid_handle )
{
    AVALANCHE_PP_RET_e          rc = __avalanche_pp_vpid_delete( vpid_handle );

    if (PP_RC_SUCCESS == rc)
    {
        // Send event
        avalanche_pp_event_report( PP_EV_VPID_DELETED, (Uint32)vpid_handle, 0 );
    }

    return (rc);
}


static AVALANCHE_PP_RET_e    __avalanche_pp_vpid_delete         ( Uint8     vpid_handle )
{
    PP_DB_VPID_Entry_t *        ptr_vpid_db;
    PP_DB_Session_Entry_t *     entry;
    struct list_head *          pos = NULL;
    struct list_head *          nextPos = NULL;
    AVALANCHE_PP_RET_e          rc = PP_RC_SUCCESS;
    struct list_head *          head        = NULL;

    if (vpid_handle >= AVALANCHE_PP_MAX_VPID)
    {
        return (PP_RC_INVALID_PARAM);
    }

    PP_DB_LOCK();

    PP_DB_CHECK_ACTIVE_UNDER_LOCK();
    
    ptr_vpid_db = &PP_DB.repository_VPIDs[ vpid_handle ];

    head = &ptr_vpid_db->list[ PP_LIST_ID_INGRESS ];
    for (pos = head->next; prefetch(pos->next), pos != (head); pos = nextPos)
    {
        nextPos = pos->next;
        entry = list_entry(pos, PP_DB_Session_Entry_t, list[ PP_LIST_ID_INGRESS ]);
        rc |= avalanche_pp_session_delete( entry->session_info.session_handle, NULL );
    }

    head = &ptr_vpid_db->list[ PP_LIST_ID_EGRESS ];
    for (pos = head->next; prefetch(pos->next), pos != (head); pos = nextPos)
    {
        nextPos = pos->next;
        entry = list_entry(pos, PP_DB_Session_Entry_t, list[ PP_LIST_ID_EGRESS ]);
        rc |= avalanche_pp_session_delete( entry->session_info.session_handle, NULL );
    }

    // HAL
    rc |= pp_hal_vpid_delete( vpid_handle );

    list_del_init( &ptr_vpid_db->pid_link );

    list_move( &ptr_vpid_db->link, &PP_DB.pool_VPIDs[ PP_DB_POOL_FREE ] );

    PP_DB.stats.active_VPIDs--;

    PP_DB_UNLOCK();

    return (rc);
}
    /* ****************************************** */


/**************************************************************************
 * FUNCTION NAME : AVALANCHE_PP_RET_e avalanche_pp_vpid_set_flags ( Uint8 vpid_handle, Uint32 new_flags )
 **************************************************************************
 * DESCRIPTION   :
 *  The function uses the information passed to modify the VPID flags in the PP.
 *  param[in] vpid_handle - handle of VPID
 *  param[in] new_flags - new flags to set
 * RETURNS       :
 *      0  -   Success
 *      >0  -   Error
 **************************************************************************/
AVALANCHE_PP_RET_e    avalanche_pp_vpid_set_flags        ( Uint8     vpid_handle,    Uint32  new_flags )
{
	AVALANCHE_PP_RET_e          rc = PP_RC_SUCCESS;
	
    if (vpid_handle >= AVALANCHE_PP_MAX_VPID)
    {
        return (PP_RC_INVALID_PARAM);
    }

    PP_DB_LOCK();

    PP_DB_CHECK_ACTIVE_UNDER_LOCK();

    PP_DB.repository_VPIDs[ vpid_handle ].vpid.flags = (Uint8) new_flags;

    rc = pp_hal_vpid_flags_set( &PP_DB.repository_VPIDs[ vpid_handle ].vpid );

    PP_DB_UNLOCK();

    return (rc);
}

/**************************************************************************
 * FUNCTION NAME : AVALANCHE_PP_RET_e avalanche_pp_vpid_get_list ( Uint8 parent_pid_handle, Uint8 * num_entries, AVALANCHE_PP_VPID_INFO_t ** vpid_list )
 **************************************************************************
 * DESCRIPTION   :
 *  The function is used to get the VPID list from PP DB and the number of entries.
 *  param[in] parent_pid_handle - PID handle 
 *  param[in] num_entries - number of entries in list
 *  param[in] vpid_list - pointer to list head
 * RETURNS       :
 *      0  -   Success
 *      >0  -   Error
 **************************************************************************/
AVALANCHE_PP_RET_e    avalanche_pp_vpid_get_list         ( Uint8     parent_pid_handle,  Uint8 *     num_entries, AVALANCHE_PP_VPID_INFO_t **    vpid_list )
{
    PP_DB_VPID_Entry_t *        ptr_vpid_db;
    struct list_head *          pos = NULL;
    Uint8                       count = 0;

    if (parent_pid_handle > AVALANCHE_PP_MAX_PID)
    {
        return (PP_RC_INVALID_PARAM);
    }

    PP_DB_LOCK();

    if (parent_pid_handle == AVALANCHE_PP_MAX_PID)
    {
        list_for_each(pos, &PP_DB.pool_VPIDs[ PP_DB_POOL_BUSY ])
        {
            ptr_vpid_db = list_entry( pos, PP_DB_VPID_Entry_t, link );

            if (vpid_list)
            {
                *vpid_list++ = &ptr_vpid_db->vpid; 
            }
            count++;
        }
    }
    else
    {
        list_for_each(pos, &PP_DB.repository_PIDs[parent_pid_handle].pid_link_head)
        {
            ptr_vpid_db = list_entry( pos, PP_DB_VPID_Entry_t, pid_link );

            if (vpid_list)
            {
                *vpid_list++ = &ptr_vpid_db->vpid; 
            }
            count++;
        }
    }

    PP_DB_UNLOCK();

    *num_entries = count;

    return (PP_RC_SUCCESS);
}

/**************************************************************************
 * FUNCTION NAME : AVALANCHE_PP_RET_e avalanche_pp_vpid_get_info ( Uint8 vpid_handle, AVALANCHE_PP_VPID_INFO_t ** ptr_vpid )
 **************************************************************************
 * DESCRIPTION   :
 *  The function is used to get the VPID Information block given a handle.
 *  param[in] vpid_handle - handle of VPID
 *  param[in] ptr_vpid - pointer to set VPID info from DB 
 * RETURNS       :
 *      0  -   Success
 *      >0  -   Error
 **************************************************************************/
AVALANCHE_PP_RET_e    avalanche_pp_vpid_get_info         ( Uint8     vpid_handle,                                 AVALANCHE_PP_VPID_INFO_t **    ptr_vpid )
{
    if (vpid_handle >= AVALANCHE_PP_MAX_VPID)
    {
        return (PP_RC_INVALID_PARAM);
    }

    if (NULL == ptr_vpid)
    {
        return (PP_RC_INVALID_PARAM);
    }

    *ptr_vpid = &PP_DB.repository_VPIDs[ vpid_handle ].vpid;

    return (PP_RC_SUCCESS);
}


/* ******************************************************************** */
/*                                                                      */
/*                 ____                _                                */
/*                / ___|  ___  ___ ___(_) ___  _ __                     */
/*                \___ \ / _ \/ __/ __| |/ _ \| '_ \                    */
/*                 ___) |  __/\__ \__ \ | (_) | | | |                   */
/*                |____/ \___||___/___/_|\___/|_| |_|                   */
/*                                                                      */
/*                                                                      */
/* ******************************************************************** */
    /* ****************************************** */
    /*                           _                */
    /*        ___ _ __ ___  __ _| |_ ___          */
    /*       / __| '__/ _ \/ _` | __/ _ \         */
    /*      | (__| | |  __/ (_| | ||  __/         */
    /*       \___|_|  \___|\__,_|\__\___|         */
    /*                                            */
    /* ****************************************** */

/**************************************************************************
 * FUNCTION NAME : AVALANCHE_PP_RET_e avalanche_pp_session_create ( AVALANCHE_PP_SESSION_INFO_t * ptr_session, void * pkt_ptr )
 **************************************************************************
 * DESCRIPTION   :
 *  The function is used to create a session.
 *  param[in] ptr_session - pointer to session information
 *  param[in] pkt_ptr - 
 * RETURNS       :
 *      0  -   Success
 *      >0  -   Error
 **************************************************************************/
AVALANCHE_PP_RET_e    avalanche_pp_session_create        ( AVALANCHE_PP_SESSION_INFO_t *  ptr_session, void * pkt_ptr )
{
    PP_DB_Session_Entry_t *             ptr_session_db;
    PP_DB_Session_LUT1_hash_entry_t *   hash_entry_1 = NULL;
    PP_DB_Session_LUT2_hash_entry_t *   hash_entry_2 = NULL;
    PP_DB_Entry_t *                     tdox_entry = NULL;
    struct list_head *                  pos = NULL;
    AVALANCHE_PP_RET_e                  rc = PP_RC_SUCCESS;
    Uint32                              hash_LUT1;
    Uint32                              hash_LUT2;
    Bool                                create_LUT1 = False;


    PP_DB_CHECK_ACTIVE();

    hash_LUT1 = 0x000000FF & pp_db_hash( (Uint8*)&ptr_session->ingress.lookup.LUT1, sizeof(ptr_session->ingress.lookup.LUT1), 0 );

    PP_DB_LOCK();

#ifndef CONFIG_MACH_PUMA5
    /* ARRIS ADD START */    
    ARRISMOD_CALL( arris_rip_create_pp_session_hook_v2, ptr_session, pkt_ptr );
    /* ARRIS ADD END */
#endif

    /*----------------------------------------------------------------------------------------------*/
    /*             LUT1 Search ...                                                                  */
    /*----------------------------------------------------------------------------------------------*/
    list_for_each( pos, &PP_DB.lut1_hash[ hash_LUT1 ] )
    {
        hash_entry_1=   list_entry( pos, PP_DB_Session_LUT1_hash_entry_t, lut1_hash_link );

        if ( memcmp( &hash_entry_1->lut1_data, &ptr_session->ingress.lookup.LUT1, sizeof(ptr_session->ingress.lookup.LUT1) ) )
        {
            hash_entry_1 = NULL;
        }
        else
        {
            // entry found ...
            break;
        }
    }

    if ( NULL == hash_entry_1 )
    {
        if (list_empty( &PP_DB.pool_lut1[ ptr_session->session_pool ][ PP_DB_POOL_FREE ] ))
        {
            PP_DB.stats.lut1_starvation++;
            PP_DB_UNLOCK();
            return (PP_RC_OUT_OF_MEMORY);
        }

        if (list_empty( &PP_DB.pool_lut2[ ptr_session->session_pool ][ PP_DB_POOL_FREE ] ))
        {
            PP_DB.stats.lut2_starvation++;
            PP_DB_UNLOCK();
            return (PP_RC_OUT_OF_MEMORY);
        }

        create_LUT1 = True;
        PP_DB.stats.active_lut1_keys++;
        if (PP_DB.stats.active_lut1_keys > PP_DB.stats.max_active_lut1_keys)
        {
            PP_DB.stats.max_active_lut1_keys = PP_DB.stats.active_lut1_keys;
        }

        pos = PP_DB.pool_lut1[ ptr_session->session_pool ][ PP_DB_POOL_FREE ].next;

        hash_entry_1 = list_entry( pos, PP_DB_Session_LUT1_hash_entry_t, link );

        list_move( pos, &PP_DB.pool_lut1[ ptr_session->session_pool ][ PP_DB_POOL_BUSY ] );

        list_add( &hash_entry_1->lut1_hash_link, &PP_DB.lut1_hash[ hash_LUT1 ] );

        memcpy( &hash_entry_1->lut1_data, &ptr_session->ingress.lookup.LUT1, sizeof(ptr_session->ingress.lookup.LUT1) );
    }
    /*------------------------------------------------------------------------------------------------*/

    ptr_session->ingress.lookup.LUT2.u.fields.LUT1_key = hash_entry_1->handle;

    hash_LUT2 = 0x000007FF & pp_db_hash( (Uint8*)&ptr_session->ingress.lookup.LUT2, sizeof(ptr_session->ingress.lookup.LUT2), 0 );

    /*----------------------------------------------------------------------------------------------*/
    /*             LUT2 Search ...                                                                  */
    /*----------------------------------------------------------------------------------------------*/
    list_for_each( pos, &PP_DB.lut2_hash[ hash_LUT2 ] )
    {
        hash_entry_2=   list_entry( pos, PP_DB_Session_LUT2_hash_entry_t, lut2_hash_link );

        if ( memcmp( &PP_DB.repository_sessions[ hash_entry_2->handle ].session_info.ingress.lookup.LUT2, 
                     &ptr_session->ingress.lookup.LUT2, 
                     sizeof(ptr_session->ingress.lookup.LUT2) ) )
        {
            hash_entry_2 = NULL;
        }
        else
        {
            // entry found ...
            break;
        }
    }

    if ( NULL == hash_entry_2 )
    {
        if (list_empty( &PP_DB.pool_lut2[ ptr_session->session_pool ][ PP_DB_POOL_FREE ] ))
        {
            PP_DB.stats.lut2_starvation++;
            PP_DB_UNLOCK();
            return (PP_RC_OUT_OF_MEMORY);
        }

        /* ======================== */
        if ( (ptr_session->egress.vpid_handle < AVALANCHE_PP_MAX_VPID) && (PP_DB.repository_VPIDs[ ptr_session->egress.vpid_handle ].session_pre_action_cb) )
        {
            AVALANCHE_EXEC_HOOK_FN_t cb = (AVALANCHE_EXEC_HOOK_FN_t)PP_DB.repository_VPIDs[ ptr_session->egress.vpid_handle ].session_pre_action_cb;

            cb( ptr_session, PP_DB.repository_VPIDs[ ptr_session->egress.vpid_handle ].session_pre_data );
        }
        /* ======================== */

        pos = PP_DB.pool_lut2[ ptr_session->session_pool ][ PP_DB_POOL_FREE ].next;

        hash_entry_2 = list_entry( pos, PP_DB_Session_LUT2_hash_entry_t, link );

        list_move( pos, &PP_DB.pool_lut2[ ptr_session->session_pool ][ PP_DB_POOL_BUSY ] );

        list_add( &hash_entry_2->lut2_hash_link, &PP_DB.lut2_hash[ hash_LUT2 ] );

        hash_entry_2->lut1_hash_entry_ptr = hash_entry_1;
        hash_entry_1->refCount++;

        memcpy( &PP_DB.repository_sessions[ hash_entry_2->handle ].session_info, 
                ptr_session, 
                sizeof(*ptr_session) );

        PP_DB.repository_sessions[ hash_entry_2->handle ].lut2_hash_entry_ptr = hash_entry_2;

        PP_DB.repository_sessions[ hash_entry_2->handle ].session_info.session_handle   = hash_entry_2->handle;
        ptr_session->session_handle                                                     = hash_entry_2->handle;

        PP_DB.stats.active_sessions++;
        if (PP_DB.stats.active_sessions > PP_DB.stats.max_active_sessions)
        {
            PP_DB.stats.max_active_sessions = PP_DB.stats.active_sessions;
        }

        list_add( &PP_DB.repository_sessions[ hash_entry_2->handle            ].list[ PP_LIST_ID_EGRESS ],
                  &PP_DB.repository_VPIDs   [ ptr_session->egress.vpid_handle ].list[ PP_LIST_ID_EGRESS ] );

        list_add( &PP_DB.repository_sessions[ hash_entry_2->handle             ].list[ PP_LIST_ID_INGRESS ],
                  &PP_DB.repository_VPIDs   [ ptr_session->ingress.vpid_handle ].list[ PP_LIST_ID_INGRESS ] );

        if ( IPPROTO_TCP == ptr_session->egress.ip_protocol )
        {
            list_add( &PP_DB.repository_sessions[ hash_entry_2->handle            ].list[ PP_LIST_ID_EGRESS_TCP ],
                      &PP_DB.repository_VPIDs   [ ptr_session->egress.vpid_handle ].list[ PP_LIST_ID_EGRESS_TCP ] );
        }

        ptr_session_db = &PP_DB.repository_sessions[ hash_entry_2->handle ];

        /*----------------------------------------------------------------------------------------------*/
        /*             TDOX allocation  ...                                                             */
        /*----------------------------------------------------------------------------------------------*/
        if ( AVALANCHE_PP_EGRESS_FIELD_ENABLE_TDOX_ENABLED & ptr_session->egress.enable )
        {
            if (list_empty( &PP_DB.pool_TDOX[ PP_DB_POOL_FREE ] ))
            {
                ptr_session->egress.enable &= ~AVALANCHE_PP_EGRESS_FIELD_ENABLE_TDOX_ENABLED;
                ptr_session_db->session_info.egress.enable &= ~AVALANCHE_PP_EGRESS_FIELD_ENABLE_TDOX_ENABLED;
                PP_DB.stats.tdox_starvation++;
            }
            else
            {
                pos = PP_DB.pool_TDOX[ PP_DB_POOL_FREE ].next;

                tdox_entry = list_entry( pos, PP_DB_Entry_t, link );


                list_move( pos, &PP_DB.pool_TDOX[ PP_DB_POOL_BUSY ] );

                ptr_session_db->session_info.egress.tdox_handle = tdox_entry->handle;
                ptr_session->egress.tdox_handle                 = tdox_entry->handle;
                ptr_session_db->session_info.priority           = 0;
                ptr_session->priority                           = 0;

                list_add( &PP_DB.repository_sessions[ hash_entry_2->handle            ].list[ PP_LIST_ID_EGRESS_TDOX ],
                          &PP_DB.repository_VPIDs   [ ptr_session->egress.vpid_handle ].list[ PP_LIST_ID_EGRESS_TDOX ] );
            }
        }
        /*----------------------------------------------------------------------------------------------*/
    }
    else
    {
        // session exist
        ptr_session->session_handle = hash_entry_2->handle;

        PP_DB_UNLOCK();
        return (PP_RC_OBJECT_EXIST);
    }
    /*------------------------------------------------------------------------------------------------*/


    rc = pp_hal_session_create( &ptr_session_db->session_info, create_LUT1 );

    /* ======================== */
    if ( (ptr_session->egress.vpid_handle < AVALANCHE_PP_MAX_VPID) && (PP_DB.repository_VPIDs[ ptr_session->egress.vpid_handle ].session_post_action_cb) )
    {
        AVALANCHE_EXEC_HOOK_FN_t cb = (AVALANCHE_EXEC_HOOK_FN_t)PP_DB.repository_VPIDs[ ptr_session->egress.vpid_handle ].session_post_action_cb;

        cb( ptr_session, PP_DB.repository_VPIDs[ ptr_session->egress.vpid_handle ].session_post_data );
    }
    /* ======================== */

    PP_DB_UNLOCK();

    if (rc != PP_RC_SUCCESS)
    {
        __avalanche_pp_session_delete( ptr_session_db->session_info.session_handle, NULL );
        return (rc);
    }

    if (create_LUT1)
    {
        PP_DB.stats.lut1_histogram[(PP_DB.stats.active_lut1_keys - 1) / AVALANCHE_PP_LUT1_HISTOGRAM_RESOLUTION]++;
    }
    PP_DB.stats.lut2_histogram[(PP_DB.stats.active_sessions - 1) / AVALANCHE_PP_LUT2_HISTOGRAM_RESOLUTION]++;

    // Send event
    avalanche_pp_event_report( PP_EV_SESSION_CREATED, (Uint32)hash_entry_2->handle, (Uint32)pkt_ptr );

    return (rc);
}

    /* ****************************************** */
    /*           _      _      _                  */
    /*        __| | ___| | ___| |_ ___            */
    /*       / _` |/ _ \ |/ _ \ __/ _ \           */
    /*      | (_| |  __/ |  __/ ||  __/           */
    /*       \__,_|\___|_|\___|\__\___|           */
    /*                                            */
    /* ****************************************** */

/**************************************************************************
 * FUNCTION NAME : AVALANCHE_PP_RET_e avalanche_pp_session_delete ( Uint32 session_handle, AVALANCHE_PP_SESSION_STATS_t * ptr_session_stats )
 **************************************************************************
 * DESCRIPTION   :
 *  The function is used to delete the session.
 *  param[in] session_handle - handle of session to delete
 *  param[in] ptr_session_stats - pointer to session statistics
 * RETURNS       :
 *      0  -   Success
 *      >0  -   Error
 **************************************************************************/
AVALANCHE_PP_RET_e    avalanche_pp_session_delete        ( Uint32    session_handle,     AVALANCHE_PP_SESSION_STATS_t *  ptr_session_stats )
{
    AVALANCHE_PP_RET_e      rc = __avalanche_pp_session_delete( session_handle, ptr_session_stats );

    if (PP_RC_SUCCESS == rc)
    {
        AVALANCHE_PP_SESSION_STATS_t    session_stats;

        avalanche_pp_get_stats_session   ( session_handle,  &session_stats );

        if (ptr_session_stats)
        {
            memcpy( ptr_session_stats, &session_stats, sizeof(session_stats) );
        }

        // Send event
        avalanche_pp_event_report( PP_EV_SESSION_DELETED, (Uint32)session_handle, (Uint32)&session_stats );
    }

    return (rc);
}

static AVALANCHE_PP_RET_e    __avalanche_pp_session_delete      ( Uint32    session_handle,     AVALANCHE_PP_SESSION_STATS_t *  ptr_session_stats )
{
    PP_DB_Session_Entry_t *             session_db_ptr = NULL;
    AVALANCHE_PP_RET_e                  rc = PP_RC_SUCCESS;
    Bool                                delete_LUT1 = False;
    PP_LIST_ID_e                        j;

    if (AVALANCHE_PP_MAX_ACCELERATED_SESSIONS <= session_handle)
    {
        return (PP_RC_INVALID_PARAM);
    }

    PP_DB_LOCK();
    
    PP_DB_CHECK_ACTIVE_UNDER_LOCK();

    session_db_ptr = &PP_DB.repository_sessions[ session_handle ];

    if (NULL == session_db_ptr->lut2_hash_entry_ptr)
    {
        printk(" Session %d has already been deleted\n", session_handle );
        return (PP_RC_INVALID_PARAM);
    }

    for (j=PP_LIST_ID_INGRESS; j<PP_LIST_ID_ALL; j++)
    {
        if (!list_empty( &session_db_ptr->list[ j ] ))
        {
            list_del_init( &session_db_ptr->list[ j ] );
        }
    }

    if (0 == --session_db_ptr->lut2_hash_entry_ptr->lut1_hash_entry_ptr->refCount)
    {
        list_del_init( &session_db_ptr->lut2_hash_entry_ptr->lut1_hash_entry_ptr->lut1_hash_link );

        list_move( &session_db_ptr->lut2_hash_entry_ptr->lut1_hash_entry_ptr->link, 
                   &PP_DB.pool_lut1[ session_db_ptr->session_info.session_pool ][ PP_DB_POOL_FREE ] );

        session_db_ptr->lut2_hash_entry_ptr->lut1_hash_entry_ptr = NULL;

        delete_LUT1 = True;
        PP_DB.stats.active_lut1_keys--;
    }

    list_del_init( &session_db_ptr->lut2_hash_entry_ptr->lut2_hash_link );

    list_move( &session_db_ptr->lut2_hash_entry_ptr->link, &PP_DB.pool_lut2[ session_db_ptr->session_info.session_pool ][ PP_DB_POOL_FREE ] );

    session_db_ptr->lut2_hash_entry_ptr = NULL;

    PP_DB.stats.active_sessions--;

    if ( AVALANCHE_PP_EGRESS_FIELD_ENABLE_TDOX_ENABLED & session_db_ptr->session_info.egress.enable )
    {
        list_move( &PP_DB.repository_TDOX[ session_db_ptr->session_info.egress.tdox_handle ].link, 
                   &PP_DB.pool_TDOX[ PP_DB_POOL_FREE ] );

        session_db_ptr->session_info.egress.enable &= ~AVALANCHE_PP_EGRESS_FIELD_ENABLE_TDOX_ENABLED;
    }

    rc = pp_hal_session_delete( &session_db_ptr->session_info, delete_LUT1 );

    PP_DB_UNLOCK();

   return (rc);
}

/**************************************************************************
 * FUNCTION NAME : AVALANCHE_PP_RET_e avalanche_pp_session_get_list ( Uint8 vpid_handle, PP_LIST_ID_e list_id, Uint32 * num_entries, Uint32 * session_handle_list )
 **************************************************************************
 * DESCRIPTION   :
 *  The function is used to get the sessions list from LUT2 PP DB and the number of entries.
 *  param[in] vpid_handle - handle of the VPID for sessions
 *  param[in] list_id - ingress / egress / tcp / tdox
 *  param[in] num_entries - number of entries in list
 *  param[in] session_handle_list - pointer to list head
 * RETURNS       :
 *      0  -   Success
 *      >0  -   Error
 **************************************************************************/
AVALANCHE_PP_RET_e    avalanche_pp_session_get_list      ( Uint8     vpid_handle,    PP_LIST_ID_e   list_id, Uint32 * num_entries, Uint32 * session_handle_list )
{
    struct list_head *          pos             = NULL;
    PP_DB_VPID_Entry_t *        vpid_db_ptr     = NULL;
    PP_DB_Session_Entry_t *     session_db_ptr  = NULL;

    if (vpid_handle > AVALANCHE_PP_MAX_VPID)
    {
        return (PP_RC_INVALID_PARAM);
    }

    if (list_id > PP_LIST_ID_ALL)
    {
        return (PP_RC_INVALID_PARAM);
    }

    if (NULL == num_entries)
    {
        return (PP_RC_INVALID_PARAM);
    }

    PP_DB_LOCK();

    *num_entries = 0;

    if (vpid_handle == AVALANCHE_PP_MAX_VPID)
    {
        PP_DB_Session_LUT2_hash_entry_t *       entry;
        int pool = 0;

        for (; pool < AVALANCHE_PP_SESSIONS_POOL_MAX; pool++)
        {
            list_for_each(pos, &PP_DB.pool_lut2[ pool ][ PP_DB_POOL_BUSY ])
            {
                entry = list_entry( pos, PP_DB_Session_LUT2_hash_entry_t, link );
                if (session_handle_list)
                {
                    *session_handle_list++ = ( entry->handle );
                }
                (*num_entries)++;
            }
        }
    }
    else
    {
        PP_LIST_ID_e    list_start  = list_id;
        PP_LIST_ID_e    list_end    = list_id;

        if ( list_id == PP_LIST_ID_ALL )
        {
            list_start = PP_LIST_ID_INGRESS;
            list_end   = PP_LIST_ID_EGRESS;
        }

        for (list_id = list_start; list_id <= list_end; list_id++)
        {
            vpid_db_ptr = &PP_DB.repository_VPIDs[vpid_handle]; 

            list_for_each( pos, &vpid_db_ptr->list[ list_id ] )
            {
                session_db_ptr = list_entry( pos, PP_DB_Session_Entry_t, list[ list_id ] );
                if (session_handle_list)
                {
                    *session_handle_list++ = (session_db_ptr->session_info.session_handle);
                }
                (*num_entries)++;
            }
        }
    }

    PP_DB_UNLOCK();

    return (PP_RC_SUCCESS);
}

/**************************************************************************
 * FUNCTION NAME : AVALANCHE_PP_RET_e avalanche_pp_session_get_info ( Uint32 session_handle, AVALANCHE_PP_SESSION_INFO_t** ptr_session_info )
 **************************************************************************
 * DESCRIPTION   :
 *  The function is used to get the session information from a session handle.
 *  param[in] session_handle - handle of the session
 *  param[in] ptr_session_info - pointer to set session info
 * RETURNS       :
 *      0  -   Success
 *      >0  -   Error
 **************************************************************************/
AVALANCHE_PP_RET_e    avalanche_pp_session_get_info      ( Uint32    session_handle,     AVALANCHE_PP_SESSION_INFO_t**   ptr_session_info )
{
    if (AVALANCHE_PP_MAX_ACCELERATED_SESSIONS <= session_handle)
    {
        return (PP_RC_INVALID_PARAM);
    }

    if (NULL == ptr_session_info)
    {
        return (PP_RC_INVALID_PARAM);
    }

    if (PP_DB.repository_sessions[session_handle].lut2_hash_entry_ptr)
    {
        *ptr_session_info = &PP_DB.repository_sessions[session_handle].session_info; 
        return (PP_RC_SUCCESS);
    }
    
    return (PP_RC_INVALID_PARAM);
}

static AVALANCHE_PP_RET_e   __avalanche_pp_flush_single_session( AVALANCHE_PP_SESSION_INFO_t *  ptr_session, Ptr     data )
{
    avalanche_pp_session_delete( ptr_session->session_handle, NULL );
    return (PP_RC_SUCCESS);
}

/**************************************************************************
 * FUNCTION NAME : AVALANCHE_PP_RET_e avalanche_pp_flush_sessions ( Uint8 vpid_handle, PP_LIST_ID_e list_id )
 **************************************************************************
 * DESCRIPTION   :
 *  The function flushes sessions from the session database for a VPID.
 *  param[in] vpid_handle - handle of the VPID
 *  param[in] list_id - ingress / egress / tcp / tdox 
 * RETURNS       :
 *      0  -   Success
 *      >0  -   Error
 **************************************************************************/
AVALANCHE_PP_RET_e    avalanche_pp_flush_sessions        ( Uint8     vpid_handle, PP_LIST_ID_e  list_id )
{
    PP_DB_CHECK_ACTIVE();

    if (vpid_handle > AVALANCHE_PP_MAX_VPID)
    {
        return (PP_RC_INVALID_PARAM);
    }

    if (list_id > PP_LIST_ID_ALL)
    {
        return (PP_RC_INVALID_PARAM);
    }

    if (vpid_handle == AVALANCHE_PP_MAX_VPID)
    {
        // TBD
        avalanche_pp_session_list_execute( vpid_handle, PP_LIST_ID_ALL, __avalanche_pp_flush_single_session, NULL );
    }
    else
    {
        avalanche_pp_session_list_execute( vpid_handle, list_id,        __avalanche_pp_flush_single_session, NULL );
    }

    return (PP_RC_SUCCESS);
}

/**************************************************************************
 * FUNCTION NAME : AVALANCHE_PP_RET_e avalanche_pp_enable_psm (void)
 **************************************************************************
 * DESCRIPTION   :
 *  This function is called to enable Power Saving mode (PSM)
 * RETURNS       :
 *      0  -   Success
 *      >0  -   Error
 **************************************************************************/
AVALANCHE_PP_RET_e    avalanche_pp_enable_psm (void)
{     
    Int32               rc;
    Int32               enablePsm;
    pdsp_cmd_params_t   pdsp_cmd;
    Int32               reg;
    Int32*              regAddr;

    PP_DB_CHECK_ACTIVE();

   /* Configure Prefetcher command */
    pdsp_cmd.pdsp_id    = PDSP_ID_Prefetcher;
    pdsp_cmd.cmd        = PRECMD_INDEX(2) | PRECMD_COMMAND(PDSP_ENABLE_PREFETCH);
    pdsp_cmd.params[0]  = 0;
    pdsp_cmd.params_len = 0;

    printk("%s: Enable prefetcher PSM mode\n", __FUNCTION__);

    if ((rc = pdsp_cmd_send( pdsp_cmd.pdsp_id, pdsp_cmd.cmd, NULL, 0, pdsp_cmd.params, 1 )) && (rc != -8))
    {
        printk(KERN_ERR"\n%s: failed to put command(%X) to the PDSP %d rc(%d)\n", __FUNCTION__, pdsp_cmd.cmd, pdsp_cmd.pdsp_id, rc );
        return (PP_RC_FAILURE);
    }

    
    /* Power down PP PDSPs using clock gating */
    printk("%s: Halt PP PDSPs\n", __FUNCTION__);

    /* Send PSM enable Cmd to PP */
    pdsp_cmd.pdsp_id = PDSP_ID_Classifier;
    pdsp_cmd.cmd = WORD_S0_B1_0 (0x01, 0x0, PDSP_SETPSM);
    pdsp_cmd.params[0] = 0;   
    pdsp_cmd.params_len =0;

    if (PP_RC_SUCCESS != (rc = pdsp_cmd_send( pdsp_cmd.pdsp_id, pdsp_cmd.cmd, NULL, 0, pdsp_cmd.params, 0 )))
    {
        printk(KERN_ERR"\n%s: failed to put command(%X) to the PDSP %d rc(%d)\n", __FUNCTION__, pdsp_cmd.cmd, pdsp_cmd.pdsp_id, rc );
        return (PP_RC_FAILURE);
    }

    /* unSet the enable bit in all PP PDSPs */
    enablePsm = 1;
    if (PP_RC_SUCCESS != (rc = pdsp_control(PDSP_ID_Classifier, PDSPCTRL_HLT, (Ptr)&enablePsm)))
    {
        printk(KERN_ERR"\n%s: failed to set PP in PSM mode rc(%d)\n", __FUNCTION__,  rc );
        return (PP_RC_FAILURE);
    }

    if (PAL_osTimerStop( gPpPollTimer.timer_handle ))
    {
        return (PP_RC_FAILURE);
    }

    regAddr = (int*)AVALANCHE_NWSS_GENERAL_MAILBOX_CLK_CTRL_REG;
    reg = *regAddr;
    reg &= ~(AVALANCHE_NWSS_GENERAL_MAILBOX_CLK_CTRL_CPDSP1 |
             AVALANCHE_NWSS_GENERAL_MAILBOX_CLK_CTRL_LUT1   |
             AVALANCHE_NWSS_GENERAL_MAILBOX_CLK_CTRL_CPDSP2 |
             AVALANCHE_NWSS_GENERAL_MAILBOX_CLK_CTRL_LUT2   |
             AVALANCHE_NWSS_GENERAL_MAILBOX_CLK_CTRL_MPDSP  |
             AVALANCHE_NWSS_GENERAL_MAILBOX_CLK_CTRL_McDMA3);
    *regAddr = reg;


    PP_DB_LOCK();
    PP_DB.status = PP_DB_STATUS_PSM;
    PP_DB_UNLOCK();

    return PP_RC_SUCCESS;
}

/**************************************************************************
 * FUNCTION NAME : AVALANCHE_PP_RET_e avalanche_pp_disable_psm (void)
 **************************************************************************
 * DESCRIPTION   :
 *  This function is called to disable Power Saving mode (PSM)
 * RETURNS       :
 *      0  -   Success
 *      >0  -   Error
 **************************************************************************/
AVALANCHE_PP_RET_e    avalanche_pp_disable_psm (void)
{
    Int32               rc;
    pdsp_cmd_params_t   pdsp_cmd;
    Int32               enablePsm;
    Int32               reg;
    Int32*              regAddr;

    PP_DB_LOCK();
    if (PP_DB.status != PP_DB_STATUS_PSM)
    {
        printk("ERROR: PP Operation %s cannot be accomplished while PP status is %d\n", __FUNCTION__, PP_DB.status);
        PP_DB_UNLOCK();
        return PP_RC_FAILURE;
    }
    PP_DB_UNLOCK();

    /* Power up PP PDSPs using clock gating */
    printk("%s: Run PP PDSPs\n", __FUNCTION__);
    regAddr = (int*)AVALANCHE_NWSS_GENERAL_MAILBOX_CLK_CTRL_REG;
    reg = *regAddr;
    reg |= (AVALANCHE_NWSS_GENERAL_MAILBOX_CLK_CTRL_CPDSP1 |
            AVALANCHE_NWSS_GENERAL_MAILBOX_CLK_CTRL_LUT1   |
            AVALANCHE_NWSS_GENERAL_MAILBOX_CLK_CTRL_CPDSP2 |
             AVALANCHE_NWSS_GENERAL_MAILBOX_CLK_CTRL_LUT2  |
            AVALANCHE_NWSS_GENERAL_MAILBOX_CLK_CTRL_MPDSP  |
            AVALANCHE_NWSS_GENERAL_MAILBOX_CLK_CTRL_McDMA3);
    *regAddr = reg;

    /* Disable prefetcher PSM */
   /* Configure Prefetcher command */
    pdsp_cmd.pdsp_id    = PDSP_ID_Prefetcher;
    pdsp_cmd.cmd        = PRECMD_INDEX(1) | PRECMD_COMMAND(PDSP_ENABLE_PREFETCH);
    pdsp_cmd.params[0]  = 0;
    pdsp_cmd.params_len = 0;

    printk("%s: Disable prefetcher PSM mode\n", __FUNCTION__);

    if ((rc = pdsp_cmd_send( pdsp_cmd.pdsp_id, pdsp_cmd.cmd, NULL, 0, pdsp_cmd.params, 1 )) && (rc != -8))
    {
        printk(KERN_ERR"\n%s: failed to put command(%X) to the PDSP %d rc(%d)\n", __FUNCTION__, pdsp_cmd.cmd, pdsp_cmd.pdsp_id, rc );
        return (PP_RC_FAILURE);
    }

    printk("%s: Disable PSM mode\n", __FUNCTION__);
    
    /* Set PP to run */
    enablePsm = 0;
    if (PP_RC_SUCCESS != (rc = pdsp_control( 1, PDSPCTRL_RESUME, (Ptr)&enablePsm )))
    {
        printk(KERN_ERR"\n%s: failed to disable PP PSM mode rc(%d)\n", __FUNCTION__, rc );
        return (PP_RC_FAILURE);
    }


    /* Send PSM disable Cmd to PP */
    pdsp_cmd.pdsp_id = PDSP_ID_Classifier;
    pdsp_cmd.cmd = WORD_S0_B1_0 (0x00, 0x0, PDSP_SETPSM);
    pdsp_cmd.params[0] = 0;   
    pdsp_cmd.params_len =0;

    if (PP_RC_SUCCESS != (rc = pdsp_cmd_send( pdsp_cmd.pdsp_id, pdsp_cmd.cmd, pdsp_cmd.params, 0, pdsp_cmd.params, 0 )))
    {
        printk(KERN_ERR"\n%s: failed to put command(%X) to the PDSP %d rc(%d)\n", __FUNCTION__, pdsp_cmd.cmd, pdsp_cmd.pdsp_id, rc );
        return (PP_RC_FAILURE);
    }

    if (PAL_osTimerStart( gPpPollTimer.timer_handle, gPpPollTimer.timer_poll_time_msec ))
    {
        return (PP_RC_FAILURE);
    }


    PP_DB_LOCK();
    PP_DB.status = PP_DB_STATUS_ACTIVE;
    PP_DB_UNLOCK();

    return PP_RC_SUCCESS;
  
}

/**************************************************************************
 * FUNCTION NAME : AVALANCHE_PP_RET_e avalanche_pp_psm ( Uint8 onOff )
 **************************************************************************
 * DESCRIPTION   :
 *  This function is called to disable / enable Power Saving mode (PSM)
 *  param[in] onOff - "1" = disable "0" = enable
 * RETURNS       :
 *      0  -   Success
 *      >0  -   Error
 **************************************************************************/
AVALANCHE_PP_RET_e    avalanche_pp_psm        ( Uint8     onOff )
{
    if (onOff)
    {
        avalanche_pp_disable_psm();    
    }
    else
    {
        /* flush all sessions */
        avalanche_pp_flush_sessions( AVALANCHE_PP_MAX_VPID, PP_LIST_ID_ALL );
        avalanche_pp_enable_psm();

    }
    
    return (PP_RC_SUCCESS);
}

/**************************************************************************
 * FUNCTION NAME : AVALANCHE_PP_RET_e avalanche_pp_set_ack_suppression ( Uint8 enDis)
 **************************************************************************
 * DESCRIPTION   :
 *  The function sets the packet processor to do Ack Suppression or not to
 *  do in case Tdox is Enabled.
 *  param[in] enDis - "1" = Ack Suppression disable "0" = Ack Suppression enable
 * RETURNS       :
 *      0  -   Success
 *      >0  -   Error
 **************************************************************************/
AVALANCHE_PP_RET_e    avalanche_pp_set_ack_suppression  ( Uint8    enDis)
{
    Int32               rc;
    pdsp_cmd_params_t   pdsp_cmd;

    /* Send ack_suppression disable/enable Cmd to PP */
    pdsp_cmd.pdsp_id = PDSP_ID_Classifier;
    pdsp_cmd.cmd = WORD_S0_B1_0 (enDis, 0x0, PDSP_SET_ACK_SUPP);
    pdsp_cmd.params[0] = 0;   
    pdsp_cmd.params_len =0;

    if (PP_RC_SUCCESS != (rc = pdsp_cmd_send( pdsp_cmd.pdsp_id, pdsp_cmd.cmd, pdsp_cmd.params, 0, pdsp_cmd.params, 0 )))
    {
        printk(KERN_ERR"\n%s: failed to put command(%X) to the PDSP %d rc(%d)\n", __FUNCTION__, pdsp_cmd.cmd, pdsp_cmd.pdsp_id, rc );
        return (PP_RC_FAILURE);
    }
    
    return (PP_RC_SUCCESS);
}

/**************************************************************************
 * FUNCTION NAME : AVALANCHE_PP_RET_e avalanche_pp_hw_init (void)
 **************************************************************************
 * DESCRIPTION   :
 *  The function initiolazed packet processor hw
 * RETURNS       :
 *      0  -   Success
 *      >0  -   Error
 **************************************************************************/
AVALANCHE_PP_RET_e    avalanche_pp_hw_init        (void)
{
    int reg;

    /* Set clock to PP peripherals (Only those that are used) */
     reg = AVALANCHE_NWSS_GENERAL_MAILBOX_CLK_CTRL_PPDSP             |
          AVALANCHE_NWSS_GENERAL_MAILBOX_CLK_CTRL_CPDSP1            |
          AVALANCHE_NWSS_GENERAL_MAILBOX_CLK_CTRL_CPDSP2         	|
          AVALANCHE_NWSS_GENERAL_MAILBOX_CLK_CTRL_MPDSP             |
          AVALANCHE_NWSS_GENERAL_MAILBOX_CLK_CTRL_QPDSP             |
          // AVALANCHE_NWSS_GENERAL_MAILBOX_CLK_CTRL_UsPrefPDSP     |
          AVALANCHE_NWSS_GENERAL_MAILBOX_CLK_CTRL_PrefSharedRAM     |
          AVALANCHE_NWSS_GENERAL_MAILBOX_CLK_CTRL_SessSharedRAM     |
          AVALANCHE_NWSS_GENERAL_MAILBOX_CLK_CTRL_MiscSharedRam     |
          AVALANCHE_NWSS_GENERAL_MAILBOX_CLK_CTRL_Counters          |
          AVALANCHE_NWSS_GENERAL_MAILBOX_CLK_CTRL_CDMA0             |
          AVALANCHE_NWSS_GENERAL_MAILBOX_CLK_CTRL_CDMA1             |
          AVALANCHE_NWSS_GENERAL_MAILBOX_CLK_CTRL_CDMA2             |
          AVALANCHE_NWSS_GENERAL_MAILBOX_CLK_CTRL_CDMA3             |
          AVALANCHE_NWSS_GENERAL_MAILBOX_CLK_CTRL_McDMA0            |
          AVALANCHE_NWSS_GENERAL_MAILBOX_CLK_CTRL_McDMA1            |
          // AVALANCHE_NWSS_GENERAL_MAILBOX_CLK_CTRL_McDMA2         |
          AVALANCHE_NWSS_GENERAL_MAILBOX_CLK_CTRL_McDMA3            |
          AVALANCHE_NWSS_GENERAL_MAILBOX_CLK_CTRL_LUT1              |
          AVALANCHE_NWSS_GENERAL_MAILBOX_CLK_CTRL_LUT2;           	//|
          // AVALANCHE_NWSS_GENERAL_MAILBOX_CLK_CTRL_LUT3);
    *(Uint32*)AVALANCHE_NWSS_GENERAL_MAILBOX_CLK_CTRL_REG = reg;

    memset( (void *)IO_PHY2VIRT(0x03200000), 0,  0x8040 );
    memset( (void *)IO_PHY2VIRT(0x03300000), 0, 0x2D000 );
    memset( (void *)IO_PHY2VIRT(0x03400300), 0,  0x1200 );


    return (PP_RC_SUCCESS);
}





/* Advanced HOOKS */
/**************************************************************************
 * FUNCTION NAME : AVALANCHE_PP_RET_e avalanche_pp_session_list_execute ( Uint8 vpid_handle, PP_LIST_ID_e list_id, AVALANCHE_EXEC_HOOK_FN_t handler, Ptr data )
 **************************************************************************
 * DESCRIPTION   :
 *  The function destroys the sessions according to specified criteria
 *  param[in] vpid_handle - handle of the VPID
 *  param[in] list_id - ingress / egress / tcp / tdox
 *  param[in] handler - 
 *  param[in] data - 
 * RETURNS       :
 *      0  -   Success
 *      >0  -   Error
 **************************************************************************/
AVALANCHE_PP_RET_e    avalanche_pp_session_list_execute( Uint8     vpid_handle, PP_LIST_ID_e   list_id,    AVALANCHE_EXEC_HOOK_FN_t   handler, Ptr  data )
{
    PP_DB_Session_Entry_t *             session_db_ptr  = NULL;
    PP_DB_Session_LUT2_hash_entry_t *   lut2_hash_ptr   = NULL;
    struct list_head *                  pos         = NULL;
    struct list_head *                  pos_next    = NULL;
    struct list_head *                  head        = NULL;

    if (vpid_handle > AVALANCHE_PP_MAX_VPID)
    {
        return (PP_RC_INVALID_PARAM);
    }

    if (list_id > PP_LIST_ID_ALL)
    {
        return (PP_RC_INVALID_PARAM);
    }

    if (NULL == handler)
    {
        return (PP_RC_INVALID_PARAM);
    }

    PP_DB_LOCK();
    {
        if ( AVALANCHE_PP_MAX_VPID == vpid_handle )
        {
            AVALANCHE_PP_SESSIONS_POOL_ID_e     pool;

            for ( pool = 0; pool < AVALANCHE_PP_SESSIONS_POOL_MAX; pool++)
            {
                head = &PP_DB.pool_lut2[ pool ][ PP_DB_POOL_BUSY ]; 

                for (pos = head->next; prefetch(pos->next), pos != (head); pos = pos_next)
                {
                    pos_next = pos->next;
                    lut2_hash_ptr = list_entry( pos, PP_DB_Session_LUT2_hash_entry_t, link );
                    handler( &PP_DB.repository_sessions[ lut2_hash_ptr->handle ].session_info, data );
                }
            }
        }
        else
        {
            PP_LIST_ID_e    list_start  = list_id;
            PP_LIST_ID_e    list_end    = list_id;

            if ( list_id == PP_LIST_ID_ALL )
            {
                list_start = PP_LIST_ID_INGRESS;
                list_end   = PP_LIST_ID_EGRESS;
            }

            for (list_id = list_start; list_id <= list_end; list_id++)
            {
                head = &PP_DB.repository_VPIDs[ vpid_handle ].list[ list_id ];

                for (pos = head->next; prefetch(pos->next), pos != (head); pos = pos_next)
                {
                    pos_next = pos->next;
                    session_db_ptr = list_entry( pos, PP_DB_Session_Entry_t, list[ list_id ] );
                    handler( &session_db_ptr->session_info, data );
                }
            }
        }
    }
    PP_DB_UNLOCK();

    return (PP_RC_SUCCESS);
}

/**************************************************************************
 * FUNCTION NAME : AVALANCHE_PP_RET_e avalanche_pp_session_pre_action_bind ( Uint8 vpid_handle, AVALANCHE_EXEC_HOOK_FN_t handler, Ptr data )
 **************************************************************************
 * DESCRIPTION   :
 *  param[in] vpid_handle - handle of the VPID
 *  param[in] handler - 
 *  param[in] data - 
 * RETURNS       :
 *      0  -   Success
 *      >0  -   Error
 **************************************************************************/
AVALANCHE_PP_RET_e    avalanche_pp_session_pre_action_bind   ( Uint8     vpid_handle,  AVALANCHE_EXEC_HOOK_FN_t  handler, Ptr   data )
{
    if (vpid_handle >= AVALANCHE_PP_MAX_VPID)
    {
        return (PP_RC_INVALID_PARAM);
    }

    PP_DB_LOCK();
    PP_DB.repository_VPIDs[ vpid_handle ].session_pre_action_cb = handler;
    PP_DB.repository_VPIDs[ vpid_handle ].session_pre_data      = data;
    PP_DB_UNLOCK();

    return (PP_RC_SUCCESS);
}

/**************************************************************************
 * FUNCTION NAME : AVALANCHE_PP_RET_e avalanche_pp_session_post_action_bind ( Uint8 vpid_handle, AVALANCHE_EXEC_HOOK_FN_t handler, Ptr data )
 **************************************************************************
 * DESCRIPTION   :
 *  param[in] vpid_handle - handle of the VPID
 *  param[in] handler - 
 *  param[in] data - 
 * RETURNS       :
 *      0  -   Success
 *      >0  -   Error
 **************************************************************************/
AVALANCHE_PP_RET_e    avalanche_pp_session_post_action_bind  ( Uint8     vpid_handle,  AVALANCHE_EXEC_HOOK_FN_t  handler, Ptr   data )
{
    if (vpid_handle >= AVALANCHE_PP_MAX_VPID)
    {
        return (PP_RC_INVALID_PARAM);
    }

    PP_DB_LOCK();
    PP_DB.repository_VPIDs[ vpid_handle ].session_post_action_cb = handler;
    PP_DB.repository_VPIDs[ vpid_handle ].session_post_data      = data;
    PP_DB_UNLOCK();

    return (PP_RC_SUCCESS);
}

static AVALANCHE_PP_RET_e  __avalanche_pp_counter64_read( Uint64 * dest, volatile Ptr src )
{
    union
    {
        Uint64  lll;
        struct
        {
            Uint32  high;
            Uint32  low;
        }s;
    }
    counter64;

    
    counter64.s.high = *(Uint32 *)(src + 4);
    counter64.s.low  = *(Uint32 *)(src);

    if(counter64.s.high != *(Uint32 *)(src + 4))
    {
        counter64.s.high = *(Uint32 *)(src + 4);
        counter64.s.low  = *(Uint32 *)(src);
    }

    *dest = counter64.lll;

    return (PP_RC_SUCCESS);
}

/* Statistics API */
/**************************************************************************
 * FUNCTION NAME : AVALANCHE_PP_RET_e avalanche_pp_get_stats_session ( Uint32 session_handle, AVALANCHE_PP_SESSION_STATS_t* ptr_session_stats )
 **************************************************************************
 * DESCRIPTION   :
 *  The function is called to get the statistics of a particular session.
 *  param[in] session_handle - handle of the session
 *  param[in] ptr_session_stats - pointer to session statistics struct
 * RETURNS       :
 *      0  -   Success
 *      >0  -   Error
 **************************************************************************/
AVALANCHE_PP_RET_e   avalanche_pp_get_stats_session   ( Uint32 session_handle,    AVALANCHE_PP_SESSION_STATS_t*    ptr_session_stats )
{
    volatile Uint32 *    stats_rgn;

    PP_DB_LOCK();
    if (PP_DB.status != PP_DB_STATUS_ACTIVE)
    {
        if (PP_DB.status == PP_DB_STATUS_PSM)
        {
            /* In PSM mode we do not fail the operation but do not inquire the PP since it is down */
            PP_DB_UNLOCK();
            return PP_RC_SUCCESS;
        }
        else
        {
            printk("ERROR: PP Operation %s cannot be accomplished while PP status is %s\n", __FUNCTION__,  "INACTIVE"); 
            PP_DB_UNLOCK();
            return PP_RC_FAILURE;
        }
    }
    PP_DB_UNLOCK();

    stats_rgn = (Uint32 *)IO_PHY2VIRT( PP_HAL_COUNTERS_SESSION_PKTS_BASE_PHY );

    ptr_session_stats->packets_forwarded = stats_rgn[ session_handle ];

    stats_rgn = (Uint32 *)IO_PHY2VIRT( PP_HAL_COUNTERS_SESSION_BYTES_BASE_PHY );

    __avalanche_pp_counter64_read( &ptr_session_stats->bytes_forwarded, &stats_rgn[ session_handle * 2 ] );

    return (PP_RC_SUCCESS);
}

/**************************************************************************
 * FUNCTION NAME : AVALANCHE_PP_RET_e avalanche_pp_get_stats_vpid ( Uint8 vpid_handle, AVALANCHE_PP_VPID_STATS_t* ptr_vpid_stats )
 **************************************************************************
 * DESCRIPTION   :
 *  The function is called to get the statistics of a particular VPID.
 *  param[in] vpid_handle - handle of the VPID
 *  param[in] ptr_vpid_stats - pointer to VPID statistics struct
 * RETURNS       :
 *      0  -   Success
 *      >0  -   Error
 **************************************************************************/
AVALANCHE_PP_RET_e   avalanche_pp_get_stats_vpid      ( Uint8  vpid_handle,       AVALANCHE_PP_VPID_STATS_t*       ptr_vpid_stats )
{
    volatile Uint32 *    stats_rgn_64 = (IO_PHY2VIRT( PP_HAL_COUNTERS_VPID_64BITS_BASE_PHY ) + vpid_handle * PP_COUNTERS_VPID_64BITS_ENTRY_SIZE);
    volatile Uint32 *    stats_rgn_32 = (IO_PHY2VIRT( PP_HAL_COUNTERS_VPID_32BITS_BASE_PHY ) + vpid_handle * PP_COUNTERS_VPID_32BITS_ENTRY_SIZE);

    PP_DB_LOCK();
    if (PP_DB.status != PP_DB_STATUS_ACTIVE)
    {
        if (PP_DB.status == PP_DB_STATUS_UNINITIALIZED)
        {
            printk("ERROR: PP Operation %s cannot be accomplished while PP status is %s\n", __FUNCTION__,  "INACTIVE"); 
            PP_DB_UNLOCK();
            return PP_RC_FAILURE;
        }
        else
        {
            /* In PSM mode we do not fail the operation but do not inquire the PP since it is down */
            PP_DB_UNLOCK();
            return PP_RC_SUCCESS;
        }
    }
    PP_DB_UNLOCK();

    __avalanche_pp_counter64_read( &ptr_vpid_stats->rx_byte, &stats_rgn_64[ PP_COUNTERS_VPID_64BITS_RX_BYTES_LSB_OFF/4 ] );

    ptr_vpid_stats->rx_unicast_pkt   = stats_rgn_64[ PP_COUNTERS_VPID_64BITS_RX_UCAST_PKTS_LSB_OFF/4 ];
    ptr_vpid_stats->rx_broadcast_pkt = stats_rgn_32[ PP_COUNTERS_VPID_32BITS_RX_BCAST_PKTS_OFF    /4 ];
    ptr_vpid_stats->rx_multicast_pkt = stats_rgn_32[ PP_COUNTERS_VPID_32BITS_RX_MCAST_PKTS_OFF    /4 ];
    ptr_vpid_stats->rx_discard_pkt   = stats_rgn_32[ PP_COUNTERS_VPID_32BITS_RX_DISCARDS_OFF      /4 ];


    __avalanche_pp_counter64_read( &ptr_vpid_stats->tx_byte, &stats_rgn_64[ PP_COUNTERS_VPID_64BITS_TX_BYTES_LSB_OFF/4 ] );

    ptr_vpid_stats->tx_unicast_pkt   = stats_rgn_64[ PP_COUNTERS_VPID_64BITS_TX_UCAST_PKTS_LSB_OFF/4 ];
    ptr_vpid_stats->tx_broadcast_pkt = stats_rgn_32[ PP_COUNTERS_VPID_32BITS_TX_BCAST_PKTS_OFF    /4 ];
    ptr_vpid_stats->tx_multicast_pkt = stats_rgn_32[ PP_COUNTERS_VPID_32BITS_TX_MCAST_PKTS_OFF    /4 ];
    ptr_vpid_stats->tx_discard_pkt   = stats_rgn_32[ PP_COUNTERS_VPID_32BITS_TX_DISCARDS_OFF      /4 ];

    ptr_vpid_stats->tx_error         = stats_rgn_32[ PP_COUNTERS_VPID_32BITS_TX_ERRORS_OFF        /4 ];

    return (PP_RC_SUCCESS);
}

/**************************************************************************
 * FUNCTION NAME : AVALANCHE_PP_RET_e avalanche_pp_get_stats_global ( AVALANCHE_PP_GLOBAL_STATS_t* ptr_stats )
 **************************************************************************
 * DESCRIPTION   :
 *  The function is called to get the statistics of the packet processor.
 *  param[in] ptr_stats - pointer to global statistics struct
 * RETURNS       :
 *      0  -   Success
 *      >0  -   Error
 **************************************************************************/
AVALANCHE_PP_RET_e   avalanche_pp_get_stats_global    (                           AVALANCHE_PP_GLOBAL_STATS_t*     ptr_stats )
{
    PP_DB_LOCK();
    if (PP_DB.status != PP_DB_STATUS_ACTIVE)
    {
        if (PP_DB.status == PP_DB_STATUS_UNINITIALIZED)
        {
            printk("ERROR: PP Operation %s cannot be accomplished while PP status is %s\n", __FUNCTION__,  "INACTIVE"); 
            PP_DB_UNLOCK();
            return PP_RC_FAILURE;
        }
        else
        {
            /* In PSM mode we do not fail the operation but do not inquire the PP since it is down */
            PP_DB_UNLOCK();
            return PP_RC_SUCCESS;
        }
    }
    PP_DB_UNLOCK();

    ptr_stats->ppdsp_rx_pkts                = *(Uint32*)(IO_PHY2VIRT(PP_HAL_COUNTERS_PPDSP_BASE_PHY) + PP_COUNTERS_PPDSP_RX_PKTS_OFF                );
    ptr_stats->ppdsp_pkts_frwrd_to_cpdsp1   = *(Uint32*)(IO_PHY2VIRT(PP_HAL_COUNTERS_PPDSP_BASE_PHY) + PP_COUNTERS_PPDSP_PKTS_FRWRD_TO_CPDSP1_OFF   );
    ptr_stats->ppdsp_not_enough_descriptors = *(Uint32*)(IO_PHY2VIRT(PP_HAL_COUNTERS_PPDSP_BASE_PHY) + PP_COUNTERS_PPDSP_NOT_ENOUGH_DESCRIPTORS_OFF );

    ptr_stats->cpdsp1_rx_pkts               = *(Uint32*)(IO_PHY2VIRT(PP_HAL_COUNTERS_CPDSP1_BASE_PHY) + PP_COUNTERS_CPDSP1_RX_PKTS_OFF              );
    ptr_stats->cpdsp1_lut1_search_attempts  = *(Uint32*)(IO_PHY2VIRT(PP_HAL_COUNTERS_CPDSP1_BASE_PHY) + PP_COUNTERS_CPDSP1_LUT1_SEARCHES_OFF        );
    ptr_stats->cpdsp1_lut1_matches          = *(Uint32*)(IO_PHY2VIRT(PP_HAL_COUNTERS_CPDSP1_BASE_PHY) + PP_COUNTERS_CPDSP1_LUT1_MATCHES_OFF         );
    ptr_stats->cpdsp1_pkts_frwrd_to_cpdsp2  = *(Uint32*)(IO_PHY2VIRT(PP_HAL_COUNTERS_CPDSP1_BASE_PHY) + PP_COUNTERS_CPDSP1_PKTS_FRWRD_TO_CPDSP2_OFF );
                                                         
    ptr_stats->cpdsp2_rx_pkts               = *(Uint32*)(IO_PHY2VIRT(PP_HAL_COUNTERS_CPDSP2_BASE_PHY) + PP_COUNTERS_CPDSP2_RX_PKTS_OFF              );
    ptr_stats->cpdsp2_lut2_search_attempts  = *(Uint32*)(IO_PHY2VIRT(PP_HAL_COUNTERS_CPDSP2_BASE_PHY) + PP_COUNTERS_CPDSP2_LUT2_SEARCHES_OFF        );
    ptr_stats->cpdsp2_lut2_matches          = *(Uint32*)(IO_PHY2VIRT(PP_HAL_COUNTERS_CPDSP2_BASE_PHY) + PP_COUNTERS_CPDSP2_LUT2_MATCHES_OFF         );
    ptr_stats->cpdsp2_pkts_frwrd_to_mpdsp   = *(Uint32*)(IO_PHY2VIRT(PP_HAL_COUNTERS_CPDSP2_BASE_PHY) + PP_COUNTERS_CPDSP2_PKTS_FRWRD_TO_MPDSP_OFF  );
    ptr_stats->cpdsp2_synch_timeout_events  = *(Uint32*)(IO_PHY2VIRT(PP_HAL_COUNTERS_CPDSP2_BASE_PHY) + PP_COUNTERS_CPDSP2_SYNCH_TIMEOUT_EVENTS_OFF );
    ptr_stats->cpdsp2_reassembly_db_full    = *(Uint32*)(IO_PHY2VIRT(PP_HAL_COUNTERS_CPDSP2_BASE_PHY) + PP_COUNTERS_CPDSP2_REASSEMBLY_DB_FULL       );
    ptr_stats->cpdsp2_reassembly_db_timeout = *(Uint32*)(IO_PHY2VIRT(PP_HAL_COUNTERS_CPDSP2_BASE_PHY) + PP_COUNTERS_CPDSP2_REASSEMBLY_DB_TIMEOUT    );
                                                         
    ptr_stats->mpdsp_rx_pkts                = *(Uint32*)(IO_PHY2VIRT(PP_HAL_COUNTERS_MPDSP_BASE_PHY) + PP_COUNTERS_MPDSP_PKTS_RECEIVED              );
    ptr_stats->mpdsp_ipv4_rx_pkts           = *(Uint32*)(IO_PHY2VIRT(PP_HAL_COUNTERS_MPDSP_BASE_PHY) + PP_COUNTERS_MPDSP_IPV4_PKTS_RECEIVED         );
    ptr_stats->mpdsp_ipv6_rx_pkts           = *(Uint32*)(IO_PHY2VIRT(PP_HAL_COUNTERS_MPDSP_BASE_PHY) + PP_COUNTERS_MPDSP_IPV6_PKTS_RECEIVED         );
    ptr_stats->mpdsp_frwrd_to_host          = *(Uint32*)(IO_PHY2VIRT(PP_HAL_COUNTERS_MPDSP_BASE_PHY) + PP_COUNTERS_MPDSP_PKTS_FRWRD_TO_HOST         );
    ptr_stats->mpdsp_frwrd_to_qpdsp         = *(Uint32*)(IO_PHY2VIRT(PP_HAL_COUNTERS_MPDSP_BASE_PHY) + PP_COUNTERS_MPDSP_PKTS_FRWRD_TO_QPDSP        );
    ptr_stats->mpdsp_frwrd_to_synch_q       = *(Uint32*)(IO_PHY2VIRT(PP_HAL_COUNTERS_MPDSP_BASE_PHY) + PP_COUNTERS_MPDSP_PKTS_FRWRD_TO_SYNCHQ       );
    ptr_stats->mpdsp_discards               = *(Uint32*)(IO_PHY2VIRT(PP_HAL_COUNTERS_MPDSP_BASE_PHY) + PP_COUNTERS_MPDSP_DISCARDS                   );
    ptr_stats->mpdsp_synchq_overflow_events = *(Uint32*)(IO_PHY2VIRT(PP_HAL_COUNTERS_MPDSP_BASE_PHY) + PP_COUNTERS_MPDSP_SYNCH_OVERFLOW_EVENTS      );

    ptr_stats->qpdsp_ooo_discards           = *(Uint32*)(IO_PHY2VIRT(PP_HAL_COUNTERS_CPDSP2_BASE_PHY) + PP_COUNTERS_QPDSP_OOO_DISCARDS_OFF      );

    return (PP_RC_SUCCESS);
}

/**************************************************************************
 * FUNCTION NAME : AVALANCHE_PP_RET_e avalanche_pp_event_handler_register ( Uint32 * handle_event_handler, AVALANCHE_EVENT_HANDLER_t handler )
 **************************************************************************
 * DESCRIPTION   :
 *  The function is the dispatcher code that passes events from PP
 *  entities to the registered event handler.
 *  param[in] handle_event_handler - pointer to event handle 
 *  param[in] handler - hendler structure
 * RETURNS       :
 *      0  -   Success
 *      >0  -   Error
 **************************************************************************/
AVALANCHE_PP_RET_e   avalanche_pp_event_handler_register     ( Uint32 *  handle_event_handler, AVALANCHE_EVENT_HANDLER_t   handler )
{
    PP_DB_EventHandler_Entry_t * entry;

    if ((NULL == handler) || (NULL == handle_event_handler))
    {
        return (PP_RC_INVALID_PARAM);
    }

    entry = kmalloc( sizeof(PP_DB_EventHandler_Entry_t), GFP_KERNEL );

    if (NULL == entry)
    {
        return (PP_RC_OUT_OF_MEMORY);
    }

    entry->handler = handler;

    PP_DB_LOCK();

    list_add( &entry->link, &PP_DB.eventHandlers );

    PP_DB_UNLOCK();

    *handle_event_handler = (Uint32)entry;

    return (PP_RC_SUCCESS);
}

/**************************************************************************
 * FUNCTION NAME : AVALANCHE_PP_RET_e avalanche_pp_event_handler_unregister ( Uint32 handle_event_handler )
 **************************************************************************
 * DESCRIPTION   :
 *  The function is the dispatcher code that unregistered event handler.
 *  param[in] handle_event_handler - event handler handle
 * RETURNS       :
 *      0  -   Success
 *      >0  -   Error
 **************************************************************************/
AVALANCHE_PP_RET_e   avalanche_pp_event_handler_unregister   ( Uint32    handle_event_handler )
{
    PP_DB_EventHandler_Entry_t * entry = (PP_DB_EventHandler_Entry_t *)handle_event_handler;

    if (NULL == entry)
    {
        return (PP_RC_INVALID_PARAM);
    }

    PP_DB_LOCK();

    list_del( &entry->link );

    PP_DB_UNLOCK();

    kfree( entry );

    return (PP_RC_SUCCESS);
}

/**************************************************************************
 * FUNCTION NAME : AVALANCHE_PP_RET_e avalanche_pp_event_report( AVALANCHE_PP_EVENT_e event, Uint32 param1, Uint32 param2 )
 **************************************************************************
 * DESCRIPTION   :
 *  param[in] event -  
 *  param[in] param1 - 
 *  param[in] param2 - 
 * RETURNS       :
 *      0  -   Success
 *      >0  -   Error
 **************************************************************************/
AVALANCHE_PP_RET_e   avalanche_pp_event_report( AVALANCHE_PP_EVENT_e  event, Uint32 param1, Uint32 param2 )
{
    struct list_head *   pos = NULL;
    AVALANCHE_PP_RET_e  rc = PP_RC_SUCCESS;

    list_for_each( pos, &PP_DB.eventHandlers )
    {
        PP_DB_EventHandler_Entry_t * entry;
        entry = list_entry(pos, PP_DB_EventHandler_Entry_t, link);

        rc |= entry->handler( event, param1, param2 );
    }

    return (rc);
}



/* ******************************************************************** */
/*                                                                      */
/*                       ___       ____                                 */
/*                      / _ \  ___/ ___|                                */
/*                     | | | |/ _ \___ \                                */
/*                     | |_| | (_) |__) |                               */
/*                      \__\_\\___/____/                                */
/*                                                                      */
/*                                                                      */
/* ******************************************************************** */

/**************************************************************************
 * FUNCTION NAME : AVALANCHE_PP_RET_e avalanche_pp_qos_cluster_setup( Uint8 clst_indx, AVALANCHE_PP_QOS_CLST_CFG_t* clst_cfg )
 **************************************************************************
 * DESCRIPTION   :
 *  This function is called to setup a QoS cluster in PP.
 *  param[in] clst_indx - cluster id 
 *  param[in] clst_cfg - pointer to cluster configuration struct
 * RETURNS       :
 *      0  -   Success
 *      >0  -   Error
 **************************************************************************/
AVALANCHE_PP_RET_e   avalanche_pp_qos_cluster_setup( Uint8     clst_indx,  AVALANCHE_PP_QOS_CLST_CFG_t*    clst_cfg )
{
    Int32   qos_q_cnt;
    Uint8   egr_q_cnt;
    Uint16  egr_q[ AVALANCHE_PP_QOS_CLST_MAX_EGRESS_QCNT ];


    if ((clst_cfg == NULL) || (clst_indx > AVALANCHE_PP_QOS_CLST_MAX_INDX) || (clst_cfg->qos_q_cnt > AVALANCHE_PP_QOS_CLST_MAX_QCNT)) 
    {
        return PP_RC_INVALID_PARAM;
    }

    PP_DB_CHECK_ACTIVE();

    memset(egr_q, 0, sizeof(egr_q));

    /* Find out the Egress queues superset ... */
    for (qos_q_cnt = 0, egr_q_cnt = 0; qos_q_cnt < clst_cfg->qos_q_cnt; qos_q_cnt++)
    {
        AVALANCHE_PP_QOS_QUEUE_t*       qos_q_cfg = &clst_cfg->qos_q_cfg[qos_q_cnt];

        if (qos_q_cfg->q_num > AVALANCHE_PP_QOS_QUEUE_MAX_INDX)
        {
            return (PP_RC_INVALID_PARAM);
        }

        {
            Uint32  i;

            for (i = 0; i < egr_q_cnt; i++)
            {
                if (egr_q[i] == qos_q_cfg->egr_q)
                {
                    /* This egr_q is already configured */
                    break;
                }
            }

            if (unlikely(i >= AVALANCHE_PP_QOS_CLST_MAX_EGRESS_QCNT))
            {
                return (PP_RC_INVALID_PARAM);
            }

            else if (i == egr_q_cnt)
            {
                /* Store the egress queue number to be used for cluster setup */
                egr_q[egr_q_cnt++] = qos_q_cfg->egr_q;
            }
        }
    }

    /* Configure QOS queues */
    for (qos_q_cnt = 0 ; qos_q_cnt < clst_cfg->qos_q_cnt; qos_q_cnt++)
    {
        pp_hal_qos_queue_config_set( &clst_cfg->qos_q_cfg[qos_q_cnt] );
    }

    /* Configure the cluster */
    pp_hal_qos_cluster_config_set( clst_indx, clst_cfg, &egr_q[0], egr_q_cnt );

    return (PP_RC_SUCCESS);
}

/**************************************************************************
 * FUNCTION NAME : AVALANCHE_PP_RET_e avalanche_pp_qos_cluster_enable( Uint8 clst_indx )
 **************************************************************************
 * DESCRIPTION   :
 *  This function enables specified QoS cluster.
 *  param[in] clst_indx - cluster id 
 * RETURNS       :
 *      0  -   Success
 *      >0  -   Error
 **************************************************************************/
AVALANCHE_PP_RET_e   avalanche_pp_qos_cluster_enable( Uint8     clst_indx )
{
    if (clst_indx > AVALANCHE_PP_QOS_CLST_MAX_INDX)
    {
        return PP_RC_INVALID_PARAM;
    }

    PP_DB_CHECK_ACTIVE();

    return ( pp_hal_qos_cluster_enable( clst_indx ) );
}

/**************************************************************************
 * FUNCTION NAME : AVALANCHE_PP_RET_e avalanche_pp_qos_cluster_disable( Uint8 clst_indx )
 **************************************************************************
 * DESCRIPTION   :
 *  This function disables specified QoS cluster.
 *  param[in] clst_indx - cluster id 
 * RETURNS       :
 *      0  -   Success
 *      >0  -   Error
 **************************************************************************/
AVALANCHE_PP_RET_e   avalanche_pp_qos_cluster_disable( Uint8     clst_indx )
{
    if (clst_indx > AVALANCHE_PP_QOS_CLST_MAX_INDX)
    {
        return PP_RC_INVALID_PARAM;
    }

    PP_DB_CHECK_ACTIVE();

    return ( pp_hal_qos_cluster_disable( clst_indx ) );
}

/**************************************************************************
 * FUNCTION NAME : AVALANCHE_PP_RET_e avalanche_pp_qos_get_queue_stats ( Uint32 qos_qnum, AVALANCHE_PP_QOS_QUEUE_STATS_t* stats )
 **************************************************************************
 * DESCRIPTION   :
 *  This function retrieves the QoS statistics for the queue specified from the PP.
 *  param[in] qos_qnum - qos queue id
 *  param[in] stats - pointer to queue statistic struct 
 * RETURNS       :
 *      0  -   Success
 *      >0  -   Error
 **************************************************************************/
AVALANCHE_PP_RET_e   avalanche_pp_qos_get_queue_stats    ( Uint32    qos_qnum,   AVALANCHE_PP_QOS_QUEUE_STATS_t*     stats )
{
    Uint32 queueStcAddr;

    if (qos_qnum > AVALANCHE_PP_QOS_QUEUE_MAX_INDX) 
    {
        return PP_RC_INVALID_PARAM;
    }

    PP_DB_LOCK();
    if (PP_DB.status != PP_DB_STATUS_ACTIVE)
    {
        memset(stats, 0, sizeof(AVALANCHE_PP_QOS_QUEUE_STATS_t));
        if (PP_DB.status == PP_DB_STATUS_UNINITIALIZED)
        {
            printk("ERROR: PP Operation %s cannot be accomplished while PP status is %s\n", __FUNCTION__,  "INACTIVE"); 
            PP_DB_UNLOCK();
            return PP_RC_FAILURE;
        }
        else
        {
            /* In PSM mode we do not fail the operation but do not inquire the PP since it is down */
            PP_DB_UNLOCK();
            return PP_RC_SUCCESS;
        }
    }
    PP_DB_UNLOCK();

    /* Get info from statistical counters */ 
    queueStcAddr = (IO_PHY2VIRT(PP_HAL_COUNTERS_QPDSP_BASE_PHY) + (PP_COUNTERS_QPDSP_Q_OFF * qos_qnum));            
    stats->fwd_pkts = *(Uint32*)(queueStcAddr + PP_COUNTERS_QPDSP_Q_PKT_FRWRD_OFF);
    stats->drp_cnt  = *(Uint32*)(queueStcAddr + PP_COUNTERS_QPDSP_Q_PKT_DROP_OFF);

    return (PP_RC_SUCCESS);
}

/**************************************************************************
 * FUNCTION NAME : AVALANCHE_PP_RET_e avalanche_pp_qos_set_cluster_max_global_credit( Bool creditTypeBytes, Uint8 cluster_id, Uint32 max_global_credit )
 **************************************************************************
 * DESCRIPTION   :
 *  This function set the cluster maximum global credit bytes / packets.
 *  param[in] creditTypeBytes - bytes / packets
 *  param[in] cluster_id - id of the cluster
 *  param[in] max_global_credit - parameter to set for the cluster max credit 
 * RETURNS       :
 *      0  -   Success
 *      >0  -   Error
 **************************************************************************/
AVALANCHE_PP_RET_e   avalanche_pp_qos_set_cluster_max_global_credit( Bool creditTypeBytes, Uint8 cluster_id, Uint32 max_global_credit )
{
    Uint8   egr_q_cnt;
    Uint16  egr_q[ AVALANCHE_PP_QOS_CLST_MAX_EGRESS_QCNT ];

    AVALANCHE_PP_QOS_CLST_CFG_t    clst_cfg;

    PP_DB_CHECK_ACTIVE();

    pp_hal_qos_cluster_config_get( cluster_id, &clst_cfg, &egr_q[0], &egr_q_cnt );
    if ( creditTypeBytes )
    {
        clst_cfg.max_global_credit_bytes    = max_global_credit;
    }
    else
    {
        clst_cfg.max_global_credit_packets  = (Uint16)max_global_credit;
    }
    pp_hal_qos_cluster_config_set( cluster_id, &clst_cfg, &egr_q[0], egr_q_cnt );

    return (PP_RC_SUCCESS);
}

/**************************************************************************
 * FUNCTION NAME : AVALANCHE_PP_RET_e avalanche_pp_qos_set_queue_max_credit( Bool creditTypeBytes, Uint8 queue_id, Uint32 max_credit )
 **************************************************************************
 * DESCRIPTION   :
 *  This function set the qos queue maximum credit bytes / packets.
 *  param[in] creditTypeBytes - bytes / packets
 *  param[in] queue_id - id of the queue
 *  param[in] max_credit - parameter to set for the queue max credit 
 * RETURNS       :
 *      0  -   Success
 *      >0  -   Error
 **************************************************************************/
AVALANCHE_PP_RET_e   avalanche_pp_qos_set_queue_max_credit( Bool creditTypeBytes, Uint8 queue_id, Uint32 max_credit )
{
    AVALANCHE_PP_QOS_QUEUE_t    qos_q_cfg;

    PP_DB_CHECK_ACTIVE();

    qos_q_cfg.q_num = queue_id;

    pp_hal_qos_queue_config_get( &qos_q_cfg );
    if ( creditTypeBytes )
    {
        qos_q_cfg.max_credit_bytes      = max_credit;
    }
    else 
    {
        qos_q_cfg.max_credit_packets    = (Uint16)max_credit;
    }
    pp_hal_qos_queue_config_set( &qos_q_cfg );

    return (PP_RC_SUCCESS);
}

/**************************************************************************
 * FUNCTION NAME : AVALANCHE_PP_RET_e avalanche_pp_qos_set_queue_iteration_credit( Bool creditTypeBytes, Uint8 queue_id, Uint32 it_credit )
 **************************************************************************
 * DESCRIPTION   :
 *  This function set the qos queue iteration credit bytes / packets.
 *  param[in] creditTypeBytes - bytes / packets
 *  param[in] queue_id - id of the queue
 *  param[in] it_credit - parameter to set for the queue iteration credit 
 * RETURNS       :
 *      0  -   Success
 *      >0  -   Error
 **************************************************************************/
AVALANCHE_PP_RET_e   avalanche_pp_qos_set_queue_iteration_credit( Bool creditTypeBytes, Uint8 queue_id, Uint32 it_credit )
{
    AVALANCHE_PP_QOS_QUEUE_t    qos_q_cfg;

    PP_DB_CHECK_ACTIVE();

    qos_q_cfg.q_num = queue_id;

    pp_hal_qos_queue_config_get( &qos_q_cfg );
    if ( creditTypeBytes )
    {
        qos_q_cfg.it_credit_bytes = it_credit;
    }
    else 
    {
        qos_q_cfg.it_credit_packets = (Uint16)it_credit;
    }
    pp_hal_qos_queue_config_set( &qos_q_cfg );

    return (PP_RC_SUCCESS);
}

/**************************************************************************
 * FUNCTION NAME : AVALANCHE_PP_RET_e avalanche_pp_session_tdox_capability_set( Uint32 session_handle, Bool enable )
 **************************************************************************
 * DESCRIPTION   :
 *  This function set the session tdox capability enable / disable.
 *  param[in] session_handle - handle of the session
 *  param[in] enable - "1" = enable, "0" = disable
 * RETURNS       :
 *      0  -   Success
 *      >0  -   Error
 **************************************************************************/
AVALANCHE_PP_RET_e    avalanche_pp_session_tdox_capability_set( Uint32 session_handle, Bool     enable )
{
    struct list_head *          pos             = NULL;
    PP_DB_Entry_t *             tdox_entry      = NULL;
    PP_DB_Session_Entry_t *     ptr_session_db  = NULL;
    AVALANCHE_PP_RET_e          rc = PP_RC_SUCCESS;

    if (AVALANCHE_PP_MAX_ACCELERATED_SESSIONS <= session_handle)
    {
        return (PP_RC_INVALID_PARAM);
    }

    if (enable)
    {
        PP_DB_LOCK();

        if (list_empty( &PP_DB.pool_TDOX[ PP_DB_POOL_FREE ] ))
        {
            PP_DB.stats.tdox_starvation++;
            PP_DB_UNLOCK();
            return (PP_RC_OUT_OF_MEMORY);
        }
        else
        {
            ptr_session_db = &PP_DB.repository_sessions[ session_handle ];

            pos = PP_DB.pool_TDOX[ PP_DB_POOL_FREE ].next;

            tdox_entry = list_entry( pos, PP_DB_Entry_t, link );

            list_move( pos, &PP_DB.pool_TDOX[ PP_DB_POOL_BUSY ] );

            ptr_session_db->session_info.egress.enable |= AVALANCHE_PP_EGRESS_FIELD_ENABLE_TDOX_ENABLED;

            ptr_session_db->session_info.egress.tdox_handle = tdox_entry->handle;
            ptr_session_db->session_info.priority                   = 0;

            if (!list_empty( &ptr_session_db->list[ PP_LIST_ID_EGRESS_TDOX ] ))
            {
                printk("%s[%d]: Error - Trying to enable TDOX for session=%d which is already in TDOX list\n", __FUNCTION__, __LINE__, session_handle);
            }
            else
            {
                list_add( &ptr_session_db->list[ PP_LIST_ID_EGRESS_TDOX ],
                          &PP_DB.repository_VPIDs[ ptr_session_db->session_info.egress.vpid_handle ].list[ PP_LIST_ID_EGRESS_TDOX ] );
            }
        }

        rc = pp_hal_session_tdox_update( &ptr_session_db->session_info );

        PP_DB_UNLOCK();
    }
    else
    {
        PP_DB_LOCK();

        ptr_session_db = &PP_DB.repository_sessions[ session_handle ];

        if ( AVALANCHE_PP_EGRESS_FIELD_ENABLE_TDOX_ENABLED & ptr_session_db->session_info.egress.enable )
        {
            if (!list_empty( &ptr_session_db->list[ PP_LIST_ID_EGRESS_TDOX ] ))
            {
                list_del_init( &ptr_session_db->list[ PP_LIST_ID_EGRESS_TDOX ] );
            }
            else
            {
                printk("%s[%d]: Error - Trying to disable TDOX for session=%d which is not found in TDOX list\n", __FUNCTION__, __LINE__, session_handle);
            }

            list_move( &PP_DB.repository_TDOX[ ptr_session_db->session_info.egress.tdox_handle ].link, 
                       &PP_DB.pool_TDOX[ PP_DB_POOL_FREE ] );

            ptr_session_db->session_info.egress.enable &= ~AVALANCHE_PP_EGRESS_FIELD_ENABLE_TDOX_ENABLED;
        }

        rc = pp_hal_session_tdox_update( &ptr_session_db->session_info );

        PP_DB_UNLOCK();
    }

    return (rc);
}

/**************************************************************************
 * FUNCTION NAME : AVALANCHE_PP_RET_e avalanche_pp_session_tdox_capability_get( Uint32 session_handle, Bool * enable )
 **************************************************************************
 * DESCRIPTION   :
 *  This function get the session tdox capability enable / disable.
 *  param[in] session_handle - handle of the session
 *  param[in] enable - pointer to set the tdox capability status
 * RETURNS       :
 *      0  -   Success
 *      >0  -   Error
 **************************************************************************/
AVALANCHE_PP_RET_e    avalanche_pp_session_tdox_capability_get( Uint32 session_handle, Bool *   enable )
{
    if (AVALANCHE_PP_MAX_ACCELERATED_SESSIONS <= session_handle)
    {
        return (PP_RC_INVALID_PARAM);
    }

    pp_hal_session_tdox_get( session_handle, enable );

    return (PP_RC_SUCCESS);
}

/**************************************************************************
 * FUNCTION NAME : AVALANCHE_PP_RET_e avalanche_pp_version_get( AVALANCHE_PP_VERSION_t * version )
 **************************************************************************
 * DESCRIPTION   :
 *  The function is called to get the version information from the packet processor.
 *  param[in] version - pointer to version struct 
 * RETURNS       :
 *      0  -   Success
 *      >0  -   Error
 **************************************************************************/
AVALANCHE_PP_RET_e      avalanche_pp_version_get( AVALANCHE_PP_VERSION_t * version )
{
    PP_DB_CHECK_ACTIVE();

    return pp_hal_version_get(version);
}

/**************************************************************************
 * FUNCTION NAME : AVALANCHE_PP_RET_e avalanche_pp_set_mta_mac_address ( Uint8 * mtaAddress )
 **************************************************************************
 * DESCRIPTION   :
 *  The function sets MTA MAC address for the packet processor.
 *  param[in] mtaAddress - pointer to MTA MAC address 
 * RETURNS       :
 *      0  -   Success
 *      >0  -   Error
 **************************************************************************/
AVALANCHE_PP_RET_e    avalanche_pp_set_mta_mac_address ( Uint8 * mtaAddress )
{
    PP_DB_CHECK_ACTIVE();

    return pp_hal_mta_address_set( mtaAddress );
}

/**************************************************************************
 * FUNCTION NAME : Bool avalanche_pp_state_is_active ( void )
 **************************************************************************
 * DESCRIPTION   :
 *  This function check if PP status is active
 * RETURNS       :
 *      True  -   PP not active
 *      False  -   PP active
 **************************************************************************/
Bool                  avalanche_pp_state_is_active( void )
{
    Bool rc;

    PP_DB_LOCK();
    rc = (PP_DB_STATUS_ACTIVE == PP_DB.status);
    PP_DB_UNLOCK();

    return (rc);
}

/**************************************************************************
 * FUNCTION NAME : Bool avalanche_pp_state_is_psm( void )
 **************************************************************************
 * DESCRIPTION   :
 *  This function check if PP status is psm mode
 * RETURNS       :
 *      True  -   PP not in psm mode
 *      False  -   PP in psm mode
 **************************************************************************/
Bool                  avalanche_pp_state_is_psm( void )
{
    Bool rc;

    PP_DB_LOCK();
    rc = (PP_DB_STATUS_PSM == PP_DB.status);
    PP_DB_UNLOCK();

    return (rc);
}

/**************************************************************************
 * FUNCTION NAME : AVALANCHE_PP_RET_e avalanche_pp_get_db_stats ( AVALANCHE_PP_Misc_Statistics_t * stats_ptr )
 **************************************************************************
 * DESCRIPTION   :
 *  The function is called to get the statistics of the PP DB.
 *  param[in] stats_ptr - pointer to set the PP DB statistics 
 * RETURNS       :
 *      0  -   Success
 *      >0  -   Error
 **************************************************************************/
AVALANCHE_PP_RET_e    avalanche_pp_get_db_stats ( AVALANCHE_PP_Misc_Statistics_t * stats_ptr )
{
    memcpy((void*)stats_ptr, (void*)&PP_DB.stats, sizeof(AVALANCHE_PP_Misc_Statistics_t));
    return (PP_RC_SUCCESS);
}

/**************************************************************************
 * FUNCTION NAME : AVALANCHE_PP_RET_e avalanche_pp_reset_db_stats ( void )
 **************************************************************************
 * DESCRIPTION   :
 *  The function reset the PP DB statistics.
 * RETURNS       :
 *      0  -   Success
 *      >0  -   Error
 **************************************************************************/
AVALANCHE_PP_RET_e    avalanche_pp_reset_db_stats ( void )
{
    Uint32 i;

    PP_DB_LOCK();

    PP_DB.stats.max_active_lut1_keys = 0;
    PP_DB.stats.max_active_sessions = 0;
    for (i = 0; i < AVALANCHE_PP_LUT_HISTOGRAM_SIZE; i++)
    {
        PP_DB.stats.lut1_histogram[i] = 0;
    }
    PP_DB.stats.lut1_starvation = 0;
    for (i = 0; i < AVALANCHE_PP_LUT_HISTOGRAM_SIZE; i++)
    {
        PP_DB.stats.lut2_histogram[i] = 0;
    }
    PP_DB.stats.lut2_starvation = 0;
    PP_DB.stats.tdox_starvation = 0;

    PP_DB_UNLOCK();

    return (PP_RC_SUCCESS); 
}








/* PID and VPID Management API */
EXPORT_SYMBOL( avalanche_pp_pid_create       );
EXPORT_SYMBOL( avalanche_pp_pid_delete       );
EXPORT_SYMBOL( avalanche_pp_pid_config_range );
EXPORT_SYMBOL( avalanche_pp_pid_remove_range );
EXPORT_SYMBOL( avalanche_pp_pid_set_flags    );
EXPORT_SYMBOL( avalanche_pp_pid_get_list     );
EXPORT_SYMBOL( avalanche_pp_pid_get_info     );

EXPORT_SYMBOL( avalanche_pp_vpid_create      );
EXPORT_SYMBOL( avalanche_pp_vpid_delete      );
EXPORT_SYMBOL( avalanche_pp_vpid_set_flags   );
EXPORT_SYMBOL( avalanche_pp_vpid_get_list    );
EXPORT_SYMBOL( avalanche_pp_vpid_get_info    );

/* Session Management API */
EXPORT_SYMBOL( avalanche_pp_session_create   );
EXPORT_SYMBOL( avalanche_pp_session_delete   );
EXPORT_SYMBOL( avalanche_pp_session_get_list );
EXPORT_SYMBOL( avalanche_pp_session_get_info );
EXPORT_SYMBOL( avalanche_pp_flush_sessions   );

EXPORT_SYMBOL( avalanche_pp_session_list_execute     );
EXPORT_SYMBOL( avalanche_pp_session_pre_action_bind  );
EXPORT_SYMBOL( avalanche_pp_session_post_action_bind );

/* Statistics API */
EXPORT_SYMBOL( avalanche_pp_get_stats_session );
EXPORT_SYMBOL( avalanche_pp_get_stats_vpid    );
EXPORT_SYMBOL( avalanche_pp_get_stats_global  );

EXPORT_SYMBOL( avalanche_pp_event_handler_register   );
EXPORT_SYMBOL( avalanche_pp_event_handler_unregister );
EXPORT_SYMBOL( avalanche_pp_event_report             );


/* QoS API. */
EXPORT_SYMBOL( avalanche_pp_qos_cluster_setup   );
EXPORT_SYMBOL( avalanche_pp_qos_cluster_enable  );
EXPORT_SYMBOL( avalanche_pp_qos_cluster_disable );
EXPORT_SYMBOL( avalanche_pp_qos_get_queue_stats );

// Not sure if we still need these ...
EXPORT_SYMBOL( avalanche_pp_qos_set_cluster_max_global_credit );
EXPORT_SYMBOL( avalanche_pp_qos_set_queue_max_credit          );
EXPORT_SYMBOL( avalanche_pp_qos_set_queue_iteration_credit    );

/* Power Saving Mode (PSM) API. */
EXPORT_SYMBOL( avalanche_pp_psm      );
EXPORT_SYMBOL( avalanche_pp_hw_init  );


/* MISC APIs */
EXPORT_SYMBOL( avalanche_pp_session_tdox_capability_set );
EXPORT_SYMBOL( avalanche_pp_session_tdox_capability_get );
EXPORT_SYMBOL( avalanche_pp_version_get                 );
EXPORT_SYMBOL( avalanche_pp_set_mta_mac_address         );
EXPORT_SYMBOL( avalanche_pp_get_db_stats );
EXPORT_SYMBOL( avalanche_pp_reset_db_stats );
EXPORT_SYMBOL( avalanche_pp_state_is_active );
