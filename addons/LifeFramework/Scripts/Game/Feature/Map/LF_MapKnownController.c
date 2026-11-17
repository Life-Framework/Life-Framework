//------------------------------------------------------------------------------------------------
//! The client side of discovery (card M13): every SCAN_MS the local player's machine looks for
//! DISCOVER places in reach that its character has not visited and asks the server once per place
//! (LF_MapKnownController.RpcAsk). The server decides; the client never marks a place itself.
class LF_MapKnownScan
{
	static const int SCAN_MS = 2000; // lf-fixed: everon-map.md section 4, the client's look-around period
	//! No discovery from a vehicle faster than 60 km/h (16.7 m/s): driving past is not a visit.
	static const float MAX_SPEED_MS = 16.7; // lf-fixed: everon-map.md section 4, drive-by bound

	protected static bool s_bRunning;
	//! Place id -> tick of our last ask, so an unanswered ask is retried after one window.
	protected static ref map<string, int> s_mAsked;
	protected static ref array<int> s_aAskTimes;

	//------------------------------------------------------------------------------------------------
	static void Start()
	{
		if (s_bRunning)
			return;

		s_bRunning = true;
		GetGame().GetCallqueue().CallLater(Tick, SCAN_MS, true);
		LF_Debug.Info(LF_MapKnown.FEATURE, "discovery scan started");
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		if (s_bRunning && GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(Tick);

		s_bRunning = false;
		s_mAsked = null;
		s_aAskTimes = null;
	}

	//------------------------------------------------------------------------------------------------
	//! The join sync replaced the known set: asks still in flight may be asked again.
	static void ClearAsked()
	{
		s_mAsked = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Our own copy of the server's rate window, so a street of shops queues instead of being refused.
	protected static bool MayAsk(int nowMs)
	{
		if (!s_aAskTimes)
			s_aAskTimes = {};

		for (int i = s_aAskTimes.Count() - 1; i >= 0; i--)
		{
			if (nowMs - s_aAskTimes[i] >= LF_MapKnownRules.WINDOW_MS || nowMs < s_aAskTimes[i])
				s_aAskTimes.Remove(i);
		}

		return s_aAskTimes.Count() < LF_MapKnownRules.ASKS_PER_WINDOW;
	}

	//------------------------------------------------------------------------------------------------
	protected static void Tick()
	{
		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!controller)
			return;

		IEntity character = controller.GetControlledEntity();
		if (!character)
			return;

		Physics physics = character.GetRootParent().GetPhysics();
		if (physics && physics.GetVelocity().Length() > MAX_SPEED_MS)
			return;

		LF_MapPoiConfig config = LF_MapPoiConfig.Get();
		if (!config || !config.m_aKinds)
			return;

		int nowMs = System.GetTickCount();
		if (!MayAsk(nowMs))
			return;

		if (!s_mAsked)
			s_mAsked = new map<string, int>();

		vector here = character.GetOrigin();
		foreach (LF_MapPoiKind row : config.m_aKinds)
		{
			if (!row || row.m_eVisibility != LF_EMapPoiVisibility.DISCOVER)
				continue;

			array<string> ids = {};
			array<IEntity> entities = {};
			LF_Places.FindAllWithIds(row.m_sKind, ids, entities);
			foreach (int i, IEntity place : entities)
			{
				string placeId = ids[i];
				if (LF_MapKnown.LocalKnownOf(placeId) == LF_EMapPoiKnown.VISITED)
					continue;

				int askedAt;
				if (s_mAsked.Find(placeId, askedAt) && nowMs - askedAt < LF_MapKnownRules.WINDOW_MS && nowMs >= askedAt)
					continue;

				if (vector.Distance(here, place.GetOrigin()) > LF_MapKnownRules.DEFAULT_RADIUS)
					continue;

				// One ask per tick: the window allows five a minute.
				s_mAsked.Set(placeId, nowMs);
				s_aAskTimes.Insert(nowMs);
				controller.LF_AskDiscover(row.m_sKind, placeId);
				return;
			}
		}
	}
}

//------------------------------------------------------------------------------------------------
//! Server: the join sync again a moment later, for a player whose character key was not ready.
//! Static and keyed by player id, so a player who left in between is simply skipped.
class LF_MapKnownSync
{
	static const int RETRIES = 5; // lf-fixed: join timing bound, not a gameplay knob
	static const int RETRY_MS = 2000; // lf-fixed: see RETRIES

	//------------------------------------------------------------------------------------------------
	static void Retry(int playerId, int retries)
	{
		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
		if (controller)
			controller.LF_SendMapKnownAll(retries);
	}

	//------------------------------------------------------------------------------------------------
	//! Least time between two sync answers to one player. Its own gate, not the shared COMMS bucket: a
	//! player who just made three chess moves or board reads and then respawned lost the new character's
	//! set there, with no retry (review 2026-11-17).
	static const int MIN_GAP_MS = 1000; // lf-fixed: anti-spam floor; a sync is once per controlled-entity change

	//! Player id -> tick of the last answered sync.
	protected static ref map<int, int> s_mLastMs;
	//! Players with a deferred sync queued.
	protected static ref map<int, bool> s_mPending;

	//------------------------------------------------------------------------------------------------
	//! Server: what to do with one sync ask. SEND records it; DEFER marks one queued send (the caller
	//! schedules Deferred after MIN_GAP_MS), since a second change in a second may be the one for the new
	//! character; DROP while one is already queued, which answers with the newest set anyway.
	static LF_EMapKnownSyncGate Admit(int playerId, int nowMs)
	{
		if (!s_mLastMs)
			s_mLastMs = new map<int, int>();

		if (!s_mPending)
			s_mPending = new map<int, bool>();

		int last;
		if (s_mLastMs.Find(playerId, last) && nowMs >= last && nowMs - last < MIN_GAP_MS)
		{
			if (s_mPending.Contains(playerId))
				return LF_EMapKnownSyncGate.DROP;

			s_mPending.Set(playerId, true);
			return LF_EMapKnownSyncGate.DEFER;
		}

		s_mLastMs.Set(playerId, nowMs);
		return LF_EMapKnownSyncGate.SEND;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the queued sync of a deferred ask. A player who left in between is skipped.
	static void Deferred(int playerId)
	{
		if (s_mPending)
			s_mPending.Remove(playerId);

		if (!s_mLastMs)
			s_mLastMs = new map<int, int>();

		s_mLastMs.Set(playerId, System.GetTickCount());
		PlayerManager players = GetGame().GetPlayerManager();
		if (!players)
			return;

		SCR_PlayerController controller = SCR_PlayerController.Cast(players.GetPlayerController(playerId));
		if (controller)
			controller.LF_SendMapKnownAll(RETRIES);
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		if (s_mPending && !s_mPending.IsEmpty() && GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(Deferred);

		s_mLastMs = null;
		s_mPending = null;
	}
}

//------------------------------------------------------------------------------------------------
//! What the server does with one join-sync ask (LF_MapKnownSync.Admit).
enum LF_EMapKnownSyncGate
{
	SEND,
	DEFER,
	DROP
}

//------------------------------------------------------------------------------------------------
//! The discovery channel on the player's own controller (the client owns it): the visit ask, the
//! server's answer, and the join sync of the character's whole known set.
modded class SCR_PlayerController
{
	//! Bound on a client-sent id; place and kind ids are short keys.
	protected static const int LF_MAP_ID_MAX = 64; // lf-fixed: payload sanity bound, not a gameplay knob

	//------------------------------------------------------------------------------------------------
	override void OnControlledEntityChanged(IEntity from, IEntity to)
	{
		super.OnControlledEntityChanged(from, to);
		if (!to)
			return;

		// The owner pulls the (maybe new) character's set on join, respawn and character switch, from
		// its own controller, so the answer never races the controller's ownership; then looks around.
		if (GetGame().GetPlayerController() != this)
			return;

		if (Replication.IsServer())
			RpcAsk_LF_MapKnownSync();
		else
			Rpc(RpcAsk_LF_MapKnownSync);

		LF_MapKnownScan.Start();
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_MapKnownSync()
	{
		int playerId = GetPlayerId();
		LF_EMapKnownSyncGate gate = LF_MapKnownSync.Admit(playerId, System.GetTickCount());
		if (gate == LF_EMapKnownSyncGate.DEFER)
		{
			GetGame().GetCallqueue().CallLater(LF_MapKnownSync.Deferred, LF_MapKnownSync.MIN_GAP_MS, false, playerId);
			LF_Debug.Info(LF_MapKnown.FEATURE, string.Format("join sync for player %1 deferred %2 ms", playerId, LF_MapKnownSync.MIN_GAP_MS));
			return;
		}

		if (gate == LF_EMapKnownSyncGate.DROP)
		{
			LF_Debug.Info(LF_MapKnown.FEATURE, string.Format("join sync for player %1 already queued", playerId));
			return;
		}

		LF_SendMapKnownAll(LF_MapKnownSync.RETRIES);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: ask the server to record a visit.
	void LF_AskDiscover(string kind, string placeId)
	{
		if (Replication.IsServer())
			RpcAsk_LF_MapDiscover(kind, placeId);
		else
			Rpc(RpcAsk_LF_MapDiscover, kind, placeId);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: one changed place to this player's map.
	void LF_SendMapKnown(string placeId, int state)
	{
		if (GetGame().GetPlayerController() == this)
			RpcDo_LF_MapKnown(placeId, state);
		else
			Rpc(RpcDo_LF_MapKnown, placeId, state);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the current character's whole known set to this player (join, respawn, character switch).
	//! \param[in] retries Tries left while the character key is not known yet.
	void LF_SendMapKnownAll(int retries = 0)
	{
		string characterId = LF_Utils.GetPlayerUID(GetPlayerId());
		if (characterId.IsEmpty())
		{
			if (retries > 0)
			{
				GetGame().GetCallqueue().CallLater(LF_MapKnownSync.Retry, LF_MapKnownSync.RETRY_MS, false, GetPlayerId(), retries - 1);
				return;
			}

			LF_Debug.Warn(LF_MapKnown.FEATURE, string.Format("join sync skipped: player %1 has no character key", GetPlayerId()));
			return;
		}

		array<string> placeIds = {};
		array<int> states = {};
		LF_MapKnown.ExportCharacter(characterId, placeIds, states);
		LF_Debug.Info(LF_MapKnown.FEATURE, string.Format("join sync: %1 known place(s) to player %2", placeIds.Count(), GetPlayerId()));
		if (GetGame().GetPlayerController() == this)
			RpcDo_LF_MapKnownSync(placeIds, states);
		else
			Rpc(RpcDo_LF_MapKnownSync, placeIds, states);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the client says it stands at a place. Checked here: the kind is a DISCOVER POI, the
	//! place is live, the server's own view of the character is in range, and the rate window.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_MapDiscover(string kind, string placeId)
	{
		int playerId = GetPlayerId();
		if (kind.IsEmpty() || placeId.IsEmpty() || kind.Length() > LF_MAP_ID_MAX || placeId.Length() > LF_MAP_ID_MAX)
		{
			LF_Debug.Warn(LF_MapKnown.FEATURE, string.Format("visit refused: malformed ask from player %1", playerId));
			return;
		}

		LF_MapPoiConfig config = LF_MapPoiConfig.Get();
		LF_MapPoiKind row;
		if (config)
			row = config.Find(kind);

		if (!row || row.m_eVisibility != LF_EMapPoiVisibility.DISCOVER)
		{
			LF_Debug.Warn(LF_MapKnown.FEATURE, string.Format("visit refused: player %1 asked for %2 of kind '%3', not a discover kind", playerId, placeId, kind));
			return;
		}

		IEntity character = GetControlledEntity();
		if (!character)
		{
			LF_Debug.Warn(LF_MapKnown.FEATURE, string.Format("visit refused: player %1 has no character", playerId));
			return;
		}

		vector origin;
		bool live = LF_Places.FindOrigin(kind, placeId, origin);
		float distance = 0;
		if (live)
			distance = vector.Distance(character.GetOrigin(), origin);

		string characterId = LF_Utils.GetPlayerUID(playerId);
		LF_EMapKnownOutcome outcome = LF_MapKnown.VisitAsk(playerId, characterId, placeId, live, distance, 0, System.GetTickCount());
		if (outcome == LF_EMapKnownOutcome.VISITED || outcome == LF_EMapKnownOutcome.KEPT)
			LF_SendMapKnown(placeId, LF_MapKnownRules.SavedState(LF_EMapPoiKnown.VISITED));
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_LF_MapKnown(string placeId, int state)
	{
		LF_MapKnown.SetLocal(placeId, state);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_LF_MapKnownSync(array<string> placeIds, array<int> states)
	{
		LF_MapKnown.SetLocalAll(placeIds, states);
		LF_MapKnownScan.ClearAsked();
		int count = 0;
		if (placeIds)
			count = placeIds.Count();

		LF_Debug.Info(LF_MapKnown.FEATURE, string.Format("join sync: this map knows %1 place(s)", count));
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of the client scan (registered at script load; Core/LF_StaticReset.c).
class LF_MapKnownScan_StaticResetHook : LF_StaticResetHook
{
	protected static bool s_bRegistered = LF_StaticReset.Register(new LF_MapKnownScan_StaticResetHook());

	override string Owner()
	{
		return "LF_MapKnownScan";
	}

	override void Apply()
	{
		LF_MapKnownScan.ResetStatics();
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of the server's join-sync gate (registered at script load; Core/LF_StaticReset.c).
class LF_MapKnownSync_StaticResetHook : LF_StaticResetHook
{
	protected static bool s_bRegistered = LF_StaticReset.Register(new LF_MapKnownSync_StaticResetHook());

	override string Owner()
	{
		return "LF_MapKnownSync";
	}

	override void Apply()
	{
		LF_MapKnownSync.ResetStatics();
	}
}
