//------------------------------------------------------------------------------------------------
//! Taxi fare numbers (#2690): Configs/Jobs/LF_TaxiFares.conf with the owner's `taxiFares` section
//! layered over it. A fare is priced by the straight-line distance between where the run started (the
//! pickup, recorded by the server) and the drop-off: a short hop pays the floor, a longer run pays the
//! floor plus the rate per metre, and the fare never goes above the cap.
[BaseContainerProps(configRoot: true)]
class LF_TaxiFareConfig
{
	static const ResourceName CONFIG_PATH = "{86CCE89242BF28CB}Configs/Jobs/LF_TaxiFares.conf";

	//! The owner config.json section this class reads (LF_ConfigService).
	static const string SECTION = "taxiFares";

	static const int DEFAULT_FLOOR = 80;	// lf-fixed: fallback only, mirrors LF_TaxiFares.conf
	static const float DEFAULT_RATE_PER_METRE = 0.04;	// lf-fixed: fallback only, mirrors LF_TaxiFares.conf
	static const int DEFAULT_CAP = 300;	// lf-fixed: fallback only, mirrors LF_TaxiFares.conf
	static const float DEFAULT_SHORT_HOP_METRES = 100;	// lf-fixed: fallback only, mirrors LF_TaxiFares.conf

	static const int CASH_MIN = 1;	// lf-fixed: the schema limit, mirrors the attribute params
	static const int CASH_MAX = 1000000;	// lf-fixed: the schema limit, mirrors the attribute params
	static const float RATE_MIN = 0.001;	// lf-fixed: the schema limit, mirrors the attribute params
	static const float RATE_MAX = 1000;	// lf-fixed: the schema limit, mirrors the attribute params
	static const float HOP_MIN = 0;	// lf-fixed: the schema limit, 0 prices every metre
	static const float HOP_MAX = 10000;	// lf-fixed: the schema limit, mirrors the attribute params

	[Attribute("80", UIWidgets.Auto, "Taxi fare: the least a fare pays, and what a short hop pays", params: "1 1000000")]
	int m_iFloor;

	[Attribute("0.04", UIWidgets.Auto, "Taxi fare: cash added per metre of straight-line distance beyond a short hop", params: "0.001 1000")]
	float m_fRatePerMetre;

	[Attribute("300", UIWidgets.Auto, "Taxi fare: the most a fare pays, however far it went. Must not be below the floor", params: "1 1000000")]
	int m_iCap;

	[Attribute("100", UIWidgets.Auto, "Taxi fare: a drop-off this many metres or fewer from the pickup pays the floor only. 0 prices every metre", params: "0 10000")]
	float m_fShortHopMetres;

	protected static ref LF_TaxiFareConfig s_Config;
	protected static bool s_bLoadAttempted;

	//------------------------------------------------------------------------------------------------
	//! The shipped values; the members carry no initializers (#1077).
	static LF_TaxiFareConfig CreateDefault()
	{
		LF_TaxiFareConfig config = new LF_TaxiFareConfig();
		config.m_iFloor = DEFAULT_FLOOR;
		config.m_fRatePerMetre = DEFAULT_RATE_PER_METRE;
		config.m_iCap = DEFAULT_CAP;
		config.m_fShortHopMetres = DEFAULT_SHORT_HOP_METRES;
		return config;
	}

	//------------------------------------------------------------------------------------------------
	//! The loaded and layered config, or null (logged once) when the file does not resolve.
	static LF_TaxiFareConfig Get()
	{
		if (s_bLoadAttempted)
			return s_Config;

		s_bLoadAttempted = true;
		Resource container = BaseContainerTools.LoadContainer(CONFIG_PATH);
		if (!container || !container.GetResource())
		{
			LF_Debug.Error("Jobs", string.Format("taxi fare config does not resolve, using defaults: %1", CONFIG_PATH));
			return null;
		}

		s_Config = LF_TaxiFareConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (!s_Config)
			LF_Debug.Error("Jobs", string.Format("taxi fare config has an unexpected root class, using defaults: %1", CONFIG_PATH));
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
	//! Tests: makes `config` the one Get returns until Reload.
	static void UseForTest(LF_TaxiFareConfig config)
	{
		s_Config = config;
		s_bLoadAttempted = true;
	}

	//------------------------------------------------------------------------------------------------
	//! Merges the layers over this instance, which holds the core `.conf`: each pack overlay of this
	//! class in load order (the fields it sets directly, #3147), then the owner's "taxiFares" section.
	void ApplyLayers()
	{
		array<ref Resource> resources = {};
		array<ResourceName> names = {};
		LF_ConfigOverlay.LoadPackOverlays(CONFIG_PATH, LF_TaxiFareConfig, SECTION, resources, names);
		foreach (int i, Resource resource : resources)
		{
			int changed = ApplyContainer(resource.GetResource().ToBaseContainer());
			LF_Debug.Info(LF_ConfigService.FEATURE, string.Format("taxiFares: pack overlay %1 changed %2 fields", names[i], changed));
		}

		LF_ConfigReader reader = LF_ConfigService.OpenOwnerSection(SECTION);
		if (!reader)
			return;

		ApplyOwner(reader);
		LF_Debug.Info(LF_ConfigService.FEATURE, string.Format("taxiFares: owner config read %1 fields, rejected %2", reader.GetAppliedCount(), reader.GetRejectedCount()));
	}

	//------------------------------------------------------------------------------------------------
	//! Applies one pack `.conf` of this class: only the fields it sets directly. \return Fields changed.
	int ApplyContainer(BaseContainer container)
	{
		if (!container)
			return 0;

		int changed;
		if (container.IsVariableSetDirectly("m_iFloor") && container.Get("m_iFloor", m_iFloor))
			changed++;
		if (container.IsVariableSetDirectly("m_fRatePerMetre") && container.Get("m_fRatePerMetre", m_fRatePerMetre))
			changed++;
		if (container.IsVariableSetDirectly("m_iCap") && container.Get("m_iCap", m_iCap))
			changed++;
		if (container.IsVariableSetDirectly("m_fShortHopMetres") && container.Get("m_fShortHopMetres", m_fShortHopMetres))
			changed++;
		return changed;
	}

	//------------------------------------------------------------------------------------------------
	//! Applies the owner's "taxiFares" section field by field; a key left out keeps the shipped value.
	void ApplyOwner(notnull LF_ConfigReader reader)
	{
		array<string> ownerKeys = {"floor", "ratePerMetre", "cap", "shortHopMetres"};
		reader.WarnUnknownKeys(ownerKeys);
		reader.ReadInt("floor", m_iFloor, CASH_MIN, CASH_MAX);
		reader.ReadFloat("ratePerMetre", m_fRatePerMetre, RATE_MIN, RATE_MAX);
		reader.ReadInt("cap", m_iCap, CASH_MIN, CASH_MAX);
		reader.ReadFloat("shortHopMetres", m_fShortHopMetres, HOP_MIN, HOP_MAX);
	}

	//------------------------------------------------------------------------------------------------
	//! \return Whether the numbers can price a fare: a positive floor and rate, and a cap that is not
	//! below the floor. A short hop below zero is invalid too.
	bool IsUsable()
	{
		if (m_iFloor <= 0 || m_fRatePerMetre <= 0 || m_iCap <= 0)
			return false;

		if (m_iCap < m_iFloor || m_fShortHopMetres < 0)
			return false;

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Prices a taxi fare from the distance driven (#2690). Pure: the caller (LF_CivilianJob) hands in the
//! server-recorded pickup and the drop-off position.
class LF_TaxiFare
{
	//------------------------------------------------------------------------------------------------
	//! \return The fare in cash for a drop-off `dropoff` metres-by-straight-line from `pickup`, under
	//! the loaded config (the shipped numbers when it does not resolve).
	static int Price(vector pickup, vector dropoff)
	{
		return PriceFor(LF_TaxiFareConfig.Get(), vector.Distance(pickup, dropoff));
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] config The numbers to price with; null or an unusable one (zero or negative floor,
	//! rate or cap, or a cap below the floor) logs an LF_Debug.Error and prices with the shipped numbers.
	//! \param[in] metres Straight-line distance from the pickup to the drop-off.
	//! \return The floor for a short hop, else the floor plus the rate per metre, never above the cap.
	static int PriceFor(LF_TaxiFareConfig config, float metres)
	{
		LF_TaxiFareConfig use = config;
		if (!use)
		{
			use = LF_TaxiFareConfig.CreateDefault();
		}
		else if (!use.IsUsable())
		{
			LF_Debug.Error("Jobs", string.Format("taxi fare config is unusable (floor %1, rate %2, cap %3): pricing with the shipped numbers", use.m_iFloor, use.m_fRatePerMetre, use.m_iCap));
			use = LF_TaxiFareConfig.CreateDefault();
		}

		if (metres <= use.m_fShortHopMetres)
			return use.m_iFloor;

		int fare = use.m_iFloor + Math.Round(use.m_fRatePerMetre * metres);
		if (fare > use.m_iCap)
			return use.m_iCap;

		return fare;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_TaxiFareConfig's statics (LF_StaticReset finds it by reflection).
class LF_TaxiFareConfig_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_TaxiFareConfig";
	}

	override void Apply()
	{
		LF_TaxiFareConfig.Reload();
	}
}
