//------------------------------------------------------------------------------------------------
//! A suspect's look at the crime (LF_WantedLook) for one report (#758): the "wantedlooks" payload
//! (save version 29) of LF_PersistenceComponentSerializer. Keyed by the report's sequence number,
//! which the "wanted" payload already saves, so the portrait on the wanted board's face comes back
//! after a restart. A separate payload because the "wanted" records are positional: their fields
//! never change.
//!
//! Positional in a binary save: never add, remove or reorder a field. A new field is a new payload.
class LF_WantedLookRow
{
	int m_iSequence;
	string m_sAppearance;

	//------------------------------------------------------------------------------------------------
	static LF_WantedLookRow Create(int sequence, string appearance)
	{
		LF_WantedLookRow row = new LF_WantedLookRow();
		row.m_iSequence = sequence;
		row.m_sAppearance = appearance;
		return row;
	}
}
