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
#else
	#include "Player.h"
#endif

#include "EventId.h"
#include <stdio.h>
#include "sme_cross_platform.h"
#include "sme_debug.h"

#ifdef SME_CURR_DEFAULT_PARENT
#undef SME_CURR_DEFAULT_PARENT
#endif
#define SME_CURR_DEFAULT_PARENT Player

// Comment the following statements for C version.
//#if SME_CPP
//SME_HANDLER_CLASS_DEF(Player)
//#endif

/* Define Root (Composite-state) of Player*/
SME_BEGIN_ROOT_COMP_STATE_DEF(SME_CURR_DEFAULT_PARENT, PlayerEntry, PlayerExit)
	SME_ON_INIT_STATE(SME_NULL_ACTION, PowerDown)
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

SME_APPLICATION_DEF(Player1, Player)
SME_APPLICATION_DEF(Player2, Player)

#if !SME_CPP

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

#else

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
	m_Timer1 = XSetTimer(this, 3000, Timer1Proc);
	m_Timer2 = XSetEventTimer(this, 1000);
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
