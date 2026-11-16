//------------------------------------------------------------------------------------------------
//! Police contact stamps (#1972, combat logout #1794). A stop, a search or a chase report (an
//! identification of the suspect) stamps "police contact" on that character; LF_DisconnectRules
//! reads how old the stamp is at logout, and a wanted player who leaves inside the contact window
//! (owner config "disconnect.contactWindowSeconds", default 120) is jailed like a cuffed logout.
//!
//! The stamp is session state: it only matters for a couple of minutes and the logout decision it
//! feeds is applied at once to the jail payload (the sentence is what survives a restart). Keyed by
//! the character uid, never an entity. Server only.
//!
//! Sources: the officer identifications on the bus (LF_OnSuspectIdentified with how "ID check" or
//! "caught in the act", IsContactSource); a stop or search action calls
//! LF_PoliceContact.Stamp(uid, ...) directly: a search (LF_ConfiscateAction.StampContact, "search")
//! and a completed cuffing (LF_Cuffing.StampContact, "cuffing"), both #2102.
//------------------------------------------------------------------------------------------------
class LF_PoliceContact
{
	static const string FEATURE = "Rejoin";

	protected static ref map<string, int> s_mStampMs;
	protected static ref LF_PoliceContactSubscriber s_Subscriber;

	//------------------------------------------------------------------------------------------------
	protected static map<string, int> Stamps()
	{
		if (!s_mStampMs)
			s_mStampMs = new map<string, int>();

		return s_mStampMs;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: uid had police contact now. \param source "stop", "search", "chase report", ...
	static void Stamp(string uid, string source)
	{
		if (!Replication.IsServer())
			return;

		World world = GetGame().GetWorld();
		if (!world)
			return;

		StampAt(uid, source, world.GetWorldTime());
	}

	//------------------------------------------------------------------------------------------------
	//! The stamp at an explicit world time in milliseconds (tests, Stamp).
	static void StampAt(string uid, string source, int worldTimeMs)
	{
		if (uid.IsEmpty())
		{
			LF_Debug.Warn(FEATURE, string.Format("police contact (%1) not stamped: empty uid", source));
			return;
		}

		Stamps().Set(uid, worldTimeMs);
		LF_Debug.Info(FEATURE, string.Format("%1 police contact stamped (%2)", uid, source));
	}

	//------------------------------------------------------------------------------------------------
	//! Seconds since uid's last contact, or -1 when there is none.
	static int AgeSeconds(string uid, int nowMs)
	{
		if (!s_mStampMs || uid.IsEmpty() || !s_mStampMs.Contains(uid))
			return -1;

		return Math.Max(0, (nowMs - s_mStampMs.Get(uid)) / 1000);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: seconds since uid's last contact right now, or -1.
	static int AgeSecondsNow(string uid)
	{
		World world = GetGame().GetWorld();
		if (!world)
			return -1;

		return AgeSeconds(uid, world.GetWorldTime());
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: is a contact this many seconds old inside the window? -1 (none) and a window of 0 (off) never are.
	static bool InWindow(int ageSeconds, int windowSeconds)
	{
		return ageSeconds >= 0 && windowSeconds > 0 && ageSeconds <= windowSeconds;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: does this identification mean an officer met the suspect? An ID check and a caught-in-
	//! the-act report do; a serial trace, an unpaid ticket or a till walk-out identify with no officer there.
	static bool IsContactSource(string how)
	{
		return how == "ID check" || how == "caught in the act";
	}

	//------------------------------------------------------------------------------------------------
	//! The stamp of a player who is gone does not outlive the rejoin decision.
	static void Forget(string uid)
	{
		if (s_mStampMs && !uid.IsEmpty())
			s_mStampMs.Remove(uid);
	}

	//------------------------------------------------------------------------------------------------
	//! Tests and world end.
	static void Reset()
	{
		s_mStampMs = null;
		s_Subscriber = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: listens for suspect identifications. Called when the game mode starts.
	static bool Install()
	{
		if (!Replication.IsServer())
			return false;

		if (s_Subscriber && LF_EventBus.SubscriberCount(LF_OnSuspectIdentified) > 0)
			return true;

		s_Subscriber = new LF_PoliceContactSubscriber();
		return LF_EventBus.Subscribe(LF_OnSuspectIdentified, s_Subscriber);
	}
}

//------------------------------------------------------------------------------------------------
//! A suspect identified by the police (an ID check, a report) had contact with them.
class LF_PoliceContactSubscriber : LF_EventSubscriber
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "PoliceContact";
	}

	//------------------------------------------------------------------------------------------------
	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnSuspectIdentified identified = LF_OnSuspectIdentified.Cast(busEvent);
		if (!identified)
			return false;

		if (!LF_PoliceContact.IsContactSource(identified.m_sHow))
			return true;

		LF_PoliceContact.Stamp(identified.m_sPlayerUid, "chase report: " + identified.m_sHow);
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Subscribes the contact stamps once per world (server only).
modded class LF_GameModeRoleplay
{
	//------------------------------------------------------------------------------------------------
	protected override void OnGameStart()
	{
		super.OnGameStart();
		LF_PoliceContact.Install();
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_PoliceContact's statics (registered at script load; Core/LF_StaticReset.c).
class LF_PoliceContact_StaticResetHook : LF_StaticResetHook
{
	protected static bool s_bRegistered = LF_StaticReset.Register(new LF_PoliceContact_StaticResetHook());

	override string Owner()
	{
		return "LF_PoliceContact";
	}

	override void Apply()
	{
		LF_PoliceContact.Reset();
	}
}
