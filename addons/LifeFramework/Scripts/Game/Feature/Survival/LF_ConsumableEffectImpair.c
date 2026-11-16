//! A consumable effect that impairs the drinker for a while (#1919). Put it on any consumable
//! beside LF_ConsumableEffectSurvival; the server adds a dose through LF_Impairment. The numbers
//! below are the defaults: a row in the drink table (LF_DrinkImpairmentTable, #2002) for the
//! consumed item's prefab wins over them, so a pack tunes a drink without editing its prefab.
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

		LF_DrinkImpairmentRow dose = ResolveDose(item);
		survival.ApplyImpairment(dose.m_fStrength, dose.m_fDurationSeconds, dose.m_eCurve, dose.m_fCap);
	}

	//------------------------------------------------------------------------------------------------
	//! The dose for the item: its table row when the table has one, else this component's values.
	LF_DrinkImpairmentRow ResolveDose(IEntity item)
	{
		LF_DrinkImpairmentRow own = LF_DrinkImpairmentRow.Make(ResourceName.Empty, m_fStrength, m_fDurationSeconds, m_eCurve, m_fCap);
		if (!item || !item.GetPrefabData())
			return own;

		LF_DrinkImpairmentRow row = LF_DrinkImpairmentTable.Find(item.GetPrefabData().GetPrefabName());
		if (!row)
			return own;

		return row;
	}
}
