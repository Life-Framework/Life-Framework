//------------------------------------------------------------------------------------------------
//! Subscribes to LF_OnPlayerSpawnedEvent and shows the configured notice once per player per
//! session (#1448). Server only.
class LF_JoinNoticeSubscriber : LF_EventSubscriber
{
	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnPlayerSpawnedEvent spawned = LF_OnPlayerSpawnedEvent.Cast(busEvent);
		if (!spawned)
			return false;

		LF_JoinNotice.OnSpawned(spawned);
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! The alpha notice: one toast at a player's first spawn of the session, text from a localization
//! key in the `joinNotice` owner section. Off by default in the core.
class LF_JoinNotice
{
	static const string FEATURE = "JoinNotice";
	static const float DURATION_S = 12.0;	// lf-fixed: how long the toast reads, not a server rule

	protected static ref LF_JoinNoticeSubscriber s_Subscriber;
	//! Players told this session: the account uid, or "player:<id>" while the uid has not arrived.
	protected static ref set<string> s_mShown;
	protected static bool s_bWarnedEmptyKey;
	protected static bool s_bCountOnly;
	protected static int s_iToasts;
	protected static int s_iWarnings;
	protected static int s_iLastPlayerId;
	protected static string s_sLastMessage;

	//------------------------------------------------------------------------------------------------
	//! Starts listening for spawns. Called once per world from the game mode start.
	static void Install()
	{
		if (!Replication.IsServer())
			return;

		if (s_Subscriber)
			LF_EventBus.Unsubscribe(LF_OnPlayerSpawnedEvent, s_Subscriber);

		s_Subscriber = new LF_JoinNoticeSubscriber();
		LF_EventBus.Subscribe(LF_OnPlayerSpawnedEvent, s_Subscriber);
	}

	//------------------------------------------------------------------------------------------------
	//! One spawn: shows the toast the first time this player is seen this session.
	static void OnSpawned(notnull LF_OnPlayerSpawnedEvent spawned)
	{
		LF_JoinNoticeConfig config = LF_JoinNoticeConfig.Get();
		if (!config || !config.m_bEnabled)
			return;

		if (!spawned.m_bFirstThisSession)
			return;

		if (config.m_sKey.IsEmpty())
		{
			if (!s_bWarnedEmptyKey)
			{
				s_bWarnedEmptyKey = true;
				s_iWarnings++;
				LF_Debug.Warn(FEATURE, "the join notice is enabled with an empty key: nothing is shown. Set joinNotice.key");
			}

			return;
		}

		string who = spawned.m_sAccountUid;
		if (who.IsEmpty())
			who = string.Format("player:%1", spawned.m_iPlayerId);

		if (!s_mShown)
			s_mShown = new set<string>();

		if (s_mShown.Find(who) >= 0)
			return;

		s_mShown.Insert(who);
		Show(spawned.m_iPlayerId, config.m_sKey);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Show(int playerId, string key)
	{
		string message = key;
		if (!message.StartsWith("#"))
			message = "#" + message;

		s_iToasts++;
		s_iLastPlayerId = playerId;
		s_sLastMessage = message;
		LF_Debug.Info(FEATURE, string.Format("join notice %1 shown to player %2", message, playerId));
		if (s_bCountOnly)
			return;

		LF_NotificationManagerComponent.NotifyPlayer(playerId, string.Empty, message, DURATION_S);
	}

	//------------------------------------------------------------------------------------------------
	//! Tests: count toasts instead of sending them.
	static void CountOnly(bool on)
	{
		s_bCountOnly = on;
	}

	static int ToastCount()
	{
		return s_iToasts;
	}

	static int WarnCount()
	{
		return s_iWarnings;
	}

	static int LastPlayerId()
	{
		return s_iLastPlayerId;
	}

	static string LastMessage()
	{
		return s_sLastMessage;
	}

	//------------------------------------------------------------------------------------------------
	//! A new session: every player is told again.
	static void ResetSession()
	{
		s_mShown = null;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		if (s_Subscriber)
			LF_EventBus.Unsubscribe(LF_OnPlayerSpawnedEvent, s_Subscriber);

		s_Subscriber = null;
		s_mShown = null;
		s_bWarnedEmptyKey = false;
		s_bCountOnly = false;
		s_iToasts = 0;
		s_iWarnings = 0;
		s_iLastPlayerId = 0;
		s_sLastMessage = string.Empty;
	}
}

//------------------------------------------------------------------------------------------------
//! Self-start: the notice listens from the moment the world starts, on the server only.
modded class LF_GameModeRoleplay
{
	//------------------------------------------------------------------------------------------------
	protected override void OnGameStart()
	{
		super.OnGameStart();
		LF_JoinNotice.Install();
	}
}
