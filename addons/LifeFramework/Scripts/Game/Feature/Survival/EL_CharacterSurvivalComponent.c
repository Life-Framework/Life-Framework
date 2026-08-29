[ComponentEditorProps(category: "EveronLife/Survival", description: "Component for character survival stats")]
class EL_CharacterSurvivalComponentClass : ScriptComponentClass
{
}

class EL_CharacterSurvivalComponent : ScriptComponent
{
	protected EL_SurvivalStats m_SurvivalStats;
	protected string m_sCharacterId;
	protected float m_fLastUpdateTime;

	//------------------------------------------------------------------------------------------------
	//! Loads (or creates and registers) the survival stats for a character, synchronously.
	//! \param characterId Character persistence id (PersistenceIdUtils generated UUID).
	void Init(string characterId)
	{
		m_sCharacterId = characterId;

		EL_PersistenceComponent persistence = GetPersistenceComponent();
		if (persistence)
		{
			m_SurvivalStats = persistence.GetSurvivalStats(characterId);
			if (!m_SurvivalStats)
			{
				m_SurvivalStats = EL_SurvivalStats.Create(characterId);
				persistence.SetSurvivalStats(characterId, m_SurvivalStats);
			}
		}
		else
		{
			m_SurvivalStats = EL_SurvivalStats.Create(characterId);
		}

		m_fLastUpdateTime = GetGame().GetWorld().GetWorldTime();
	}

	//------------------------------------------------------------------------------------------------
	//! Resolves the game-mode persistence component (EL_PersistenceComponent) attached to the
	//! game-mode entity.
	//! \return The persistence component, or null when no game mode carries one.
	protected EL_PersistenceComponent GetPersistenceComponent()
	{
		EL_GameModeRoleplay mode = EL_GameModeRoleplay.Cast(GetGame().GetGameMode());
		if (!mode)
			return null;

		return EL_PersistenceComponent.Cast(mode.FindComponent(EL_PersistenceComponent));
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

		// Keep the persistence store in sync so a save picks up the live value.
		if (m_SurvivalStats)
		{
			EL_PersistenceComponent persistence = GetPersistenceComponent();
			if (persistence)
				persistence.SetSurvivalStats(m_sCharacterId, m_SurvivalStats);
		}
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