//------------------------------------------------------------------------------------------------
//! The `dayCycle` owner section (M4, #1343): how fast the in-game clock runs. A shape only; the clock
//! driver (X1) reads this class.
//!
//! Owner JSON (field names are the contract the dependents code against):
//!   "dayCycle": { "mode": "vanilla", "dayLengthMinutes": 1440, "nightSpeed": 1, "startHour": 8,
//!                 "clockOffsetMinutes": 0 }
//!
//! - `mode` (m_sMode): `vanilla` (the base game's clock, the default), `accelerated` (one in-game
//!   day lasts `dayLengthMinutes` real minutes) or `realtime` (the in-game clock follows the real
//!   one, shifted by `clockOffsetMinutes`).
//! - `dayLengthMinutes` (m_iDayLengthMinutes): real minutes one in-game day lasts under
//!   `accelerated`, 1 to 10080. Ignored by the other modes.
//! - `nightSpeed` (m_fNightSpeed): not supported (#1650): the vanilla time manager has no
//!   night rate, so a value other than 1 logs a Warn. Accepted 0.1 to 10 so old configs load.
//! - `startHour` (m_fStartHour): the in-game hour (0 to 24) a fresh world starts at.
//! - `clockOffsetMinutes` (m_iClockOffsetMinutes): minutes added to the in-game clock, -1440 to 1440.
//!
//! The flagship's value (a 120-minute accelerated day) belongs in the flagship's config.json, never
//! here as a core default.
//------------------------------------------------------------------------------------------------
[BaseContainerProps(configRoot: true)]
class LF_DayCycleConfig
{
	//! The owner config.json section this class reads (LF_ConfigService).
	static const string SECTION = "dayCycle";
	static const string FEATURE = "DayCycle";

	static const string MODE_VANILLA = "vanilla";
	static const string MODE_ACCELERATED = "accelerated";
	static const string MODE_REALTIME = "realtime";

	static const int DEFAULT_DAY_MINUTES = 1440;
	static const float DEFAULT_NIGHT_SPEED = 1;
	static const float DEFAULT_START_HOUR = 8;

	static const int DAY_MIN = 1;	// lf-fixed: the schema limit, mirrors the attribute params
	static const int DAY_MAX = 10080;	// lf-fixed: the schema limit, one week
	static const float NIGHT_MIN = 0.1;	// lf-fixed: the schema limit, mirrors the attribute params
	static const float NIGHT_MAX = 10;	// lf-fixed: the schema limit, mirrors the attribute params
	static const float HOUR_MIN = 0;	// lf-fixed: the schema limit, mirrors the attribute params
	static const float HOUR_MAX = 24;	// lf-fixed: the schema limit, mirrors the attribute params
	static const int OFFSET_MIN = -1440;	// lf-fixed: the schema limit, a day back
	static const int OFFSET_MAX = 1440;	// lf-fixed: the schema limit, a day forward

	[Attribute("vanilla", UIWidgets.Auto, "How the in-game clock runs: vanilla (the base game's), accelerated (a day lasts dayLengthMinutes real minutes) or realtime (follows the real clock plus clockOffsetMinutes)")]
	string m_sMode;

	[Attribute("1440", UIWidgets.Auto, "Real minutes one in-game day lasts under accelerated mode. Ignored by the other modes", params: "1 10080")]
	int m_iDayLengthMinutes;

	[Attribute("1", UIWidgets.Auto, "Not supported (#1650): the vanilla time manager has one day duration and no night rate, so a value other than 1 logs a Warn and the night runs at the day's rate. Kept so old configs still load", params: "0.1 10")]
	float m_fNightSpeed;

	[Attribute("8", UIWidgets.Auto, "The in-game hour a fresh world starts at", params: "0 24")]
	float m_fStartHour;

	[Attribute("0", UIWidgets.Auto, "Minutes added to the in-game clock", params: "-1440 1440")]
	int m_iClockOffsetMinutes;

	protected static ref LF_DayCycleConfig s_Config;

	//------------------------------------------------------------------------------------------------
	//! The shipped values; the members carry no initializers (#1077).
	static LF_DayCycleConfig CreateDefault()
	{
		LF_DayCycleConfig config = new LF_DayCycleConfig();
		config.m_sMode = MODE_VANILLA;
		config.m_iDayLengthMinutes = DEFAULT_DAY_MINUTES;
		config.m_fNightSpeed = DEFAULT_NIGHT_SPEED;
		config.m_fStartHour = DEFAULT_START_HOUR;
		config.m_iClockOffsetMinutes = 0;
		return config;
	}

	//------------------------------------------------------------------------------------------------
	//! The shipped values with the owner's "dayCycle" section over them.
	static LF_DayCycleConfig Get()
	{
		if (s_Config)
			return s_Config;

		s_Config = CreateDefault();
		LF_ConfigReader reader = LF_ConfigService.OpenOwnerSection(SECTION);
		if (reader)
		{
			s_Config.ApplyOwner(reader);
			LF_Debug.Info(LF_ConfigService.FEATURE, string.Format("dayCycle: owner config read %1 fields, rejected %2", reader.GetAppliedCount(), reader.GetRejectedCount()));
		}

		return s_Config;
	}

	//------------------------------------------------------------------------------------------------
	//! Tests and the config reload: the next Get() layers the owner file again.
	static void Reload()
	{
		s_Config = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Tests: use this config until Reload.
	static void Use(LF_DayCycleConfig config)
	{
		s_Config = config;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsMode(string mode)
	{
		return mode == MODE_VANILLA || mode == MODE_ACCELERATED || mode == MODE_REALTIME;
	}

	//------------------------------------------------------------------------------------------------
	//! Applies the owner's "dayCycle" section field by field; a key left out keeps the shipped value.
	void ApplyOwner(notnull LF_ConfigReader reader)
	{
		array<string> ownerKeys = {"mode", "dayLengthMinutes", "nightSpeed", "startHour", "clockOffsetMinutes"};
		reader.WarnUnknownKeys(ownerKeys);

		string mode = m_sMode;
		if (reader.ReadString("mode", mode))
		{
			mode.ToLower();
			if (IsMode(mode))
				m_sMode = mode;
			else
				LF_Debug.Error(FEATURE, string.Format("dayCycle.mode = %1 is not vanilla, accelerated or realtime; keeping %2", mode, m_sMode));
		}

		reader.ReadInt("dayLengthMinutes", m_iDayLengthMinutes, DAY_MIN, DAY_MAX);
		reader.ReadFloat("nightSpeed", m_fNightSpeed, NIGHT_MIN, NIGHT_MAX);
		reader.ReadFloat("startHour", m_fStartHour, HOUR_MIN, HOUR_MAX);
		reader.ReadInt("clockOffsetMinutes", m_iClockOffsetMinutes, OFFSET_MIN, OFFSET_MAX);
	}
}
