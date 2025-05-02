

#include "sme.h"

#if !SME_CPP
	#include "Player_c.h"
	#include "Player2_c.h"
#else
	#include "Player.h"
#endif

#include "EventId.h"
#include "SingleState.h"
#include <stdio.h>
#include "sme_cross_platform.h"
#include "sme_debug.h"

#if SME_CPP
SME_HANDLER_CLASS_DEF(Player)
#endif

/*******************************************************************************
The followings could be defined in another file.
*/
/* Define composite state PowerUp */
#ifdef SME_CURR_DEFAULT_PARENT
#undef SME_CURR_DEFAULT_PARENT
#endif
#define SME_CURR_DEFAULT_PARENT PowerUp

SME_BEGIN_COMP_STATE_DEF(PowerUp, Player, PowerUpEntry, PowerUpExit)
	SME_ON_INIT_STATE(OnPowerUpInitChild, Playing)
SME_END_STATE_DEF

SME_BEGIN_LEAF_STATE_DEF_P(Playing, PlayingEntry, PlayingExit)
	SME_ON_EVENT(EXT_EVENT_ID_PAUSE_RESUME,OnPlayingEXT_EVENT_ID_PAUSE_RESUME,Pause)
	SME_ON_INTERNAL_TRAN_WITH_GUARD(SME_EVENT_TIMER,GuardTimer2_func,OnTimer2Proc) /* Regular timer event */
SME_END_STATE_DEF

SME_BEGIN_LEAF_STATE_DEF_P(Pause, PauseEntry, PauseExit)
	SME_ON_EVENT(EXT_EVENT_ID_PAUSE_RESUME,OnPauseEXT_EVENT_ID_PAUSE_RESUME,Playing)
SME_END_STATE_DEF

/****************************************************************
Player1&2, SingleState1&2, Timer1&2 run at threadcontext1
Player3&4, SingleState3&4, Timer3&4 run at threadcontext2
*/
#if SME_CPP
	SME_DEC_EXT_APP_VAR(Player1, Player)
	SME_DEC_EXT_APP_VAR(SingleState1, SingleState)
	SME_DEC_EXT_APP_VAR(Player2, Player)
	SME_DEC_EXT_APP_VAR(SingleState2, SingleState)
#else
	SME_DEC_EXT_APP_VAR(Player1)
	SME_DEC_EXT_APP_VAR(SingleState1)
	SME_DEC_EXT_APP_VAR(Player2)
	SME_DEC_EXT_APP_VAR(SingleState2)
#endif

unsigned int m_Timer1=0;
unsigned int m_Timer2=0;
unsigned int m_Timer3=0;
unsigned int m_Timer4=0;

#if !SME_CPP

int Timer1Proc(SME_APP_T *pDestApp, unsigned long nSequenceNum)
{
	printf("Timer SeqNum=%d time out\n", nSequenceNum);
	return 0;
}

int OnTimer2Proc(SME_APP_T *pDestApp, SME_EVENT_T *pEvent)
{
	printf("Timer SeqNum=%d time out\n", pEvent->nSequenceNum);
	//XSleep(4000);// Test Case: Time-out event overflow
	return 0;
}

int PlayingTimeOut(SME_APP_T *pDestApp, SME_EVENT_T *pEvent)
{
	printf("Playing State timeout SeqNum=%d\n", pEvent->nSequenceNum);
	return 0;
}

int PowerUpTimeOut(SME_APP_T *pDestApp, SME_EVENT_T *pEvent)
{
	printf("PowerUp State timeout SeqNum=%d\n", pEvent->nSequenceNum);
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

extern SME_THREAD_CONTEXT_T g_AppThreadContext1;
extern SME_THREAD_CONTEXT_T g_AppThreadContext2;


int PlayingEntry(SME_APP_T *pApp, SME_EVENT_T *pEvent)
{
	SME_APP_T *pPlayer=NULL;
	SME_APP_T *pSingleState=NULL;

	if (XGetThreadContext()==&g_AppThreadContext1)
	{
		pPlayer=&SME_GET_APP_VAR(Player1);
		pSingleState=&SME_GET_APP_VAR(SingleState1);

		m_Timer1 = XSetTimer(pPlayer, 3000, Timer1Proc);
		m_Timer2 = XSetEventTimer(pPlayer, 1000);

		printf("PlayingEntry\n Start Timer1 & Timer2 in context-1: Timer1-SeqNum=%d Timer2-SeqNum=%d\n", m_Timer1, m_Timer2);
	} else if (XGetThreadContext()==&g_AppThreadContext2)
	{
		pPlayer=&SME_GET_APP_VAR(Player2);
		pSingleState=&SME_GET_APP_VAR(SingleState2);

		m_Timer3 = XSetTimer(pPlayer, 3000, Timer1Proc);
		m_Timer4 = XSetEventTimer(pPlayer, 1000);

		printf("PlayingEntry\n Start Timer3 & Timer4 in context-2: Timer3-SeqNum=%d Timer4-SeqNum=%d\n", m_Timer3, m_Timer4);
	}

	//SME_CATCH_STATES();

	/* Test Case: Activate an corresponding application at current thread. */
	SmeActivateApp(pSingleState,pPlayer);

	return 0;
}


int PlayingExit(SME_APP_T *pApp, SME_EVENT_T *pEvent)
{
	SME_APP_T *pSingleState=NULL;
	if (XGetThreadContext()==&g_AppThreadContext1)
	{
		pSingleState=&SME_GET_APP_VAR(SingleState1);
		printf("PlayingExit\n Stop Timer1 and Timer2 in context-1\n");

		if (m_Timer1)
			XKillTimer(m_Timer1);
		if (m_Timer2)
			XKillTimer(m_Timer2);

	} else if (XGetThreadContext()==&g_AppThreadContext2)
	{
		pSingleState=&SME_GET_APP_VAR(SingleState2);
		printf("PlayingExit\n Stop Timer3 and Timer4 in context-2\n");

		if (m_Timer3)
			XKillTimer(m_Timer3);
		if (m_Timer4)
			XKillTimer(m_Timer4);
	}

	/* Test Case: De-activate an application */
	SmeDeactivateApp(pSingleState);
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

int OnPowerUpInitChild(SME_APP_T *pApp, SME_EVENT_T *pEvent)
{
	printf("OnPowerUpInitChild-Playing\n");
	return 0;
}

#else

int CPlayer::OnPowerUpInitChild(SME_APP_T *pApp, SME_EVENT_T *pEvent)
{
	printf("OnPowerUpInitChild-Playing\n");
	return 0;
}


#endif //SME_CPP

