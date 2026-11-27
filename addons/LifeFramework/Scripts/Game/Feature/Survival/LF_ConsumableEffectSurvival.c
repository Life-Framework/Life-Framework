[BaseContainerProps()]
class LF_ConsumableEffectSurvival : LF_ConsumableEffectEat
{
	[Attribute(defvalue: "0", uiwidget: UIWidgets.Auto, desc: "Hunger restore amount")]
	protected float m_fHungerRestore;

	[Attribute(defvalue: "0", uiwidget: UIWidgets.Auto, desc: "Thirst restore amount")]
	protected float m_fThirstRestore;

	[Attribute(defvalue: "0", uiwidget: UIWidgets.Auto, desc: "Health restore amount")]
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
	//------------------------------------------------------------------------------------------------
	//! Server, from LF_ConsumableEffectEat.ApplyEffect once the use animation completes and the
	//! rules allow it. `target` is the entity receiving the effect.
	override protected void ApplyNeeds(notnull IEntity target, notnull IEntity user, IEntity item)
	{
		LF_CharacterSurvivalComponent survivalComp = LF_CharacterSurvivalComponent.Cast(target.FindComponent(LF_CharacterSurvivalComponent));
		if (!survivalComp)
			return;

		LF_SurvivalStats stats = survivalComp.GetSurvivalStats();
		float hungerBefore = -1;
		float thirstBefore = -1;
		if (stats)
		{
			hungerBefore = stats.GetHunger();
			thirstBefore = stats.GetThirst();
		}

		// Survival off: food and drink restore nothing; the saved stats stay as they are (#1075).
		bool needsOn = LF_Systems.IsEnabled(LF_CharacterSurvivalComponent.SYSTEM);
		if (needsOn && m_fHungerRestore > 0)
			survivalComp.Eat(m_fHungerRestore);
		if (needsOn && m_fThirstRestore > 0)
			survivalComp.Drink(m_fThirstRestore);
		if (m_fHealthRestore > 0)
			survivalComp.Heal(m_fHealthRestore);

		if (stats)
			LF_Debug.Info(LF_ConsumeRules.FEATURE, string.Format("%1 consumed %2: hunger %3 -> %4, thirst %5 -> %6", LF_Utils.GetPlayerUid(target), LF_Utils.GetPrefabName(item), hungerBefore, stats.GetHunger(), thirstBefore, stats.GetThirst()));
	}
};