//------------------------------------------------------------------------------------------------
//! Marks a door as a bank vault and registers it with LF_Bank on the server.
//! The vault id is the breach target id. The bag entity receives the MoneyStack haul.
//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "LifeFramework/Feature/Crime", description: "Vault that spills MoneyStack bags after a breach.")]
class LF_BankVaultComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class LF_BankVaultComponent : ScriptComponent
{
	[Attribute("", UIWidgets.EditBox, desc: "Stable vault id. Empty uses the entity name.")]
	protected string m_sVaultId;

	[Attribute("", UIWidgets.EditBox, desc: "World entity name of the hand-carry bag.")]
	protected string m_sBagEntity;

	[Attribute("1", UIWidgets.Auto, desc: "Bags spawned when the vault opens.")]
	protected int m_iBagCount;

	[Attribute("500", UIWidgets.Auto, desc: "MoneyStack quantity placed in each bag.")]
	protected int m_iCashPerBag;

	[Attribute("60", UIWidgets.Auto, desc: "Seconds before another haul can spawn.")]
	protected float m_fCooldownSeconds;

	[Attribute("0", UIWidgets.Auto, desc: "Minimum police on duty. Same gate as EL_RobAction.")]
	protected int m_iMinPoliceOnDuty;

	[Attribute("0", UIWidgets.Auto, desc: "Breach fuse seconds. Zero opens inside Rob.")]
	protected float m_fFuseSeconds;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (!Replication.IsServer())
			return;

		string vaultId = GetVaultId();
		if (vaultId.IsEmpty())
		{
			EL_Debug.Error("Bank", "vault component has no id and the entity is unnamed");
			return;
		}

		if (!EL_Component<EL_LockComponent>.Find(owner))
		{
			EL_Debug.Error("Bank", string.Format("vault %1 has no EL_LockComponent", vaultId));
			return;
		}

		LF_Bank.Configure(vaultId, m_iBagCount, m_iCashPerBag, m_fCooldownSeconds, m_iMinPoliceOnDuty, m_fFuseSeconds, m_sBagEntity);
	}

	//------------------------------------------------------------------------------------------------
	string GetVaultId()
	{
		if (!m_sVaultId.IsEmpty())
			return m_sVaultId;

		if (GetOwner())
			return GetOwner().GetName();

		return "";
	}
}
