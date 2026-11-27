//------------------------------------------------------------------------------------------------
//! The rules of each contested site: radius, member minimum, capture, decay, flip cooldown and rent, per hideout id (#2679, gang war #1022). Loaded from Configs/Sites/LF_Sites.conf
//! (the core: no sites, only the default rules), then each pack overlay of it in load order, then the
//! owner's `sites` section. A site is a record keyed by the hideout id it guards; a field a record
//! leaves out has the default rule. A bad value logs an error and keeps the value below it.
//!
//! Owner JSON:
//!   "sites": { "rules": { "<hideoutId>": { "captureSeconds": 120, "rentPerHour": 900 } } }
//! Each record may carry an "op" (add, replace, patch, remove): docs/config/override-model.md.
[BaseContainerProps(configRoot: true)]
class LF_SitesConfig
{
	static const ResourceName CONFIG_PATH = "{3E5E7C6014640532}Configs/Sites/LF_Sites.conf";

	//! The owner config.json section this class reads (LF_ConfigService).
	static const string SECTION = "sites";

	static const int RADIUS_MIN = 1;	// lf-fixed: the schema limit, mirrors the attribute params
	static const int RADIUS_MAX = 1000;	// lf-fixed: the schema limit, mirrors the attribute params
	static const int INNER_RADIUS_MIN = 0;	// lf-fixed: the schema limit, mirrors the attribute params
	static const int MEMBERS_MIN = 1;	// lf-fixed: the schema limit, mirrors the attribute params
	static const int MEMBERS_MAX = 100;	// lf-fixed: the schema limit, mirrors the attribute params
	static const int CAPTURE_MIN = 1;	// lf-fixed: the schema limit, mirrors the attribute params
	static const int CAPTURE_MAX = 86400;	// lf-fixed: the schema limit, one day
	static const int DECAY_MIN = 0;	// lf-fixed: the schema limit, mirrors the attribute params
	static const int DECAY_MAX = 1000000;	// lf-fixed: the schema limit, mirrors the attribute params
	static const int COOLDOWN_MIN = 0;	// lf-fixed: the schema limit, mirrors the attribute params
	static const int COOLDOWN_MAX = 604800;	// lf-fixed: the schema limit, one week
	static const int RENT_MIN = 0;	// lf-fixed: the schema limit, mirrors the attribute params
	static const int RENT_MAX = 1000000;	// lf-fixed: the schema limit, mirrors the attribute params
	static const int RENT_BANKED_MIN = 0;	// lf-fixed: the schema limit, mirrors the attribute params
	static const int RENT_BANKED_MAX = 100000000;	// lf-fixed: the schema limit, mirrors the attribute params
	static const int WINDOW_MIN = 1;	// lf-fixed: the schema limit, mirrors the attribute params
	static const int WINDOW_MAX = 86400;	// lf-fixed: the schema limit, one day

	static const int DEFAULT_RADIUS = 60;
	static const int DEFAULT_INNER_RADIUS = 20;
	static const int DEFAULT_MIN_MEMBERS = 2;
	static const int DEFAULT_CAPTURE_SECONDS = 300;
	static const int DEFAULT_DECAY_PER_HOUR = 600;
	static const int DEFAULT_FLIP_COOLDOWN_SECONDS = 1800;
	static const int DEFAULT_RENT_PER_HOUR = 600;
	static const int DEFAULT_MAX_RENT_BANKED = 3000;
	static const int DEFAULT_ACTIVE_WINDOW_SECONDS = 300;

	[Attribute("", UIWidgets.Object, "The rules of each contested site, one entry per hideout id the site guards. A field an entry leaves out has the default rule. The core ships none: a pack adds the sites of its map")]
	ref array<ref LF_SiteRuleDef> m_aRules;

	protected static ref LF_SitesConfig s_Config;
	protected static bool s_bLoadAttempted;

	//------------------------------------------------------------------------------------------------
	//! The loaded and layered config, or null (logged once) when the file does not resolve.
	static LF_SitesConfig Get()
	{
		if (s_bLoadAttempted)
			return s_Config;

		s_bLoadAttempted = true;
		s_Config = LoadCore();
		if (s_Config)
			s_Config.ApplyLayers();

		return s_Config;
	}

	//------------------------------------------------------------------------------------------------
	//! The core `.conf` alone, no pack and no owner layer: a new instance each call. Null (logged)
	//! when the file does not resolve.
	static LF_SitesConfig LoadCore()
	{
		Resource container = BaseContainerTools.LoadContainer(CONFIG_PATH);
		if (!container || !container.GetResource())
		{
			LF_Debug.Error("Sites", string.Format("sites config does not resolve, using defaults: %1", CONFIG_PATH));
			return null;
		}

		LF_SitesConfig config = LF_SitesConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (!config)
		{
			LF_Debug.Error("Sites", string.Format("sites config has an unexpected root class, using defaults: %1", CONFIG_PATH));
			return null;
		}

		if (!config.m_aRules)
			config.m_aRules = new array<ref LF_SiteRuleDef>();

		return config;
	}

	//------------------------------------------------------------------------------------------------
	//! Drops the cached instance so the next Get() layers the owner file again (/configreload).
	static void Reload()
	{
		s_Config = null;
		s_bLoadAttempted = false;
	}

	//------------------------------------------------------------------------------------------------
	//! The rules of one site: a copy, so a caller cannot change the config. A hideout id with no
	//! entry, or a missing config, gives the default rules.
	static LF_SiteSettings For(string hideoutId)
	{
		LF_SitesConfig config = Get();
		if (!config)
			return new LF_SiteSettings();

		return config.SettingsOf(hideoutId);
	}

	//------------------------------------------------------------------------------------------------
	//! \return The entry of this hideout id, or null.
	LF_SiteRuleDef Find(string hideoutId)
	{
		if (!m_aRules)
			return null;

		foreach (LF_SiteRuleDef rule : m_aRules)
		{
			if (rule && rule.m_sHideoutId == hideoutId)
				return rule;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! \return A copy of the entry's values, or the default rules when the id has no entry.
	LF_SiteSettings SettingsOf(string hideoutId)
	{
		LF_SiteSettings settings = new LF_SiteSettings();
		LF_SiteRuleDef rule = Find(hideoutId);
		if (!rule)
			return settings;

		settings.m_bConfigured = true;
		settings.m_iRadius = rule.m_iRadius;
		settings.m_iInnerRadius = rule.m_iInnerRadius;
		settings.m_iMinMembersPresent = rule.m_iMinMembersPresent;
		settings.m_iCaptureSeconds = rule.m_iCaptureSeconds;
		settings.m_iDecayPerHour = rule.m_iDecayPerHour;
		settings.m_iFlipCooldownSeconds = rule.m_iFlipCooldownSeconds;
		settings.m_iRentPerHour = rule.m_iRentPerHour;
		settings.m_iMaxRentBanked = rule.m_iMaxRentBanked;
		settings.m_iActiveWindowSeconds = rule.m_iActiveWindowSeconds;
		return settings;
	}

	//------------------------------------------------------------------------------------------------
	//! Merges the layers over this instance, which holds the core `.conf`: each pack overlay in load
	//! order, then the owner's "sites" section.
	void ApplyLayers()
	{
		if (!m_aRules)
			m_aRules = new array<ref LF_SiteRuleDef>();

		array<ref Resource> resources = {};
		array<ResourceName> names = {};
		LF_ConfigOverlay.LoadPackOverlays(CONFIG_PATH, LF_SitesConfig, SECTION, resources, names);
		LF_ConfigClaims claims = new LF_ConfigClaims();
		foreach (int i, Resource resource : resources)
		{
			int changed = ApplyPackContainer(resource.GetResource().ToBaseContainer(), names[i], claims);
			LF_Debug.Info(LF_ConfigService.FEATURE, string.Format("%1: pack overlay %2 changed %3 sites", SECTION, names[i], changed));
		}

		LF_ConfigReader reader = LF_ConfigService.OpenOwnerSection(SECTION);
		if (!reader)
			return;

		ApplyOwner(reader);
		LF_Debug.Info(LF_ConfigService.FEATURE, string.Format("%1: owner config read %2 fields, rejected %3", SECTION, reader.GetAppliedCount(), reader.GetRejectedCount()));
	}

	//------------------------------------------------------------------------------------------------
	//! Applies one pack overlay's `m_aRules` by hideout id. \return Entries changed.
	//! \param[in] root The overlay's root container.
	//! \param[in] source The overlay file, for log lines.
	int ApplyPackContainer(notnull BaseContainer root, string source, LF_ConfigClaims claims = null)
	{
		if (!m_aRules)
			m_aRules = new array<ref LF_SiteRuleDef>();

		LF_SiteRuleList list = new LF_SiteRuleList(this);
		return LF_ConfigOverlay.ApplyContainerList(list, root, "m_aRules", source, claims);
	}

	//------------------------------------------------------------------------------------------------
	//! Applies the owner's "sites" section: `rules` is keyed by hideout id, each record with an
	//! optional "op". A key left out, or a value the reader refuses (it logs the error), keeps the
	//! value below it.
	//! \param[in] reader A reader on the section.
	void ApplyOwner(notnull LF_ConfigReader reader)
	{
		array<string> ownerKeys = {"rules"};
		reader.WarnUnknownKeys(ownerKeys);
		if (!m_aRules)
			m_aRules = new array<ref LF_SiteRuleDef>();

		LF_ConfigOverlay.ApplyRecords(reader, "rules", new LF_SiteRuleList(this), LF_ConfigService.GetOwnerSource());
	}
}

//------------------------------------------------------------------------------------------------
//! One contested site's rules, keyed by the hideout id it guards. The defaults are the default rules.
[BaseContainerProps()]
class LF_SiteRuleDef : LF_ConfigRecord
{
	[Attribute("", UIWidgets.EditBox, "The hideout id the site guards")]
	string m_sHideoutId;

	[Attribute("60", UIWidgets.Auto, "Metres from the site's centre within which a member counts as on the site", params: "1 1000")]
	int m_iRadius;

	[Attribute("20", UIWidgets.Auto, "Metres from the site's centre of the inner ring; a member there counts double. 0 means no inner ring. At most radius", params: "0 1000")]
	int m_iInnerRadius;

	[Attribute("2", UIWidgets.Auto, "Members of one organization that must stand on the site before it gains or captures", params: "1 100")]
	int m_iMinMembersPresent;

	[Attribute("300", UIWidgets.Auto, "Capture progress, in counted-member seconds, at which the site changes hands", params: "1 86400")]
	int m_iCaptureSeconds;

	[Attribute("600", UIWidgets.Auto, "Seconds of capture progress lost per hour while nobody stands on the site. 0 means progress never fades", params: "0 1000000")]
	int m_iDecayPerHour;

	[Attribute("1800", UIWidgets.Auto, "Seconds after the site changes hands in which no new capture completes. 0 means no cooldown", params: "0 604800")]
	int m_iFlipCooldownSeconds;

	[Attribute("600", UIWidgets.Auto, "Cash the holder earns per hour of holding the site. 0 earns nothing", params: "0 1000000")]
	int m_iRentPerHour;

	[Attribute("3000", UIWidgets.Auto, "Most cash a site banks for its holder before it is collected", params: "0 100000000")]
	int m_iMaxRentBanked;

	[Attribute("300", UIWidgets.Auto, "Seconds a member stays counted as active after the last thing they did on the site", params: "1 86400")]
	int m_iActiveWindowSeconds;
}

//------------------------------------------------------------------------------------------------
//! The rules of one site as a system reads them: plain values, handed out by LF_SitesConfig.For().
//! The defaults are the default rules, so a hideout with no entry still has working rules.
class LF_SiteSettings
{
	//! True when the config has an entry for the hideout id; false means these are the defaults.
	bool m_bConfigured;
	int m_iRadius = LF_SitesConfig.DEFAULT_RADIUS;
	int m_iInnerRadius = LF_SitesConfig.DEFAULT_INNER_RADIUS;
	int m_iMinMembersPresent = LF_SitesConfig.DEFAULT_MIN_MEMBERS;
	int m_iCaptureSeconds = LF_SitesConfig.DEFAULT_CAPTURE_SECONDS;
	int m_iDecayPerHour = LF_SitesConfig.DEFAULT_DECAY_PER_HOUR;
	int m_iFlipCooldownSeconds = LF_SitesConfig.DEFAULT_FLIP_COOLDOWN_SECONDS;
	int m_iRentPerHour = LF_SitesConfig.DEFAULT_RENT_PER_HOUR;
	int m_iMaxRentBanked = LF_SitesConfig.DEFAULT_MAX_RENT_BANKED;
	int m_iActiveWindowSeconds = LF_SitesConfig.DEFAULT_ACTIVE_WINDOW_SECONDS;

	//------------------------------------------------------------------------------------------------
	//! The scoring rules LF_SiteHold.Step takes: this site's member minimum, capture time, decay and
	//! flip cooldown over LF_SiteRules.Default() (the counted-member cap and the hold points stay its).
	LF_SiteRules ToHoldRules()
	{
		LF_SiteRules rules = LF_SiteRules.Default();
		rules.m_iMinMembersPresent = m_iMinMembersPresent;
		rules.m_fCaptureSeconds = m_iCaptureSeconds;
		rules.m_fDecayPerHour = m_iDecayPerHour;
		rules.m_fFlipCooldownSeconds = m_iFlipCooldownSeconds;
		return rules;
	}
}

//------------------------------------------------------------------------------------------------
//! The sites `rules` list for the shared merge (LF_ConfigOverlay), keyed by hideout id. A new or
//! replaced entry starts from the default rules.
class LF_SiteRuleList : LF_ConfigList
{
	protected LF_SitesConfig m_Config;

	void LF_SiteRuleList(LF_SitesConfig config)
	{
		m_Config = config;
		if (!m_Config.m_aRules)
			m_Config.m_aRules = new array<ref LF_SiteRuleDef>();
	}

	override string GetName()
	{
		return "sites.rules";
	}

	override int Count()
	{
		return m_Config.m_aRules.Count();
	}

	override LF_ConfigRecord Get(int index)
	{
		return m_Config.m_aRules[index];
	}

	override string IdOf(notnull LF_ConfigRecord record)
	{
		return LF_SiteRuleDef.Cast(record).m_sHideoutId;
	}

	override string IdOfContainer(notnull BaseContainer container)
	{
		string id;
		container.Get("m_sHideoutId", id);
		return id;
	}

	override string IdHint()
	{
		return "a hideout id";
	}

	override LF_ConfigRecord NewRecord(string id)
	{
		LF_SiteRuleDef rule = new LF_SiteRuleDef();
		rule.m_sHideoutId = id;
		rule.m_iRadius = LF_SitesConfig.DEFAULT_RADIUS;
		rule.m_iInnerRadius = LF_SitesConfig.DEFAULT_INNER_RADIUS;
		rule.m_iMinMembersPresent = LF_SitesConfig.DEFAULT_MIN_MEMBERS;
		rule.m_iCaptureSeconds = LF_SitesConfig.DEFAULT_CAPTURE_SECONDS;
		rule.m_iDecayPerHour = LF_SitesConfig.DEFAULT_DECAY_PER_HOUR;
		rule.m_iFlipCooldownSeconds = LF_SitesConfig.DEFAULT_FLIP_COOLDOWN_SECONDS;
		rule.m_iRentPerHour = LF_SitesConfig.DEFAULT_RENT_PER_HOUR;
		rule.m_iMaxRentBanked = LF_SitesConfig.DEFAULT_MAX_RENT_BANKED;
		rule.m_iActiveWindowSeconds = LF_SitesConfig.DEFAULT_ACTIVE_WINDOW_SECONDS;
		return rule;
	}

	override void Set(int index, notnull LF_ConfigRecord record)
	{
		LF_SiteRuleDef rule = LF_SiteRuleDef.Cast(record);
		if (index < m_Config.m_aRules.Count())
			m_Config.m_aRules.Set(index, rule);
		else
			m_Config.m_aRules.Insert(rule);
	}

	override void RemoveAt(int index)
	{
		m_Config.m_aRules.RemoveOrdered(index);
	}

	override string Incomplete(notnull LF_ConfigRecord record)
	{
		LF_SiteRuleDef rule = LF_SiteRuleDef.Cast(record);
		if (rule.m_sHideoutId.IsEmpty())
			return "the entry has no hideout id";

		return RangeProblem(rule);
	}

	//------------------------------------------------------------------------------------------------
	//! Reads one int of a pack entry when it sets it directly; a value outside min..max is logged and
	//! the value below it stays.
	protected static bool PatchInt(notnull BaseContainer container, string member, inout int value, int min, int max)
	{
		if (!container.IsVariableSetDirectly(member))
			return false;

		int read;
		if (!container.Get(member, read))
			return false;

		if (read < min || read > max)
		{
			LF_Debug.Error(LF_ConfigService.FEATURE, string.Format("sites.rules: pack value %1 %2 is outside %3..%4; keeping %5", member, read, min, max, value));
			return false;
		}

		value = read;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override int PatchFromContainer(notnull LF_ConfigRecord record, notnull BaseContainer container)
	{
		LF_SiteRuleDef rule = LF_SiteRuleDef.Cast(record);
		int changed;
		if (PatchInt(container, "m_iRadius", rule.m_iRadius, LF_SitesConfig.RADIUS_MIN, LF_SitesConfig.RADIUS_MAX))
			changed++;
		if (PatchInt(container, "m_iInnerRadius", rule.m_iInnerRadius, LF_SitesConfig.INNER_RADIUS_MIN, LF_SitesConfig.RADIUS_MAX))
			changed++;
		if (PatchInt(container, "m_iMinMembersPresent", rule.m_iMinMembersPresent, LF_SitesConfig.MEMBERS_MIN, LF_SitesConfig.MEMBERS_MAX))
			changed++;
		if (PatchInt(container, "m_iCaptureSeconds", rule.m_iCaptureSeconds, LF_SitesConfig.CAPTURE_MIN, LF_SitesConfig.CAPTURE_MAX))
			changed++;
		if (PatchInt(container, "m_iDecayPerHour", rule.m_iDecayPerHour, LF_SitesConfig.DECAY_MIN, LF_SitesConfig.DECAY_MAX))
			changed++;
		if (PatchInt(container, "m_iFlipCooldownSeconds", rule.m_iFlipCooldownSeconds, LF_SitesConfig.COOLDOWN_MIN, LF_SitesConfig.COOLDOWN_MAX))
			changed++;
		if (PatchInt(container, "m_iRentPerHour", rule.m_iRentPerHour, LF_SitesConfig.RENT_MIN, LF_SitesConfig.RENT_MAX))
			changed++;
		if (PatchInt(container, "m_iMaxRentBanked", rule.m_iMaxRentBanked, LF_SitesConfig.RENT_BANKED_MIN, LF_SitesConfig.RENT_BANKED_MAX))
			changed++;
		if (PatchInt(container, "m_iActiveWindowSeconds", rule.m_iActiveWindowSeconds, LF_SitesConfig.WINDOW_MIN, LF_SitesConfig.WINDOW_MAX))
			changed++;

		KeepInnerInsideRadius(rule);
		return changed;
	}

	//------------------------------------------------------------------------------------------------
	override void PatchFromReader(notnull LF_ConfigRecord record, notnull LF_ConfigReader reader)
	{
		LF_SiteRuleDef rule = LF_SiteRuleDef.Cast(record);
		array<string> recordKeys = {"op", "radius", "innerRadius", "minMembersPresent", "captureSeconds", "decayPerHour", "flipCooldownSeconds", "rentPerHour", "maxRentBanked", "activeWindowSeconds"};
		reader.WarnUnknownKeys(recordKeys);
		reader.ReadInt("radius", rule.m_iRadius, LF_SitesConfig.RADIUS_MIN, LF_SitesConfig.RADIUS_MAX);
		reader.ReadInt("innerRadius", rule.m_iInnerRadius, LF_SitesConfig.INNER_RADIUS_MIN, LF_SitesConfig.RADIUS_MAX);
		reader.ReadInt("minMembersPresent", rule.m_iMinMembersPresent, LF_SitesConfig.MEMBERS_MIN, LF_SitesConfig.MEMBERS_MAX);
		reader.ReadInt("captureSeconds", rule.m_iCaptureSeconds, LF_SitesConfig.CAPTURE_MIN, LF_SitesConfig.CAPTURE_MAX);
		reader.ReadInt("decayPerHour", rule.m_iDecayPerHour, LF_SitesConfig.DECAY_MIN, LF_SitesConfig.DECAY_MAX);
		reader.ReadInt("flipCooldownSeconds", rule.m_iFlipCooldownSeconds, LF_SitesConfig.COOLDOWN_MIN, LF_SitesConfig.COOLDOWN_MAX);
		reader.ReadInt("rentPerHour", rule.m_iRentPerHour, LF_SitesConfig.RENT_MIN, LF_SitesConfig.RENT_MAX);
		reader.ReadInt("maxRentBanked", rule.m_iMaxRentBanked, LF_SitesConfig.RENT_BANKED_MIN, LF_SitesConfig.RENT_BANKED_MAX);
		reader.ReadInt("activeWindowSeconds", rule.m_iActiveWindowSeconds, LF_SitesConfig.WINDOW_MIN, LF_SitesConfig.WINDOW_MAX);
		KeepInnerInsideRadius(rule);
	}

	//------------------------------------------------------------------------------------------------
	//! An inner ring wider than the site is a mistake: log it and make the ring as wide as the site.
	protected static void KeepInnerInsideRadius(notnull LF_SiteRuleDef rule)
	{
		if (rule.m_iInnerRadius <= rule.m_iRadius)
			return;

		LF_Debug.Error("Sites", string.Format("site '%1': innerRadius %2 is above radius %3; using %3", rule.m_sHideoutId, rule.m_iInnerRadius, rule.m_iRadius));
		rule.m_iInnerRadius = rule.m_iRadius;
	}

	//------------------------------------------------------------------------------------------------
	//! Why a whole new entry cannot stand (a pack entry is built from its container, so its values
	//! are not range-checked until here), or empty.
	protected static string RangeProblem(notnull LF_SiteRuleDef rule)
	{
		if (rule.m_iRadius < LF_SitesConfig.RADIUS_MIN || rule.m_iRadius > LF_SitesConfig.RADIUS_MAX)
			return string.Format("radius %1 is outside %2..%3", rule.m_iRadius, LF_SitesConfig.RADIUS_MIN, LF_SitesConfig.RADIUS_MAX);
		if (rule.m_iMinMembersPresent < LF_SitesConfig.MEMBERS_MIN || rule.m_iMinMembersPresent > LF_SitesConfig.MEMBERS_MAX)
			return string.Format("minMembersPresent %1 is outside %2..%3", rule.m_iMinMembersPresent, LF_SitesConfig.MEMBERS_MIN, LF_SitesConfig.MEMBERS_MAX);
		if (rule.m_iCaptureSeconds < LF_SitesConfig.CAPTURE_MIN || rule.m_iCaptureSeconds > LF_SitesConfig.CAPTURE_MAX)
			return string.Format("captureSeconds %1 is outside %2..%3", rule.m_iCaptureSeconds, LF_SitesConfig.CAPTURE_MIN, LF_SitesConfig.CAPTURE_MAX);
		if (rule.m_iActiveWindowSeconds < LF_SitesConfig.WINDOW_MIN || rule.m_iActiveWindowSeconds > LF_SitesConfig.WINDOW_MAX)
			return string.Format("activeWindowSeconds %1 is outside %2..%3", rule.m_iActiveWindowSeconds, LF_SitesConfig.WINDOW_MIN, LF_SitesConfig.WINDOW_MAX);

		KeepInnerInsideRadius(rule);
		return string.Empty;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_SitesConfig's statics (LF_StaticReset finds it by reflection).
class LF_SitesConfig_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_SitesConfig";
	}

	override void Apply()
	{
		LF_SitesConfig.Reload();
	}
}
