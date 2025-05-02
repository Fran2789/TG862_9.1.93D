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

enum
{
	/*{{SME_EVENT_ID_LIST_DECLARE*/
	// External event
	EXT_EVENT_ID_POWER=1,
	EXT_EVENT_ID_PAUSE_RESUME=2,
	/*}}SME_EVENT_ID_LIST_DECLARE*/
};

#ifdef __cplusplus
}
#endif

#endif

