//! A consumable effect that impairs the drinker for a while (#1919). Put it on any consumable
//! beside LF_ConsumableEffectSurvival; the server adds a dose through LF_Impairment.
[BaseContainerProps()]
class LF_ConsumableEffectImpair : SCR_ConsumableEffectBase
{
	[Attribute(defvalue: "20", uiwidget: UIWidgets.Slider, desc: "Impairment points one use adds (0-100)", params: "0 100 1")]
	protected float m_fStrength;

	[Attribute(defvalue: "600", uiwidget: UIWidgets.Auto, desc: "Seconds until the level has worn off")]
	protected float m_fDurationSeconds;

	[Attribute(defvalue: "0", uiwidget: UIWidgets.ComboBox, desc: "How the level falls", enums: ParamEnumArray.FromEnum(LF_EImpairmentCurve))]
	protected LF_EImpairmentCurve m_eCurve;

	[Attribute(defvalue: "60", uiwidget: UIWidgets.Slider, desc: "Most impairment drinking this can stack up to (0-100)", params: "0 100 1")]
	protected float m_fCap;

	//------------------------------------------------------------------------------------------------
	override void ApplyEffect(notnull IEntity target, notnull IEntity user, IEntity item, ItemUseParameters animParams)
	{
		if (!Replication.IsServer())
			return;

		LF_CharacterSurvivalComponent survival = LF_CharacterSurvivalComponent.Cast(target.FindComponent(LF_CharacterSurvivalComponent));
		if (!survival)
		{
			LF_Debug.Warn(LF_Impairment.LOG, "impairing consumable used on an entity with no survival component");
			return;
		}

		survival.ApplyImpairment(m_fStrength, m_fDurationSeconds, m_eCurve, m_fCap);
	}
}
