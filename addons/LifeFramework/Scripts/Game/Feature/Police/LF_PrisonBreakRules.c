//------------------------------------------------------------------------------------------------
//! Prison break rules (#3420, #798), pure logic with no world, entity or UI. A break is rare and
//! player-started: it needs a free member of an organisation that has a member inside, enough
//! officers on duty, and the island cooldown spent. A good start opens a short window; the
//! prisoners choose to walk. The core ships it off (m_bEnabled 0); the owner turns it on.
//! Callers pass the facts and the clock (unix seconds); TryStart is the one place that decides,
//! and it logs every refusal as [LFDebug:PrisonBreak].
[BaseContainerProps(configRoot: true)]
class LF_PrisonBreakConfig
{
	static const ResourceName CONFIG_PATH = "{9F139D3FBF6AE924}Configs/Police/LF_PrisonBreak.conf";

	static const int DEFAULT_MIN_OFFICERS = 3;	// lf-fixed: the shipped default, mirrors the attribute and the conf
	static const int DEFAULT_COOLDOWN_SECONDS = 7200;	// lf-fixed: the shipped default, mirrors the attribute and the conf
	static const int DEFAULT_WINDOW_SECONDS = 90;	// lf-fixed: the shipped default, mirrors the attribute and the conf
	static const int DEFAULT_ESCAPE_TERM_SECONDS = 300;	// lf-fixed: the shipped default, mirrors the attribute and the conf

	static const int OFFICERS_MIN = 0;	// lf-fixed: the schema limit, mirrors the attribute params
	static const int OFFICERS_MAX = 32;	// lf-fixed: the schema limit, mirrors the attribute params
	static const int COOLDOWN_MAX = 86400;	// lf-fixed: the schema limit, one day
	static const int WINDOW_MIN = 10;	// lf-fixed: the schema limit, a door shorter than this cannot be used
	static const int WINDOW_MAX = 600;	// lf-fixed: the schema limit, ten minutes
	static const int ESCAPE_TERM_MAX = 7200;	// lf-fixed: the schema limit, two hours

	[Attribute("0", UIWidgets.CheckBox, "Prison breaks are allowed. Off by default: the owner turns the feature on")]
	bool m_bEnabled;

	[Attribute("3", UIWidgets.Auto, "Officers that must be on duty before a break can start", params: "0 32")]
	int m_iMinOfficersOnDuty;

	[Attribute("7200", UIWidgets.Auto, "Seconds after a break starts before the next one may start, island wide", params: "0 86400")]
	int m_iCooldownSeconds;

	[Attribute("90", UIWidgets.Auto, "Seconds the door stays open after a good start", params: "10 600")]
	int m_iWindowSeconds;

	[Attribute("300", UIWidgets.Auto, "Seconds added to the rest of the sentence when an escapee is arrested again", params: "0 7200")]
	int m_iEscapeTermSeconds;

	protected static ref LF_PrisonBreakConfig s_Config;
	protected static bool s_bLoadAttempted;

	//------------------------------------------------------------------------------------------------
	//! The loaded config, or null (logged once) when the file does not resolve.
	static LF_PrisonBreakConfig Get()
	{
		if (s_bLoadAttempted)
			return s_Config;

		s_bLoadAttempted = true;
		Resource container = BaseContainerTools.LoadContainer(CONFIG_PATH);
		if (!container || !container.GetResource())
		{
			LF_Debug.Error(LF_PrisonBreakRules.FEATURE, string.Format("prison break config does not resolve, breaks stay off: %1", CONFIG_PATH));
			return null;
		}

		s_Config = LF_PrisonBreakConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (!s_Config)
			LF_Debug.Error(LF_PrisonBreakRules.FEATURE, string.Format("prison break config has an unexpected root class, breaks stay off: %1", CONFIG_PATH));

		return s_Config;
	}

	//------------------------------------------------------------------------------------------------
	//! Drops the cached instance so the next Get() loads the file again.
	static void Reload()
	{
		s_Config = null;
		s_bLoadAttempted = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Tests: makes `config` the one Get returns until Reload.
	static void UseForTest(LF_PrisonBreakConfig config)
	{
		s_Config = config;
		s_bLoadAttempted = true;
	}

	//------------------------------------------------------------------------------------------------
	//! The shipped values in code, for tests; the members carry no initializers (#1077).
	static LF_PrisonBreakConfig CreateDefault()
	{
		LF_PrisonBreakConfig config = new LF_PrisonBreakConfig();
		config.m_bEnabled = false;
		config.m_iMinOfficersOnDuty = DEFAULT_MIN_OFFICERS;
		config.m_iCooldownSeconds = DEFAULT_COOLDOWN_SECONDS;
		config.m_iWindowSeconds = DEFAULT_WINDOW_SECONDS;
		config.m_iEscapeTermSeconds = DEFAULT_ESCAPE_TERM_SECONDS;
		return config;
	}

	//------------------------------------------------------------------------------------------------
	int MinOfficers()
	{
		return Math.ClampInt(m_iMinOfficersOnDuty, OFFICERS_MIN, OFFICERS_MAX);
	}

	//------------------------------------------------------------------------------------------------
	int CooldownSeconds()
	{
		return Math.ClampInt(m_iCooldownSeconds, 0, COOLDOWN_MAX);
	}

	//------------------------------------------------------------------------------------------------
	int WindowSeconds()
	{
		return Math.ClampInt(m_iWindowSeconds, WINDOW_MIN, WINDOW_MAX);
	}

	//------------------------------------------------------------------------------------------------
	int EscapeTermSeconds()
	{
		return Math.ClampInt(m_iEscapeTermSeconds, 0, ESCAPE_TERM_MAX);
	}
}

//------------------------------------------------------------------------------------------------
class LF_PrisonBreakRules
{
	static const string FEATURE = "PrisonBreak";

	// Refusal codes: a machine-readable reason, not player text. "" means the break may start.
	static const string REFUSE_OFF = "off";
	static const string REFUSE_NOT_FREE = "starter-not-free";
	static const string REFUSE_NOT_MEMBER = "starter-not-member";
	static const string REFUSE_NO_MEMBER_INSIDE = "no-member-inside";
	static const string REFUSE_TOO_FEW_OFFICERS = "too-few-officers";
	static const string REFUSE_COOLDOWN = "cooldown";
	static const string REFUSE_WINDOW_OPEN = "window-open";

	//! Unix time of the last good start, 0 for none; the island has one cooldown.
	protected static int s_iLastStartAt;
	//! Unix time the open window ends, 0 for none.
	protected static int s_iWindowEndsAt;

	//------------------------------------------------------------------------------------------------
	//! Why a break cannot start now, or "" when it can. Pure: changes nothing.
	//! \param config The rules; null refuses (a missing file keeps breaks off).
	//! \param starterFree The player who starts it is free: not cuffed, jailed or downed.
	//! \param starterIsMember The starter belongs to an organisation.
	//! \param membersInside Members of the starter's organisation currently serving a sentence.
	//! \param officersOnDuty Officers on duty now.
	//! \param now Unix seconds.
	static string Refuse(LF_PrisonBreakConfig config, bool starterFree, bool starterIsMember, int membersInside, int officersOnDuty, int now)
	{
		if (!config || !config.m_bEnabled)
			return REFUSE_OFF;

		if (!starterFree)
			return REFUSE_NOT_FREE;

		if (!starterIsMember)
			return REFUSE_NOT_MEMBER;

		if (membersInside < 1)
			return REFUSE_NO_MEMBER_INSIDE;

		if (officersOnDuty < config.MinOfficers())
			return REFUSE_TOO_FEW_OFFICERS;

		if (IsWindowOpen(now))
			return REFUSE_WINDOW_OPEN;

		if (s_iLastStartAt > 0 && now < s_iLastStartAt + config.CooldownSeconds())
			return REFUSE_COOLDOWN;

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Starts a break when Refuse allows it: records the start and opens the window.
	//! \param[out] refusal "" when it started, else the code from Refuse.
	//! \return true when the window opened.
	static bool TryStart(LF_PrisonBreakConfig config, bool starterFree, bool starterIsMember, int membersInside, int officersOnDuty, int now, out string refusal)
	{
		refusal = Refuse(config, starterFree, starterIsMember, membersInside, officersOnDuty, now);
		if (!refusal.IsEmpty())
		{
			LF_Debug.Info(FEATURE, string.Format("break refused: %1", refusal));
			return false;
		}

		s_iLastStartAt = now;
		s_iWindowEndsAt = now + config.WindowSeconds();
		LF_Debug.Info(FEATURE, string.Format("break started: door open %1 s, %2 members inside, %3 officers on duty", config.WindowSeconds(), membersInside, officersOnDuty));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the door is still open at `now`.
	static bool IsWindowOpen(int now)
	{
		return s_iWindowEndsAt > 0 && now < s_iWindowEndsAt;
	}

	//------------------------------------------------------------------------------------------------
	//! Seconds the door stays open from `now`, 0 when it is shut.
	static int WindowSecondsLeft(int now)
	{
		if (!IsWindowOpen(now))
			return 0;

		return s_iWindowEndsAt - now;
	}

	//------------------------------------------------------------------------------------------------
	//! Seconds until the next break may start, 0 when the cooldown is spent.
	static int CooldownSecondsLeft(LF_PrisonBreakConfig config, int now)
	{
		if (!config || s_iLastStartAt <= 0)
			return 0;

		int left = s_iLastStartAt + config.CooldownSeconds() - now;
		if (left < 0)
			return 0;

		return left;
	}

	//------------------------------------------------------------------------------------------------
	//! Closes the window early (the door is shut again). The cooldown keeps running.
	static void CloseWindow()
	{
		s_iWindowEndsAt = 0;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_iLastStartAt = 0;
		s_iWindowEndsAt = 0;
		LF_PrisonBreakConfig.Reload();
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_PrisonBreakRules' statics (LF_StaticReset finds it by reflection).
class LF_PrisonBreakRules_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_PrisonBreakRules";
	}

	override void Apply()
	{
		LF_PrisonBreakRules.ResetStatics();
	}
}
