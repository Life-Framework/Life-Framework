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

	[Attribute("30", UIWidgets.Auto, desc: "Seconds the drill must run, held at the door, before the vault opens. Cancelled earlier pays nothing.")]
	protected float m_fDrillSeconds;

	[Attribute("", UIWidgets.EditBox, desc: "Safe type id (Configs/Crime/LF_SafeTypes.conf): store_box, bank_room or reserve_vault. The type supplies every value above that is left at its default; a value set here still wins. Empty keeps the values above as they are.")]
	protected string m_sSafeType;

	//! The numbers this safe runs with: the attributes above with the safe type's row applied (LF_SafeTypes).
	protected ref LF_SafeTuning m_Tuning;

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

		m_Tuning = BuildTuning(vaultId);

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

		LF_Bank.Configure(vaultId, m_Tuning.m_iBagCount, m_Tuning.m_iCashPerBag, m_Tuning.m_fCooldownSeconds, m_Tuning.m_iMinPoliceOnDuty, m_Tuning.m_fFuseSeconds);
		LF_Bank.ConfigureTools(vaultId, m_Tuning.m_sRequiredTools, m_Tuning.m_fJamChance);
		LF_Bank.ConfigureDrillSeconds(vaultId, m_Tuning.m_fDrillSeconds);
		LF_Bank.ConfigureAlarmKind(vaultId, m_Tuning.m_sAlarmKind);
		if (m_Tuning.m_bLoud)
			LF_Breach.SetAlarm(vaultId, "", true);

		ConfigureHeist(vaultId);
	}

	//------------------------------------------------------------------------------------------------
	//! The entity's own attributes with its safe type applied over every value left at the default.
	//! An unknown type id logs LF_Debug.Error and keeps the attributes (LF_SafeTypes.Resolve).
	protected LF_SafeTuning BuildTuning(string vaultId)
	{
		LF_SafeTuning tuning = new LF_SafeTuning();
		tuning.m_iBagCount = m_iBagCount;
		tuning.m_iCashPerBag = m_iCashPerBag;
		tuning.m_fCooldownSeconds = m_fCooldownSeconds;
		tuning.m_iMinPoliceOnDuty = m_iMinPoliceOnDuty;
		tuning.m_fFuseSeconds = m_fFuseSeconds;
		tuning.m_sRequiredTools = m_sRequiredTools;
		tuning.m_sAlarmKind = m_sAlarmKind;
		tuning.m_fJamChance = m_fJamChance;
		tuning.m_fDrillSeconds = m_fDrillSeconds;
		if (!m_sSafeType.IsEmpty())
			LF_SafeTypes.Resolve(LF_SafeTypeConfig.Get(), m_sSafeType, vaultId, tuning);

		return tuning;
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
		if (m_Tuning && !m_Tuning.m_sActionName.IsEmpty())
			return m_Tuning.m_sActionName;

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
