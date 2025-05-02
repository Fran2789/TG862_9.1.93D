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


#include "sme.h"

#if !SME_CPP

#include "Player_c.h"
#include "Player2_c.h"
#include "EventId.h"
#include <stdio.h>
#include "sme_cross_platform.h"
#include "sme_debug.h"

#ifdef SME_CURR_DEFAULT_PARENT
#undef SME_CURR_DEFAULT_PARENT
#endif
#define SME_CURR_DEFAULT_PARENT Player


/* Define Root (Composite-state) of Player*/
SME_BEGIN_ROOT_COMP_STATE_DEF(Player, PlayerEntry, PlayerExit)
	SME_ON_INIT_STATE(PlayerInitAction, PowerDown)
SME_END_STATE_DEF

/* Define state PowerDown */
//SME_BEGIN_LEAF_STATE_DEF(PowerDown, Player, PowerDownEntry, PowerDownExit)
SME_BEGIN_LEAF_STATE_DEF_P(PowerDown, PowerDownEntry, PowerDownExit)
	SME_ON_EVENT(EXT_EVENT_ID_POWER, OnPowerDownEXT_EVENT_ID_POWER, PowerUp) // Test Case: transit to a composite state
	//SME_ON_EVENT(EXT_EVENT_ID_POWER, OnPowerDownEXT_EVENT_ID_POWER, Cond1) // Test Case: transit to a conditional pseudo state
	//SME_ON_EVENT_WITH_GUARD(EXT_EVENT_ID_POWER, Guard1_func, OnPowerDownEXT_EVENT_ID_POWER, Join1) // Test Case: transit to a join pseudo state
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
    /* else */
	SME_ON_EVENT(SME_EVENT_COND_ELSE, CondActElse, PowerUp)
SME_END_STATE_DEF

/* Define Join */
SME_BEGIN_JOIN_STATE_DEF_P(Join1) /*Test Case: a pseudo state -> a pseudo state */
	SME_ON_JOIN_TRAN(JoinAct, Cond1)
SME_END_STATE_DEF



SME_APPLICATION_DEF(Player1,Player)

unsigned int m_Timer1=0;
unsigned int m_Timer2=0;

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

int Timer1Proc(SME_APP_T *pDestApp, unsigned long nSequenceNum)
{
	printf("Timer1 SeqNum=%d time out\n", nSequenceNum);
	return 0;
}

int OnTimer2Proc(SME_APP_T *pDestApp, SME_EVENT_T *pEvent)
{
	printf("Timer2 SeqNum=%d time out\n", pEvent->nSequenceNum);
	//XSleep(4000);// Test Case: Time-out event overflow
	return 0;
}


int PlayerInitAction(SME_APP_T *pApp, SME_EVENT_T *pEvent)
{
	printf("PlayerInitAction\n");
	return 0;
}

int PowerUpInitAction(SME_APP_T *pApp, SME_EVENT_T *pEvent)
{
	printf("PowerUpInitAction\n");
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


int PowerUpEntry(SME_APP_T *pApp, SME_EVENT_T *pEvent)
{
	printf("PowerUpEntry\n");
	return 0;
}


int PowerUpExit(SME_APP_T *pApp, SME_EVENT_T *pEvent)
{
	printf("PowerUpExit\n");
	return 0;
}


int PlayingEntry(SME_APP_T *pApp, SME_EVENT_T *pEvent)
{
	printf("PlayingEntry\n Start 2 Timers\n");
	m_Timer1 = XSetTimer(&SME_GET_APP_VAR(Player1), 3000, Timer1Proc);
	m_Timer2 = XSetEventTimer(&SME_GET_APP_VAR(Player1), 1000);

	SME_CATCH_STATES();
	return 0;
}


int PlayingExit(SME_APP_T *pApp, SME_EVENT_T *pEvent)
{
	printf("PlayingExit\n Stop 2 timers\n");
	if (m_Timer1)
		XKillTimer(m_Timer1);
	if (m_Timer2)
		XKillTimer(m_Timer2);

	return 0;
}


int PauseEntry(SME_APP_T *pApp, SME_EVENT_T *pEvent)
{
	printf("PauseEntry\n");
	return 0;
}


int PauseExit(SME_APP_T *pApp, SME_EVENT_T *pEvent)
{
	printf("PauseExit\n");
	return 0;
}


int OnPowerDownEXT_EVENT_ID_POWER(SME_APP_T *pApp, SME_EVENT_T *pEvent)
{
	printf("OnPowerDownEXT_EVENT_ID_POWER\n");
	return 0;
}


int OnPlayingEXT_EVENT_ID_PAUSE_RESUME(SME_APP_T *pApp, SME_EVENT_T *pEvent)
{
	printf("OnPlayingEXT_EVENT_ID_PAUSE_RESUME\n");
	return 0;
}


int OnPauseEXT_EVENT_ID_PAUSE_RESUME(SME_APP_T *pApp, SME_EVENT_T *pEvent)
{
	printf("OnPauseEXT_EVENT_ID_PAUSE_RESUME\n");
	return 0;
}


int OnPowerUpEXT_EVENT_ID_POWER(SME_APP_T *pApp, SME_EVENT_T *pEvent)
{
	printf("OnPowerUpEXT_EVENT_ID_POWER\n");
	return 0;
}

#endif //SME_CPP
