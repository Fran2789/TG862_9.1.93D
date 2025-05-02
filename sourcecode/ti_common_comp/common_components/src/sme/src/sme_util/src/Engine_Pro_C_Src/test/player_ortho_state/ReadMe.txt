========================================================================
       CONSOLE APPLICATION : Player_Ortho_State
========================================================================
Version: 
========================================================================

A state machine sample running on Linux/Win32 platforms

This sample demonstrates the following features:

  *) Built-in Timers running at two modes: callback function and event trigger.
  *) Player state machine application with C standard and C++ standard version depending on preprocessor definition SME_CPP.
  *) Event loop on Linux/Win32 platforms.
  *) Separate state machine definitions to different source files.
  *) Conditional Pseudo State
  *) Join Pseudo State
  *) Guard the internal transition from a time out event. 
  *) Guard an transition from PowerDown to PowerUp
  *) Activate an application, deactiate an application.
  *) Thread Local Storage
  *) Multi-thread model

The StateWizard provides a multi-threaded concurrency model for state machine applications. This model allows several groups 
of applications run at independent thread. Meanwhile, users may define more than one application instances based on 
a state machine profile.

Each application thread has an independent message pool, an event/condition and a Mutext. When a new external event post, 
set the event/condition signaled. The Mutext for a thread pool is to synchronize the message pool access between 
the event sending thread and the receiving thread.

An external message pool for an application is defined as below:

typedef struct tagEXTMSGPOOL
{
	int nMsgBufHdr;
	int nMsgBufRear;
	X_EXT_MSG_T MsgBuf[MSG_BUF_SIZE];
	XEVENT EventToThread;
	XMUTEX MutextForPool;
} X_EXT_MSG_POOL_T;

The benefits of this model is that all concurrently running/active applications follow Run_To_Completion fashion 
and developers do not need to handling complex and error-prone synchronization issues as little as possible, so as to achieve maximum state bandwidth. 

There are 3 running threads in this sample: the state machine application thread-1 for Player-1 application, 
thread-2 for Player-2 application and the external event trigger thread. Player-1 and Player-2 applications are created 
based on the Player state machine profile. They run independent. Player-1 or Player-2 has an independent control panel.

  *) Orthogonal State.

In the StateWizard, you may define a transition from/to an Orthogonal state, using SME_BEGIN_ORTHO_SUB_STATE_DEF(). 
Refer to the sample. However, you can not define an explicit entry to one of child state of a region.
And all regions work in form of applications which run concurrently. 

On orthogonal state entry, all regions will be activated automatically. The child regions may run at the parent thread or an independent thread. 
On orthogonal state exit, all regions will be de-activated automatically. The engine will deactivate the child regions which runs at the parent thread.  
and post SME_EVENT_EXIT_LOOP events to the running region threads, and wait these regions to exit their threads. 

/////////////////////////////////////////////////////////////////////////////
