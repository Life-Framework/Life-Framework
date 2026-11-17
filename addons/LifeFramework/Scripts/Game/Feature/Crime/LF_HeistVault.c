//------------------------------------------------------------------------------------------------
//! A forced-entry heist target (#274 cash van, #275 police armory). It is a bank vault
//! (LF_BankVaultComponent: gates, fence tool, breach, dispatch) whose haul is configured here:
//! "bags" spill physical money bags behind the target, "weapons" lay serial-stamped weapons in front
//! of it. The target needs the same LF_LockComponent as a vault.
//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "LifeFramework/Feature/Crime", description: "Heist target: cash van box or police armory store.")]
class LF_HeistVaultComponentClass : LF_BankVaultComponentClass
{
}

//------------------------------------------------------------------------------------------------
class LF_HeistVaultComponent : LF_BankVaultComponent
{
	static const string DEFAULT_ACTION_NAME = "#LF-Heist_Van_Open";	// lf-fixed: mirrors the m_sActionName attribute default

	[Attribute("bags", UIWidgets.EditBox, desc: "Loot kind: bags (cash van) or weapons (armory).")]
	protected string m_sLootKind;

	[Attribute("", UIWidgets.ResourceNamePicker, desc: "Weapons kind only: one entry per weapon. Each prefab must carry LF_SerialComponent.", params: "et")]
	protected ref array<ResourceName> m_aLoot;

	[Attribute("", UIWidgets.EditBox, desc: "Localization key of the offence dispatch reports, for example #LF-Offence_CashVan. Empty is a plain break-in.")]
	protected string m_sOffence;

	[Attribute("0", UIWidgets.CheckBox, desc: "Sound the loud alarm even with officers on duty.")]
	protected bool m_bLoud;

	[Attribute("-1", UIWidgets.Auto, desc: "Hauls the target holds. Negative is unlimited.")]
	protected int m_iStock;

	[Attribute("0", UIWidgets.Auto, desc: "Seconds to restore one haul to stock. Zero never refills.")]
	protected float m_fRefillSeconds;

	[Attribute("#LF-Heist_Van_Open", UIWidgets.EditBox, desc: "Localization key of the action name.")]
	protected string m_sActionName;

	//------------------------------------------------------------------------------------------------
	override protected void ConfigureHeist(string vaultId)
	{
		string loot;
		if (m_aLoot)
		{
			foreach (ResourceName weapon : m_aLoot)
			{
				if (!loot.IsEmpty())
					loot += ",";

				loot += weapon;
			}
		}

		bool loud = m_bLoud;
		if (m_Tuning && m_Tuning.m_bLoud)
			loud = true;

		LF_Bank.ConfigureHeist(vaultId, m_sLootKind, loot, m_sOffence, loud, m_iStock, m_fRefillSeconds);
	}

	//------------------------------------------------------------------------------------------------
	override string GetActionName()
	{
		// The cash van's name is this attribute's default, so it counts as unset: a safe type's own
		// name (store_box: "Crack the box safe") fills it. A name a prefab sets itself still wins.
		if (m_sActionName == DEFAULT_ACTION_NAME && m_Tuning && !m_Tuning.m_sActionName.IsEmpty())
			return m_Tuning.m_sActionName;

		return m_sActionName;
	}
}

//------------------------------------------------------------------------------------------------
//! The robbery action on a heist target. The same offer and perform path as the bank vault, under
//! the target's own name ("Break open the cash box", "Breach the armory store").
class LF_HeistRobAction : LF_BankRobAction
{
	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		LF_BankVaultComponent vault = Vault();
		if (vault)
			outName = vault.GetActionName();
		else
			outName = "#EL-Bank_Rob";

		return true;
	}
}
