// LF_ResidentSpawner.c - residents appear on their anchors when a player is near, and leave when
// nobody is (#2858, epic #1012 beat 1). Server side. A sweep every few seconds asks the timetable
// (#2665) where each resident should be now, resolves that to an anchor (#2857) and spawns the
// resident's character there when a player is within the spawn radius; a live resident whose line
// moved on, or with no player within the despawn radius, is removed. A hard cap bounds how many are
// live; the director (#2910) picks which due residents take the free slots. Residents stand and are logged with their activity; nobody walks in this wave. The spawned
// character is an ordinary replicated entity, so a late joiner sees it with no extra state.
// Entries hold EntityID, never IEntity (rule: entities vanish at any time).

//------------------------------------------------------------------------------------------------
//! Where the players are. The default reads the controlled entity of every connected player; a test
//! overrides Positions to stand a player anywhere.
class LF_ResidentPlayers
{
	//! Appends the world position of each player. \return how many.
	int Positions(notnull array<vector> into)
	{
		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!playerManager)
			return 0;

		array<int> ids = {};
		playerManager.GetPlayers(ids);
		int found;
		foreach (int id : ids)
		{
			IEntity controlled = playerManager.GetPlayerControlledEntity(id);
			if (!controlled)
				continue;

			into.Insert(controlled.GetOrigin());
			found++;
		}

		return found;
	}
}

//------------------------------------------------------------------------------------------------
//! One live resident: the character, and the timetable line it stands for.
class LF_ResidentLive
{
	string m_sId;
	EntityID m_Entity;
	string m_sRole;
	string m_sAnchor;
	string m_sActivity;
}

//------------------------------------------------------------------------------------------------
class LF_ResidentSpawner
{
	static const string FEATURE = "Residents";
	//! The neutral civilian a resident with no prefab of their own spawns as.
	static const ResourceName DEFAULT_PREFAB = "{D9D016A65F3245D7}Prefabs/Characters/Residents/Resident_Civilian.et";
	static const int DEFAULT_SWEEP_SECONDS = 5;	// lf-fixed: used only when the config does not resolve

	//! The floor search starts this far above the spot (a table top is not reached from a floor anchor) ...
	static const float FLOOR_UP_M = 0.5; // lf-fixed: a step up, not a counter
	//! ... and looks this far below it (an anchor written a little high).
	static const float FLOOR_DOWN_M = 1.5; // lf-fixed: a typed anchor is never further off the ground than a body is tall

	protected static ref map<string, ref LF_ResidentLive> s_mLive;
	protected static ref set<string> s_mCapped;
	protected static bool s_bScheduled;
	protected static int s_iSpawned;
	protected static int s_iDespawned;
	protected static int s_iCapWarns;

	//------------------------------------------------------------------------------------------------
	//! Server: starts the sweep. Idempotent. A client or a world with no call queue does nothing.
	static void Start()
	{
		if (s_bScheduled || !Replication.IsServer() || !GetGame() || !GetGame().GetCallqueue())
			return;

		int seconds = DEFAULT_SWEEP_SECONDS;
		LF_ResidentsConfig config = LF_ResidentsConfig.Get();
		if (config)
			seconds = config.SweepSeconds();

		s_bScheduled = true;
		GetGame().GetCallqueue().CallLater(Tick, seconds * 1000, true);
		LF_Debug.Info(FEATURE, string.Format("resident spawner sweeps every %1 s", seconds));
	}

	//------------------------------------------------------------------------------------------------
	protected static void Tick()
	{
		SweepAt(LF_ResidentTimetable.NowMinute());
	}

	//------------------------------------------------------------------------------------------------
	//! Server: one pass at game minute `minute`. Despawns first (so freed slots count against the cap),
	//! then spawns the due residents a player is near that `LF_ResidentDirector` picks for the free slots
	//! (nearest a player first, a counter worker beside one before a bench sitter, ties by id).
	//! \param[in] players Where the players are; null reads the live players.
	//! \return How many residents were spawned or despawned.
	static int SweepAt(int minute, LF_ResidentPlayers players = null)
	{
		LF_ResidentsConfig config = LF_ResidentsConfig.Get();
		if (!config)
			return 0;

		if (!s_mLive)
			s_mLive = new map<string, ref LF_ResidentLive>();

		ref LF_ResidentPlayers source = players;
		if (!source)
			source = new LF_ResidentPlayers();

		array<vector> where = {};
		source.Positions(where);

		float spawnSq = config.SpawnRadius() * config.SpawnRadius();
		float despawnSq = config.DespawnRadius() * config.DespawnRadius();
		int changes = Despawn(minute, where, despawnSq);

		if (where.IsEmpty())
			return changes;

		array<string> ids = {};
		LF_ResidentTimetable.GetIds(ids);
		ids.Sort();
		array<ref LF_ResidentCandidate> due = {};
		foreach (string id : ids)
		{
			if (s_mLive.Contains(id))
				continue;

			LF_ResidentCandidate found = Candidate(id, minute, where, spawnSq);
			if (found)
				due.Insert(found);
		}

		// The director fills the places players are in first (#2910); the rest wait for a free slot.
		array<string> chosen = {};
		LF_ResidentDirector.Pick(due, where, config.MaxLive() - s_mLive.Count(), chosen);
		foreach (LF_ResidentCandidate candidate : due)
		{
			if (chosen.Contains(candidate.m_sId))
			{
				if (Spawn(candidate, config))
					changes++;
			}
			else
			{
				WarnCapped(candidate.m_sId, config);
			}
		}

		return changes;
	}

	//------------------------------------------------------------------------------------------------
	//! The live resident entity, or null when `id` is not live (or the character is gone).
	static IEntity GetLive(string id)
	{
		LF_ResidentLive live;
		if (!s_mLive || !s_mLive.Find(id, live))
			return null;

		return Entity(live);
	}

	//------------------------------------------------------------------------------------------------
	//! The resident a spawned character stands for, or empty when it is not a live resident.
	static string ResidentIdOf(IEntity entity)
	{
		if (!entity || !s_mLive)
			return string.Empty;

		foreach (string id, LF_ResidentLive live : s_mLive)
		{
			if (live.m_Entity == entity.GetID())
				return id;
		}

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	static int LiveCount()
	{
		if (!s_mLive)
			return 0;

		return s_mLive.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Residents spawned and despawned since world start (tests read them).
	static int SpawnedTotal()
	{
		return s_iSpawned;
	}

	static int DespawnedTotal()
	{
		return s_iDespawned;
	}

	//------------------------------------------------------------------------------------------------
	//! How many residents the cap has held back (one log line per waiting resident) since world start.
	static int CapWarnCount()
	{
		return s_iCapWarns;
	}

	//------------------------------------------------------------------------------------------------
	//! World end (and tests): stops the sweep and forgets the live residents. The characters go with the world.
	static void ResetStatics()
	{
		if (s_bScheduled && GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(Tick);

		s_bScheduled = false;
		s_mLive = null;
		s_mCapped = null;
		s_iSpawned = 0;
		s_iDespawned = 0;
		s_iCapWarns = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Tests: deletes every live resident character and forgets them.
	static void DespawnAll()
	{
		if (!s_mLive)
			return;

		foreach (string id, LF_ResidentLive live : s_mLive)
		{
			LF_ResidentWorkLoop.Stop(Entity(live));
			SCR_EntityHelper.DeleteEntityAndChildren(Entity(live));
		}

		s_mLive.Clear();
	}

	//------------------------------------------------------------------------------------------------
	protected static IEntity Entity(LF_ResidentLive live)
	{
		if (!live || !GetGame() || !GetGame().GetWorld())
			return null;

		return GetGame().GetWorld().FindEntityByID(live.m_Entity);
	}

	//------------------------------------------------------------------------------------------------
	//! Removes a live resident that vanished, whose line moved on (other place or anchor), or that no
	//! player is within the despawn radius of. An activity change alone only updates the record.
	protected static int Despawn(int minute, notnull array<vector> where, float despawnSq)
	{
		int removed;
		array<string> drop = {};
		foreach (string id, LF_ResidentLive live : s_mLive)
		{
			IEntity entity = Entity(live);
			if (!entity)
			{
				LF_Debug.Info(FEATURE, string.Format("resident '%1' is gone from the world; forgotten", id));
				drop.Insert(id);
				continue;
			}

			LF_ResidentSlot slot = LF_ResidentTimetable.WhereIs(id, minute);
			bool moved = slot && (slot.m_sRole != live.m_sRole || slot.m_sAnchor != live.m_sAnchor);
			// A resident a player can see walks to the next anchor, or is placed there, instead of popping (#2909).
			if (moved && LF_ResidentWalk.Move(id, entity, slot, where, 0, Math.Sqrt(despawnSq)) != LF_EResidentMove.NONE)
			{
				live.m_sRole = slot.m_sRole;
				live.m_sAnchor = slot.m_sAnchor;
				live.m_sActivity = slot.m_sActivity;
				continue;
			}

			if (!slot || moved)
			{
				LF_Debug.Info(FEATURE, string.Format("resident '%1' despawned: the timetable moved them on", id));
				LF_ResidentWorkLoop.Stop(entity);
				SCR_EntityHelper.DeleteEntityAndChildren(entity);
				drop.Insert(id);
				removed++;
				s_iDespawned++;
				continue;
			}

			live.m_sActivity = slot.m_sActivity;
			if (!AnyWithin(where, entity.GetOrigin(), despawnSq))
			{
				LF_Debug.Info(FEATURE, string.Format("resident '%1' despawned: no player within the despawn radius", id));
				LF_ResidentWorkLoop.Stop(entity);
				SCR_EntityHelper.DeleteEntityAndChildren(entity);
				drop.Insert(id);
				removed++;
				s_iDespawned++;
			}
		}

		foreach (string gone : drop)
			s_mLive.Remove(gone);

		return removed;
	}

	//------------------------------------------------------------------------------------------------
	//! `id` as a candidate to spawn: their current line and the anchor spot, when a player is within the spawn
	//! radius of it; null otherwise.
	protected static LF_ResidentCandidate Candidate(string id, int minute, notnull array<vector> where, float spawnSq)
	{
		LF_ResidentSlot slot = LF_ResidentTimetable.WhereIs(id, minute);
		if (!slot)
			return null;

		// The place of the kind nearest a player, so the resident of a kind with several places appears
		// at the one the player is walking up to.
		LF_ResidentSpot spot = new LF_ResidentSpot();
		bool near;
		foreach (vector player : where)
		{
			if (!LF_ResidentAnchors.ResolveIn(slot.m_sKind, slot.m_sPlaceId, slot.m_sAnchor, player, spot))
				return null;

			if (DistanceSqXZ(player, spot.m_vOrigin) <= spawnSq)
			{
				near = true;
				break;
			}
		}

		if (!near)
			return null;

		return LF_ResidentCandidate.Make(id, slot, spot);
	}

	//------------------------------------------------------------------------------------------------
	//! One log line per resident the cap holds back, until they spawn.
	protected static void WarnCapped(string id, LF_ResidentsConfig config)
	{
		if (!s_mCapped)
			s_mCapped = new set<string>();

		if (s_mCapped.Contains(id))
			return;

		s_mCapped.Insert(id);
		s_iCapWarns++;
		LF_Debug.Warn(FEATURE, string.Format("resident '%1' is due but the live cap of %2 is reached; waiting", id, config.MaxLive()));
	}

	//------------------------------------------------------------------------------------------------
	//! Spawns the character of a chosen candidate on their anchor.
	protected static bool Spawn(notnull LF_ResidentCandidate candidate, LF_ResidentsConfig config)
	{
		string id = candidate.m_sId;
		LF_ResidentSlot slot = candidate.m_Slot;
		LF_ResidentSpot spot = candidate.m_Spot;
		ResourceName prefab = DEFAULT_PREFAB;
		LF_ResidentDef def = DefOf(config, id);
		if (def && !def.m_sPrefab.IsEmpty())
			prefab = def.m_sPrefab;
		else if (def && !def.m_sNeedQuest.IsEmpty())
			prefab = LF_ResidentNeeds.GIVER_PREFAB;

		// Nobody spawns inside another character: the nearest free spot beside the anchor when it is taken (#3831).
		spot = LF_ResidentActivity.FreeSpot(spot, slot.m_sActivity, id);
		// Feet on the floor under the spot, not on the height the anchor was written at (#3885).
		spot = OnFloor(spot, id);
		IEntity entity = LF_Utils.SpawnEntityPrefab(prefab, spot.m_vOrigin, spot.m_vAngles, true);
		if (!entity)
		{
			LF_Debug.Error(FEATURE, string.Format("resident '%1' could not be spawned from %2", id, prefab));
			return false;
		}

		// The spawner places every resident again at each boot; a saved copy stood beside it after a restart.
		LF_WorldSaveHold.Hold(entity);
		LF_NpcSpots.Claim(entity);
		Dormant(entity);
		LF_ResidentNeeds.Attach(entity, def);
		LF_ResidentActivity.Apply(entity, slot.m_sActivity, spot);
		LF_ResidentLive live = new LF_ResidentLive();
		live.m_sId = id;
		live.m_Entity = entity.GetID();
		live.m_sRole = slot.m_sRole;
		live.m_sAnchor = slot.m_sAnchor;
		live.m_sActivity = slot.m_sActivity;
		s_mLive.Set(id, live);
		if (s_mCapped)
		{
			int waiting = s_mCapped.Find(id);
			if (waiting >= 0)
				s_mCapped.Remove(waiting);
		}

		s_iSpawned++;
		string activity = slot.m_sActivity;
		if (activity.IsEmpty())
			activity = "idle";

		LF_Debug.Info(FEATURE, string.Format("resident '%1' appeared at %2 anchor '%3', %4", id, slot.m_sRole, slot.m_sAnchor, activity));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The floor under `at`: the first solid under it (every layer; characters, trees and loose items are not
	//! floor), searched FLOOR_UP_M above to FLOOR_DOWN_M below. Returns false when nothing is there.
	static bool FloorAt(vector at, out float floorY)
	{
		if (!GetGame() || !GetGame().GetWorld())
			return false;

		IEntity hit;
		vector from = at + Vector(0, FLOOR_UP_M, 0);
		float y = LF_PlaceSnap.Down(null, from, FLOOR_UP_M + FLOOR_DOWN_M, -1, hit);
		if (y < -1e8)
			return false;

		floorY = y;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! A copy of `spot` with its height on the floor under it, so a body spawned there stands on it and does
	//! not drop (or hover) from an anchor written at another height. No floor found: the spot comes back and
	//! a line is logged.
	static LF_ResidentSpot OnFloor(notnull LF_ResidentSpot spot, string who)
	{
		float floorY;
		if (!FloorAt(spot.m_vOrigin, floorY))
		{
			LF_Debug.Warn(FEATURE, string.Format("resident '%1': no floor within %2 m of %3; it spawns at the anchor height", who, FLOOR_DOWN_M, spot.m_vOrigin));
			return spot;
		}

		LF_ResidentSpot snapped = new LF_ResidentSpot();
		snapped.m_vOrigin = spot.m_vOrigin;
		snapped.m_vOrigin[1] = floorY;
		snapped.m_vAngles = spot.m_vAngles;
		snapped.m_bAnchored = spot.m_bAnchored;
		return snapped;
	}

	//------------------------------------------------------------------------------------------------
	//! A resident stands where they are put: any AI on the character is switched off (no pathing yet).
	protected static void Dormant(notnull IEntity entity)
	{
		AIControlComponent control = AIControlComponent.Cast(entity.FindComponent(AIControlComponent));
		if (control && control.IsAIActivated())
			control.DeactivateAI();
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_ResidentDef DefOf(LF_ResidentsConfig config, string id)
	{
		if (!config.m_aResidents)
			return null;

		foreach (LF_ResidentDef def : config.m_aResidents)
		{
			if (def && def.m_sId == id)
				return def;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool AnyWithin(notnull array<vector> where, vector at, float radiusSq)
	{
		foreach (vector player : where)
		{
			if (DistanceSqXZ(player, at) <= radiusSq)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Squared horizontal distance: a player on a roof or a hill is as near as one on the ground.
	protected static float DistanceSqXZ(vector a, vector b)
	{
		float dx = a[0] - b[0];
		float dz = a[2] - b[2];
		return dx * dx + dz * dz;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of the spawner statics (LF_StaticReset finds it by reflection).
class LF_ResidentSpawner_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_ResidentSpawner";
	}

	override void Apply()
	{
		LF_ResidentSpawner.ResetStatics();
	}
}
