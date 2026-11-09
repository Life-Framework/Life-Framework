//------------------------------------------------------------------------------------------------
//! Persisted form of a character's survival stats.
//!
//! A DEDICATED RECORD, NOT LF_SurvivalStats ITSELF - the live class is the mutable session object.
//! m_sPersistentId is the character persistence id; the floats mirror LF_SurvivalStats
//! (hunger/thirst, each 0-100). m_fHealth is retired (2026-11-08, M1-28): needs act on the
//! character's real health now. The slot stays so the save format is unchanged; it is written as
//! RETIRED_HEALTH and ignored on load.
//------------------------------------------------------------------------------------------------
class LF_SurvivalStatsRecord
{
	string m_sPersistentId;
	float m_fHunger;
	float m_fThirst;
	float m_fHealth;

	static const float RETIRED_HEALTH = 100;

	//------------------------------------------------------------------------------------------------
	//! \param[in] persistentId Character persistence id.
	//! \param[in] hunger Hunger value (0-100).
	//! \param[in] thirst Thirst value (0-100).
	//! \param[in] health Retired slot; pass RETIRED_HEALTH.
	//! \return A fully populated survival stats record.
	static LF_SurvivalStatsRecord Create(string persistentId, float hunger, float thirst, float health)
	{
		LF_SurvivalStatsRecord record();
		record.m_sPersistentId = persistentId;
		record.m_fHunger = hunger;
		record.m_fThirst = thirst;
		record.m_fHealth = health;
		return record;
	}
}