/*-----------------------------------------------------------------------------------------------------
 * Copyright 2007, Texas Instruments Incorporated
 *
 * This program has been modified from its original operation by Texas Instruments
 * to do the following:
 *       Pointed SME_DEF_DBGLOG_FILE to console (and to stdout and /var/something in comment)
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
 */
/* sme_conf.h */

/*****************************************************************************************
*  The configuration for the StateWizard Engine, you could customize it for your project.
******************************************************************************************/


#ifndef SME_CONF_H
#define SME_CONF_H


/*****************************************************************************************
*  State Machine Engine Configuration
******************************************************************************************/
/* Preprocessor definitions */
#ifdef linux // This is (one of) the standard way(s) you can identify a Linux OS using gcc.
    #ifndef LINUX
        #define LINUX
    #endif
#endif

#ifdef linux
    #ifndef SME_LINUX
        #define SME_LINUX
    #endif
#elif defined(WIN32)
    #ifndef SME_WIN32
        #define SME_WIN32
    #endif
#endif


#define SME_CPP      FALSE /* FALSE for standard C edition, TRUE for standard C++ edition */
#define SME_DEBUG    TRUE  /* FALSE to turn off debugging, TURE to turn on debugging */
#define SME_UNICODE  FALSE

#define SME_MAX_STATE_NUM			65532
#define SME_MAX_STATE_TREE_DEPTH	16 /* The maximum state tree depth from the root to the leaf including the root */
#define SME_MAX_PSEUDO_TRAN_NUM	    4   /* The maximum pseudo state transition number on a state transition for the prevention of dead loop. */
#define SME_EVENT_POOL_SIZE			8   /* The size of the pool for internal events */
#define SME_MAX_APP_NAME_LEN    64
#define SME_MAX_STR_BUF_LEN		513 /* The maximum string buffer length of output debugging string. */

#define SME_REGION_NAME_FMT "%s:%d"
//#define SME_DEF_DBGLOG_FILE         "/var/sme.log"
#define SME_DEF_DBGLOG_FILE         "/dev/console"
//#define SME_DEF_DBGLOG_FILE         "/dev/stdout"
#define SME_DEF_UNICODE_DBGLOG_FILE  "SmeDbgLogU.txt"

#define SME_CONST_HANDLER_TABLE FALSE /* Preserved for history transition feature.*/
#define SME_UI_SUPPORT   FALSE

typedef unsigned int SME_UINT32;
typedef int SME_INT32;
typedef unsigned char SME_BYTE;


/*******************************************************************************************
	Message strings for debugging support for localization.
******************************************************************************************/
#define SMESTR_START_STATE_TRACKING	"Start realtime state tracking ..."
#define SMESTR_CATCH_STATES			"Catching the current active application states ... "

#define SMESTR_ERR_DEL_INVALID_MEM   "Error! Try to delete an unknown or freed memory block at %s (%d)."
#define SMESTR_ERR_MEM_OVER_RUN      "Error! The memory block is damaged, which is allocated at %s (%d)."
#define SMESTR_INFO_MEM_DUMP			"Memory dumping:"
#define SMESTR_INFO_MEM_INFO			"The memory block %X with size of %d is allocated at %s (%d)."
#define SMESTR_INFO_MEM_NUM			"In total, %d memory block(s) is(are) allocated."
#define SMESTR_TOTAL_MEM_SIZE			"Total Allocated Memory Size:%d bytes"

#define SMESTR_ERR_NO_INIT_STATE_IN_COMP	"Error. No initial child state in a composite state. "
#define SMESTR_ERR_NO_JOIN_TRAN_STATE 		"Error. No transition state in a Join pseudo state. "
#define SMESTR_ERR_A_LOOP_PSEUDO_STATE_TRAN "Error. A loop state transition among pseudo states. "
#define SMESTR_ERR_FAIL_TO_SET_TIMER		"Error. Failed to set a timer. "
#define SMESTR_ERR_FAIL_TO_EVAL_COND		"Error. Failed to evalate a destination state in the conditional pseudo state. "
#define SMESTR_ERR_DEACTIVATE_NON_LEAF_APP  "Error. Try to de-acitvate an application exisiting one of its child application is still active."
#define SMESTR_ERR							"Error!"

#define SMESTR_FIELD_APP			"APPLICATION"
#define SMESTR_FIELD_ROOT			"ROOT"
#define SMESTR_FIELD_SRC_STATE		"SRC STATE"
#define SMESTR_FIELD_DEST_STATE	"DEST STATE"
#define SMESTR_FIELD_STATE_DEPTH	"DEPTH"
#define SMESTR_FIELD_EVENT			"EVENT"
#define SMESTR_FIELD_EVENT_SEQNUM	"SEQ NUM"
#define SMESTR_FIELD_EVENT_DATA		"EVENT DATA"
#define SMESTR_FIELD_EVENT_DATA_PTR_FMT		"P(%-8p,%-4u)    "
#define SMESTR_FIELD_EVENT_DATA_INT_FMT		"I(%-4u,%-4u)        "
#define SMESTR_FIELD_TIME			"TIME(ms)"
#define SMESTR_FIELD_REASON		"REASON"

#define SMESTR_EVENT_TIMEOUT        "Timeout"
#define SMESTR_EVENT_STATE_TIMER    "StateTimer"
#define SMESTR_INTERNAL_TRAN		"Internal Tran"
#define SMESTR_APP_ACTIVATED		"Activated"
#define SMESTR_APP_DEACTIVATED		"De-activated"

#define SMESTR_HIT					"Hit"
#define SMESTR_NOT_MATCH			"Not Matched"
#define SMESTR_GUARDED				"Guard Returned FALSE"
#define SMESTR_COND				"Cond"
#define SMESTR_JOIN				"Join"

#define SMESTR_TIME_STAMP_FMT	"%H:%M:%S" 

/*******************************************************************************************/


#endif /* SME_CONF_H */
