/*-----------------------------------------------------------------------------------------------------
 * Copyright 2007, Texas Instruments Incorporated
 *
 * This program has been modified from its original operation by Texas Instruments
 * to do the following:
 *       Replaced calls to XPost... with calls to Sme. This was done in order to
 *          refrain from using the original sme_ext_event API.
 *       Added an API to set the Timer thread stack.
 *       Added an API to get the time left for a timer in msec.
 *       UnOverride SetTimer and killTimer
 *       Remove support for multi threading
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

// Platform.cpp
// The cross-platform operation system functions 

#include "sme.h"
#include "sme_cross_platform.h"
#include "sme_ext_event.h"

#define NO_TIMER_SUPPORT
#define NO_THREAD_SUPPORT
#ifdef NO_THREAD_SUPPORT
    #define GET_THREAD_ID getpid()
#else /* NO_THREAD_SUPPORT */
    #define GET_THREAD_ID pthread_self()
#endif /* NO_THREAD_SUPPORT */


#ifdef XSetTimer 
    #undef XSetTimer 
#endif
#ifdef XKillTimer 
    #undef XKillTimer 
#endif

#ifdef SME_WIN32
	#if !defined(_MT)
	#error "_beginthreadex requires a multithreaded C run-time library."
	#endif
#endif

#ifndef NO_THREAD_SUPPORT
static int ThreadStackSize = 0;
#endif

int XSetThreadStackSize(int size)
{
#ifdef NO_THREAD_SUPPORT
    return 0;
#else /* NO_THREAD_SUPPORT */
    int prevSize = ThreadStackSize;

    ThreadStackSize = size;

    return prevSize;
#endif /* NO_THREAD_SUPPORT */
}

int XCreateThread(XTHREAD_PROC_T start_routine, void *arg, XTHREADHANDLE *new_thread_handle)
{
#ifdef NO_THREAD_SUPPORT
    return 0;
#else /* NO_THREAD_SUPPORT */

#if defined SME_LINUX 
	pthread_attr_t attr;
#endif

    XTHREADHANDLE  thr_handle = 0;

    int   ret_code = 0; 

    if (start_routine == NULL) {
		return -1;
    }

	memset(&thr_handle, 0, sizeof(XTHREADHANDLE));

#if defined SME_LINUX 
    pthread_attr_init(&attr);
    if (ThreadStackSize)
    {
        if (pthread_attr_setstacksize(&attr, ThreadStackSize) != 0) 
            return -1;
    }

	// When a thread is created detached (PTHREAD_CREATE_DETACHED), its thread ID and other resources can be reused as soon as 
	// the thread terminates.
	// When a thread is created nondetached (PTHREAD_CREATE_JOINABLE), it is assumed that you will be waiting for it. 
	// That is, it is assumed that you will be executing a pthread_join() on the thread. 
	// Whether a thread is created detached or nondetached, the process does not exit until all threads have exited.
    if (pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED) != 0)
		return -1;
    
    if ((ret_code = pthread_create(&thr_handle, &attr, start_routine, arg)) == 0) 
	{ 
        if (new_thread_handle != NULL)
            *new_thread_handle = thr_handle;

        return 0;
    }
    else {
        return ret_code;
    }

#elif defined SME_WIN32
	/*
	Use of the Win32 APIs CreateThread() and EndThread() directly is incompatible with the C Runtime (CRT) Library. 
	In particular, use of CreateThread() and EndThread() directly can cause loss of memory that the CRT allocates. 
	Instead, we must use either _beginthread() and _endthread() or _beginthreadex() and _endthreadex().

	The advantage of _beginthreadex() compared to _beginthread() is that _beginthreadex() provides more control 
	and is safer. For example, with _beginthreadex(), you can:

		* use security information
		* create the thread suspended initially
		* get the thread identifier of the thread
		* use the thread handle after the thread completes, allowing use of the the synchronization APIs (as above) for the thread

	The advantage of _endthreadex() compared to _endthread() is that _endthreadex() is more flexible. 

	uintptr_t _beginthreadex( 
	   void *security,
	   unsigned stack_size,
	   unsigned ( *start_address )( void * ),
	   void *arglist,
	   unsigned initflag, 
	   unsigned *thrdaddr 
	);

	initflag
		Initial state of a new thread (0 for running or CREATE_SUSPENDED for suspended); use ResumeThread to execute the thread.
	thrdaddr
		Points to a 32-bit variable that receives the thread identifier. Might be NULL, in which case it is not used.
	*/
    thr_handle = (HANDLE)_beginthreadex(NULL, 0, start_routine, arg, 0, NULL);
    if (0 != (int)thr_handle) 
	{
        if (new_thread_handle != NULL)
            *new_thread_handle = thr_handle;
        return 0;
    }else 
	{
		ret_code = errno; 
        return ret_code;
    }
#endif 
#endif /* NO_THREAD_SUPPORT */
}

int XSetThreadPriority(XTHREADHANDLE thread_handle, int nPriority)
{
#ifdef NO_THREAD_SUPPORT
    return 0;
#else /* NO_THREAD_SUPPORT */
#ifdef SME_WIN32
	/*
	nPriority 
	[in] Priority value for the thread. This parameter can be one of the following values. Priority Meaning 
	THREAD_PRIORITY_ABOVE_NORMAL Priority 1 point above the priority class. 
	THREAD_PRIORITY_BELOW_NORMAL Priority 1 point below the priority class. 
	THREAD_PRIORITY_HIGHEST Priority 2 points above the priority class. 
	THREAD_PRIORITY_IDLE Base priority of 1 for IDLE_PRIORITY_CLASS, BELOW_NORMAL_PRIORITY_CLASS, NORMAL_PRIORITY_CLASS, ABOVE_NORMAL_PRIORITY_CLASS, or HIGH_PRIORITY_CLASS processes, and a base priority of 16 for REALTIME_PRIORITY_CLASS processes. 
	THREAD_PRIORITY_LOWEST Priority 2 points below the priority class. 
	THREAD_PRIORITY_NORMAL Normal priority for the priority class. 
	THREAD_PRIORITY_TIME_CRITICAL Base priority of 15 for IDLE_PRIORITY_CLASS, BELOW_NORMAL_PRIORITY_CLASS, NORMAL_PRIORITY_CLASS, ABOVE_NORMAL_PRIORITY_CLASS, or HIGH_PRIORITY_CLASS processes, and a base priority of 31 for REALTIME_PRIORITY_CLASS processes. 

	This parameter can also be -7, -6, -5, -4, -3, 3, 4, 5, or 6. For more information, see Scheduling Priorities. 
	*/

	return SetThreadPriority(thread_handle, nPriority)? 0: GetLastError();

#else // SME_LINUX
	struct sched_param    param;
	int                   policy = SCHED_OTHER;
	/*
	Valid settings for policy include:

	SCHED_FIFO  
		Threads are scheduled in a first-in-first-out order within each priority.
	SCHED_OTHER  
		Scheduling behavior is determined by the operating system.
	SCHED_RR  
		Threads are scheduled in a round-robin fashion within each priority.

	The specified scheduling policy will only be used if the scheduling parameter inheritance attribute is PTHREAD_EXPLICIT_SCHED. 
	*/

	memset(&param, 0, sizeof(param));
	param.sched_priority = nPriority;

	return pthread_setschedparam(thread_handle, policy, &param);
#endif // SME_LINUX
#endif /* NO_THREAD_SUPPORT */
}

#ifndef NO_THREAD_SUPPORT
int XGetThreadPriority(XTHREADHANDLE thread_handle, int *pPriority)
{
#ifdef SME_WIN32
	if (NULL==pPriority)
		return -1;

	*pPriority = GetThreadPriority(thread_handle);
	return (THREAD_PRIORITY_ERROR_RETURN!=*pPriority)?0:GetLastError();
#else //SME_LINUX
	struct sched_param   param;
	int                  rc;
	int                  nPolicy = SCHED_OTHER;

	if (NULL==pPriority)
		return -1;

	rc = pthread_getschedparam(thread_handle, &nPolicy, &param);
	*pPriority = param.sched_priority;
	return rc;
#endif
}
	
void XEndThread()
{
#if defined SME_LINUX 
    pthread_exit((void*)0);

#elif defined SME_WIN32
    _endthreadex(0);

#endif 
}

#ifdef SME_WIN32
void XCloseHandle(XTHREADHANDLE thread_handle)
{
	CloseHandle(thread_handle);
}
#endif
#endif /* NO_THREAD_SUPPORT */

XTHREADID XGetCurrentThreadId()
{
#ifdef NO_THREAD_SUPPORT
    return 0;
#else /* NO_THREAD_SUPPORT */
#if defined SME_LINUX 
    return pthread_self();

#elif defined SME_WIN32
    return GetCurrentThreadId();

#endif 
#endif /* NO_THREAD_SUPPORT */
}


BOOL XIsThreadRunning(XTHREADHANDLE thread_handle)
{
#ifdef NO_THREAD_SUPPORT
    return FALSE;
#else /* NO_THREAD_SUPPORT */
#if defined SME_LINUX 
    if (pthread_kill(thread_handle, 0) == 0)
#elif defined SME_WIN32
		if (GetThreadPriority(thread_handle) != THREAD_PRIORITY_ERROR_RETURN)
#endif 
        return TRUE;
    else
        return FALSE;
#endif /* NO_THREAD_SUPPORT */
}

int XWaitForThread(XTHREADHANDLE ThreadHandle)
{
#ifdef NO_THREAD_SUPPORT
    return 0;
#else /* NO_THREAD_SUPPORT */
#if defined SME_LINUX 
	do{
		XSleep(100);
	} while (XIsThreadRunning(ThreadHandle));
	return 0;
#elif defined SME_WIN32
	return WaitForSingleObject(ThreadHandle,INFINITE);
#endif 
#endif /* NO_THREAD_SUPPORT */
}

/////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef NO_THREAD_SUPPORT
BOOL XCreateProcess(const char* pProgramPath,int* ppid)
{
#ifdef SME_WIN32
  //## begin OSRelatedSpawnProcess%42660B6200A9.body preserve=yes
	BOOL bRet;
	STARTUPINFO	sinfo;
	PROCESS_INFORMATION pinfo;
	char CurDir[MAX_PATH];
	memset(&sinfo,0,sizeof(sinfo));
	memset(&pinfo,0,sizeof(pinfo));

	GetStartupInfo(&sinfo);

	strcpy(CurDir, pProgramPath);
	PathRemoveFileSpec(CurDir); // Get the file path.
	//*(strrchr(CurDir, '\\'))=0;
	bRet = CreateProcess(pProgramPath, NULL, NULL, NULL, FALSE, CREATE_NO_WINDOW,
		NULL, CurDir,  &sinfo, &pinfo);
	if(bRet && ppid){
		*ppid=(int)(pinfo.dwProcessId);
	}
	return bRet?TRUE:FALSE;
#else
	int pid=fork();
	switch((int)pid)
	{
	case 0:
		// Child process
		{
			//char *dir_buf=new char[strlen(pProgramPath)+1];
			//strcpy(dir_buf, pProgramPath);
			//chdir(dirname(dir_buf)); //???
			execl(pProgramPath, pProgramPath, NULL);
			_exit(-1);
		}
		break;
	case -1:
		// Error
		break;
	default:
		// Parent process
		break;
	}
	if(ppid) *ppid=(int)pid;
	return TRUE;
#endif
}

void XKillProcess(int pid)
{
#ifdef SME_WIN32
	HANDLE hProc; 
	if (pid==-1)
		return;
	hProc = OpenProcess(PROCESS_ALL_ACCESS, 0, pid);
	if(hProc)
	{
		TerminateProcess(hProc, 0);
		CloseHandle(hProc);
	}
#else
	if (pid==-1)
		return;
	kill(pid, SIGKILL);
#endif
}

BOOL XIsProcessRunning(int pid)
{
#ifdef SME_WIN32
	HANDLE hProc = OpenProcess(PROCESS_ALL_ACCESS, 0, pid);
	if(hProc)
	{
		CloseHandle(hProc);
		return TRUE;
	} else
		return FALSE;
#else
	int nRet = kill(pid, 0);
	if (nRet==0)
		return TRUE;
	else 
		return FALSE;
#endif
}
#endif /* NO_THREAD_SUPPORT */

/****************************************************************************************************
 Time and Clock 
*****************************************************************************************************/
#ifndef NO_THREAD_SUPPORT
void XSleep(unsigned int milliseconds)
{
#ifdef SME_WIN32
	Sleep(milliseconds);
#else
	usleep(milliseconds * 1000);
#endif
}
#endif /* NO_THREAD_SUPPORT */

int XGetTick(void)
{
#ifdef NO_THREAD_SUPPORT
    return 0;
#else /* NO_THREAD_SUPPORT */
#ifdef SME_WIN32
  return (int)GetTickCount();
#else
  struct timeval val;
  gettimeofday(&val, NULL);
  return (val.tv_sec *1000 + val.tv_usec / 1000);
#endif
#endif /* NO_THREAD_SUPPORT */
}

char* XGetTimeStr(time_t nTime, char *szBuf, int nLen, const char* szFmt)
{
	const struct tm *pTime =localtime(&nTime);
	char s_sTime[256];
	memset(s_sTime,0,sizeof(s_sTime));

	if (nTime!=0)
		strftime(s_sTime, sizeof(s_sTime), szFmt, pTime);
	
	// Return an blank string if nTime is 0.
	if (szBuf && nLen>0)
	{
		strncpy(szBuf,s_sTime, nLen);
		szBuf[nLen-1] = 0;
	}

	return szBuf;
}


char* XGetCurrentTimeStr(char *szBuf, int nLen, const char* szFmt)
{
	time_t nTime=0;
	const struct tm *pTime; 
	char s_sTime[256];
	time(&nTime);
	pTime =localtime(&nTime);
	memset(s_sTime,0,sizeof(s_sTime));
	strftime(s_sTime, sizeof(s_sTime), szFmt, pTime);

	if (szBuf && nLen>0)
	{
		strncpy(szBuf,s_sTime, nLen);
		szBuf[nLen-1] = 0;
	}

	return szBuf;
}

#define STR_FMT_ELAPSED_TIME "%M:%S"

char* XGetElapsedTimeStr(time_t nTime, char* szBuf, int nLen)
{
	// If the default time zone is n. If nTime is 0, the time with the format "%H:%M:%S" will be "n:0:0"
	const struct tm *pTime =localtime(&nTime);
	char s_sTime[256];
	memset(s_sTime,0,sizeof(s_sTime));
	strftime(s_sTime, sizeof(s_sTime), STR_FMT_ELAPSED_TIME, pTime);

	if (szBuf && nLen>0)
	{
		strncpy(szBuf,s_sTime, nLen);
		szBuf[nLen-1] = 0;
	}

	return szBuf;
}

///////////////////////////////////////////////////////////////////////
// Mutex
///////////////////////////////////////////////////////////////////////
#ifndef NO_THREAD_SUPPORT
int XCreateMutex(XMUTEX  *mutex_ptr)
{
    int  ret_code = 0; 

    if (mutex_ptr == NULL) {
        return -1;
    }

    // the mutex can be used to sync. threads in this process only.
#if defined SME_LINUX
	// A mutex has two possible states: unlocked (not owned by  any  thread),
    // and  locked  (owned  by one thread). A mutex can never be owned by two
    // different threads simultaneously. A thread attempting to lock a  mutex
    // that is already locked by another thread is suspended until the owning
    // thread unlocks the mutex first.
    if ((ret_code = pthread_mutex_init(mutex_ptr,NULL)) == 0) 
#elif defined SME_WIN32
	// The state of a mutex object is signaled when it is not owned by any thread.
	// The creating thread can use the bInitialOwner flag to request immediate ownership of the mutex. 
	// Otherwise, a thread must use one of the wait functions to request ownership. 
	// When the mutex's state is signaled, one waiting thread is granted ownership, the mutex's state changes to nonsignaled, 
	// and the wait function returns. Only one thread can own a mutex at any given time. 
	// The owning thread uses the ReleaseMutex function to release its ownership.	

	// bInitialOwner: [in] If this value is TRUE and the caller created the mutex, the calling thread obtains initial ownership of the mutex object. Otherwise, the calling thread does not obtain ownership of the mutex. To determine if the caller created the mutex, see the Return Values section.
    *mutex_ptr = CreateMutex(NULL, /*bInitialOwner*/FALSE, NULL);
    if ( *mutex_ptr != NULL)
#endif
        return 0;
    else {
        return -1;
    }
}

int XMutexLock(XMUTEX *mutex_ptr)
{
    int  ret_code = 0; 

    if (mutex_ptr == NULL) {
        return -1;
    }

#if defined SME_LINUX 
    if ((ret_code = pthread_mutex_lock(mutex_ptr)) == 0) 
#elif defined SME_WIN32
    if (WaitForSingleObject(*mutex_ptr, INFINITE)!= WAIT_FAILED)
#endif
        return 0; // the mutex's state changes to nonsignaled
    else {
        return -1;
    }
}

int XMutexUnlock(XMUTEX *mutex_ptr)
{
	int  ret_code = 0; 
    
    if (mutex_ptr == NULL){
        return -1;
    }

#if defined SME_LINUX 
    if ((ret_code = pthread_mutex_unlock(mutex_ptr)) == 0) 
#elif defined SME_WIN32
	// Releases ownership of the specified mutex object.
    if (ReleaseMutex(*mutex_ptr) != 0)
#endif
        return 0; // Release the ownership. The mutex's state changes to signaled. 
    else {
        return -1;
    }
}

int XMutexDestroy(XMUTEX  *mutex_ptr)
{
    int  ret_code = 0; 

    if (mutex_ptr == NULL) {
        return -1;
    }

    // the mutex can be used to sync. threads in this process only.
#if defined SME_LINUX 
    if ((ret_code = pthread_mutex_destroy(mutex_ptr)) == 0)
#elif defined SME_WIN32
    if (CloseHandle(*mutex_ptr) != 0)
#endif
        return 0;
    else {
        return -1;
    }
}
#endif /* NO_THREAD_SUPPORT */

///////////////////////////////////////////////////////////////////////////////////////////////////
// Event
///////////////////////////////////////////////////////////////////////////////////////////////////
#ifndef NO_THREAD_SUPPORT
int XCreateEvent(XEVENT *pEvent)
{
	if (pEvent==NULL)
		return -1;
#ifdef SME_WIN32
	*pEvent = GetCurrentThreadId();
	return 0;
#else
	return pthread_cond_init(pEvent, NULL);
#endif
}

int XDestroyEvent(XEVENT *pEvent)
{
	if (pEvent==NULL)
		return -1;

#ifdef SME_WIN32

	return 0;
#else
	return pthread_cond_destroy(pEvent);
#endif
}

// Wait for an event signaled and then take some thread-safe actions.
int XWaitForEvent(XEVENT *pEvent, XMUTEX *pMutex, XIS_CODITION_OK_T pIsConditionOK, void *pCondParam,
				  XTHREAD_SAFE_ACTION_T pAction, void *pActionParam)
{
#ifdef SME_WIN32

	DWORD nRet=WAIT_OBJECT_0;
	MSG WinMsg;

	if (pEvent==NULL || pMutex==NULL || pIsConditionOK==NULL)
		return -1;

	// GetMessage()
	// If the function retrieves a message other than WM_QUIT, the return value is nonzero.
	// If the function retrieves the WM_QUIT message, the return value is zero. 
	while (GetMessage(&WinMsg, NULL, 0, 0))
	{
		if (WinMsg.message == WM_EXT_EVENT_ID)
		{
			// External event is triggered. App go to running state.
			if (pAction)
			{
				XMutexLock(pMutex);
				(*pAction)(pActionParam);
				XMutexUnlock(pMutex);
			}
			return  0; 
		} 
		// Handle Windows messages in MMI thread, for example audio messages.
		else //if (handle_win_msg_in_mmi_thread(&WinMsg) == FALSE)
		{
			// MSDN: DispatchMessage API
			// The DispatchMessage function dispatches a message to a window procedure.

			// MSDN: DefWindowProc API
			// The DefWindowProc function calls the default window procedure to provide default 
			// processing for any window messages that an application does not process. This 
			// function ensures that every message is processed. 

			// MSDN: WM_TIMER
			// The WM_TIMER message is posted to the installing thread's message queue 
			// when a timer expires. You can process the message by providing a WM_TIMER case 
			// in the window procedure. Otherwise, the default window procedure (DefWindowProc) 
			// will call the TimerProc callback function specified in the call to the SetTimer 
			// function used to install the timer. 

			// Handle Windows messages by the default window procedure in MMI thread  
			// for example calling the TimerProc callback function for timer messages.
			DispatchMessage(&WinMsg);
		};
	}

	return 0;
#else

	int rc=0;  

	if (pEvent==NULL || pMutex==NULL || pIsConditionOK==NULL)
		return -1;
	// pthread_cond_wait() atomically unlocks the mutex (as per pthread_unlock_mutex) and waits for the condition variable cond to be  signaled.  The  thread
    //   execution is suspended and does not consume any CPU time until the condition variable is signaled. The mutex must be locked by the calling thread on
    //   entrance to pthread_cond_wait.  Before returning to the calling thread, pthread_cond_wait re-acquires mutex (as per pthread_lock_mutex).

	pthread_mutex_lock(pMutex);

	if (!(*pIsConditionOK)(pCondParam))
	{
		rc =  pthread_cond_wait(pEvent, pMutex);
	}

	if (0 == rc)
	{
		if (pAction)
			(*pAction)(pActionParam);
	}

	pthread_mutex_unlock(pMutex);

	if (rc == ETIMEDOUT)
		return XWAIT_TIMEOUT; // Actually XWAIT_TIMEOUT is ETIMEDOUT
	else
		return rc;
#endif
}

// Take some thread-safe actions before signal the event.
int XSignalEvent(XEVENT *pEvent, XMUTEX *pMutex, XTHREAD_SAFE_ACTION_T pAction, void *pActionParam)
{
	int ret=0; 
	if (pEvent==NULL || pMutex==NULL)
		return -1;
#ifdef SME_WIN32

	if (pAction)
	{
		XMutexLock(pMutex);
		(*pAction)(pActionParam); // At this time, (*pIsConditionOK)() should return TRUE.
		XMutexUnlock(
			pMutex);
	}

	if (0==*pEvent) return -1;

	PostThreadMessage(*pEvent, WM_EXT_EVENT_ID, 0, 0);
	return 0;
#else

	if (pMutex==NULL)
		return -1;

	pthread_mutex_lock(pMutex);
	// Modifications on the shared resources, which are protected by the mutex, meet the condition and should signal the if needed.
	if (pAction)
		(*pAction)(pActionParam);

	// pthread_cond_signal restarts one of the threads that are waiting on the condition variable condition.
	// pthread_cond_broadcast restarts all of the threads that are waiting on the condition variable condition.
	ret = pthread_cond_broadcast(pEvent);
	pthread_mutex_unlock(pMutex);

	return ret;

#endif
}
#endif /* NO_THREAD_SUPPORT */

///////////////////////////////////////////////////////////////////////////////////////////////////
// Thread Local Storage.
#ifdef SME_WIN32
	static unsigned long g_dwTlsIndex=0xFFFFFFFF;
#else
	typedef struct TLS_DATA_T_TAG
	{
		pthread_t nPThreadID;
		SME_THREAD_CONTEXT_PT pThreadContext;
	} TLS_DATA_T;

	#define SME_MAX_TLS_NUM  10
	static TLS_DATA_T g_TlsData[SME_MAX_TLS_NUM];
#endif


int XTlsAlloc()
{ 
#ifdef SME_WIN32
	g_dwTlsIndex = TlsAlloc();
#else
	memset(g_TlsData,0,sizeof(g_TlsData));
#endif
	return 0;
}

/* Allocate thread local storage resource. */
BOOL XSetThreadContext(SME_THREAD_CONTEXT_PT p)
{
#ifdef SME_WIN32
	if (NULL==p)
		return FALSE;

	if (0xFFFFFFFF != g_dwTlsIndex)
		return TlsSetValue(g_dwTlsIndex, p);
	else
		return FALSE;
#else
	/* A version of get_thread_area() first appeared in Linux 2.5.32.
	struct user_desc u_info;
	memset(&u_info,0,sizeof(u_info));
	u_info.entry_number =1;
	u_info.base_addr = (void*)p;
	return (0==set_thread_area (&u_info));
	*/
	int i=0;
	if (NULL==p)
		return FALSE;
	for (i=0; i<SME_MAX_TLS_NUM; i++)
	{
		if (NULL==g_TlsData[i].pThreadContext)
		{
			g_TlsData[i].nPThreadID = GET_THREAD_ID;
			g_TlsData[i].pThreadContext = p;
			return TRUE;
		}
	}
	return FALSE;
#endif
}

SME_THREAD_CONTEXT_PT XGetThreadContext()
{
#ifdef SME_WIN32
	if (0xFFFFFFFF != g_dwTlsIndex)
		return (SME_THREAD_CONTEXT_T*)TlsGetValue(g_dwTlsIndex);
	else
		return NULL;
#else
	/* A version of get_thread_area() first appeared in Linux 2.5.32.
	struct user_desc u_info;
	int nRet;
	memset(&u_info,0,sizeof(u_info));
	nRet = get_thread_info(&u_info);
	if (0==nRet)
		return u_info.base_addr;
	else
		return NULL;
	*/
	int i=0;
	pthread_t nSelf = GET_THREAD_ID;
	for (i=0; i<SME_MAX_TLS_NUM; i++)
	{
		if (nSelf==g_TlsData[i].nPThreadID)
		{
			return g_TlsData[i].pThreadContext;
		}
	}
	return NULL;
#endif
}

/* Free thread local storage resource. */
BOOL XFreeThreadContext(SME_THREAD_CONTEXT_PT p)
{
#ifdef SME_WIN32
	return FALSE;
#else
	int i=0;
	for (i=0; i<SME_MAX_TLS_NUM; i++)
	{
		if (p==g_TlsData[i].pThreadContext)
		{
			g_TlsData[i].pThreadContext=NULL;
			return TRUE;
		}
	}
	return FALSE;
#endif
}
///////////////////////////////////////////////////////////////////////////////////////////////////
/******************************************************************************************
*  Built-in Timer
******************************************************************************************/
#ifndef NO_TIMER_SUPPORT
typedef struct tagXTIMER_T
{
	unsigned int nTimerID;
	unsigned int nTimeOut; /* SME_IS_STATE_BUILT_IN_TIMEOUT_VAL can check whether a state timer or not. */
	int nLeft;
	unsigned long nSequenceNum;
	SME_TIMER_PROC_T pfnTimerFunc;
	SME_APP_T *pDestApp;
	SME_THREAD_CONTEXT_T *pDestThread;
	struct tagXTIMER_T *pNext;
} XTIMER_T;

static XTIMER_T *g_pTimerList=NULL;
static unsigned long g_nTimerSeqNum =1;
static XMUTEX g_TimerMutex;
static BOOL g_bInitedTimer = FALSE;


#ifdef SME_WIN32

// The running thread of this function is as same as the running thread of the XSetTimer() namely application thread.
void CALLBACK WinTimerProc(HWND hwnd, UINT uMsg, UINT_PTR nTimerID, DWORD dwTime)
{
	XTIMER_T *pTimerData = g_pTimerList;

	//printf("WinTimerProc\n");

	while (pTimerData)
	{
		if (pTimerData->nTimerID == nTimerID)
		{
			if (pTimerData->pfnTimerFunc)
			{
				// Invoke the call back function.
				(*(pTimerData->pfnTimerFunc))(pTimerData->pDestApp, pTimerData->nSequenceNum);
			} else
			{
				// Create an external timer event and post it .
				SmePostThreadExtIntEvent(pTimerData->pDestThread, 
					SME_IS_STATE_BUILT_IN_TIMEOUT_VAL(pTimerData->nTimeOut) ? SME_EVENT_STATE_TIMER : SME_EVENT_TIMER, 
					pTimerData->nSequenceNum,
					SME_GET_STATE_BUILT_IN_TIMEOUT_VAL(pTimerData->nTimeOut),
					pTimerData->pDestApp,
					pTimerData->nSequenceNum,
					SME_EVENT_CAT_OTHER
					);
			}
			return;
		}
		pTimerData = pTimerData->pNext;
	}
}
	
#else // SME_WIN32

#define LINUX_BASIC_TIMER_INT   100 // 0.1 sec.

static volatile BOOL g_bExitPluseThread = FALSE;
static XTHREADHANDLE g_PlusThreadHandle;

/* Signal Handling in Linux
 If one signal is generated, one signal is delivered, so any single signal will only be delivered to a single thread.
 If it is an asynchronous signal, it could go to any of the threads that haven't masked out that signal using sigprocmask(). 
 This makes life even more complicated. 
 
For instance, suppose your signal handler must access a global variable. This is normally handled quite happily by using mutex, 
as follows:

void signal_handler( int sig )
{
        ...
        pthread_mutex_lock( &mutex1 );
        ...
        pthread_mutex_unlock( &mutex1 );
        ...
}

Looks fine at first sight. However, what if the thread that was interrupted by the signal had just itself locked mutex1? 
The signal_handler() function will block, and will wait for the mutex to be unlocked. And the thread that is currently holding 
the mutex will not restart, and so will not be able to release the mutex until the signal handler exits. A nice deadly embrace.

So a common way of handling asynchronous signals in a multi-threaded program is to mask signals in all the threads, and then 
create a separate thread (or threads) whose sole purpose is to catch signals and handle them. The signal-handler thread catches 
signals by calling the function sigwait() with details of the signals it wishes to wait for. 
*/

// The running thread of this function is NOT as same as the running thread of the XSetTimer().
//static void LinuxTimerProc(int sig) // For Linux ALARM signal solution.
static void LinuxTimerProc()
{

	XTIMER_T *pTimerData = g_pTimerList;

	//if (SIGALRM!=sig)
	//	return;

	while (pTimerData)
	{
		if (pTimerData->nLeft <= LINUX_BASIC_TIMER_INT)
		{
			// Time out
			// printf("TimeOut\n");
			if (pTimerData->pfnTimerFunc)
			{
#if 0
				SmePostThreadExtIntEvent(pTimerData->pDestThread, 
					SME_TIMER_TYPE_CALLBACK, 
					(int)(pTimerData->pfnTimerFunc), 
					pTimerData->pDestApp,
					pTimerData->nSequenceNum,
					SME_EVENT_CAT_OTHER);
#endif
                (*(pTimerData->pfnTimerFunc))(pTimerData->pDestApp, pTimerData->nSequenceNum);

			} else
			{
				// Create an external timer event and post it .
				// printf("Post TimeOut event\n");
				SmePostThreadExtIntEvent(pTimerData->pDestThread, 
					SME_IS_STATE_BUILT_IN_TIMEOUT_VAL(pTimerData->nTimeOut) ? SME_EVENT_STATE_TIMER : SME_EVENT_TIMER, 
					SME_TIMER_TYPE_EVENT,
					0, 
					pTimerData->pDestApp,
					pTimerData->nSequenceNum,
					SME_EVENT_CAT_OTHER);
			}
			pTimerData->nLeft = SME_GET_STATE_BUILT_IN_TIMEOUT_VAL(pTimerData->nTimeOut); // Restart timer
			//return;
		} else
			pTimerData->nLeft -= LINUX_BASIC_TIMER_INT;

		pTimerData = pTimerData->pNext;
	}

	return;
}

static void* LinuxPulseProc(void *Param)
{
	SME_UNUSED_VOIDP_PARAM(Param);
	while(TRUE)
	{
		if (g_bExitPluseThread)
			break;
		LinuxTimerProc();
		XSleep(LINUX_BASIC_TIMER_INT);
	}
	g_bInitedTimer = FALSE;
	return NULL;
}


//static int LinuxInitTimer(unsigned int nBasicTimeOut) //milli-seconds
static int LinuxInitTimer() 
{
	/* Linux Timer: ALARM signal solution:
	The potential issue is that: 

	When the timer expires (timeout event), the timer signal handler (LinuxTimerProc) calls XPostThreadExtIntEvent --> XSignalEvent --> pthread_cond_broadcast.
	This last is a problem. 
	It is not safe to use the pthread_cond_signal() function in a signal handler that is invoked asynchronously. Even if it were safe, 
	there would still be a race between the test of the Boolean pthread_cond_wait() that could not be efficiently eliminated.

	#include <sys/time.h>
	int getitimer(int which, struct itimerval *value);
	int setitimer(int which, const struct itimerval *value,
				  struct itimerval *ovalue); //old value

	The system provides each process with three interval timers, each decrementing in a distinct time domain. 
	When any timer expires, a signal is sent to the process, and the timer (potentially) restarts.

	ITIMER_REAL
		decrements in real time, and delivers SIGALRM upon expiration. 
	ITIMER_VIRTUAL
		decrements only when the process is executing, and delivers SIGVTALRM upon expiration. 
	ITIMER_PROF
		decrements both when the process executes and when the system is executing on behalf of the process. 
		Coupled with ITIMER_VIRTUAL, this timer is usually used to profile the time spent by the application in user and kernel space. 
		SIGPROF is delivered upon expiration.

	Timer values are defined by the following structures:

		struct itimerval {
			struct timeval it_interval; // next value 
			struct timeval it_value;    // current value 
		};
		struct timeval {
			long tv_sec;                // seconds 
			long tv_usec;               // microseconds 
		};

	The function getitimer() fills the structure indicated by value with the current setting for the timer indicated by which 
	(one of ITIMER_REAL, ITIMER_VIRTUAL, or ITIMER_PROF). The element it_value is set to the amount of time remaining on the timer, 
	or zero if the timer is disabled. Similarly, it_interval is set to the reset value. The function setitimer() sets the indicated 
	timer to the value in value. If ovalue is non-zero, the old value of the timer is stored there.

	Timers decrement from it_value to zero, generate a signal, and reset to it_interval. A timer which is set to zero (it_value is 
	zero or the timer expires and it_interval is zero) stops.

	Both tv_sec and tv_usec are significant in determining the duration of a timer.

	Timers will never expire before the requested time, but may expire some (short) time afterwards, which depends on the system timer 
	resolution and on the system load. (But see BUGS below.) Upon expiration, a signal will be generated and the timer reset. 
	If the timer expires while the process is active (always true for ITIMER_VIRTUAL) the signal will be delivered immediately 
	when generated. Otherwise the delivery will be offset by a small time dependent on the system loading. 

	if it_interval is set too big,like 100 sec,real_handl() won't be invoked.because SIGALRM is send out every 100 seconds.
	if it_interval is set too small,like 1000 usec,real_handle() will be invoked too frequently!
	
	struct itimerval tv;
	int iRet = -1;
	tv.it_interval.tv_sec = (int)(nBasicTimeOut / 1000);
	tv.it_interval.tv_usec = (nBasicTimeOut % 1000) *1000;
	tv.it_value.tv_sec = (int)(nBasicTimeOut / 1000);
	tv.it_value.tv_usec = (nBasicTimeOut % 1000) *1000;
	iRet = setitimer(ITIMER_REAL, &tv, (struct itimerval *)0); 

	if (iRet!=0) return iRet; //Failed.

	struct sigaction siga;
	siga.sa_handler = &LinuxTimerProc;
	siga.sa_flags  = SA_RESTART; //????
	memset (&siga.sa_mask, 0, sizeof(sigset_t));
	iRet = sigaction (SIGALRM, &siga, NULL);

	if (0==iRet)
		g_bInitedTimer = TRUE;

	return iRet; //Failed.
	*/

	/* Linux Timer: Sleep at a separate thread solution: */

	if (g_bInitedTimer)
		return 0;

	g_bInitedTimer = TRUE;
	g_bExitPluseThread = FALSE;
	// Create a thread to trigger external events.
	return  XCreateThread(LinuxPulseProc, NULL, &g_PlusThreadHandle);

}

#endif // SME_LINUX

// Initialize timer at the specific thread. The timer signal handler will be called at this calling thread.
int XInitTimer()
{
	XCreateMutex(&g_TimerMutex);

#ifdef SME_WIN32
	g_bInitedTimer = TRUE;
	return 0;
#else
	return LinuxInitTimer();
#endif
}

// Stop the pluse thread. 
int XDestroyTimer()
{
	if (FALSE==g_bInitedTimer)
		return -1;

#ifdef SME_WIN32
	g_bInitedTimer = FALSE;
#else
	g_bExitPluseThread = TRUE;
	XWaitForThread(g_PlusThreadHandle);
	g_bInitedTimer = FALSE;
#endif

	return 0;
}

unsigned int XSetTimer(SME_APP_T *pDestApp, unsigned int nTimeOut, SME_TIMER_PROC_T pfnTimerFunc)
{
	XTIMER_T *pTimerData;
	unsigned long nSeqNum = g_nTimerSeqNum++;
#ifdef SME_WIN32
	UINT_PTR nTimerID;
#endif

	if (!g_bInitedTimer)
		return 0;

#ifdef SME_WIN32
	nTimerID = SetTimer(NULL, 0, SME_GET_STATE_BUILT_IN_TIMEOUT_VAL(nTimeOut), (TIMERPROC)WinTimerProc); 
	if (nTimerID==0)
		return 0;

    pTimerData = (XTIMER_T*)malloc(sizeof(XTIMER_T));
	if (!pTimerData)
		return 0;
	pTimerData->nTimerID = nTimerID;
	pTimerData->nLeft = 0; // For Linux only
		
#else //SME_LINUX
    pTimerData = (XTIMER_T*)malloc(sizeof(XTIMER_T));
	if (!pTimerData)
		return 0;
	pTimerData->nTimerID = nSeqNum;
	pTimerData->nLeft = SME_GET_STATE_BUILT_IN_TIMEOUT_VAL(nTimeOut); // For Linux only
#endif

	pTimerData->pDestApp = pDestApp;
	pTimerData->pDestThread = XGetThreadContext(); /* The time-out event destination thread is the current calling thread. */
	pTimerData->nTimeOut = nTimeOut; /* SME_IS_STATE_BUILT_IN_TIMEOUT_VAL can check whether a state timer or not. */
	pTimerData->nSequenceNum = nSeqNum;
	pTimerData->pfnTimerFunc = pfnTimerFunc;

	XMutexLock(&g_TimerMutex);
	pTimerData->pNext = g_pTimerList;
	g_pTimerList = pTimerData;
	XMutexUnlock(&g_TimerMutex);

	return nSeqNum;
}

unsigned int XGetTimerRemain(unsigned int nSequenceNum)
{
    XTIMER_T *pTimerData;
    int nLeft = -1;

    XMutexLock(&g_TimerMutex);

    pTimerData = g_pTimerList;
    
    while(pTimerData != NULL)
    {
        if(pTimerData->nSequenceNum == nSequenceNum)
        {
            nLeft = pTimerData->nLeft;
            break;
        }
        pTimerData = pTimerData->pNext;
    }

    XMutexUnlock(&g_TimerMutex);
    return nLeft;
}



unsigned int XSetEventTimer(SME_APP_T *pDestApp, unsigned  int nTimeOut)
{
	return XSetTimer(pDestApp,nTimeOut,NULL);
}

BOOL XKillTimer(unsigned int nSequenceNum)
{
	BOOL bRet=FALSE;

	XTIMER_T *pTimerData = g_pTimerList;
	XTIMER_T *pPre = NULL;

	XMutexLock(&g_TimerMutex);

	while (pTimerData)
	{
		if (pTimerData->nSequenceNum == nSequenceNum)
		{
#ifdef SME_WIN32
			bRet = KillTimer(NULL, pTimerData->nTimerID);
#endif
			// Remove the timer entity from the timer list.
			if (pTimerData == g_pTimerList)
			{
				g_pTimerList =g_pTimerList->pNext;
			} else
			{
				pPre->pNext = pTimerData->pNext;
			}

			free(pTimerData);
			goto Last;
		}
		pPre = pTimerData;
		pTimerData = pTimerData->pNext;
	}

Last:	XMutexUnlock(&g_TimerMutex);

    return bRet;	
}
#endif /* NO_TIMER_SUPPORT */

void* XEmptyMemAlloc(int nSize)
{
	void *p=malloc(nSize); 
	if (p) memset(p,0,nSize);
	return p;
}

void XMemFree(void* p)
{
	free(p);
}
