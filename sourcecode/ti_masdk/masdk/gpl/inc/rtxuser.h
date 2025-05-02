/*
 * File Name: rtxuser.h
 *
 * Description: MXP types definitions.
 *
 * Copyright (C) 2008 Texas Instruments, Incorporated
 */

#ifndef __RTX_USER_H__
#define __RTX_USER_H__

/*
 * Get MX_Result definition.
 */
#include "rtxerr.h"
#include "signal.h"

#define MXP_UEVT_SUPPORT 0
#undef MXP_UEVT_SUPPORT

/********************************************************************
			Array containing the Revision number of MXP
*********************************************************************/
extern char* mx_version;

/*
 * DATA PURPOSE:        Resource naming
 *
 * DESCRIPTION:         Type definition for a character sequence name
 *                      for a resource. There is a fixed maximum length.
 */
#define MX_resourceNameLength   (8)

typedef char MX_ResourceName[MX_resourceNameLength];

typedef unsigned int * MX_NativePointer;

/*******************************************************************************
 * DATA PURPOSE:        Define types for all system defined identifiers.
 *
 * DESCRIPTION:         Includes typedefs for all system defined identifiers.
 ******************************************************************************/
#ifdef GG_LINUX

typedef int           MX_TaskId;
typedef int           MX_TimerId;
typedef int           MX_QueueId;
typedef int           MX_SqueueId;
typedef int           MX_SemaphoreId;
typedef int           MX_SegmentId;
typedef int           MX_PartitionId;

#else

#ifdef MXP_VOID_PTR_TYPES
    typedef void*           MX_TaskId;
    typedef void*           MX_TimerId;
    typedef void*           MX_QueueId;
    typedef void*           MX_SqueueId;
    typedef void*           MX_SemaphoreId;
    typedef void*           MX_SegmentId;
    typedef void*           MX_PartitionId;
#else
    typedef struct X_TCB*   MX_TaskId;
    typedef struct X_TmrCB* MX_TimerId;
    typedef struct X_QCB*   MX_QueueId;
    typedef struct X_SQCB*  MX_SqueueId;
    typedef struct X_SCB*   MX_SemaphoreId;
    typedef struct X_SegCB* MX_SegmentId;
    typedef struct X_PCB*   MX_PartitionId;
#endif /* MXP_VOID_PTR_TYPES */

#endif /* GG_LINUX */

/*
 * Miscellaneous definitions
 */
typedef unsigned long   MX_TimeInterval;
typedef short           MX_TaskPriority;
typedef int             MX_StackSize;
typedef unsigned int    MX_QueueDepth;
typedef unsigned long   MX_EventFlagSet;
typedef void*           MX_DataPointer;
typedef unsigned int    MX_DataSize;
typedef unsigned int    MX_BlockCount;
typedef unsigned int    MX_BlockSize;
typedef MX_DataPointer  MX_MessagePointer;
typedef int             MX_TokenCount;
typedef MX_ResourceName MX_SemaphoreName;
typedef MX_ResourceName MX_SegmentName;
typedef MX_ResourceName MX_PartitionName;
typedef void			(* MX_SwapFunc ) (void *, unsigned int);
typedef	void *			MX_SwapData;
typedef unsigned int	MX_NativeData;

typedef unsigned int    CriticalSectionContext;
void X_beginCriticalSection(CriticalSectionContext* contextPtr);
void X_endCriticalSection(CriticalSectionContext context);

/* Description of the contents of the Crash Area */
typedef struct Crash
{
	MX_ErrorCode		ErrorCode;  	/*Error code passed as a paramter*/
	MX_TaskId			CurrentTask;	/*Task thta called XfatalError */
	MX_NativePointer	PCValue;		/*Contents of PC when XfatalError was
										called */
	MX_NativePointer 	SPValue;		/* Contents of SP at the time of call*/
	MX_NativeData		SP_0_Contents;	/* contents of sp[0] */
	MX_NativeData		SP_1_Contents;	/* contents of sp[1] */
	MX_NativeData		SP_2_Contents;	/* contents of sp[2] */
	MX_NativeData		SP_3_Contents;	/* contents of sp[3] */
	MX_NativeData		SP_4_Contents;	/* contents of sp[4] */
} MX_Crash;

/* MX Spy Defines */

/* User must supply the Spy Table that is customized   */
/* to their Application.  See the example in minimum.c */

typedef int                 MX_SpyKey_t;

typedef enum MX_SpyLevel_enum
{
  SPY_GEN_INFO = 0,     /* superfluous information */
  SPY_FNENTER,          /* entering function */
  SPY_EVENT,            /* normal and expected event(i.e. link up) */
  SPY_MINOR_ERR,    /* minor unexpected event */
  SPY_MAJOR_ERR,    /* major unexpected event */
  SPY_FATAL_ERR,    /* fatal error */
  SPY_LEVEL_OFF,        /* Level to turn off Spy Information */
  SPY_EVENT_INIT        /* special event reserved for initialization prints 
                           should be printed regardless of regular Spy severity settings
                           to disable prints use SPY_LEVEL_OFF (to turn off all prints)
                           sent to logger as INFO */
} MX_SpyLevel_t;

#if defined(_WINDOWS) || defined(_WIN32)
typedef enum MX_SpyDest_enum
{
	NULL_DEST = 0,          /* Null Destination */
	MX_DEBUGGER_OUT,        /* Application or System Debugger (OutputDebugString) */
	SPY_WINDOW,             /* Windowed Dev Env. Only: Spy Display Window */
	COMMAND_DISPLAY_WINDOW, /* Windowed Dev Env. Only: Debug Command Display window */
	PROFILE_WINDOW,         /* Windowed Dev Env. Only: Profiler Display window */
	PC_COM1,                /* PC Commport 1 */
	PC_COM2,                /* PC Commport 2 */
	PC_COM3,                /* PC Commport 3 */
	PC_COM4,                /* PC Commport 4 */

	MSVC_WINDOW = MX_DEBUGGER_OUT,			/* alias for back compat. */
	MX_STDERR   = SPY_WINDOW,				/* Console Dev Env. Only: stderr */
	MX_STDIO    = COMMAND_DISPLAY_WINDOW,	/* Console Dev Env. Only: stdout & stdin */
} MX_SpyDest_t;

#if defined(_WINDOWS)
#define SPY_DEFAULT_DEST		SPY_WINDOW
#define SPY_DEFAULT_CMD_DEST	COMMAND_DISPLAY_WINDOW
#else
#define SPY_DEFAULT_DEST		MX_STDIO
#define SPY_DEFAULT_CMD_DEST	MX_STDIO
#endif

#else

typedef int MX_SpyDest_t;
#define SPY_DEFAULT_DEST		0
#define SPY_DEFAULT_CMD_DEST	0

#endif

typedef struct MXSpyTable_s
{
  char *                  keyName;
  MX_SpyKey_t             keyNumber;
  MX_SpyLevel_t   filterLevel;
  MX_SpyDest_t    destination;
} MXSpyTable_t;

/* Structure to be used in filling up the user debug command table */
typedef struct MX_DebugCommandTable_s
{
	char * commandStringPtr;
	void ( *commandFunction )( int argc, char **argv );
	char * helpTextPtr;
} MX_DebugCommandTable_t;

typedef struct MX_FunctionProfileTbl_s
{
	char *name;
	unsigned long count;
} MX_FunctionProfileTbl_t;

/* the following defines can be used for timeout parameters */
#define MX_INDEFINITE   (unsigned long) ((long)-1)      /* wait indefinitely */
#define MX_NO_BLOCK             0       /* do not wait, return immediately */

/*
** the "level" parameter is obsolete, new code should simply pass 0
** these are defined for back compatability only
** MX_DeferTaskSwitch is an int so that there are no "lint"-ish warnings
** about passing a constant where an enum is expected
*/
typedef enum MX_ObsoleteDeferTaskSwitch_e {
	MX_noTaskSwitchDefer = 0,
	MX_deferTaskSwitch = 0
} MX_ObsoleteDeferTaskSwitch;

typedef int MX_DeferTaskSwitch;
#define MX_TASK         MX_noTaskSwitchDefer
#define MX_INTERRUPT    MX_deferTaskSwitch


/* the following defines can be used for the "condition" parameter when
 * checking system event flags
 */
typedef enum MX_EventCondition_e {
  MX_anySpecifiedEvents = 0,  /* need only one of the specified events */
  MX_allSpecifiedEvents = 1 /* must have all of the specified events */
} MX_EventCondition;

/*
 *      The following two names are defined as aliases for the specification
 *      of what conditions to trigger an event match. This is for purposes
 *      of backward compatability.
 */
#define MX_AND_COND     MX_allSpecifiedEvents
#define MX_OR_COND      MX_anySpecifiedEvents

#define MX_ALL_EVENTS	(~(MX_EventFlagSet)0)

/* the following defines can be used for the "private" parameter when
 * creating memory partitions
 */
typedef enum MX_Protection_e {
  MX_publicAccess = 0,    /* may be accessed by others */
  MX_privateAccess = 1    /* private */
} MX_Protection;

/*
 *      The following name is defined as an alias for the specification
 *      of what protection a partition is to be given. This is for purposes
 *      of backward compatability.
 */
#define MX_PUBLIC               MX_publicAccess
#define MX_PRIVATE      MX_privateAccess

/* the state returned by the XtmrInquiry call can be interpreted as follows */
typedef enum MX_TimerState_e {
  MX_timerUnassigned = 0,	/* timer is unassigned */
  MX_timerIdle = 1,       /* timer is allocated but idle */
  MX_timerActive = 2,     /* timer is activated(on active list) */
  MX_timerFired = 4       /* timer fired, currently processing */
} MX_TimerState;

/*
 *      The following three names are defined as aliases for the specification
 *      of timer state. This is for purposes of backward compatability.
 */
#define TIMER_UNASSGN	MX_timerUnassigned
#define TIMER_IDLE      MX_timerIdle
#define TIMER_ACTIVE    MX_timerActive
#define TIMER_FIRED     MX_timerFired

/*
 *      The following structure is used to pass a list of timers to
 *      XtmrStartSync and XtmrAbortSync to start off a list of
 *      timers synchronously and have them reloaded at the gievn periods
 *
 */
typedef struct X_SyncTmrList_s
{
  MX_TimerId              tmrId;
  MX_TimeInterval time;
  MX_TimeInterval reloadPeriod;
} MX_SyncTmrList;

/*
 * DATA PURPOSE:        Task Name
 *
 * DESCRIPTION:         A task name is used explicitly to refer to a task
 *                      when the task is being created or when an identifier
 *                      is not known for the task.
 */
typedef MX_ResourceName MX_TaskName;

/*
 * DATA PURPOSE:        Task Argument
 *
 * DESCRIPTION:         When a task first starts executing, the code which
 *                      it runs is typically called as a function. This
 *                      function may take one actual parameter, or argument.
 */
typedef void*           MX_TaskArgument;

/*
 * DATA PURPOSE:        Task Entry
 *
 * DESCRIPTION:         When a task first starts executing, the code which
 *                      it runs is typically called as a function. This
 *                      function must be specified as a pointer to a function.
 *                      because this function is expected to never return,
 *                      it is assumed to return void. It may take one parameter
 *                      which is defined as the argument to the function.
 */
typedef void            (*MX_TaskEntry)( MX_TaskArgument );


/*******************************************************************************
 * DATA PURPOSE:        Define Task Attribute Object.
 *
 * DESCRIPTION:         This structure is used in creating system managed tasks
 ******************************************************************************/
typedef struct taskAttr
{
    MX_ResourceName     name;           /* user defined task name */
    MX_TaskPriority     priority;       /* user assigned task priority */
    MX_TimeInterval     timeSlice;      /* user assigned time slice interval */
    MX_NativePointer    stackAddr;      /* Address of the stack for the task */
    MX_StackSize        stackSize;      /* user defined stack size for task */
    MX_SwapFunc         SwapIn;         /* Pointer to the function called when
                                           a new task is being swapped in */
    MX_SwapFunc         SwapOut;        /* Pointer to the function called at
                                           time of swap-out of current task */
    MX_SwapData         SwapData;       /* Pointer to data required at
                                           swapping  time */
} MX_TaskAttr;




/*
 * DATA PURPOSE:        Queue Name
 *
 * DESCRIPTION:         A queue name is used explicitly to refer to a queue
 *                      when the queue is being created or when an identifier
 *                      is not known for the queue.
 */
typedef MX_ResourceName MX_QueueName;

/* Specify Task Switch mode */
typedef enum MX_QueueMode_e
{
    MX_NoSwitch = 0,
    MX_Switch = 1
} MX_QueueMode;

#define     MX_NOSWITCH     MX_NoSwitch
#define     MX_SWITCH       MX_Switch


/*******************************************************************************
 * DATA PURPOSE:Define System Queue Attribute Object.
 *
 * DESCRIPTION: This structure is used in creating system managed queues.
 ******************************************************************************/
typedef struct sysQAttr
{
    MX_QueueName        name;           /* user defined queue name */
    MX_QueueDepth       depth;          /* maximum depth of the FIFO queue */
    MX_TaskId           taskId;         /* identifier of synchronized task */
    MX_EventFlagSet     evFlag;         /* post this event(s) to synched task */
    MX_QueueMode	mode;		/* task synchronization - yes/no */
} MX_QueueAttr;

/*******************************************************************************
 * DATA PURPOSE:Define Sorted Queue Attribute Object.
 *
 * DESCRIPTION: This structure is used in creating sorted queues.
******************************************************************************/
typedef MX_ResourceName MX_SqueueName;

typedef int (* MX_SqCompareFunc) (void *msg1, void *msg2);

typedef struct sysSQAttr
{
    MX_SqueueName       name;           /* user defined Sorted queue name */
    MX_QueueDepth       depth;          /* maximum depth of the Sorted queue */
    MX_TaskId           taskId;         /* identifier of synchronized task */
    MX_EventFlagSet     evFlag;         /* post this event(s) to synched task */
    MX_QueueMode	mode;		/* Mode specifying task synchronization - needed not needed */
    MX_SqCompareFunc	compare;	/* Function to compare priorities of two messages */
} MX_SqueueAttr;

/* thread function types */
typedef void MX_ThreadInit_t(MX_TaskArgument arg);
typedef void MX_ThreadEvent_t(MX_TaskArgument arg, MX_EventFlagSet events);
typedef void MX_ThreadSleepDone_t(MX_TaskArgument arg);


/* following are the prototypes defining the API for the
 * TNIx kernel
 */

/* task management */
extern MX_Result XtIdentify( MX_TaskName name, MX_TaskId *taskId );
extern MX_Result XtCreate( MX_TaskAttr* attr,
                            MX_TaskEntry entryFunc,
                            MX_TaskArgument entryArg,
                            MX_TaskId *taskId );
extern MX_Result XtChangeTimeSlice( MX_TimeInterval ticks );
extern MX_Result XtGetTimeSlice( MX_TaskId taskId, MX_TimeInterval* ticks );
extern MX_Result XtChangePriority( MX_TaskPriority priority );
extern MX_Result XtChangePriorityEx( MX_TaskId taskId, MX_TaskPriority priority );
extern MX_Result XtGetPriority( MX_TaskId taskId, MX_TaskPriority* priority );
extern MX_Result XtResume( MX_TaskId taskId, MX_DeferTaskSwitch level );
extern MX_Result XtSleep( MX_TaskId taskId, MX_TimeInterval time );
extern MX_Result XtSleepQuick(void);
extern MX_Result XtDelete( MX_TaskId taskId );
extern MX_Result XtLock( MX_TaskId taskId );
extern MX_Result XtUnlock( MX_TaskId taskId );
extern MX_Result XtgetName ( MX_TaskId taskId, MX_DataPointer name);

/* Signal mechanism */
extern MX_Result XsigPost( MX_TaskId taskId, MX_DeferTaskSwitch level );
extern MX_Result XsigWait (void);


/* SIGTERM mechanism */
#ifdef GG_LINUX
typedef  void (*MX_SigTermCB)(int signum, siginfo_t* siginfo, void* context);
extern MX_Result XSigTermRegister(MX_SigTermCB);
#endif
/* end SIGTERM mechanism */

/* queue management */
extern MX_Result XqIdentify( MX_QueueName name, MX_QueueId* queueId );
extern MX_Result XqCreate( MX_QueueAttr* attr, MX_QueueId* queueId );
extern MX_Result XqWait( MX_QueueId queueId,
                          MX_TimeInterval timeout,
                          MX_MessagePointer* msg );
extern MX_Result XqJam( MX_QueueId queueId,
                        MX_MessagePointer msg,
                        MX_DeferTaskSwitch level );
extern MX_Result XqPost( MX_QueueId queueId,
                          MX_MessagePointer msg,
                          MX_DeferTaskSwitch level );
extern MX_Result XqDelete( MX_QueueId queueId );
extern MX_Result XqDeleteAll( void );
extern MX_Result XqInquiry( MX_QueueId queueId, MX_QueueDepth* count );
extern MX_Result XqgetName( MX_QueueId queueId, MX_DataPointer name);
extern MX_Result XqPeek(MX_QueueId queueId,
                        MX_MessagePointer* msg );

/* Sorted Queue Management */
extern MX_Result XsqIdentify( MX_SqueueName name, MX_SqueueId* squeueId );
extern MX_Result XsqCreate( MX_SqueueAttr* attr, MX_SqueueId* squeueId );
extern MX_Result XsqWait( MX_SqueueId squeueId,
                          MX_TimeInterval timeout,
                          MX_MessagePointer* msg );
extern MX_Result XsqPeek( MX_SqueueId squeueId,
                          MX_MessagePointer* msg);
extern MX_Result XsqPost( MX_SqueueId squeueId,
                          MX_MessagePointer msg,
                          MX_DeferTaskSwitch level );
extern MX_Result XsqDelete( MX_SqueueId squeueId );
extern MX_Result XsqInquiry( MX_SqueueId squeueId, MX_QueueDepth* count );
extern MX_Result XsqgetName( MX_SqueueId squeueId, MX_DataPointer name);


/* timer management */
extern MX_Result XtmrCreate( MX_EventFlagSet evFlag,
                            MX_QueueId queueId,
                            MX_MessagePointer msg,
                            MX_TimerId* tmrId );
extern MX_Result XtmrStart( MX_TimerId tmrId, MX_TimeInterval time,
                            MX_TimeInterval reloadPeriod );
extern MX_Result XtmrAbort( MX_TimerId tmrId );
extern MX_Result XtmrDelete( MX_TimerId tmrId );
extern void XtmrTick( void );
extern MX_Result XtmrInquiry( MX_TimerId tmrId, MX_TimerState* state );
extern MX_Result XgetTicks( MX_TimeInterval *ticks );
extern MX_Result XsetTicks( MX_TimeInterval ticks );
extern MX_Result XgetNtpTime( unsigned int * p_seconds, unsigned int * p_frac_seconds );
extern MX_Result XtmrStartSync ( MX_SyncTmrList *list);
extern MX_Result XtmrAbortSync ( MX_SyncTmrList *list);


/* event management */
extern MX_Result XevWait( MX_EventFlagSet events,
                          MX_EventCondition condition,
                          MX_TimeInterval timeout,
                          MX_EventFlagSet* eventsPending );
extern MX_Result XevPost( MX_TaskId taskId,
                          MX_EventFlagSet events,
                          MX_DeferTaskSwitch level );
extern MX_Result XevClear( MX_TaskId taskId, MX_EventFlagSet events );
extern MX_Result XevInquiry( MX_TaskId taskId, MX_EventFlagSet* eventFlags );

/* semaphore management */
extern MX_Result XsIdentify( MX_SemaphoreName name, MX_SemaphoreId* semId );
extern MX_Result XsCreate( MX_SemaphoreName name,
                            MX_TokenCount count,
                            MX_SemaphoreId* semId );
extern MX_Result XsWait( MX_SemaphoreId semId, MX_TimeInterval timeout );
extern MX_Result XsPost( MX_SemaphoreId semId, MX_DeferTaskSwitch level );
extern MX_Result XsDelete( MX_SemaphoreId semId );
extern MX_Result XsInquiry( MX_SemaphoreId semId,
                            MX_TokenCount* tokens,
                            MX_TokenCount* tokensAvail );
extern MX_Result XsgetName( MX_SemaphoreId semId, MX_DataPointer name);

/* memory management */
extern MX_Result XsegInit(void);
extern MX_Result XsegIdentify( MX_SegmentName name, MX_SegmentId* segId );
extern MX_Result XsegCreate( MX_SegmentName name,
                            MX_DataPointer address,
                            MX_DataSize length,
                            MX_SegmentId* segId );
extern MX_Result XpIdentify( MX_PartitionName name,
                            MX_SegmentId segId, MX_PartitionId* partId );
extern MX_Result XpCreate( MX_PartitionName name,
                            MX_BlockCount blocks,
                            MX_BlockSize blkSize,
                            MX_SegmentId segId,
                            MX_Protection rprivate,
                            MX_TaskId taskId,
                            MX_PartitionId* partId );
extern MX_Result XpBalloc( MX_PartitionId partId, MX_DataPointer* block );
extern MX_Result XpBfree( MX_PartitionId partId, MX_DataPointer block );
extern MX_Result XpInquiry( MX_PartitionId partId, MX_BlockCount* count );
extern MX_Result XpSizeInquiry( MX_PartitionId partId, MX_BlockSize* size );
extern MX_Result XpgetName( MX_SegmentId segId,
                            MX_PartitionId partId,
                            MX_DataPointer name );
extern MX_Result XseggetName( MX_SegmentId segId, MX_DataPointer name);

/* system fatal error */
extern MX_Result XfatalError( MX_ErrorCode error );

extern void XiEnter( void );
extern void XiExit( void );
extern void XcExit( void );
extern void Xinit(void);

/* thread APIs */
extern MX_Result XthCreate(
                    const MX_TaskAttr*		p_attr,
                    MX_ThreadInit_t*		initFunc,
                    MX_ThreadEvent_t*		eventFunc,
                    MX_TaskArgument			initEventArg,
                    MX_TaskId*				p_threadId);
extern MX_Result XthSetEventMask(
                    MX_TaskId				threadId,
                    MX_EventFlagSet			events,
                    MX_EventCondition		condition);
extern MX_Result XthSleep(
                      MX_TaskId				threadId,
                      MX_TimeInterval			time,
                      MX_ThreadSleepDone_t*	doneFunc,
                      MX_TaskArgument         sleepDoneArg);
extern MX_Result XthRun(
            MX_TaskPriority         min_priority,
            MX_BlockCount           run_limit);
extern MX_Result XthIsSleeping(
                MX_TaskId				threadId);

/* MXP Profiler Calls */
extern void XtInitProfile(MX_FunctionProfileTbl_t *profileFnCountTable);
extern void XtStartProfile( unsigned int fn_index );
extern void XtEndProfile( void );
extern MX_Result XPrintTaskProfile ( void );
extern MX_Result XPrintFunctionProfile(void);
extern void XshowFullTaskInfo(void);

/*MXP Debug Calls */
extern MX_Result XdbgSetDest(int DbgDest );
extern int XdbgGetDest(void);

/*MXP trace call */

/****************************************************/
/*Defines the tracing events to be triggered from the user land*/
/****************************************************/
/* MXP_VPKTRCV -->                                                           

#define MXP_VPKTRCV 0*/
extern MX_Result XtraceStart(int);
extern MX_Result XtraceStop(int);

/* MXP Debug APIs - functions to be provided by the user */
void XDebugPutString( int destId, char * OutputStrPtr );
void dbgCommProcessInput( int owner );


/* Debugger/Spy Calls */
extern MX_Result Xspy(  MX_SpyKey_t SpyKey, MX_SpyLevel_t SpyLevel,
                        char *fmt, ... );
extern MX_SpyDest_t XspyGetDest( MX_SpyKey_t SpyKey );
extern MX_SpyLevel_t XspyGetLevel( MX_SpyKey_t SpyKey );
extern MX_Result XspySetDest( MX_SpyKey_t SpyKey, MX_SpyDest_t SpyDest );
extern MX_Result XspySetLevel( MX_SpyKey_t SpyKey, MX_SpyLevel_t SpyLevel );

extern int Xsprintf(char *str, char *fmt, ...);

/* API for the Oscilloscope calls in the Windows DE */
extern char **MXSpyLevelsDesc(MX_SpyLevel_t SpyLevel);
extern void MXSetXtOscope (float x, long y);
extern void MXSetXYOscope (double x, double y);
extern void MXXmOscopeInit(float scaleMaxValue );
extern void MXSetXmOscope (int index, float x, long t);

/* API for the Ladder Diagram and Node Map calls in the Windows 3.1 DE */
extern MX_Result MXLadderInit( char *queueName,
                              char *taskName,
                              char * (*displayFn)( void *),
                              void (*displayCompleteFn)( char *)
                              );

extern MX_Result MXLadderWrite( char *taskName, char *queueName, void *msgPtr );
extern void writeLadder( char *sourceTaskNamePtr,
                  char *sourceQueueNamePtr,
                        char *destQueueNamePtr,
                        void *msgPtr,
                        char *msgTextPtr );


extern MX_Result MXNodeName( int NodeNumber, char *name );
extern MX_Result MXNodeActivate( int NodeNumber );
extern MX_Result MXNodeDeactivate( int NodeNumber );
extern MX_Result MXNodeConnect( int fromNodeNumber, int toNodeNumber );
extern MX_Result MXNodeDisconnect( int fromNodeNumber, int toNodeNumber );

/* API for Interrupt Simulation functions in Windows 3.1 */
#ifdef WINDOWS3_1    
typedef void*   MX_InterruptId;
typedef char MX_InterruptName [MX_resourceNameLength] ;   
typedef void (* MX_ISRpointer)(void);

extern MX_Result XiCreate (MX_InterruptName name, 
			MX_ISRpointer ISRoutine,                                                          
            MX_TimeInterval schedule_period);
#endif

extern MX_Result XdebugGetCmdBuffer( unsigned char **cmdBufferPtr );
extern MX_BlockSize XdebugGetCmdBufferLength( void );
extern MX_Result XdebugReturnCmdBuffer( unsigned char *cmdBufferPtr );
extern MX_Result XdebugPutCmd( unsigned char * cmdStringPtr,
								MX_DeferTaskSwitch level );
extern MX_Result XdebugPutSpyMsg( unsigned char * spyMsgPtr,
								MX_DeferTaskSwitch level );
extern MX_Result XdbgPutS (char *strPtr);
extern MX_Result XdbgPrintf(const char* fmt, ...);

/* API for XdbgPutS output setting */
extern void XdbgSetPutSFunction(void (*puts_function)(char* string));
/* API for Xspy output setting */
extern void XdbgSetSpyFunction(void (*spy_function)(char* string));

/* API for registering exit handler function 
typedef void (*MX_Exit_Function) (void* arg);
extern void XsetExitFunction(MX_Exit_Function function);*/
 
#ifdef MXP_OLD_TYPES
    typedef long                    MX_SpyArg;
    typedef MX_SpyArg               MX_SpyArg_t;
    typedef MX_TaskId               TASKID_t;
    typedef MX_QueueId              QID_t;
    typedef MX_SqueueId             SQID_t;
    typedef MX_SemaphoreId          SEMID_t;
    typedef MX_SegmentId            SEGID_t;
    typedef MX_PartitionId          PARTID_t;
    typedef MX_TimerId              TMRID_t;
    typedef MX_FunctionProfileTbl_t MX_FunctionProfileTbl;
    typedef MX_TaskAttr             taskAttr_t;
    typedef MX_QueueAttr            sysQAttr_t;
    typedef MX_SqueueAttr           sysSQAttr_t;
    typedef char*                   MX_SpyCtrlString;
    typedef MX_SpyKey_t             MX_SpyKey;
    typedef MX_SpyCtrlString        MX_SpyCtrlString_t;
#endif

typedef int MX_DBG_SPY_HOOK_FUNC_T
(
    MX_SpyKey_t     SpyKey,         /* Spy Key passed by Xspy caller */
    MX_SpyLevel_t   SpyLevel,       /* Spy level passed by Xspy caller */
    const char*     msg             /* Message passed by Xspy caller w/ 
                                    ** % sequences expanded.  The message
                                    ** is null terminated.  The message does not
                                    ** include timestamp, end of lines
                                    ** characters, or any other additions made 
                                    ** by the Xspy function itself */
);

MX_DBG_SPY_HOOK_FUNC_T* XspyHookInstall(MX_DBG_SPY_HOOK_FUNC_T* hook_func);

/* types and api for user events utility*/ 
#ifdef MXP_UEVT_SUPPORT 
typedef unsigned int    MX_UEventId_t;
typedef unsigned int    MX_UEventFlags_t;
typedef unsigned int    MX_UEventClientId_t;

typedef MX_Result (*MX_UEventRecieveFn_t)(MX_UEventClientId_t senderId,
                                        char *senderName,
                                        MX_UEventId_t eventId,
                                        char *eventName,
                                        int eventDataLen,
                                        void *eventData);
typedef char* (*MX_UEventFormatFn_t)(MX_UEventId_t eventId,
                                   char *eventName,
                                   int eventDataLen,
                                   void *eventData);
typedef struct MX_UEventClientIf_s
{
    MX_UEventRecieveFn_t eventRecieve;
} MX_UEventClientIf_t;


typedef struct MX_UEventEntry_s
{
    char                *eventName;
    MX_UEventFlags_t    eventFlags;
    MX_UEventFormatFn_t eventFormatFunction;
    
} MX_UEventEntry_t;

#define MX_UEVENT_MAX_CLIENT_NAME_LEN 4
#define MX_UEVENT_CLIENT_FLAG_LOOPBACK 0x00000001

typedef struct MX_UEventClientEntry_s
{
    char                clientName[MX_UEVENT_MAX_CLIENT_NAME_LEN + 1];
    MX_UEventFlags_t    clientFlags;
    MX_UEventClientIf_t clientIf;
    
} MX_UEventClientEntry_t;

typedef struct MX_UEventSubscribeEntry_s
{
    unsigned int numClients;
    unsigned char *clients; /* bit mask of subscribing clients */
} MX_UEventSubscribeEntry_t;

#define MX_UEVENT_CLIENT_NONE 0
#define MX_UEVENT_CLIENT_ANONYMOUS 0xffffffff
#define MX_UEVENT_EVENT_NONE 0
#define MX_UEVENT_EVENT_ALL 0xffffffff

extern MX_UEventClientId_t XUEvtClientRegister(char *clientName,
                                             MX_UEventFlags_t clientFlags,
                                             MX_UEventClientIf_t *clientIf);
extern MX_Result XUEvtSubscribe(MX_UEventClientId_t clientId, MX_UEventId_t eventId);
extern MX_Result XUEvtSend(MX_UEventClientId_t clientId, MX_UEventId_t eventId,
                    int eventDataLen, void *eventData);
#endif
#endif /* !defined(__RTX_USER_H__) */
