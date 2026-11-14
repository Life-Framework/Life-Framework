//------------------------------------------------------------------------------------------------
//! The `restarts` owner section (M4, #1343): when the server restarts each day and how far ahead
//! players are warned. A shape only; the scheduler and the warnings (O5) read this class.
//!
//! Owner JSON (field names are the contract the dependents code against):
//!   "restarts": { "times": ["04:00", "16:30"], "warningMinutes": [15, 5, 1] }
//!
//! - `times` (m_aTimes): daily restart times, 24-hour `HH:MM`, server local time. Empty = no
//!   scheduled restarts (the default).
//! - `warningMinutes` (m_aWarningMinutes): minutes before a restart at which players are warned,
//!   each 1 to 120. Default 15, 5, 1.
//! A bad list (a time that is not HH:MM, a lead outside 1 to 120) is refused whole, logged, and the
//! shipped list stays. Server only: nothing here is streamed to clients.
//------------------------------------------------------------------------------------------------
[BaseContainerProps(configRoot: true)]
class LF_RestartConfig
{
	//! The owner config.json section this class reads (LF_ConfigService).
	static const string SECTION = "restarts";
	static const string FEATURE = "Restarts";

	static const int LEAD_MIN = 1;	// lf-fixed: the schema limit, mirrors the attribute params
	static const int LEAD_MAX = 120;	// lf-fixed: the schema limit, mirrors the attribute params
	static const int TIMES_MAX = 24;	// lf-fixed: the schema limit, one restart an hour

	[Attribute("", UIWidgets.Auto, "Daily restart times, 24-hour HH:MM in the server's local time (for example 04:00). Empty means no scheduled restarts")]
	ref array<string> m_aTimes;

	[Attribute("", UIWidgets.Auto, "Minutes before a restart at which players are warned, each 1 to 120. The shipped leads are 15, 5 and 1")]
	ref array<int> m_aWarningMinutes;

	protected static ref LF_RestartConfig s_Config;

	//------------------------------------------------------------------------------------------------
	//! The shipped values; the members carry no initializers (#1077).
	static LF_RestartConfig CreateDefault()
	{
		LF_RestartConfig config = new LF_RestartConfig();
		config.m_aTimes = {};
		config.m_aWarningMinutes = {15, 5, 1};
		return config;
	}

	//------------------------------------------------------------------------------------------------
	//! The shipped values with the owner's "restarts" section over them.
	static LF_RestartConfig Get()
	{
		if (s_Config)
			return s_Config;

		s_Config = CreateDefault();
		LF_ConfigReader reader = LF_ConfigService.OpenOwnerSection(SECTION);
		if (reader)
		{
			s_Config.ApplyOwner(reader);
			LF_Debug.Info(LF_ConfigService.FEATURE, string.Format("restarts: owner config read %1 fields, rejected %2", reader.GetAppliedCount(), reader.GetRejectedCount()));
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
	static void Use(LF_RestartConfig config)
	{
		s_Config = config;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True for a 24-hour clock time written HH:MM ("04:00", "16:30").
	static bool IsValidTime(string time)
	{
		if (time.Length() != 5 || time.Get(2) != ":")
			return false;

		string hh = time.Substring(0, 2);
		string mm = time.Substring(3, 2);
		if (!IsDigits(hh) || !IsDigits(mm))
			return false;

		return hh.ToInt() <= 23 && mm.ToInt() <= 59;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool IsDigits(string text)
	{
		for (int i = 0; i < text.Length(); i++)
		{
			string c = text.Get(i);
			if (c < "0" || c > "9")
				return false;
		}
		return !text.IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	//! Applies the owner's "restarts" section field by field; a key left out keeps the shipped value.
	void ApplyOwner(notnull LF_ConfigReader reader)
	{
		array<string> ownerKeys = {"times", "warningMinutes"};
		reader.WarnUnknownKeys(ownerKeys);

		array<string> times = {};
		if (reader.ReadStrings("times", times))
		{
			string bad;
			foreach (string time : times)
			{
				if (!IsValidTime(time))
				{
					bad = time;
					break;
				}
			}

			if (!bad.IsEmpty())
				LF_Debug.Error(FEATURE, string.Format("restarts.times has %1, which is not HH:MM; keeping the shipped list", bad));
			else if (times.Count() > TIMES_MAX)
				LF_Debug.Error(FEATURE, string.Format("restarts.times has %1 entries, at most %2; keeping the shipped list", times.Count(), TIMES_MAX));
			else
				m_aTimes.Copy(times);
		}

		array<int> leads = {};
		if (reader.ReadInts("warningMinutes", leads, LEAD_MIN, LEAD_MAX))
			m_aWarningMinutes.Copy(leads);
	}
}
