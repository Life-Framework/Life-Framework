[ComponentEditorProps(category: "EveronLife/Survival", description: "Component for character survival stats")]
class EL_CharacterSurvivalComponentClass : ScriptComponentClass
{
}

class EL_CharacterSurvivalComponent : ScriptComponent
{
	protected EL_SurvivalStats m_SurvivalStats;
	protected float m_fLastUpdateTime;

	//------------------------------------------------------------------------------------------------
	void Init(string characterId, EDF_DataCallbackSingle<EL_SurvivalStats> callback)
	{
		// Load or create survival stats
		auto processorCallback = EL_SurvivalStatsProcessorCallback.Create(characterId, callback);
		EPF_PersistentScriptedStateLoader<EL_SurvivalStats>.LoadAsync(characterId, processorCallback);
	}

	//------------------------------------------------------------------------------------------------
	override void EOnFrame(IEntity owner, float timeSlice)
	{
		if (!m_SurvivalStats)
			return;

		float currentTime = GetGame().GetWorld().GetWorldTime();
		float deltaTime = currentTime - m_fLastUpdateTime;
		m_fLastUpdateTime = currentTime;

		m_SurvivalStats.UpdateStats(deltaTime);
	}

	//------------------------------------------------------------------------------------------------
	EL_SurvivalStats GetSurvivalStats()
	{
		return m_SurvivalStats;
	}

	//------------------------------------------------------------------------------------------------
	void SetSurvivalStats(EL_SurvivalStats stats)
	{
		m_SurvivalStats = stats;
		m_fLastUpdateTime = GetGame().GetWorld().GetWorldTime();
	}

	//------------------------------------------------------------------------------------------------
	void Eat(float amount)
	{
		if (m_SurvivalStats)
			m_SurvivalStats.Eat(amount);
	}

	//------------------------------------------------------------------------------------------------
	void Drink(float amount)
	{
		if (m_SurvivalStats)
			m_SurvivalStats.Drink(amount);
	}

	//------------------------------------------------------------------------------------------------
	void Heal(float amount)
	{
		if (m_SurvivalStats)
			m_SurvivalStats.Heal(amount);
	}
};

class EL_SurvivalStatsProcessorCallback : EDF_DataCallbackSingle<EL_SurvivalStats>
{
	string m_sCharacterId;
	ref EDF_DataCallbackSingle<EL_SurvivalStats> m_pCallback;

	//------------------------------------------------------------------------------------------------
	override void OnComplete(EL_SurvivalStats data, Managed context)
	{
		EL_SurvivalStats result = data;
		if (!result)
			result = EL_SurvivalStats.Create(m_sCharacterId);

		if (m_pCallback)
			m_pCallback.Invoke(result);
	}

	//------------------------------------------------------------------------------------------------
	static EL_SurvivalStatsProcessorCallback Create(string characterId, EDF_DataCallbackSingle<EL_SurvivalStats> callback)
	{
		EL_SurvivalStatsProcessorCallback instance();
		instance.m_sCharacterId = characterId;
		instance.m_pCallback = callback;
		return instance;
	}
};