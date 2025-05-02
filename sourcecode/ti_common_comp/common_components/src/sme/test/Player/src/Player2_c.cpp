

#include "sme.h"

#if !SME_CPP


#include "Player_c.h"
#include "Player2_c.h"
#include "EventId.h"
#include <stdio.h>
#include "sme_cross_platform.h"

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
	SME_ON_EXPLICIT_ENTRY(EXT_EVENT_ID_POWER, Pause)
	/* The following makes "EXT_EVENT_ID_POWER" an explicit event going out of the State Playing instead of 
	a "transition from all children of the State PowerUp" */
	SME_EXPLICIT_EXIT_FROM(EXT_EVENT_ID_POWER, Playing)
SME_END_STATE_DEF

SME_BEGIN_LEAF_STATE_DEF_P(Playing, PlayingEntry, PlayingExit)
	SME_ON_EVENT(EXT_EVENT_ID_PAUSE_RESUME,OnPlayingEXT_EVENT_ID_PAUSE_RESUME,Pause)
	SME_ON_INTERNAL_TRAN_WITH_GUARD(SME_EVENT_TIMER,GuardTimer2_func,OnTimer2Proc)
SME_END_STATE_DEF

SME_BEGIN_LEAF_STATE_DEF_P(Pause, PauseEntry, PauseExit)
	SME_ON_EVENT(EXT_EVENT_ID_PAUSE_RESUME,OnPauseEXT_EVENT_ID_PAUSE_RESUME,Playing)
SME_END_STATE_DEF

#endif //SME_CPP

