/*
FILE:Player2_c.h
*/

#ifndef PLAYER2_H
#define PLAYER2_H

#include "sme.h"


#ifdef __cplusplus
extern "C" {
#endif

/* Pre-declare states that are referenced before they are defined */

SME_BEGIN_STATE_DECLARE(PowerUp)
	SME_LEAF_STATE_DECLARE(Playing)
	SME_LEAF_STATE_DECLARE(Pause)
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

