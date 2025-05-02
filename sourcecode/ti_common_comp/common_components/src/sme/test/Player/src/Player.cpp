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

#if SME_CPP

#include "Player.h"
#include "EventId.h"
#include <stdio.h>
#include "sme_cross_platform.h"

SME_HANDLER_CLASS_DEF(Player)


/* Define Root (Composite-state) of Player*/
SME_BEGIN_ROOT_COMP_STATE_DEF(Player, PlayerEntry, PlayerExit)
	SME_ON_INIT_STATE(SME_NULL_ACTION, PowerDown)
SME_END_STATE_DEF

/* Define state PowerDown */
SME_BEGIN_LEAF_STATE_DEF(PowerDown, Player, PowerDownEntry, PowerDownExit)
	//SME_ON_EVENT(EXT_EVENT_ID_POWER, OnPowerDownEXT_EVENT_ID_POWER, PowerUp) // Test Case: transit to a composite state
	//SME_ON_EVENT(EXT_EVENT_ID_POWER, OnPowerDownEXT_EVENT_ID_POWER, Cond1) // Test Case: transit to a conditional pseudo state
	SME_ON_EVENT_WITH_GUARD(EXT_EVENT_ID_POWER, Guard1_func, OnPowerDownEXT_EVENT_ID_POWER, Join1) // Test Case: transit to a join pseudo state
SME_END_STATE_DEF

SME_BEGIN_SUB_STATE_DEF(PowerUp, Player)
	SME_ON_EVENT(EXT_EVENT_ID_POWER,OnPowerUpEXT_EVENT_ID_POWER,PowerDown)
SME_END_STATE_DEF

/*******************************************************************************
The followings could be defined in another file.
*/
/* Define composite state PowerUp */
SME_BEGIN_COMP_STATE_DEF(PowerUp, Player, PowerUpEntry, PowerUpExit)
	SME_ON_INIT_STATE(SME_NULL_ACTION, Playing)
SME_END_STATE_DEF

SME_BEGIN_LEAF_STATE_DEF(Playing, PowerUp, PlayingEntry, PlayingExit)
	SME_ON_EVENT(EXT_EVENT_ID_PAUSE_RESUME,OnPlayingEXT_EVENT_ID_PAUSE_RESUME,Pause)
	SME_ON_INTERNAL_TRAN_WITH_GUARD(SME_EVENT_TIMER,GuardTimer2_func,OnTimer2Proc)
SME_END_STATE_DEF

SME_BEGIN_LEAF_STATE_DEF(Pause, PowerUp, PauseEntry, PauseExit)
	SME_ON_EVENT(EXT_EVENT_ID_PAUSE_RESUME,OnPauseEXT_EVENT_ID_PAUSE_RESUME,Playing)
SME_END_STATE_DEF

enum {COND_EV_COND1, COND_EV_COND2};
/******************************************************************************** */
/* Define Conditional Note: No Guard for pseudo state.*/
SME_BEGIN_COND_STATE_DEF(Cond1, Player, Cond1_func)
	SME_ON_EVENT(COND_EV_COND1, CondAct1, Playing) 
	SME_ON_EVENT(COND_EV_COND2, CondAct2, Pause)
    /* else */
	SME_ON_EVENT(SME_EVENT_COND_ELSE, CondActElse, PowerUp)
SME_END_STATE_DEF

/* Define Join */
SME_BEGIN_JOIN_STATE_DEF(Join1, Player) /*Test Case: a pseudo state -> a pseudo state */
	SME_ON_JOIN_TRAN(JoinAct, Cond1)
SME_END_STATE_DEF


/*{{SME_DEC_IMP_SEPARATOR}}*/

SME_APPLICATION_DEF(Player1, Player)

BOOL CPlayer::Guard1_func(SME_APP_T *pDestApp, SME_EVENT_T *pEvent)
{
	printf("Guard1_func\n");
	return TRUE;
}

BOOL CPlayer::GuardTimer2_func(SME_APP_T *pDestApp, SME_EVENT_T *pEvent)
{
	//return TRUE;
	return FALSE; /* Filter out timer2 events. */
}

int CPlayer::Cond1_func(SME_APP_T *pDestApp, SME_EVENT_T *pEvent)
{
	printf("Cond1_func\n");
	return SME_EVENT_COND_ELSE; 
}

int CPlayer::CondAct1(SME_APP_T *pDestApp, SME_EVENT_T *pEvent)
{
	printf("CondAct1c\n");
	return 0;
}

int CPlayer::CondAct2(SME_APP_T *pDestApp, SME_EVENT_T *pEvent)
{
	printf("CondAct2\n");
	return 0;
}

int CPlayer::CondActElse(SME_APP_T *pDestApp, SME_EVENT_T *pEvent)
{
	printf("CondActElse\n");
	return 0;
}

int CPlayer::JoinAct(SME_APP_T *pDestApp, SME_EVENT_T *pEvent)
{
	printf("JoinAct\n");
	return 0;
}

int Timer1Proc(SME_APP_T *pDestApp, unsigned long nSequenceNum)
{
	printf("Timer1 SeqNum=%d time out\n", nSequenceNum);
	//XSleep(3000); // Test case: Timer event overflow
	return 0;
}

int CPlayer::OnTimer2Proc(SME_APP_T *pDestApp, SME_EVENT_T *pEvent)
{
	printf("Timer2 SeqNum=%d time out\n", pEvent->nSequenceNum);
	return 0;
}


int CPlayer::PlayerEntry(SME_APP_T *pApp, SME_EVENT_T *pEvent)
{
	printf("PlayerEntry\n");
	return 0;
}

int CPlayer::PlayerExit(SME_APP_T *pApp, SME_EVENT_T *pEvent)
{
	printf("PlayerExit\n");
	return 0;
}


int CPlayer::PowerDownEntry(SME_APP_T *pApp, SME_EVENT_T *pEvent)
{
	printf("PowerDownEntry\n");
	return 0;
}


int CPlayer::PowerDownExit(SME_APP_T *pApp, SME_EVENT_T *pEvent)
{
	printf("PowerDownExit\n");
	return 0;
}


int CPlayer::PowerUpEntry(SME_APP_T *pApp, SME_EVENT_T *pEvent)
{
	printf("PowerUpEntry\n");
	return 0;
}


int CPlayer::PowerUpExit(SME_APP_T *pApp, SME_EVENT_T *pEvent)
{
	printf("PowerUpExit\n");
	return 0;
}


int CPlayer::PlayingEntry(SME_APP_T *pApp, SME_EVENT_T *pEvent)
{
	printf("PlayingEntry\n Start 2 Timers\n");
	m_Timer1 = XSetTimer(this, 3000, Timer1Proc, &m_TimerSeq1);
	m_Timer2 = XSetEventTimer(this, 1000, &m_TimerSeq2);
	return 0;
}


int CPlayer::PlayingExit(SME_APP_T *pApp, SME_EVENT_T *pEvent)
{
	printf("PlayingExit\n Stop 2 timers\n");
	if (m_Timer1)
		XKillTimer(m_Timer1);
	if (m_Timer2)
		XKillTimer(m_Timer2);

	return 0;
}


int CPlayer::PauseEntry(SME_APP_T *pApp, SME_EVENT_T *pEvent)
{
	printf("PauseEntry\n");
	return 0;
}


int CPlayer::PauseExit(SME_APP_T *pApp, SME_EVENT_T *pEvent)
{
	printf("PauseExit\n");
	return 0;
}


int CPlayer::OnPowerDownEXT_EVENT_ID_POWER(SME_APP_T *pApp, SME_EVENT_T *pEvent)
{
	printf("OnPowerDownEXT_EVENT_ID_POWER\n");
	return 0;
}


int CPlayer::OnPlayingEXT_EVENT_ID_PAUSE_RESUME(SME_APP_T *pApp, SME_EVENT_T *pEvent)
{
	printf("OnPlayingEXT_EVENT_ID_PAUSE_RESUME\n");
	return 0;
}


int CPlayer::OnPauseEXT_EVENT_ID_PAUSE_RESUME(SME_APP_T *pApp, SME_EVENT_T *pEvent)
{
	printf("OnPauseEXT_EVENT_ID_PAUSE_RESUME\n");
	return 0;
}


int CPlayer::OnPowerUpEXT_EVENT_ID_POWER(SME_APP_T *pApp, SME_EVENT_T *pEvent)
{
	// Memorize the history state of the composite state PowerUp
	//SME_UPDATE_STATE_TRAN_DEST(PowerDown, 2, pApp->pHistoryState)
	
	printf("CPlayer::OnPowerUpEXT_EVENT_ID_POWER\n");
	return 0;
}

//////////////////////////
int CSuperPlayer::OnPauseEXT_EVENT_ID_PAUSE_RESUME(SME_APP_T *pApp, SME_EVENT_T *pEvent)
{
	printf("OnPauseEXT_EVENT_ID_PAUSE_RESUME\n");
	return 0;
}


int CSuperPlayer::OnPowerUpEXT_EVENT_ID_POWER(SME_APP_T *pApp, SME_EVENT_T *pEvent)
{

	printf("CSuperPlayer::OnPowerUpEXT_EVENT_ID_POWER\n");
	
	return 0;
}

#endif //SME_CPP