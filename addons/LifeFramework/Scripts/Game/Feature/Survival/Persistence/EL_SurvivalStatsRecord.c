//------------------------------------------------------------------------------------------------
//! Persisted form of a character's survival stats.
//!
//! A DEDICATED RECORD, NOT EL_SurvivalStats ITSELF - the live class is the mutable session object.
//! m_sPersistentId is the character persistence id; the floats mirror EL_SurvivalStats
//! (hunger/thirst/health, each 0-100).
//------------------------------------------------------------------------------------------------
class EL_SurvivalStatsRecord
{
	string m_sPersistentId;
	float m_fHunger;
	float m_fThirst;
	float m_fHealth;

	//------------------------------------------------------------------------------------------------
	//! \param[in] persistentId Character persistence id.
	//! \param[in] hunger Hunger value (0-100).
	//! \param[in] thirst Thirst value (0-100).
	//! \param[in] health Health value (0-100).
	//! \return A fully populated survival stats record.
	static EL_SurvivalStatsRecord Create(string persistentId, float hunger, float thirst, float health)
	{
		EL_SurvivalStatsRecord record();
		record.m_sPersistentId = persistentId;
		record.m_fHunger = hunger;
		record.m_fThirst = thirst;
		record.m_fHealth = health;
		return record;
	}
}