/*
FILE:EventId.h
NOTE: The State Machine Wizard will add mapping macros between /*{{ and /*}}. Do NOT modify manually.
*/

#ifndef EVENTID_H
#define EVENTID_H

#include "sme.h"

#ifdef __cplusplus
extern "C" {
#endif

SME_BEGIN_EVENT_ID_LIST_DECLARE
	EXT_EVENT_ID_POWER=1,
	EXT_EVENT_ID_PAUSE_RESUME=2
SME_END_EVENT_ID_LIST_DECLARE


#ifdef __cplusplus
}
#endif

#endif

