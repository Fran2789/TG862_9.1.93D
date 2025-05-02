/*
FILE:Player_c.h
NOTE: The State Machine Wizard will add mapping macros between /*{{ and /*}}. Do NOT modify manually.
*/

#ifndef PLAYER_H
#define PLAYER_H

#include "sme.h"


#ifdef __cplusplus
extern "C" {
#endif

/* Pre-declare states that are referenced before they are defined */
SME_BEGIN_STATE_DECLARE(Player)
	SME_COMP_STATE_DECLARE(Player)
	SME_COMP_STATE_DECLARE(PlayerUp)
	SME_LEAF_STATE_DECLARE(PowerDown)
	SME_PSEUDO_STATE_DECLARE(Cond1)
	SME_PSEUDO_STATE_DECLARE(Join1)
SME_END_STATE_DECLARE


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
int OnPauseEXT_EVENT_ID_PAUSE_RESUME(SME_APP_T *pApp, SME_EVENT_T *pEvent);
int OnPowerUpEXT_EVENT_ID_POWER(SME_APP_T *pApp, SME_EVENT_T *pEvent);

int Cond1_func(SME_APP_T *pDestApp, SME_EVENT_T *pEvent);
int CondAct1(SME_APP_T *pDestApp, SME_EVENT_T *pEvent);
int CondAct2(SME_APP_T *pDestApp, SME_EVENT_T *pEvent);
int CondActElse(SME_APP_T *pDestApp, SME_EVENT_T *pEvent);

int JoinAct(SME_APP_T *pDestApp, SME_EVENT_T *pEvent);
BOOL Guard1_func(SME_APP_T *pDestApp, SME_EVENT_T *pEvent);
BOOL GuardTimer2_func(SME_APP_T *pDestApp, SME_EVENT_T *pEvent);

/*}}SME_END_EVENT_HANDLER*/

#ifdef __cplusplus
}
#endif

#endif

