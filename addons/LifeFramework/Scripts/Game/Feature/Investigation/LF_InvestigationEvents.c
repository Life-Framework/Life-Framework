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

//! The evidence locker is full enough that a convoy should collect it (LF_EvidenceLockerFill, #3378).
//! Raised once when the seized rows reach the threshold, or at once on a break-in at the locker.
class LF_OnEvidenceConvoyDue : LF_Event
{
	int m_iRows;
	int m_iThreshold;
	//! "full" (the threshold was crossed) or "break-in" (the locker was broken into).
	string m_sReason;

	static LF_OnEvidenceConvoyDue Create(int rows, int threshold, string reason)
	{
		LF_OnEvidenceConvoyDue e = new LF_OnEvidenceConvoyDue();
		e.m_iRows = rows;
		e.m_iThreshold = threshold;
		e.m_sReason = reason;
		return e;
	}
}
