//------------------------------------------------------------------------------------------------
//! The rules of an organization war: the fee, the answer window, how long a war lasts, the cooldown
//! between the same two organizations, the member minimum and the two kill rules (#2667). Loaded from
//! Configs/Organizations/LF_OrgWars.conf, then each pack overlay of it in load order, then the owner's
//! `organizations.wars` keys. A bad owner value logs an error and keeps the value below it.
//!
//! Owner JSON:
//!   "organizations": { "wars": { "fee": 5000, "answerSeconds": 600, "lengthSeconds": 7200 } }
[BaseContainerProps(configRoot: true)]
class LF_OrgWarConfig
{
	static const ResourceName CONFIG_PATH = "{DC782E205D749EA4}Configs/Organizations/LF_OrgWars.conf";

	//! A dotted section is an object inside the `organizations` section (tools/config/schema.mjs).
	static const string SECTION = "organizations.wars";

	static const int FEE_MIN = 0;	// lf-fixed: the schema limit, mirrors the attribute params
	static const int FEE_MAX = 100000000;	// lf-fixed: the schema limit, mirrors the attribute params
	static const int ANSWER_MIN = 10;	// lf-fixed: the schema limit, mirrors the attribute params
	static const int ANSWER_MAX = 86400;	// lf-fixed: the schema limit, one day
	static const int LENGTH_MIN = 60;	// lf-fixed: the schema limit, mirrors the attribute params
	static const int LENGTH_MAX = 604800;	// lf-fixed: the schema limit, one week
	static const int COOLDOWN_MIN = 0;	// lf-fixed: the schema limit, mirrors the attribute params
	static const int COOLDOWN_MAX = 2592000;	// lf-fixed: the schema limit, thirty days
	static const int MEMBERS_MIN = 1;	// lf-fixed: the schema limit, mirrors the attribute params
	static const int MEMBERS_MAX = 100;	// lf-fixed: the schema limit, mirrors the attribute params

	static const int DEFAULT_FEE = 5000;
	static const int DEFAULT_ANSWER_SECONDS = 600;
	static const int DEFAULT_LENGTH_SECONDS = 7200;
	static const int DEFAULT_COOLDOWN_SECONDS = 86400;
	static const int DEFAULT_MIN_MEMBERS = 3;

	[Attribute("5000", UIWidgets.Auto, "Cash the declaring organization pays from its fund to declare a war", params: "0 100000000")]
	int m_iFee;

	[Attribute("600", UIWidgets.Auto, "Seconds the challenged organization has to answer a declaration before it lapses", params: "10 86400")]
	int m_iAnswerSeconds;

	[Attribute("7200", UIWidgets.Auto, "Seconds a war lasts once answered (7200 is one in-game day at the 2 hour day)", params: "60 604800")]
	int m_iLengthSeconds;

	[Attribute("86400", UIWidgets.Auto, "Seconds before the same two organizations may fight another war. 0 means no cooldown", params: "0 2592000")]
	int m_iCooldownSeconds;

	[Attribute("3", UIWidgets.Auto, "Members each side needs to declare or answer a war", params: "1 100")]
	int m_iMinMembers;

	[Attribute("0", UIWidgets.CheckBox, "When on, a kill between organizations at war is murder like any other")]
	bool m_bWarKillsAreMurder;

	[Attribute("1", UIWidgets.CheckBox, "When on, a kill between organizations at war is tagged as a war kill in the record")]
	bool m_bTagWarKills;

	protected static ref LF_OrgWarConfig s_Config;
	protected static bool s_bLoadAttempted;

	//------------------------------------------------------------------------------------------------
	//! The loaded and layered config, or null (logged once) when the file does not resolve.
	static LF_OrgWarConfig Get()
	{
		if (s_bLoadAttempted)
			return s_Config;

		s_bLoadAttempted = true;
		Resource container = BaseContainerTools.LoadContainer(CONFIG_PATH);
		if (!container || !container.GetResource())
		{
			LF_Debug.Error("Org", string.Format("organization war config does not resolve, using defaults: %1", CONFIG_PATH));
			return null;
		}

		s_Config = LF_OrgWarConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (!s_Config)
			LF_Debug.Error("Org", string.Format("organization war config has an unexpected root class, using defaults: %1", CONFIG_PATH));
		else
			s_Config.ApplyLayers();

		return s_Config;
	}

	//------------------------------------------------------------------------------------------------
	//! Drops the cached instance so the next Get() layers the owner file again (/configreload).
	static void Reload()
	{
		s_Config = null;
		s_bLoadAttempted = false;
	}

	//------------------------------------------------------------------------------------------------
	//! The rules in force: a copy, so a caller cannot change the config. A missing or broken file gives
	//! the core defaults.
	static LF_OrgWarRules Rules()
	{
		LF_OrgWarRules rules = new LF_OrgWarRules();
		LF_OrgWarConfig config = Get();
		if (!config)
			return rules;

		rules.m_iFee = config.m_iFee;
		rules.m_iAnswerSeconds = config.m_iAnswerSeconds;
		rules.m_iLengthSeconds = config.m_iLengthSeconds;
		rules.m_iCooldownSeconds = config.m_iCooldownSeconds;
		rules.m_iMinMembers = config.m_iMinMembers;
		rules.m_bWarKillsAreMurder = config.m_bWarKillsAreMurder;
		rules.m_bTagWarKills = config.m_bTagWarKills;
		return rules;
	}

	//------------------------------------------------------------------------------------------------
	//! Merges the layers over this instance, which holds the core `.conf`: each pack overlay in load
	//! order, then the owner's "organizations.wars" keys.
	void ApplyLayers()
	{
		array<ref Resource> resources = {};
		array<ResourceName> names = {};
		LF_ConfigOverlay.LoadPackOverlays(CONFIG_PATH, LF_OrgWarConfig, SECTION, resources, names);
		foreach (int i, Resource resource : resources)
		{
			int changed = ApplyContainer(resource.GetResource().ToBaseContainer());
			LF_Debug.Info(LF_ConfigService.FEATURE, string.Format("%1: pack overlay %2 changed %3 fields", SECTION, names[i], changed));
		}

		LF_ConfigReader reader = LF_ConfigService.OpenOwnerSection("organizations");
		if (!reader || !reader.Has("wars"))
			return;

		if (!reader.BeginRecord("wars"))
			return;

		ApplyOwner(reader);
		LF_Debug.Info(LF_ConfigService.FEATURE, string.Format("%1: owner config read %2 fields, rejected %3", SECTION, reader.GetAppliedCount(), reader.GetRejectedCount()));
		reader.EndRecord();
	}

	//------------------------------------------------------------------------------------------------
	//! Applies one pack overlay: each field it sets directly replaces the core value.
	//! \return Fields changed.
	int ApplyContainer(notnull BaseContainer container)
	{
		int changed;
		if (container.IsVariableSetDirectly("m_iFee") && container.Get("m_iFee", m_iFee))
			changed++;
		if (container.IsVariableSetDirectly("m_iAnswerSeconds") && container.Get("m_iAnswerSeconds", m_iAnswerSeconds))
			changed++;
		if (container.IsVariableSetDirectly("m_iLengthSeconds") && container.Get("m_iLengthSeconds", m_iLengthSeconds))
			changed++;
		if (container.IsVariableSetDirectly("m_iCooldownSeconds") && container.Get("m_iCooldownSeconds", m_iCooldownSeconds))
			changed++;
		if (container.IsVariableSetDirectly("m_iMinMembers") && container.Get("m_iMinMembers", m_iMinMembers))
			changed++;
		if (container.IsVariableSetDirectly("m_bWarKillsAreMurder") && container.Get("m_bWarKillsAreMurder", m_bWarKillsAreMurder))
			changed++;
		if (container.IsVariableSetDirectly("m_bTagWarKills") && container.Get("m_bTagWarKills", m_bTagWarKills))
			changed++;
		return changed;
	}

	//------------------------------------------------------------------------------------------------
	//! Applies the owner's "organizations.wars" object field by field; a key left out, or a value the
	//! reader refuses (it logs the error), keeps the value below it.
	//! \param[in] reader A reader inside the wars object.
	void ApplyOwner(notnull LF_ConfigReader reader)
	{
		array<string> ownerKeys = {"fee", "answerSeconds", "lengthSeconds", "cooldownSeconds", "minMembers", "warKillsAreMurder", "tagWarKills"};
		reader.WarnUnknownKeys(ownerKeys);
		reader.ReadInt("fee", m_iFee, FEE_MIN, FEE_MAX);
		reader.ReadInt("answerSeconds", m_iAnswerSeconds, ANSWER_MIN, ANSWER_MAX);
		reader.ReadInt("lengthSeconds", m_iLengthSeconds, LENGTH_MIN, LENGTH_MAX);
		reader.ReadInt("cooldownSeconds", m_iCooldownSeconds, COOLDOWN_MIN, COOLDOWN_MAX);
		reader.ReadInt("minMembers", m_iMinMembers, MEMBERS_MIN, MEMBERS_MAX);
		reader.ReadBool("warKillsAreMurder", m_bWarKillsAreMurder);
		reader.ReadBool("tagWarKills", m_bTagWarKills);
	}
}

//------------------------------------------------------------------------------------------------
//! The war rules a system reads: plain values, handed out by LF_OrgWarConfig.Rules(). The defaults
//! are the core ones, so a missing config still gives a working rule set.
class LF_OrgWarRules
{
	int m_iFee = LF_OrgWarConfig.DEFAULT_FEE;
	int m_iAnswerSeconds = LF_OrgWarConfig.DEFAULT_ANSWER_SECONDS;
	int m_iLengthSeconds = LF_OrgWarConfig.DEFAULT_LENGTH_SECONDS;
	int m_iCooldownSeconds = LF_OrgWarConfig.DEFAULT_COOLDOWN_SECONDS;
	int m_iMinMembers = LF_OrgWarConfig.DEFAULT_MIN_MEMBERS;
	bool m_bWarKillsAreMurder;
	bool m_bTagWarKills = true;
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_OrgWarConfig's statics (registered at script load; Core/LF_StaticReset.c).
class LF_OrgWarConfig_StaticResetHook : LF_StaticResetHook
{
	protected static bool s_bRegistered = LF_StaticReset.Register(new LF_OrgWarConfig_StaticResetHook());

	override string Owner()
	{
		return "LF_OrgWarConfig";
	}

	override void Apply()
	{
		LF_OrgWarConfig.Reload();
	}
}
