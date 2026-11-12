//------------------------------------------------------------------------------------------------
//! One offence of the crime catalog (#685a). The id is the offence key a report, the wanted board,
//! the dispatch radio and the saved crime record (LF_CrimeRecordRow.m_sOffence) already carry, so a
//! save needs no migration. Core ships the structure and today's offences (Configs/Crime/
//! LF_CrimeCatalog.conf); a pack or the owner's `crime` section changes the numbers.
[BaseContainerProps()]
class LF_CrimeEntry
{
	[Attribute("", UIWidgets.EditBox, "Stable offence id, a localization key such as #LF-Offence_Robbery. Saved crime records are keyed by it, so never rename one")]
	string m_sCrimeId;

	[Attribute("1", UIWidgets.Auto, "Wanted levels the crime adds when it is committed. Wanted caps at 5", params: "0 5")]
	int m_iWantedAdd;

	[Attribute("0", UIWidgets.Auto, "Fixed fine in cash. Above 0 the offence is listed on the officer's ticket form, in catalog order", params: "0 1000000")]
	int m_iFine;

	[Attribute("0", UIWidgets.Auto, "Jail time in seconds when this offence is on the prisoner's reports. 0 leaves the sentence to the wanted level; otherwise the longer of the two applies", params: "0 86400")]
	int m_iSentenceSeconds;

	[Attribute("0", UIWidgets.Auto, "Completed crimes of this kind after which a repeat adds wanted levels. 0 turns the repeat rule off", params: "0 1000")]
	int m_iRepeatThreshold;

	[Attribute("0", UIWidgets.Auto, "Extra wanted levels a repeat adds on top of the normal amount", params: "0 5")]
	int m_iRepeatWantedAdd;

	//------------------------------------------------------------------------------------------------
	static LF_CrimeEntry Make(string crimeId, int wantedAdd, int fine)
	{
		LF_CrimeEntry entry = new LF_CrimeEntry();
		entry.m_sCrimeId = crimeId;
		entry.m_iWantedAdd = wantedAdd;
		entry.m_iFine = fine;
		return entry;
	}
}

//------------------------------------------------------------------------------------------------
//! The crime catalog config: Configs/Crime/LF_CrimeCatalog.conf, with the owner's `crime` section
//! layered over it. Server only; no client reads it, so it is not in the config stream.
[BaseContainerProps(configRoot: true)]
class LF_CrimeCatalogConfig
{
	static const ResourceName CONFIG_PATH = "{6D2A94E1B07C53F8}Configs/Crime/LF_CrimeCatalog.conf";

	//! The owner config.json section this class reads (LF_ConfigService).
	static const string SECTION = "crime";

	static const int WANTED_MIN = 0;	// lf-fixed: the schema limit, mirrors the attribute params
	static const int WANTED_MAX = 5;	// lf-fixed: the schema limit, wanted caps at 5
	static const int FINE_MIN = 0;	// lf-fixed: the schema limit, mirrors the attribute params
	static const int FINE_MAX = 1000000;	// lf-fixed: the schema limit, mirrors the attribute params
	static const int SENTENCE_MIN = 0;	// lf-fixed: the schema limit, mirrors the attribute params
	static const int SENTENCE_MAX = 86400;	// lf-fixed: the schema limit, one day
	static const int THRESHOLD_MIN = 0;	// lf-fixed: the schema limit, mirrors the attribute params
	static const int THRESHOLD_MAX = 1000;	// lf-fixed: the schema limit, mirrors the attribute params

	[Attribute("", UIWidgets.Object, "Offences, in order. The ticket form lists the ones with a fine in this order")]
	ref array<ref LF_CrimeEntry> m_aCrimes;

	protected static ref LF_CrimeCatalogConfig s_Config;
	protected static bool s_bLoadAttempted;

	//------------------------------------------------------------------------------------------------
	//! The loaded and layered catalog, or null (logged once) when the file does not resolve.
	static LF_CrimeCatalogConfig Get()
	{
		if (s_bLoadAttempted)
			return s_Config;

		s_bLoadAttempted = true;
		Resource container = BaseContainerTools.LoadContainer(CONFIG_PATH);
		if (!container || !container.GetResource())
		{
			LF_Debug.Error(LF_CrimeCatalog.FEATURE, string.Format("crime catalog does not resolve, every offence falls back to %1 wanted: %2", LF_CrimeCatalog.UNKNOWN_WANTED, CONFIG_PATH));
			return null;
		}

		s_Config = LF_CrimeCatalogConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (!s_Config)
			LF_Debug.Error(LF_CrimeCatalog.FEATURE, string.Format("crime catalog has an unexpected root class: %1", CONFIG_PATH));
		else
			s_Config.ApplyLayers();

		return s_Config;
	}

	//------------------------------------------------------------------------------------------------
	//! Drops the cached instance so the next Get() loads the core file and layers it again
	//! (server after /configreload).
	static void Reload()
	{
		s_Config = null;
		s_bLoadAttempted = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Merges the owner's "crime" section over this instance, which holds the core `.conf`.
	void ApplyLayers()
	{
		LF_ConfigReader reader = LF_ConfigService.OpenOwnerSection(SECTION);
		if (!reader)
			return;

		ApplyOwner(reader);
		LF_Debug.Info(LF_ConfigService.FEATURE, string.Format("crime: owner config read %1 fields, rejected %2", reader.GetAppliedCount(), reader.GetRejectedCount()));
	}

	//------------------------------------------------------------------------------------------------
	//! Applies the owner's "crime" section: `crimes` is an object keyed by the crime id. A record
	//! changes the entry with that id field by field; an id no entry has adds one at the end.
	//! \param[in] reader A reader on the section.
	void ApplyOwner(notnull LF_ConfigReader reader)
	{
		array<string> ownerKeys = {"crimes"};
		reader.WarnUnknownKeys(ownerKeys);

		if (!m_aCrimes)
			m_aCrimes = new array<ref LF_CrimeEntry>();

		int count = reader.BeginRecords("crimes");
		for (int i = 0; i < count; i++)
		{
			string key = reader.GetRecordKey(i);
			if (!reader.BeginRecord(key))
				continue;

			if (key.IsEmpty())
			{
				reader.RejectRecord("the key is empty; it is the crime id");
				reader.EndRecord();
				continue;
			}

			LF_CrimeEntry entry = Find(key);
			if (!entry)
			{
				entry = LF_CrimeEntry.Make(key, 1, 0);
				m_aCrimes.Insert(entry);
			}

			array<string> recordKeys = {"wantedAdd", "fine", "sentenceSeconds", "repeatThreshold", "repeatWantedAdd"};
			reader.WarnUnknownKeys(recordKeys);
			reader.ReadInt("wantedAdd", entry.m_iWantedAdd, WANTED_MIN, WANTED_MAX);
			reader.ReadInt("fine", entry.m_iFine, FINE_MIN, FINE_MAX);
			reader.ReadInt("sentenceSeconds", entry.m_iSentenceSeconds, SENTENCE_MIN, SENTENCE_MAX);
			reader.ReadInt("repeatThreshold", entry.m_iRepeatThreshold, THRESHOLD_MIN, THRESHOLD_MAX);
			reader.ReadInt("repeatWantedAdd", entry.m_iRepeatWantedAdd, WANTED_MIN, WANTED_MAX);
			reader.EndRecord();
		}

		if (count >= 0)
			reader.EndRecords();
	}

	//------------------------------------------------------------------------------------------------
	//! The entry with this id, or null.
	LF_CrimeEntry Find(string crimeId)
	{
		if (!m_aCrimes)
			return null;

		foreach (LF_CrimeEntry entry : m_aCrimes)
		{
			if (entry && entry.m_sCrimeId == crimeId)
				return entry;
		}

		return null;
	}
}

//------------------------------------------------------------------------------------------------
//! The one place a crime's numbers are read (#685a). Call sites pass the crime id and never keep
//! a wanted amount, fine or sentence of their own. Server logic; the repeat rule reads the hidden
//! crime record (LF_CrimeRecord.Count), which is the saved {uid, offence, count}.
class LF_CrimeCatalog
{
	static const string FEATURE = "CrimeCatalog";

	//! Wanted an id the catalog does not list adds: what a custom heist offence did before the catalog.
	static const int UNKNOWN_WANTED = 1;	// lf-fixed: the fallback for an id the catalog does not list, not a crime's value

	protected static ref map<string, bool> s_mWarned;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_mWarned = null;
	}

	//------------------------------------------------------------------------------------------------
	//! The entry for an id, or null when the catalog is missing or does not list it. An unlisted id
	//! logs one warning per id.
	static LF_CrimeEntry Lookup(string crimeId)
	{
		LF_CrimeCatalogConfig config = LF_CrimeCatalogConfig.Get();
		LF_CrimeEntry entry;
		if (config)
			entry = config.Find(crimeId);

		if (!entry)
			WarnUnknown(crimeId);

		return entry;
	}

	//------------------------------------------------------------------------------------------------
	protected static void WarnUnknown(string crimeId)
	{
		if (!s_mWarned)
			s_mWarned = new map<string, bool>();

		if (s_mWarned.Contains(crimeId))
			return;

		s_mWarned.Set(crimeId, true);
		LF_Debug.Warn(FEATURE, string.Format("crime '%1' is not in the catalog; it adds %2 wanted and has no fine or sentence of its own", crimeId, UNKNOWN_WANTED));
	}

	//------------------------------------------------------------------------------------------------
	//! Wanted levels the crime adds, without the repeat rule.
	static int BaseWanted(string crimeId)
	{
		LF_CrimeEntry entry = Lookup(crimeId);
		if (!entry)
			return UNKNOWN_WANTED;

		return Math.ClampInt(entry.m_iWantedAdd, LF_CrimeCatalogConfig.WANTED_MIN, LF_CrimeCatalogConfig.WANTED_MAX);
	}

	//------------------------------------------------------------------------------------------------
	//! Wanted levels this account gets for the crime now: the base amount, plus the repeat amount once
	//! the account's record holds `repeatThreshold` of this crime already. Call it before the report
	//! that counts this crime.
	static int WantedFor(string playerUid, string crimeId)
	{
		int wanted = BaseWanted(crimeId);
		LF_CrimeEntry entry = Lookup(crimeId);
		if (!entry || entry.m_iRepeatThreshold <= 0 || playerUid.IsEmpty())
			return wanted;

		if (LF_CrimeRecord.Count(playerUid, crimeId) < entry.m_iRepeatThreshold)
			return wanted;

		return Math.ClampInt(wanted + entry.m_iRepeatWantedAdd, LF_CrimeCatalogConfig.WANTED_MIN, LF_CrimeCatalogConfig.WANTED_MAX);
	}

	//------------------------------------------------------------------------------------------------
	//! Raises the account's wanted level by what the catalog says for the crime.
	//! \return The wanted level after, or 0 with no account.
	static int ApplyWanted(LF_PlayerAccount account, string playerUid, string crimeId)
	{
		if (!account)
			return 0;

		int amount = WantedFor(playerUid, crimeId);
		account.IncreaseWantedLevel(amount);
		LF_Debug.Info(FEATURE, string.Format("%1 committed %2: wanted +%3 -> %4", playerUid, crimeId, amount, account.GetWantedLevel()));
		return account.GetWantedLevel();
	}

	//------------------------------------------------------------------------------------------------
	//! The offences with a fine, in catalog order: the officer's ticket form.
	static void CollectTicketOffences(notnull array<ref LF_TicketOffence> outOffences)
	{
		outOffences.Clear();
		LF_CrimeCatalogConfig config = LF_CrimeCatalogConfig.Get();
		if (!config || !config.m_aCrimes)
			return;

		foreach (LF_CrimeEntry entry : config.m_aCrimes)
		{
			if (entry && !entry.m_sCrimeId.IsEmpty() && entry.m_iFine > 0)
				outOffences.Insert(LF_TicketOffence.Create(entry.m_sCrimeId, entry.m_iFine));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The sentence in seconds: `scaledSeconds` (the wanted level times the cell's step), or the
	//! longest `sentenceSeconds` among the offences on the prisoner's open reports when that is longer.
	static int SentenceFor(string playerUid, int scaledSeconds)
	{
		int seconds = scaledSeconds;
		array<ref LF_WantedReportRecord> reports = LF_WantedReports.GetFor(playerUid);
		LF_CrimeCatalogConfig config = LF_CrimeCatalogConfig.Get();
		if (!reports || !config)
			return seconds;

		foreach (LF_WantedReportRecord report : reports)
		{
			if (!report)
				continue;

			LF_CrimeEntry entry = config.Find(report.m_sOffence);
			if (entry && entry.m_iSentenceSeconds > seconds)
				seconds = entry.m_iSentenceSeconds;
		}

		return seconds;
	}
}
