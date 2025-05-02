#include "SingleState.h"

#include "sme.h"
#include <stdio.h>
#include "sme_cross_platform.h"

/* Test Case: A state machine with a single state. */

#if SME_CPP
	SME_HANDLER_CLASS_DEF(SingleState)
#endif 

/* Define Root (Composite-state) of Player*/
SME_BEGIN_ROOT_COMP_STATE_DEF(SingleState, SingleStateEntry, SingleStateExit)
SME_END_STATE_DEF

//SME_BEGIN_LEAF_STATE_DEF(SingleStateLeaf, SingleState, SingleStateEntry, SingleStateExit)
//SME_END_STATE_DEF

SME_APPLICATION_DEF(SingleState1, SingleState)
SME_APPLICATION_DEF(SingleState2, SingleState)

#if SME_CPP
	BOOL CSingleState::SingleStateEntry(SME_APP_T *pDestApp, SME_EVENT_T *pEvent)
	{
		printf("SingleStateEntry\n");
		return TRUE;
	}

	BOOL CSingleState::SingleStateExit(SME_APP_T *pDestApp, SME_EVENT_T *pEvent)
	{
		printf("SingleStateExit\n");
		return TRUE;
	}

#else
	BOOL SingleStateEntry(SME_APP_T *pDestApp, SME_EVENT_T *pEvent)
	{
		printf("SingleStateEntry\n");
		return TRUE;
	}



	BOOL SingleStateExit(SME_APP_T *pDestApp, SME_EVENT_T *pEvent)
	{
		printf("SingleStateExit\n");
		return TRUE;
	}
#endif
