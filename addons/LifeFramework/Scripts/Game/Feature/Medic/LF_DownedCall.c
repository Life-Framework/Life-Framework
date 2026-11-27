//------------------------------------------------------------------------------------------------
//! Downed-player calls (#5152): a player who goes down rings the on-duty medics the same way a
//! casualty does (LF_CasualtyCalls). LF_OnPlayerDowned places a medic service call through
//! LF_ServiceCalls.Place at the body, so the clinic's medic board row, the pager card, the toast and
//! the dispatch map mark all come from the one existing call path. The caller is the downed player,
//! so when a medic takes the call the patient hears that a medic is on the way. The call closes
//! (LF_OnServiceCallClosed: board row and map mark go) when the player is revived, delivered to a
//! bed (which revives first) or dies (bleed-out, give-up). The downed player is never a receiver of
//! their own call. An NPC body (a casualty, the practice patient) places nothing here: a casualty
//! places its own call. Server only; nothing new crosses the network.
class LF_DownedCalls
{
	static const string FEATURE = "DownedCall";
	static const string SERVICE = "medic";
	static const string SUMMARY = "#LF-DownedCall_Summary";

	//! Uids of the downed players whose call is open.
	protected static ref set<string> s_Open;
	protected static ref LF_DownedCallSubscriber s_Subscriber;
	//! Tests: the routing candidates and victim id the bus path uses instead of the connected players.
	protected static ref array<ref LF_ServiceCandidate> s_aTestPlayers;
	protected static int s_iTestVictimId = -1;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		Uninstall();
		s_Open = null;
		s_aTestPlayers = null;
		s_iTestVictimId = -1;
	}

	//------------------------------------------------------------------------------------------------
	//! Tests: route the bus path to these candidates (null restores the connected players).
	static void SetTestPlayers(array<ref LF_ServiceCandidate> players, int victimId)
	{
		s_aTestPlayers = players;
		s_iTestVictimId = victimId;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the bus entry for LF_OnPlayerDowned.
	static LF_ServiceRoute HandleDowned(notnull LF_OnPlayerDowned downed)
	{
		return OnDowned(downed.m_sPlayerUid, downed.m_vPosition, s_aTestPlayers, s_iTestVictimId);
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: does a downed body with this uid ring the medics? An NPC ("npc" or no uid) does not.
	static bool Calls(string playerUid)
	{
		return !playerUid.IsEmpty() && playerUid != "npc";
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: the routing candidates without the downed player (a medic who goes down is not paged
	//! about themselves). \param victimId The downed player's id; <= 0 keeps everyone.
	static array<ref LF_ServiceCandidate> WithoutVictim(notnull array<ref LF_ServiceCandidate> players, int victimId)
	{
		array<ref LF_ServiceCandidate> result = {};
		foreach (LF_ServiceCandidate player : players)
		{
			if (player && (victimId <= 0 || player.m_iPlayerId != victimId))
				result.Insert(player);
		}
		return result;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a player went down; page the medics. \param players Routing candidates (tests); null
	//! reads the connected players. \return The route, or null when no call was placed.
	static LF_ServiceRoute OnDowned(string playerUid, vector position, array<ref LF_ServiceCandidate> players = null, int victimId = -1)
	{
		if (!Replication.IsServer() || !Calls(playerUid))
			return null;

		// A strong local: the fresh candidate list must outlive this call.
		ref array<ref LF_ServiceCandidate> list = players;
		if (!list)
		{
			list = LF_ServiceCalls.Candidates();
			victimId = LF_ServiceTake.FindPlayerId(playerUid);
		}

		ref array<ref LF_ServiceCandidate> receivers = WithoutVictim(list, victimId);
		LF_ServiceRoute route = LF_ServiceCalls.Place(playerUid, SERVICE, LF_WantedReports.GridPlace(position), SUMMARY, position, receivers);
		if (!route || !route.IsAnswered())
		{
			LF_Debug.Info(FEATURE, string.Format("downed %1 at %2: no medic on duty to page", playerUid, position));
			return null;
		}

		Open().Insert(playerUid);
		LF_Debug.Info(FEATURE, string.Format("downed %1 at %2 paged %3 medic(s)", playerUid, position, route.m_aReceivers.Count()));
		return route;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the downed player is up again or dead; close their call. \return True when one was open.
	static bool Close(string playerUid, string reason)
	{
		if (!s_Open || !s_Open.Contains(playerUid))
			return false;

		s_Open.RemoveItem(playerUid);
		LF_EventBus.Publish(LF_OnServiceCallClosed.Create(SERVICE, playerUid));
		LF_Debug.Info(FEATURE, string.Format("call of downed %1 closed (%2)", playerUid, reason));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsOpen(string playerUid)
	{
		return s_Open && s_Open.Contains(playerUid);
	}

	//------------------------------------------------------------------------------------------------
	protected static set<string> Open()
	{
		if (!s_Open)
			s_Open = new set<string>();
		return s_Open;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: subscribe to the downed, revived and killed events (idempotent, also after a bus reset).
	static void Install()
	{
		if (!Replication.IsServer())
			return;

		Uninstall();
		s_Subscriber = new LF_DownedCallSubscriber();
		LF_EventBus.Subscribe(LF_OnPlayerDowned, s_Subscriber);
		LF_EventBus.Subscribe(LF_OnPlayerRevived, s_Subscriber);
		LF_EventBus.Subscribe(LF_OnPlayerKilled, s_Subscriber);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Uninstall()
	{
		if (!s_Subscriber)
			return;

		LF_EventBus.Unsubscribe(LF_OnPlayerDowned, s_Subscriber);
		LF_EventBus.Unsubscribe(LF_OnPlayerRevived, s_Subscriber);
		LF_EventBus.Unsubscribe(LF_OnPlayerKilled, s_Subscriber);
		s_Subscriber = null;
	}
}

//------------------------------------------------------------------------------------------------
class LF_DownedCallSubscriber : LF_EventSubscriber
{
	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnPlayerDowned downed = LF_OnPlayerDowned.Cast(busEvent);
		if (downed)
		{
			LF_DownedCalls.HandleDowned(downed);
			return true;
		}

		LF_OnPlayerRevived revived = LF_OnPlayerRevived.Cast(busEvent);
		if (revived)
		{
			LF_DownedCalls.Close(revived.m_sPlayerUid, "revived");
			return true;
		}

		LF_OnPlayerKilled killed = LF_OnPlayerKilled.Cast(busEvent);
		if (killed)
		{
			LF_DownedCalls.Close(killed.m_sVictimUid, "died");
			return true;
		}

		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! Self-start: the server listens from the world's start.
modded class LF_GameModeRoleplay
{
	//------------------------------------------------------------------------------------------------
	protected override void OnGameStart()
	{
		super.OnGameStart();
		LF_DownedCalls.Install();
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_DownedCalls' statics (LF_StaticReset finds it by reflection).
class LF_DownedCalls_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_DownedCalls";
	}

	override void Apply()
	{
		LF_DownedCalls.ResetStatics();
	}
}
