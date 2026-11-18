//------------------------------------------------------------------------------------------------
//! Persisted standing of one character with one faction (#2698): the "reputation" payload (save
//! version 53) of LF_PersistenceComponentSerializer (docs/architecture/reputation.md section 6).
//! Keyed by the character record key (LF_Utils.GetPlayerUID), never an entity. The tier is never
//! saved: the faction's thresholds derive it on every read, and the points clamp to them on load.
//!
//! Positional in a binary save: never add, remove or reorder a field. A new field is a new payload.
class LF_ReputationRow
{
	//! Row format, written first. 1 is this shape.
	int m_iVersion;
	string m_sCharacterKey;
	string m_sFaction;
	int m_iPoints;

	static const int ROW_VERSION = 1;

	//------------------------------------------------------------------------------------------------
	static LF_ReputationRow Create(string characterKey, string faction, int points)
	{
		LF_ReputationRow row = new LF_ReputationRow();
		row.m_iVersion = ROW_VERSION;
		row.m_sCharacterKey = characterKey;
		row.m_sFaction = faction;
		row.m_iPoints = points;
		return row;
	}
}
