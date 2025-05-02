
#ifndef SINGLE_STATE_H
#define SINGLE_STATE_H

#include "sme.h"


#ifdef __cplusplus
extern "C" {
#endif

#if SME_CPP

class CSingleState: public SME_APP_T
{
public:
	CSingleState(const char* _sAppName, SME_STATE_T *_pRoot):SME_APP_T(_sAppName, _pRoot) {
	};

SME_BEGIN_CLASS_MEMBER_DECLARE(SingleState)
SME_END_CLASS_MEMBER_DECLARE

#else
/* Pre-declare states that are referenced before they are defined */
SME_BEGIN_STATE_DECLARE(SingleState)
SME_END_STATE_DECLARE

#endif


/*{{SME_BEGIN_EVENT_HANDLER(Player)*/
int SingleStateEntry(SME_APP_T *pApp, SME_EVENT_T *pEvent);
int SingleStateExit(SME_APP_T *pApp, SME_EVENT_T *pEvent);
/*}}SME_END_EVENT_HANDLER*/

#if SME_CPP

};

#endif

#ifdef __cplusplus
}
#endif

#endif

