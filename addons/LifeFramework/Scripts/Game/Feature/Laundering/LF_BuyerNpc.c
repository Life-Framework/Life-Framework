// LF_BuyerNpc.c - the illicit buyer stands at today's open spot (#2919, M7 beat 2 of #1025). Server side.
// A sweep every few seconds asks LF_BuyerRotation which `chain_buyer` spot is open now. When a player is
// within the spawn radius of that spot the buyer (a man in a work jacket, `LF_IllicitBuyer`) is spawned
// there and loiters; when the rotation moves, he leaves the old spot (is removed) and appears at the new
// one the same way; with no player near his spot he is removed, like a resident (LF_ResidentSpawner).
// One buyer at most, held by EntityID. The character is an ordinary replicated entity, so a late joiner
// sees him with no extra state. Nothing is saved: the spot is a function of unix time and the seed.

//------------------------------------------------------------------------------------------------
class LF_BuyerNpc
{
	static const string FEATURE = "Laundering";
	//! The neutral core buyer; a pack dresses him with its own prefab (B4).
	static const ResourceName PREFAB = "{3AF0C7C21AB03542}Prefabs/Characters/LF_IllicitBuyer.et";
	static const int SWEEP_SECONDS = 5; // lf-fixed: a housekeeping tick, not tuning
	//! The same distances residents use: he appears within 150 m of a player and goes past 200 m.
	static const float SPAWN_RADIUS_M = 150; // lf-fixed: matches the resident spawner's default
	static const float DESPAWN_RADIUS_M = 200; // lf-fixed: matches the resident spawner's default

	protected static EntityID s_Live = EntityID.INVALID;
	protected static string s_sSpot;
	protected static bool s_bScheduled;
	protected static int s_iSpawned;
	protected static int s_iDespawned;

	//------------------------------------------------------------------------------------------------
	//! Server: starts the sweep. Idempotent. A client or a world with no call queue does nothing.
	static void Start()
	{
		if (s_bScheduled || !Replication.IsServer() || !GetGame() || !GetGame().GetCallqueue())
			return;

		s_bScheduled = true;
		GetGame().GetCallqueue().CallLater(Tick, SWEEP_SECONDS * 1000, true);
		LF_Debug.Info(FEATURE, string.Format("illicit buyer sweeps every %1 s", SWEEP_SECONDS));
	}

	//------------------------------------------------------------------------------------------------
	protected static void Tick()
	{
		SweepAt(System.GetUnixTime());
	}

	//------------------------------------------------------------------------------------------------
	//! Server: one pass at unix time `unixNow`. Removes the buyer when he stands at a spot that is no longer
	//! open, is gone from the world, or has no player within the despawn radius; then spawns him at the open
	//! spot when a player is within the spawn radius of it.
	//! \param[in] players Where the players are; null reads the live players.
	//! \return How many times he was spawned or removed.
	static int SweepAt(int unixNow, LF_ResidentPlayers players = null)
	{
		if (!Replication.IsServer())
			return 0;

		ref LF_ResidentPlayers source = players;
		if (!source)
			source = new LF_ResidentPlayers();

		array<vector> where = {};
		source.Positions(where);

		string open = LF_BuyerRotation.OpenSpot(unixNow);
		IEntity spot;
		if (!open.IsEmpty())
			spot = LF_Places.Find(LF_BuyerRotation.PLACE_KIND, open);

		int changes;
		IEntity live = GetLive();
		if (live)
		{
			string why = "";
			if (!spot || open != s_sSpot)
				why = "the open spot moved on";
			else if (!AnyWithin(where, live.GetOrigin(), DESPAWN_RADIUS_M * DESPAWN_RADIUS_M))
				why = "no player within the despawn radius";

			if (!why.IsEmpty())
			{
				LF_Debug.Info(FEATURE, string.Format("illicit buyer left spot '%1': %2", s_sSpot, why));
				Remove(live);
				changes++;
				live = null;
			}
		}
		else if (s_Live != EntityID.INVALID)
		{
			LF_Debug.Info(FEATURE, "illicit buyer is gone from the world; forgotten");
			s_Live = EntityID.INVALID;
			s_sSpot = "";
		}

		if (live || !spot || !AnyWithin(where, spot.GetOrigin(), SPAWN_RADIUS_M * SPAWN_RADIUS_M))
			return changes;

		if (Spawn(open, spot))
			changes++;

		return changes;
	}

	//------------------------------------------------------------------------------------------------
	//! The live buyer entity, or null when none stands (or the character is gone).
	static IEntity GetLive()
	{
		if (s_Live == EntityID.INVALID || !GetGame() || !GetGame().GetWorld())
			return null;

		return GetGame().GetWorld().FindEntityByID(s_Live);
	}

	//------------------------------------------------------------------------------------------------
	//! The id of the spot the live buyer stands at, or "" when none.
	static string LiveSpot()
	{
		if (!GetLive())
			return "";

		return s_sSpot;
	}

	//------------------------------------------------------------------------------------------------
	//! Buyers spawned and removed since world start (tests read them).
	static int SpawnedTotal()
	{
		return s_iSpawned;
	}

	static int DespawnedTotal()
	{
		return s_iDespawned;
	}

	//------------------------------------------------------------------------------------------------
	//! Tests: deletes the live buyer and forgets him.
	static void DespawnAll()
	{
		IEntity live = GetLive();
		if (live)
			SCR_EntityHelper.DeleteEntityAndChildren(live);

		s_Live = EntityID.INVALID;
		s_sSpot = "";
	}

	//------------------------------------------------------------------------------------------------
	//! World end (and tests): stops the sweep and forgets the buyer. The character goes with the world.
	static void ResetStatics()
	{
		if (s_bScheduled && GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(Tick);

		s_bScheduled = false;
		s_Live = EntityID.INVALID;
		s_sSpot = "";
		s_iSpawned = 0;
		s_iDespawned = 0;
	}

	//------------------------------------------------------------------------------------------------
	protected static void Remove(notnull IEntity live)
	{
		LF_NpcLoiter.Release(live, "buyer left");
		SCR_EntityHelper.DeleteEntityAndChildren(live);
		s_Live = EntityID.INVALID;
		s_sSpot = "";
		s_iDespawned++;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool Spawn(string id, notnull IEntity spot)
	{
		vector mat[4];
		spot.GetWorldTransform(mat);
		vector angles = Math3D.MatrixToAngles(mat);
		vector at = LF_NpcSpots.Free(spot.GetOrigin(), angles[0], "illicit buyer " + id);
		IEntity entity = LF_Utils.SpawnEntityPrefab(PREFAB, at, angles, true);
		if (!entity)
		{
			LF_Debug.Error(FEATURE, string.Format("illicit buyer could not be spawned from %1", PREFAB));
			return false;
		}

		// Session state: the buyer is spawned again at each boot, so a saved copy would stand beside him.
		LF_WorldSaveHold.Hold(entity);
		LF_NpcSpots.Claim(entity);

		// He stands where he is put: any AI on the character is switched off (no pathing), then he loiters.
		AIControlComponent control = AIControlComponent.Cast(entity.FindComponent(AIControlComponent));
		if (control && control.IsAIActivated())
			control.DeactivateAI();

		LF_NpcLoiter.Settle(entity);
		s_Live = entity.GetID();
		s_sSpot = id;
		s_iSpawned++;
		LF_Debug.Info(FEATURE, string.Format("illicit buyer appeared at spot '%1'", id));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool AnyWithin(notnull array<vector> where, vector at, float radiusSq)
	{
		foreach (vector player : where)
		{
			float dx = player[0] - at[0];
			float dz = player[2] - at[2];
			if (dx * dx + dz * dz <= radiusSq)
				return true;
		}

		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of the buyer statics (LF_StaticReset finds it by reflection).
class LF_BuyerNpc_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_BuyerNpc";
	}

	override void Apply()
	{
		LF_BuyerNpc.ResetStatics();
	}
}
