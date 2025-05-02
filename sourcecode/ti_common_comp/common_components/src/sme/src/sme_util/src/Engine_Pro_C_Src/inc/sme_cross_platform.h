/*-----------------------------------------------------------------------------------------------------
 * Copyright 2007, Texas Instruments Incorporated
 *
 * This program has been modified from its original operation by Texas Instruments
 * to do the following:
 *       Added an API to set the Timer thread stack.
 *       Override SetTimer and KillTimer
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
  -------------------
  Feature List:
  -------------------
  Process Mangement
  Thread Mangement
  Mutex
  Event Loop
  Clock
  Built-in Timer
  Thread Local Storage
 * ==============================================================================================================================*/
#ifndef _CROSS_PLATFORM_H_
#define _CROSS_PLATFORM_H_

#include <time.h>
#include <errno.h>

#include "sme.h"

#if defined SME_LINUX
	#include <string.h>
	#include <unistd.h>
	#include <pthread.h>
	#include <signal.h>
	#include <sys/time.h>
	#include <stdio.h>
	#include <stdlib.h>
	#include <linux/unistd.h>
	typedef pthread_t         XTHREADID;
	typedef pthread_t         XTHREADHANDLE;
	typedef pthread_mutex_t   XMUTEX; 
	typedef pthread_cond_t    XEVENT;

	#define XINFINITE 0xFFFFFFFF
	#define XWAIT_TIMEOUT     ETIMEDOUT
#elif defined SME_WIN32
	#include <windows.h>
	#include <winbase.h>
	#include <process.h>
	#include <shlwapi.h>
	#include <stdio.h>
	typedef DWORD             XTHREADID;
	typedef HANDLE            XTHREADHANDLE;
	typedef HANDLE            XMUTEX;
	typedef DWORD            XEVENT; // The thread ID for GetMessage()

	#define WM_EXT_EVENT_ID  0xBFFF
	
	#define XINFINITE INFINITE
	#define XWAIT_TIMEOUT     WAIT_TIMEOUT
	
	#define snprintf  _snprintf
#endif /* __SOL */

/****************************************************************/
#ifdef __cplusplus   
extern "C" {
#endif

#ifdef SME_WIN32
	typedef unsigned (__stdcall *XTHREAD_PROC_T)(void*);
	void XCloseHandle(XTHREADHANDLE thread_handle);
	#define XCLOSE_HANDLE XCloseHandle
#else
	typedef void* (*XTHREAD_PROC_T)(void*);
	#define XCLOSE_HANDLE(x) 
#endif
	
int XSetThreadStackSize(int size);
int XCreateThread(XTHREAD_PROC_T start_routine, void *arg, XTHREADHANDLE *new_thread_handle);
void XEndThread(void);
XTHREADID XGetCurrentThreadId(void);
BOOL  XIsThreadRunning(XTHREADHANDLE thread_handle);
int XWaitForThread(XTHREADHANDLE thread_handle);
int XSetThreadPriority(XTHREADHANDLE thread_handle, int nPriority);

BOOL XCreateProcess(const char* pProgramPath,int* ppid);
void XKillProcess(int pid);
BOOL XIsProcessRunning(int pid);


void XSleep(unsigned int milliseconds);
int XGetTick(void);

/* #define STR_TIME_FMT		"%m/%d/%y %H:%M:%S" 
Note: You may change order of the %m/%d/%y for the month/day/year. Do not change the flags m d y H M S.*/
char* XGetCurrentTimeStr(char *szBuf, int nLen, const char* szFmt);
char* XGetTimeStr(time_t nTime, char *szBuf, int nLen, const char* szFmt);
char* XGetElapsedTimeStr(time_t nTime, char* szBuf, int nLen);


// A  mutex  is a MUTual EXclusion device, and is useful for protecting shared data structures from concurrent modifications, 
// and implementing critical sections and monitors.
// This Mutex object can be used to synchronize threads in the process only.

// In Linux, pthread_mutex_t is type of a structure.
int  XCreateMutex(XMUTEX *mutex_ptr);
int  XMutexLock(XMUTEX *mutex_ptr);
int  XMutexUnlock(XMUTEX *mutex_ptr);
int  XDestroyMutex(XMUTEX *mutex_ptr);


// A event/condition (short for condition variable) is a synchronization device that allows threads to suspend execution and relinquish  the  processors
// until  some  predicate  on shared data is satisfied. The basic operations on conditions are: signal the condition (when the predicate becomes true),
// and wait for the condition, suspending the thread execution until another thread signals the condition.

// This Event object can be used to synchronize threads in the process only.

typedef int (*XTHREAD_SAFE_ACTION_T)(void*p);
typedef BOOL (*XIS_CODITION_OK_T)(void*p);

int XCreateEvent(XEVENT *pEvent);
int XWaitForEvent(XEVENT *pEvent, XMUTEX *pMutex, XIS_CODITION_OK_T pIsConditionOK, void *pCondParam,
				  XTHREAD_SAFE_ACTION_T pAction, void *pActionParam);
int XSignalEvent(XEVENT *pEvent, XMUTEX *pMutex, XTHREAD_SAFE_ACTION_T pAction, void *pActionParam);
int XDestroyEvent(XEVENT *pEvent);

// Thread Local Storage
int XTlsAlloc();
BOOL XSetThreadContext(SME_THREAD_CONTEXT_PT p);
SME_THREAD_CONTEXT_PT XGetThreadContext();
BOOL XFreeThreadContext(SME_THREAD_CONTEXT_PT p);


/******************************************************************************************
*  Built-in Timer
******************************************************************************************/
typedef int (*SME_TIMER_PROC_T)(SME_APP_T *pDestApp, unsigned long nSequenceNum);

// On Linux platform, call the XInitTimer function at the time-out event trigger thread. 
// On time-out, the external event trigger thread posts SME_EVENT_TIMER to the state machine application thread,
// and then invokes the callback function installed by the XSetTimer function. 
int XInitTimer();
int XDestroyTimer();

/*
SmeSetTimer or SmeSetEventTimer function creates a timer. The timer is set for every nElapse mili-seconds. SmeSetTimer setup a function to be notified when the time-out value elapses.

If the function succeeds, the return value is an sequence number identifying the new timer. If an application receive SME_EVENT_TIMER event from more than 1 timers, application can identify which timer triggers the event through sequence number.

SmeSetEventTimer posts an SME event with the timer identifier in the nSequenceNum member of SME_EVENT_T structure, when the time-out value elapses.

The return value of the function is an ID to the new timer. An application can pass this handle to the SmeKillTimer function to destroy the timer. 

SmeSetTimer or SmeSetEventTimer function returns 0 if the function fails.

NOTE: The nTimeOut parameter for regular timers should be less than 0x80000000. Other values are reserved for the state built-in timers.
*/

#define SME_STATE_BUILT_IN_TIMEOUT_VAL(_Elapse)  (0x80000000  | _Elapse)
#define SME_GET_STATE_BUILT_IN_TIMEOUT_VAL(_Data) (_Data & 0x7FFFFFFF)
#define SME_IS_STATE_BUILT_IN_TIMEOUT_VAL(_Data)  (_Data & 0x80000000)

unsigned int XSetTimer(SME_APP_T *pDestApp, unsigned int nTimeOut, SME_TIMER_PROC_T pfnTimerFunc);
unsigned int XGetTimerRemain(unsigned int nSequenceNum);
unsigned int XSetEventTimer(SME_APP_T *pDestApp, unsigned  int nTimeOut); 
BOOL XKillTimer(unsigned int nSequenceNum);

/******************************************************************************************
*  Dynamic Memory Management
******************************************************************************************/
void* XEmptyMemAlloc(int nSize);
void XMemFree(void* p);


#ifdef __cplusplus
}
#endif 

//#include "ti_sme_ext_event.h"
#define XSetTimer SME_SetTimer
#define XKillTimer SME_RemoveTimer
#define XGetTimerRemain SME_GetRemainTime


#endif /* _CROSS_PLATFORM_H_ */
