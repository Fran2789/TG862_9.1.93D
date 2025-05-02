========================================================================
       CONSOLE APPLICATION : Player_Pseudo_State
========================================================================
Version: 
========================================================================

A state machine sample running on Linux/Win32 platforms

This sample demonstrates the following features:

  1) Built-in Timers running at two modes: callback function and event trigger.
  2) History Transition
  3) Player state machine application with C standard and C++ standard version depending on preprocessor definition SME_CPP.
  4) Event loop on Linux/Win32 platforms.
  5) Separate state machine definitions to different source files.
  6) Conditional Pseudo State
  7) Join Pseudo State
  8) Guard the internal transition from a time out event. 
  9) Guard an transition from PowerDown to PowerUp
  10) Activate an application, deactiate an application.
  11) Thread Local Storage
  12) Make a state as temporary state for debugging.


	//////////////////////////////////////////////////////////////////////////////////////////
	// Define a dummy root state, for example.

	#ifdef SME_CURR_DEFAULT_PARENT
	#undef SME_CURR_DEFAULT_PARENT
	#endif
	#define SME_CURR_DEFAULT_PARENT DummyRoot

	SME_BEGIN_ROOT_COMP_STATE_DEF(SME_CURR_DEFAULT_PARENT, SME_NULL_ACTION, SME_NULL_ACTION)
		SME_ON_INIT_STATE(SME_NULL_ACTION, Player)
	SME_END_STATE_DEF

	SME_BEGIN_SUB_STATE_DEF_P(Player)
	SME_END_STATE_DEF

	//////////////////////////////////////////////////////////////////////////////////////////
	// Define a composite state Player

	#ifdef SME_CURR_DEFAULT_PARENT
	#undef SME_CURR_DEFAULT_PARENT
	#endif
	#define SME_CURR_DEFAULT_PARENT Player

	SME_BEGIN_COMP_STATE_DEF(SME_CURR_DEFAULT_PARENT, DummyRoot, PlayerEntry, PlayerExit)
		SME_ON_INIT_STATE(SME_NULL_ACTION, PowerDown)
	SME_END_STATE_DEF


	// Define an application based on the the state DummyRoot
	SME_APPLICATION_DEF(Player1, DummyRoot)


	// Make the Player stae as a temporary root instead of the DummpRoot.
	SME_MAKE_TEMP_ROOT(&SME_GET_APP_VAR(Player1), &SME_STATE_REF(Player));
	SmeActivateApp(&SME_GET_APP_VAR(Player1),NULL);



There are two running threads in this sample: the state machine application thread and the external event trigger thread.


State Chart of Player Application:

PowerDown -> Join1 -> Cond1 -> PowerUp -> 
    ^                                   |  
    -------------------------------------

PowerUp (Playing <---> Pause)


A state machine with a single state.

/////////////////////////////////////////////////////////////////////////////
