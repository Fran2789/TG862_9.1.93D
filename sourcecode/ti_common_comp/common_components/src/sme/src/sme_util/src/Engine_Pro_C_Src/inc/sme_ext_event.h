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
// ExtEvent.h
#ifndef _EXT_EVENT_H_
#define _EXT_EVENT_H_

#include "sme.h"

#ifdef __cplusplus   
extern "C" {
#endif

enum {SME_TIMER_TYPE_CALLBACK, SME_TIMER_TYPE_EVENT};

BOOL XInitMsgBuf();
BOOL XFreeMsgBuf();

int XPostThreadExtIntEvent(SME_THREAD_CONTEXT_T* pDestThreadContext, int nMsgID, int Param1, int Param2, 
						   SME_APP_T *pDestApp, unsigned long nSequenceNum,unsigned char nCategory);
int XPostThreadExtPtrEvent(SME_THREAD_CONTEXT_T* pDestThreadContext, int nMsgID, void *pData, int nDataSize, 
						   SME_APP_T *pDestApp, unsigned long nSequenceNum,unsigned char nCategory);

BOOL XGetExtEvent(SME_EVENT_T *pEvent);
BOOL XDelExtEvent(SME_EVENT_T *pEvent);

#ifdef __cplusplus
}
#endif 

#endif
