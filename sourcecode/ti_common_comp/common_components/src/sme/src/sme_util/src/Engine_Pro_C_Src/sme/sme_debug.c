/*-----------------------------------------------------------------------------------------------------
 * Copyright 2007, Texas Instruments Incorporated
 *
 * This program has been modified from its original operation by Texas Instruments
 * to do the following:
 *      Reformat the output of trace lines in SmeStateTrack
 *      Reformat the output of trace lines in SmeCatchCurrentStates
 *      Added the ability to print event names, taken from Engine_Pro_C_Src_070516_EventName
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
 * -----------------------------------------------------------------------------
 *                               Revision History
 * -----------------------------------------------------------------------------
 * Version   Date      Author          Revision Detail
 * 1.0.0    2004/11/26                 Initial
 *		    2004/11/26                 $ Fix memory free bug.
 *          2005/9/6                   Module Tracer
			2005/12/26				   Merge Unicode edition with ASCII edition
 * ===========================================================================*/


#include <stdio.h> 
#include <stdarg.h>
#include <memory.h>
#include <time.h>

#include "sme.h"
#if SME_UNICODE
#include <wchar.h>
#endif
#include "sme_debug.h"
#include "sme_cross_platform.h"

#ifdef SME_WIN32
	#include "windows.h"
	// If the compiler encounters the use of a deprecated identifier, a C4996 warning is thrown.
	#pragma warning(disable : 4996)
	// warning C4244: '=' : conversion from 'int ' to 'unsigned char ', possible loss of data
	#pragma warning(disable : 4244)
	#define vsnprintf _vsnprintf
	#define snprintf  _snprintf
#endif 

/* Remove comment to use original SmeCatchCurrentStates() */
//#define USE_ORIGINAL_SmeCatchCurrentStates 1

static SME_OUTPUT_DEBUG_ASCII_STR_PROC_T g_fnOutputDebugAsciiStr = NULL; 
static SME_OUTPUT_DEBUG_UNICODE_STR_PROC_T g_fnOutputDebugUnicodeStr = NULL; 
static SME_MEM_OPR_HOOK_T g_fnMemOprHook = NULL;

#define MAX_MODULE_NUM32	4
#define MAX_MODULE_NUM		128 
static SME_UINT32 g_ModuleToLog[MAX_MODULE_NUM32] = {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF}; 

static SME_UINT32 g_FieldToLog = 0xFFFFFFFF;

static BOOL g_bStateTrackinTree =TRUE;
static BOOL g_bStateTrackinOutputWin =TRUE;

#if defined(_WIN32_WCE) || defined(SME_WIN32)

///////////////////////////////////////////////////////////////////////////////////////////
// DESCRIPTION: This function gets VC workspace window handle by environment variables. 
// INPUT:  
//  None. 
// OUTPUT: 
// RETURN: 
// NOTE: 
//	It will be used to detect memory leaks, if developer call it when application enter some status.
///////////////////////////////////////////////////////////////////////////////////////////
#define VCDEV_INFO_VAR	"smw_vcdev_info_var"
HWND GetDSWorkspaceHandle()
{
	TCHAR sEnvVar[128];
	TCHAR *pEnd;

	if (GetEnvironmentVariable(VCDEV_INFO_VAR, sEnvVar, 128)<=0)
		return NULL;

	return (HWND)(strtoul((const char *)sEnvVar, &pEnd, 16));
}

HWND g_hWorkspaceWnd= NULL; 
#endif // defined(_WIN32_WCE) || defined(SME_WIN32)

#define ID_COPYDATA_STATE_TRACK 1100
#define ID_COPYDATA_DEBUG_STR	1101

/* Output debug string in a default mode. */
static BOOL OutputDebugAsciiStr(const char * sDebugStr);
static BOOL OutputDebugUnicodeStr(const wchar_t* sDebugStr);

/*******************************************************************************************
* DESCRIPTION: Turn on the debug string tracer for given module.
* INPUT:  
*  1) nModuleID: The module ID, ranging from 0 to 127 
* OUTPUT: None.
* NOTE: 
*******************************************************************************************/
void SmeTurnOnModuleTracer(int nModuleID)
{
	g_ModuleToLog[nModuleID>>5] |= (1 << (nModuleID%32));
}

/*******************************************************************************************
* DESCRIPTION: Turn off the debug string tracer of given module.
* INPUT:  
*  1) nModuleID: The module ID, ranging from 0 to 127 
* OUTPUT: None.
* NOTE: 
*******************************************************************************************/
void SmeTurnOffModuleTracer(int nModuleID)
{
	g_ModuleToLog[nModuleID>>5] &= ~(1 << (nModuleID%32));
}

/*******************************************************************************************
* DESCRIPTION: Turn on all module tracers.
* INPUT:  
*  None.
* OUTPUT: None.
* NOTE: 
*******************************************************************************************/
void SmeTurnOnAllModuleTracers()
{
	int i;
	for (i=0; i<MAX_MODULE_NUM32; i++)
		g_ModuleToLog[i] = 0xFFFFFFFF;
}

/*******************************************************************************************
* DESCRIPTION: Turn on all module tracers.
* INPUT:  
*  None
* OUTPUT: None.
* NOTE: 
*******************************************************************************************/
void SmeTurnOffAllModuleTracers()
{
	int i;
	for (i=0; i<MAX_MODULE_NUM32; i++)
		g_ModuleToLog[i] = 0x0;
}

void SmeTurnOnLogField(int nField)
{
	if (nField<0 || nField>=SME_LOG_FIELD_NUM)
		return;

	g_FieldToLog |= (1 << nField);

}
void SmeTurnOffLogField(int nField)
{
	if (nField<0 || nField>=SME_LOG_FIELD_NUM)
		return;

	g_FieldToLog &= ~(1 << nField);
}
void SmeTurnOnAllLogFields()
{
	g_FieldToLog = 0xFFFFFFFF;
}
void SmeTurnOffAllLogFields()
{
	g_FieldToLog = 0x0;
}

/*******************************************************************************************
* DESCRIPTION: Output debug string through user defined function.
* INPUT:  
*  1) sFormat: The format of output string 
*  2) ...: The variable argument list.
* OUTPUT: None.
* NOTE: 
*******************************************************************************************/
SME_OUTPUT_DEBUG_ASCII_STR_PROC_T	SmeSetOutputDebugAsciiStrProc(SME_OUTPUT_DEBUG_ASCII_STR_PROC_T fnProc)
{
	SME_OUTPUT_DEBUG_ASCII_STR_PROC_T	fnOldProc = g_fnOutputDebugAsciiStr;
	g_fnOutputDebugAsciiStr = fnProc;
	return fnOldProc;
}

SME_OUTPUT_DEBUG_UNICODE_STR_PROC_T	SmeSetOutputDebugUnicodeStrProc(SME_OUTPUT_DEBUG_UNICODE_STR_PROC_T fnProc)
{
	SME_OUTPUT_DEBUG_UNICODE_STR_PROC_T	fnOldProc = g_fnOutputDebugUnicodeStr;
	g_fnOutputDebugUnicodeStr = fnProc;
	return fnOldProc;
}
/*******************************************************************************************
* DESCRIPTION: Output formated Unicode debug string through user defined function if available.
* INPUT:
*  1) nModuleID: Range from 0 to (32*MAX_MODULE_NUM32 - 1), which identify a module.  
*  2) sFormat: The format of output string in ASCII format. 
*  3) ...: The variable argument list.
* OUTPUT: None.
* NOTE: 
* 1) No matter, SME_UNICODE is defined or not, the format of output string is in ASCII format. 
* It will be easier for developer to print Unicode string. Nevertheless, the string argument 
* should be in Unicode format.
* 2) If the given module's tracer is turned on, print the debug string. If module id is -1, print it.  
*******************************************************************************************/
void SmeUnicodeTrace(int nModuleID, const wchar_t * wsFormat, ...)
{
	va_list   ArgList;
	wchar_t sBuf[SME_MAX_STR_BUF_LEN]; /* ASCII/Unicode string buffer */
	memset(sBuf, 0, sizeof(sBuf));

	if (nModuleID>=MAX_MODULE_NUM) return;
	if (nModuleID!=-1 && (g_ModuleToLog[nModuleID>>5] & (1 << (nModuleID%32))) == 0) return;

	va_start(ArgList, wsFormat);

#ifdef SME_WIN32
	_vsnwprintf(sBuf, SME_DIM(sBuf) - 1, (wchar_t*)wsFormat, ArgList);
#else
	 //vswprintf(sBuf, SME_DIM(sBuf) - 1, (wchar_t*)wsFormat, ArgList); 
#endif

	if (g_fnOutputDebugUnicodeStr)
		(*g_fnOutputDebugUnicodeStr)(sBuf);
	else
		OutputDebugUnicodeStr(sBuf);

	va_end(ArgList);

	return;
}

void SmeUnicodeTraceAscFmt(int nModuleID, const char * sFormat, ...)
{
	va_list   ArgList;
	unsigned int i;
	wchar_t sBuf[SME_MAX_STR_BUF_LEN]; /* ASCII/Unicode string buffer */
	wchar_t wsFormatBuf[SME_MAX_STR_BUF_LEN];
	memset(sBuf, 0, sizeof(sBuf));
	memset(wsFormatBuf, 0, sizeof(wsFormatBuf));

	if (nModuleID>=MAX_MODULE_NUM) return;
	if (nModuleID!=-1 && (g_ModuleToLog[nModuleID>>5] & (1 << (nModuleID%32))) == 0) return;

	va_start(ArgList, sFormat);

	for (i = 0; i <= strlen(sFormat); i++)
		wsFormatBuf[i] = sFormat[i];

#ifdef SME_WIN32
	_vsnwprintf(sBuf, SME_DIM(sBuf) - 1, (wchar_t*)wsFormatBuf, ArgList);
#else
	// vswprintf(sBuf, SME_DIM(sBuf) - 1, (wchar_t*)wsFormatBuf, ArgList); ???
#endif

	if (g_fnOutputDebugUnicodeStr)
		(*g_fnOutputDebugUnicodeStr)(sBuf);
	else
		OutputDebugUnicodeStr(sBuf);

	va_end(ArgList);

	return;
}

/*******************************************************************************************
* DESCRIPTION: Output formated ASCII debug string through user defined function if available.
* INPUT:  
*  1) sFormat: The format of output string in ASCII format. 
*  2) ...: The variable argument list. Text string should be in ASCII format. 
* OUTPUT: None.
* NOTE: 
* 1) No matter, SME_UNICODE is defined or not, the format of output string and text string argument should be in ASCII format. 
* It will be easier for developer to print ACSII string if _UNICODE is defined.
* 2) If the given module's tracer is turned on, print the debug string. If module id is -1, print it.  
*******************************************************************************************/
void SmeAsciiTrace(int nModuleID, const char * sFormat, ...)
{
	va_list   ArgList;
	char sBuf[SME_MAX_STR_BUF_LEN]; /* ASCII string buffer */
	memset(sBuf, 0, sizeof(sBuf));

	if (nModuleID>=MAX_MODULE_NUM) return;
	if (nModuleID!=-1 && (g_ModuleToLog[nModuleID>>5] & (1 << (nModuleID%32))) == 0) return;

	va_start(ArgList, sFormat);
	vsnprintf(sBuf, SME_DIM(sBuf) - 1, sFormat, ArgList);
	va_end(ArgList);

    if (g_fnOutputDebugAsciiStr)
       (*g_fnOutputDebugAsciiStr)(nModuleID, sBuf);
    else
        OutputDebugAsciiStr(sBuf);
    
	return;
}

/*******************************************************************************************
* DESCRIPTION:  Convert the formated input ASCII string into an output string in the character set 
depending on SME_UNICODE. 
*******************************************************************************************/
void SmeConvAsciiStr(SME_CHAR* sOutput, int nLen, const char * sFormat, ...)
{
	va_list   ArgList;

#if SME_UNICODE
	char sBuf[SME_MAX_STR_BUF_LEN]; /* ASCII string buffer */
	memset(sBuf, 0, sizeof(sBuf));

	va_start(ArgList, sFormat);
	vsnprintf(sBuf, SME_DIM(sBuf) -1, sFormat, ArgList);
	va_end(ArgList);

	{
		int i=0;
		while (0!=sBuf[i] && i<nLen-1)
		{
			sOutput[i] = sBuf[i];
			i++;
		}
		sOutput[i]=0;
	}
#else
	char *sBuf = sOutput;

	if (NULL==sOutput || nLen <=1 || NULL==sFormat)
		return;

	sBuf[nLen-1] =0; /* Ensure ended with NULL.*/

	va_start(ArgList, sFormat);
	vsnprintf(sBuf, nLen-1, sFormat, ArgList);
	va_end(ArgList);
#endif

}

/*******************************************************************************************
* The memory block information nodes will linked as a stack. The header will be g_pMemBlkList.
* The pNext property in SME_MEM_BLK_INFO_T will pointer to the previous node.
*  NULL <--- Node.pNext  <--- Node.pNext  <--- g_pMemBlkList
*******************************************************************************************/
#define SME_NO_MANS_LAND 0xFDFDFDFD
#define SME_NEW_MEM_BLK 0xCD

typedef struct SME_MEM_BLK_INFO_T_TAG
{
	void* pUserData;
	struct SME_MEM_BLK_INFO_T_TAG *pNext;
	unsigned int nSize;
	const char * sFile;
	int nLine;
} SME_MEM_BLK_INFO_T;

static SME_MEM_BLK_INFO_T *g_pMemBlkList = NULL;
static int g_nMemBlkNum=0;
static int g_nTotalAllocMem=0;

/*******************************************************************************************
* DESCRIPTION:  State machine engine memory allocation function in debug version. 
* INPUT: nSize: Memory size;
* OUTPUT: Allocated memory pointer.
* NOTE: 
*   The new user data is set to SME_NEW_MEM_BLK and no man land is set to SME_NO_MANS_LAND.
*******************************************************************************************/
void * SmeMAllocDebug(unsigned int nSize, const char * sFile, int nLine)
{
	void *p;
	unsigned long *pLong;
	SME_MEM_BLK_INFO_T *pMemBlkInfo;
	
	/* Call pre-allocation hook function. Allocation abort if hook function returns FALSE. */
	if (g_fnMemOprHook)
		if ((*g_fnMemOprHook)(SME_HOOK_PRE_ALLOC, NULL, nSize, sFile, nLine) == FALSE)
			return NULL;

	/* Adjust to be 4-byte boundary aligned. */
	nSize = ((nSize + 3) >> 2)<<2; 
	p = SmeMAllocRelease(nSize + sizeof(unsigned long));
	if (p==NULL) return NULL;

	/* Set new user data and no man land. */
	memset(p, SME_NEW_MEM_BLK, nSize);
	pLong = (unsigned long *)((unsigned char*)p + nSize);
	*pLong = SME_NO_MANS_LAND; 

	pMemBlkInfo = (SME_MEM_BLK_INFO_T *)SmeMAllocRelease(sizeof(SME_MEM_BLK_INFO_T));
	if (pMemBlkInfo==NULL) return p;

	/* Push pMemBlkInfo to the memory block stack. */
	pMemBlkInfo->pUserData = p;
	pMemBlkInfo->nSize = nSize;
	pMemBlkInfo->sFile = sFile;
	pMemBlkInfo->nLine = nLine;
	pMemBlkInfo->pNext = g_pMemBlkList;
	g_pMemBlkList = pMemBlkInfo;
	g_nMemBlkNum++;
	g_nTotalAllocMem+=pMemBlkInfo->nSize;

	/* Call post-allocation hook function. */
	if (g_fnMemOprHook)
		(*g_fnMemOprHook)(SME_HOOK_POST_ALLOC, p, nSize, sFile, nLine);

	return p;
}

/*******************************************************************************************
* DESCRIPTION:  State machine engine memory free function in release version. 
* INPUT: pUserData: Memory block pointer;
* OUTPUT: The result depends on user defined memory free function.
* NOTE: It will check no man land to detect whether memory overwriting occurs.
*   
*******************************************************************************************/
BOOL SmeMFreeDebug(void * pUserData, const char * sFile, int nLine)
{
	SME_MEM_BLK_INFO_T *pMemBlkInfo = g_pMemBlkList;
	SME_MEM_BLK_INFO_T *pPre = g_pMemBlkList;
	unsigned long *pLong;
	
	/* Get memory block information from stack. */
	while (pMemBlkInfo!=NULL && pMemBlkInfo->pUserData != pUserData)
	{
		pPre = pMemBlkInfo;
		pMemBlkInfo = pMemBlkInfo->pNext; 
	};

	if (pMemBlkInfo == NULL)
	{
		SME_ASCII_TRACE(SME_MODULE_ERR_OUTPUT, SMESTR_ERR_DEL_INVALID_MEM,
			sFile, nLine);
		return FALSE;
	};

	/* Call pre-free hook function. */
	if (g_fnMemOprHook)
		(*g_fnMemOprHook)(SME_HOOK_PRE_FREE, 
			pMemBlkInfo->pUserData, 
			pMemBlkInfo->nSize, 
			pMemBlkInfo->sFile, 
			pMemBlkInfo->nLine);

	/* Check no man land. */
	pLong = (unsigned long *)((unsigned char*)pMemBlkInfo->pUserData + pMemBlkInfo->nSize);
	if (*pLong != SME_NO_MANS_LAND)
	{
		SME_ASCII_TRACE(SME_MODULE_ERR_OUTPUT,SMESTR_ERR_MEM_OVER_RUN,
			pMemBlkInfo->sFile, pMemBlkInfo->nLine);
		return FALSE;
	}

	/* Call post-free hook function. */
	if (g_fnMemOprHook)
		(*g_fnMemOprHook)(SME_HOOK_POST_FREE, 
			pMemBlkInfo->pUserData, 
			pMemBlkInfo->nSize, 
			pMemBlkInfo->sFile, 
			pMemBlkInfo->nLine);

	/* Remove this memory block information node from list. */
	if (pMemBlkInfo == g_pMemBlkList)
		/* $$ What to remove is located in the header. */
		g_pMemBlkList = g_pMemBlkList->pNext;
	else
		pPre->pNext = pMemBlkInfo->pNext;

	g_nMemBlkNum--;
	g_nTotalAllocMem-=pMemBlkInfo->nSize;

	SmeMFreeRelease(pMemBlkInfo);
	
	return SmeMFreeRelease(pUserData);
}

/*******************************************************************************************
* DESCRIPTION:  Reset memory block information list. 
* INPUT: None;
* OUTPUT: None.
* NOTE: It will release all allocated memory blocks including user data and memory information blocks.
*   
*******************************************************************************************/
BOOL SmeDbgInit()
{
	SME_MEM_BLK_INFO_T *pMemBlkInfo = g_pMemBlkList;
	if (g_pMemBlkList == NULL) return FALSE;
	while (g_pMemBlkList!=NULL)
	{
		pMemBlkInfo = g_pMemBlkList;
		g_pMemBlkList = g_pMemBlkList->pNext; 

		SmeMFreeRelease(pMemBlkInfo->pUserData);
		SmeMFreeRelease(pMemBlkInfo);
	}

	g_nMemBlkNum=0;
	g_nTotalAllocMem=0;
	return TRUE;
}
/*******************************************************************************************
* DESCRIPTION: This function installs a user defined hook function and return the old hook function.
*  Hook function is called every time memory is pre-allocated, post-allocated, pre-freed, or post-freed. 
* INPUT:  
*  pMemOprHook: New memory operation hook function. 
* OUTPUT: Old memory operation hook function. 
* 
* NOTE: 
*******************************************************************************************/
SME_MEM_OPR_HOOK_T SmeSetMemOprHook(SME_MEM_OPR_HOOK_T pMemOprHook)
{
	SME_MEM_OPR_HOOK_T fnOld = g_fnMemOprHook;
	g_fnMemOprHook = pMemOprHook;
	return fnOld;
}

/*******************************************************************************************
* DESCRIPTION: This function enumerates all allocated memory blocks. The fnEnumMemCallBack 
*  function is called once per allocated memory block and is passed the information for each block.
* INPUT:  
*  SME_ENUM_MEM_CALLBACK_T fnEnumMemCallBack: Memory block enumeration call back function. 
* OUTPUT: 
* 
* NOTE: 
* typedef BOOL (*SME_ENUM_MEM_CALLBACK_T)( void *pUserData, unsigned int nSize, const unsigned char *sFileName, int nLineNumber);
* If the function returns TRUE, the enumeration will continue. If the function returns FALSE, the enumeration will stop.
*******************************************************************************************/
BOOL SmeEnumMem(SME_ENUM_MEM_CALLBACK_T fnEnumMemCallBack)
{
	SME_MEM_BLK_INFO_T *pMemBlkInfo = g_pMemBlkList;

	if (fnEnumMemCallBack==NULL) return FALSE;

	/* Get memory block information from stack. */
	while (pMemBlkInfo!=NULL)
	{
		if (FALSE == (*fnEnumMemCallBack)(pMemBlkInfo->pUserData, 
			pMemBlkInfo->nSize, 
			pMemBlkInfo->sFile, 
			pMemBlkInfo->nLine))
			break;
		pMemBlkInfo = pMemBlkInfo->pNext; 
	};

	return TRUE;
}

/*******************************************************************************************
* DESCRIPTION: This function print the total allocated memory block size. 
* INPUT:  
*  None. 
* OUTPUT: 
* NOTE: 
*	It will be used to detect memory leaks, if developer call it when application enter some status.
*******************************************************************************************/
void SmeAllocMemSize()
{
	SME_ASCII_TRACE(-1,SMESTR_TOTAL_MEM_SIZE, g_nTotalAllocMem);
}

/*******************************************************************************************
* DESCRIPTION: This function dumps the allocated memory block information list. 
* INPUT:  
*  None. 
* OUTPUT: 
* NOTE: 
*	It will be used to detect memory leaks, if developer call it when application enter some status.
*******************************************************************************************/
void SmeDumpMem()
{
	SME_MEM_BLK_INFO_T *pMemBlkInfo = g_pMemBlkList;
	SME_ASCII_TRACE(-1,SMESTR_INFO_MEM_DUMP);
	/* Get memory block information from stack. */
	while (pMemBlkInfo!=NULL)
	{
		SME_ASCII_TRACE(-1,SMESTR_INFO_MEM_INFO,
			pMemBlkInfo->pUserData, 
			pMemBlkInfo->nSize, 
			pMemBlkInfo->sFile, 
			pMemBlkInfo->nLine);
		pMemBlkInfo = pMemBlkInfo->pNext; 
	};
	SME_ASCII_TRACE(-1,SMESTR_INFO_MEM_NUM);
	return;
}

void SmeEnableStateTracking(BOOL bStateTree, BOOL bOutputWin)
{
	g_bStateTrackinTree = bStateTree;
	g_bStateTrackinOutputWin =bOutputWin;
}

static SME_GET_EVENT_NAME_PROC_T g_pfnGetEventNameProc = NULL;
SME_GET_EVENT_NAME_PROC_T SmeInstallGetEventNameProc(SME_GET_EVENT_NAME_PROC_T pfnProc)
{
	SME_GET_EVENT_NAME_PROC_T fnOld = g_pfnGetEventNameProc;
	g_pfnGetEventNameProc = pfnProc;
	return fnOld;
}

#ifdef _WIN32_WCE
extern BOOL EST_StateTrack(SME_APP_T *pDestApp, SME_STATE_T* pNewState);
#endif
/*******************************************************************************************
* DESCRIPTION: Send state update information to DS. 
* INPUT: 
*  None. 
* OUTPUT: 
* NOTE: Format of Data: Application name, root state name, and new state name separated with NULL.
* 1) If new state is "", the given application is inactive.
* 2) If application name is "", the application thread ends.

The state tracking in Studio only support ONE instance of an application. 
If an application runs at multiple threads, please turn off this function. 
*******************************************************************************************/
BOOL SmeStateTrack(SME_EVENT_T *pEvent, 
			SME_APP_T *pDestApp,
			SME_STATE_T* pOldState, SME_REASON_FOR_HANDLE_T nReason, int nTime) 			   
{
	// Print state tracking in output window:
	static BOOL bFirst = TRUE;
	char sLog[SME_MAX_STR_BUF_LEN];
	char sTemp[64];
	sTemp[SME_DIM(sTemp)-1] =0; /* The last character will never be touched. */
	sLog[SME_DIM(sLog)-1] =0; /* The last character will never be touched. */
	sLog[0]=0;

	if (bFirst)
	{
		bFirst = FALSE;
		SME_ASCII_TRACE(SME_MODULE_ENGINE, "");
		SME_ASCII_TRACE(SME_MODULE_ENGINE, SMESTR_START_STATE_TRACKING);
		if (g_FieldToLog & (1<<SME_LOG_FIELD_APP))
		{
			snprintf(sTemp, SME_DIM(sTemp)-1, "%-16s", SMESTR_FIELD_APP);
			strncat(sLog,sTemp,SME_DIM(sLog) - 1 - strlen(sLog));
		}
		if (g_FieldToLog & (1<<SME_LOG_FIELD_ROOT))
		{
			snprintf(sTemp, SME_DIM(sTemp)-1, "%-16s", SMESTR_FIELD_ROOT);
			strncat(sLog,sTemp,SME_DIM(sLog) - 1 - strlen(sLog));
		}
		if (g_FieldToLog & (1<<SME_LOG_FIELD_SRC_STATE))
		{
			snprintf(sTemp, SME_DIM(sTemp)-1, "%-16s", SMESTR_FIELD_SRC_STATE);
			strncat(sLog,sTemp,SME_DIM(sLog) - 1 - strlen(sLog));
		}
		if (g_FieldToLog & (1<<SME_LOG_FIELD_DEST_STATE))
		{
			snprintf(sTemp, SME_DIM(sTemp)-1, "%-16s", SMESTR_FIELD_DEST_STATE);
			strncat(sLog,sTemp,SME_DIM(sLog) - 1 - strlen(sLog));
		}
		if (g_FieldToLog & (1<<SME_LOG_FIELD_STATE_DEPTH))
		{
			snprintf(sTemp, SME_DIM(sTemp)-1, "%-6s", SMESTR_FIELD_STATE_DEPTH);
			strncat(sLog,sTemp,SME_DIM(sLog) - 1 - strlen(sLog));
		}
		if (g_FieldToLog & (1<<SME_LOG_FIELD_EVENT))
		{
			snprintf(sTemp, SME_DIM(sTemp)-1, "%-10s", SMESTR_FIELD_EVENT);
			strncat(sLog,sTemp,SME_DIM(sLog) - 1 - strlen(sLog));
		}
		if (g_FieldToLog & (1<<SME_LOG_FIELD_EVENT_SEQNUM))
		{
			snprintf(sTemp, SME_DIM(sTemp)-1, "%-10s", SMESTR_FIELD_EVENT_SEQNUM);
			strncat(sLog,sTemp,SME_DIM(sLog) - 1 - strlen(sLog));
		}
		if (g_FieldToLog & (1<<SME_LOG_FIELD_EVENT_DATA))
		{
			snprintf(sTemp, SME_DIM(sTemp)-1, "%-20s", SMESTR_FIELD_EVENT_DATA);
			strncat(sLog,sTemp,SME_DIM(sLog) - 1 - strlen(sLog));
		}
		if (g_FieldToLog & (1<<SME_LOG_FIELD_HANDLING_TIME))
		{
			snprintf(sTemp, SME_DIM(sTemp)-1, "%-10s", SMESTR_FIELD_TIME);
			strncat(sLog,sTemp,SME_DIM(sLog) - 1 - strlen(sLog));
		}
		
		if (g_FieldToLog & (1<<SME_LOG_FIELD_REASON))
		{
			snprintf(sTemp, SME_DIM(sTemp)-1, "%-30s", SMESTR_FIELD_REASON);
			strncat(sLog,sTemp,SME_DIM(sLog) - 1 - strlen(sLog));
		}
		SME_ASCII_TRACE(SME_MODULE_ENGINE, sLog);
	}

	if (!(g_FieldToLog & (1<<SME_LOG_INTERNAL_TRAN)))
	{
		/* Turned off internal transition log. */
		if (SME_REASON_INTERNAL_TRAN==nReason)
			return FALSE;
	}
	

	sLog[0]=0;
    /* Time stamp comes before, let is mark the beg of a record */
    /* the rest of the lines are indented */
	strncat(sLog, "\n\t", SME_DIM(sLog) - 1 - strlen("\n\t"));
	if (g_FieldToLog & (1<<SME_LOG_FIELD_APP))
	{
		if (pDestApp)
			snprintf(sTemp, SME_DIM(sTemp)-1, "%-16s", pDestApp->sAppName);
		else
			snprintf(sTemp, SME_DIM(sTemp)-1, "%-16s", "");
		strncat(sLog,sTemp,SME_DIM(sLog) - 1 - strlen(sLog));
	}
	if (g_FieldToLog & (1<<SME_LOG_FIELD_ROOT))
	{
		if (pDestApp && pDestApp->pRoot)
			snprintf(sTemp, SME_DIM(sTemp)-1, "%-16s", pDestApp->pRoot->sStateName);
		else
			snprintf(sTemp, SME_DIM(sTemp)-1, "%-16s", "");
		strncat(sLog,sTemp,SME_DIM(sLog) - 1 - strlen(sLog));
	}
    strncat(sLog, "\n\t", SME_DIM(sLog) - 1 - strlen("\n"));
	if (g_FieldToLog & (1<<SME_LOG_FIELD_SRC_STATE))
	{
		// No source state for state entry event.
		if (pOldState && nReason!=SME_REASON_ACTIVATED)
			snprintf(sTemp, SME_DIM(sTemp)-1, "%-16s", pOldState->sStateName);
		else
			snprintf(sTemp, SME_DIM(sTemp)-1, "%-16s", "");
		strncat(sLog,sTemp,SME_DIM(sLog) - 1 - strlen(sLog));
	}
    strncat(sLog, " --> ", SME_DIM(sLog) - 1 - strlen(" --> "));
    if (g_FieldToLog & (1<<SME_LOG_FIELD_EVENT))
    {
        if (pEvent)
            if (SME_EVENT_TIMER == pEvent->nEventID)
                snprintf(sTemp, SME_DIM(sTemp)-1, "%-15s", SMESTR_EVENT_TIMEOUT);
            else if (SME_EVENT_STATE_TIMER == pEvent->nEventID)
                snprintf(sTemp, SME_DIM(sTemp)-1, "%-15s", SMESTR_EVENT_STATE_TIMER);
            else
			{
				const char *sEventName = NULL;
				if (g_pfnGetEventNameProc)
					sEventName = (*g_pfnGetEventNameProc)(pEvent->nEventID);

				if (sEventName == NULL) 
                {
                    if (pEvent->nEventID == SME_EVENT_COND_ELSE)
                        sEventName = "SME_EVENT_COND_ELSE";
                    else if (pEvent->nEventID == SME_EVENT_EXIT_LOOP)
                        sEventName = "SME_EVENT_EXIT_LOOP";
                }

				if (sEventName)
					snprintf(sTemp, SME_DIM(sTemp)-1, "%-15s", sEventName);
				else
					snprintf(sTemp, SME_DIM(sTemp)-1, "%-15X", pEvent->nEventID);
			}
        else
            snprintf(sTemp, SME_DIM(sTemp)-1, "%-10s", "");
        strncat(sLog,sTemp,SME_DIM(sLog) - 1 - strlen(sLog));
    }
    strncat(sLog, " --> ", SME_DIM(sLog) - 1 - strlen(" --> "));
	if (g_FieldToLog & (1<<SME_LOG_FIELD_DEST_STATE))
	{
		// No destination state for state exit event.
		if (pDestApp->pAppState && nReason!=SME_REASON_DEACTIVATED)
			snprintf(sTemp, SME_DIM(sTemp)-1, "%-16s", pDestApp->pAppState->sStateName);
		else
			snprintf(sTemp, SME_DIM(sTemp)-1, "%-16s", "");
		strncat(sLog,sTemp,SME_DIM(sLog) - 1 - strlen(sLog));
	}
    strncat(sLog, "\n\t", SME_DIM(sLog) - 1 - strlen("\n"));
	if (g_FieldToLog & (1<<SME_LOG_FIELD_STATE_DEPTH))
	{
		snprintf(sTemp, SME_DIM(sTemp)-1, "%-6d", pDestApp->nStateNum);
		strncat(sLog,sTemp,SME_DIM(sLog) - 1 - strlen(sLog));
	}
	if (g_FieldToLog & (1<<SME_LOG_FIELD_EVENT_SEQNUM))
	{
		if (pEvent)
			snprintf(sTemp, SME_DIM(sTemp)-1, "%-10u", pEvent->nSequenceNum);
		strncat(sLog,sTemp,SME_DIM(sLog) - 1 - strlen(sLog));
	}
	if (g_FieldToLog & (1<<SME_LOG_FIELD_EVENT_DATA))
	{
		sTemp[0]=0;
		if (pEvent)
         {
			if (SME_EVENT_DATA_FORMAT_INT == pEvent->nDataFormat)
              {
				snprintf(sTemp, SME_DIM(sTemp)-1, SMESTR_FIELD_EVENT_DATA_INT_FMT, pEvent->Data.Int.nParam1, pEvent->Data.Int.nParam2);
              }
			else
              {
                if (SME_EVENT_DATA_FORMAT_PTR == pEvent->nDataFormat)
				snprintf(sTemp, SME_DIM(sTemp)-1, SMESTR_FIELD_EVENT_DATA_PTR_FMT, pEvent->Data.Ptr.pData, pEvent->Data.Ptr.nSize);
              }     
         }     
		strncat(sLog,sTemp,SME_DIM(sLog) - 1 - strlen(sLog));
	}
	if (g_FieldToLog & (1<<SME_LOG_FIELD_HANDLING_TIME))
	{
		snprintf(sTemp, SME_DIM(sTemp)-1, "%-10d", nTime);
		strncat(sLog,sTemp,SME_DIM(sLog) - 1 - strlen(sLog));
	}
	if (g_FieldToLog & (1<<SME_LOG_FIELD_REASON))
	{
		switch (nReason)
		{
		case SME_REASON_INTERNAL_TRAN:
			snprintf(sTemp, SME_DIM(sTemp)-1, "%-30s", SMESTR_INTERNAL_TRAN);
			break;
		case SME_REASON_ACTIVATED:
			snprintf(sTemp, SME_DIM(sTemp)-1, "%-30s", SMESTR_APP_ACTIVATED);
			break;
		case SME_REASON_DEACTIVATED:
			snprintf(sTemp, SME_DIM(sTemp)-1, "%-30s", SMESTR_APP_DEACTIVATED);
			break;
		case SME_REASON_HIT:
			snprintf(sTemp, SME_DIM(sTemp)-1, "%-30s", SMESTR_HIT);
			break;
		case SME_REASON_NOT_MATCH:
			snprintf(sTemp, SME_DIM(sTemp)-1, "%-30s", SMESTR_NOT_MATCH);
			break;
		case SME_REASON_GUARD:
			snprintf(sTemp, SME_DIM(sTemp)-1, "%-30s", SMESTR_GUARDED);
			break;
		case SME_REASON_COND:
			snprintf(sTemp, SME_DIM(sTemp)-1, "%-30s", SMESTR_COND);
			break;
		case SME_REASON_JOIN:
			snprintf(sTemp, SME_DIM(sTemp)-1, "%-30s", SMESTR_JOIN);
			break;
		default:
			snprintf(sTemp, SME_DIM(sTemp)-1, "%-30s", "");
		}
		strncat(sLog,sTemp,SME_DIM(sLog) - 1 - strlen(sLog));
	}

	SME_ASCII_TRACE(SME_MODULE_ENGINE, sLog);
	
	if (!g_bStateTrackinTree)
		return TRUE;

	/*  Send state tracking data to the State Tree in the Studio. */
	/* It is not necessary to state track is state is not updated.*/
	if (pOldState==pDestApp->pAppState)
		return TRUE;

#if defined(_WIN32_WCE)
	return EST_StateTrack(pDestApp, pNewState);
#elif defined(SME_WIN32)

	#define MAX_STATE_NAME_LEN 64
	#define MAX_TRACKING_MSG_LEN (MAX_STATE_NAME_LEN+1)*3

	{
	unsigned char byBuf[MAX_TRACKING_MSG_LEN]={0};
	const char* sApp,*sRoot,*sState;
	int nAppLen=0, nRootLen=0, nStateLen=0; 
	COPYDATASTRUCT cds;

	if (g_hWorkspaceWnd == NULL) 
	{
		g_hWorkspaceWnd= GetDSWorkspaceHandle();
		if (g_hWorkspaceWnd==NULL)
			return FALSE;
	}

	memset(byBuf,0, sizeof(byBuf));

	if (NULL==pDestApp)
		return FALSE;

	if (SME_IS_ACTIVATED(pDestApp))
	{
		sApp = pDestApp->sAppName;
		sRoot = pDestApp->pRoot->sStateName;
		sState = pDestApp->pAppState->sStateName;
	}
	else
	{
		sApp = "";
		sRoot = "";
		sState = "";
	}

	nAppLen = strlen(sApp);
	nRootLen = strlen(sRoot);
	nStateLen = strlen(sState);

	if (nAppLen > MAX_STATE_NAME_LEN) return FALSE;
	if (nRootLen > MAX_STATE_NAME_LEN) return FALSE;
	if (nStateLen > MAX_STATE_NAME_LEN) return FALSE;

	strcpy((char*)(&byBuf[0]), pDestApp->sAppName);
	strcpy((char*)(&byBuf[nAppLen+1]), sRoot);
	strcpy((char*)(&byBuf[nAppLen+1+nRootLen+1]), sState);

	cds.dwData = ID_COPYDATA_STATE_TRACK;
	cds.cbData = sizeof(byBuf);
	cds.lpData = byBuf;
	SendMessage(g_hWorkspaceWnd, WM_COPYDATA, (WPARAM)NULL, (LPARAM)&cds);
	}
	return TRUE;
#endif
	return TRUE;
}

extern SME_GET_THREAD_CONTEXT_PROC g_pfnGetThreadContext;

#ifndef USE_ORIGINAL_SmeCatchCurrentStates

static void SmePrintCurrStatePath(SME_STATE_T *pState, char **pStr)
{
    char *p;

    if (!pState || !pStr)
    {
        return;
    }

    /* The states are linked from child to parent.                                    */
    /* Therefore, 1st collect pParent, then collet curr.                              */
    if (pState->pParent)
    {
        SmePrintCurrStatePath(pState->pParent, pStr);
        if (!(*pStr))
        {
            return;
        }
    }

    /* Now add state */
    if (pState->sStateName)
    {
        p = realloc(*pStr, strlen(*pStr) + strlen(" --> ") + strlen(pState->sStateName) + 1);
        if (!p)
        {
            return;
        }
        else
        {
            *pStr = p;
            strcat(*pStr, " --> ");
        }
        strcat(*pStr, pState->sStateName);
    }
}

static void SmePrintCurrObj(SME_APP_T *pApp, BOOL bWithStatePath)
{
    static char sLog[SME_MAX_STR_BUF_LEN];
    static char sTemp[64];
    static int stlen;
    char *pathStrPtr = NULL;
#define MAX_ROOTNAME_LEN 35
#define MAX_APPNAME_LEN 30

    if (!pApp)
    {
        return;
    }

    /* Print from top to bottom hierarchy.                                            */
    /* The Objs are added to head, so the root is last, leaf is 1st.                  */
    /* Therefore, 1st print pNext, then print curr.                                   */
    if (pApp->pNext)
    {
        SmePrintCurrObj(pApp->pNext, bWithStatePath);
    }

    /* Now print curr */
    sLog[0]=0;
    if (pApp->pParent && pApp->pParent->sAppName)
    {
        snprintf(sTemp, SME_DIM(sTemp)-1, "[%s]", pApp->pParent->sAppName);
    }
    else
        snprintf(sTemp, SME_DIM(sTemp)-1, "[Root]");

    stlen = strlen(sTemp);
    if (stlen < MAX_ROOTNAME_LEN)
        memset(sTemp + stlen, ' ', MAX_ROOTNAME_LEN - stlen);
    sTemp[MAX_ROOTNAME_LEN] = '\0';
    strncat(sLog,sTemp,SME_DIM(sLog) - 1-strlen(sLog));
    strncat(sLog," --> ",SME_DIM(sLog) - 1-strlen(sLog));

    snprintf(sTemp, SME_DIM(sTemp)-1, "%s", pApp->sAppName);
    stlen = strlen(sTemp);
    if (stlen < MAX_APPNAME_LEN)
        memset(sTemp + stlen, ' ', MAX_APPNAME_LEN - stlen);
    sTemp[MAX_APPNAME_LEN] = '\0';
    strncat(sLog,sTemp,SME_DIM(sLog) - 1-strlen(sLog));
    if (pApp->pAppState)
        snprintf(sTemp, SME_DIM(sTemp)-1, "%s", pApp->pAppState->sStateName);
    else
        snprintf(sTemp, SME_DIM(sTemp)-1, " -->");
    strncat(sLog,sTemp,SME_DIM(sLog) - 1-strlen(sLog));
    SME_ASCII_TRACE(SME_MODULE_ENGINE, sLog);

    if (bWithStatePath)
    {
        pathStrPtr = malloc(strlen(pApp->sAppName) + 1);
        if (pathStrPtr)
        {
            pathStrPtr[0] = '\0';
            strcat(pathStrPtr, pApp->sAppName);
            SmePrintCurrStatePath(pApp->pAppState, &pathStrPtr);
            if (pathStrPtr)
            {
                SME_ASCII_TRACE(SME_MODULE_ENGINE, pathStrPtr);
                free(pathStrPtr);
            }
        }
    }
#undef MAX_APPNAME_LEN 
#undef MAX_ROOTNAME_LEN 
}

BOOL SmeCatchCurrentStates(BOOL bWithStatePath)
#else
BOOL SmeCatchCurrentStates()
#endif
{
	/* Traverse all active applications. */
	SME_APP_T *pApp;
	SME_THREAD_CONTEXT_PT pThreadContext=NULL;
	char sTemp[64];
	char sLog[SME_MAX_STR_BUF_LEN];

	sTemp[SME_DIM(sTemp)-1] =0;
	sLog[SME_DIM(sLog)-1] =0;

	if (g_pfnGetThreadContext)
		pThreadContext = (*g_pfnGetThreadContext)();
	if (!pThreadContext) return FALSE;

#ifdef USE_ORIGINAL_SmeCatchCurrentStates
	SME_ASCII_TRACE(SME_MODULE_ENGINE, "");
	SME_ASCII_TRACE(SME_MODULE_ENGINE, SMESTR_CATCH_STATES);
	sLog[0]=0;
	snprintf(sTemp, SME_DIM(sTemp)-1, "%-16s", "APPLICATION");
	strncat(sLog,sTemp,SME_DIM(sLog) - 1-strlen(sLog));
	snprintf(sTemp, SME_DIM(sTemp)-1, "%-16s", "STATE");
	strncat(sLog,sTemp,SME_DIM(sLog) - 1-strlen(sLog));
	SME_ASCII_TRACE(SME_MODULE_ENGINE, sLog);


    pApp = pThreadContext->pActAppHdr;
    while (pApp != NULL) 
	{
		sLog[0]=0;
		snprintf(sTemp, SME_DIM(sTemp)-1, "%-16s", pApp->sAppName);
		strncat(sLog,sTemp,SME_DIM(sLog) - 1-strlen(sLog));
		if (pApp->pAppState)
			snprintf(sTemp, SME_DIM(sTemp)-1, "%-16s", pApp->pAppState->sStateName);
		else
			snprintf(sTemp, SME_DIM(sTemp)-1, "%-16s", "");
		strncat(sLog,sTemp,SME_DIM(sLog) - 1-strlen(sLog));
		SME_ASCII_TRACE(SME_MODULE_ENGINE, sLog);

		pApp = pApp->pNext; 
	}
	SME_ASCII_TRACE(SME_MODULE_ENGINE, "");
#else
	pApp = pThreadContext->pActAppHdr;
	SmePrintCurrObj(pApp, bWithStatePath);
#endif

	return TRUE;
}
/*******************************************************************************************
* DESCRIPTION: Output debug string in ASCII in the default mode. Send it to Studio if running in, and 
  output the string to debug file. 
* INPUT:  
*  sDebugStr: NULL-terminated text string.
* OUTPUT: 
* NOTE: 
*******************************************************************************************/
BOOL OutputDebugAsciiStr(const char* sDebugStr)
{
	if (NULL==sDebugStr)
		return FALSE;

#if SME_UNICODE
	{
		/* Direct output ASCII string to Unicode file. */
		wchar_t sUnicode[SME_MAX_STR_BUF_LEN];
		SmeConvAsciiStr(sUnicode,SME_DIM(sUnicode),sDebugStr);
		return OutputDebugUnicodeStr(sUnicode);
	}

#endif

#ifdef SME_WIN32
	if (NULL != g_hWorkspaceWnd)
	{
		COPYDATASTRUCT cds;
		cds.dwData = ID_COPYDATA_DEBUG_STR;
		cds.cbData = strlen(sDebugStr)+1;
		cds.lpData = (void*)sDebugStr;
		SendMessage(g_hWorkspaceWnd, WM_COPYDATA, (WPARAM)NULL, (LPARAM)&cds);
	}
#endif
	{
		char sTimeStamp[32];
		FILE *f = fopen(SME_DEF_DBGLOG_FILE, "a+t");
		if (NULL==f)
			return FALSE;

		XGetCurrentTimeStr(sTimeStamp, sizeof(sTimeStamp),SMESTR_TIME_STAMP_FMT);
		fputs(sTimeStamp,f);
		fputs("\t",f);
		fputs(sDebugStr,f);
		fputs("\n",f);
		fclose(f);
	}
	return TRUE;
}


/*******************************************************************************************
* DESCRIPTION: Output debug string in Unicode in the default mode. Send it to Studio if running in, and 
  output the string to debug file in Little Endian. 
* INPUT:  
*  sDebugStr: Unicode text string.
* OUTPUT: 
* NOTE: 
*******************************************************************************************/
static unsigned char UNICODE_NEWLINE[] = {0x0D, 0, 0x0A, 0};
static unsigned char UNICODE_FILE_HEADER[] = {0xFF, 0xFE}; /*Little Endian */

static BOOL WriteStrToUnicodeFile(const char* s,FILE* f)
{
	int i=0;

	if (NULL==s || NULL==f)
		return FALSE;

	while (s[i])
	{
		if ('\n' == s[i])
		{
			fwrite(UNICODE_NEWLINE,1, sizeof(UNICODE_NEWLINE),f);
		} else
		{
			unsigned char b1,b2;
			b1 = s[i] & 255;
			b2 = (s[i] >> 8) & 255; 
			fwrite((const void*)&b1,1, 1,f);
			fwrite((const void*)&b2,1, 1,f);
		}
		i++;
	}
	return TRUE;
}

static BOOL WriteUStrToUnicodeFile(const wchar_t* s,FILE* f)
{
	int i=0;

	if (NULL==s || NULL==f)
		return FALSE;

	while (s[i])
	{
		if ('\n' == s[i])
		{
			fwrite(UNICODE_NEWLINE,1, sizeof(UNICODE_NEWLINE),f);
		} else
		{
			unsigned char b1,b2;
			b1 = s[i] & 255;
			b2 = (s[i] >> 8) & 255; 
			fwrite((const void*)&b1,1, 1,f);
			fwrite((const void*)&b2,1, 1,f);
		}
		i++;
	}
	return TRUE;
}

BOOL OutputDebugUnicodeStr(const wchar_t* sDebugStr)
{
	if (NULL==sDebugStr)
		return FALSE;
#ifdef SME_WIN32
	if (NULL != g_hWorkspaceWnd)
	{
		COPYDATASTRUCT cds;
		cds.dwData = ID_COPYDATA_DEBUG_STR;
		cds.cbData = (wcslen((wchar_t*)sDebugStr)+1)<<1;
		cds.lpData = (void*)sDebugStr;
		SendMessage(g_hWorkspaceWnd, WM_COPYDATA, (WPARAM)NULL, (LPARAM)&cds);
	}
#endif

	{
		char sTimeStamp[32];
		FILE *f = fopen(SME_DEF_UNICODE_DBGLOG_FILE, "rb");
		if (NULL==f)
		{
			/* Create it */
			f = fopen(SME_DEF_UNICODE_DBGLOG_FILE, "a+b");
			if( f )
                  fwrite(UNICODE_FILE_HEADER,1, sizeof(UNICODE_FILE_HEADER),f);
		};
		if( f )
             fclose(f);
		
		f = fopen(SME_DEF_UNICODE_DBGLOG_FILE, "a+b");

		if( NULL!=f )
		{
			XGetCurrentTimeStr(sTimeStamp, sizeof(sTimeStamp),SMESTR_TIME_STAMP_FMT);
			WriteStrToUnicodeFile(sTimeStamp,f);
			WriteStrToUnicodeFile("\t",f);
			WriteUStrToUnicodeFile(sDebugStr,f);
			fwrite(UNICODE_NEWLINE,1, sizeof(UNICODE_NEWLINE),f);
			fclose(f);
		}
	}

	return TRUE;
}


/*******************************************************************************************
* DESCRIPTION: Make a state as a temporary root.
*******************************************************************************************/
BOOL SmeMakeTempRoot(SME_APP_T *pApp, SME_STATE_T *pTempRoot)
{
	if (NULL==pApp || NULL==pTempRoot)
		return FALSE;

	pApp->pRoot = pTempRoot;
	pTempRoot->pParent = NULL;
	return TRUE;
}
