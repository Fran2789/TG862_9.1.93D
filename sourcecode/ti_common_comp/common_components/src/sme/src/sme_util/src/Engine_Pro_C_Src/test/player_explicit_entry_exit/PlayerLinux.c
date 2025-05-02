/* ==============================================================================================================================
 * This notice must be untouched at all times.
 *
 * Copyright  IntelliWizard Inc. 
 * All rights reserved.
 * LICENSE: LGPL. 
 * Redistributions of source code modifications must send back to the Intelliwizard Project and republish them. 
 * Web: http://www.intelliwizard.com
 * eMail: info@intelliwizard.com
 * We provide technical supports for UML StateWizard users.
 * ==============================================================================================================================*/
/* A state machine sample running on Linux/Win32 platforms, refer to Readme.txt.
*/


#include <stdio.h>
#include "sme_cross_platform.h"
#include "sme_ext_event.h"
#include "sme.h"
#include "sme_debug.h"

#if SME_CPP
	#include "Player.h"
#else
	#include "Player_c.h"
#endif

#include "SingleState.h"
#include "EventId.h"

#ifdef SME_WIN32
	unsigned __stdcall ConsoleProc(void *Param);
#else
	void* ConsoleProc(void *Param);
#endif

BOOL g_bQuit=FALSE;

#if SME_CPP
	SME_DEC_EXT_APP_VAR(Player1, Player)
	SME_DEC_EXT_APP_VAR(SingleState1, SingleState)
#else
	SME_DEC_EXT_APP_VAR(Player1)
	SME_DEC_EXT_APP_VAR(SingleState1)
#endif

SME_THREAD_CONTEXT_T g_AppThreadContext;

int main(int argc, char* argv[])
{
	XTHREADHANDLE ThreadHandle = 0;
	int ret;
	printf("Cross-Platform Player Applicaton. \n");


	// Install thread local storage data functions.
	XTlsAlloc();
	SmeSetTlsProc(XSetThreadContext, XGetThreadContext);
	////////////////////////////////////////////////////////////////
	// Engine initialization.
	SmeInitEngine(&g_AppThreadContext); // Initialize engine
	// Save the thread context pointer to the TLS.
	// XSetThreadContext(&g_AppThreadContext);

	// Install event handler functions.
	SmeSetExtEventOprProc(XGetExtEvent, XDelExtEvent);
	// Install memory operation functions.
	// SmeSetMemOprProc(SrvMAlloc, SrvMFree);???
	////////////////////////////////////////////////////////////////

	//SME_TURN_OFF_ALL_LOG_FIELDS(); // Test case

	XInitMsgBuf();
	////////////////////////////////////////////////////////////////
	// Create a thread to trigger external events.
	ret = XCreateThread(ConsoleProc, NULL, &ThreadHandle);

	SmeActivateApp(&SME_GET_APP_VAR(Player1),NULL);
	SmeActivateApp(&SME_GET_APP_VAR(SingleState1),&SME_GET_APP_VAR(Player1));
	SmeRun();
	printf("Exit from SmeRun() \n");
	XFreeMsgBuf();
	XFreeThreadContext(&g_AppThreadContext); /* At last, free thread local storage resource. */

	/*
	do {
		SME_EVENT_T * ret = XGetExtEvent();

		if (ret == NULL)
			printf("Timeout! \n");
		else
			printf("Message Received (id=%d,p1=%d,p2=%d)\n", ret->nEventID, 0,0);
	} while (XIsThreadRunning(ThreadHandle));
	*/
	return 0;
}

void ConsoleProcUsage()
{
	printf("Console command usage:\n");
	printf("==============================================================\n");
	printf("? - Help\n");
	printf("1 - Press Power button \n");
	printf("2 - Press Pause/Resume button \n");
	printf("x/X- Quit\n");
	printf("Others- Post a message.\n");
	printf("==============================================================\n");
}

#ifdef SME_WIN32
	unsigned __stdcall ConsoleProc(void *Param)
#else
	void* ConsoleProc(void *Param)
#endif
{
	// On Linux platform, call the XInitTimer function at the time-out event trigger thread. 
	// On time-out, the external event trigger thread posts SME_EVENT_TIMER to the state machine application thread,
	// and then invokes the callback function installed by the XSetTimer function. 
	XInitTimer();

	printf("Enter the console procedure thread.\n");
	ConsoleProcUsage();
	
	SME_TRACE_ASC_FMT(0,"Trace Test");

	while(TRUE)
	{
		int nParam1 =0;
		if(g_bQuit) break;

		//OSRelated::Sleep(ISVW_LOOP_INTERVAL);

		nParam1 = fgetc(stdin);

		switch(nParam1)
		{
			case EOF:	return 0;//Cancel ConsoleProc in Daemon

			case 'x':
			case 'X'://Quit
				XPostThreadExtIntEvent(&g_AppThreadContext, SME_EVENT_EXIT_LOOP, 0, 0, NULL,0,SME_EVENT_CAT_OTHER);
				printf("Exiting... Please wait. \n");
				return 0;
				break;
			case '?'://Help
				ConsoleProcUsage();
				break;
			case '1':
				XPostThreadExtIntEvent(&g_AppThreadContext, EXT_EVENT_ID_POWER, 0, 0, NULL,0,SME_EVENT_CAT_OTHER);
				printf("Post a message: EXT_EVENT_ID_POWER!\n");
				break;
			case '2':
				XPostThreadExtIntEvent(&g_AppThreadContext, EXT_EVENT_ID_PAUSE_RESUME, 0, 0, NULL,0,SME_EVENT_CAT_OTHER);
				printf("Post a message: EXT_EVENT_ID_PAUSE_RESUME!\n");
				break;

			case '\n':
			case '\r':
				break;
			default:
				break;
		}
	}
	XDestroyTimer();

	return 0;
}


