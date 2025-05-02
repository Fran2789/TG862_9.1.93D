/*-----------------------------------------------------------------------------------------------------
 * Copyright 2007, Texas Instruments Incorporated
 *
 * This program has been modified from its original operation by Texas Instruments
 * to do the following:
 *       Added a range of predefined events for debug
 *       Corrected compilation warning at SME_APPLICATION_DEF, SME_END_ORTHO_STATE_DEF
 *       Added nRegionId to SME_APP_T
 *       Added predefined values for nRegionId 
 *       Added init of nRegionId to SME_APPLICATION_DEF
 *       Added prototype of SmeSetEventFilterOprProc function to set the event filter plugin
 *       Added prototype of SmePostThreadExtIntEvent function that uses the appropriate plugin to send events
 *       Added prototype of SmePostThreadExtPtrEvent function that uses the appropriate plugin to send events
 *       Added prototype and data types for plugin of timer handling functions
 *
 *   THIS MODIFIED SOFTWARE AND DOCUMENTATION ARE PROVIDED
 *   "AS IS," AND TEXAS INSTRUMENTS MAKES NO REPRESENTATIONS 
 *   OR WARRENTIES, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED 
 *   TO, WARRANTIES OF MERCHANTABILITY OR FITNESS FOR ANY 
 *   PARTICULAR PURPOSE OR THAT THE USE OF THE SOFTWARE OR 
 *   DOCUMENTATION WILL NOT INFRINGE ANY THIRD PARTY PATENTS, 
 *   COPYRIGHTS, TRADEMARKS OR OTHER RIGHTS. 
 *   See The GNU Lesser General Public License for more details.
 *
 *  These changes are covered under version 2.1 of the GNU Lesser General Public 
 *  License, dated February 1999.
 *---------------------------------------------------------------------------------------------------*/

/* ==============================================================================================================================
 * This notice must be untouched at all times.
 *
 * Copyright  IntelliWizard Inc. 
 * All rights reserved.
 * LICENSE: LGPL. 
 * Redistributions of source code modifications must send back to the Intelliwizard Project and republish them. 
 * Web: http://www.intelliwizard.com
 * eMail: info@intelliwizard.com
 * We provide technical supports for UML StateWizard users. The StateWizard users do NOT have to pay for technical supports 
 * from the Intelliwizard team. We accept donation, but it is not mandatory.
 * ==============================================================================================================================
 * -----------------------------------------------------------------------------
 *                               Overview
 * -----------------------------------------------------------------------------
  The goals of the StateWizard framework:
  -------------------
  Simplicity: It should be easy to construct state machine trees/applications.
  Extensibility: It should be extensible to add other pseudo states. 
  Cross-Platform, Cross-Compiler: Support for VC++, EVC++, GCC, Diab C and for Win32/Linux
  Multi-threaded: Several groups of state machines (applications) are allowed to run at each separate thread/task 
                  using a thread context to identify an independent thread. 
  Unicode Support: 

  -------------------
  Feature List:
  -------------------
	* Two application frameworks: Standard Embedded C++ and Standard Embedded C.
	* These two versions share an identical framework API set. Rename .c files to .cpp files and declare SME_CPP=TRUE for C++ version
	* Hierarchical state machines.
	* Support large state machine with hundreds of states through separating a state tree definitions to several C/C++ files.
	* State history information and history transitions
	* Explicit entry to or exit from a child state in a composite state
	* Guarded transitions on event handling
	* Conditional pseudo-states
	* Join pseudo-states
	* Multi-threaded orthogonal states
	* Built-in state timers. On state entry, starts the built-in timer automatically, stops it on state exit.
	* Cross-platform event loop and OS-related API library for Linux/Win32 .

 * ===========================================================================*/


#ifndef SME_H
#define SME_H

#ifndef BOOL
	typedef int  BOOL;
#endif
#ifndef FALSE
	#define FALSE	0
#endif
#ifndef TRUE
	#define TRUE	1
#endif
#ifndef NULL
	#define NULL    0
#endif

#ifdef __cplusplus   
extern "C" {
#endif

#include "sme_conf.h"
#include <string.h>

/*****************************************************************************************
*  State Machine Engine Type Definitions
******************************************************************************************/
	
/* Event Definition
* Positive 32-bit integer values from 0 to SME_MAX_EVENT_ID are user-defined event identifiers.
* Negative 32-bit integer values are state machine engine defined event identifiers.
*/
typedef SME_UINT32 SME_EVENT_ID_T;

#define SME_MAX_EVENT_ID 0x0FFFFFFF


/* Assign first 4 management bits to event types. */
#define SME_EVENT_TYPE_PREDEFINE 0x80000000
#define SME_EVENT_TYPE_EXPLICIT_ENTRY 0x40000000
#define SME_EVENT_TYPE_EXPLICIT_EXIT 0x20000000
#define SME_EVENT_TYPE_STATE_TIMEOUT 0x10000000

#define SME_INVALID_EVENT_ID	(SME_EVENT_TYPE_PREDEFINE | 1)
#define SME_EVENT_KILL_FOCUS	(SME_EVENT_TYPE_PREDEFINE | 2)
#define SME_EVENT_SET_FOCUS		(SME_EVENT_TYPE_PREDEFINE | 3)
#define SME_EVENT_TIMER			(SME_EVENT_TYPE_PREDEFINE | 4) /* Regular timer event */
#define SME_EVENT_STATE_TIMER	(SME_EVENT_TYPE_PREDEFINE | 5) /* State built-in timer event */
#define SME_EVENT_COND_ELSE		(SME_EVENT_TYPE_PREDEFINE | 6)
#define SME_EVENT_EXIT_LOOP		(SME_EVENT_TYPE_PREDEFINE | 7)

#define SME_INIT_CHILD_STATE_ID (SME_EVENT_TYPE_PREDEFINE | 100)
#define SME_JOIN_STATE_ID		(SME_EVENT_TYPE_PREDEFINE | 101)
#define SME_MIN_PRESERVED_ID	(SME_EVENT_TYPE_PREDEFINE | 102)

/* 0x1000..0x1FFF --> debug */
#define SME_DBG_1ST_STATES_ID	(SME_EVENT_TYPE_PREDEFINE | 0x1000)
#define SME_DBG_LAST_STATES_ID	(SME_DBG_1ST_STATES_ID + 0xFFF)

/* Region IDs */
#define SME_REGIONID_DUMMY (-1)
/* RegionId assigned to the */
#define SME_REGIONID_ROOT_APP (-2)

#define SME_EXPLICIT_ENTRY_EVENT_ID(_EventID) (SME_EVENT_TYPE_EXPLICIT_ENTRY | _EventID)
#define SME_EXPLICIT_EXIT_EVENT_ID(_EventID)  (SME_EVENT_TYPE_EXPLICIT_EXIT  | _EventID)
#define SME_STATE_TIMEOUT_EVENT_ID(_Elapse)  (SME_EVENT_TYPE_STATE_TIMEOUT  | _Elapse)

#define SME_IS_STATE_TIMEOUT_EVENT_ID(_EventID) (_EventID & SME_EVENT_TYPE_STATE_TIMEOUT)
#define SME_GET_STATE_TIMEOUT_VAL(_EventID) (_EventID & 0x0FFFFFFF)

#define SME_BEGIN_EVENT_ID_LIST_DECLARE enum { 
#define SME_END_EVENT_ID_LIST_DECLARE };

#define SME_BEGIN_EVENT_HANDLER(_Root,_CompState)
#define SME_EVENT_HANDLER_DEC(_Handler) int _Handler(SME_APP_T *pDestApp, SME_EVENT_T *pEvent);
#define SME_END_EVENT_HANDLER(_Root,_CompState)

/**** Event Definition *****/
typedef enum
{
	SME_EVENT_CAT_OTHER=0,
	SME_EVENT_CAT_UI  /* The engine dispatches UI events to the current focused application(window) which is activated at last or being set focused.*/
} SME_EVENT_CAT_E;
typedef unsigned char SME_EVENT_CAT_T;

typedef enum 
{
	SME_EVENT_ORIGIN_INTERNAL=0,
	SME_EVENT_ORIGIN_EXTERNAL
} SME_EVENT_ORIGIN_E;
typedef unsigned char SME_EVENT_ORIGIN_T;

typedef enum 
{
	SME_EVENT_DATA_FORMAT_INT=0,
	SME_EVENT_DATA_FORMAT_PTR
} SME_EVENT_DATA_FORMAT_E;
typedef unsigned char SME_EVENT_DATA_FORMAT_T;

typedef	struct SME_INT_DATA_T 
{
		SME_UINT32 nParam1; 
		SME_UINT32 nParam2;
} SME_INT_DATA_T;

typedef struct SME_PTR_DATA_T 
{
		void* pData;
		SME_UINT32 nSize;
} SME_PTR_DATA_T;

union SME_EVENT_DATA_T
{
	SME_INT_DATA_T	Int;
	SME_PTR_DATA_T  Ptr;
};

typedef struct SME_EVENT_T_TAG
{
	SME_EVENT_ID_T nEventID;
	SME_UINT32 nSequenceNum;
	struct SME_EVENT_T_TAG *pNext; 
	/* Provide 2 data formats: integer or pointer */
	union SME_EVENT_DATA_T Data;
#if SME_CPP
	struct SME_APP_T *pDestApp; /* The destination application. */ 
#else
	struct SME_APP_T_TAG *pDestApp; /* The destination application. */ 
#endif
	void* pPortInfo; /* Point to a destination port information data. */
	SME_INT32 nOrigin :8; /* An internal event or an external event */
	SME_INT32 nCategory :8; /* Category of this event. */
	SME_INT32 nDataFormat :8; /* Flag for this event. */
	SME_INT32 bIsConsumed :8; /* Is consumed. */
}SME_EVENT_T,*SME_EVENT_PT;


/**** Event Handler *****/
#if SME_CPP
	struct SME_APP_T;
	typedef int (SME_APP_T::*SME_EVENT_HANDLER_T)(struct SME_APP_T *, SME_EVENT_T *);  

	/* SME_NULL , SME_NULL_GUARD and SME_NULL_ACTION define to member function.*/
	#define SME_NULL_STATE      NULL
	#define SME_INTERNAL_TRAN	NULL

#else
	struct SME_APP_T_TAG;
	typedef int (* SME_EVENT_HANDLER_T)(struct SME_APP_T_TAG *, SME_EVENT_T *);  

	#define SME_NULL            NULL
	#define SME_NULL_GUARD      SME_NULL
	#define SME_NULL_ACTION     SME_NULL
	#define SME_NULL_STATE      SME_NULL
	#define SME_INTERNAL_TRAN	SME_NULL

#endif

typedef SME_EVENT_HANDLER_T SME_TRAN_GUARD_T;


/**** State Definition *****/
/*
Simple States (Leaf States): 

Simplest of all states, they have no sub-states.

Composite States:

A composite state is a state that consists of sub-states.
A composite state is a state that can be decomposed using and-relationships into two or more concurrent regions which are containers for sub-states,
or using or-relationships into mutually exclusive disjoint sub-states,
if a composite state, which can be decomposed using and-relationships into two or more concurrent regions, is called Orthogonal State. 

PseudoState:

A PseudoState is an abstraction of different types of nodes in the state machine graph which represent transient points 
in transition paths from one state to another (e.g. branch and fork points). Pseudo states are used to construct complex 
transitions from simple transitions. For example, by combining a transition entering a fork pseudo state with a set of 
transitions exiting the fork pseudo state, we get a complex transition that leads to a set of target states. 
  
Conditional/Fork PseudoStates are a notational shorthand for multiple exiting transitions all triggered by the same event 
but each having different guards.

Join PseudoState: a state with several incoming transitions and a single outgoing one. 

An event is the specification of a significant occurrence that has a location in time and space. An instance of an event 
can lead to the activation of a behavioral feature in an object.

Guard:

A guard condition is a boolean expression that may be attached to a transition in order to determine 
whether that transition is enabled or not.
The guard is evaluated when an event occurrence triggers the transition. Only if the guard is true at the time 
the event is presented to the state machine will the transition actually take place. 
*/
typedef enum
{
    SME_STYPE_LEAF = 0, /* Leaf State or Simple State*/
    SME_STYPE_COMP, /* The components of a composite state which children states are in or-relationships. */
    SME_STYPE_SUB,  /* A pointer to the sub-machine's internal behaviors */
	SME_STYPE_ORTHO_COMP, /* The holder to the region application table of a composite state which children states are in and-relationships. */
	/* Pseudo States */
	SME_STYPE_BEGIN_PSEUDO,
    SME_STYPE_COND = SME_STYPE_BEGIN_PSEUDO,
    SME_STYPE_JOIN,
	SME_STYPE_END_PSEUDO, /* Extend pseudo state here. */
    SME_NUM_STYPES = SME_STYPE_END_PSEUDO
} SME_STATE_TYPE_E;

typedef const struct SME_EVENT_TABLE_T_TAG {                                    
	SME_EVENT_ID_T nEventID;                                        
	SME_TRAN_GUARD_T pGuardFunc;
	SME_EVENT_HANDLER_T pHandler;
	struct SME_STATE_T_TAG *pNewState;	
}SME_EVENT_TABLE_T, *SME_EVENT_TABLE_PT;

struct SME_STATE_T_TAG
{
    const char				*sStateName;
    SME_STATE_TYPE_E		nStateType;
	struct SME_STATE_T_TAG  *pParent;
#if SME_CPP
    SME_EVENT_HANDLER_T		pfnFunc; /* EntryFunc | CondFunc in C++ version*/
#else
    void 					*pfnFunc; /* EntryFunc | CondFunc | CompState for SME_STYPE_SUB | Region app table for SME_STYPE_ORTHO_COMP in C version */
#endif
    SME_EVENT_HANDLER_T		pfnExitFunc;
    SME_EVENT_TABLE_T	*EventTable; /* Event Table or SME_REGION_CONTEXT_T table for orthogonal state in C version. */
#if SME_CPP
	struct SME_STATE_T_TAG *pCompState; /* CompState for SME_STYPE_SUB in C++ version */
#endif
};
typedef struct SME_STATE_T_TAG SME_STATE_T;
typedef SME_STATE_T  *SME_STATE_PT;

#define SME_IS_PSEUDO_STATE(pState) (SME_NULL_STATE!=pState && SME_STYPE_BEGIN_PSEUDO<=pState->nStateType && SME_STYPE_END_PSEUDO>=pState->nStateType)

/**** Application Definition *****/
/*
State Machine Application:

An application is an instance of a state machine or an instance of a region which is an orthogonal part of either a composite state or a state machine.  
Applications can have one of two modes: active or inactive. 
Active applications are the ones running on the state machine at a given time whereas inactive applications are not. 
In other words, only active applications can handle events. A State machine engine is responsible for managing these applications 
and dispatching events to specific applications. 
*/
#if SME_CPP /* struct SME_APP_T is a class. */
	typedef struct SME_APP_T 
#else
	typedef struct SME_APP_T_TAG 
#endif
{
	char sAppName[SME_MAX_APP_NAME_LEN+1];
	SME_STATE_T *pRoot;  /* Pointer to the Composite state of the tree. */
	SME_STATE_T *pAppState;
	SME_STATE_T *pHistoryState; /* The original state before transition. */
	unsigned int StateTimers[SME_MAX_STATE_TREE_DEPTH]; /* State timers from the root to the leaf. */
	int nStateNum;
	void *pData;
	
#if SME_CPP
	struct SME_APP_T *pParent; /* Who starts me */
	struct SME_APP_T *pNext; /* Applications are linked together. */

	public:
	SME_APP_T(const char* _sAppName, SME_STATE_T *_pRoot)
	{
		pAppState=NULL; 
		memset(sAppName, 0, sizeof(sAppName)); 
		strncpy(sAppName,_sAppName,sizeof(sAppName)-1); 
		pRoot=_pRoot; 
		pSME_NULL_GUARD=&SME_APP_T::SME_NULL_GUARD; 
		pParent=pNext=NULL; pRegionThreadContextList=NULL;
	};
	int SME_NULL_ACTION(SME_APP_T *, SME_EVENT_T *){return TRUE;}; 
	int SME_NULL_GUARD(SME_APP_T *, SME_EVENT_T *){return TRUE;}; 
	SME_EVENT_HANDLER_T pSME_NULL_GUARD;
	
	virtual ~SME_APP_T(){};
#else
	struct SME_APP_T_TAG *pParent; /* Who starts me */
	struct SME_APP_T_TAG *pNext; /* Applications are linked together. */
#endif

	void *pRegionThreadContextList; /* Point to the region thread context list for all orthogonal regions. The first item is the current thread context.*/
    int nRegionId; /* Index of current region, when creating a Multi-Region */
}SME_APP_T, *SME_APP_PT;

#define SME_APP_DATA(app) (app->pData)
#define SME_GET_APP_STATE(app) (app->pAppState) 
#define SME_IS_ACTIVATED(app) (app->pAppState!=SME_NULL_STATE)

struct SME_THREAD_CONTEXT_T_TAG;
/********************************************************************************************************
*  State Machine Engine hook function prototypes.
*********************************************************************************************************/
typedef BOOL (*SME_GET_EXT_EVENT_PROC_T)(SME_EVENT_T *pEvent);
typedef BOOL (*SME_DEL_EXT_EVENT_PROC_T)(SME_EVENT_T *pEvent);
 
typedef int (*SME_POST_THREAD_EXT_INT_EVENT_PROC_T)(struct SME_THREAD_CONTEXT_T_TAG* pDestThreadContext, int nMsgID, int Param1, int Param2, 
						   SME_APP_T *pDestApp, unsigned long nSequenceNum,unsigned char nCategory);

typedef int (*SME_POST_THREAD_EXT_PTR_EVENT_PROC_T)(struct SME_THREAD_CONTEXT_T_TAG* pDestThreadContext, int nMsgID, void *pData, int nDataSize, 
						   SME_APP_T *pDestApp, unsigned long nSequenceNum,unsigned char nCategory);

typedef BOOL (*SME_INIT_THREAD_EXT_MSG_BUF_PROC_T)();
typedef BOOL (*SME_FREE_THREAD_EXT_MSG_BUF_PROC_T)();

typedef void* (*SME_MEM_ALLOC_PROC_T)(unsigned int nSize);
typedef BOOL  (*SME_MEM_FREE_PROC_T)(void* pUserData);

typedef int (*SME_ON_EVENT_COME_HOOK_T)(SME_EVENT_ORIGIN_T nEventOrigin, SME_EVENT_T *pEvent);
typedef int (*SME_ON_EVENT_HANDLE_HOOK_T)(SME_EVENT_ORIGIN_T nEventOrigin, SME_EVENT_T *pEvent, 
										  SME_APP_T *pDestApp, SME_STATE_T* pNewState);

typedef unsigned int (*SME_STATE_TIMER_PROC_T)(SME_APP_T *pDestApp, unsigned  int nTimeOut); 
typedef int (*SME_KILL_TIMER_PROC_T)(unsigned  int handle); 

/********************************************************************************************************
*  State Machine Engine multi-thread support.
*********************************************************************************************************/
typedef struct SME_THREAD_CONTEXT_T_TAG{
	SME_APP_T *pActAppHdr;
	SME_APP_T *pFocusedApp;
	struct SME_EVENT_T_TAG EventPool[SME_EVENT_POOL_SIZE]; /* Internal event buffer */
	int nEventPoolIdx;
	struct SME_EVENT_T_TAG *pEventQueueFront; 
	struct SME_EVENT_T_TAG *pEventQueueRear;
	SME_ON_EVENT_COME_HOOK_T fnOnEventComeHook; 
	SME_ON_EVENT_HANDLE_HOOK_T fnOnEventHandleHook;
	unsigned long		nAppThreadID;
	void *pData; /* Preserved for application. */
	void *pExtEventPool; /* A pointer to the external event pool information. */
}SME_THREAD_CONTEXT_T, *SME_THREAD_CONTEXT_PT;

typedef BOOL (*SME_SET_THREAD_CONTEXT_PROC)(SME_THREAD_CONTEXT_PT p);
typedef SME_THREAD_CONTEXT_PT (*SME_GET_THREAD_CONTEXT_PROC)();

/* On orthogonal state entry, the child regions may run at the parent thread or an independent thread. 
On orthogonal state exit, deactivate the child region(s) which runs at the parent thread,  
and the orthogonal state will post SME_EVENT_EXIT_LOOP events
to the running region threads, and hold these regions to exit their threads. 
*/
typedef enum 
{
	SME_RUN_MODE_PARENT_THREAD=0,
	SME_RUN_MODE_SEPARATE_THREAD,
	SME_RUN_MODE_SEPARATE_PROCESS /* Unsupported for the time being. */
} SME_REGION_RUN_MODE_E;

typedef struct SME_REGION_CONTEXT_T_TAG{
	const char* sRegionName;
	int nInstanceNum;
	SME_STATE_T *pRoot;
	int nRunningMode; /* Running mode for the region state machine. */
	int nPriority; /* The priority of the running thread. */
} SME_REGION_CONTEXT_T;

/******************************************************************************************
*  State Machine Data Mapping Definitions
*******************************************************************************************/
/* Variable definitions */
#define ACTUAL_CAT(x,y) x##y
#define CAT(x,y) ACTUAL_CAT(x,y)

#define SME_STATE_REF(_state) \
	CAT(_state,_descriptor)

#define SME_COMPSTATE_REF( _state )\
    CAT(_comp_,SME_STATE_REF(_state))

#define SME_STATE_EVT_TBL_REF(_state) \
	CAT(SME_STATE_REF(_state),_evt_tbl)

#define SME_STATE_EVT_TBL_POS_REF(_state,_pos) \
	CAT(SME_STATE_REF(_state),_evt_tbl[_pos])

#define SME_STATE_REGION_TBL_REF(_state) \
	CAT(SME_STATE_REF(_state),_region_tbl)

#define SME_COMPSTATE_EVT_TBL_REF( _state )\
    CAT(_comp_,SME_STATE_EVT_TBL_REF(_state))

/* So that we may use SME_CURR_DEFAULT_PARENT macro instead of a constant state name. */
#define SME_STRINGIZE(_x) #_x

#if SME_CPP /* Standard C++ Version */
	/* _HandlerClass solves the warning: ISO C++ forbids taking the address of a bound member function to form a pointer to member function. */

	/*** Application Class Members***/	
	#define SME_COMP_STATE_DECLARE(_state) \
		static SME_EVENT_TABLE_T SME_STATE_EVT_TBL_REF(_state)[]; \
		static SME_EVENT_TABLE_T SME_COMPSTATE_EVT_TBL_REF(_state)[]; \
		static SME_STATE_T SME_COMPSTATE_REF(_state); \
		static SME_STATE_T SME_STATE_REF(_state); \

	#define SME_ORTHO_COMP_STATE_DECLARE(_state) \
		static SME_EVENT_TABLE_T SME_STATE_EVT_TBL_REF(_state)[]; \
		static SME_REGION_CONTEXT_T SME_STATE_REGION_TBL_REF(_state)[]; \
		static SME_STATE_T SME_COMPSTATE_REF(_state); \
		static SME_STATE_T SME_STATE_REF(_state); \

	#define SME_LEAF_STATE_DECLARE(_state) \
		static SME_EVENT_TABLE_T SME_STATE_EVT_TBL_REF(_state)[]; \
		static SME_STATE_T SME_STATE_REF(_state); \

	#define SME_PSEUDO_STATE_DECLARE(_state) \
		SME_LEAF_STATE_DECLARE(_state)

	#define SME_BEGIN_CLASS_MEMBER_DECLARE(_app) \
		SME_COMP_STATE_DECLARE(_app)
	#define SME_END_CLASS_MEMBER_DECLARE

	#define SME_HANDLER_CLASS_DEF(_app) \
		typedef C##_app _HandlerClass;


	/*** State Definition ***/	
	#define SME_BEGIN_STATE_DEF(_state, _type, _parent, _f1, _f2)					\
    	SME_STATE_T _HandlerClass::SME_STATE_REF(_state) =					        \
        {  SME_STRINGIZE(_state), _type, &_HandlerClass::SME_STATE_REF(_parent), (SME_EVENT_HANDLER_T)&_HandlerClass::_f1, (SME_EVENT_HANDLER_T)&_HandlerClass::_f2, _HandlerClass::SME_STATE_EVT_TBL_REF(_state) };		\
		SME_EVENT_TABLE_T _HandlerClass::SME_STATE_EVT_TBL_REF(_state)[] =	        \
		{
		
	#define SME_END_STATE_DEF \
        { SME_INVALID_EVENT_ID, NULL, NULL, SME_NULL_STATE} \
			};

	#define SME_BEGIN_COMP_STATE_DEF(_state, _parent, _entry, _exit)								\
    	SME_STATE_T _HandlerClass::SME_COMPSTATE_REF(_state) =											\
		{ SME_STRINGIZE(_state), SME_STYPE_COMP, &_HandlerClass::SME_STATE_REF(_parent), (SME_EVENT_HANDLER_T)&_HandlerClass::_entry, (SME_EVENT_HANDLER_T)&_HandlerClass::_exit, _HandlerClass::SME_COMPSTATE_EVT_TBL_REF(_state) };    \
		SME_EVENT_TABLE_T _HandlerClass::SME_COMPSTATE_EVT_TBL_REF(_state)[] =						\
		{

	#define SME_BEGIN_ROOT_COMP_STATE_DEF(_root_state, _entry, _exit)								\
    	SME_STATE_T C##_root_state::SME_STATE_REF(_root_state) = { #_root_state, SME_STYPE_SUB, (SME_STATE_T*)NULL, &_HandlerClass::SME_NULL_GUARD, &_HandlerClass::SME_NULL_ACTION, NULL, &_HandlerClass::SME_COMPSTATE_REF(_root_state) }; \
    	SME_STATE_T C##_root_state::SME_COMPSTATE_REF(_root_state) =											\
		{ #_root_state, SME_STYPE_COMP, SME_NULL_STATE, (SME_EVENT_HANDLER_T)&C##_root_state::_entry, (SME_EVENT_HANDLER_T)&C##_root_state::_exit, _HandlerClass::SME_COMPSTATE_EVT_TBL_REF(_root_state) };    \
		SME_EVENT_TABLE_T C##_root_state::SME_COMPSTATE_EVT_TBL_REF(_root_state)[] =						\
		{
		
    #define SME_BEGIN_SUB_STATE_DEF(_state, _parent) \
    	SME_STATE_T _HandlerClass::SME_STATE_REF(_state) =					        \
        {  SME_STRINGIZE(_state), SME_STYPE_SUB, &_HandlerClass::SME_STATE_REF(_parent), &_HandlerClass::SME_NULL_GUARD, &_HandlerClass::SME_NULL_ACTION, _HandlerClass::SME_STATE_EVT_TBL_REF(_state), &_HandlerClass::SME_COMPSTATE_REF(_state) };		\
		SME_EVENT_TABLE_T _HandlerClass::SME_STATE_EVT_TBL_REF(_state)[] =	        \
		{

    #define SME_BEGIN_LEAF_STATE_DEF(_state, _parent, _entry, _exit) \
        SME_BEGIN_STATE_DEF(_state, SME_STYPE_LEAF, _parent, _entry, _exit)

    /* Conditional Pseudo State */
	#define SME_BEGIN_COND_STATE_DEF(_state, _parent, _cond) \
        SME_BEGIN_STATE_DEF(_state, SME_STYPE_COND, _parent, _cond, SME_NULL_ACTION)

    /* Join Pseudo State */
    #define SME_BEGIN_JOIN_STATE_DEF(_state, _parent) \
        SME_BEGIN_STATE_DEF(_state, SME_STYPE_JOIN, _parent, SME_NULL_GUARD, SME_NULL_ACTION)

	/*** Event Handler ***/	
	#define SME_ON_INIT_STATE( _Handler, _NewState) \
	{  SME_INIT_CHILD_STATE_ID, &_HandlerClass::SME_NULL_GUARD, (SME_EVENT_HANDLER_T)&_HandlerClass::_Handler, &_HandlerClass::SME_STATE_REF(_NewState)},

	#define SME_ON_EVENT(_EventID, _Handler, _NewState) \
		{  _EventID, &_HandlerClass::SME_NULL_GUARD, (SME_EVENT_HANDLER_T)&_HandlerClass::_Handler, &_HandlerClass::SME_STATE_REF(_NewState)},

	#define SME_ON_EVENT_WITH_GUARD(_EventID, _Guard, _Handler, _NewState) \
		{  _EventID, (SME_TRAN_GUARD_T)&_HandlerClass::_Guard, (SME_EVENT_HANDLER_T)&_HandlerClass::_Handler, &_HandlerClass::SME_STATE_REF(_NewState)},

	/* Internal Transition without a guard */
	#define SME_ON_INTERNAL_TRAN(_EventID, _Handler) \
		{  _EventID, &_HandlerClass::SME_NULL_GUARD, (SME_EVENT_HANDLER_T)&_HandlerClass::_Handler, SME_INTERNAL_TRAN },

	/* Internal Transition with a guard */
	#define SME_ON_INTERNAL_TRAN_WITH_GUARD(_EventID, _Guard, _Handler) \
		{  _EventID, (SME_TRAN_GUARD_T)&_HandlerClass::_Guard, (SME_EVENT_HANDLER_T)&_HandlerClass::_Handler, SME_INTERNAL_TRAN },

	#define SME_ON_JOIN_TRAN( _Handler, _NewState) \
		{  SME_JOIN_STATE_ID, (SME_TRAN_GUARD_T)&_HandlerClass::SME_NULL_GUARD, (SME_EVENT_HANDLER_T)&_HandlerClass::_Handler, &_HandlerClass::SME_STATE_REF(_NewState)},

	#define SME_ON_EVENT_COND_ELSE ( _CondElseAct, _NewState) \
		SME_ON_EVENT(SME_EVENT_COND_ELSE, _CondElseAct, _NewState)
	
	/* A state built-in time out. On state entry, start a timer with given time-out value in milli-seconds.  
	There may be nested timeouts. It is not the SMEs responsibility to check whether the timeout hierarchy make sense.
	*/
	#define SME_ON_STATE_TIMEOUT(_Elapse, _Handler, _NewState) \
		{  SME_STATE_TIMEOUT_EVENT_ID(_Elapse), &_HandlerClass::SME_NULL_GUARD, (SME_EVENT_HANDLER_T)&_HandlerClass::_Handler, &SME_STATE_REF(_NewState)},

	#define SME_ON_STATE_TIMEOUT_WITH_GUARD(_Elapse, _Guard, _Handler, _NewState) \
		{  SME_STATE_TIMEOUT_EVENT_ID(_Elapse), &_HandlerClass::_Guard, (SME_EVENT_HANDLER_T)&_HandlerClass::_Handler, &SME_STATE_REF(_NewState)},

	#define SME_ON_STATE_TIMEOUT_INTERNAL_TRAN(_Elapse, _Handler) \
		{  SME_STATE_TIMEOUT_EVENT_ID(_Elapse), &_HandlerClass::SME_NULL_GUARD, (SME_EVENT_HANDLER_T)&_HandlerClass::_Handler, SME_INTERNAL_TRAN},

	#define SME_ON_STATE_TIMEOUT_INTERNAL_TRAN_WITH_GUARD(_Elapse, _Guard, _Handler) \
		{  SME_STATE_TIMEOUT_EVENT_ID(_Elapse), (SME_TRAN_GUARD_T)&_HandlerClass::_Guard, (SME_EVENT_HANDLER_T)&_HandlerClass::_Handler, SME_INTERNAL_TRAN },


	/*** Application ***/
	/* SME_APP_T FORMAT: const char * sAppName; SME_STATE_T *pRoot; SME_STATE_T *pAppState; SME_STATE_T *pHistoryState; void *pData; struct SME_APP_T *pParent; struct SME_APP_T *pNext; */	
    /* SME_STATE_T FORMAT: const char	*sStateName; SME_STATE_TYPE_E nStateType; struct SME_STATE_T_TAG  *pParent; SME_EVENT_HANDLER_T	pfnFunc; 
    SME_EVENT_HANDLER_T		pfnExitFunc; SME_EVENT_TABLE_T	*EventTable; struct SME_STATE_T_TAG *pCompState;*/

	#define SME_APPLICATION_DEF(_app_name, _root_state) \
		C##_root_state _app_name##App(#_app_name, &C##_root_state::SME_COMPSTATE_REF(_root_state));		

	/* Get application variable name. */
	#define SME_GET_APP_VAR(_app) _app##App

	/* Declare application variable that has external linkage. */
	#define SME_DEC_EXT_APP_VAR(_app,_root) extern C##_root _app##App;

	/*** History Transition ***/	
	/* Update state transition destination state for history transition. 
	#define SME_UPDATE_STATE_TRAN_DEST(_state, _tbl_idx, _p_new_state) \
		SME_STATE_EVT_TBL_POS_REF(_state,_tbl_idx) = _p_new_state;
	*/

	/*** Orthogonal State ***/	
	#define SME_BEGIN_ORTHO_COMP_STATE_DEF(_state, _parent, _entry, _exit)								\
    	SME_STATE_T _HandlerClass::SME_COMPSTATE_REF(_state) =											\
		{ SME_STRINGIZE(_state), SME_STYPE_ORTHO_COMP, &_HandlerClass::SME_STATE_REF(_parent), (SME_EVENT_HANDLER_T)&_HandlerClass::_entry, (SME_EVENT_HANDLER_T)&_HandlerClass::_exit, (SME_EVENT_TABLE_T*)_HandlerClass::SME_STATE_REGION_TBL_REF(_state) };    \
		SME_REGION_CONTEXT_T _HandlerClass::SME_STATE_REGION_TBL_REF(_state)[] =						\
		{

	#define SME_MULTI_REGION_DEF(_RegionName,_InstanceNum, _Region, _RunMode, _nPriority) \
        {#_RegionName, _InstanceNum, &C##_Region::SME_COMPSTATE_REF(_Region), _RunMode, _nPriority}, 

	#define SME_END_ORTHO_STATE_DEF \
        NULL,NULL,0,0};

#else
	/* Standard C Version */

	#define SME_COMP_STATE_DECLARE(_state) \
    	extern SME_STATE_T SME_STATE_REF(_state); \
    	extern SME_STATE_T SME_COMPSTATE_REF(_state); 

	#define SME_LEAF_STATE_DECLARE(_state) \
    	extern SME_STATE_T SME_STATE_REF(_state); 

	#define SME_PSEUDO_STATE_DECLARE(_state) \
		SME_LEAF_STATE_DECLARE(_state)

	#define SME_BEGIN_STATE_DECLARE(_state)	\
		SME_COMP_STATE_DECLARE(_state)
	#define SME_END_STATE_DECLARE

	#define SME_ORTHO_COMP_STATE_DECLARE(_state)  SME_COMP_STATE_DECLARE(_state)


	/* FORMAT: {const char *sStateName, SME_STATE_TYPE_E nStateType, SME_STATE_T *pParent; void *pfnFunc, void *pfnExitFunc,
	{SME_EVENT_ID_T nEventID, SME_TRAN_GUARD_T pGuardFunc, SME_EVENT_HANDLER_T pHandler, void *pNewState}	
	....
	}
    */
	#define SME_BEGIN_STATE_DEF(_state, _type, _parent, _f1, _f2)					\
		extern SME_EVENT_TABLE_T SME_STATE_EVT_TBL_REF(_state)[];		    \
    	/*static*/ SME_STATE_T SME_STATE_REF(_state) =					        \
        {  SME_STRINGIZE(_state), _type, &SME_STATE_REF(_parent), (void*)_f1, _f2, SME_STATE_EVT_TBL_REF(_state) };		\
		/*static*/ SME_EVENT_TABLE_T SME_STATE_EVT_TBL_REF(_state)[] =	        \
		{
		
	#define SME_END_STATE_DEF \
        { SME_INVALID_EVENT_ID, SME_NULL_GUARD, SME_NULL_ACTION, SME_NULL_STATE} \
			};

	/* Composite States: which have one of more regions for sub-states. 
	A composite state has an internal behavior, which is the entry/exit functions, initial state, internal event/transitions, child states 
	and the transitions among these child states. These are defined in the COMP (component) definition and the appropriate definitions of all the sub-states. 
	It also has an external behavior, which is its interaction with its parent and siblings. This is defined in the SUB definition. 

	If a state has no child states, a leaf state or a pseudo state, it only has an external behavior, thus the COMP def is not necessary, and the complete behavior 
	can be defined in LEAF/COND/JOIN definitions.

    A hierarchical state machine is divided to several levels through COMP_STATE and SUB_STATE definitions. The reason is to provide 
	some sort of modularity/information hiding, by hiding the real destination from deeper states. 
	*/
	#define SME_BEGIN_COMP_STATE_DEF(_state, _parent, _entry, _exit)								\
		extern SME_EVENT_TABLE_T SME_COMPSTATE_EVT_TBL_REF(_state)[];						\
    	SME_STATE_T SME_COMPSTATE_REF(_state) =											\
		{ SME_STRINGIZE(_state), SME_STYPE_COMP, &SME_STATE_REF(_parent), (void*)_entry, _exit, SME_COMPSTATE_EVT_TBL_REF(_state) };    \
		SME_EVENT_TABLE_T SME_COMPSTATE_EVT_TBL_REF(_state)[] =						\
		{

	#define SME_BEGIN_ROOT_COMP_STATE_DEF(_root_state, _entry, _exit)								\
    	SME_STATE_T SME_STATE_REF(_root_state) =											\
		{ SME_STRINGIZE(_root_state), SME_STYPE_SUB, SME_NULL_STATE, (void*)(&SME_COMPSTATE_REF(_root_state)), SME_NULL_ACTION, NULL }; \
		extern SME_EVENT_TABLE_T SME_COMPSTATE_EVT_TBL_REF(_root_state)[];						\
    	SME_STATE_T SME_COMPSTATE_REF(_root_state) =											\
		{ SME_STRINGIZE(_root_state), SME_STYPE_COMP, SME_NULL_STATE, (void*)_entry, _exit, SME_COMPSTATE_EVT_TBL_REF(_root_state) };    \
		SME_EVENT_TABLE_T SME_COMPSTATE_EVT_TBL_REF(_root_state)[] =						\
		{
		

	/* The SUB definition holds a pointer to the sub-machine's internal behaviors. The COMP def appears in the file which defines 
	the internals of the state, and the SUB may appear in a different file, the one defining the behavior of the state's parent, 
	thus defining the external behavior of the state. For example, a state transition from this composite state to its sibling state 
	may be defined in the SUB definition body. 
	*/
    #define SME_BEGIN_SUB_STATE_DEF(_state, _parent) \
		extern SME_STATE_T SME_COMPSTATE_REF(_state); \
        SME_BEGIN_STATE_DEF(_state, SME_STYPE_SUB, _parent, (void*)(&SME_COMPSTATE_REF(_state)), SME_NULL_ACTION)

    #define SME_BEGIN_LEAF_STATE_DEF(_state, _parent, _entry, _exit) \
        SME_BEGIN_STATE_DEF(_state, SME_STYPE_LEAF, _parent, _entry, _exit)

    /* Conditional Pseudo State */
	#define SME_BEGIN_COND_STATE_DEF(_state, _parent, _cond) \
        SME_BEGIN_STATE_DEF(_state, SME_STYPE_COND, _parent, _cond, SME_NULL_ACTION)

    /* Join Pseudo State */
    #define SME_BEGIN_JOIN_STATE_DEF(_state, _parent) \
        SME_BEGIN_STATE_DEF(_state, SME_STYPE_JOIN, _parent, SME_NULL_ACTION, SME_NULL_ACTION)

	/* Event Handler Table */

	/* Initial Child State: A child which identifies initial state for state machines that have sub-states. 
       This child is required if and only if the machine has one or more <state> children */
	#define SME_ON_INIT_STATE( _Handler, _NewState) \
	{  SME_INIT_CHILD_STATE_ID, SME_NULL_GUARD, (SME_EVENT_HANDLER_T)_Handler, &SME_STATE_REF(_NewState)},
	
	/* External Transition without a guard. Note that no guard for pseudo states.*/
	#define SME_ON_EVENT(_EventID, _Handler, _NewState) \
	{  _EventID, SME_NULL_GUARD, (SME_EVENT_HANDLER_T)_Handler, &SME_STATE_REF(_NewState)},

	/* External Transition with a guard */
	#define SME_ON_EVENT_WITH_GUARD(_EventID, _Guard, _Handler, _NewState) \
	{  _EventID, (SME_TRAN_GUARD_T)_Guard, (SME_EVENT_HANDLER_T)_Handler, &SME_STATE_REF(_NewState)},

	/* Internal Transition without a guard */
	#define SME_ON_INTERNAL_TRAN(_EventID, _Handler) \
	{  _EventID, SME_NULL_GUARD, (SME_EVENT_HANDLER_T)_Handler, SME_INTERNAL_TRAN },

	/* Internal Transition with a guard */
	#define SME_ON_INTERNAL_TRAN_WITH_GUARD(_EventID, _Guard, _Handler) \
	{  _EventID, (SME_TRAN_GUARD_T)_Guard, (SME_EVENT_HANDLER_T)_Handler, SME_INTERNAL_TRAN },

	#define SME_ON_JOIN_TRAN( _Handler, _NewState) \
	{  SME_JOIN_STATE_ID, SME_NULL_GUARD, (SME_EVENT_HANDLER_T)_Handler, &SME_STATE_REF(_NewState)},
	
	#define SME_ON_EVENT_COND_ELSE( _Handler, _NewState) \
		SME_ON_EVENT(SME_EVENT_COND_ELSE, _Handler, _NewState)

	/* Specify an explicit entry to a child state when a state transition takes place to a composite state.  
	The reason of this macro is to enhance modularity in the C version engine. The component definitions of a composite state may be defined in a module.  
	Concerning a transition to a child state of the source state's sibling,  
	It can be separated into a 2-stage transition: from the source to the sibling state; and from the sibling state to its child state.
	Use SME_ON_EXPLICIT_ENTRY to define an explicit entry from parent state to a child state.
	For a deeper transition, it can be separated into several stages. Define every explicit entry in parent state through SME_ON_EXPLICIT_ENTRY.
	*/
	#define SME_ON_EXPLICIT_ENTRY(_EventID, _NewState) \
	{  SME_EXPLICIT_ENTRY_EVENT_ID(_EventID), SME_NULL_GUARD, SME_NULL_ACTION, &SME_STATE_REF(_NewState)},

	/* The SME_EXPLICIT_EXIT_FROM makes _EventID an explicit event going out of _SrcState instead of a transition from all children of the current composite state.  
	The reason of this macro is to enhance modularity in C version engine. The component definitions of a composite state may be defined in a module.  
	Concerning an outside transition to a sibling state of the source state's parent,  
	It can be separated into a 2-stage transition: explicit exit from the source to the parent state; and from the parent state to the sibling state.
	*/
	#define SME_EXPLICIT_EXIT_FROM(_EventID, _SrcState) \
	{  SME_EXPLICIT_EXIT_EVENT_ID(_EventID), SME_NULL_GUARD, SME_NULL_ACTION, &SME_STATE_REF(_SrcState)},

	/* A state built-in time out. On state entry, start a timer with given time-out value in milli-seconds.  
	There may be nested timeouts. It is not the SMEs responsibility to check whether the timeout hierarchy make sense.
	*/
	#define SME_ON_STATE_TIMEOUT(_Elapse, _Handler, _NewState) \
	{  SME_STATE_TIMEOUT_EVENT_ID(_Elapse), SME_NULL_GUARD, (SME_EVENT_HANDLER_T)_Handler, &SME_STATE_REF(_NewState)},

	#define SME_ON_STATE_TIMEOUT_WITH_GUARD(_Elapse, _Guard, _Handler, _NewState) \
	{  SME_STATE_TIMEOUT_EVENT_ID(_Elapse), (SME_TRAN_GUARD_T)_Guard, (SME_EVENT_HANDLER_T)_Handler, &SME_STATE_REF(_NewState)},

	#define SME_ON_STATE_TIMEOUT_INTERNAL_TRAN(_Elapse, _Handler) \
	{  SME_STATE_TIMEOUT_EVENT_ID(_Elapse), SME_NULL_GUARD, (SME_EVENT_HANDLER_T)_Handler, SME_INTERNAL_TRAN },

	#define SME_ON_STATE_TIMEOUT_INTERNAL_TRAN_WITH_GUARD(_Elapse, _Guard, _Handler) \
	{  SME_STATE_TIMEOUT_EVENT_ID(_Elapse), (SME_TRAN_GUARD_T)_Guard, (SME_EVENT_HANDLER_T)_Handler, SME_INTERNAL_TRAN },

	/*** Application ***/	
	/* State Machine Application ie. State Tree Root 
	FORMAT: const char * sAppName; const SME_STATE_T *pRoot; 
			SME_STATE_T *pAppState, *pHistoryState; void *pData; 
			SME_APP_T*pParent; SME_APP_T*pNext
	And a root sub-state which point to the root composite state.

    Users may define more than 1 application instances based on a state machine profile.
    */
	#define SME_APPLICATION_DEF(_app_name, _root_state) \
		SME_APP_T _app_name##App = { \
		#_app_name, &SME_COMPSTATE_REF(_root_state), SME_NULL_STATE, SME_NULL_STATE, {0}, 0, NULL, NULL, NULL, NULL, SME_REGIONID_ROOT_APP};

	/* Get application variable name. */
	#define SME_GET_APP_VAR(_app) _app##App

	/* Declare application variable that has external linkage. */
	#define SME_DEC_EXT_APP_VAR(_app) extern SME_APP_T _app##App;

	/*** History Transition ***/	
	/* Update state transition destination state for history transition. 
	#define SME_UPDATE_STATE_TRAN_DEST(_state, _tbl_idx, _p_new_state) \
		SME_STATE_EVT_TBL_POS_REF(_state,_tbl_idx) = _p_new_state;
	*/

	/*** Orthogonal State ***/	
	#define SME_BEGIN_ORTHO_COMP_STATE_DEF(_state, _parent, _entry, _exit)								\
		extern SME_REGION_CONTEXT_T SME_STATE_REGION_TBL_REF(_state)[];						\
    	SME_STATE_T SME_COMPSTATE_REF(_state) =											\
		{ SME_STRINGIZE(_state), SME_STYPE_ORTHO_COMP, &SME_STATE_REF(_parent), (void*)_entry, _exit, (SME_EVENT_TABLE_T*)(void*)(&SME_STATE_REGION_TBL_REF(_state)) };    \
		SME_REGION_CONTEXT_T SME_STATE_REGION_TBL_REF(_state)[] =						\
		{

	#define SME_MULTI_REGION_DEF(_RegionName,_InstanceNum, _Region, _RunMode, _nPriority) \
        {#_RegionName, _InstanceNum, &SME_COMPSTATE_REF(_Region), _RunMode, _nPriority}, 

	#define SME_END_ORTHO_STATE_DEF \
		{NULL, 0, NULL, 0, 0}       \
			};

#endif /* SME_CPP=FALSE */

#define SME_REGION_DEF(_RegionName, _Region, _RunMode, _nPriority) SME_MULTI_REGION_DEF(_RegionName, 1, _Region, _RunMode, _nPriority)

/* Use SME_CURR_DEFAULT_PARENT as the default parent of all sibling states under a composite state. */
#define SME_BEGIN_LEAF_STATE_DEF_P(_state, _entry, _exit) \
	SME_BEGIN_LEAF_STATE_DEF(_state, SME_CURR_DEFAULT_PARENT, _entry, _exit) 

#define SME_BEGIN_SUB_STATE_DEF_P(_state) \
	SME_BEGIN_SUB_STATE_DEF(_state, SME_CURR_DEFAULT_PARENT)

#define SME_BEGIN_COND_STATE_DEF_P(_state, _cond) \
	SME_BEGIN_COND_STATE_DEF(_state, SME_CURR_DEFAULT_PARENT, _cond)

#define SME_BEGIN_JOIN_STATE_DEF_P(_state) \
	SME_BEGIN_JOIN_STATE_DEF(_state, SME_CURR_DEFAULT_PARENT)

/******************************************************************************************
*  State Machine Engine APIs
******************************************************************************************/
void SmeInitEngine(SME_THREAD_CONTEXT_PT pThreadContext);

BOOL SmeActivateApp(SME_APP_T *pNewApp, SME_APP_T *pParentApp);
BOOL SmeDeactivateApp(SME_APP_T *pApp);
BOOL SmeSetFocus(SME_APP_T *pApp);
BOOL SmeDispatchEvent(SME_EVENT_T *pEvent, SME_APP_T *pApp);
void SmeRun();

typedef int (* SME_INIT_CALLBACK_T)(void *);  
void SmeThreadLoop(SME_THREAD_CONTEXT_T* pThreadContext, SME_APP_T *pApp, SME_INIT_CALLBACK_T pfnInitProc, void* pParam);

SME_EVENT_T *SmeCreateIntEvent(SME_EVENT_ID_T nEventId,
								   unsigned long nParam1,
								   unsigned long nParam2,
								   SME_EVENT_CAT_T nCategory,
								   SME_APP_T *pDestApp);
SME_EVENT_T *SmeCreatePtrEvent(SME_EVENT_ID_T nEventId,
								   void* pData,
								   unsigned long nSize,
								   SME_EVENT_CAT_T nCategory,
								   SME_APP_T *pDestApp);

BOOL SmeDeleteEvent(SME_EVENT_T *pEvent);
void SmeConsumeEvent(SME_EVENT_T *pEvent);
BOOL SmePostEvent(SME_EVENT_T *pEvent);

int SmePostThreadExtIntEvent(SME_THREAD_CONTEXT_T* pDestThreadContext, int nMsgID, int Param1, int Param2, 
						   SME_APP_T *pDestApp, unsigned long nSequenceNum,unsigned char nCategory);
int SmePostThreadExtPtrEvent(SME_THREAD_CONTEXT_T* pDestThreadContext, int nMsgID, void *pData, int nDataSize, 
						   SME_APP_T *pDestApp, unsigned long nSequenceNum,unsigned char nCategory);
SME_EVENT_HANDLER_T SmeSetEventFilterOprProc(SME_EVENT_HANDLER_T pfnEventFilter);
void SmeSetTimerProc(SME_STATE_TIMER_PROC_T pfnTimerProc, SME_KILL_TIMER_PROC_T pfnKillTimerProc);

void SmeSetExtEventOprProc(SME_GET_EXT_EVENT_PROC_T fnGetExtEvent, 
						   SME_DEL_EXT_EVENT_PROC_T fnDelExtEvent,
	SME_POST_THREAD_EXT_INT_EVENT_PROC_T fnPostThreadExtIntEvent,
	SME_POST_THREAD_EXT_PTR_EVENT_PROC_T fnPostThreadExtPtrEvent,
	SME_INIT_THREAD_EXT_MSG_BUF_PROC_T fnInitThreadExtMsgBuf,
	SME_INIT_THREAD_EXT_MSG_BUF_PROC_T fnFreeThreadExtMsgBuf);

SME_ON_EVENT_COME_HOOK_T SmeSetOnEventComeHook(SME_ON_EVENT_COME_HOOK_T pOnEventComeHook);
SME_ON_EVENT_HANDLE_HOOK_T SmeSetOnEventHandleHook(SME_ON_EVENT_HANDLE_HOOK_T pOnEventHandleHook);

void* SmeMAllocRelease(unsigned int nSize);
BOOL SmeMFreeRelease(void *pUserData);
void SmeSetMemOprProc(SME_MEM_ALLOC_PROC_T fnMAllocProc, SME_MEM_FREE_PROC_T fnMFreeProc);

void SmeSetTlsProc(SME_SET_THREAD_CONTEXT_PROC pfnSetThreadContext, SME_GET_THREAD_CONTEXT_PROC pfnGetThreadContext);

#if SME_UI_SUPPORT
	#define SME_SET_FOCUS SmeSetFocus
#else
	#define SME_SET_FOCUS(pApp) 
#endif

SME_APP_T* SmeCreateApp(const char* sAppName, int nNum, SME_STATE_T *pRoot);
BOOL SmeDestroyApp(SME_APP_T *pApp);

int SmeGetRegions(SME_APP_T *pApp, int nCount, SME_APP_T *pRegionAppList[], SME_THREAD_CONTEXT_T *pRegionThreadContextList[]);

/********************************************************************************************************** 
 * Macro to avoid compiler warning when a formal parameter is not used within a function.
 * This can happen if the function prototype is forced from outside, e.g. a callback for a system call,
 * and in this specific situation, the parameter is not used
 * The macro expansion solves the warning, and should be omitted by any optimizer.
 *
 * The "int a = x" inserts a reference to x. If you leave it like this, you get
 * a new warning "1 not used".
 * The "x = a" solves this.
 * Ergo ...
 */
#define SME_UNUSED_INT_PARAM(x) {int a = (x); (x) = a;}
#define SME_UNUSED_VOIDP_PARAM(x) {void* a = (x); (x) = a;}
#define SME_DIM(x) (sizeof(x)/sizeof(x[0]))

/*
warning C4127: conditional expression is constant
warning C4189: local variable is initialized but not referenced
warning C4100: unreferenced formal parameter
*/
#ifdef SME_WIN32
	#pragma warning( disable : 4127)
	#pragma warning( disable : 4189)
	#pragma warning( disable : 4100)
#endif 

#ifdef __cplusplus
}
#endif 

#endif /* #define SME_H */
