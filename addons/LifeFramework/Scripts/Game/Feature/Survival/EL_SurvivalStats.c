class EL_SurvivalStats : Managed
{
	protected string m_sPersistentId;
	protected float m_fHunger; // 0-100
	protected float m_fThirst; // 0-100
	protected float m_fHealth; // 0-100

	//------------------------------------------------------------------------------------------------
	string GetPersistentId()
	{
		return m_sPersistentId;
	}

	//------------------------------------------------------------------------------------------------
	void SetPersistentId(string id)
	{
		m_sPersistentId = id;
	}

	//------------------------------------------------------------------------------------------------
	float GetHunger()
	{
		return m_fHunger;
	}

	//------------------------------------------------------------------------------------------------
	float GetThirst()
	{
		return m_fThirst;
	}

	//------------------------------------------------------------------------------------------------
	float GetHealth()
	{
		return m_fHealth;
	}

	//------------------------------------------------------------------------------------------------
	void SetHunger(float value)
	{
		m_fHunger = Math.Clamp(value, 0, 100);
	}

	//------------------------------------------------------------------------------------------------
	void SetThirst(float value)
	{
		m_fThirst = Math.Clamp(value, 0, 100);
	}

	//------------------------------------------------------------------------------------------------
	void SetHealth(float value)
	{
		m_fHealth = Math.Clamp(value, 0, 100);
	}

	//------------------------------------------------------------------------------------------------
	void Eat(float amount)
	{
		SetHunger(m_fHunger + amount);
		EL_Debug.Log("Survival", string.Format("eat +%1 -> hunger %2", amount, m_fHunger));
	}

	//------------------------------------------------------------------------------------------------
	void Drink(float amount)
	{
		SetThirst(m_fThirst + amount);
		EL_Debug.Log("Survival", string.Format("drink +%1 -> thirst %2", amount, m_fThirst));
	}

	//------------------------------------------------------------------------------------------------
	void Heal(float amount)
	{
		SetHealth(m_fHealth + amount);
	}

	//------------------------------------------------------------------------------------------------
	//! \param config Survival tuning; null uses LF_SurvivalConfig.Get().
	//! \return True when hunger or thirst is below the config's low-needs threshold.
	bool AreNeedsLow(LF_SurvivalConfig config = null)
	{
		if (!config)
			config = LF_SurvivalConfig.Get();

		return m_fHunger < config.m_fLowNeedThreshold || m_fThirst < config.m_fLowNeedThreshold;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The stamina drain multiplier needs impose: the config value while needs are low, else 1.
	float GetStaminaDrainMultiplier(LF_SurvivalConfig config = null)
	{
		if (!config)
			config = LF_SurvivalConfig.Get();

		if (!AreNeedsLow(config))
			return 1;

		return Math.Max(config.m_fStaminaDrainMultiplier, 1);
	}

	//------------------------------------------------------------------------------------------------
	//! Drains hunger and thirst by deltaTime seconds. Low needs pull health down to the config cap
	//! and never below it, so needs never kill; met needs regenerate health up to 100.
	//! \param deltaTime Seconds elapsed; zero or negative is a no-op.
	//! \param config Survival tuning; null uses LF_SurvivalConfig.Get().
	void UpdateStats(float deltaTime, LF_SurvivalConfig config = null)
	{
		if (deltaTime <= 0)
			return;

		if (!config)
			config = LF_SurvivalConfig.Get();

		SetHunger(m_fHunger - deltaTime * config.m_fHungerDrainPerSecond);
		SetThirst(m_fThirst - deltaTime * config.m_fThirstDrainPerSecond);

		if (AreNeedsLow(config))
		{
			// Only health above the cap is pulled down; an injury below the cap is not worsened.
			if (m_fHealth > config.m_fHealthCap)
				SetHealth(Math.Max(m_fHealth - deltaTime * config.m_fHealthDrainPerSecond, config.m_fHealthCap));
		}
		else if (config.m_fHealthRegenPerSecond > 0)
		{
			SetHealth(m_fHealth + deltaTime * config.m_fHealthRegenPerSecond);
		}
	}

	//------------------------------------------------------------------------------------------------
	static EL_SurvivalStats Create(string statsId)
	{
		EL_SurvivalStats stats();
		stats.SetPersistentId(statsId);
		stats.m_fHunger = 100;
		stats.m_fThirst = 100;
		stats.m_fHealth = 100;
		return stats;
	}
};