//------------------------------------------------------------------------------------------------
//! One seizure rule (#3428): a crime code, the licences a conviction for it takes, and how long the
//! holder is banned from getting them back, in in-game days. The crime code is the offence id the
//! crime catalog and the conviction record already carry (LF_CrimeEntry.m_sCrimeId).
[BaseContainerProps()]
class LF_LicenceSeizureRule : LF_ConfigRecord
{
	[Attribute("", UIWidgets.EditBox, "Offence id this rule belongs to, as LF_CrimeCatalog spells it (#LF-Ticket_OffenceBrandishing)")]
	string m_sCrimeId;

	[Attribute("", UIWidgets.Object, "Licence names the conviction takes, as LF_ELicenseType spells them (FIREARM_PERMIT, DRIVING_LICENCE)")]
	ref array<string> m_aLicences;

	[Attribute("0", UIWidgets.Auto, "In-game days the holder cannot get each of these licences back. 0 seizes with no ban", params: "0 3650")]
	int m_iBanDays;
}

//------------------------------------------------------------------------------------------------
//! One ban a conviction hands out: a licence and the in-game days before the holder may have it again.
class LF_LicenceBan
{
	LF_ELicenseType m_eLicence;
	int m_iDays;

	static LF_LicenceBan Make(LF_ELicenseType licence, int days)
	{
		LF_LicenceBan ban = new LF_LicenceBan();
		ban.m_eLicence = licence;
		ban.m_iDays = days;
		return ban;
	}
}

//------------------------------------------------------------------------------------------------
//! The seizure rules config: Configs/Crime/LF_LicenceSeizure.conf with the pack overlays layered over
//! it (LF_ConfigOverlay, #962). Server only; no client reads it. This slice is the rule table and the
//! lookup; what takes the licence from a character at conviction calls BansFor.
[BaseContainerProps(configRoot: true)]
class LF_LicenceSeizureRules
{
	static const ResourceName CONFIG_PATH = "{AAE3CE9E3D8CA45A}Configs/Crime/LF_LicenceSeizure.conf";
	static const string FEATURE = "License";
	static const string SECTION = "licenceSeizure";
	static const int BAN_DAYS_MAX = 3650;	// lf-fixed: the schema limit, mirrors the attribute params

	[Attribute("", UIWidgets.Object, "Seizure rules, keyed by crime id")]
	ref array<ref LF_LicenceSeizureRule> m_aRules;

	protected static ref LF_LicenceSeizureRules s_Config;
	protected static bool s_bLoadAttempted;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_Config = null;
		s_bLoadAttempted = false;
	}

	//------------------------------------------------------------------------------------------------
	//! The loaded and layered rules, or null (logged once) when the file does not resolve.
	static LF_LicenceSeizureRules Get()
	{
		if (s_bLoadAttempted)
			return s_Config;

		s_bLoadAttempted = true;
		Resource container = BaseContainerTools.LoadContainer(CONFIG_PATH);
		if (!container || !container.GetResource())
		{
			LF_Debug.Error(FEATURE, "licence seizure rules do not resolve, no conviction takes a licence: " + CONFIG_PATH);
			return null;
		}

		s_Config = LF_LicenceSeizureRules.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (!s_Config)
			LF_Debug.Error(FEATURE, "licence seizure rules have an unexpected root class: " + CONFIG_PATH);
		else
			s_Config.ApplyLayers();

		return s_Config;
	}

	//------------------------------------------------------------------------------------------------
	//! Merges each pack overlay of this class over the core file, in load order.
	void ApplyLayers()
	{
		if (!m_aRules)
			m_aRules = new array<ref LF_LicenceSeizureRule>();

		array<ref Resource> resources = {};
		array<ResourceName> names = {};
		LF_ConfigOverlay.LoadPackOverlays(CONFIG_PATH, LF_LicenceSeizureRules, SECTION, resources, names);
		LF_ConfigClaims claims = new LF_ConfigClaims();
		LF_LicenceSeizureList list = new LF_LicenceSeizureList(this);
		foreach (int i, Resource resource : resources)
		{
			int changed = LF_ConfigOverlay.ApplyContainerList(list, resource.GetResource().ToBaseContainer(), "m_aRules", names[i], claims);
			LF_Debug.Info(LF_ConfigService.FEATURE, string.Format("licenceSeizure: pack overlay %1 changed %2 rules", names[i], changed));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The rule for a crime id, or null.
	LF_LicenceSeizureRule Find(string crimeId)
	{
		if (!m_aRules)
			return null;

		foreach (LF_LicenceSeizureRule rule : m_aRules)
		{
			if (rule && rule.m_sCrimeId == crimeId)
				return rule;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! The bans a conviction for these crime codes hands out: one per licence the mapped codes take,
	//! with the longest ban when two codes take the same licence. A code with no rule adds nothing; a
	//! licence name that is not an LF_ELicenseType is logged and skipped. Order follows the first
	//! mention of each licence.
	//! \param[in] crimeIds The offence ids of the conviction.
	//! \param[out] bans The result, cleared first.
	void CollectBans(notnull array<string> crimeIds, notnull array<ref LF_LicenceBan> bans)
	{
		bans.Clear();
		foreach (string crimeId : crimeIds)
		{
			LF_LicenceSeizureRule rule = Find(crimeId);
			if (!rule || !rule.m_aLicences)
				continue;

			int days = Math.ClampInt(rule.m_iBanDays, 0, BAN_DAYS_MAX);
			foreach (string name : rule.m_aLicences)
			{
				LF_ELicenseType licence = typename.StringToEnum(LF_ELicenseType, name);
				if (typename.EnumToString(LF_ELicenseType, licence) != name)
				{
					LF_Debug.Warn(FEATURE, string.Format("licence seizure rule %1 names '%2', which is not a licence; skipped", crimeId, name));
					continue;
				}

				AddLongest(bans, licence, days);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static void AddLongest(notnull array<ref LF_LicenceBan> bans, LF_ELicenseType licence, int days)
	{
		foreach (LF_LicenceBan ban : bans)
		{
			if (ban.m_eLicence != licence)
				continue;

			if (days > ban.m_iDays)
				ban.m_iDays = days;

			return;
		}

		bans.Insert(LF_LicenceBan.Make(licence, days));
	}

	//------------------------------------------------------------------------------------------------
	//! The bans for a conviction against the shipped and layered rules (see CollectBans). Empty when
	//! the config does not resolve.
	static void BansFor(notnull array<string> crimeIds, notnull array<ref LF_LicenceBan> bans)
	{
		bans.Clear();
		LF_LicenceSeizureRules config = Get();
		if (config)
			config.CollectBans(crimeIds, bans);
	}

	//------------------------------------------------------------------------------------------------
	//! The ban in in-game days a conviction for these codes puts on one licence, or -1 when none of them takes it.
	static int BanDays(notnull array<string> crimeIds, LF_ELicenseType licence)
	{
		array<ref LF_LicenceBan> bans = {};
		BansFor(crimeIds, bans);
		foreach (LF_LicenceBan ban : bans)
		{
			if (ban.m_eLicence == licence)
				return ban.m_iDays;
		}

		return -1;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_LicenceSeizureRules' statics (LF_StaticReset finds it by reflection).
class LF_LicenceSeizureRules_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_LicenceSeizureRules";
	}

	override void Apply()
	{
		LF_LicenceSeizureRules.ResetStatics();
	}
}

//------------------------------------------------------------------------------------------------
//! The seizure `m_aRules` list for the shared merge (LF_ConfigOverlay), keyed by crime id.
class LF_LicenceSeizureList : LF_ConfigList
{
	protected LF_LicenceSeizureRules m_Config;

	void LF_LicenceSeizureList(LF_LicenceSeizureRules config)
	{
		m_Config = config;
		if (!m_Config.m_aRules)
			m_Config.m_aRules = new array<ref LF_LicenceSeizureRule>();
	}

	override string GetName()
	{
		return "licenceSeizure.rules";
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
		return LF_LicenceSeizureRule.Cast(record).m_sCrimeId;
	}

	override string IdOfContainer(notnull BaseContainer container)
	{
		string id;
		container.Get("m_sCrimeId", id);
		return id;
	}

	override string IdHint()
	{
		return "a crime id";
	}

	override LF_ConfigRecord NewRecord(string id)
	{
		LF_LicenceSeizureRule rule = new LF_LicenceSeizureRule();
		rule.m_sCrimeId = id;
		rule.m_aLicences = {};
		return rule;
	}

	override void Set(int index, notnull LF_ConfigRecord record)
	{
		LF_LicenceSeizureRule rule = LF_LicenceSeizureRule.Cast(record);
		if (index < m_Config.m_aRules.Count())
			m_Config.m_aRules.Set(index, rule);
		else
			m_Config.m_aRules.Insert(rule);
	}

	override void RemoveAt(int index)
	{
		m_Config.m_aRules.RemoveOrdered(index);
	}

	override int PatchFromContainer(notnull LF_ConfigRecord record, notnull BaseContainer container)
	{
		LF_LicenceSeizureRule rule = LF_LicenceSeizureRule.Cast(record);
		int changed;
		if (container.IsVariableSetDirectly("m_aLicences"))
		{
			array<string> licences = {};
			if (container.Get("m_aLicences", licences))
			{
				rule.m_aLicences = {};
				rule.m_aLicences.Copy(licences);
				changed++;
			}
		}
		if (container.IsVariableSetDirectly("m_iBanDays") && container.Get("m_iBanDays", rule.m_iBanDays))
			changed++;
		return changed;
	}
}
