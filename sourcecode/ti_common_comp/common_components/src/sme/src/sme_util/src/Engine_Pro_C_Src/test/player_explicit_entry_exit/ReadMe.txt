========================================================================
       CONSOLE APPLICATION : Player_Explicit_Entry_Exit in C version.
========================================================================
Version: 
========================================================================

A state machine sample running on Linux/Win32 platforms

There are two running threads in this sample: the state machine application thread and the external event trigger thread.

The State Trees:

Player Application:
   --->PowerDown (init)
   --->PowerUp
         --->Playing
		 --->Pause

A state machine with a single state.
SingleState Application:

This sample demonstrates the following features:

  1) Built-in Timers running at two modes: callback function and event trigger.
  2) History Transition
  3) Player state machine application with C standard and C++ standard version depending on preprocessor definition SME_CPP.
  4) Event loop on Linux/Win32 platforms.
  5) Separate state machine definitions to different source files.
  6) Explicit Entry to the PowerDown state to the PowerUp composite state from the PowerDown state.

In the composite Player definition module, define a transition from the PowerDown state to the PowerUp composite state 
on EXT_EVENT_ID_POWER event.

/* Define Root (Composite-state) of Player*/
SME_BEGIN_ROOT_COMP_STATE_DEF(Player, PlayerEntry, PlayerExit)
	SME_ON_INIT_STATE(SME_NULL_ACTION, PowerDown)
SME_END_STATE_DEF

/* Define state PowerDown */
SME_BEGIN_LEAF_STATE_DEF_P(PowerDown, PowerDownEntry, PowerDownExit)
	SME_ON_EVENT(EXT_EVENT_ID_POWER, OnPowerDownEXT_EVENT_ID_POWER, PowerUp) // Test Case: transit to a composite state
SME_END_STATE_DEF

In the composite PowerUp definition module, declare an explicit entry to Pause child state of PowerUp, so that  
SME_ON_EXPLICIT_ENTRY makes "EXT_EVENT_ID_POWER" an explicit entry to the Pause child state instead of 
an default entry to the Playing initial child state.   

SME_BEGIN_COMP_STATE_DEF(PowerUp, Player, PowerUpEntry, PowerUpExit)
	SME_ON_INIT_STATE(SME_NULL_ACTION, Playing)
	SME_ON_EXPLICIT_ENTRY(EXT_EVENT_ID_POWER, Pause)
	SME_EXPLICIT_EXIT_FROM(EXT_EVENT_ID_POWER, Playing)
SME_END_STATE_DEF

  7) Explicit Exit from the Playing state to the PowerDown state:

In the composite Player definition module, define a transition from the PowerUp composite state to the PowerDown state 
on EXT_EVENT_ID_POWER event.

/* Define Root (Composite-state) of Player*/
SME_BEGIN_ROOT_COMP_STATE_DEF(Player, PlayerEntry, PlayerExit)
	SME_ON_INIT_STATE(SME_NULL_ACTION, PowerDown)
SME_END_STATE_DEF

SME_BEGIN_SUB_STATE_DEF_P(PowerUp)
	SME_ON_EVENT(EXT_EVENT_ID_POWER,OnPowerUpEXT_EVENT_ID_POWER,PowerDown)
SME_END_STATE_DEF


In the composite PowerUp definition module, declare an explicit exit from Playing state on EXT_EVENT_ID_POWER, so that  
SME_EXPLICIT_EXIT_FROM makes "EXT_EVENT_ID_POWER" an explicit event going out of the State Playing instead of 
a transition from all children of the State PowerUp.   

SME_BEGIN_COMP_STATE_DEF(PowerUp, Player, PowerUpEntry, PowerUpExit)
	SME_ON_INIT_STATE(SME_NULL_ACTION, Playing)
	SME_ON_EXPLICIT_ENTRY(EXT_EVENT_ID_POWER, Pause)
	/* The following makes "EXT_EVENT_ID_POWER" an explicit event going out of the State Playing instead of 
	a "transition from all children of the State PowerUp" */
	SME_EXPLICIT_EXIT_FROM(EXT_EVENT_ID_POWER, Playing)
SME_END_STATE_DEF






/////////////////////////////////////////////////////////////////////////////
