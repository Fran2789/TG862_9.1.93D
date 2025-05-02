/*-----------------------------------------------------------------------------------------------------
 * Copyright 2008, Texas Instruments Incorporated
 *
 * This program has been modified from its original operation by Texas Instruments
 * to do the following:
 *      Solved compilation errors, due to missing wchar_t
 *      Added the ability to print event names, taken from Engine_Pro_C_Src_070516_EventName
 *      Reformat the output of trace lines in SmeCatchCurrentStates
 *      Add ModuleID to debug output callback pointer.
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

/* =============================================================================
 * This notice must be untouched at all times.
 *
 * Copyright  IntelliWizard Inc. 
 * All rights reserved.
 * LICENSE: LGPL
 * Redistributions of source code modifications must send back to the Intelliwizard Project and republish them. 
 * Web: http://www.intelliwizard.com
 * eMail: info@intelliwizard.com
 * We provide technical supports for UML StateWizard users. The StateWizard users do NOT have to pay for technical supports 
 * from the Intelliwizard team. We accept donation, but it is not mandatory.
 * -----------------------------------------------------------------------------
 * General description of this file:
 *
 * State machine engine debug support global header file.
 * -----------------------------------------------------------------------------
  -------------------
  Feature List:
  -------------------
  Real time trace of the engine behavior and state tracking to the StateTree in the StateWizard and output to a debug log file including 
		time stamp, application, source state, destination state, event, time to handling, reason for handling.
  Debug log data output to the VC++ Studio and output to a debug log file.
  Debug log data output depending on the switches for application modules.
  Dynamic memory leak and memory overwrite detection.
  Support ASCII or Unicode strings output in ASCII or Unicode file depending on the preprocessor SME_UNICODE in sme_confi.h.
  Tracer Filter based on the logging fields.
  Tracer Filter based on application modules. 
  Capture the current active applications and corresponding states on the current running thread.
  Explicitly make a state as a tempoaray root.
 * ===========================================================================*/

#ifndef SME_DEBUG_H
#define SME_DEBUG_H

#include "sme.h"
#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus   
extern "C" {
#endif

/******************************************************************************************
*  ASCII and Unicode string supports
*******************************************************************************************/
#if SME_UNICODE
	#define SME_CHAR	wchar_t 
#else
	#define SME_CHAR	char
#endif

/*****************************************************************************************
*  Diagnostic function prototypes.
******************************************************************************************/
#define SME_MODULE_ENGINE 0 /* The engine output some debugging data, such as state tracking. It can be turned off by SmeTurnOffModuleTracer().*/
#define SME_MODULE_ERR_OUTPUT -1 /* The engine output some running error information. It can not be turned off. */

typedef enum {
	SME_LOG_FIELD_APP=0,
	SME_LOG_FIELD_ROOT,
	SME_LOG_FIELD_SRC_STATE,
	SME_LOG_FIELD_DEST_STATE,
	SME_LOG_FIELD_STATE_DEPTH,
	SME_LOG_FIELD_EVENT,
	SME_LOG_FIELD_EVENT_SEQNUM,
	SME_LOG_FIELD_EVENT_DATA,
	SME_LOG_FIELD_HANDLING_TIME,
	SME_LOG_FIELD_REASON,
	SME_LOG_INTERNAL_TRAN, /* If turn off this field, the engine will not log internal transition. */
	SME_LOG_FIELD_NUM /* It should be less than 32. */
} SME_LOG_FIELD_E;
typedef unsigned char SME_LOG_FIELD_T;

typedef enum {
	SME_REASON_INTERNAL_TRAN=0,
	SME_REASON_ACTIVATED, // State Entry
	SME_REASON_DEACTIVATED, // State Exit
	SME_REASON_HIT,
	SME_REASON_NOT_MATCH,
	SME_REASON_GUARD,
	SME_REASON_COND,
	SME_REASON_JOIN
} SME_REASON_FOR_HANDLE_E;
typedef unsigned int SME_REASON_FOR_HANDLE_T;

typedef void (*SME_OUTPUT_DEBUG_ASCII_STR_PROC_T)(int nModuleID, const char * sDebugStr); 
typedef void (*SME_OUTPUT_DEBUG_UNICODE_STR_PROC_T)(const wchar_t * sDebugStr); 
SME_OUTPUT_DEBUG_ASCII_STR_PROC_T	SmeSetOutputDebugAsciiStrProc(SME_OUTPUT_DEBUG_ASCII_STR_PROC_T fnProc);
SME_OUTPUT_DEBUG_UNICODE_STR_PROC_T	SmeSetOutputDebugUnicodeStrProc(SME_OUTPUT_DEBUG_UNICODE_STR_PROC_T fnProc);

void SmeUnicodeTrace(int nModuleID, const wchar_t * wsFormat, ...);
void SmeUnicodeTraceAscFmt(int nModuleID, const char * sFormat, ...);
void SmeAsciiTrace(int nModuleID, const char * sFormat, ...);
void SmeConvAsciiStr(SME_CHAR* sOutput, int nLen, const char * sFormat, ...);
void SmeTurnOnModuleTracer(int nModuleID);
void SmeTurnOffModuleTracer(int nModuleID);
void SmeTurnOnAllModuleTracers();
void SmeTurnOffAllModuleTracers();
void SmeTurnOnLogField(int nField);
void SmeTurnOffLogField(int nField);
void SmeTurnOnAllLogFields();
void SmeTurnOffAllLogFields();
void SmeEnableStateTracking(BOOL bStateTree, BOOL bOutputWin);
BOOL SmeStateTrack(SME_EVENT_T *pEvent, SME_APP_T *pDestApp, SME_STATE_T* pOldState, SME_REASON_FOR_HANDLE_T nReason, int nTime);
BOOL SmeCatchCurrentStates(BOOL bWithStatePath);
BOOL SmeMakeTempRoot(SME_APP_T *pApp, SME_STATE_T *pTempRoot);

typedef const char* (*SME_GET_EVENT_NAME_PROC_T)(SME_EVENT_ID_T nEventID);
SME_GET_EVENT_NAME_PROC_T SmeInstallGetEventNameProc(SME_GET_EVENT_NAME_PROC_T pfnProc);
	
#if SME_DEBUG
	#define SME_TURN_ON_MODULE_TRACER(nModuleID) SmeTurnOnModuleTracer(nModuleID)
	#define SME_TURN_OFF_MODULE_TRACER(nModuleID) SmeTurnOffModuleTracer(nModuleID)
	#define SME_TURN_ON_ALL_MODULE_TRACERS() SmeTurnOnAllModuleTracers()
	#define SME_TURN_OFF_ALL_MODULE_TRACERS() SmeTurnOffAllModuleTracers()

	#define SME_TURN_ON_LOG_FIELD(nField) SmeTurnOnLogField(nField)
	#define SME_TURN_OFF_LOG_FIELD(nField) SmeTurnOffLogField(nField)
	#define SME_TURN_ON_ALL_LOG_FIELDS() SmeTurnOnAllLogFields()
	#define SME_TURN_OFF_ALL_LOG_FIELDS() SmeTurnOffAllLogFields()

	#define SME_STATE_TRACK  SmeStateTrack
	#define SME_ASCII_TRACE  SmeAsciiTrace
	#define SME_CATCH_STATES  SmeCatchCurrentStates
	#define SME_MAKE_TEMP_ROOT(pApp,pTempRoot) SmeMakeTempRoot(pApp,pTempRoot)


	#define SME_ASSERT(BoolExpress)       do { \
		if (!(BoolExpress)) {\
		    SME_ASCII_TRACE(SME_MODULE_ERR_OUTPUT,"Assertion fails at %s (%d).\n", __FILE__, __LINE__); while(1); \
		}\
	} while(0)

	#define SME_ASSERT_MSG(BoolExpress, Msg)       do { \
		if (!(BoolExpress)) {\
			SME_ASCII_TRACE(SME_MODULE_ERR_OUTPUT,"%s\nAssertion fails at %s (%d).\n", Msg, __FILE__, __LINE__); while(1); \
		}\
	} while(0)

	#if SME_UNICODE
		#define SME_TRACE  SmeUnicodeTrace
		#define SME_TRACE_ASC_FMT SmeUnicodeTraceAscFmt
		#define SME_SET_TRACER(fnTracer)	SmeSetOutputDebugUnicodeStrProc(fnTracer)
	#else
		#define SME_TRACE  SmeAsciiTrace
		#define SME_TRACE_ASC_FMT SmeAsciiTrace
		#define SME_SET_TRACER(fnTracer)	    SmeSetOutputDebugAsciiStrProc(fnTracer)
	#endif

	#define SME_INSTALL_GET_EVENT_NAME_PROC(fnProc) SmeInstallGetEventNameProc(fnProc)

#else /* !SME_DEBUG */

	/* The number of function parameters is unknown.*/
	#define SME_STATE_TRACK        1 ? (void)0 : SmeStateTrack
	#define SME_ASCII_TRACE		   1 ? (void)0 : SmeAsciiTrace

	#if SME_UNICODE
		#define SME_TRACE              1 ? (void)0 : SmeUnicodeTrace
		#define SME_TRACE_ASC_FMT      1 ? (void)0 : SmeUnicodeTraceAscFmt
	#else
		#define SME_TRACE              1 ? (void)0 : SmeAsciiTrace
		#define SME_TRACE_ASC_FMT      1 ? (void)0 : SmeAsciiTrace
	#endif

	#define SME_ASSERT(BoolExpress)
	#define SME_ASSERT_MSG(BoolExpress, Msg)
	#define SME_SET_TRACER(fnTracer)
	#define SME_TURN_ON_MODULE_TRACER(nModuleID)
	#define SME_TURN_OFF_MODULE_TRACER(nModuleID)
	#define SME_TURN_ON_ALL_MODULE_TRACERS
	#define SME_TURN_OFF_ALL_MODULE_TRACERS

	#define SME_TURN_ON_LOG_FIELD(nField) 
	#define SME_TURN_OFF_LOG_FIELD(nField) 
	#define SME_TURN_ON_ALL_LOG_FIELDS() 
	#define SME_TURN_OFF_ALL_LOG_FIELDS() 

	#define SME_CATCH_STATES()
	#define SME_MAKE_TEMP_ROOT(pApp,pTempRoot) 
	
	#define SME_INSTALL_GET_EVENT_NAME_PROC(fnProc) 

#endif  /* _DEBUG */

#define SME_DECLARE_TEMP_ROOT(_temp_root,_comp_state) \
	SME_BEGIN_ROOT_COMP_STATE_DEF(_temp_root, SME_NULL_ACTION, SME_NULL_ACTION) \
	SME_ON_INIT_STATE(SME_NULL_ACTION, _comp_state) \
	SME_END_STATE_DEF \
	SME_BEGIN_SUB_STATE_DEF_P(_comp_state) \
	SME_END_STATE_DEF

/******************************************************************************************
*  Dynamic memory operation function prototypes.
*******************************************************************************************/
typedef BOOL (*SME_ENUM_MEM_CALLBACK_T)(void *pUserData, unsigned int nSize, 
	const char *sFileName, int nLineNumber);

void * SmeMAllocDebug (unsigned int nSize, const char * sFile, int nLine);
int SmeMFreeDebug (void * pUserData, const char * sFile, int nLine);

enum {
	SME_HOOK_PRE_ALLOC,
	SME_HOOK_POST_ALLOC,
	SME_HOOK_PRE_FREE,
	SME_HOOK_POST_FREE
};
typedef unsigned char SME_MEM_OPR_HOOK_TYPE_T;

typedef int (*SME_MEM_OPR_HOOK_T)(SME_MEM_OPR_HOOK_TYPE_T nAllocType, void *pUserData, unsigned int nSize, 
	const char *sFileName, int nLineNumber);

SME_MEM_OPR_HOOK_T SmeSetMemOprHook(SME_MEM_OPR_HOOK_T pMemOprHook);
BOOL SmeEnumMem(SME_ENUM_MEM_CALLBACK_T fnEnumMemCallBack);
void SmeDumpMem();
void SmeAllocMemSize();
BOOL SmeDbgInit();

#if SME_DEBUG
	#define SME_DBG_INIT SmeDbgInit
	#define SmeMAlloc(nSize)  	SmeMAllocDebug((nSize), __FILE__, __LINE__)
	#define SmeMFree(pUserData)  	SmeMFreeDebug((pUserData), __FILE__, __LINE__)

	#define SME_SET_MEM_OPR_HOOK(pMemOprHook)	SmeSetMemOprHook(pMemOprHook) 
	#define SME_ENUM_MEM(fnEnumMemCallBack) SmeEnumMem(fnEnumMemCallBack)
	#define SME_DUMP_MEM SmeDumpMem
	#define SME_ALLOC_MEM_SIZE SmeAllocMemSize
	#define SME_ENABLE_STATE_TRACK(bStateTree, bOutputWin) SmeEnableStateTracking(bStateTree, bOutputWin)

#else /* not SME_DEBUG */
	#define SME_DBG_INIT()
	#define SmeMAlloc(nSize)	SmeMAllocRelease((nSize))
	#define SmeMFree(pUserData)		SmeMFreeRelease((pUserData))

	#define SME_SET_MEM_OPR_HOOK(pMemOprHook)
	#define SME_ENUM_MEM(fnEnumMemCallBack)
	#define SME_DUMP_MEM()
	#define SME_ALLOC_MEM_SIZE()
	#define SME_ENABLE_STATE_TRACK(bStateTree, bOutputWin) 
#endif  

#ifdef __cplusplus
}
#endif 


#endif /* SME_DEBUG_H */
