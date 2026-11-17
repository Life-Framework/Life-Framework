//------------------------------------------------------------------------------------------------
//! The grace period setting (Configs/Moderation/LF_Grace.conf). A missing or broken file logs once
//! and leaves the grace off. The owner's "grace" section sets the minutes over the .conf (#1779):
//!   "grace": { "minutes": 30 }
[BaseContainerProps(configRoot: true)]
class LF_GraceConfig
{
	//! The owner config.json section this class reads (LF_ConfigService).
	static const string SECTION = "grace";
	static const int MINUTES_MIN = 0;	// lf-fixed: the schema limit, mirrors the attribute params
	static const int MINUTES_MAX = 600;	// lf-fixed: the schema limit, ten hours

	static const ResourceName CONFIG_PATH = "{D9E1D9BFA17BD167}Configs/Moderation/LF_Grace.conf";

	protected static ref LF_GraceConfig s_Config;
	protected static bool s_bLoadAttempted;

	[Attribute("0", UIWidgets.Auto, "Minutes of playtime a new character is protected from being held up or arrested (and may not rob). 0 = no grace. A first crime ends it early", params: "0 600")]
	int m_iMinutes;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_Config = null;
		s_bLoadAttempted = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Minutes of grace this server gives; 0 when the file is missing, broken or says so.
	static int Minutes()
	{
		if (!s_bLoadAttempted)
		{
			s_bLoadAttempted = true;
			Resource container = BaseContainerTools.LoadContainer(CONFIG_PATH);
			if (!container || !container.GetResource())
			{
				LF_Debug.Error(LF_Grace.FEATURE, string.Format("grace config does not resolve, grace is off: %1", CONFIG_PATH));
			}
			else
			{
				s_Config = LF_GraceConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
				if (!s_Config)
					LF_Debug.Error(LF_Grace.FEATURE, string.Format("grace config has an unexpected root class, grace is off: %1", CONFIG_PATH));
			}

			if (s_Config)
			{
				LF_ConfigReader reader = LF_ConfigService.OpenOwnerSection(SECTION);
				if (reader)
				{
					array<string> ownerKeys = {"minutes"};
					reader.WarnUnknownKeys(ownerKeys);
					reader.ReadInt("minutes", s_Config.m_iMinutes, MINUTES_MIN, MINUTES_MAX);
					LF_Debug.Info(LF_ConfigService.FEATURE, string.Format("grace: owner config read %1 fields, rejected %2", reader.GetAppliedCount(), reader.GetRejectedCount()));
				}
			}
		}

		if (!s_Config || s_Config.m_iMinutes < 0)
			return 0;

		return s_Config.m_iMinutes;
	}
}

//------------------------------------------------------------------------------------------------
//! The new-character grace period (#1780, chapter Arrival #1007, #794).
//!
//! For its first N minutes of playtime a character cannot be held up or arrested, and may not
//! rob (a protected robber would be a robber nobody can arrest). The protection ends for good
//! the first time the character completes any crime. Nothing is saved here: the clock is the
//! character's own persisted played time (LF_GraceCharacterClock, #2443; a second character on an
//! old account gets its own grace) and the end is the persisted crime record of the character
//! (LF_CrimeRecord.Total), so a restart or a relog neither restarts nor extends it. The server decides; a client offer check cannot see playtime, so the server
//! refuses and tells the player (the refusal names the grace). Nothing is replicated.
//! Off by default (LF_GraceConfig.m_iMinutes = 0).
class LF_Grace
{
	static const string FEATURE = "Grace";
	static const string REFUSAL_KEY = "#LF-Grace_Refused";
	static const string TITLE_KEY = "#LF-Grace_Title";

	//! Test seam: -1 reads the config, otherwise the minutes to use.
	protected static int s_iMinutesOverride = -1;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_iMinutesOverride = -1;
	}

	//------------------------------------------------------------------------------------------------
	//! Tests only: pins the grace length; -1 gives the config back.
	static void SetMinutesOverride(int minutes)
	{
		s_iMinutesOverride = minutes;
	}

	//------------------------------------------------------------------------------------------------
	static int Minutes()
	{
		if (s_iMinutesOverride >= 0)
			return s_iMinutesOverride;

		return LF_GraceConfig.Minutes();
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: is a character with this much playtime and this many completed crimes protected?
	static bool IsProtected(int graceMinutes, int playtimeSeconds, int crimesCommitted)
	{
		if (graceMinutes <= 0)
			return false;

		if (crimesCommitted > 0)
			return false;

		return playtimeSeconds < graceMinutes * 60;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: is the character with this record key in its grace now? The clock is the character's
	//! own played time, never its account's (#2443).
	static bool Protects(string recordKey)
	{
		if (recordKey.IsEmpty() || Minutes() <= 0)
			return false;

		// A clock stops at CAP_SECONDS: a .conf longer than MINUTES_MAX would otherwise never end.
		int played = LF_GraceCharacterClock.Seconds(recordKey);
		if (played >= LF_GraceCharacterClock.CAP_SECONDS)
			return false;

		return IsProtected(Minutes(), played, LF_CrimeRecord.Total(recordKey));
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the gate every crime path asks. Logs the refusal when the character is protected.
	//! \param what What was refused ("hold-up", "arrest", ...), for the log.
	static bool Refuses(string recordKey, string what)
	{
		if (!Protects(recordKey))
			return false;

		LF_Debug.Info(FEATURE, string.Format("%1 refused: %2 is in its grace period", what, recordKey));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Tells the player whose action (or whose protected target) was refused, naming the grace.
	static void Notify(IEntity player)
	{
		LF_Utils.NotifyEntity(player, REFUSAL_KEY, TITLE_KEY, 4.0);
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_Grace's statics (registered at script load; Core/LF_StaticReset.c).
class LF_Grace_StaticResetHook : LF_StaticResetHook
{
	protected static bool s_bRegistered = LF_StaticReset.Register(new LF_Grace_StaticResetHook());

	override string Owner()
	{
		return "LF_Grace";
	}

	override void Apply()
	{
		LF_Grace.ResetStatics();
		LF_GraceConfig.ResetStatics();
	}
}
