/*-----------------------------------------------------------------------------------------------------
 * Copyright 2007, Texas Instruments Incorporated
 *
 * This program has been modified from its original operation by Texas Instruments
 * to do the following:
 *       Update pApp->nRegionId in SmeCreateApp
 *       Added event filter plugin to SmeDispatchEvent
 *       Added SmeSetEventFilterOprProc function to set the event filter plugin
 *       Added SmePostThreadExtIntEvent function that uses the appropriate plugin to send events
 *       Added SmePostThreadExtPtrEvent function that uses the appropriate plugin to send events
 *       Made SetEventTimer and killTimer - plugins
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
 * ==============================================================================================================================*/

#include "sme.h"
#include "sme_debug.h"

#include "sme_cross_platform.h"

#if !SME_CPP && defined(SME_WIN32)
	/* C4055: A data pointer is cast (possibly incorrectly) to a function pointer. This is a level 1 warning under /Za and a level 4 warning under /Ze. */
	#pragma warning( disable : 4055)
#endif
/*******************************************************************************************
* State Machine Engine static variables.
*******************************************************************************************/
static SME_MEM_ALLOC_PROC_T g_fnMAllocProc = NULL;
static SME_MEM_FREE_PROC_T g_fnMFreeProc = NULL;

static SME_SET_THREAD_CONTEXT_PROC g_pfnSetThreadContext=NULL;
SME_GET_THREAD_CONTEXT_PROC g_pfnGetThreadContext=NULL;

static SME_GET_EXT_EVENT_PROC_T  g_pfnGetExtEvent=NULL;
static SME_DEL_EXT_EVENT_PROC_T  g_pfnDelExtEvent=NULL;
static SME_POST_THREAD_EXT_INT_EVENT_PROC_T g_pfnPostThreadExtIntEvent=NULL;
static SME_POST_THREAD_EXT_PTR_EVENT_PROC_T g_pfnPostThreadExtPtrEvent=NULL;
static SME_INIT_THREAD_EXT_MSG_BUF_PROC_T g_pfnInitThreadExtMsgBuf=NULL;
static SME_FREE_THREAD_EXT_MSG_BUF_PROC_T g_pfnFreeThreadExtMsgBuf=NULL;

static SME_EVENT_HANDLER_T g_pfnEventFilter = NULL;

static SME_STATE_TIMER_PROC_T g_pfnStateTimer = NULL;
static SME_KILL_TIMER_PROC_T g_pfnKillTimerProc = NULL;

BOOL DispatchInternalEvents(SME_THREAD_CONTEXT_PT pThreadContext);
BOOL DispatchEventToApps(SME_THREAD_CONTEXT_PT pThreadContext,SME_EVENT_T *pEvent);

static SME_STATE_T* TransitToState(SME_APP_T *pApp, SME_STATE_T *pOldState, SME_STATE_T *pNewState, SME_EVENT_T *pEvent,
								   SME_STATE_T *pExplicitNextState,/* IN/OUT */ int* pTranReason);

/*******************************************************************************************
* DESCRIPTION:  Initialize state machine engine given the thread context.
* INPUT:  None.
* OUTPUT: None.
* NOTE: 
*******************************************************************************************/
void SmeInitEngine(SME_THREAD_CONTEXT_PT pThreadContext)
{
	int i;
	if (!pThreadContext) return;

	// For the platform independent engine..
	// Save the thread context pointer to the TLS.
	if (g_pfnSetThreadContext) 
		(*g_pfnSetThreadContext)(pThreadContext);
	else
		XSetThreadContext(pThreadContext); /* By default. */

	memset(pThreadContext,0,sizeof(SME_THREAD_CONTEXT_T));

	pThreadContext->pActAppHdr = NULL;
	pThreadContext->pFocusedApp = NULL;

	pThreadContext->pEventQueueFront=NULL; 
	pThreadContext->pEventQueueRear=NULL;


	pThreadContext->fnOnEventComeHook = NULL; 
	pThreadContext->fnOnEventHandleHook = NULL;

	/*
	if (NULL==pThreadContext->fnGetExtEvent) 
		pThreadContext->fnGetExtEvent = XGetExtEvent; 
	if (NULL==pThreadContext->fnDelExtEvent)
		pThreadContext->fnDelExtEvent = XDelExtEvent;
	*/

	pThreadContext->nAppThreadID = XGetCurrentThreadId();

	/* Set all event pool are empty. */
	for (i=0; i<SME_EVENT_POOL_SIZE; i++)
		pThreadContext->EventPool[i].nEventID = SME_INVALID_EVENT_ID;
	pThreadContext->nEventPoolIdx =0;

}

/*******************************************************************************************
* DESCRIPTION:  Get an event data buffer from event pool.
* INPUT:  None
* OUTPUT: New event pointer. If pool is used up, return NULL. 
* NOTE: 
*   
*******************************************************************************************/
static SME_EVENT_T *GetAEvent()
{
	SME_EVENT_T *e=NULL;
	int nOldEventPoolIdx;
	SME_THREAD_CONTEXT_PT pThreadContext=NULL;

	if (g_pfnGetThreadContext)
		pThreadContext = (*g_pfnGetThreadContext)();
	if (!pThreadContext) return NULL;

	nOldEventPoolIdx = pThreadContext->nEventPoolIdx;
	do {
		if (pThreadContext->EventPool[pThreadContext->nEventPoolIdx].nEventID == SME_INVALID_EVENT_ID)
		{
			/*This event data buffer is available.*/
			e = &(pThreadContext->EventPool[pThreadContext->nEventPoolIdx]);
			pThreadContext->nEventPoolIdx = (pThreadContext->nEventPoolIdx+1)%SME_EVENT_POOL_SIZE;
			return e;
		} else
		{
			pThreadContext->nEventPoolIdx = (pThreadContext->nEventPoolIdx+1)%SME_EVENT_POOL_SIZE;
			if (pThreadContext->nEventPoolIdx == nOldEventPoolIdx)
				return NULL;
		}
	} while(TRUE);

}
/*******************************************************************************************
* DESCRIPTION:  Create a state machine event.
* INPUT:  event id, parameter1, parameter2, event category, destination application pointer.
* OUTPUT: New event pointer.
* NOTE: 
*   
*******************************************************************************************/
SME_EVENT_T *SmeCreateIntEvent(SME_EVENT_ID_T nEventId,
								   unsigned long nParam1,
								   unsigned long nParam2,
								   SME_EVENT_CAT_T nCategory,
								   SME_APP_T *pDestApp)
{
	SME_EVENT_T *e=NULL;

	if (nEventId==SME_INVALID_EVENT_ID)
		return NULL;

	e=GetAEvent();
	if(e)
	{
		e->nEventID=nEventId;
		e->nSequenceNum =0;
		e->pNext = NULL;
		e->Data.Int.nParam1 = nParam1;
		e->Data.Int.nParam2 = nParam2;
		e->pDestApp=pDestApp;
		e->pPortInfo = NULL;
 		e->nOrigin = SME_EVENT_ORIGIN_INTERNAL;
		e->nCategory=nCategory;
		e->nDataFormat = SME_EVENT_DATA_FORMAT_INT;
		e->bIsConsumed = FALSE;
	}
	return e;
}

SME_EVENT_T *SmeCreatePtrEvent(SME_EVENT_ID_T nEventId,
								   void* pData,
								   unsigned long nSize,
								   SME_EVENT_CAT_T nCategory,
								   SME_APP_T *pDestApp)
{
	SME_EVENT_T *e=NULL;

	e=GetAEvent();
	if(e)
	{
		e->nEventID=nEventId;
		e->nSequenceNum =0;
		e->pNext = NULL;
		e->Data.Ptr.pData = pData;
		e->Data.Ptr.nSize = nSize;
		e->pDestApp=pDestApp;
		e->pPortInfo = NULL;
		e->nOrigin = SME_EVENT_ORIGIN_INTERNAL; //by default
		e->nCategory=nCategory;
		e->nDataFormat = SME_EVENT_DATA_FORMAT_PTR;
		e->bIsConsumed = FALSE;
	}
	return e;
}

/*******************************************************************************************
* DESCRIPTION:  Delete a event by marking with SME_INVALID_EVENT_ID.
* INPUT:  
* OUTPUT: None.
* NOTE: 
*   
*******************************************************************************************/
BOOL SmeDeleteEvent(SME_EVENT_T *pEvent)
{
	if(!pEvent) return FALSE;
	pEvent->nEventID = SME_INVALID_EVENT_ID;
	return TRUE;
}

/*******************************************************************************************
Utilities for the state 
*******************************************************************************************/
static SME_STATE_T* GetCompState(SME_STATE_T *pSubState)
{
	if (SME_NULL_STATE==pSubState || SME_STYPE_SUB!=pSubState->nStateType)
		return SME_NULL_STATE;

#if SME_CPP	
	return (SME_STATE_T *)(pSubState->pCompState);
#else
	return (SME_STATE_T *)pSubState->pfnFunc;
#endif
}

static SME_EVENT_HANDLER_T GetStateEntryAction(SME_STATE_T *pState)
{
	if (SME_NULL_STATE == pState) 
		return NULL;

	if (SME_IS_PSEUDO_STATE(pState))
		return NULL;

	switch (pState->nStateType)
	{
	case SME_STYPE_LEAF:
	case SME_STYPE_COMP:
		return (SME_EVENT_HANDLER_T)(pState->pfnFunc); /* State Entry Action */
		break;
	case SME_STYPE_SUB:
		{
			SME_STATE_T *pCompState = GetCompState(pState);
			if (NULL!=pCompState) 
			{
				return (SME_EVENT_HANDLER_T)(pCompState->pfnFunc); /* State Entry Action */
			}
		}
		break;
	default:
		return NULL;
	};

	return NULL;
}

static SME_EVENT_HANDLER_T GetStateExitAction(SME_STATE_T *pState)
{
	if (SME_NULL_STATE == pState) 
		return NULL;

	if (SME_IS_PSEUDO_STATE(pState))
		return NULL;

	switch (pState->nStateType)
	{
	case SME_STYPE_LEAF:
	case SME_STYPE_COMP:
		return (SME_EVENT_HANDLER_T)(pState->pfnExitFunc);
		break;
	case SME_STYPE_SUB:
		{
			SME_STATE_T *pCompState = GetCompState(pState);
			if (NULL!=pCompState) 
			{
				return (SME_EVENT_HANDLER_T)(pCompState->pfnExitFunc);
			}
		}
		break;
	default:
		return NULL;
	};

	return NULL;
}

/* Get initial child state, initial action, and state built-in timeout information */
static void GetStateInfo(SME_STATE_T *pState, SME_STATE_T **ppInitChildState, SME_EVENT_HANDLER_T *ppfnInitAction, 
						 int *pTimeout, SME_EVENT_HANDLER_T *ppfnTimeoutAction, SME_STATE_T **ppTimeoutDestState)
{
	SME_EVENT_TABLE_T *pEvtHldTbl;

	if (NULL==pState || NULL==ppInitChildState || NULL==ppfnInitAction)
		return;

	*ppInitChildState = SME_NULL_STATE; /* By default. */

	if (SME_STYPE_SUB == pState->nStateType)
	{
		SME_STATE_T *pCompState = GetCompState(pState);
		/*Note: No event handler table for ortho-composite state */
		if (SME_STYPE_COMP == pCompState->nStateType)
			pState = pCompState; /* Get Composite State */
		else
			*ppInitChildState = SME_NULL_STATE;
	}
	else if (SME_STYPE_COMP != pState->nStateType)
		*ppInitChildState = SME_NULL_STATE;
	
	/* Composite State */
	pEvtHldTbl = pState->EventTable;

	if (NULL==pEvtHldTbl)
	{
		return;
	}

	/* Traverse the event handler table. */
	while (SME_INVALID_EVENT_ID != pEvtHldTbl->nEventID)
	{
		if (SME_INIT_CHILD_STATE_ID == pEvtHldTbl->nEventID)
		{
			*ppInitChildState = pEvtHldTbl->pNewState;
			*ppfnInitAction = pEvtHldTbl->pHandler;
		} else if (SME_IS_STATE_TIMEOUT_EVENT_ID(pEvtHldTbl->nEventID))
		{
			if ( (NULL!=pTimeout) && (NULL!=ppfnTimeoutAction) && (NULL!=ppTimeoutDestState))
			{
				*pTimeout = SME_GET_STATE_TIMEOUT_VAL(pEvtHldTbl->nEventID);
				*ppfnTimeoutAction = pEvtHldTbl->pHandler;
				*ppTimeoutDestState = pEvtHldTbl->pNewState;
			}
		}
		pEvtHldTbl++;
	}

	/* Can not find an initial child state in a composite state. 
	   A special case: an application with a single state. 
	*/
	SME_ASSERT_MSG(!(SME_STYPE_COMP == pState->nStateType && *ppInitChildState == SME_NULL_STATE && pState->pParent!=NULL)
		, SMESTR_ERR_NO_INIT_STATE_IN_COMP);
	return; 
}

/* Call an event handler, or a conditional function.
If the handler function is NULL, return SME_EVENT_COND_ELSE.
*/
static int CallHandler(SME_EVENT_HANDLER_T pEvtHdl, SME_APP_T* pApp, SME_EVENT_T *pEvent)
{
	if(NULL==pEvtHdl)
		return SME_EVENT_COND_ELSE;
#if SME_CPP
	return (pApp->*pEvtHdl)(pApp,pEvent);
#else
	return pEvtHdl(pApp,pEvent);
#endif
}

/* Dynamically create an application. */
SME_APP_T* SmeCreateApp(const char* sAppName, int nNum, SME_STATE_T *pRoot)
{
	SME_APP_T *pApp;
	char sAppNameBuf[SME_MAX_APP_NAME_LEN+1];
	memset(sAppNameBuf, 0, sizeof(sAppNameBuf));

	if (nNum>=0)
		snprintf(sAppNameBuf, sizeof(sAppNameBuf)-1, SME_REGION_NAME_FMT,sAppName,nNum);
	else
		strncpy(sAppNameBuf, sAppName, sizeof(sAppNameBuf)-1);

#if SME_CPP
	pApp = new SME_APP_T(sAppNameBuf,pRoot);
#else
	pApp = (SME_APP_T *)XEmptyMemAlloc(sizeof(SME_APP_T));
	if (pApp)
	{
		strncpy(pApp->sAppName, sAppNameBuf, SME_MAX_APP_NAME_LEN);
		pApp->pRoot = pRoot;
        pApp->nRegionId = nNum;
	}
#endif

	return pApp;
}

/* Dynamically destroy an application. */
BOOL SmeDestroyApp(SME_APP_T *pApp)
{
	if (pApp)
	{
#if SME_CPP
		delete pApp;
#else
		XMemFree(pApp);
#endif
		return TRUE;
	} else
		return FALSE;
}

typedef struct SME_REGION_THREAD_CONTEXT_T_TAG
{
	SME_THREAD_CONTEXT_T ThreadContext;
	XTHREADHANDLE ThreadHandle;
	const char* sRegionName;
	int nNum;
	SME_STATE_T *pRegionRoot;
	SME_APP_T *pOrthoApp; /* The common dummy application for all regions. */
	struct SME_REGION_THREAD_CONTEXT_T_TAG *pNext;
} SME_REGION_THREAD_CONTEXT_T;

#ifdef SME_WIN32
	static unsigned __stdcall RegionThreadProc(void *Param)
#else
	static void* RegionThreadProc(void *Param)
#endif
{
	SME_REGION_THREAD_CONTEXT_T *pRegionThreadContext = (SME_REGION_THREAD_CONTEXT_T*)(Param);
	SME_APP_T* pRegionApp=NULL;

	if (NULL==pRegionThreadContext)
		return 0;

	pRegionApp = SmeCreateApp(pRegionThreadContext->sRegionName,pRegionThreadContext->nNum,pRegionThreadContext->pRegionRoot); 
	SmeThreadLoop(&(pRegionThreadContext->ThreadContext), pRegionApp, NULL, NULL);
	SmeDestroyApp(pRegionApp);
	return 0;
}

void SmeThreadLoop(SME_THREAD_CONTEXT_T* pThreadContext, SME_APP_T *pApp, SME_INIT_CALLBACK_T pfnInitProc, void* pParam)
{
	if (NULL==pThreadContext || NULL==pApp)
		return;
	
	SmeInitEngine(pThreadContext); 
	
	(*g_pfnInitThreadExtMsgBuf)();

	if (pfnInitProc)
		(*pfnInitProc)(pParam);

	SmeActivateApp(pApp, NULL);

	SmeRun();

	(*g_pfnFreeThreadExtMsgBuf)();

	XFreeThreadContext(pThreadContext); 
}


/* Enter the orthogonal state */
static BOOL EnterOrthoState(SME_STATE_T *pOrthoState, SME_APP_T *pApp)
{
	SME_REGION_CONTEXT_T *pRegion = NULL;
	SME_STATE_T *pCompOrthoState=NULL;
	SME_APP_T* pOrthoApp;
	SME_REGION_THREAD_CONTEXT_T *pRegionThreadContext1=NULL;

	if (NULL==pOrthoState)
		return FALSE;

	pCompOrthoState = GetCompState(pOrthoState);
	if (pCompOrthoState->nStateType != SME_STYPE_ORTHO_COMP)
		return FALSE;

	pRegion = (SME_REGION_CONTEXT_T*)(pCompOrthoState->EventTable);

	if (NULL==pRegion)
		return FALSE;

	/* Create a dummy application which is the parent of all region applications.*/
	/* SmeActivateApp(): The new original application will be inserted into the current thread context. 
	   There is no regular children state for an Orthogonal state, 
	   so if a set of active applications with a same parent application, they should be 
	   the regions of an orthogonal state.
	*/
	pOrthoApp = SmeCreateApp(pOrthoState->sStateName, SME_REGIONID_DUMMY, NULL); /* The root of this dummy application is NULL. */
	SmeActivateApp(pOrthoApp,pApp);

	/* Copy the current thread context to the region thread context as the first item in the region thread context list. */
	pRegionThreadContext1 = (SME_REGION_THREAD_CONTEXT_T *)XEmptyMemAlloc(sizeof(SME_REGION_THREAD_CONTEXT_T));
	SME_ASSERT_MSG(NULL!=pRegionThreadContext1, SMESTR_ERR);
	memcpy(&(pRegionThreadContext1->ThreadContext),XGetThreadContext(), sizeof(SME_THREAD_CONTEXT_T));
	pRegionThreadContext1->pOrthoApp = pOrthoApp; 
	pRegionThreadContext1->ThreadHandle = 0;
	pRegionThreadContext1->sRegionName = pOrthoState->sStateName;
	pRegionThreadContext1->nNum = 1;
	pRegionThreadContext1->pRegionRoot = NULL;
	pRegionThreadContext1->pNext =NULL;
	pApp->pRegionThreadContextList = pRegionThreadContext1; /* The fist item in the list. */

	while (pRegion->pRoot)
	{
		int i=0;
		for (i=0; i<pRegion->nInstanceNum; i++)
		{
			switch (pRegion->nRunningMode)
			{
			case SME_RUN_MODE_PARENT_THREAD:
				{
					SME_APP_T* pRegionApp = SmeCreateApp(pRegion->sRegionName,(pRegion->nInstanceNum>1)?i:SME_REGIONID_DUMMY,pRegion->pRoot); 
					SmeActivateApp(pRegionApp,pOrthoApp);
				}
				break;
			case SME_RUN_MODE_SEPARATE_THREAD:
				{
					SME_REGION_THREAD_CONTEXT_T *pRegionThreadContext = (SME_REGION_THREAD_CONTEXT_T *)XEmptyMemAlloc(sizeof(SME_REGION_THREAD_CONTEXT_T));
					if (!pRegionThreadContext)
						return FALSE;
					pRegionThreadContext->pRegionRoot = pRegion->pRoot;
					pRegionThreadContext->sRegionName = pRegion->sRegionName;
					pRegionThreadContext->nNum = (pRegion->nInstanceNum>1)?i:-1;
					pRegionThreadContext->pOrthoApp = pOrthoApp;
					pRegionThreadContext->pNext = pRegionThreadContext1->pNext;
					pRegionThreadContext1->pNext = pRegionThreadContext; /* Insert to the list after the first item. */
					XCreateThread(RegionThreadProc, pRegionThreadContext, &(pRegionThreadContext->ThreadHandle));
					XSetThreadPriority(pRegionThreadContext->ThreadHandle,pRegion->nPriority);
				}
				break;
			case SME_RUN_MODE_SEPARATE_PROCESS:
				break;
			}
		} 
		pRegion++; 
	}

	//SME_CATCH_STATES();
	return TRUE;
}

static int ExitOrthoState(SME_APP_T *pApp)
{
	SME_THREAD_CONTEXT_T *pThreadContext = XGetThreadContext();
	SME_APP_T *p = pThreadContext->pActAppHdr;
	SME_REGION_THREAD_CONTEXT_T *pRegionThreadContext1;
	SME_REGION_THREAD_CONTEXT_T *pChildThreadContext=NULL;

	if (pApp)
		pRegionThreadContext1 = (SME_REGION_THREAD_CONTEXT_T *)(pApp->pRegionThreadContextList);
	else
		return -1;

	/* Free all region applications running at the current thread.*/
	while (p!=NULL)
	{
		if (pRegionThreadContext1 && p->pParent==pRegionThreadContext1->pOrthoApp)
		{
			SmeDeactivateApp(p);
			SmeDestroyApp(p);
			/* Note: pThreadContext->pActAppHdr probably changed. */
			p = pThreadContext->pActAppHdr;
		} else
		{
			p=p->pNext;
		}
	}

	if (pRegionThreadContext1)
	{
		SmeDeactivateApp(pRegionThreadContext1->pOrthoApp);
		SmeDestroyApp(pRegionThreadContext1->pOrthoApp);
		pRegionThreadContext1->pOrthoApp=NULL;
	}

	/* Request all region applications to stop running.*/
	if (pRegionThreadContext1 && pRegionThreadContext1->pNext)
	{
		SME_REGION_THREAD_CONTEXT_T *pNextChild=NULL;

		 /* Post SME_EVENT_EXIT_LOOP event to all region threads.*/
		 pChildThreadContext = pRegionThreadContext1->pNext;
		 while (pChildThreadContext)
		 {
			(*g_pfnPostThreadExtIntEvent)(&(pChildThreadContext->ThreadContext), SME_EVENT_EXIT_LOOP, 0, 0, NULL,0,SME_EVENT_CAT_OTHER);
			pChildThreadContext = pChildThreadContext->pNext;
		 }

		 /* Wait for all region threads to exit.*/
		 pChildThreadContext = pRegionThreadContext1->pNext;
		 while (pChildThreadContext)
		 {
			XWaitForThread(pChildThreadContext->ThreadHandle);
			XCLOSE_HANDLE(pChildThreadContext->ThreadHandle);

			pNextChild = pChildThreadContext->pNext;
			XMemFree(pChildThreadContext);
			pChildThreadContext = pNextChild;
		 }
	}

	XMemFree(pRegionThreadContext1);
	pApp->pRegionThreadContextList = NULL;

	//SME_CATCH_STATES();
	return 0;
}

/*
Return the current region applications and region thread contexts of the given application.

pApp
[in] The given application.

nCount 
[in] Number of region number of the given application.

pRegionAppList 
[out] Pointer to an array of region applications. 

pRegionThreadContextList 
[out] Pointer to an array of the thread context list that regions are running at. 

Note: This function should be call at the thread that the given application is running.

*/
int SmeGetRegions(SME_APP_T *pApp, int nCount, SME_APP_T *pRegionAppList[], SME_THREAD_CONTEXT_T *pRegionThreadContextList[])
{
	SME_THREAD_CONTEXT_T *pThreadContext = XGetThreadContext();
	SME_APP_T *p = pThreadContext->pActAppHdr;
	SME_REGION_THREAD_CONTEXT_T *pRegionThreadContext1;
	SME_REGION_THREAD_CONTEXT_T *pChildThreadContext=NULL;
	int i=0;

	if (pApp && nCount>0)
		pRegionThreadContext1 = (SME_REGION_THREAD_CONTEXT_T *)(pApp->pRegionThreadContextList);
	else
		return -1;

	/* Free all region applications running at the current thread.*/
	while (p!=NULL)
	{
		if (pRegionThreadContext1 && p->pParent==pRegionThreadContext1->pOrthoApp)
		{
			pRegionAppList[i] = p;
			pRegionThreadContextList[i] = &(pRegionThreadContext1->ThreadContext);
			i++;

			if (i>=nCount)
				return nCount;
		}

		p=p->pNext;
	}

	/* Request all region applications to stop running.*/
	if (pRegionThreadContext1 && pRegionThreadContext1->pNext)
	{
		 /* Post SME_EVENT_EXIT_LOOP event to all region threads.*/
		 pChildThreadContext = pRegionThreadContext1->pNext;
		 while (pChildThreadContext)
		 {
			pRegionAppList[i] = pChildThreadContext->ThreadContext.pActAppHdr;
			pRegionThreadContextList[i] = &(pChildThreadContext->ThreadContext);
			i++;

			if (i>=nCount)
				return nCount;

			pChildThreadContext = pChildThreadContext->pNext;
		 }
	}

	return i;
}

/*******************************************************************************************
* DESCRIPTION: Activate the given application. 
* INPUT:  
*  1) pNewApp: The new activated application.
*  2) pParentApp: who starts this new activated application.
* OUTPUT: None.
*  TRUE: Start it successfully.
*  FALSE: Fail to start it.
* NOTE: 
*  The active applications are linked as a stack.
*  NULL <--- Node.pNext  <--- Node.pNext  <--- pThreadContext->pActAppHdr
*******************************************************************************************/
BOOL SmeActivateApp(SME_APP_T *pNewApp, SME_APP_T *pParentApp)
{
	SME_STATE_T *pState;
	SME_THREAD_CONTEXT_PT pThreadContext=NULL;
	int nTranReason = SME_REASON_ACTIVATED;

	if (g_pfnGetThreadContext)
		pThreadContext = (*g_pfnGetThreadContext)();
	if (!pThreadContext) return FALSE;

	if(!pNewApp || (SME_IS_ACTIVATED(pNewApp) && pNewApp->pRoot !=NULL)) return FALSE;

	pNewApp->pParent = pParentApp;
    /* Push the new application to active application stack. */
	pNewApp->pNext=pThreadContext->pActAppHdr;
	pThreadContext->pActAppHdr=pNewApp;
	
	/* Call default entry functions. */
	pState = pNewApp->pRoot; /* The new application state. */
	pNewApp->pAppState = pNewApp->pRoot; /* Keep track of the route of state transitions including non-leaf node.*/
	pNewApp->pHistoryState = pNewApp->pRoot;
	memset(pNewApp->StateTimers,0,sizeof(pNewApp->StateTimers));
	pNewApp->nStateNum =0;
	pNewApp->pRegionThreadContextList = NULL;


	while (pState)
	{
		pState = TransitToState(pNewApp,NULL,pState,NULL,NULL,&nTranReason); 
	}

	/* Dispatch all internal events which may be triggered by entry functions.*/
	DispatchInternalEvents(pThreadContext);
	
	/* Focus on the new application. */
	SME_SET_FOCUS(pNewApp);

	return TRUE;
}

/*******************************************************************************************
* DESCRIPTION:  De-activate the given application.
* INPUT:  
*  pApp: the application to be de-activated.
* OUTPUT: None.
*  TRUE: Deactivate it successfully.
*  FALSE: Fail to de-activate it.
* NOTE: 
*******************************************************************************************/
BOOL SmeDeactivateApp(SME_APP_T *pApp)
{
	SME_APP_T *p,*pPre;
	SME_STATE_T *pState;
	SME_THREAD_CONTEXT_PT pThreadContext=NULL;
	SME_EVENT_HANDLER_T pEvtHdl=NULL;

	if (g_pfnGetThreadContext)
		pThreadContext = (*g_pfnGetThreadContext)();
	if (!pThreadContext) return FALSE;

	if (!pApp || (!SME_IS_ACTIVATED(pApp) && pApp->pRoot !=NULL)) return FALSE;

	/* Locate the application in the active application stack.*/
	p=pThreadContext->pActAppHdr;
	pPre=NULL;
	while (p!=NULL)
	{
		/* The application should be located in the leaf of the active application tree.*/
		if (p->pParent == pApp)
		{
			SME_ASSERT_MSG(p->pParent != pApp, SMESTR_ERR_DEACTIVATE_NON_LEAF_APP);
			return FALSE;
		}
		/* 1) Get the previous node of pApp.*/
		if (p->pNext == pApp)
			pPre=p;
		/* 2) Move to next application and traverse all applications. Assert pApp is a leaf. */
		p=p->pNext;
	}

	/* Adjust active application stack. */
	if (pApp==pThreadContext->pActAppHdr)
		/* the application is the active application header.*/
		pThreadContext->pActAppHdr=pApp->pNext;
	else
	{
		SME_ASSERT_MSG(NULL!=pPre, SMESTR_ERR);
		pPre->pNext=pApp->pNext;
	}

	/* If de-activated application is a focused application,the de-activated application's
	parent will be new focused application*/
	if (pApp==pThreadContext->pFocusedApp)
    {
		if (pApp->pParent != NULL)
         {
			SME_SET_FOCUS(pApp->pParent); /* Focus on the parent application.*/
         }
         else
         {
			pThreadContext->pFocusedApp = NULL; /* No any application focused.*/
         }
    }     
	/* Call exit functions from leaf to the root.*/
	pState = pApp->pAppState; /* It should be leaf. Special case: if the root is NULL, pState is NULL. */
	while (pState!=SME_NULL_STATE)
	{
		/* Kill the state built-in timers. */
		if (0!=pApp->StateTimers[pApp->nStateNum-1])
		{
			g_pfnKillTimerProc(pApp->StateTimers[pApp->nStateNum-1]);
			pApp->StateTimers[pApp->nStateNum-1]=0;
		}
		pApp->nStateNum--;

		pEvtHdl = GetStateExitAction(pState);
		CallHandler(pEvtHdl,pApp,NULL);

		SME_STATE_TRACK(NULL, pApp, pApp->pAppState, SME_REASON_DEACTIVATED,0); // Log the old state only. No destination state for state exit.

		pState = pState->pParent;
		pApp->pAppState = pState;
	}; 

	/* Reset the application data.*/
	pApp->pAppState = SME_NULL_STATE;
	pApp->pHistoryState = SME_NULL_STATE;
	pApp->pNext =NULL;
	pApp->pParent =NULL;
	pApp->pRegionThreadContextList =NULL;

	/* Dispatch all internal events which may be triggered by exit functions.*/
	DispatchInternalEvents(pThreadContext);

	return TRUE;
}

/*******************************************************************************************
* DESCRIPTION: Set an application focused. 
* INPUT:  
* OUTPUT: None.
* NOTE: 
*   Send SME_EVENT_KILL_FOCUS event to old focused application. And send SME_EVENT_SET_FOCUS
*   to the new focused application.
*******************************************************************************************/
#if SME_UI_SUPPORT

BOOL SmeSetFocus(SME_APP_T *pApp)
{
	SME_EVENT_T *pEvent1,*pEvent2;
	SME_THREAD_CONTEXT_PT pThreadContext=NULL;

	if (g_pfnGetThreadContext)
		pThreadContext = (*g_pfnGetThreadContext)();
	if (!pThreadContext) return FALSE;

	if (!pApp) return FALSE;

	if (pThreadContext->pFocusedApp)
	{
		pEvent1=SmeCreateIntEvent(SME_EVENT_KILL_FOCUS,
			0, /*(unsigned long)pApp,*/
			0,
			SME_EVENT_CAT_UI,
			pThreadContext->pFocusedApp);
		if (pEvent1!=NULL)
		{
			SmeDispatchEvent(pEvent1,pThreadContext->pFocusedApp);
			SmeDeleteEvent(pEvent1);
		} else return FALSE;
	}

    pEvent2=SmeCreateIntEvent(SME_EVENT_SET_FOCUS,
		0, /*(unsigned long)pThreadContext->pFocusedApp,*/
		0,
		SME_EVENT_CAT_UI,
		pApp);
	if (pEvent2!=NULL)
	{
		SmeDispatchEvent(pEvent2,pApp);
		SmeDeleteEvent(pEvent2);
	}

    pThreadContext->pFocusedApp=pApp;
	return TRUE;
}
#endif /*SME_UI_SUPPORT*/

/*******************************************************************************************
* DESCRIPTION:  Post an event to queue.
* INPUT:  pEvent: An event.
* OUTPUT: None.
* NOTE: 
*   
*******************************************************************************************/
BOOL SmePostEvent(SME_EVENT_T *pEvent)
{
	SME_THREAD_CONTEXT_PT pThreadContext=NULL;

	if (g_pfnGetThreadContext)
		pThreadContext = (*g_pfnGetThreadContext)();
	if (!pThreadContext) return FALSE;

	if (pEvent == NULL) return FALSE;

	if (pThreadContext->pEventQueueRear==NULL) 
	{ 
		/* The first event in queue. */
		pThreadContext->pEventQueueFront=pThreadContext->pEventQueueRear=pEvent;
	}
	else 
	{
		/* Append the event to queue.*/
		pThreadContext->pEventQueueRear->pNext=pEvent;
		pEvent->pNext=NULL;
		pThreadContext->pEventQueueRear=pEvent;
	}
	return TRUE;
}

/*******************************************************************************************
* DESCRIPTION:  Get an event from queue.
* INPUT:  pEvent: An event.
* RETURN: Event in the head of queue. return NULL if not available.
* NOTE: 
*   
*******************************************************************************************/
SME_EVENT_T * GetEventFromQueue()
{
	/* Get an event from event queue if available. */
	SME_EVENT_T *pEvent = NULL;
	SME_THREAD_CONTEXT_PT pThreadContext=NULL;

	if (g_pfnGetThreadContext)
		pThreadContext = (*g_pfnGetThreadContext)();
	if (!pThreadContext) return NULL;

	if (pThreadContext->pEventQueueFront != NULL)
	{
		pEvent = pThreadContext->pEventQueueFront;
		pThreadContext->pEventQueueFront = pThreadContext->pEventQueueFront->pNext;
		/* Set the end of queue to NULL if queue is empty.*/
		if (pThreadContext->pEventQueueFront == NULL)
			pThreadContext->pEventQueueRear = NULL;
	}
	return pEvent;
}

/*******************************************************************************************
 Check the state's event handler table from leaf to root.
 If find it and pass the guard if available, stop searching, no matter there is another handler in parent state's event table.
********************************************************************************************/
static BOOL IsHandlerAvailable(/* IN */SME_STATE_T *pState, SME_APP_T *pApp, SME_EVENT_T *pEvent,
						/* OUT */ SME_STATE_T **ppNewState, SME_EVENT_HANDLER_T *ppHandler)
{
	/* Trace back from leaf to root, so as to retrieve all event handler tables.
	 Find what nNewState is */
	SME_STATE_T *pSuperState=SME_NULL_STATE;
	SME_EVENT_TABLE_T *pStateEventTable;
	SME_STATE_T *pCurrState = pState;
	int i=0;
	int nStateDepth = pApp->nStateNum-1;
	BOOL bSubStateChecked = FALSE;
	if (SME_NULL_STATE==pState || NULL==pEvent || NULL==ppNewState || NULL==ppHandler)
		return FALSE;

	if (SME_IS_PSEUDO_STATE(pState))
		return FALSE;

	/* Check the event handler table in Composite state, and then the table in sub state.*/
	if (SME_STYPE_SUB == pState->nStateType)
	{
		pSuperState = GetCompState(pState);
	}
	
	/* Note: No event handler table for SME_STYPE_ORTHO_COMP */
	if (pSuperState && pSuperState->nStateType==SME_STYPE_COMP)
	{
		pStateEventTable = pSuperState->EventTable;
		bSubStateChecked = FALSE;
	}
	else
	{
		pStateEventTable = pState->EventTable;
		bSubStateChecked = TRUE;
	}

	while (TRUE)
	{
		/* Check the current state's event handler table.*/
		i=0;
		while (pStateEventTable && pStateEventTable[i].nEventID != SME_INVALID_EVENT_ID)
		{
			if (pStateEventTable[i].nEventID == SME_EXPLICIT_EXIT_EVENT_ID(pEvent->nEventID)) 
			{
				/* The Explicit Exit makes nEventID an explicit event going out of pNewState (the source state) instead of 
				a transition from all children of the current composite state.  */
				SME_STATE_T *p = pCurrState;
				BOOL bIsSrcState = FALSE; /* Is the current state is the source state of the explicit exit.*/
				
				/* Check whether the current state is the source state of the explicit exit or its children state. */
				while(SME_NULL_STATE!=p)
				{
					if (p==pStateEventTable[i].pNewState)
					{
						/* It is an explicit exit, proceed with looking for SME_ON_EVENT() for this explicit exit.*/
						bIsSrcState = TRUE;
						break;
					}
					p=p->pParent;
				}

				if (!bIsSrcState)
				{
					/* Not match an explicit exit. */
					SME_STATE_TRACK(pEvent, pApp, pApp->pAppState, SME_REASON_NOT_MATCH,0);
					return FALSE;
				}
			} else if (
				(pStateEventTable[i].nEventID == pEvent->nEventID) /* Regular events including SME_EVENT_TIMER */
				|| (SME_IS_STATE_TIMEOUT_EVENT_ID(pStateEventTable[i].nEventID) && SME_EVENT_STATE_TIMER==pEvent->nEventID
					&& pEvent->nSequenceNum == pApp->StateTimers[nStateDepth]) /* State built-in timeout events with the corresponding state timer sequence number*/
				) /* Distinguish regular timer and state built-in timer.*/
			{
#if SME_CPP
				if ((0 == pStateEventTable[i].pGuardFunc) /* NULL pointer */
				|| (pApp->pSME_NULL_GUARD == pStateEventTable[i].pGuardFunc) /* pointer to a NULL function. */
				|| (pApp->*pStateEventTable[i].pGuardFunc)(pApp,pEvent) /* Call guard function. */
				)
#else
				if (SME_NULL == pStateEventTable[i].pGuardFunc 
					|| (*pStateEventTable[i].pGuardFunc)(pApp,pEvent)
				)
#endif
				{
					/* Match, and the guard returns TRUE. */
					/* Hit */
					*ppNewState=pStateEventTable[i].pNewState;
					*ppHandler = pStateEventTable[i].pHandler;
					return TRUE;
				} else
				{
					/* Match, however the guard returns FALSE. */
					SME_STATE_TRACK(pEvent, pApp, pApp->pAppState, SME_REASON_GUARD,0);
					return FALSE;
				}
			} 
			i++;
		} /* Search the event handler table. */
		
		if (!bSubStateChecked)
		{
			/* About to check sub-state */
			pStateEventTable = pState->EventTable;
			bSubStateChecked = TRUE;
		} else
		{
			/* Get the parent state's event handler table. */
			if (SME_NULL_STATE==pState->pParent) break; /* Reach the root. */
			/* Search the parent. */
			pState = pState->pParent;
			nStateDepth--;
			/* SME_STYPE_SUB == pState->nStateType */
			pSuperState = GetCompState(pState);
			pStateEventTable = pSuperState->EventTable;
			bSubStateChecked = FALSE;
		}
	}

	SME_STATE_TRACK(pEvent, pApp, pApp->pAppState, SME_REASON_NOT_MATCH,0);
	return FALSE;
}

/*******************************************************************************************
 Check whether an explicit entry on the given event is available in a given composite state event handler table, 
 If find it, return the entry child state .
********************************************************************************************/
static BOOL IsExplicitEntryAvailable(/* IN */SME_STATE_T *pState, SME_EVENT_T *pEvent,
						/* OUT */ SME_STATE_T **ppNewState)
{
	SME_EVENT_TABLE_T *pStateEventTable=NULL;
	SME_STATE_T *pCompState=SME_NULL_STATE;

	if (SME_NULL_STATE ==pState || NULL == pEvent || NULL==ppNewState)
		return FALSE;

	if (SME_STYPE_COMP != pState->nStateType && SME_STYPE_SUB != pState->nStateType)
		return FALSE;

	if (SME_STYPE_SUB == pState->nStateType)
	{
		SME_STATE_T *p = GetCompState(pState);
		if (p->nStateType == SME_STYPE_COMP)
			pCompState = GetCompState(pState);
		else pCompState = pState;

	}else
		pCompState = pState;
	
	pStateEventTable = pCompState->EventTable;

	if (pStateEventTable)
	{
		int i=0;
		while (SME_INVALID_EVENT_ID != pStateEventTable[i].nEventID)
		{
			if (SME_EXPLICIT_ENTRY_EVENT_ID(pEvent->nEventID) == pStateEventTable[i].nEventID)
			{
				*ppNewState = pStateEventTable[i].pNewState;
				return TRUE;
			}

			i++;
		}
	}
	return FALSE;
}

/*******************************************************************************************
* DESCRIPTION: Dispatch the incoming event to an application if it is specified, otherwise
*  dispatch to all active applications until it is consumed.  
* INPUT:  
*  1) pEvent: Incoming event 
*  2) pApp: The destination application that event will be dispatched.
* OUTPUT: None.
* NOTE: 
*	1) Call exit functions in old state   
*	2) Call event handler functions   
*	3) Call entry functions in new state  
*  4) Transit from one state region to another state region. All states exit functions that jump out 
*  the old region will be called. And all states exit functions that jump in the new region will be called.
*  5) Although there is a property pEvent->pDestApp in SME_EVENT_T, this function will ignore this one,
*		because if pEvent->pDestApp is NULL, this event have to dispatch to all active applications.
*******************************************************************************************/
BOOL SmeDispatchEvent(SME_EVENT_T *pEvent, SME_APP_T *pApp)
{
	SME_STATE_T *pOldState=SME_NULL_STATE; /* Old state should be a leaf.*/
	SME_STATE_T *pState=SME_NULL_STATE;
    SME_STATE_T *pNewState=SME_NULL_STATE;
	int i;
	SME_EVENT_HANDLER_T pHandler = NULL;

	SME_STATE_T *OldStateStack[SME_MAX_STATE_TREE_DEPTH];
	SME_STATE_T *NewStateStack[SME_MAX_STATE_TREE_DEPTH];
	int nOldStateStackTop =0;
	int nNewStateStackTop =0;
	int nRepeatTime=0;
	int nTranReason=SME_REASON_HIT;

	SME_THREAD_CONTEXT_PT pThreadContext=NULL;
	
	#if SME_DEBUG
	int nBeginTick=XGetTick();
	#endif

	if (g_pfnGetThreadContext)
		pThreadContext = (*g_pfnGetThreadContext)();
	if (!pThreadContext) return FALSE;

	if (pEvent==NULL || pApp==NULL) return FALSE;

    /* Check event filter */
    if (g_pfnEventFilter)
    {
        /* If filter denies access, return FALSE */
        if ((*g_pfnEventFilter)(pApp, pEvent) == FALSE)
            return FALSE;
    }
	pOldState = pApp->pAppState; /* Old state should be a leaf. */

	if (!IsHandlerAvailable(pOldState, pApp, pEvent, &pNewState, &pHandler)) 
		return FALSE;

	do { /* Loop until the destination state is not a pseudo state .*/

		/* Call the exit functions if it is not an internal transition and the old state is a regular state. */
		if (SME_INTERNAL_TRAN != pNewState)
		{
			if (!SME_IS_PSEUDO_STATE(pNewState))
				pApp->pHistoryState = pOldState; /* The original state before transition.*/

			/* The state stack: 0  the leaf (pOldState) --> the root state . */
			/* It is a state transition.
			 Push all old state's ancestors.
			*/
			nOldStateStackTop =0;
			nNewStateStackTop =0;

			pState = pOldState;
			while (pState!=SME_NULL_STATE)
			{
				OldStateStack[nOldStateStackTop++] = pState;
				pState=pState->pParent;
				if (nOldStateStackTop >= SME_MAX_STATE_TREE_DEPTH)
					return FALSE;
			}

			/* Push all new state's ancestors. */
			pState = pNewState;
			while (pState!=SME_NULL_STATE)
			{
				NewStateStack[nNewStateStackTop++] = pState;
				pState=pState->pParent;
				if (nNewStateStackTop >= SME_MAX_STATE_TREE_DEPTH)
					return FALSE;
			}

			/* Pop all equal states except the last one.
			 Special case 1: self transition state1->state1, leave one item in each stack.
			 Special case 2: a parent state transits to its child state, leave one item in the parent state stack.
			*/
			while ((nOldStateStackTop>1) && (nNewStateStackTop>1)
				&& (OldStateStack[nOldStateStackTop-1] == NewStateStack[nNewStateStackTop-1]))
			{
				nOldStateStackTop--;
				nNewStateStackTop--;
			}

			/* Get the leaf of the old state.
			 Note: Old state should be a leaf state.
			 Call exit functions from the leaf nState to the top state in the old state stack.
			 */
			for (i=0; i<nOldStateStackTop; i++)
			{
				SME_EVENT_HANDLER_T pEvtHdl;
				pState = OldStateStack[i];

				/* Stop state built-in timer. */
				if (0!=pApp->StateTimers[pApp->nStateNum-1])
				{
					g_pfnKillTimerProc(pApp->StateTimers[pApp->nStateNum-1]);
					pApp->StateTimers[pApp->nStateNum-1] =0; /* Clear the state timer. */
				}

				SME_STATE_TRACK(pEvent, pApp, pApp->pAppState, SME_REASON_DEACTIVATED,0); // No destination state for state exit.

				pApp->nStateNum--;
				pApp->pAppState = pState; /* Keep track of the route of state transitions including non-leaf node.*/

				if (SME_STYPE_SUB == pState->nStateType)
				{
					SME_STATE_T	*pCompOrthoState = GetCompState(pState);
					if (pCompOrthoState->nStateType == SME_STYPE_ORTHO_COMP)
						/* Exit Orthogonal state*/
						ExitOrthoState(pApp);
				}

				pEvtHdl=GetStateExitAction(pState); /* If it is pseudo state, return NULL. */
				CallHandler(pEvtHdl,pApp,pEvent);

			};
		}; /* end of non internal transition.*/

		/*******************************************************************************************
		 Call event handler function if given event handler is available and handler is not empty.
		 Maybe their is a transition, however handler is empty.
		*/
		CallHandler(pHandler,pApp,pEvent);
		pHandler= NULL;

		if (pNewState == SME_INTERNAL_TRAN)
			SME_STATE_TRACK(pEvent, pApp, pOldState, SME_REASON_INTERNAL_TRAN, XGetTick() - nBeginTick);
		else
		{
			SME_STATE_T *pExplicitNextState=NULL;
			SME_STATE_T *pNextState=NULL;
			/* Explicit entry to a composite state. 
			 Note: Do not update the old state.
			*/
			for (i=nNewStateStackTop-1; i>=0; i--)
			{
				pNewState = NewStateStack[i];
				if (i>=1)
					pExplicitNextState = NewStateStack[i-1];
				else
					pExplicitNextState = NULL;

				pNewState = TransitToState(pApp,pOldState,pNewState,pEvent,pExplicitNextState,&nTranReason);
				/* On loop exit, pNewState will be next state by default.*/
			}

			/* Default entry to a state. */
			while (pNewState)
			{
				pNextState = TransitToState(pApp,pOldState,pNewState,pEvent,NULL,&nTranReason); 
				pOldState = pNewState;
				pNewState = pNextState;
			}
		}

	   nRepeatTime++;
	} while (nRepeatTime<=SME_MAX_PSEUDO_TRAN_NUM && pNewState != SME_INTERNAL_TRAN 
		&& (SME_IS_PSEUDO_STATE(pNewState) || SME_IS_PSEUDO_STATE(pOldState))); 
	/* Loop until the destination state is not a pseudo state. */

	SME_ASSERT_MSG(nRepeatTime<SME_MAX_PSEUDO_TRAN_NUM, SMESTR_ERR_A_LOOP_PSEUDO_STATE_TRAN);

	/*******************************************************************************************
	 Call event handle hook function if given event handler is available and no matter whether handler is empty or not.
	 */
	if (pThreadContext->fnOnEventHandleHook)
		(*pThreadContext->fnOnEventHandleHook)((SME_EVENT_ORIGIN_T)(pEvent->nOrigin), 
			pEvent, 
			pApp, 
			SME_GET_APP_STATE(pApp));

	return TRUE;
}

/************************************************************************************************************************************ 
TRIGGERS:
1) Active an application. pOldState==NULL
2) Default State Transition. pExplicitNextState==NULL
3) Explicit State Transition.
*************************************************************************************************************************************/
static SME_STATE_T* TransitToState(SME_APP_T *pApp, SME_STATE_T *pOldState, SME_STATE_T *pNewState, SME_EVENT_T *pEvent,
								   SME_STATE_T *pExplicitNextState, /* IN/OUT */ int* pTranReason)
{
	int nBeginTick=XGetTick();
	SME_STATE_T *pNextState=NULL;
	
	if (SME_INTERNAL_TRAN == pNewState)
		return NULL;

	/******************************************************************************************
		New state is a Pseudo State
	*/
	if (SME_STYPE_COND == pNewState->nStateType)
	{
		/* Conditional Pseudo State => a state */
		SME_EVENT_ID_T nCondRet = (SME_EVENT_ID_T)CallHandler((SME_EVENT_HANDLER_T)pNewState->pfnFunc,pApp,pEvent);
		SME_EVENT_TABLE_T *pStateEventTable = pNewState->EventTable;
		int i=0;

		pApp->pAppState = pNewState; 
		SME_STATE_TRACK(pEvent, pApp, pOldState, *pTranReason, XGetTick() - nBeginTick);
		nBeginTick = XGetTick();
		*pTranReason = SME_REASON_COND;

		/* There should be no built-in state timer in pseudo states. Do not increase nStateNum when enters a pseudo state. */

		while (pStateEventTable && pStateEventTable[i].nEventID != SME_INVALID_EVENT_ID)
		{
			if (nCondRet == pStateEventTable[i].nEventID
				|| SME_EVENT_COND_ELSE == pStateEventTable[i].nEventID)
			{
				/* Call the conditional action function. */
				CallHandler(pStateEventTable[i].pHandler,pApp,pEvent);
				/* Get the destination state */
				pNextState = pStateEventTable[i].pNewState;
				/* The SME_EVENT_COND_ELSE probably is not located at the last item in the condition list. */
				if (nCondRet == pStateEventTable[i].nEventID)
					break;
			}
			i++;
		}
		SME_ASSERT_MSG(pStateEventTable[i].nEventID != SME_INVALID_EVENT_ID, SMESTR_ERR_FAIL_TO_EVAL_COND);

	} else if (SME_STYPE_JOIN == pNewState->nStateType)
	{
		/* Join Pseudo State => a state */
		SME_EVENT_TABLE_T *pStateEventTable = pNewState->EventTable;

		pApp->pAppState = pNewState; 
		SME_STATE_TRACK(pEvent, pApp, pOldState, *pTranReason, XGetTick() - nBeginTick);
		nBeginTick = XGetTick();
		*pTranReason = SME_REASON_JOIN;

		/* There should be no built-in state timer in pseudo states. Do not increase nStateNum when enters a pseudo state. */

		if (pStateEventTable && SME_JOIN_STATE_ID == pStateEventTable[0].nEventID)
		{
			/* Call the conditional action function. */
			CallHandler(pStateEventTable[0].pHandler,pApp,pEvent);
			/* Get the destination state */
			pNextState = pStateEventTable[0].pNewState;

			SME_ASSERT_MSG(SME_NULL_STATE != pNewState, SMESTR_ERR_NO_JOIN_TRAN_STATE);
		} else
		{
			/* ERROR */
			SME_ASSERT_MSG(TRUE, SMESTR_ERR_NO_JOIN_TRAN_STATE);
		}

	} else if (!SME_IS_PSEUDO_STATE(pNewState))
	{
		/******************************************************************************************
			New state is a Regular State or Orthogonal state.
		*/
		SME_STATE_T *pChildState=NULL;
		SME_EVENT_HANDLER_T pfnDefSubStateAction=NULL;
		int nStateTimeOut=-1;
		SME_EVENT_HANDLER_T pfnStateTimeoutAction=NULL;
		SME_STATE_T *pTimeOutDestState=NULL;

		/* Keep track of the route of state transitions including non-leaf node.*/
		pApp->pHistoryState = pOldState; /* The destination state after transition.*/
		pApp->pAppState = pNewState;
		GetStateInfo(pNewState, &pChildState, &pfnDefSubStateAction, &nStateTimeOut, &pfnStateTimeoutAction, &pTimeOutDestState);

		/* Start the state built-in timer. */
		if (nStateTimeOut!=-1)
		{
			/* Set a state built-in timer. */
			pApp->StateTimers[pApp->nStateNum] = g_pfnStateTimer(pApp,SME_STATE_BUILT_IN_TIMEOUT_VAL(nStateTimeOut)); 
			SME_ASSERT_MSG(0!=pApp->StateTimers[pApp->nStateNum], SMESTR_ERR_FAIL_TO_SET_TIMER);
		}
		pApp->nStateNum++;

		CallHandler(GetStateEntryAction(pNewState), pApp,pEvent);
		if (SME_STYPE_SUB==pNewState->nStateType)
		{
			SME_STATE_T	*pCompOrthoState = GetCompState(pNewState);
			if (pCompOrthoState->nStateType == SME_STYPE_ORTHO_COMP)
				/* Orthogonal state */
				EnterOrthoState(pNewState,pApp);
		}

		SME_STATE_TRACK(pEvent, pApp, pOldState, *pTranReason, XGetTick() - nBeginTick);
		*pTranReason = SME_REASON_ACTIVATED;

		if (NULL==pExplicitNextState)
		{
			pNextState = pChildState;
			/* pNewState is a composite state. */
			if (NULL!=pChildState)
			{
				/* Explicit entry to a specific child state. */
				if (!IsExplicitEntryAvailable(pNewState,pEvent,&pNextState))
				{
					/* Call the action for the initial sub-state. */
					CallHandler(pfnDefSubStateAction,pApp,pEvent);
					pNextState = pChildState;
				} /* Note: pNextState is the explicit entry state */
			} else
			{
				/* Call the action for the initial sub-state. */
				CallHandler(pfnDefSubStateAction,pApp,pEvent);
				pNextState = pChildState;
			}
		} else
		{
			pNextState = pExplicitNextState;
		}

	} /* End of regular state. */

	return pNextState;
}
/*******************************************************************************************
* DESCRIPTION:  This API function installs getting and deleting external event functions. 
*  It will never exit. 
* INPUT:  
* OUTPUT: None.
* NOTE: 
*   
*******************************************************************************************/
void SmeSetExtEventOprProc(SME_GET_EXT_EVENT_PROC_T fnGetExtEvent, 
						   SME_DEL_EXT_EVENT_PROC_T fnDelExtEvent,
	SME_POST_THREAD_EXT_INT_EVENT_PROC_T fnPostThreadExtIntEvent,
	SME_POST_THREAD_EXT_PTR_EVENT_PROC_T fnPostThreadExtPtrEvent,
	SME_INIT_THREAD_EXT_MSG_BUF_PROC_T fnInitThreadExtMsgBuf,
	SME_INIT_THREAD_EXT_MSG_BUF_PROC_T fnFreeThreadExtMsgBuf)
{
	g_pfnGetExtEvent = fnGetExtEvent;
	g_pfnDelExtEvent = fnDelExtEvent;

	g_pfnPostThreadExtIntEvent = fnPostThreadExtIntEvent;
	g_pfnPostThreadExtPtrEvent = fnPostThreadExtPtrEvent;

	g_pfnInitThreadExtMsgBuf = fnInitThreadExtMsgBuf;
	g_pfnFreeThreadExtMsgBuf = fnFreeThreadExtMsgBuf;
}

/*******************************************************************************************
* DESCRIPTION:  This API function is the state machine engine event handling loop function. 
*  It will never exit. 
* INPUT:  
* OUTPUT: None.
* NOTE: 

Engine has to check internal event first, because SmeActivateApp() may trigger some internal events.

Case:
	SmeActivateApp(&SME_GET_APP_VAR(Player1),NULL);
	SmeRun();
  
*******************************************************************************************/
void SmeRun()
{
	SME_EVENT_T ExtEvent;
	SME_EVENT_T *pEvent=NULL;
	
	SME_THREAD_CONTEXT_PT pThreadContext=NULL;
	if (g_pfnGetThreadContext)
		pThreadContext = (*g_pfnGetThreadContext)();
	if (!pThreadContext) return;

	if (!g_pfnGetExtEvent) return;


	while (TRUE)
	{
		/* Check the internal event pool firstly. */
		pEvent = GetEventFromQueue();
		if (pEvent == NULL)
		{
			/* Wait for an external event. */
			if (FALSE == (*g_pfnGetExtEvent)(&ExtEvent)) 
				return; // Exit the thread.

			pEvent = &ExtEvent;
			pEvent->nOrigin = SME_EVENT_ORIGIN_EXTERNAL;

			/* Call hook function on an external event coming. */
			if (pThreadContext->fnOnEventComeHook)
				(*pThreadContext->fnOnEventComeHook)(SME_EVENT_ORIGIN_EXTERNAL, pEvent);
		}else
		{
			/* Call hook function on an internal event coming. */
			if (pThreadContext->fnOnEventComeHook)
				(*pThreadContext->fnOnEventComeHook)(SME_EVENT_ORIGIN_INTERNAL, pEvent);
		}

		do { 
			DispatchEventToApps(pThreadContext, pEvent);

			/* Free internal event. Free external event later. */
			if (pEvent != &ExtEvent)
				SmeDeleteEvent(pEvent);

			/* Get an event from event queue if available. */
			pEvent = GetEventFromQueue();
			if (pEvent != NULL)
			{
				/* Call hook function on an internal event coming. */
				if (pThreadContext->fnOnEventComeHook)
					(*pThreadContext->fnOnEventComeHook)(SME_EVENT_ORIGIN_INTERNAL, pEvent);
			}
			else 
			{
				/* The internal event queue is empty. */
				break;
			}
		} while (TRUE); /* Get all events from the internal event pool. */

		/* Free external event if necessary. */
		if (g_pfnDelExtEvent)
		{
			(*g_pfnDelExtEvent)(&ExtEvent);
			// Engine should delete this event, because translation of external event will create an internal event. 
			SmeDeleteEvent(&ExtEvent); 
		}

	} /* Wait for an external event. */
}

/*******************************************************************************************
* DESCRIPTION:   
* INPUT:  
* OUTPUT: None.
* NOTE: SmeRun() and SmeActivateApp()/SmeDeactivateApp() calls this function.  
********************************************************************************************/
BOOL DispatchInternalEvents(SME_THREAD_CONTEXT_PT pThreadContext)
{
	SME_EVENT_T *pEvent=NULL;
	
	if (!pThreadContext) return FALSE;


	pEvent = GetEventFromQueue();
	while (pEvent != NULL)
	{
		pEvent->nOrigin = SME_EVENT_ORIGIN_INTERNAL;
		/* Call hook function on an internal event coming. */
		if (pThreadContext->fnOnEventComeHook)
			(*pThreadContext->fnOnEventComeHook)(SME_EVENT_ORIGIN_INTERNAL, pEvent);
		
		DispatchEventToApps(pThreadContext, pEvent);

		/* Free internal event*/
		SmeDeleteEvent(pEvent);
		/* Next internal event? */
		pEvent = GetEventFromQueue();
	}
	return TRUE;
}

/*******************************************************************************************
* DESCRIPTION:   
* INPUT:  
* OUTPUT: None.
* NOTE: SmeRun() and SmeActivateApp()/SmeDeactivateApp() calls this function.  
********************************************************************************************/
BOOL DispatchEventToApps(SME_THREAD_CONTEXT_PT pThreadContext,SME_EVENT_T *pEvent)
{
	SME_APP_T *pApp;
	if (pThreadContext==NULL || pEvent==NULL) return FALSE;

	/*Dispatch it to active applications*/
	if (pEvent->pDestApp)
	{
		/* This event has destination application. Dispatch it if the application is active.*/
		if (SME_IS_ACTIVATED(pEvent->pDestApp))
			/* Dispatch it to an destination application. */
			SmeDispatchEvent(pEvent, pEvent->pDestApp);
	}
	else if (pEvent->nCategory == SME_EVENT_CAT_UI)
		/* Dispatch UI event to the focused application. */
		SmeDispatchEvent(pEvent, pThreadContext->pFocusedApp);
	else 
	{
		/* Traverse all active applications. */
		pApp = pThreadContext->pActAppHdr;
		while (pApp != NULL) 
		{
			SmeDispatchEvent(pEvent, pApp);
			if (pEvent->bIsConsumed) 
				break;
			else pApp = pApp->pNext; 
		}
	}
	return TRUE;
}
/*******************************************************************************************
* DESCRIPTION:  This API function install a hook function. It will be called when event comes. 
* INPUT:  
* OUTPUT: None.
* NOTE: 
*   
*******************************************************************************************/
SME_ON_EVENT_COME_HOOK_T SmeSetOnEventComeHook(SME_ON_EVENT_COME_HOOK_T fnHook)
{
	SME_ON_EVENT_COME_HOOK_T fnOldHook;
	SME_THREAD_CONTEXT_PT pThreadContext=NULL;
	if (g_pfnGetThreadContext)
		pThreadContext = (*g_pfnGetThreadContext)();
	if (!pThreadContext) return NULL;

	fnOldHook = pThreadContext->fnOnEventComeHook;
	pThreadContext->fnOnEventComeHook = fnHook;
	return fnOldHook;
}

/*******************************************************************************************
* DESCRIPTION:  This API function install a hook function. It will be called when event is handled. 
* INPUT:  
* OUTPUT: None.
* NOTE: 
*   
*******************************************************************************************/
SME_ON_EVENT_HANDLE_HOOK_T SmeSetOnEventHandleHook(SME_ON_EVENT_HANDLE_HOOK_T fnHook)
{
	SME_ON_EVENT_HANDLE_HOOK_T fnOldHook;
	SME_THREAD_CONTEXT_PT pThreadContext=NULL;
	if (g_pfnGetThreadContext)
		pThreadContext = (*g_pfnGetThreadContext)();
	if (!pThreadContext) return NULL;

	fnOldHook = pThreadContext->fnOnEventHandleHook;
	pThreadContext->fnOnEventHandleHook = fnHook;
	return fnOldHook;
}
/*******************************************************************************************
* DESCRIPTION:  This API function set an event consumed. It will not dispatched to another application. 
* INPUT:  
* OUTPUT: None.
* NOTE: 
*   
*******************************************************************************************/
void SmeConsumeEvent(SME_EVENT_T *pEvent)
{
	if (!pEvent) return;
	pEvent->bIsConsumed = TRUE;
}

/*******************************************************************************************
* DESCRIPTION:  This API function sets memory allocation and free function pointers. 
* INPUT: fnMAllocProc: Memory allocation function pointer;
*	      fnMFreeProc: 	Memory free function pointer.	 
* OUTPUT: None.
* NOTE: 
*   
*******************************************************************************************/
void SmeSetMemOprProc(SME_MEM_ALLOC_PROC_T fnMAllocProc, SME_MEM_FREE_PROC_T fnMFreeProc)
{
	g_fnMAllocProc= fnMAllocProc;
	g_fnMFreeProc = fnMFreeProc;
}

/*******************************************************************************************
* DESCRIPTION:  State machine engine memory allocation function in release version. 
* INPUT: nSize: Memory size;
* OUTPUT: Allocated memory pointer.
* NOTE: It will just call user defined memory allocation function.
*   
*******************************************************************************************/
void* SmeMAllocRelease(unsigned int nSize)
{
	if (g_fnMAllocProc)
		return (*g_fnMAllocProc)(nSize);
	else return NULL;
}

/*******************************************************************************************
* DESCRIPTION:  State machine engine memory free function in release version. 
* INPUT: pUserData: Memory block pointer;
* OUTPUT: The result depends on user defined memory free function.
* NOTE: It will just call user defined memory free function.
*   
*******************************************************************************************/
BOOL SmeMFreeRelease(void * pUserData)
{
	if (g_fnMFreeProc)
		return (*g_fnMFreeProc)(pUserData);
	else return FALSE;
}

/*******************************************************************************************
* DESCRIPTION:  This API function sets thread local storage function pointers. 
* INPUT: pSetThreadContextProc: Thread context setting function pointer;
*	      pGetThreadContext: 	Thread context getting function pointer.	 
* OUTPUT: None.
* NOTE: 
*   
*******************************************************************************************/
void SmeSetTlsProc(SME_SET_THREAD_CONTEXT_PROC pfnSetThreadContext, SME_GET_THREAD_CONTEXT_PROC pfnGetThreadContext)
{
	g_pfnSetThreadContext = pfnSetThreadContext;
	g_pfnGetThreadContext = pfnGetThreadContext;
}

/*******************************************************************************************
* DESCRIPTION:  This API function uses the appropriate plugin to send INT events
*   
*******************************************************************************************/
int SmePostThreadExtIntEvent(SME_THREAD_CONTEXT_T* pDestThreadContext, int nMsgID, int Param1, int Param2, 
						   SME_APP_T *pDestApp, unsigned long nSequenceNum,unsigned char nCategory)
{
    if (g_pfnPostThreadExtIntEvent)
    {
        (*g_pfnPostThreadExtIntEvent)(pDestThreadContext, nMsgID, Param1, Param2, 
                                      pDestApp, nSequenceNum, nCategory);
    }
    return 0;
}

/*******************************************************************************************
* DESCRIPTION:  This API function uses the appropriate plugin to send PTR events
*   
*******************************************************************************************/
int SmePostThreadExtPtrEvent(SME_THREAD_CONTEXT_T* pDestThreadContext, int nMsgID, void *pData, int nDataSize, 
						   SME_APP_T *pDestApp, unsigned long nSequenceNum,unsigned char nCategory)
{
    if (g_pfnPostThreadExtPtrEvent)
    {
        (*g_pfnPostThreadExtPtrEvent)(pDestThreadContext, nMsgID, pData, nDataSize, 
                                      pDestApp, nSequenceNum, nCategory);
    }
    return 0;
}

/*******************************************************************************************
* DESCRIPTION:  This API function sets the SME event filter.
* INPUT: pfnEventFilter: Pointer to event filter function
* Return: Value of previous filter function pointer. Can be used for chaining, etc.
*   
*******************************************************************************************/
SME_EVENT_HANDLER_T SmeSetEventFilterOprProc(SME_EVENT_HANDLER_T pfnEventFilter)
{
    SME_EVENT_HANDLER_T p;

    p = g_pfnEventFilter;
	g_pfnEventFilter = pfnEventFilter;
    return p;
}

/*******************************************************************************************
* DESCRIPTION:  This API function sets the Timer functions
* INPUT: pfnEventFilter: Pointer to timer function
* Return: Value of previous filter function pointer. Can be used for chaining, etc.
*   
*******************************************************************************************/
void SmeSetTimerProc(SME_STATE_TIMER_PROC_T pfnTimerProc,
                     SME_KILL_TIMER_PROC_T pfnKillTimerProc)
{
	g_pfnStateTimer = pfnTimerProc;
    g_pfnKillTimerProc = pfnKillTimerProc;
}

