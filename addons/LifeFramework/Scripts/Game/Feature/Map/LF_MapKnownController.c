// Discovery on the wire (card M13): the server's join sync of a character's known set and each later change.
// The look-around that finds visits runs on the server (LF_MapDiscoverScan, #2492): a client never knows
// where a place it has not discovered is.

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
	//------------------------------------------------------------------------------------------------
	override void OnControlledEntityChanged(IEntity from, IEntity to)
	{
		super.OnControlledEntityChanged(from, to);
		if (!to)
			return;

		// The owner pulls the (maybe new) character's set on join, respawn and character switch, from
		// its own controller, so the answer never races the controller's ownership.
		if (GetGame().GetPlayerController() != this)
			return;

		if (Replication.IsServer())
			RpcAsk_LF_MapKnownSync();
		else
			Rpc(RpcAsk_LF_MapKnownSync);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_MapKnownSync()
	{
		// Discovery is the server's own look-around (#2492): the client never knows where an unvisited place is.
		LF_MapDiscoverScan.Start();
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
	//! Server: one changed place to this player's map, then (unless the caller batches) the private places
	//! the player may now see, since a heard-of or visited place may be one of them.
	void LF_SendMapKnown(string placeId, int state, bool withPlaces = true)
	{
		if (GetGame().GetPlayerController() == this)
			RpcDo_LF_MapKnown(placeId, state);
		else
			Rpc(RpcDo_LF_MapKnown, placeId, state);

		if (withPlaces)
			LF_MapPrivatePlaces.SendTo(this);
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

		// The join sync is also the late joiner's private places: only this player's set, to this owner.
		LF_MapPrivatePlaces.SendTo(this);
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
		int count = 0;
		if (placeIds)
			count = placeIds.Count();

		LF_Debug.Info(LF_MapKnown.FEATURE, string.Format("join sync: this map knows %1 place(s)", count));
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
