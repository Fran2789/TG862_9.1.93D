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

/* State Chart:

PowerDown -> Join1 -> Cond1 -> PowerUp -> 
    ^                                   |  
    -------------------------------------

PowerUp (Playing <---> Pause)

Refer to Readme.txt for more information.
*/

#include "sme.h"

#if !SME_CPP

#include "Player_c.h"
#include "Player2_c.h"
#include "EventId.h"
#include <stdio.h>
#include "sme_cross_platform.h"
#include "sme_debug.h"

/////////////////////////////////////////////////////////////////////////////////
#ifdef SME_CURR_DEFAULT_PARENT
#undef SME_CURR_DEFAULT_PARENT
#endif
#define SME_CURR_DEFAULT_PARENT DummyRoot

// Define a dummy root state. Make Player state as a temp state.
/*
SME_BEGIN_ROOT_COMP_STATE_DEF(DummyRoot, SME_NULL_ACTION, SME_NULL_ACTION)
	SME_ON_INIT_STATE(SME_NULL_ACTION, Player)
SME_END_STATE_DEF

SME_BEGIN_SUB_STATE_DEF_P(Player)
SME_END_STATE_DEF
*/

SME_DECLARE_TEMP_ROOT(DummyRoot,Player)

/////////////////////////////////////////////////////////////////////////////////
#ifdef SME_CURR_DEFAULT_PARENT
#undef SME_CURR_DEFAULT_PARENT
#endif
#define SME_CURR_DEFAULT_PARENT Player


SME_BEGIN_COMP_STATE_DEF(Player, DummyRoot, PlayerEntry, PlayerExit)
	//SME_ON_INIT_STATE(SME_NULL_ACTION, PowerDown) //
	SME_ON_INIT_STATE(SME_NULL_ACTION, Join1)  // Test case: Initial state is a pseudo state.
SME_END_STATE_DEF

/* Define state PowerDown */
SME_BEGIN_LEAF_STATE_DEF_P(PowerDown, PowerDownEntry, PowerDownExit)
	//SME_ON_EVENT(EXT_EVENT_ID_POWER, OnPowerDownEXT_EVENT_ID_POWER, PowerUp) // Test Case: transit to a composite state
	//SME_ON_EVENT(EXT_EVENT_ID_POWER, OnPowerDownEXT_EVENT_ID_POWER, Cond1) // Test Case: transit to a conditional pseudo state
	SME_ON_EVENT_WITH_GUARD(EXT_EVENT_ID_POWER, Guard1_func, OnPowerDownEXT_EVENT_ID_POWER, Join1) // Test Case: transit to a join pseudo state
SME_END_STATE_DEF

SME_BEGIN_SUB_STATE_DEF_P(PowerUp)
	SME_ON_EVENT(EXT_EVENT_ID_POWER,OnPowerUpEXT_EVENT_ID_POWER,PowerDown)
SME_END_STATE_DEF

enum {COND_EV_COND1, COND_EV_COND2};
/******************************************************************************** */
/* Define Conditional Note: No Guard for pseudo state.*/
SME_BEGIN_COND_STATE_DEF_P(Cond1, Cond1_func)
	SME_ON_EVENT(COND_EV_COND1, CondAct1, Playing) 
	SME_ON_EVENT(COND_EV_COND2, CondAct2, Pause)
	SME_ON_EVENT_COND_ELSE(CondActElse, PowerUp)
SME_END_STATE_DEF

/* Define Join */
SME_BEGIN_JOIN_STATE_DEF_P(Join1) /*Test Case: a pseudo state -> a pseudo state */
	SME_ON_JOIN_TRAN(JoinAct, Cond1)
SME_END_STATE_DEF

// SME_APPLICATION_DEF(Player1, Player)
SME_APPLICATION_DEF(Player1, DummyRoot)

BOOL Guard1_func(SME_APP_T *pDestApp, SME_EVENT_T *pEvent)
{
	printf("Guard1_func\n");
	return TRUE;
}

BOOL GuardTimer2_func(SME_APP_T *pDestApp, SME_EVENT_T *pEvent)
{
	//return TRUE;
	return FALSE;
}

int Cond1_func(SME_APP_T *pDestApp, SME_EVENT_T *pEvent)
{
	printf("Cond1_func\n");
	return SME_EVENT_COND_ELSE;
}

int CondAct1(SME_APP_T *pDestApp, SME_EVENT_T *pEvent)
{
	printf("CondAct1c\n");
	return 0;
}

int CondAct2(SME_APP_T *pDestApp, SME_EVENT_T *pEvent)
{
	printf("CondAct2\n");
	return 0;
}

int CondActElse(SME_APP_T *pDestApp, SME_EVENT_T *pEvent)
{
	printf("CondActElse\n");
	return 0;
}

int JoinAct(SME_APP_T *pDestApp, SME_EVENT_T *pEvent)
{
	printf("JoinAct\n");
	return 0;
}


int PlayerEntry(SME_APP_T *pApp, SME_EVENT_T *pEvent)
{
	printf("PlayerEntry\n");
	return 0;
}

int PlayerExit(SME_APP_T *pApp, SME_EVENT_T *pEvent)
{
	printf("PlayerExit\n");
	return 0;
}


int PowerDownEntry(SME_APP_T *pApp, SME_EVENT_T *pEvent)
{
	printf("PowerDownEntry\n");
	return 0;
}


int PowerDownExit(SME_APP_T *pApp, SME_EVENT_T *pEvent)
{
	printf("PowerDownExit\n");
	return 0;
}



#endif //SME_CPP
