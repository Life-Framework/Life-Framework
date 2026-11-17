//------------------------------------------------------------------------------------------------
//! One safe tier: the numbers a robbable safe starts from (Configs/Crime/LF_SafeTypes.conf). A vault
//! component names a row by id (LF_BankVaultComponent.m_sSafeType) and the row supplies every value
//! the entity leaves at its class default; a value the prefab or the layer sets itself still wins.
//! The three core tiers: store_box (a box safe in a shop), bank_room (a vault room in a bank) and
//! reserve_vault (the big standalone vault). The id is the type id. Core ships neutral numbers; a
//! pack overlays or adds rows through its mission header (LF_ConfigOverlay).
[BaseContainerProps()]
class LF_SafeTypeRow : LF_ConfigRecord
{
	[Attribute("", UIWidgets.EditBox, "Type id the component names: store_box, bank_room, reserve_vault")]
	string m_sId;

	[Attribute("1", UIWidgets.Auto, "Bags spawned when the safe opens", params: "1 50")]
	int m_iBagCount;

	[Attribute("500", UIWidgets.Auto, "MoneyStack quantity in each bag", params: "1 1000000")]
	int m_iCashPerBag;

	[Attribute("60", UIWidgets.Auto, "Seconds before another haul can spawn", params: "0 86400")]
	float m_fCooldownSeconds;

	[Attribute("0", UIWidgets.Auto, "Minimum police on duty. Same gate as LF_RobAction", params: "0 20")]
	int m_iMinPoliceOnDuty;

	[Attribute("0", UIWidgets.Auto, "Breach fuse seconds. Zero opens inside Rob", params: "0 3600")]
	float m_fFuseSeconds;

	[Attribute("drill,charge", UIWidgets.EditBox, "Fence tools that open it, any one of: drill, charge, keycard. Empty needs none")]
	string m_sRequiredTools;

	[Attribute("", UIWidgets.EditBox, "Alarm kind (an LF_AlarmKind id in LF_Police.conf: bank, store, fuel). Empty uses the offence's kind")]
	string m_sAlarmKind;

	[Attribute("0.15", UIWidgets.Auto, "Chance 0..1 the drill jams and stays in the wall", params: "0 1")]
	float m_fJamChance;

	[Attribute("30", UIWidgets.Auto, "Seconds the drill must run held at the safe. Must be positive; unused when no drill is accepted", params: "1 3600")]
	float m_fDrillSeconds;

	[Attribute("0", UIWidgets.CheckBox, "Sound the loud alarm even with officers on duty")]
	bool m_bLoud;

	[Attribute("", UIWidgets.EditBox, "Localization key of the action name, e.g. #LF-Safe_BoxRob. Empty keeps the prefab's own")]
	string m_sActionName;

	//------------------------------------------------------------------------------------------------
	static LF_SafeTypeRow Make(string id)
	{
		LF_SafeTypeRow row = new LF_SafeTypeRow();
		row.m_sId = id;
		return row;
	}
}

//------------------------------------------------------------------------------------------------
//! The safe type table: Configs/Crime/LF_SafeTypes.conf with each pack overlay layered over it
//! (LF_ConfigOverlay, #962). A pack adds, replaces, patches or removes a row by id. No owner
//! section: a safe's numbers are content, set by the pack, not a server knob.
[BaseContainerProps(configRoot: true)]
class LF_SafeTypeConfig
{
	static const string FEATURE = "SafeTypes";
	static const ResourceName CONFIG_PATH = "{3D8F6B2A94E17C05}Configs/Crime/LF_SafeTypes.conf";

	static const string TYPE_STORE_BOX = "store_box";
	static const string TYPE_BANK_ROOM = "bank_room";
	static const string TYPE_RESERVE_VAULT = "reserve_vault";

	[Attribute("", UIWidgets.Object, "One row per safe tier")]
	ref array<ref LF_SafeTypeRow> m_aTypes;

	protected static ref LF_SafeTypeConfig s_Config;
	protected static bool s_bLoadAttempted;

	//------------------------------------------------------------------------------------------------
	//! The loaded and layered table, or null (logged once) when the core file does not resolve.
	static LF_SafeTypeConfig Get()
	{
		if (s_bLoadAttempted)
			return s_Config;

		s_bLoadAttempted = true;
		Resource container = BaseContainerTools.LoadContainer(CONFIG_PATH);
		if (!container || !container.GetResource())
		{
			LF_Debug.Error(FEATURE, string.Format("safe type table does not resolve, safes keep their own numbers: %1", CONFIG_PATH));
			return null;
		}

		s_Config = LF_SafeTypeConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (!s_Config)
			LF_Debug.Error(FEATURE, string.Format("safe type table has an unexpected root class: %1", CONFIG_PATH));
		else
			s_Config.ApplyLayers();

		return s_Config;
	}

	//------------------------------------------------------------------------------------------------
	//! Drops the cached table so the next Get() loads and layers it again.
	static void Reload()
	{
		s_Config = null;
		s_bLoadAttempted = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Merges each pack overlay of this class, in load order, over this instance.
	void ApplyLayers()
	{
		if (!m_aTypes)
			m_aTypes = new array<ref LF_SafeTypeRow>();

		LF_SafeTypeList list = new LF_SafeTypeList(this);
		array<ref Resource> resources = {};
		array<ResourceName> names = {};
		LF_ConfigOverlay.LoadPackOverlays(CONFIG_PATH, LF_SafeTypeConfig, "safeTypes", resources, names);
		LF_ConfigClaims claims = new LF_ConfigClaims();
		foreach (int i, Resource resource : resources)
		{
			int changed = LF_ConfigOverlay.ApplyContainerList(list, resource.GetResource().ToBaseContainer(), "m_aTypes", names[i], claims);
			LF_Debug.Info(FEATURE, string.Format("pack overlay %1 changed %2 safe type rows", names[i], changed));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The row with this type id, or null.
	LF_SafeTypeRow Find(string id)
	{
		if (!m_aTypes || id.IsEmpty())
			return null;

		foreach (LF_SafeTypeRow row : m_aTypes)
		{
			if (row && row.m_sId == id)
				return row;
		}

		return null;
	}
}

//------------------------------------------------------------------------------------------------
//! The safe type table's `m_aTypes` list for the shared merge (LF_ConfigOverlay), keyed by type id.
class LF_SafeTypeList : LF_ConfigList
{
	protected LF_SafeTypeConfig m_Config;

	void LF_SafeTypeList(LF_SafeTypeConfig config)
	{
		m_Config = config;
	}

	override string GetName()
	{
		return "safeTypes.types";
	}

	override int Count()
	{
		return m_Config.m_aTypes.Count();
	}

	override LF_ConfigRecord Get(int index)
	{
		return m_Config.m_aTypes[index];
	}

	override string IdOf(notnull LF_ConfigRecord record)
	{
		return LF_SafeTypeRow.Cast(record).m_sId;
	}

	override string IdOfContainer(notnull BaseContainer container)
	{
		string id;
		container.Get("m_sId", id);
		return id;
	}

	override string IdHint()
	{
		return "a safe type id such as store_box";
	}

	override LF_ConfigRecord NewRecord(string id)
	{
		return LF_SafeTypeRow.Make(id);
	}

	override void Set(int index, notnull LF_ConfigRecord record)
	{
		LF_SafeTypeRow row = LF_SafeTypeRow.Cast(record);
		if (index < m_Config.m_aTypes.Count())
			m_Config.m_aTypes.Set(index, row);
		else
			m_Config.m_aTypes.Insert(row);
	}

	override void RemoveAt(int index)
	{
		m_Config.m_aTypes.RemoveOrdered(index);
	}

	override int PatchFromContainer(notnull LF_ConfigRecord record, notnull BaseContainer container)
	{
		LF_SafeTypeRow row = LF_SafeTypeRow.Cast(record);
		int changed;
		if (container.IsVariableSetDirectly("m_iBagCount") && container.Get("m_iBagCount", row.m_iBagCount))
			changed++;
		if (container.IsVariableSetDirectly("m_iCashPerBag") && container.Get("m_iCashPerBag", row.m_iCashPerBag))
			changed++;
		if (container.IsVariableSetDirectly("m_fCooldownSeconds") && container.Get("m_fCooldownSeconds", row.m_fCooldownSeconds))
			changed++;
		if (container.IsVariableSetDirectly("m_iMinPoliceOnDuty") && container.Get("m_iMinPoliceOnDuty", row.m_iMinPoliceOnDuty))
			changed++;
		if (container.IsVariableSetDirectly("m_fFuseSeconds") && container.Get("m_fFuseSeconds", row.m_fFuseSeconds))
			changed++;
		if (container.IsVariableSetDirectly("m_sRequiredTools") && container.Get("m_sRequiredTools", row.m_sRequiredTools))
			changed++;
		if (container.IsVariableSetDirectly("m_sAlarmKind") && container.Get("m_sAlarmKind", row.m_sAlarmKind))
			changed++;
		if (container.IsVariableSetDirectly("m_fJamChance") && container.Get("m_fJamChance", row.m_fJamChance))
			changed++;
		if (container.IsVariableSetDirectly("m_fDrillSeconds") && container.Get("m_fDrillSeconds", row.m_fDrillSeconds))
			changed++;
		if (container.IsVariableSetDirectly("m_bLoud") && container.Get("m_bLoud", row.m_bLoud))
			changed++;
		if (container.IsVariableSetDirectly("m_sActionName") && container.Get("m_sActionName", row.m_sActionName))
			changed++;
		return changed;
	}
}

//------------------------------------------------------------------------------------------------
//! The numbers one safe runs with after its type is applied. LF_BankVaultComponent fills it from its
//! attributes (the per-entity values), LF_SafeTypes.Apply lets a type row replace every value still at
//! its class default, and the component configures LF_Bank from the result.
class LF_SafeTuning
{
	// The attribute defaults of LF_BankVaultComponent. A value at its default counts as "not set on
	// this entity", so the type fills it. lf-fixed: these mirror the component's [Attribute] defaults.
	static const int DEFAULT_BAGS = 1;	// lf-fixed: mirrors LF_BankVaultComponent.m_iBagCount
	static const int DEFAULT_CASH = 500;	// lf-fixed: mirrors LF_BankVaultComponent.m_iCashPerBag
	static const float DEFAULT_COOLDOWN = 60;	// lf-fixed: mirrors LF_BankVaultComponent.m_fCooldownSeconds
	static const int DEFAULT_POLICE = 0;	// lf-fixed: mirrors LF_BankVaultComponent.m_iMinPoliceOnDuty
	static const float DEFAULT_FUSE = 0;	// lf-fixed: mirrors LF_BankVaultComponent.m_fFuseSeconds
	static const string DEFAULT_TOOLS = "drill,charge";	// lf-fixed: mirrors LF_BankVaultComponent.m_sRequiredTools
	static const string DEFAULT_ALARM = "";	// lf-fixed: mirrors LF_BankVaultComponent.m_sAlarmKind
	static const float DEFAULT_JAM = 0.15;	// lf-fixed: mirrors LF_BankVaultComponent.m_fJamChance
	static const float DEFAULT_DRILL_SECONDS = 30;	// lf-fixed: mirrors LF_BankVaultComponent.m_fDrillSeconds

	int m_iBagCount = DEFAULT_BAGS;
	int m_iCashPerBag = DEFAULT_CASH;
	float m_fCooldownSeconds = DEFAULT_COOLDOWN;
	int m_iMinPoliceOnDuty = DEFAULT_POLICE;
	float m_fFuseSeconds = DEFAULT_FUSE;
	string m_sRequiredTools = DEFAULT_TOOLS;
	string m_sAlarmKind = DEFAULT_ALARM;
	float m_fJamChance = DEFAULT_JAM;
	float m_fDrillSeconds = DEFAULT_DRILL_SECONDS;
	bool m_bLoud;
	string m_sActionName;
}

//------------------------------------------------------------------------------------------------
class LF_SafeTypes
{
	//------------------------------------------------------------------------------------------------
	//! Lets `row` fill every value of `tuning` that is still at its class default. A value the entity
	//! set to something else is kept. The loud flag only turns on (an entity that is loud stays loud).
	//! The action name fills when the entity has none.
	static void Apply(notnull LF_SafeTypeRow row, notnull LF_SafeTuning tuning)
	{
		if (tuning.m_iBagCount == LF_SafeTuning.DEFAULT_BAGS)
			tuning.m_iBagCount = row.m_iBagCount;

		if (tuning.m_iCashPerBag == LF_SafeTuning.DEFAULT_CASH)
			tuning.m_iCashPerBag = row.m_iCashPerBag;

		if (tuning.m_fCooldownSeconds == LF_SafeTuning.DEFAULT_COOLDOWN)
			tuning.m_fCooldownSeconds = row.m_fCooldownSeconds;

		if (tuning.m_iMinPoliceOnDuty == LF_SafeTuning.DEFAULT_POLICE)
			tuning.m_iMinPoliceOnDuty = row.m_iMinPoliceOnDuty;

		if (tuning.m_fFuseSeconds == LF_SafeTuning.DEFAULT_FUSE)
			tuning.m_fFuseSeconds = row.m_fFuseSeconds;

		if (tuning.m_sRequiredTools == LF_SafeTuning.DEFAULT_TOOLS)
			tuning.m_sRequiredTools = row.m_sRequiredTools;

		if (tuning.m_sAlarmKind == LF_SafeTuning.DEFAULT_ALARM)
			tuning.m_sAlarmKind = row.m_sAlarmKind;

		if (tuning.m_fJamChance == LF_SafeTuning.DEFAULT_JAM)
			tuning.m_fJamChance = row.m_fJamChance;

		if (tuning.m_fDrillSeconds == LF_SafeTuning.DEFAULT_DRILL_SECONDS)
			tuning.m_fDrillSeconds = row.m_fDrillSeconds;

		if (row.m_bLoud)
			tuning.m_bLoud = true;

		if (tuning.m_sActionName.IsEmpty())
			tuning.m_sActionName = row.m_sActionName;
	}

	//------------------------------------------------------------------------------------------------
	//! Applies the type `typeId` from `config` to `tuning`. An empty id is the legacy path: nothing
	//! changes and true comes back. An id with no row logs LF_Debug.Error and degrades: the entity
	//! keeps its own numbers (the class defaults where it set none), it never aborts.
	//! \return False when the id names no row.
	static bool Resolve(LF_SafeTypeConfig config, string typeId, string who, notnull LF_SafeTuning tuning)
	{
		if (typeId.IsEmpty())
			return true;

		LF_SafeTypeRow row;
		if (config)
			row = config.Find(typeId);

		if (!row)
		{
			LF_Debug.Error(LF_SafeTypeConfig.FEATURE, string.Format("%1: unknown safe type '%2', keeping its own numbers", who, typeId));
			return false;
		}

		Apply(row, tuning);
		return true;
	}
}
