//------------------------------------------------------------------------------------------------
//! The ambulance's siren (#923). Two things the police car does not need:
//!  - the toggle sits with the vehicle's other light controls (the `light_switch` context of
//!    LF_M997_Ambulance.et, where the vanilla lights, hazards and high beams are) as a pair of
//!    LF_SirenModeAction entries with m_bDutyOnly, offered to an on-duty medic in the driver's seat;
//!  - its own sound, a 1980s European two-tone hi-lo (Sounds/Siren/Ambulance/, made by the asset
//!    pipeline's sound stage: tools/assets/ambulance-siren/sounds.json), played by
//!    LF_SirenManagerComponent as a loop on the vehicle in every mode but default. The shared
//!    police wail (Vehicles_Siren.acp) stays silent on a vehicle that sets m_sLoopProject.
//! Rules here are pure; the manager and the action call them.
//------------------------------------------------------------------------------------------------
class LF_AmbulanceSiren
{
	static const string FEATURE = "Siren";

	//! The mode the "on" action selects and the one the "off" action returns to.
	static const string MODE_ON = "Mode2";
	static const string MODE_OFF = "default";

	//------------------------------------------------------------------------------------------------
	//! Whether a vehicle's own siren loop should play: it has one, and the mode is not the off mode.
	static bool LoopWanted(bool hasOwnLoop, string modeName)
	{
		return hasOwnLoop && modeName != MODE_OFF;
	}
}

//------------------------------------------------------------------------------------------------
//! What a siren action looks like to one user.
enum LF_ESirenActionView
{
	HIDDEN,
	SHOWN,
	NEEDS_DUTY
}

//------------------------------------------------------------------------------------------------
//! Pure rule for the siren action's menu row, shared by LF_SirenModeAction and the LOGIC test.
class LF_SirenActionRules
{
	//! A user who is not seated where the action applies, or whose mode already runs, sees nothing; a
	//! seated user who may not run the siren sees the row greyed with the reason, never an empty menu.
	static LF_ESirenActionView View(bool seatAllows, bool mayOperate, bool modeRunning)
	{
		if (!seatAllows || modeRunning)
			return LF_ESirenActionView.HIDDEN;

		if (!mayOperate)
			return LF_ESirenActionView.NEEDS_DUTY;

		return LF_ESirenActionView.SHOWN;
	}
}
