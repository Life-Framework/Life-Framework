//------------------------------------------------------------------------------------------------
//! The persistence-facing store attached to the game-mode entity (GameMode_Roleplay.et).
//!
//! BINDING. This component is added to the game-mode prefab by the config agent and serialized by
//! EL_PersistenceComponentSerializer, listed in Configs/Systems/Persistence/LifeFramework.conf.
//!
//! WHAT LIVES HERE. Only the survival stats, keyed by character persistence id. Player accounts and
//! bank accounts are NOT cached here - they live in the EL_PlayerAccountManager / EL_ATMManager
//! singletons, and the serializer pulls from / applies to those managers directly (their static
//! ExportAll()/ApplyAll() seam). Keeping the accounts out of this component avoids duplicating the
//! manager cache: this component is the store of last resort, not a shadow copy.
//!
//! IDEMPOTENT RE-APPLY. Deserialize also runs when saved data is re-applied to a live session.
//! ClearSurvivalStats() plus per-key SetSurvivalStats() makes the second pass produce exactly the
//! same end state as the first.
//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "EveronLife/Persistence", description: "Persistence-facing store on the game-mode entity")]
class EL_PersistenceComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class EL_PersistenceComponent : ScriptComponent
{
	protected ref map<string, ref EL_SurvivalStats> m_mSurvivalStats = new map<string, ref EL_SurvivalStats>();

	//------------------------------------------------------------------------------------------------
	//! \param[in] characterId Character persistence id (PersistenceIdUtils generated UUID).
	//! \return The survival stats for the character, or null when none have been registered.
	EL_SurvivalStats GetSurvivalStats(string characterId)
	{
		return m_mSurvivalStats.Get(characterId);
	}

	//------------------------------------------------------------------------------------------------
	//! Registers the survival stats for a character, replacing any existing entry for that id.
	//! \param[in] characterId Character persistence id (PersistenceIdUtils generated UUID).
	//! \param[in] stats Survival stats to store.
	void SetSurvivalStats(string characterId, EL_SurvivalStats stats)
	{
		m_mSurvivalStats.Set(characterId, stats);
	}

	//------------------------------------------------------------------------------------------------
	//! Removes all survival stats. Used by the serializer on load so a re-apply to a live session
	//! rebuilds the map to exactly the saved state (idempotent).
	void ClearSurvivalStats()
	{
		m_mSurvivalStats.Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! \return The live survival stats map, for serialization. Read-only use only.
	map<string, ref EL_SurvivalStats> GetSurvivalStatsMap()
	{
		return m_mSurvivalStats;
	}
}