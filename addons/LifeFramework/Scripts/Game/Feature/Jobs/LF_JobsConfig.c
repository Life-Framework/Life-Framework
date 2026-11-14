//------------------------------------------------------------------------------------------------
//! One job's level ladder (#749, D12): the total XP each level needs. A character's level in a job
//! is 1 plus the thresholds its XP has reached, so a rebalanced table reaches old saves (the save
//! holds XP, never a level).
[BaseContainerProps()]
class LF_JobTrackDef : LF_ConfigRecord
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
class LF_JobXpAwardDef : LF_ConfigRecord
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
class LF_JobLicenceGateDef : LF_ConfigRecord
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
	//! Merges the layers over this instance, which holds the core `.conf`: each pack overlay of this
	//! class in load order (its three lists), then the owner's "jobs" section (LF_ConfigOverlay, #1106).
	void ApplyLayers()
	{
		EnsureLists();
		array<ref Resource> resources = {};
		array<ResourceName> names = {};
		LF_ConfigOverlay.LoadPackOverlays(CONFIG_PATH, LF_JobsConfig, SECTION, resources, names);
		LF_ConfigClaims claims = new LF_ConfigClaims();
		foreach (int i, Resource resource : resources)
		{
			BaseContainer root = resource.GetResource().ToBaseContainer();
			int changed = LF_ConfigOverlay.ApplyContainerList(new LF_JobTrackList(this), root, "m_aTracks", names[i], claims);
			changed += LF_ConfigOverlay.ApplyContainerList(new LF_JobXpAwardList(this), root, "m_aXpAwards", names[i], claims);
			changed += LF_ConfigOverlay.ApplyContainerList(new LF_JobLicenceGateList(this), root, "m_aLicenceGates", names[i], claims);
			LF_Debug.Info(LF_ConfigService.FEATURE, string.Format("jobs: pack overlay %1 changed %2 entries", names[i], changed));
		}

		LF_ConfigReader reader = LF_ConfigService.OpenOwnerSection(SECTION);
		if (!reader)
			return;

		ApplyOwner(reader, LF_ConfigService.GetOwnerSource());
		LF_Debug.Info(LF_ConfigService.FEATURE, string.Format("jobs: owner config read %1 fields, rejected %2", reader.GetAppliedCount(), reader.GetRejectedCount()));
	}

	//------------------------------------------------------------------------------------------------
	protected void EnsureLists()
	{
		if (!m_aTracks)
			m_aTracks = new array<ref LF_JobTrackDef>();
		if (!m_aXpAwards)
			m_aXpAwards = new array<ref LF_JobXpAwardDef>();
		if (!m_aLicenceGates)
			m_aLicenceGates = new array<ref LF_JobLicenceGateDef>();
	}

	//------------------------------------------------------------------------------------------------
	//! Applies the owner's "jobs" section: a key the owner leaves out keeps the shipped value.
	//! `tracks` is keyed by job id, `xpAwards` by action id and `licenceGates` by licence name; each
	//! record may carry an "op" (add, replace, patch, remove). With no op an existing key changes only
	//! the fields given and a new key adds a record (docs/config/override-model.md).
	//! \param[in] reader A reader on the section.
	//! \param[in] source The file, for log lines.
	void ApplyOwner(notnull LF_ConfigReader reader, string source = "config.json")
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

		EnsureLists();
		LF_ConfigOverlay.ApplyRecords(reader, "tracks", new LF_JobTrackList(this), source);
		LF_ConfigOverlay.ApplyRecords(reader, "xpAwards", new LF_JobXpAwardList(this), source);
		LF_ConfigOverlay.ApplyRecords(reader, "licenceGates", new LF_JobLicenceGateList(this), source);
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

//------------------------------------------------------------------------------------------------
//! The `tracks` list for the shared merge (LF_ConfigOverlay), keyed by job id. A new or replaced
//! track needs level thresholds.
class LF_JobTrackList : LF_ConfigList
{
	protected LF_JobsConfig m_Config;

	void LF_JobTrackList(LF_JobsConfig config)
	{
		m_Config = config;
	}

	override string GetName()
	{
		return "jobs.tracks";
	}

	override int Count()
	{
		return m_Config.m_aTracks.Count();
	}

	override LF_ConfigRecord Get(int index)
	{
		return m_Config.m_aTracks[index];
	}

	override string IdOf(notnull LF_ConfigRecord record)
	{
		return LF_JobTrackDef.Cast(record).m_sJob;
	}

	override string IdOfContainer(notnull BaseContainer container)
	{
		string id;
		container.Get("m_sJob", id);
		return id;
	}

	override string IdHint()
	{
		return "a job id";
	}

	override LF_ConfigRecord NewRecord(string id)
	{
		array<int> none = {};
		return LF_JobTrackDef.Make(id, none);
	}

	override void Set(int index, notnull LF_ConfigRecord record)
	{
		LF_JobTrackDef track = LF_JobTrackDef.Cast(record);
		if (index < m_Config.m_aTracks.Count())
			m_Config.m_aTracks.Set(index, track);
		else
			m_Config.m_aTracks.Insert(track);
	}

	override void RemoveAt(int index)
	{
		m_Config.m_aTracks.RemoveOrdered(index);
	}

	override int PatchFromContainer(notnull LF_ConfigRecord record, notnull BaseContainer container)
	{
		LF_JobTrackDef track = LF_JobTrackDef.Cast(record);
		int changed;
		if (container.IsVariableSetDirectly("m_aLevelThresholds"))
		{
			array<int> thresholds = {};
			if (container.Get("m_aLevelThresholds", thresholds))
			{
				track.m_aLevelThresholds = {};
				track.m_aLevelThresholds.Copy(thresholds);
				changed++;
			}
		}
		if (container.IsVariableSetDirectly("m_iDailyXpCap") && container.Get("m_iDailyXpCap", track.m_iDailyXpCap))
			changed++;
		return changed;
	}

	override void PatchFromReader(notnull LF_ConfigRecord record, notnull LF_ConfigReader reader)
	{
		LF_JobTrackDef track = LF_JobTrackDef.Cast(record);
		array<string> recordKeys = {"op", "levelThresholds", "dailyXpCap"};
		reader.WarnUnknownKeys(recordKeys);
		array<int> thresholds = {};
		if (reader.ReadInts("levelThresholds", thresholds, LF_JobsConfig.THRESHOLD_MIN, LF_JobsConfig.THRESHOLD_MAX))
		{
			track.m_aLevelThresholds = {};
			track.m_aLevelThresholds.Copy(thresholds);
		}
		reader.ReadInt("dailyXpCap", track.m_iDailyXpCap, LF_JobsConfig.XP_CAP_MIN, LF_JobsConfig.XP_CAP_MAX);
	}

	override string Incomplete(notnull LF_ConfigRecord record)
	{
		LF_JobTrackDef track = LF_JobTrackDef.Cast(record);
		if (!track.m_aLevelThresholds || track.m_aLevelThresholds.IsEmpty())
			return "a job the shipped tracks do not list needs levelThresholds";

		return string.Empty;
	}
}

//------------------------------------------------------------------------------------------------
//! The `xpAwards` list for the shared merge, keyed by action id. A new or replaced award needs a job.
class LF_JobXpAwardList : LF_ConfigList
{
	protected LF_JobsConfig m_Config;

	void LF_JobXpAwardList(LF_JobsConfig config)
	{
		m_Config = config;
	}

	override string GetName()
	{
		return "jobs.xpAwards";
	}

	override int Count()
	{
		return m_Config.m_aXpAwards.Count();
	}

	override LF_ConfigRecord Get(int index)
	{
		return m_Config.m_aXpAwards[index];
	}

	override string IdOf(notnull LF_ConfigRecord record)
	{
		return LF_JobXpAwardDef.Cast(record).m_sAction;
	}

	override string IdOfContainer(notnull BaseContainer container)
	{
		string id;
		container.Get("m_sAction", id);
		return id;
	}

	override string IdHint()
	{
		return "an action id";
	}

	override LF_ConfigRecord NewRecord(string id)
	{
		return LF_JobXpAwardDef.Make(id, string.Empty, 0);
	}

	override void Set(int index, notnull LF_ConfigRecord record)
	{
		LF_JobXpAwardDef award = LF_JobXpAwardDef.Cast(record);
		if (index < m_Config.m_aXpAwards.Count())
			m_Config.m_aXpAwards.Set(index, award);
		else
			m_Config.m_aXpAwards.Insert(award);
	}

	override void RemoveAt(int index)
	{
		m_Config.m_aXpAwards.RemoveOrdered(index);
	}

	override int PatchFromContainer(notnull LF_ConfigRecord record, notnull BaseContainer container)
	{
		LF_JobXpAwardDef award = LF_JobXpAwardDef.Cast(record);
		int changed;
		if (container.IsVariableSetDirectly("m_sJob") && container.Get("m_sJob", award.m_sJob))
			changed++;
		if (container.IsVariableSetDirectly("m_iXp") && container.Get("m_iXp", award.m_iXp))
			changed++;
		if (container.IsVariableSetDirectly("m_fCooldownSeconds") && container.Get("m_fCooldownSeconds", award.m_fCooldownSeconds))
			changed++;
		return changed;
	}

	override void PatchFromReader(notnull LF_ConfigRecord record, notnull LF_ConfigReader reader)
	{
		LF_JobXpAwardDef award = LF_JobXpAwardDef.Cast(record);
		array<string> recordKeys = {"op", "job", "xp", "cooldownSeconds"};
		reader.WarnUnknownKeys(recordKeys);
		reader.ReadString("job", award.m_sJob);
		reader.ReadInt("xp", award.m_iXp, LF_JobsConfig.XP_AWARD_MIN, LF_JobsConfig.XP_AWARD_MAX);
		reader.ReadFloat("cooldownSeconds", award.m_fCooldownSeconds, LF_JobsConfig.COOLDOWN_MIN, LF_JobsConfig.COOLDOWN_MAX);
	}

	override string Incomplete(notnull LF_ConfigRecord record)
	{
		if (LF_JobXpAwardDef.Cast(record).m_sJob.IsEmpty())
			return "an action the shipped awards do not list needs a job";

		return string.Empty;
	}
}

//------------------------------------------------------------------------------------------------
//! The `licenceGates` list for the shared merge, keyed by licence name (LF_ELicenseType). A new or
//! replaced gate needs a job and a level; NewRecord leaves the level at 0 so a missing one is seen.
class LF_JobLicenceGateList : LF_ConfigList
{
	protected LF_JobsConfig m_Config;

	void LF_JobLicenceGateList(LF_JobsConfig config)
	{
		m_Config = config;
	}

	override string GetName()
	{
		return "jobs.licenceGates";
	}

	override int Count()
	{
		return m_Config.m_aLicenceGates.Count();
	}

	override LF_ConfigRecord Get(int index)
	{
		return m_Config.m_aLicenceGates[index];
	}

	override string IdOf(notnull LF_ConfigRecord record)
	{
		return LF_JobLicenceGateDef.Cast(record).m_sLicence;
	}

	override string IdOfContainer(notnull BaseContainer container)
	{
		string id;
		container.Get("m_sLicence", id);
		return id;
	}

	//! An owner key must be a licence name.
	override string CanonicalId(string key)
	{
		LF_ELicenseType licence = typename.StringToEnum(LF_ELicenseType, key);
		if (typename.EnumToString(LF_ELicenseType, licence) != key)
			return string.Empty;

		return key;
	}

	override string IdHint()
	{
		return "a licence name (LF_ELicenseType)";
	}

	override LF_ConfigRecord NewRecord(string id)
	{
		return LF_JobLicenceGateDef.Make(id, string.Empty, 0);
	}

	override void Set(int index, notnull LF_ConfigRecord record)
	{
		LF_JobLicenceGateDef gate = LF_JobLicenceGateDef.Cast(record);
		if (index < m_Config.m_aLicenceGates.Count())
			m_Config.m_aLicenceGates.Set(index, gate);
		else
			m_Config.m_aLicenceGates.Insert(gate);
	}

	override void RemoveAt(int index)
	{
		m_Config.m_aLicenceGates.RemoveOrdered(index);
	}

	override int PatchFromContainer(notnull LF_ConfigRecord record, notnull BaseContainer container)
	{
		LF_JobLicenceGateDef gate = LF_JobLicenceGateDef.Cast(record);
		int changed;
		if (container.IsVariableSetDirectly("m_sJob") && container.Get("m_sJob", gate.m_sJob))
			changed++;
		if (container.IsVariableSetDirectly("m_iMinLevel") && container.Get("m_iMinLevel", gate.m_iMinLevel))
			changed++;
		return changed;
	}

	override void PatchFromReader(notnull LF_ConfigRecord record, notnull LF_ConfigReader reader)
	{
		LF_JobLicenceGateDef gate = LF_JobLicenceGateDef.Cast(record);
		array<string> recordKeys = {"op", "job", "minLevel"};
		reader.WarnUnknownKeys(recordKeys);
		reader.ReadString("job", gate.m_sJob);
		reader.ReadInt("minLevel", gate.m_iMinLevel, LF_JobsConfig.GATE_LEVEL_MIN, LF_JobsConfig.GATE_LEVEL_MAX);
	}

	override string Incomplete(notnull LF_ConfigRecord record)
	{
		LF_JobLicenceGateDef gate = LF_JobLicenceGateDef.Cast(record);
		if (gate.m_sJob.IsEmpty() || gate.m_iMinLevel < LF_JobsConfig.GATE_LEVEL_MIN)
			return "a licence gate the shipped config does not list needs a job and a minLevel";

		return string.Empty;
	}
}
