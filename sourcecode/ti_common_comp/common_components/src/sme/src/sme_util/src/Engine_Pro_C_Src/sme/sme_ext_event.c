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
 Externel event translation 
 Users may customize the external event handling in this file based on the working environment of your program. 
*/

#include "sme_ext_event.h"
#include "sme_cross_platform.h"
#include <memory.h>
#include <stdio.h>


typedef struct tagEXTMSG
{
	SME_EVENT_ID_T nMsgID; // 0: stand for empty entity
	unsigned char nDataFormat ; /* Flag for this event. SME_EVENT_DATA_FORMAT_INT=0, SME_EVENT_DATA_FORMAT_PTR*/
	unsigned char nCategory ; /* Category of this event. */
	union SME_EVENT_DATA_T Data;
	SME_APP_T *pDestApp;
	unsigned long nSequenceNum;
	SME_THREAD_CONTEXT_T* pDestThread;
}	X_EXT_MSG_T;


/* Note: Each thread has an independent message pool, an event/condition and a Mutex. 
When a new external event post, set the event/condition signaled.
The Mutex for a thread pool is to synchronize the message pool access between the event sending thread and the receiving thread.

*/
#define MSG_BUF_SIZE  100
typedef struct tagEXTMSGPOOL
{
	int nMsgBufHdr;
	int nMsgBufRear;
	X_EXT_MSG_T MsgBuf[MSG_BUF_SIZE];
	XEVENT EventToThread;
	XMUTEX MutexForPool;
} X_EXT_MSG_POOL_T;

///////////////////////////////////////////////////////////////////////////////////////////
//   nMsgBufHdr  (Get from the head) <============== (Append to the rear) nMsgBufRear
///////////////////////////////////////////////////////////////////////////////////////////
/* Initialize the external event buffer at the current thread. */
BOOL XInitMsgBuf()
{
	SME_THREAD_CONTEXT_T* pThreadContext = XGetThreadContext();
	X_EXT_MSG_POOL_T *pMsgPool;
	
	if (NULL!=pThreadContext->pExtEventPool) /* Prevent from creating more than once. */
		return FALSE;

	pThreadContext->pExtEventPool = (void*)malloc(sizeof(X_EXT_MSG_POOL_T));
	pMsgPool =(X_EXT_MSG_POOL_T*)(pThreadContext->pExtEventPool);
	if (NULL==pMsgPool) 
		return FALSE;
	memset(pMsgPool, 0, sizeof(X_EXT_MSG_POOL_T));

	XCreateMutex(&(pMsgPool->MutexForPool));
	XCreateEvent(&(pMsgPool->EventToThread));

	return TRUE;
}

/* Free the external event buffer at the current thread. */
BOOL XFreeMsgBuf()
{
	SME_THREAD_CONTEXT_T* pThreadContext = XGetThreadContext();

	if (NULL!=pThreadContext && NULL!=pThreadContext->pExtEventPool)
	{
		free(pThreadContext->pExtEventPool);
		pThreadContext->pExtEventPool= NULL;
		return TRUE;
	}

	return FALSE;
}

/* Is message available at the current thread event pool?*/
static BOOL XIsMsgAvailable(void *pArg)
{
	SME_THREAD_CONTEXT_T* p = XGetThreadContext();
	X_EXT_MSG_POOL_T *pMsgPool;
	if (NULL==p ||  NULL== p->pExtEventPool)
		return FALSE;

	SME_UNUSED_VOIDP_PARAM(pArg);

	pMsgPool = (X_EXT_MSG_POOL_T*)p->pExtEventPool;

	if (pMsgPool->nMsgBufHdr==pMsgPool->nMsgBufRear)
		return FALSE; // empty buffer.

	return TRUE;
}


/* Thread-safe action to append an external event to the rear of the queue at the destination thread.
 Timer event overflow prevention.
*/
static void XAppendMsgToBuf(void *pArg)
{
	X_EXT_MSG_T *pMsg = (X_EXT_MSG_T*)pArg;
	int nHdr;
	X_EXT_MSG_POOL_T *pMsgPool;
	if (NULL==pMsg || NULL==pMsg->pDestThread || NULL==pMsg->pDestThread->pExtEventPool)
		return;

	pMsgPool = (X_EXT_MSG_POOL_T*)(pMsg->pDestThread->pExtEventPool);
	
	if (((pMsgPool->nMsgBufRear+1) % MSG_BUF_SIZE) == pMsgPool->nMsgBufHdr)
		return; // buffer full.

	// Prevent duplicate SME_EVENT_TIMER event triggered by a timer in the queue.
	nHdr = pMsgPool->nMsgBufHdr;
	if (SME_EVENT_TIMER == pMsg->nMsgID)
	{
		while (nHdr != pMsgPool->nMsgBufRear)
		{
			if (SME_EVENT_TIMER == pMsgPool->MsgBuf[nHdr].nMsgID
			&& pMsg->nSequenceNum == pMsgPool->MsgBuf[nHdr].nSequenceNum)
				return; 
			nHdr = (nHdr+1) % MSG_BUF_SIZE;
		}
	}

	memcpy(&(pMsgPool->MsgBuf[pMsgPool->nMsgBufRear]),pMsg,sizeof(X_EXT_MSG_T));

	pMsgPool->nMsgBufRear = (pMsgPool->nMsgBufRear+1)%MSG_BUF_SIZE;
}

/* Thread-safe action to remove an external event from the current thread event pool.*/
static void XGetMsgFromBuf(void *pArg)
{
	X_EXT_MSG_T *pMsg = (X_EXT_MSG_T*)pArg;
	SME_THREAD_CONTEXT_T* p = XGetThreadContext();
	X_EXT_MSG_POOL_T *pMsgPool;
	if (NULL==pMsg || NULL==p || NULL==p->pExtEventPool)
		return;

	pMsgPool = (X_EXT_MSG_POOL_T*)(p->pExtEventPool);

	if (pMsgPool->nMsgBufHdr==pMsgPool->nMsgBufRear)
		return; // empty buffer.

	memcpy(pMsg,&(pMsgPool->MsgBuf[pMsgPool->nMsgBufHdr]),sizeof(X_EXT_MSG_T));

	pMsgPool->MsgBuf[pMsgPool->nMsgBufHdr].nMsgID =0;

	pMsgPool->nMsgBufHdr = (pMsgPool->nMsgBufHdr+1)%MSG_BUF_SIZE;

}

int XPostThreadExtIntEvent(SME_THREAD_CONTEXT_T* pDestThreadContext, int nMsgID, int Param1, int Param2, 
						   SME_APP_T *pDestApp, unsigned long nSequenceNum,unsigned char nCategory)
{
	X_EXT_MSG_T Msg;
	X_EXT_MSG_POOL_T *pMsgPool;
	if (nMsgID==0 || NULL== pDestThreadContext || NULL==pDestThreadContext->pExtEventPool)
		return -1;

	Msg.nMsgID = nMsgID;
	Msg.pDestApp = pDestApp;
	Msg.pDestThread = pDestThreadContext;
	Msg.nSequenceNum = nSequenceNum;

	Msg.nDataFormat = SME_EVENT_DATA_FORMAT_INT;
	Msg.nCategory = nCategory;
	Msg.Data.Int.nParam1 = Param1;
	Msg.Data.Int.nParam2 = Param2;

	pMsgPool = (X_EXT_MSG_POOL_T *)(pDestThreadContext->pExtEventPool);

	XSignalEvent(&(pMsgPool->EventToThread),&(pMsgPool->MutexForPool),(XTHREAD_SAFE_ACTION_T)XAppendMsgToBuf,&Msg);
	return 0;
}

int XPostThreadExtPtrEvent(SME_THREAD_CONTEXT_T* pDestThreadContext, int nMsgID, void *pData, int nDataSize, 
						   SME_APP_T *pDestApp, unsigned long nSequenceNum,unsigned char nCategory)
{
	X_EXT_MSG_T Msg;
	X_EXT_MSG_POOL_T *pMsgPool;
	if (nMsgID==0 || pDestThreadContext==NULL || NULL==pDestThreadContext->pExtEventPool) 
		return -1;

	Msg.nMsgID = nMsgID;
	Msg.pDestApp = pDestApp;
	Msg.pDestThread = pDestThreadContext;
	Msg.nSequenceNum = nSequenceNum;
	Msg.nCategory = nCategory;

	Msg.nDataFormat = SME_EVENT_DATA_FORMAT_PTR;

	if (pData!=NULL && nDataSize>0)
	{
#if SME_CPP
		Msg.Data.Ptr.pData = new char[nDataSize];
#else
		Msg.Data.Ptr.pData = malloc(nDataSize);
#endif
		memcpy(Msg.Data.Ptr.pData, pData, nDataSize);
		Msg.Data.Ptr.nSize = nDataSize;
	} else
	{
		Msg.Data.Ptr.pData = NULL;
		Msg.Data.Ptr.nSize = 0;
	}
	pMsgPool = (X_EXT_MSG_POOL_T *)(pDestThreadContext->pExtEventPool);


	XSignalEvent(&(pMsgPool->EventToThread),&(pMsgPool->MutexForPool),(XTHREAD_SAFE_ACTION_T)XAppendMsgToBuf,&Msg);
	return 0;
}

BOOL XGetExtEvent(SME_EVENT_T* pEvent)
{
	X_EXT_MSG_T NativeMsg;
	int ret=0;

	SME_THREAD_CONTEXT_T* p = XGetThreadContext();
	X_EXT_MSG_POOL_T *pMsgPool;
	if (NULL==pEvent || NULL==p || NULL==p->pExtEventPool)
		return FALSE;

	pMsgPool = (X_EXT_MSG_POOL_T*)(p->pExtEventPool);

	memset(&NativeMsg,0,sizeof(NativeMsg));
	while (TRUE)
	{
		ret = XWaitForEvent(&(pMsgPool->EventToThread), &(pMsgPool->MutexForPool), (XIS_CODITION_OK_T)XIsMsgAvailable, NULL, (XTHREAD_SAFE_ACTION_T)XGetMsgFromBuf,&NativeMsg);
			
		if (NativeMsg.nMsgID == SME_EVENT_EXIT_LOOP)
		{
			return FALSE; //Request Exit
		}
#ifdef SME_WIN32
#else
		// Built-in call back timer on Linux
		else if (SME_EVENT_TIMER == NativeMsg.nMsgID  && SME_TIMER_TYPE_CALLBACK == NativeMsg.Data.Int.nParam1)
		{
			// Invoke the call back function. 
			SME_TIMER_PROC_T pfnCallback = (SME_TIMER_PROC_T)(NativeMsg.Data.Int.nParam2);
			(*pfnCallback)(NativeMsg.pDestApp, NativeMsg.nSequenceNum);
		}
#endif
		else {
			// Translate the native message to SME event.
			memset(pEvent,0,sizeof(SME_EVENT_T));
			pEvent->nEventID = NativeMsg.nMsgID;
			pEvent->pDestApp = NativeMsg.pDestApp;
			pEvent->nSequenceNum = NativeMsg.nSequenceNum;
			pEvent->nDataFormat = NativeMsg.nDataFormat;
			pEvent->nCategory = NativeMsg.nCategory;
			pEvent->bIsConsumed = FALSE;
			memcpy(&(pEvent->Data),&(NativeMsg.Data), sizeof(union SME_EVENT_DATA_T));
		}

		//printf("External message received. \n");

		return TRUE;
	}; // while (TRUE)
}

BOOL XDelExtEvent(SME_EVENT_T *pEvent)
{
	if (0==pEvent)
		return FALSE;

	if (pEvent->nDataFormat == SME_EVENT_DATA_FORMAT_PTR)
	{
		if (pEvent->Data.Ptr.pData)
		{
#if SME_CPP
			delete pEvent->Data.Ptr.pData;
#else
			free(pEvent->Data.Ptr.pData);
#endif
			pEvent->Data.Ptr.pData=NULL;
		}
	}
	return TRUE;
}

