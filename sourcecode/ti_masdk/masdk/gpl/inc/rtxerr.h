/*
 * File Name: rtxerr.h
 *
 * Description: Definitions for system error codes.
 *
 * Copyright (C) 2008 Texas Instruments, Incorporated
 */

#ifndef __RTX_ERR_H__
#define __RTX_ERR_H__

/*******************************************************************************
 * DATA PURPOSE:	Define System Error Codes
 *
 * DESCRIPTION: 	Following are the system error codes returned by the 
 *			system calls 
 ******************************************************************************/

enum MX_Result_e {
	ERR_NOERR	= 0,	/* no error occurred */
	ERR_INVNAME	= 1,	/* no match for the given name */
	ERR_TIMEOUT	= 2,	/* specified time elasped for system call */
	ERR_ASGN	= 3,	/* the given name is already assigned */

	ERR_NOTCB	= 4,	/* no available Task Control Blocks */
	ERR_TIDINV	= 5,	/* given task identifier is invalid */
	ERR_PRIINV	= 6,	/* given priority level is out of range */

	ERR_NOQCB	= 7,	/* no available Queue Control Blocks */
	ERR_QIDINV	= 8,	/* given queue identifier is invalid */
	ERR_QFULL	= 9,	/* cannot post, the target queue is full */
	ERR_QEMPTY	= 10,	/* requested queue is empty */
	ERR_QUNASGN = 11,   /* The QCB was either not assigned or deleted */

	ERR_NOEVT	= 12,	/* requested event condition not met */
	ERR_DELETED	= 13,	/* queue synched to this event was deleted */

	ERR_NOSCB	= 14,	/* no available Semaphore Control Blocks */
	ERR_SIDINV	= 15,	/* given semaphore identifier is invalid */
	ERR_NOSEM	= 16,	/* requested semaphore is unavailable */
	ERR_SUNASGN = 17,   /* The SCB was either not assigned or deleted */

	ERR_NOMEM	= 18,	/* not enough memory to satisfy the request */
	ERR_PPRIVATE= 19,	/* cannot allocate from private partition */
	ERR_NOSEG	= 20,	/* no available Segment Control Blocks */
	ERR_SEGINV	= 21,	/* given segment identifier is invalid */
	ERR_FREE	= 22,	/* attempt to free an already free block */
	ERR_INVBLK	= 23,	/* given block address is out of range */
	ERR_PARTINV  = 24,	/* given partition ID is invalid */


	ERR_NOTMR	= 25,	/* no available Timer Control Blocks */
	ERR_TMRINV	= 26,	/* given timer identifier is invalid */
	ERR_TMREXP	= 27,	/* timer expired */
	ERR_TMRIDLE	= 28,	/* attempt to abort an idle timer */
	ERR_TMRACTIVE	= 29,	/* attempt to start an active timer */
	ERR_NULLPTR	= 30,	/* NULL pointer passed as a parameter to 
									Xspy or sprintf*/

/* fatal error definitions */
	SYS_CONFIG_ERR	= 31,
	SYS_MEM_CORRUPT	= 32,
	SYS_NO_SUPPORT	= 33,
	SYS_CRITICAL_SECTION_OVERFLOW	 = 34,	/* exceeded max. nesting */
	SYS_CRITICAL_SECTION_UNDERFLOW	 = 35,	/* tried to end > started  */
	SYS_ILLEGAL_REQUEST = 36,			/* Too many timer requests */

	ERR_NOICB = 37,				/* GlobalAlloc for XiCreate failed */

	ERR_NOSQCB	= 38,	/* no available Sorted Queue Control Blocks */
	ERR_SQIDINV	= 39,	/* given sorted queue identifier is invalid */
	ERR_SQFULL	= 40,	/* cannot post, the target sorted queue is full */
	ERR_SQEMPTY	= 41,	/* requested sorted queue is empty */
	ERR_SQUNASGN    = 42,   /* The SQCB was either not assigned or deleted */
	ERR_INTR_NESTING = 43,	/* XiEnter & XiExit not balanced */
	ERR_HOST_API    = 44,   /* Host OS API Call failed unexpectedly */
	ERR_RETURN_FROM_TASK = 45,  /* Task Function Returned */
	ERR_INV_HANDLE = 46,  	    /* Invalid Handle for other objects type clk, etc */
    ERR_INV_THREAD_FUNC = 47,   /* Invalid Thread Function specified */
    ERR_BLOCK_THREAD    = 48,   /* Can't block a thread */
    ERR_NONTASK			= 49,   /* Function or mode not supported in 
								** nontask model or mode */
    ERR_PRI_LIMIT       = 50,   /* thread run hit priority limit */
    ERR_RUN_LIMIT       = 51,   /* thread run hit count limit */
    ERR_TASK_THREAD_RUN = 52,   /* thread run encounted a task */ 
    ERR_ALREADY_SLEEPING = 53,  /* thread already sleeping with
                                ** a sleep done function */
    ERR_NOT_DBG_TASK    = 54,   /* A Debug Task API was called from
                                ** another task */
    ERR_INV_SYS_CALL    = 55,   /* Invalid system call */
    ERR_SYS_CALL_IOCTL  = 56,   /* Error in ioctl system call */

/* Debug Command Enhancement Error Definitions */
    ERR_END_OF_CMD      = 57,
    ERR_NUM_ENUM				/* Marker for the last error + 1 */
};

typedef enum MX_Result_e MX_Result;

typedef MX_Result MX_ErrorCode;

#endif /* !defined(__RTX_ERR_H__) */
