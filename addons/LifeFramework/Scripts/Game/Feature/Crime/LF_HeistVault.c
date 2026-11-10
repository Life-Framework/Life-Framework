//------------------------------------------------------------------------------------------------
//! A forced-entry heist target (#274 cash van). It is a bank vault
//! (LF_BankVaultComponent: gates, fence tool, breach, dispatch) whose haul is configured here:
//! "bags" spill physical money bags behind the target. The target needs the same LF_LockComponent as a vault.
//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "LifeFramework/Feature/Crime", description: "Heist target: the cash van box.")]
class LF_HeistVaultComponentClass : LF_BankVaultComponentClass
{
}

//------------------------------------------------------------------------------------------------
class LF_HeistVaultComponent : LF_BankVaultComponent
{
	[Attribute("bags", UIWidgets.EditBox, desc: "Loot kind: bags (the cash van spills money bags).")]
	protected string m_sLootKind;

	[Attribute("", UIWidgets.EditBox, desc: "Localization key of the offence dispatch reports, for example #LF-Offence_CashVan. Empty is a plain break-in.")]
	protected string m_sOffence;

	[Attribute("#LF-Heist_Van_Open", UIWidgets.EditBox, desc: "Localization key of the action name.")]
	protected string m_sActionName;

	//------------------------------------------------------------------------------------------------
	override protected void ConfigureHeist(string vaultId)
	{
		LF_Bank.ConfigureHeist(vaultId, m_sLootKind, m_sOffence);
	}

	//------------------------------------------------------------------------------------------------
	override string GetActionName()
	{
		return m_sActionName;
	}
}

//------------------------------------------------------------------------------------------------
//! The robbery action on a heist target. The same offer and perform path as the bank vault, under
//! the target's own name ("Break open the cash box").
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
