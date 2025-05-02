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

/************* A new file **************/
/* Define Orthogonal Sub State */

/*
SME_DEC_EXT_APP_VAR(SingleState1)

SME_BEGIN_ORTHO_COMP_STATE_DEF(OrthoState, Player)	
SME_REGION_DEF(SingleState1,SME_RUN_MODE_PARENT_THREAD,0)
SME_REGION_DEF(SingleState1,SME_RUN_MODE_PARENT_THREAD,0)
SME_END_ORTHO_STATE_DEF
*/

#endif