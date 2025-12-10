class EL_ConsumableEffectSurvival : SCR_ConsumableEffectBase
{
	protected float m_fHungerRestore;
	protected float m_fThirstRestore;
	protected float m_fHealthRestore;

	//------------------------------------------------------------------------------------------------
	void SetHungerRestore(float value)
	{
		m_fHungerRestore = value;
	}

	//------------------------------------------------------------------------------------------------
	void SetThirstRestore(float value)
	{
		m_fThirstRestore = value;
	}

	//------------------------------------------------------------------------------------------------
	void SetHealthRestore(float value)
	{
		m_fHealthRestore = value;
	}

	//------------------------------------------------------------------------------------------------
	// Match engine/base signature: target is the entity receiving the effect
	override void ApplyEffect(notnull IEntity target, notnull IEntity user, IEntity item, ItemUseParameters animParams)
	{
		if (!target)
			return;

		EL_CharacterSurvivalComponent survivalComp = EL_CharacterSurvivalComponent.Cast(target.FindComponent(EL_CharacterSurvivalComponent));
		if (survivalComp)
		{
			if (m_fHungerRestore > 0)
				survivalComp.Eat(m_fHungerRestore);
			if (m_fThirstRestore > 0)
				survivalComp.Drink(m_fThirstRestore);
			if (m_fHealthRestore > 0)
				survivalComp.Heal(m_fHealthRestore);
		}
	}
};