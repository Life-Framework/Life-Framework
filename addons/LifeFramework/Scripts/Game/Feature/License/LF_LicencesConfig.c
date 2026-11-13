//------------------------------------------------------------------------------------------------
//! One licence's owner-settable flags. The record key is the licence name (LF_ELicenseType).
[BaseContainerProps()]
class LF_LicenceRuleDef
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
	//! Layers the owner's "licences" section over a freshly built catalog.
	//! \param[in] catalog The catalog the licence manager just built.
	static void ApplyTo(notnull map<LF_ELicenseType, ref LF_LicenseConfig> catalog)
	{
		LF_ConfigReader reader = LF_ConfigService.OpenOwnerSection(SECTION);
		if (!reader)
			return;

		LF_LicencesConfig config = new LF_LicencesConfig();
		config.ApplyOwner(reader, catalog);
		LF_Debug.Info(LF_ConfigService.FEATURE, string.Format("licences: owner config read %1 fields, rejected %2", reader.GetAppliedCount(), reader.GetRejectedCount()));
	}

	//------------------------------------------------------------------------------------------------
	//! Applies the owner's "licences" section: `rules` is an object keyed by licence name. A record
	//! changes that licence's flags field by field; a name that is not a licence is refused.
	//! \param[in] reader A reader on the section.
	//! \param[in] catalog The catalog to change in place.
	void ApplyOwner(notnull LF_ConfigReader reader, notnull map<LF_ELicenseType, ref LF_LicenseConfig> catalog)
	{
		array<string> ownerKeys = {"rules"};
		reader.WarnUnknownKeys(ownerKeys);

		int count = reader.BeginRecords("rules");
		for (int i = 0; i < count; i++)
		{
			string key = reader.GetRecordKey(i);
			if (!reader.BeginRecord(key))
				continue;

			LF_ELicenseType licence = typename.StringToEnum(LF_ELicenseType, key);
			LF_LicenseConfig entry;
			if (typename.EnumToString(LF_ELicenseType, licence) == key)
				entry = catalog.Get(licence);

			if (!entry)
			{
				reader.RejectRecord("the key is not a licence name (LF_ELicenseType)");
				reader.EndRecord();
				continue;
			}

			array<string> recordKeys = {"isInitialLicense", "requiresWhitelist"};
			reader.WarnUnknownKeys(recordKeys);
			reader.ReadBool("isInitialLicense", entry.m_bIsInitialLicense);
			reader.ReadBool("requiresWhitelist", entry.m_bRequiresWhitelist);
			reader.EndRecord();
		}

		if (count >= 0)
			reader.EndRecords();
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
