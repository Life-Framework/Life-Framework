//------------------------------------------------------------------------------------------------
//! One job's level ladder (#749, D12): the total XP each level needs. A character's level in a job
//! is 1 plus the thresholds its XP has reached, so a rebalanced table reaches old saves (the save
//! holds XP, never a level).
[BaseContainerProps()]
class LF_JobTrackDef
{
	[Attribute("", UIWidgets.EditBox, "Job id, e.g. CIVILIAN, DELIVERY, FARMER. The record key")]
	string m_sJob;

	[Attribute("", UIWidgets.Auto, "Total XP to reach level 2, 3, ... in rising order; the level is 1 plus the thresholds reached")]
	ref array<int> m_aLevelThresholds;

	[Attribute("0", UIWidgets.Auto, "Most XP one character earns in this job per day of server time (24 h since boot); 0 for no cap", params: "0 100000000")]
	int m_iDailyXpCap;

	//------------------------------------------------------------------------------------------------
	static LF_JobTrackDef Make(string job, notnull array<int> thresholds, int dailyCap = 0)
	{
		LF_JobTrackDef track = new LF_JobTrackDef();
		track.m_sJob = job;
		track.m_aLevelThresholds = {};
		track.m_aLevelThresholds.Copy(thresholds);
		track.m_iDailyXpCap = dailyCap;
		return track;
	}

	//------------------------------------------------------------------------------------------------
	//! \return 1 plus the thresholds `xp` has reached. The count does not depend on the order, so a
	//! table written out of order still gives a level that never falls as XP rises.
	int LevelFor(int xp)
	{
		int level = 1;
		if (!m_aLevelThresholds)
			return level;

		foreach (int threshold : m_aLevelThresholds)
		{
			if (xp >= threshold)
				level++;
		}

		return level;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Total XP that reaches `level`; 0 for level 1, -1 when the table stops below it.
	int XpForLevel(int level)
	{
		if (level <= 1)
			return 0;

		if (!m_aLevelThresholds)
			return -1;

		array<int> sorted = {};
		sorted.Copy(m_aLevelThresholds);
		sorted.Sort();
		if (level - 2 >= sorted.Count())
			return -1;

		return sorted[level - 2];
	}
}

//------------------------------------------------------------------------------------------------
//! XP one in-world action awards (#749): which job it feeds, how much per unit, and how soon the
//! same character may earn it again. The game names the action (LF_JobLevels.Award); the numbers
//! live here.
[BaseContainerProps()]
class LF_JobXpAwardDef
{
	[Attribute("", UIWidgets.EditBox, "Action id the game awards, e.g. delivery_run, fruit_pick, gather. The record key")]
	string m_sAction;

	[Attribute("", UIWidgets.EditBox, "Job id the XP goes to")]
	string m_sJob;

	[Attribute("0", UIWidgets.Auto, "XP for one unit of the action (one run, one fruit)", params: "0 100000")]
	int m_iXp;

	[Attribute("0", UIWidgets.Auto, "Seconds before the same character earns this action's XP again; 0 for none", params: "0 86400")]
	float m_fCooldownSeconds;

	//------------------------------------------------------------------------------------------------
	static LF_JobXpAwardDef Make(string action, string job, int xp, float cooldownSeconds = 0)
	{
		LF_JobXpAwardDef award = new LF_JobXpAwardDef();
		award.m_sAction = action;
		award.m_sJob = job;
		award.m_iXp = xp;
		award.m_fCooldownSeconds = cooldownSeconds;
		return award;
	}
}

//------------------------------------------------------------------------------------------------
//! A licence's job gate (#749, D12): the job level a licence needs. A licence with no row keeps the
//! gate LF_LicenseManagerComponent ships with it (its family's job and its minimum level).
[BaseContainerProps()]
class LF_JobLicenceGateDef
{
	[Attribute("", UIWidgets.EditBox, "Licence name (LF_ELicenseType), e.g. MINER_GOLD. The record key")]
	string m_sLicence;

	[Attribute("", UIWidgets.EditBox, "Job id whose level the licence needs")]
	string m_sJob;

	[Attribute("1", UIWidgets.Auto, "Job level the licence needs", params: "1 100")]
	int m_iMinLevel;

	//------------------------------------------------------------------------------------------------
	static LF_JobLicenceGateDef Make(string licence, string job, int minLevel)
	{
		LF_JobLicenceGateDef gate = new LF_JobLicenceGateDef();
		gate.m_sLicence = licence;
		gate.m_sJob = job;
		gate.m_iMinLevel = minLevel;
		return gate;
	}
}

//------------------------------------------------------------------------------------------------
//! Job pay, job XP and job levels. Loaded from Configs/Jobs/LF_Jobs.conf; systems hold no prices.
//! There is no timer paycheck: civilians earn cash per completed run and from trader sales,
//! police earn duty pay at clock-out, and a broke player collects a benefit at the counter.
[BaseContainerProps(configRoot: true)]
class LF_JobsConfig
{
	static const ResourceName CONFIG_PATH = "{94C638F6312DA36A}Configs/Jobs/LF_Jobs.conf";

	//! The owner config.json section (D8).
	static const string SECTION = "jobs";

	//! The limits of the attributes' `params`; the owner reader refuses a number outside them.
	static const float PAY_RATE_MIN = 0;	// lf-fixed: the schema limit, mirrors the attribute params
	static const float PAY_RATE_MAX = 100;	// lf-fixed: the schema limit, mirrors the attribute params
	static const int CASH_MIN = 0;	// lf-fixed: the schema limit, mirrors the attribute params
	static const int CASH_MAX = 1000000;	// lf-fixed: the schema limit, mirrors the attribute params
	static const float COOLDOWN_MIN = 0;	// lf-fixed: the schema limit, mirrors the attribute params
	static const float COOLDOWN_MAX = 86400;	// lf-fixed: the schema limit, one day
	static const int XP_CAP_MIN = 0;	// lf-fixed: the schema limit, 0 is no cap
	static const int XP_CAP_MAX = 100000000;	// lf-fixed: the schema limit, mirrors the attribute params
	static const int XP_AWARD_MIN = 0;	// lf-fixed: the schema limit, mirrors the attribute params
	static const int XP_AWARD_MAX = 100000;	// lf-fixed: the schema limit, mirrors the attribute params
	static const int THRESHOLD_MIN = 1;	// lf-fixed: a threshold of 0 would be level 2 at no XP
	static const int THRESHOLD_MAX = 100000000;	// lf-fixed: the XP cap limit
	static const int GATE_LEVEL_MIN = 1;	// lf-fixed: the schema limit, levels start at 1
	static const int GATE_LEVEL_MAX = 100;	// lf-fixed: the schema limit, mirrors the attribute params
	static const int LEGACY_FIRST_MIN = 1;	// lf-fixed: the schema limit, mirrors the attribute params
	static const int LEGACY_FIRST_MAX = 1000000;	// lf-fixed: the schema limit, mirrors the attribute params
	static const float LEGACY_GROWTH_MIN = 1;	// lf-fixed: the schema limit, mirrors the attribute params
	static const float LEGACY_GROWTH_MAX = 10;	// lf-fixed: the schema limit, mirrors the attribute params

	//! The pre-#749 career curve when the `.conf` does not resolve (the values it shipped with).
	static const int DEFAULT_JOB_XP_FIRST_LEVEL = 500;
	static const float DEFAULT_JOB_XP_LEVEL_GROWTH = 2;

	[Attribute("0.5", UIWidgets.Auto, "Police duty pay in cash per second on duty, paid at the station desk on going off duty. 0 disables duty pay", params: "0 100")]
	float m_fPoliceDutyPayPerSecond;

	[Attribute("0.4", UIWidgets.Auto, "Medic duty pay in cash per second on duty, paid at the hospital desk on going off duty. 0 disables duty pay", params: "0 100")]
	float m_fMedicDutyPayPerSecond = 0.4;

	[Attribute("500", UIWidgets.Auto, "Unemployment benefit: a bank balance strictly below this may collect", params: "0 1000000")]
	int m_iBenefitBalanceThreshold;

	[Attribute("100", UIWidgets.Auto, "Unemployment benefit: cash handed over per collection. 0 disables the benefit", params: "0 1000000")]
	int m_iBenefitAmount;

	[Attribute("1200", UIWidgets.Auto, "Unemployment benefit: seconds between collections for one account", params: "0 86400")]
	float m_fBenefitCooldownSeconds;

	[Attribute("500", UIWidgets.Auto, "Pre-#749 delivery career: XP from level 1 to 2. Read only to migrate a version 7-27 save's career onto the DELIVERY job; set it to what this server used", params: "1 1000000")]
	int m_iJobXpFirstLevel;

	[Attribute("2", UIWidgets.Auto, "Pre-#749 delivery career: each later level needed this many times the XP of the one before. Read only to migrate a version 7-27 save", params: "1 10")]
	float m_fJobXpLevelGrowth;

	[Attribute("", UIWidgets.Object, "Job level ladders: the XP thresholds of each job (#749)")]
	ref array<ref LF_JobTrackDef> m_aTracks;

	[Attribute("", UIWidgets.Object, "XP each in-world action awards, the job it feeds and its cooldown (#749)")]
	ref array<ref LF_JobXpAwardDef> m_aXpAwards;

	[Attribute("", UIWidgets.Object, "Licence gates that differ from the shipped catalog: the job and level a licence needs (#749)")]
	ref array<ref LF_JobLicenceGateDef> m_aLicenceGates;

	protected static ref LF_JobsConfig s_Config;
	protected static bool s_bLoadAttempted;

	//------------------------------------------------------------------------------------------------
	//! The loaded config with the owner's "jobs" section merged over it, or null (logged once) when
	//! the file does not resolve.
	static LF_JobsConfig Get()
	{
		if (s_bLoadAttempted)
			return s_Config;

		s_bLoadAttempted = true;
		Resource container = BaseContainerTools.LoadContainer(CONFIG_PATH);
		if (!container || !container.GetResource())
		{
			LF_Debug.Error("Jobs", string.Format("jobs config does not resolve: %1", CONFIG_PATH));
			return null;
		}

		s_Config = LF_JobsConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (!s_Config)
			LF_Debug.Error("Jobs", string.Format("jobs config has an unexpected root class: %1", CONFIG_PATH));
		else
			s_Config.ApplyLayers();

		return s_Config;
	}

	//------------------------------------------------------------------------------------------------
	//! Drops the cached instance; the next Get loads the `.conf` and the owner section again
	//! (/configreload, LF_ConfigStream.ApplyToFeatures, tests).
	static void Reload()
	{
		s_Config = null;
		s_bLoadAttempted = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Tests: makes `config` the one Get returns until Reload.
	static void UseForTest(LF_JobsConfig config)
	{
		s_Config = config;
		s_bLoadAttempted = true;
	}

	//------------------------------------------------------------------------------------------------
	//! Merges the owner's "jobs" section over this instance, which holds the core `.conf`.
	void ApplyLayers()
	{
		LF_ConfigReader reader = LF_ConfigService.OpenOwnerSection(SECTION);
		if (!reader)
			return;

		ApplyOwner(reader);
		LF_Debug.Info(LF_ConfigService.FEATURE, string.Format("jobs: owner config read %1 fields, rejected %2", reader.GetAppliedCount(), reader.GetRejectedCount()));
	}

	//------------------------------------------------------------------------------------------------
	//! Applies the owner's "jobs" section field by field: a key the owner leaves out keeps the shipped
	//! value. `tracks` is keyed by job id, `xpAwards` by action id and `licenceGates` by licence name;
	//! a key no shipped record has adds one, an existing key changes only the fields given.
	//! \param[in] reader A reader on the section.
	void ApplyOwner(notnull LF_ConfigReader reader)
	{
		array<string> ownerKeys = {"policeDutyPayPerSecond", "medicDutyPayPerSecond", "benefitBalanceThreshold", "benefitAmount", "benefitCooldownSeconds", "jobXpFirstLevel", "jobXpLevelGrowth", "tracks", "xpAwards", "licenceGates"};
		reader.WarnUnknownKeys(ownerKeys);
		reader.ReadFloat("policeDutyPayPerSecond", m_fPoliceDutyPayPerSecond, PAY_RATE_MIN, PAY_RATE_MAX);
		reader.ReadFloat("medicDutyPayPerSecond", m_fMedicDutyPayPerSecond, PAY_RATE_MIN, PAY_RATE_MAX);
		reader.ReadInt("benefitBalanceThreshold", m_iBenefitBalanceThreshold, CASH_MIN, CASH_MAX);
		reader.ReadInt("benefitAmount", m_iBenefitAmount, CASH_MIN, CASH_MAX);
		reader.ReadFloat("benefitCooldownSeconds", m_fBenefitCooldownSeconds, COOLDOWN_MIN, COOLDOWN_MAX);
		reader.ReadInt("jobXpFirstLevel", m_iJobXpFirstLevel, LEGACY_FIRST_MIN, LEGACY_FIRST_MAX);
		reader.ReadFloat("jobXpLevelGrowth", m_fJobXpLevelGrowth, LEGACY_GROWTH_MIN, LEGACY_GROWTH_MAX);

		ApplyOwnerTracks(reader);
		ApplyOwnerAwards(reader);
		ApplyOwnerGates(reader);
	}

	//------------------------------------------------------------------------------------------------
	protected void ApplyOwnerTracks(notnull LF_ConfigReader reader)
	{
		if (!m_aTracks)
			m_aTracks = new array<ref LF_JobTrackDef>();

		int count = reader.BeginRecords("tracks");
		for (int i = 0; i < count; i++)
		{
			string key = reader.GetRecordKey(i);
			if (!reader.BeginRecord(key))
				continue;

			LF_JobTrackDef track = FindTrack(key);
			if (!track && !reader.Has("levelThresholds"))
			{
				reader.RejectRecord("a job the shipped tracks do not list needs levelThresholds");
				reader.EndRecord();
				continue;
			}

			array<string> trackKeys = {"levelThresholds", "dailyXpCap"};
			reader.WarnUnknownKeys(trackKeys);
			array<int> thresholds = {};
			if (reader.ReadInts("levelThresholds", thresholds, THRESHOLD_MIN, THRESHOLD_MAX))
			{
				if (!track)
				{
					track = LF_JobTrackDef.Make(key, thresholds);
					m_aTracks.Insert(track);
				}
				else
				{
					track.m_aLevelThresholds = {};
					track.m_aLevelThresholds.Copy(thresholds);
				}
			}

			if (track)
				reader.ReadInt("dailyXpCap", track.m_iDailyXpCap, XP_CAP_MIN, XP_CAP_MAX);

			reader.EndRecord();
		}

		if (count >= 0)
			reader.EndRecords();
	}

	//------------------------------------------------------------------------------------------------
	protected void ApplyOwnerAwards(notnull LF_ConfigReader reader)
	{
		if (!m_aXpAwards)
			m_aXpAwards = new array<ref LF_JobXpAwardDef>();

		int count = reader.BeginRecords("xpAwards");
		for (int i = 0; i < count; i++)
		{
			string key = reader.GetRecordKey(i);
			if (!reader.BeginRecord(key))
				continue;

			LF_JobXpAwardDef award = FindAward(key);
			if (!award && !reader.Has("job"))
			{
				reader.RejectRecord("an action the shipped awards do not list needs a job");
				reader.EndRecord();
				continue;
			}

			if (!award)
			{
				award = LF_JobXpAwardDef.Make(key, "", 0);
				m_aXpAwards.Insert(award);
			}

			array<string> awardKeys = {"job", "xp", "cooldownSeconds"};
			reader.WarnUnknownKeys(awardKeys);
			reader.ReadString("job", award.m_sJob);
			reader.ReadInt("xp", award.m_iXp, XP_AWARD_MIN, XP_AWARD_MAX);
			reader.ReadFloat("cooldownSeconds", award.m_fCooldownSeconds, COOLDOWN_MIN, COOLDOWN_MAX);
			reader.EndRecord();
		}

		if (count >= 0)
			reader.EndRecords();
	}

	//------------------------------------------------------------------------------------------------
	protected void ApplyOwnerGates(notnull LF_ConfigReader reader)
	{
		if (!m_aLicenceGates)
			m_aLicenceGates = new array<ref LF_JobLicenceGateDef>();

		int count = reader.BeginRecords("licenceGates");
		for (int i = 0; i < count; i++)
		{
			string key = reader.GetRecordKey(i);
			if (!reader.BeginRecord(key))
				continue;

			LF_ELicenseType licence = typename.StringToEnum(LF_ELicenseType, key);
			if (typename.EnumToString(LF_ELicenseType, licence) != key)
			{
				reader.RejectRecord("the key is not a licence name (LF_ELicenseType)");
				reader.EndRecord();
				continue;
			}

			LF_JobLicenceGateDef gate = FindLicenceGate(key);
			if (!gate && (!reader.Has("job") || !reader.Has("minLevel")))
			{
				reader.RejectRecord("a licence gate the shipped config does not list needs a job and a minLevel");
				reader.EndRecord();
				continue;
			}

			string job;
			int minLevel = 1;
			if (gate)
			{
				job = gate.m_sJob;
				minLevel = gate.m_iMinLevel;
			}

			array<string> gateKeys = {"job", "minLevel"};
			reader.WarnUnknownKeys(gateKeys);
			bool jobRead = reader.ReadString("job", job);
			bool levelRead = reader.ReadInt("minLevel", minLevel, GATE_LEVEL_MIN, GATE_LEVEL_MAX);
			if (!gate)
			{
				if (!jobRead || !levelRead)
				{
					reader.RejectRecord("a new licence gate needs a valid job and minLevel");
					reader.EndRecord();
					continue;
				}

				gate = LF_JobLicenceGateDef.Make(key, job, minLevel);
				m_aLicenceGates.Insert(gate);
			}
			else
			{
				gate.m_sJob = job;
				gate.m_iMinLevel = minLevel;
			}

			reader.EndRecord();
		}

		if (count >= 0)
			reader.EndRecords();
	}

	//------------------------------------------------------------------------------------------------
	//! \return The ladder of a job, or null when the config has none (the job is unknown).
	LF_JobTrackDef FindTrack(string job)
	{
		if (!m_aTracks || job.IsEmpty())
			return null;

		foreach (LF_JobTrackDef track : m_aTracks)
		{
			if (track && track.m_sJob == job)
				return track;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The award of an action, or null when the config has none.
	LF_JobXpAwardDef FindAward(string action)
	{
		if (!m_aXpAwards || action.IsEmpty())
			return null;

		foreach (LF_JobXpAwardDef award : m_aXpAwards)
		{
			if (award && award.m_sAction == action)
				return award;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The configured gate of a licence (by LF_ELicenseType name), or null for the shipped one.
	LF_JobLicenceGateDef FindLicenceGate(string licence)
	{
		if (!m_aLicenceGates || licence.IsEmpty())
			return null;

		foreach (LF_JobLicenceGateDef gate : m_aLicenceGates)
		{
			if (gate && gate.m_sLicence == licence)
				return gate;
		}

		return null;
	}
}
