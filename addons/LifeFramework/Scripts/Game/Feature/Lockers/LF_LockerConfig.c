//------------------------------------------------------------------------------------------------
//! The lockers owner-config section (#2658): Configs/Lockers/LF_Lockers.conf with the owner's
//! `lockers` section layered over it. Read on the server only (rent and lost-and-found are server rules).
//!
//! Owner JSON:
//!   "lockers": { "rentPrice": 100, "rentPeriodDays": 7, "gracePeriodDays": 2, "reclaimFee": 50, "weightCapacity": 60 }
//! `rentPrice` is the cash for one rent period. `rentPeriodDays` is how long one payment lasts; 0 means a
//! locker never lapses. `gracePeriodDays` is the wait after the paid-until moment before the contents move to
//! the site's lost-and-found. `reclaimFee` is the cash for taking them back. `weightCapacity` is the kg one
//! locker holds.
[BaseContainerProps(configRoot: true)]
class LF_LockerConfig
{
	static const ResourceName CONFIG_PATH = "{6D1F3A52B9C07E84}Configs/Lockers/LF_Lockers.conf";

	//! The owner config.json section this class reads (LF_ConfigService).
	static const string SECTION = "lockers";

	static const int DEFAULT_RENT_PRICE = 100;
	static const int DEFAULT_RENT_PERIOD_DAYS = 7;
	static const int DEFAULT_GRACE_PERIOD_DAYS = 2;
	static const int DEFAULT_RECLAIM_FEE = 50;
	static const float DEFAULT_WEIGHT_CAPACITY = 60;

	[Attribute("100", UIWidgets.Auto, "Cash for one rent period at a locker site. 0 rents free", params: "0 1000000")]
	int m_iRentPrice;

	[Attribute("7", UIWidgets.Auto, "Days one rent payment lasts. 0 means a locker never lapses", params: "0 3650")]
	int m_iRentPeriodDays;

	[Attribute("2", UIWidgets.Auto, "Days after the paid-until moment before the contents move to the site's lost-and-found", params: "0 3650")]
	int m_iGracePeriodDays;

	[Attribute("50", UIWidgets.Auto, "Cash to take the contents back from lost-and-found. 0 reclaims free", params: "0 1000000")]
	int m_iReclaimFee;

	[Attribute("60", UIWidgets.Auto, "Kilograms one locker holds", params: "1 10000")]
	float m_fWeightCapacity;

	protected static ref LF_LockerConfig s_Config;
	protected static bool s_bLoadAttempted;

	//------------------------------------------------------------------------------------------------
	//! The loaded and layered config, or null (logged once) when the file does not resolve.
	static LF_LockerConfig Get()
	{
		if (s_bLoadAttempted)
			return s_Config;

		s_bLoadAttempted = true;
		Resource container = BaseContainerTools.LoadContainer(CONFIG_PATH);
		if (!container || !container.GetResource())
		{
			LF_Debug.Error("Lockers", string.Format("lockers config does not resolve, using defaults: %1", CONFIG_PATH));
			return null;
		}

		s_Config = LF_LockerConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (!s_Config)
			LF_Debug.Error("Lockers", string.Format("lockers config has an unexpected root class, using defaults: %1", CONFIG_PATH));
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
	//! Merges the owner's "lockers" section over this instance, which holds the core `.conf`.
	void ApplyLayers()
	{
		LF_ConfigReader reader = LF_ConfigService.OpenOwnerSection(SECTION);
		if (!reader)
			return;

		ApplyOwner(reader);
		LF_Debug.Info(LF_ConfigService.FEATURE, string.Format("lockers: owner config read %1 fields, rejected %2", reader.GetAppliedCount(), reader.GetRejectedCount()));
	}

	//------------------------------------------------------------------------------------------------
	//! Applies the owner's "lockers" section field by field; a key left out, or refused for being
	//! out of range or not a number, keeps the shipped value.
	void ApplyOwner(notnull LF_ConfigReader reader)
	{
		array<string> ownerKeys = {"rentPrice", "rentPeriodDays", "gracePeriodDays", "reclaimFee", "weightCapacity"};
		reader.WarnUnknownKeys(ownerKeys);

		reader.ReadInt("rentPrice", m_iRentPrice, 0, 1000000);
		reader.ReadInt("rentPeriodDays", m_iRentPeriodDays, 0, 3650);
		reader.ReadInt("gracePeriodDays", m_iGracePeriodDays, 0, 3650);
		reader.ReadInt("reclaimFee", m_iReclaimFee, 0, 1000000);
		reader.ReadFloat("weightCapacity", m_fWeightCapacity, 1, 10000);
	}

	//------------------------------------------------------------------------------------------------
	//! Cash for one rent period.
	static int RentPrice()
	{
		LF_LockerConfig config = Get();
		if (!config)
			return DEFAULT_RENT_PRICE;

		return config.m_iRentPrice;
	}

	//------------------------------------------------------------------------------------------------
	//! Days one rent payment lasts; 0 means a locker never lapses.
	static int RentPeriodDays()
	{
		LF_LockerConfig config = Get();
		if (!config)
			return DEFAULT_RENT_PERIOD_DAYS;

		return config.m_iRentPeriodDays;
	}

	//------------------------------------------------------------------------------------------------
	//! Days between the paid-until moment and the move to lost-and-found.
	static int GracePeriodDays()
	{
		LF_LockerConfig config = Get();
		if (!config)
			return DEFAULT_GRACE_PERIOD_DAYS;

		return config.m_iGracePeriodDays;
	}

	//------------------------------------------------------------------------------------------------
	//! Cash to take the contents back from lost-and-found.
	static int ReclaimFee()
	{
		LF_LockerConfig config = Get();
		if (!config)
			return DEFAULT_RECLAIM_FEE;

		return config.m_iReclaimFee;
	}

	//------------------------------------------------------------------------------------------------
	//! Kilograms one locker holds.
	static float WeightCapacity()
	{
		LF_LockerConfig config = Get();
		if (!config)
			return DEFAULT_WEIGHT_CAPACITY;

		return config.m_fWeightCapacity;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_LockerConfig's statics (registered at script load; Core/LF_StaticReset.c).
class LF_LockerConfig_StaticResetHook : LF_StaticResetHook
{
	protected static bool s_bRegistered = LF_StaticReset.Register(new LF_LockerConfig_StaticResetHook());

	override string Owner()
	{
		return "LF_LockerConfig";
	}

	override void Apply()
	{
		LF_LockerConfig.Reload();
	}
}
