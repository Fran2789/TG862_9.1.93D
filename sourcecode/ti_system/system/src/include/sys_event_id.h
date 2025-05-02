/***************************************************************************
**+----------------------------------------------------------------------+**
**|                                ****                                  |**
**|                                ****                                  |**
**|                                ******o***                            |**
**|                          ********_///_****                           |**
**|                           ***** /_//_/ ****                          |**
**|                            ** ** (__/ ****                           |**
**|                                *********                             |**
**|                                 ****                                 |**
**|                                  ***                                 |**
**|                                                                      |**
**|     Copyright (c) 2006-2007 Texas Instruments Incorporated           |**
**|                        ALL RIGHTS RESERVED                           |**
**|                                                                      |**
**| Permission is hereby granted to licensees of Texas Instruments       |**
**| Incorporated (TI) products to use this computer program for the sole |**
**| purpose of implementing a licensee product based on TI products.     |**
**| No other rights to reproduce, use, or disseminate this computer      |**
**| program, whether in part or in whole, are granted.                   |**
**|                                                                      |**
**| TI makes no representation or warranties with respect to the         |**
**| performance of this computer program, and specifically disclaims     |**
**| any responsibility for any damages, special or consequential,        |**
**| connected with the use of this program.                              |**
**|                                                                      |**
**+----------------------------------------------------------------------+**
***************************************************************************/

/***************************************************************************/

/*! \file sys_event_id.h
    \brief Generic event ID creation

****************************************************************************/

/* We actually WANT several instances in the same compilation unit, 
    for several imported modules, each with its own events.
    Therefore, the usual conditional for non-repeating inclusion goes only 
    till someplace in the middle.
*/
#ifndef _SYS_EVENT_ID_H_
#define _SYS_EVENT_ID_H_

#include "sys_event.h"

/**************************************************************************/
/*      INCLUDES                                                          */
/**************************************************************************/

/**************************************************************************/
/*      INTERFACE  Defines and Structs                                    */
/**************************************************************************/

/*! \var typedef struct SampleStruct SampleStruct_t
    \brief A sample structure
*/

/* 
    To define events, the user must do the following, in the specified order :
	-	Define the COMPONENT_MODULES definition to define the modules include file.
    -   Defines the module prefix, out of the prefixes available in EVID_ModuleEventRange_e.
            #define EVID_MODULE_PREFIX  EVID_ModuleEventRange_e
    -   Defines the list of events as follows
            #define EVID_EVENT_LIST
                EVID(ev_name1) \
                EVID(ev_name2) \
                EVID(ev_name2) \
        The macro EVID has 2 expansions.
        The 1st expansion just outputs the event name, and is used to generate the
        event list enum.
        The 2nd expansion stringizes the event name, and is put into an array of 
        strings. This can be used during debug in order to print the event name.
        Using the same event list forces both the enum and the string array to have 
        the same order, and the event id can be used as an index in to the string array 
        to get the event name.
    -   Include this file (event_id.h)
*/
#define EVID_ACTUAL_CAT(x,y) x##y
#define EVID_CAT(x,y) EVID_ACTUAL_CAT(x,y)
#define EVID_STRINGIZE(x) #x

/* Create names for special "events" */

/* Name of module events enum : MOD_events_e */
#define EVID_MOD_EVENTS(mODULEpREFIX) EVID_CAT(mODULEpREFIX, _events_e)

/* Name of low bound from EVID_module_prefix_e */
#define EVID_MOD_RANGE_LOW_BOUND(mODULEpREFIX) EVID_CAT(EVID_, EVID_CAT(mODULEpREFIX, _EventGroupLowBound))

/* Name of high bound from EVID_module_prefix_e */
#define EVID_MOD_RANGE_HIGH_BOUND(mODULEpREFIX) EVID_CAT(EVID_, EVID_CAT(mODULEpREFIX, _EventGroupHighBound))

/* Get the low bound from EVID_module_prefix_e */
#define EVID_EVENT_LOW_BOUND(mODULEpREFIX) EVID_MOD_RANGE_LOW_BOUND(mODULEpREFIX)

/* Get the high bound from EVID_module_prefix_e */
#define EVID_EVENT_HIGH_BOUND(mODULEpREFIX) EVID_MOD_RANGE_HIGH_BOUND(mODULEpREFIX)

/* Create a name for the enum before the 1st event : EVID_MOD_EventLowBound */
#define EVID_EVENT_START_AFTER(mODULEpREFIX) EVID_CAT(EVID_, EVID_CAT(mODULEpREFIX, _EventLowBound))

/* Value of the 1st event in the module */
#define EVID_EVENT_FIRST_EVENT(mODULEpREFIX) (EVID_EVENT_START_AFTER(mODULEpREFIX) + 1)

/* Create a name for the enum after the last event : EVID_MOD_EventHighBound */
#define EVID_EVENT_END_BEFORE(mODULEpREFIX) EVID_CAT(EVID_, EVID_CAT(mODULEpREFIX, _EventHighBound))

/* Value of the last event in the module */
#define EVID_EVENT_LAST_EVENT(mODULEpREFIX) (EVID_EVENT_END_BEFORE(mODULEpREFIX) - 1)

/* Name of array of strings describing event names */
#define EVID_STR_NAME(mODULEpREFIX) EVID_CAT(EVID_MOD_EVENTS(mODULEpREFIX), _str)

/* Definition of array of strings */
#define EVID_STR_DEF(mODULEpREFIX) static const char *EVID_STR_NAME(mODULEpREFIX)[]

/* Reference to array of strings */
#define EVID_STR_REF(mODULEpREFIX) extern const char **EVID_STR_NAME(mODULEpREFIX)

/* Get the string name of an event in a module  */
#ifdef EVID_EVENTS_DBG
    #define EVID_STR(mODULEpREFIX, eVENT) EVID_STR_NAME(mODULEpREFIX)[(eVENT - EVID_EVENT_START_AFTER(mODULEpREFIX))]
#else
    #define EVID_STR(mODULEpREFIX, eVENT) "No debug info for " #eVENT
#endif

/*! \var typedef enum EVID_ModuleEventRange_e
    \brief Define ranges of event IDs for modules
*/
#ifdef EVID_MOD_RANGE
    #undef EVID_MOD_RANGE
#endif
#define EVID_MOD_RANGE(mODULEpREFIX, sTART, lEN)                    \
    EVID_MOD_RANGE_LOW_BOUND(mODULEpREFIX) = (sTART),               \
    EVID_MOD_RANGE_HIGH_BOUND(mODULEpREFIX) = ((sTART) + (lEN)),    \

// ARRIS ADD
#define EVID_MODULE_EVENT_RANGE_TYPE(mODULEpREFIX) EVID_CAT(mODULEpREFIX, _ModuleEventRange_e)
#define EVID_MODULE_EVENT_RANGE_HIGHEST(mODULEpREFIX) EVID_CAT(mODULEpREFIX, _ModuleEventRange_e_highest)
// ARRIS END
typedef enum
{
    #include COMPONENT_MODULES
    EVID_MODULE_EVENT_RANGE_HIGHEST(EVID_MODULE_PREFIX) /* ARRIS MOD - EVID_ModuleEventRange_e_highest*/
} EVID_MODULE_EVENT_RANGE_TYPE(EVID_MODULE_PREFIX); /* ARRIS MOD - EVID_ModuleEventRange_e;*/
#endif

/* Allow usage of all macros and structures without creating an event list */
#ifndef EVID_INTERFACE_ONLY

#ifdef EVID
    #undef EVID
#endif
/* Creates a list of enum names */
#define EVID(x) x,
typedef enum
{
    EVID_EVENT_START_AFTER(EVID_MODULE_PREFIX) = EVID_EVENT_LOW_BOUND(EVID_MODULE_PREFIX),
    EVID_EVENT_LIST
    EVID_EVENT_END_BEFORE(EVID_MODULE_PREFIX)
} EVID_MOD_EVENTS(EVID_MODULE_PREFIX);

#ifdef EVID_EVENTS_DBG
    #ifdef EVID
        #undef EVID
    #endif
    /* Creates a list of strings */
    #define EVID(x) EVID_STRINGIZE(x),
    EVID_STR_DEF(EVID_MODULE_PREFIX) = 
    {
        EVID(EVID_EVENT_START_AFTER(EVID_MODULE_PREFIX))
        EVID_EVENT_LIST
        EVID(EVID_EVENT_END_BEFORE(EVID_MODULE_PREFIX))
    } ;
#endif

/* So they can be used again other imported modules */
#undef EVID
#undef EVID_EVENT_LIST
#undef EVID_MODULE_PREFIX

#endif
/**************************************************************************/
/*      EXTERN definition block                                           */
/**************************************************************************/

/**************************************************************************/
/*      INTERFACE VARIABLES (prefix with EXTERN)                          */
/**************************************************************************/

/**************************************************************************/
/*      INTERFACE FUNCTIONS Prototypes:                                   */
/**************************************************************************/
/* 
#endif
*/

