class EL_SurvivalStats : EPF_PersistentScriptedState
{
	protected float m_fHunger; // 0-100
	protected float m_fThirst; // 0-100
	protected float m_fHealth; // 0-100

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
	}

	//------------------------------------------------------------------------------------------------
	void Drink(float amount)
	{
		SetThirst(m_fThirst + amount);
	}

	//------------------------------------------------------------------------------------------------
	void Heal(float amount)
	{
		SetHealth(m_fHealth + amount);
	}

	//------------------------------------------------------------------------------------------------
	void UpdateStats(float deltaTime)
	{
		// Decrease hunger and thirst over time
		SetHunger(m_fHunger - deltaTime * 0.1); // Adjust rate as needed
		SetThirst(m_fThirst - deltaTime * 0.15);

		// Health decreases if hunger or thirst is low
		if (m_fHunger < 20 || m_fThirst < 20)
		{
			SetHealth(m_fHealth - deltaTime * 0.05);
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