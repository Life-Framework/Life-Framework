[
	EPF_PersistentScriptedStateSettings(EL_SurvivalStats),
	EDF_DbName.Automatic()
]
class EL_SurvivalStatsSaveData : EPF_ScriptedStateSaveData
{
	float m_fHunger;
	float m_fThirst;
	float m_fHealth;

	//------------------------------------------------------------------------------------------------
	override EPF_EReadResult ReadFrom(notnull Managed scriptedState)
	{
		EL_SurvivalStats stats = EL_SurvivalStats.Cast(scriptedState);
		SetId(stats.GetPersistentId());
		m_fHunger = stats.GetHunger();
		m_fThirst = stats.GetThirst();
		m_fHealth = stats.GetHealth();
		return EPF_EReadResult.OK;
	}

	//------------------------------------------------------------------------------------------------
	override EPF_EApplyResult ApplyTo(notnull Managed scriptedState)
	{
		EL_SurvivalStats stats = EL_SurvivalStats.Cast(scriptedState);
		stats.SetPersistentId(GetId());
		stats.SetHunger(m_fHunger);
		stats.SetThirst(m_fThirst);
		stats.SetHealth(m_fHealth);
		return EPF_EApplyResult.OK;
	}

	//------------------------------------------------------------------------------------------------
	override bool Equals(notnull EPF_ScriptedStateSaveData other)
	{
		EL_SurvivalStatsSaveData otherData = EL_SurvivalStatsSaveData.Cast(other);
		return m_fHunger == otherData.m_fHunger && m_fThirst == otherData.m_fThirst && m_fHealth == otherData.m_fHealth;
	}
};