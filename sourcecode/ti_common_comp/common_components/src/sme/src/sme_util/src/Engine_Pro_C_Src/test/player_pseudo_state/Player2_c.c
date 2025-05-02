

#include "sme.h"

#if !SME_CPP


#include "Player_c.h"
#include "Player2_c.h"
#include "EventId.h"
#include <stdio.h>
#include "sme_cross_platform.h"
#include "sme_debug.h"

/*******************************************************************************
The followings could be defined in another file.
*/
/* Define composite state PowerUp */
#ifdef SME_CURR_DEFAULT_PARENT
#undef SME_CURR_DEFAULT_PARENT
#endif
#define SME_CURR_DEFAULT_PARENT PowerUp

SME_BEGIN_COMP_STATE_DEF(PowerUp, Player, PowerUpEntry, PowerUpExit)
	SME_ON_INIT_STATE(SME_NULL_ACTION, Playing)
SME_END_STATE_DEF

SME_BEGIN_LEAF_STATE_DEF_P(Playing, PlayingEntry, PlayingExit)
	SME_ON_EVENT(EXT_EVENT_ID_PAUSE_RESUME,OnPlayingEXT_EVENT_ID_PAUSE_RESUME,Pause)
	SME_ON_INTERNAL_TRAN_WITH_GUARD(SME_EVENT_TIMER,GuardTimer2_func,OnTimer2Proc)
SME_END_STATE_DEF

SME_BEGIN_LEAF_STATE_DEF_P(Pause, PauseEntry, PauseExit)
	SME_ON_EVENT(EXT_EVENT_ID_PAUSE_RESUME,OnPauseEXT_EVENT_ID_PAUSE_RESUME,Playing)
SME_END_STATE_DEF


/*****************************************************************/
#if SME_CPP
	SME_DEC_EXT_APP_VAR(Player1, Player)
	SME_DEC_EXT_APP_VAR(SingleState1, SingleState)
#else
	SME_DEC_EXT_APP_VAR(Player1)
	SME_DEC_EXT_APP_VAR(SingleState1)
#endif

unsigned int m_Timer1=0;
unsigned int m_Timer2=0;


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

	/* Test Case: Activate an application */
	SmeActivateApp(&SME_GET_APP_VAR(SingleState1),&SME_GET_APP_VAR(Player1));

	return 0;
}


int PlayingExit(SME_APP_T *pApp, SME_EVENT_T *pEvent)
{
	printf("PlayingExit\n Stop 2 timers\n");
	if (m_Timer1)
		XKillTimer(m_Timer1);
	if (m_Timer2)
		XKillTimer(m_Timer2);

	/* Test Case: De-activate an application */
	SmeDeactivateApp(&SME_GET_APP_VAR(SingleState1));
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

