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

	[Attribute("1", UIWidgets.Auto, desc: "Bags spawned when the vault opens.")]
	protected int m_iBagCount;

	[Attribute("500", UIWidgets.Auto, desc: "MoneyStack quantity placed in each bag.")]
	protected int m_iCashPerBag;

	[Attribute("60", UIWidgets.Auto, desc: "Seconds before another haul can spawn.")]
	protected float m_fCooldownSeconds;

	[Attribute("0", UIWidgets.Auto, desc: "Minimum police on duty. Same gate as LF_RobAction.")]
	protected int m_iMinPoliceOnDuty;

	[Attribute("0", UIWidgets.Auto, desc: "Breach fuse seconds. Zero opens inside Rob.")]
	protected float m_fFuseSeconds;

	[Attribute("drill,charge", UIWidgets.EditBox, desc: "Fence tools that open this vault, any one of: drill, charge. Empty needs none.")]
	protected string m_sRequiredTools;

	[Attribute("", UIWidgets.EditBox, desc: "Alarm kind of this vault's alarm (an LF_AlarmKind id in LF_Police.conf: bank). Empty uses the kind of the reported offence, else the plain siren.")]
	protected string m_sAlarmKind;

	[Attribute("0.15", UIWidgets.Auto, desc: "Chance 0..1 that the drill jams and stays in the wall.")]
	protected float m_fJamChance;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		string vaultId = GetVaultId();
		if (!vaultId.IsEmpty())
		{
			LF_Places.Register(LF_Bank.PLACE_KIND, vaultId, owner);
			// LF_Bank.Configure makes the vault a breach target under the same id, and LF_Breach finds
			// its lock there. A vault prefab that also carries LF_BreachTargetComponent registers the same entity.
			LF_Places.Register(LF_Breach.PLACE_KIND, vaultId, owner);
		}

		if (!Replication.IsServer())
			return;

		if (vaultId.IsEmpty())
		{
			LF_Debug.Error("Bank", "vault component has no id and the entity is unnamed");
			return;
		}

		if (!LF_Component<LF_LockComponent>.Find(owner))
		{
			LF_Debug.Error("Bank", string.Format("vault %1 has no LF_LockComponent", vaultId));
			return;
		}

		LF_Bank.Configure(vaultId, m_iBagCount, m_iCashPerBag, m_fCooldownSeconds, m_iMinPoliceOnDuty, m_fFuseSeconds);
		LF_Bank.ConfigureTools(vaultId, m_sRequiredTools, m_fJamChance);
		LF_Bank.ConfigureAlarmKind(vaultId, m_sAlarmKind);
		ConfigureHeist(vaultId);
	}

	//------------------------------------------------------------------------------------------------
	//! Subclass hook: the heist targets add their loot to the registered vault.
	protected void ConfigureHeist(string vaultId)
	{
	}

	//------------------------------------------------------------------------------------------------
	//! Localization key of the action name. The bank reads "Rob the bank".
	string GetActionName()
	{
		return "#EL-Bank_Rob";
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
