//------------------------------------------------------------------------------------------------
//! One licence's owner-settable flags. The record key is the licence name (LF_ELicenseType).
[BaseContainerProps()]
class LF_LicenceRuleDef : LF_ConfigRecord
{
	[Attribute("", UIWidgets.EditBox, "Licence name, as LF_ELicenseType spells it (POLICE_ACCESS, MINER_GOLD, ...)")]
	string m_sLicence;

	[Attribute("0", UIWidgets.CheckBox, "Granted free when a character starts with no licences")]
	bool m_bIsInitialLicense;

	[Attribute("0", UIWidgets.CheckBox, "Needs the job whitelist before it can be unlocked")]
	bool m_bRequiresWhitelist;
}

//------------------------------------------------------------------------------------------------
//! The owner's `licences` section (D8): which licences a new character starts with and which
//! need the job whitelist. The catalog is built in LF_LicenseManagerComponent (names, descriptions,
//! the job-level gate); the gate level is the `jobs` section's `licenceGates` (#749), so it is not
//! repeated here. Server only: the licence manager runs on the server and clients read the
//! unlocked licences from the replicated mask, so nothing is in the config stream. A character
//! created after a change picks it up; a change does not take a licence from one who holds it.
[BaseContainerProps(configRoot: true)]
class LF_LicencesConfig
{
	//! The owner config.json section this class reads (LF_ConfigService).
	static const string SECTION = "licences";

	[Attribute("", UIWidgets.Object, "Per-licence flags, keyed by licence name. A licence with no row keeps the catalog's flags")]
	ref array<ref LF_LicenceRuleDef> m_aRules;

	//------------------------------------------------------------------------------------------------
	//! Layers the pack overlays of this class (load order), then the owner's "licences" section, over
	//! a freshly built catalog (LF_ConfigOverlay, #1108).
	//! \param[in] catalog The catalog the licence manager just built.
	static void ApplyTo(notnull map<LF_ELicenseType, ref LF_LicenseConfig> catalog)
	{
		LF_LicencesConfig config = new LF_LicencesConfig();
		config.LoadFrom(catalog);
		LF_LicenceRuleList list = new LF_LicenceRuleList(config, catalog);

		// No core file: the catalog is built in script, so no mission overlay can be the core itself.
		array<ref Resource> resources = {};
		array<ResourceName> names = {};
		LF_ConfigOverlay.LoadPackOverlays(ResourceName.Empty, LF_LicencesConfig, SECTION, resources, names);
		LF_ConfigClaims claims = new LF_ConfigClaims();
		foreach (int i, Resource resource : resources)
		{
			int changed = LF_ConfigOverlay.ApplyContainerList(list, resource.GetResource().ToBaseContainer(), "m_aRules", names[i], claims);
			LF_Debug.Info(LF_ConfigService.FEATURE, string.Format("licences: pack overlay %1 changed %2 rules", names[i], changed));
		}

		LF_ConfigReader reader = LF_ConfigService.OpenOwnerSection(SECTION);
		if (reader)
		{
			config.ApplyOwner(reader, catalog);
			LF_Debug.Info(LF_ConfigService.FEATURE, string.Format("licences: owner config read %1 fields, rejected %2", reader.GetAppliedCount(), reader.GetRejectedCount()));
		}

		config.WriteTo(catalog);
	}

	//------------------------------------------------------------------------------------------------
	//! Makes one rule per catalog licence, in the catalog's order, holding the catalog's flags.
	void LoadFrom(notnull map<LF_ELicenseType, ref LF_LicenseConfig> catalog)
	{
		m_aRules = new array<ref LF_LicenceRuleDef>();
		foreach (LF_ELicenseType licence : LF_LicenseBits.Order())
		{
			LF_LicenseConfig entry = catalog.Get(licence);
			if (!entry)
				continue;

			LF_LicenceRuleDef rule = new LF_LicenceRuleDef();
			rule.m_sLicence = typename.EnumToString(LF_ELicenseType, licence);
			rule.m_bIsInitialLicense = entry.m_bIsInitialLicense;
			rule.m_bRequiresWhitelist = entry.m_bRequiresWhitelist;
			m_aRules.Insert(rule);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Puts the layered flags back on the catalog.
	void WriteTo(notnull map<LF_ELicenseType, ref LF_LicenseConfig> catalog)
	{
		if (!m_aRules)
			return;

		foreach (LF_LicenceRuleDef rule : m_aRules)
		{
			LF_ELicenseType licence = typename.StringToEnum(LF_ELicenseType, rule.m_sLicence);
			LF_LicenseConfig entry;
			if (typename.EnumToString(LF_ELicenseType, licence) == rule.m_sLicence)
				entry = catalog.Get(licence);

			if (!entry)
				continue;

			entry.m_bIsInitialLicense = rule.m_bIsInitialLicense;
			entry.m_bRequiresWhitelist = rule.m_bRequiresWhitelist;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Applies the owner's "licences" section to the rules LoadFrom made: `rules` is an object keyed
	//! by licence name, each record with an optional "op" (patch changes the flags given; a licence
	//! is a catalog entry, so add of an existing one and every remove are refused; a name that is not a
	//! licence is refused).
	//! \param[in] reader A reader on the section.
	//! \param[in] catalog The catalog the rules were made from.
	void ApplyOwner(notnull LF_ConfigReader reader, notnull map<LF_ELicenseType, ref LF_LicenseConfig> catalog)
	{
		array<string> ownerKeys = {"rules"};
		reader.WarnUnknownKeys(ownerKeys);

		if (!m_aRules)
			LoadFrom(catalog);

		LF_ConfigOverlay.ApplyRecords(reader, "rules", new LF_LicenceRuleList(this, catalog), LF_ConfigService.GetOwnerSource());
	}

	//------------------------------------------------------------------------------------------------
	//! The licences a character with none starts with, in the catalog's order (LF_LicenseBits.Order).
	//! \param[in] catalog The catalog, with the owner's layer applied.
	//! \param[out] initial The licences flagged initial.
	static void InitialLicences(notnull map<LF_ELicenseType, ref LF_LicenseConfig> catalog, notnull array<LF_ELicenseType> initial)
	{
		initial.Clear();
		foreach (LF_ELicenseType licence : LF_LicenseBits.Order())
		{
			LF_LicenseConfig entry = catalog.Get(licence);
			if (entry && entry.m_bIsInitialLicense)
				initial.Insert(licence);
		}
	}
}

//------------------------------------------------------------------------------------------------
//! The licences `rules` list for the shared merge (LF_ConfigOverlay), keyed by licence name. A rule
//! stands for a catalog licence, which LF_LicenseManagerComponent builds in script, so the ids are
//! the catalog's: a name that is not in it is refused, add of an existing one is refused, and a
//! licence cannot be removed (turn its flags off instead). Replace rebuilds the rule with both flags off.
class LF_LicenceRuleList : LF_ConfigList
{
	protected LF_LicencesConfig m_Config;
	protected map<LF_ELicenseType, ref LF_LicenseConfig> m_Catalog;

	void LF_LicenceRuleList(LF_LicencesConfig config, notnull map<LF_ELicenseType, ref LF_LicenseConfig> catalog)
	{
		m_Config = config;
		m_Catalog = catalog;
		if (!m_Config.m_aRules)
			m_Config.m_aRules = new array<ref LF_LicenceRuleDef>();
	}

	override string GetName()
	{
		return "licences.rules";
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
		return LF_LicenceRuleDef.Cast(record).m_sLicence;
	}

	override string IdOfContainer(notnull BaseContainer container)
	{
		string id;
		container.Get("m_sLicence", id);
		return id;
	}

	override string IdHint()
	{
		return "a licence name (LF_ELicenseType)";
	}

	override LF_ConfigRecord NewRecord(string id)
	{
		LF_LicenceRuleDef rule = new LF_LicenceRuleDef();
		rule.m_sLicence = id;
		return rule;
	}

	override void Set(int index, notnull LF_ConfigRecord record)
	{
		LF_LicenceRuleDef rule = LF_LicenceRuleDef.Cast(record);
		if (index < m_Config.m_aRules.Count())
			m_Config.m_aRules.Set(index, rule);
		else
			m_Config.m_aRules.Insert(rule);
	}

	override void RemoveAt(int index)
	{
		m_Config.m_aRules.RemoveOrdered(index);
	}

	override string RemoveRefusal(int index)
	{
		return "a licence cannot be removed: it is a catalog entry, so set isInitialLicense and requiresWhitelist instead";
	}

	override string Incomplete(notnull LF_ConfigRecord record)
	{
		string name = LF_LicenceRuleDef.Cast(record).m_sLicence;
		LF_ELicenseType licence = typename.StringToEnum(LF_ELicenseType, name);
		if (typename.EnumToString(LF_ELicenseType, licence) != name || !m_Catalog.Get(licence))
			return "the id is not a licence in the catalog (LF_ELicenseType)";

		return string.Empty;
	}

	override int PatchFromContainer(notnull LF_ConfigRecord record, notnull BaseContainer container)
	{
		LF_LicenceRuleDef rule = LF_LicenceRuleDef.Cast(record);
		int changed;
		if (container.IsVariableSetDirectly("m_bIsInitialLicense") && container.Get("m_bIsInitialLicense", rule.m_bIsInitialLicense))
			changed++;
		if (container.IsVariableSetDirectly("m_bRequiresWhitelist") && container.Get("m_bRequiresWhitelist", rule.m_bRequiresWhitelist))
			changed++;
		return changed;
	}

	override void PatchFromReader(notnull LF_ConfigRecord record, notnull LF_ConfigReader reader)
	{
		LF_LicenceRuleDef rule = LF_LicenceRuleDef.Cast(record);
		array<string> recordKeys = {"op", "isInitialLicense", "requiresWhitelist"};
		reader.WarnUnknownKeys(recordKeys);
		reader.ReadBool("isInitialLicense", rule.m_bIsInitialLicense);
		reader.ReadBool("requiresWhitelist", rule.m_bRequiresWhitelist);
	}
}
