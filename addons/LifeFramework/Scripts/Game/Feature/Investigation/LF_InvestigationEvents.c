//------------------------------------------------------------------------------------------------
//! Investigation events on the server bus (docs/architecture/events.md). Fields are plain values: uids and
//! ints, never IEntity or EntityID. Published on the server only, after the state change happened.
//! A new event of this feature is appended to this file, not to a central list.

//! A witness described a suspect (LF_Witness): from the payphone or to an officer in person.
//! Carries what the witness could see, never the suspect's name.
class LF_OnWitnessStatement : LF_Event
{
	int m_iReport;
	//! The player witness; empty for a town NPC.
	string m_sWitnessUid;
	bool m_bNpc;
	//! "payphone" or "officer".
	string m_sChannel;
	string m_sClothing;
	string m_sVehicle;
	string m_sPlateFragment;

	static LF_OnWitnessStatement Create(int report, string witnessUid, bool npc, string channel, string clothing, string vehicle, string plateFragment)
	{
		LF_OnWitnessStatement e = new LF_OnWitnessStatement();
		e.m_iReport = report;
		e.m_sWitnessUid = witnessUid;
		e.m_bNpc = npc;
		e.m_sChannel = channel;
		e.m_sClothing = clothing;
		e.m_sVehicle = vehicle;
		e.m_sPlateFragment = plateFragment;
		return e;
	}
}
