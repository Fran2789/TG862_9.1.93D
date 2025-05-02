/*
FILE:Player.h
NOTE: The State Machine Wizard will add mapping macros between /*{{ and /*}}. Do NOT modify manually.
*/

#ifndef PLAYER_H
#define PLAYER_H

#include "sme.h"


#ifdef __cplusplus
extern "C" {
#endif

class CPlayer: public SME_APP_T
{
public:
	CPlayer(const char* _sAppName, SME_STATE_T *_pRoot):SME_APP_T(_sAppName, _pRoot) {
		m_Timer1 =0;
		m_Timer2 =0;
	};

	unsigned int m_Timer1;
	unsigned long m_TimerSeq1;
	unsigned int m_Timer2;
	unsigned long m_TimerSeq2;

SME_BEGIN_CLASS_MEMBER_DECLARE(Player)
	SME_LEAF_STATE_DECLARE(PowerDown)
	SME_COMP_STATE_DECLARE(PowerUp)
	SME_LEAF_STATE_DECLARE(Playing)
	SME_LEAF_STATE_DECLARE(Pause)
	SME_LEAF_STATE_DECLARE(Cond1)
	SME_LEAF_STATE_DECLARE(Join1)
SME_END_CLASS_MEMBER_DECLARE

/*{{SME_BEGIN_EVENT_HANDLER(Player)*/
int OnTimer2Proc(SME_APP_T *pApp, SME_EVENT_T *pEvent);
int PlayerEntry(SME_APP_T *pApp, SME_EVENT_T *pEvent);
int PlayerExit(SME_APP_T *pApp, SME_EVENT_T *pEvent);
int PowerDownEntry(SME_APP_T *pApp, SME_EVENT_T *pEvent);
int PowerDownExit(SME_APP_T *pApp, SME_EVENT_T *pEvent);
int PowerUpEntry(SME_APP_T *pApp, SME_EVENT_T *pEvent);
int PowerUpExit(SME_APP_T *pApp, SME_EVENT_T *pEvent);
int PlayingEntry(SME_APP_T *pApp, SME_EVENT_T *pEvent);
int PlayingExit(SME_APP_T *pApp, SME_EVENT_T *pEvent);
int PauseEntry(SME_APP_T *pApp, SME_EVENT_T *pEvent);
int PauseExit(SME_APP_T *pApp, SME_EVENT_T *pEvent);
int OnPowerDownEXT_EVENT_ID_POWER(SME_APP_T *pApp, SME_EVENT_T *pEvent);
int OnPlayingEXT_EVENT_ID_PAUSE_RESUME(SME_APP_T *pApp, SME_EVENT_T *pEvent);
virtual int OnPauseEXT_EVENT_ID_PAUSE_RESUME(SME_APP_T *pApp, SME_EVENT_T *pEvent);
virtual int OnPowerUpEXT_EVENT_ID_POWER(SME_APP_T *pApp, SME_EVENT_T *pEvent);


int Cond1_func(SME_APP_T *pDestApp, SME_EVENT_T *pEvent);
int CondAct1(SME_APP_T *pDestApp, SME_EVENT_T *pEvent);
int CondAct2(SME_APP_T *pDestApp, SME_EVENT_T *pEvent);
int CondActElse(SME_APP_T *pDestApp, SME_EVENT_T *pEvent);

int JoinAct(SME_APP_T *pDestApp, SME_EVENT_T *pEvent);
BOOL Guard1_func(SME_APP_T *pDestApp, SME_EVENT_T *pEvent);
BOOL GuardTimer2_func(SME_APP_T *pDestApp, SME_EVENT_T *pEvent);

/*}}SME_END_EVENT_HANDLER*/

};

class CSuperPlayer: public CPlayer
{
public:
	CSuperPlayer(const char* _sAppName, SME_STATE_T *_pRoot):CPlayer(_sAppName, _pRoot) {};

	virtual int OnPauseEXT_EVENT_ID_PAUSE_RESUME(SME_APP_T *pApp, SME_EVENT_T *pEvent);
	virtual int OnPowerUpEXT_EVENT_ID_POWER(SME_APP_T *pApp, SME_EVENT_T *pEvent);

};

#ifdef __cplusplus
}
#endif

#endif

