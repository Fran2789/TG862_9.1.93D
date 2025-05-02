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

#ifndef PP_DB_H
#define PP_DB_H

#include <linux/kernel.h>
#include <asm-arm/arch-avalanche/generic/avalanche_pp_api.h>


typedef enum
{
    PP_DB_STATUS_UNINITIALIZED,
    PP_DB_STATUS_INITIALIZED,
    PP_DB_STATUS_ACTIVE,
    PP_DB_STATUS_PSM
}
PP_DB_STATUS_e;

typedef struct
{
    struct list_head                        link;
    Uint32                                  handle;
}
PP_DB_Entry_t;

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

typedef struct
{
    struct list_head                        link;
    struct list_head                        lut1_hash_link;
    Uint32                                  handle;
    Uint32                                  refCount;

    __Avalanche_PP_LUT1_inputs_t            lut1_data;
}
PP_DB_Session_LUT1_hash_entry_t;

/* ******************************************************************** */

typedef struct
{
    struct list_head                        link;
    struct list_head                        lut2_hash_link;
    PP_DB_Session_LUT1_hash_entry_t *       lut1_hash_entry_ptr;
    Uint32                                  handle;
}
PP_DB_Session_LUT2_hash_entry_t;

/* ******************************************************************** */

typedef struct
{
    AVALANCHE_PP_SESSION_INFO_t             session_info;               // The basic session information including ID

    PP_DB_Session_LUT2_hash_entry_t *       lut2_hash_entry_ptr;        // Pointer to the hash entry

    struct list_head                        list[ PP_LIST_ID_ALL ];     // Array of linked lists managed per type
}
PP_DB_Session_Entry_t;

/* ******************************************************************** */




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

/**************************************************************************
 * STRUCTURE NAME : PP_DB_VPID_Entry_t
 **************************************************************************
 * DESCRIPTION   :
 *  Internal structure that is used for keeping track of the VPID.
 **************************************************************************/
typedef struct
{
    struct list_head                        link;

    /* List of VPIDs attached to a PID */
    struct list_head                        pid_link;

    Uint8                                   handle;
    PP_DB_STATUS_e                          status;
    Uint8                                   reserved[2];

    AVALANCHE_EXEC_HOOK_FN_t                session_pre_action_cb;
    Ptr                                     session_pre_data;
    AVALANCHE_EXEC_HOOK_FN_t                session_post_action_cb;
    Ptr                                     session_post_data;

    /* VPID Information as passed by the callee. */
    AVALANCHE_PP_VPID_INFO_t                vpid;

    /* VPID --> Session Mapping.  */
    struct list_head                        list[ PP_LIST_ID_ALL ];     // Array of linked lists managed per type
}
PP_DB_VPID_Entry_t;


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

/**************************************************************************
 * STRUCTURE NAME : PPM_PID
 **************************************************************************
 * DESCRIPTION   :
 *  Internal structure that is used for keeping track of the PID.
 **************************************************************************/
typedef struct
{
    /* PID Information as passed by the callee. */
    AVALANCHE_PP_PID_t                  pid;

    /* List of all VPID handles that are created on the PID. */
    struct list_head                    pid_link_head;
    PP_DB_STATUS_e                      status;
}
PP_DB_PID_Entry_t;




/* ******************************************************************** */


typedef struct
{
    struct list_head                        link;
    AVALANCHE_EVENT_HANDLER_t               handler;
}
PP_DB_EventHandler_Entry_t;


typedef enum
{
    PP_DB_POOL_FREE,
    PP_DB_POOL_BUSY,
    PP_DB_POOL_TYPE_MAX
}
PP_DB_POOL_TYPE_e;



typedef struct
{
    /* ------------------------------------------------------------------------------------------------- */
    PP_DB_Session_Entry_t               repository_sessions     [ AVALANCHE_PP_MAX_ACCELERATED_SESSIONS ];
    PP_DB_Session_LUT2_hash_entry_t     repository_lut2_hash    [ AVALANCHE_PP_MAX_ACCELERATED_SESSIONS ];
    PP_DB_Session_LUT1_hash_entry_t     repository_lut1_hash    [ AVALANCHE_PP_MAX_LUT1_KEYS            ];
    PP_DB_Entry_t                       repository_TDOX         [ AVALANCHE_PP_MAX_ACCELERATED_TDOX_SESSIONS ];
    PP_DB_VPID_Entry_t                  repository_VPIDs        [ AVALANCHE_PP_MAX_VPID ];
    PP_DB_PID_Entry_t                   repository_PIDs         [ AVALANCHE_PP_MAX_PID  ];
    /* ------------------------------------------------------------------------------------------------- */
    struct list_head                    lut2_hash               [ AVALANCHE_PP_MAX_ACCELERATED_SESSIONS ];
    struct list_head                    lut1_hash               [ AVALANCHE_PP_MAX_LUT1_KEYS            ];
    /* ------------------------------------------------------------------------------------------------- */
    struct list_head                    pool_lut1               [ AVALANCHE_PP_SESSIONS_POOL_MAX ]  [ PP_DB_POOL_TYPE_MAX ];
    struct list_head                    pool_lut2               [ AVALANCHE_PP_SESSIONS_POOL_MAX ]  [ PP_DB_POOL_TYPE_MAX ];
    struct list_head                    pool_VPIDs                                                  [ PP_DB_POOL_TYPE_MAX ];
    struct list_head                    pool_TDOX                                                   [ PP_DB_POOL_TYPE_MAX ];
    /* ------------------------------------------------------------------------------------------------- */
    struct list_head                    eventHandlers;
    atomic_t                            lock_nesting;
    Uint32                              lock_state;
    PP_DB_STATUS_e                      status;
    AVALANCHE_PP_Misc_Statistics_t      stats;
}
PP_DB_t;

extern PP_DB_t     PP_DB;


AVALANCHE_PP_RET_e  pp_db_init( void );
AVALANCHE_PP_RET_e  PP_DB_LOCK( void );
AVALANCHE_PP_RET_e  PP_DB_UNLOCK( void );
Uint32              pp_db_hash( register Uint8 *k, register Uint32  length, register Uint32  initval );


#endif // PP_DB_H
