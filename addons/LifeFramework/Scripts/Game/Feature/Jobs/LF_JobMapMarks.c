//------------------------------------------------------------------------------------------------
//! One mark this feature owns: the share row of one player and the waypoint sent with it.
class LF_JobMapMark : Managed
{
	//! The share key: "taxi:<fareId>" (driver), "taxi:<fareId>:p" (passenger), "job:<uid>:<stopId>".
	string m_sKey;
	//! The fare id, or "" for a job stop.
	string m_sFareId;
	//! The character the row and the waypoint are for.
	string m_sUid;
	//! The LF_Waypoint id on that player's client.
	string m_sWaypointId;
	//! A job stop made while its runner had a run: it goes when that run ends, however it ends.
	bool m_bRunBound;
	//! What the waypoint said and where, kept so a respawned player gets it again (ResendTo).
	string m_sText;
	int m_iX;
	int m_iZ;
}

//------------------------------------------------------------------------------------------------
//! Where the waypoint goes. The game sink is an owner RPC on the player's controller
//! (modded LF_MapShareComponent below); tests install a recording sink.
class LF_JobWaypointSink : Managed
{
	void Mark(string uid, string id, string label, int x, int z) {}
	void Clear(string uid, string id) {}
}

//------------------------------------------------------------------------------------------------
class LF_JobWaypointRplSink : LF_JobWaypointSink
{
	//------------------------------------------------------------------------------------------------
	override void Mark(string uid, string id, string label, int x, int z)
	{
		LF_MapShareComponent channel = Channel(uid);
		if (channel)
			channel.ServerJobWaypoint(id, label, x, z);
	}

	//------------------------------------------------------------------------------------------------
	override void Clear(string uid, string id)
	{
		LF_MapShareComponent channel = Channel(uid);
		if (channel)
			channel.ServerJobWaypointClear(id);
	}

	//------------------------------------------------------------------------------------------------
	//! The channel of the player who plays this character; null when they are not on the server.
	protected LF_MapShareComponent Channel(string uid)
	{
		if (!GetGame() || !GetGame().GetPlayerManager())
			return null;

		LF_MapShareGameFacts facts = new LF_MapShareGameFacts();
		array<int> players = {};
		facts.Players(players);
		foreach (int playerId : players)
		{
			LF_MapShareMember member = facts.Of(playerId);
			if (member && member.m_sUid == uid)
				return LF_MapShareComponent.OfPlayer(playerId);
		}

		return null;
	}
}

//------------------------------------------------------------------------------------------------
//! Taxi fare and job stop marks (card M11, #2239; everon-map.md section 3 "Shared marks"). The accepted
//! fare's destination goes to the driver and the passenger, and a delivery run's current stop goes to
//! its runner: each is a PLAYER shared mark (LF_MapShares) plus the waypoint on that client
//! (LF_Waypoint, the one GPS-like aid, D-M9). The job owns the marks: finishing the fare or the run
//! (LF_OnJobCompleted), abandoning the run, or the safety expiry removes both the row and the waypoint.
//! Server only; nothing is saved, a restart clears runs and marks alike (D-M11).
//! The fare and stop systems call OnFareAccepted/OnFareDone and OnStop/OnStopDone. Callers (follow-up to #2239):
//! starting a delivery run puts its stop on the map (OnRunStarted: the nearest drop, a DROP
//! LF_JobSiteComponent registered in LF_Places); LF_CivilianJob.AbandonRun clears it at once (a run
//! ended any other way is swept); completing a run clears it by LF_OnJobCompleted; a respawn resends
//! the waypoint (ResendTo). A run has the one stop today; a route with several calls OnStop for each
//! next one and the new stop replaces the last. Taxi runs get no stop mark: no fare system yet (M6).
class LF_JobMapMarks
{
	static const string FEATURE = "Jobs";
	static const string KEY_TAXI = "taxi:";
	static const string KEY_JOB = "job:";
	//! LF_Places kind of a drop-off (an LF_JobSiteComponent of site DROP); the id is the entity name.
	static const string PLACE_DROP = "JobDrop";
	static const string WAYPOINT_FARE = "taxi-fare";
	static const string WAYPOINT_STOP = "job-stop";
	static const string TEXT_FARE = "#LF-JobMap_Fare";
	static const string TEXT_STOP = "#LF-JobMap_Stop";
	//! Indexes into the vanilla placed-marker config (Configs/Map/MapMarkerConfig.conf): icon 0 is the
	//! first general icon; colour 2 dark orange (taxi), 5 green (job stop).
	static const int ICON = 0; // lf-fixed: vanilla config index, not a server tunable
	static const int COLOUR_FARE = 2; // lf-fixed: vanilla config index
	static const int COLOUR_STOP = 5; // lf-fixed: vanilla config index
	//! A mark whose owner never finishes (left the server) goes after this long.
	static const float EXPIRY_SECONDS = 3600; // lf-fixed: safety net only; finishing the job removes the mark
	static const int SWEEP_MS = 5000; // lf-fixed: how soon an abandoned run's mark goes

	protected static ref array<ref LF_JobMapMark> s_aMarks;
	protected static ref LF_JobWaypointSink s_Sink;
	protected static bool s_bSweeping;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		LF_JobMapMarkSubscriber.ResetStatics();
		if (s_bSweeping && GetGame())
			GetGame().GetCallqueue().Remove(LF_JobMapMarks.Sweep);

		s_bSweeping = false;
		s_aMarks = null;
		s_Sink = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Tests: replace where waypoints go. Null restores the game's.
	static void SetSink(LF_JobWaypointSink sink)
	{
		s_Sink = sink;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: listen to finished runs and sweep abandoned ones (idempotent).
	static void Install()
	{
		if (!Replication.IsServer())
			return;

		LF_JobMapMarkSubscriber.Install();
		if (!s_bSweeping && GetGame())
		{
			GetGame().GetCallqueue().CallLater(LF_JobMapMarks.Sweep, SWEEP_MS, true);
			s_bSweeping = true;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: a run-bound stop whose runner no longer has a run is stale.
	static bool IsStale(bool runBound, bool runActive)
	{
		return runBound && !runActive;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: index of the position nearest `from` (ground distance), or -1 for none.
	static int NearestIndex(notnull array<vector> positions, vector from)
	{
		int best = -1;
		float bestDistance;
		foreach (int i, vector position : positions)
		{
			float distance = vector.DistanceSq(Vector(position[0], 0, position[2]), Vector(from[0], 0, from[2]));
			if (best == -1 || distance < bestDistance)
			{
				best = i;
				bestDistance = distance;
			}
		}

		return best;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a delivery run started with its cargo in hand. Its stop is the nearest placed drop-off.
	//! \return The share key, or "" when the run gets no mark (a taxi run, or no drop placed).
	static string OnRunStarted(string runnerUid, LF_EJobType job, vector from)
	{
		array<IEntity> drops = {};
		LF_Places.FindAll(PLACE_DROP, drops);
		array<string> ids = {};
		array<vector> positions = {};
		foreach (IEntity drop : drops)
		{
			ids.Insert(drop.GetName());
			positions.Insert(drop.GetOrigin());
		}

		return StopForRun(runnerUid, job, from, ids, positions);
	}

	//------------------------------------------------------------------------------------------------
	//! The run's stop from the drops that exist: ids[i] stands at positions[i].
	static string StopForRun(string runnerUid, LF_EJobType job, vector from, notnull array<string> ids, notnull array<vector> positions)
	{
		if (job == LF_EJobType.TAXI)
			return "";

		int nearest = NearestIndex(positions, from);
		if (nearest < 0 || nearest >= ids.Count())
		{
			LF_Debug.Warn(FEATURE, string.Format("no stop map mark for %1: no drop-off is placed", runnerUid));
			return "";
		}

		return OnStop(runnerUid, ids[nearest], positions[nearest]);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a player spawned (late join or respawn): put their waypoints on the new client again.
	//! The map rows come back through LF_MapShares.OnSpawned; the waypoint lives only on the client.
	//! \return How many waypoints were sent.
	static int ResendTo(string uid)
	{
		if (!s_aMarks || uid.IsEmpty())
			return 0;

		Prune();
		int sent;
		foreach (LF_JobMapMark mark : s_aMarks)
		{
			if (mark.m_sUid != uid)
				continue;

			Sink().Mark(uid, mark.m_sWaypointId, mark.m_sText, mark.m_iX, mark.m_iZ);
			sent++;
		}

		if (sent > 0)
			LF_Debug.Info(FEATURE, string.Format("%1 spawned: %2 waypoint(s) sent again", uid, sent));

		return sent;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a taxi fare was accepted. The destination becomes a mark and a waypoint for the driver
	//! and, when there is one, the passenger. \return How many players got it (0 to 2).
	static int OnFareAccepted(string fareId, string driverUid, string passengerUid, vector destination)
	{
		if (fareId.IsEmpty() || driverUid.IsEmpty())
		{
			LF_Debug.Warn(FEATURE, "no fare map mark: the fare has no id or no driver");
			return 0;
		}

		if (destination == vector.Zero)
		{
			LF_Debug.Warn(FEATURE, string.Format("no fare map mark for fare %1: the destination has no position", fareId));
			return 0;
		}

		OnFareDone(fareId);
		int sent;
		if (Give(KEY_TAXI + fareId, fareId, driverUid, WAYPOINT_FARE, TEXT_FARE, COLOUR_FARE, destination, false))
			sent++;

		if (!passengerUid.IsEmpty() && passengerUid != driverUid && Give(KEY_TAXI + fareId + ":p", fareId, passengerUid, WAYPOINT_FARE, TEXT_FARE, COLOUR_FARE, destination, false))
			sent++;

		return sent;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the fare ended (dropped off or cancelled). \return How many players lost the mark.
	static int OnFareDone(string fareId)
	{
		if (!s_aMarks || fareId.IsEmpty())
			return 0;

		return RemoveWhere(fareId, string.Empty, true);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a delivery run's next stop. One stop per runner: a new stop replaces the last.
	//! \return The share key, or "" when the stop gets no mark.
	static string OnStop(string runnerUid, string stopId, vector position)
	{
		if (runnerUid.IsEmpty() || stopId.IsEmpty())
		{
			LF_Debug.Warn(FEATURE, "no stop map mark: the stop has no runner or no id");
			return "";
		}

		if (position == vector.Zero)
		{
			LF_Debug.Warn(FEATURE, string.Format("no stop map mark for '%1': the stop has no position", stopId));
			return "";
		}

		OnStopDone(runnerUid);
		string key = KEY_JOB + runnerUid + ":" + stopId;
		if (!Give(key, string.Empty, runnerUid, WAYPOINT_STOP, TEXT_STOP, COLOUR_STOP, position, LF_CivilianJob.HasActiveRun(runnerUid)))
			return "";

		return key;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the runner's stop is done. \return How many marks went (0 or 1).
	static int OnStopDone(string runnerUid)
	{
		if (!s_aMarks || runnerUid.IsEmpty())
			return 0;

		return RemoveWhere(string.Empty, runnerUid, false);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a run was completed and paid. The runner's stop goes, and so does the fare they drive.
	static int OnJobCompleted(notnull LF_OnJobCompleted done)
	{
		if (!s_aMarks || done.m_sPlayerUid.IsEmpty())
			return 0;

		int closed = OnStopDone(done.m_sPlayerUid);
		array<string> fares = {};
		foreach (LF_JobMapMark mark : s_aMarks)
		{
			if (!mark.m_sFareId.IsEmpty() && mark.m_sKey == KEY_TAXI + mark.m_sFareId && mark.m_sUid == done.m_sPlayerUid)
				fares.Insert(mark.m_sFareId);
		}

		foreach (string fareId : fares)
		{
			closed += OnFareDone(fareId);
		}

		return closed;
	}

	//------------------------------------------------------------------------------------------------
	//! Marks the server still tracks (an expired or abandoned one leaves at the next sweep or call).
	static int Count()
	{
		Prune();
		if (!s_aMarks)
			return 0;

		return s_aMarks.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Is this key one of this feature's marks?
	static bool Has(string key)
	{
		if (!s_aMarks)
			return false;

		foreach (LF_JobMapMark mark : s_aMarks)
		{
			if (mark.m_sKey == key)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Drops marks whose run ended without a completion (abandoned) or whose row expired.
	static void Sweep()
	{
		Prune();
	}

	//------------------------------------------------------------------------------------------------
	protected static bool Give(string key, string fareId, string uid, string waypointId, string text, int colour, vector position, bool runBound)
	{
		Prune();
		int x = Math.Round(position[0]);
		int z = Math.Round(position[2]);
		LF_MapShareRow row = LF_MapShareRow.Make(key, LF_EMapShareAudience.PLAYER, uid, x, z, ICON, colour, text, LF_MapShares.Now() + EXPIRY_SECONDS);
		if (!LF_MapShares.Put(row))
			return false;

		LF_JobMapMark mark = new LF_JobMapMark();
		mark.m_sKey = key;
		mark.m_sFareId = fareId;
		mark.m_sUid = uid;
		mark.m_sWaypointId = waypointId;
		mark.m_bRunBound = runBound;
		mark.m_sText = text;
		mark.m_iX = x;
		mark.m_iZ = z;
		Marks().Insert(mark);
		Sink().Mark(uid, waypointId, text, x, z);
		LF_Debug.Info(FEATURE, string.Format("map mark %1 and waypoint %2 for %3 at %4 %5", key, waypointId, uid, x, z));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Removes a fare's marks (fareId) or a runner's stops (uid, fares excluded). \return How many went.
	protected static int RemoveWhere(string fareId, string uid, bool fare)
	{
		int removed;
		for (int i = s_aMarks.Count() - 1; i >= 0; i--)
		{
			LF_JobMapMark mark = s_aMarks[i];
			bool match;
			if (fare)
				match = mark.m_sFareId == fareId;
			else
				match = mark.m_sFareId.IsEmpty() && mark.m_sUid == uid;

			if (!match)
				continue;

			Close(mark);
			s_aMarks.Remove(i);
			removed++;
		}

		return removed;
	}

	//------------------------------------------------------------------------------------------------
	protected static void Close(notnull LF_JobMapMark mark)
	{
		LF_MapShares.Remove(mark.m_sKey);
		Sink().Clear(mark.m_sUid, mark.m_sWaypointId);
		LF_Debug.Info(FEATURE, string.Format("map mark %1 and its waypoint cleared for %2", mark.m_sKey, mark.m_sUid));
	}

	//------------------------------------------------------------------------------------------------
	protected static void Prune()
	{
		if (!s_aMarks)
			return;

		for (int i = s_aMarks.Count() - 1; i >= 0; i--)
		{
			LF_JobMapMark mark = s_aMarks[i];
			if (LF_MapShares.Has(mark.m_sKey) && !IsStale(mark.m_bRunBound, LF_CivilianJob.HasActiveRun(mark.m_sUid)))
				continue;

			Close(mark);
			s_aMarks.Remove(i);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static array<ref LF_JobMapMark> Marks()
	{
		if (!s_aMarks)
			s_aMarks = {};

		return s_aMarks;
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_JobWaypointSink Sink()
	{
		if (!s_Sink)
			s_Sink = new LF_JobWaypointRplSink();

		return s_Sink;
	}
}

//------------------------------------------------------------------------------------------------
//! Bus subscriber: completed runs clear their marks, a spawn resends the player's waypoints.
class LF_JobMapMarkSubscriber : LF_EventSubscriber
{
	protected static ref LF_JobMapMarkSubscriber s_Subscriber;
	static const int RETRY_MS = 2000; // lf-fixed: one retry for an account that has no character at the spawn event

	//------------------------------------------------------------------------------------------------
	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnJobCompleted done = LF_OnJobCompleted.Cast(busEvent);
		if (done)
		{
			LF_JobMapMarks.OnJobCompleted(done);
			return true;
		}

		LF_OnPlayerSpawnedEvent spawned = LF_OnPlayerSpawnedEvent.Cast(busEvent);
		if (!spawned)
			return false;

		if (!ResendFor(spawned.m_iPlayerId) && GetGame())
			GetGame().GetCallqueue().CallLater(LF_JobMapMarkSubscriber.RetryResend, RETRY_MS, false, spawned.m_iPlayerId);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The account may not have its character yet when the spawn event fires: ask once more.
	protected static void RetryResend(int playerId)
	{
		ResendFor(playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! \return false when the player's character is not known yet.
	protected static bool ResendFor(int playerId)
	{
		LF_MapShareGameFacts facts = new LF_MapShareGameFacts();
		LF_MapShareMember member = facts.Of(playerId);
		if (!member)
			return false;

		LF_JobMapMarks.ResendTo(member.m_sUid);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	static void Install()
	{
		ResetStatics();
		s_Subscriber = new LF_JobMapMarkSubscriber();
		LF_EventBus.Subscribe(LF_OnJobCompleted, s_Subscriber);
		LF_EventBus.Subscribe(LF_OnPlayerSpawnedEvent, s_Subscriber);
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		if (s_Subscriber)
		{
			LF_EventBus.Unsubscribe(LF_OnJobCompleted, s_Subscriber);
			LF_EventBus.Unsubscribe(LF_OnPlayerSpawnedEvent, s_Subscriber);
		}

		s_Subscriber = null;
	}
}

//------------------------------------------------------------------------------------------------
//! The waypoint leg of the owner channel: the server sets or clears the waypoint on one client.
modded class LF_MapShareComponent
{
	//------------------------------------------------------------------------------------------------
	//! Server: put a waypoint on this player's client.
	void ServerJobWaypoint(string id, string label, int x, int z)
	{
		if (!Replication.IsServer())
			return;

		if (IsLocalOwner())
			RpcDo_JobWaypoint(id, label, x, z);
		else
			Rpc(RpcDo_JobWaypoint, id, label, x, z);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: take that waypoint off this player's client.
	void ServerJobWaypointClear(string id)
	{
		if (!Replication.IsServer())
			return;

		if (IsLocalOwner())
			RpcDo_JobWaypointClear(id);
		else
			Rpc(RpcDo_JobWaypointClear, id);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_JobWaypoint(string id, string label, int x, int z)
	{
		float y;
		BaseWorld world = GetGame().GetWorld();
		if (world)
			y = world.GetSurfaceY(x, z);

		// Mark on the same id would take it away: a new stop replaces the last.
		if (LF_Waypoint.IsMarked(id))
			LF_Waypoint.Clear("replaced");

		LF_Waypoint.Mark(id, label, Vector(x, y, z));
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_JobWaypointClear(string id)
	{
		if (LF_Waypoint.IsMarked(id))
			LF_Waypoint.Clear("job done");
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
		LF_JobMapMarks.Install();
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_JobMapMarks' statics (registered at script load; Core/LF_StaticReset.c).
class LF_JobMapMarks_StaticResetHook : LF_StaticResetHook
{
	protected static bool s_bRegistered = LF_StaticReset.Register(new LF_JobMapMarks_StaticResetHook());

	override string Owner()
	{
		return "LF_JobMapMarks";
	}

	override void Apply()
	{
		LF_JobMapMarks.ResetStatics();
	}
}
