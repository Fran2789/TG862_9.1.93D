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
#include "SingleState.h"

/************* A new file **************/
/* Define Orthogonal Sub State */

SME_DEC_EXT_APP_VAR(SingleState1)
SME_DEC_EXT_APP_VAR(Player)

int OrthoStateEntry(SME_APP_T *pApp, SME_EVENT_T *pEvent);
int OrthoStateExit(SME_APP_T *pApp, SME_EVENT_T *pEvent);


SME_BEGIN_ORTHO_COMP_STATE_DEF(OrthoState, Player, OrthoStateEntry, OrthoStateExit)	
SME_REGION_DEF(SingleStateReg1,SingleState,SME_RUN_MODE_PARENT_THREAD,0)
SME_REGION_DEF(SingleStateReg2,SingleState,SME_RUN_MODE_PARENT_THREAD,0)
//SME_REGION_DEF(PlayerReg1,Player,SME_RUN_MODE_SEPARATE_THREAD,0)
//SME_REGION_DEF(PlayerReg2,Player,SME_RUN_MODE_SEPARATE_THREAD,0)
SME_MULTI_REGION_DEF(PlayerReg,2,Player,SME_RUN_MODE_SEPARATE_THREAD,0)
SME_END_ORTHO_STATE_DEF


int OrthoStateEntry(SME_APP_T *pApp, SME_EVENT_T *pEvent)
{
	printf("OrthoStateEntry\n");
	return 0;
}


int OrthoStateExit(SME_APP_T *pApp, SME_EVENT_T *pEvent)
{
	printf("OrthoStateExit\n");
	return 0;
}


#endif