//------------------------------------------------------------------------------------------------
//! The people of one running scene (#1955): who was spawned, the parked vehicle, and which planned
//! walkers are still waiting for their stagger. Holds EntityIDs, never entities, so a walker deleted by
//! anything else is skipped. Server only.
class LF_EventSceneCrew
{
	string m_sSceneId;
	ref LF_EventScenePlan m_Plan;
	ref array<EntityID> m_aPeople = {};
	EntityID m_VehicleId = EntityID.INVALID;
	//! Index of the next planned walker to spawn.
	int m_iNext;
	//! Seconds since the scene started, as the stagger counts them.
	float m_fElapsed;

	//------------------------------------------------------------------------------------------------
	//! The live entities of the scene (walkers, then the vehicle).
	int GetEntities(notnull array<IEntity> into)
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return 0;

		int found = 0;
		foreach (EntityID id : m_aPeople)
		{
			IEntity person = world.FindEntityByID(id);
			if (!person)
				continue;

			into.Insert(person);
			found++;
		}

		IEntity vehicle = world.FindEntityByID(m_VehicleId);
		if (vehicle)
		{
			into.Insert(vehicle);
			found++;
		}

		return found;
	}

	//------------------------------------------------------------------------------------------------
	//! Every planned walker has been spawned (or failed to).
	bool IsFullySpawned()
	{
		return !m_Plan || m_iNext >= m_Plan.m_aNpcs.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Stagger tick from the call queue: spawns the walkers whose delay has passed.
	void Tick()
	{
		m_fElapsed += LF_EventSceneSpawn.TICK_MS / 1000.0;
		LF_EventSceneSpawn.SpawnDue(this, m_fElapsed);
		if (IsFullySpawned())
			GetGame().GetCallqueue().Remove(Tick);
	}
}

//------------------------------------------------------------------------------------------------
//! The scene operation (#1955, chapter #1015): spawns a plan's walkers by role or outfit at the scene's
//! spawn markers (and a parked vehicle, never driven), and at the scene's end removes them only when
//! no player can see them. The free AI budget comes from the engine's AIWorld limit, so a scene never
//! takes the slots a job NPC needs. One scene runs at a time (LF_EventScenePlanner.Begin); a restart
//! drops it (the static reset below). Walking the route is not done here.
class LF_EventSceneSpawn
{
	static const string FEATURE = "Events";
	//! Stagger tick of a running scene.
	static const int TICK_MS = 500; // lf-fixed: stagger resolution only; the config sets the stagger itself
	//! Beyond this a player does not see a walker, whatever the line of sight says.
	static const float SIGHT_RANGE_M = 400; // lf-fixed: a character is a few pixels past this at the default view distance
	//! Eye height over a player's origin, and the point of a walker the eye looks at.
	static const vector EYE_OFFSET = "0 1.7 0";
	static const vector BODY_OFFSET = "0 1.2 0";
	//! Walkers sharing one spawn marker stand this far apart, sideways.
	static const float SPACING_M = 1.2;
	//! The parked vehicle stands this far to the side of the first spawn marker.
	static const float VEHICLE_SIDE_M = 5;

	protected static ref LF_EventSceneCrew s_Running;

	//------------------------------------------------------------------------------------------------
	//! How many AI the engine can still take: AIWorld's limit of active AIs minus the active ones.
	//! 0 (logged) with no AIWorld, so a world that runs no AI never takes a scene.
	static int FreeAiSlots()
	{
		AIWorld aiWorld = GetGame().GetAIWorld();
		if (!aiWorld)
		{
			LF_Debug.Warn(FEATURE, "no AIWorld in this world; the AI budget is 0");
			return 0;
		}

		return Math.Max(0, aiWorld.GetLimitOfActiveAIs() - aiWorld.GetCurrentNumOfActiveAIs());
	}

	//------------------------------------------------------------------------------------------------
	//! The running scene's crew, or null.
	static LF_EventSceneCrew Running()
	{
		return s_Running;
	}

	//------------------------------------------------------------------------------------------------
	//! Starts a plan: marks it running, spawns the walkers due now and ticks the rest in on their
	//! stagger. \return the crew, or null (logged) when the plan is a skip, another scene runs, or the
	//! scene has no spawn marker placed.
	static LF_EventSceneCrew Start(LF_EventScenePlan plan)
	{
		if (!Replication.IsServer())
			return null;

		if (!plan || !plan.m_bGo)
		{
			LF_Debug.Info(FEATURE, "scene spawn refused: the plan does not go");
			return null;
		}

		array<IEntity> spawns = {};
		if (LF_EventWaypointComponent.FindSpawns(plan.m_sSceneId, spawns) == 0)
		{
			LF_Debug.Error(FEATURE, string.Format("scene %1 has no spawn marker placed; nobody spawns", plan.m_sSceneId));
			return null;
		}

		if (!LF_EventScenePlanner.Begin(plan))
			return null;

		LF_EventSceneCrew crew = new LF_EventSceneCrew();
		crew.m_sSceneId = plan.m_sSceneId;
		crew.m_Plan = plan;
		s_Running = crew;

		SpawnVehicle(crew, spawns[0]);
		SpawnDue(crew, 0);
		if (!crew.IsFullySpawned())
			GetGame().GetCallqueue().CallLater(crew.Tick, TICK_MS, true);

		return crew;
	}

	//------------------------------------------------------------------------------------------------
	//! Spawns every planned walker whose stagger delay is at most `elapsedSeconds`. \return how many.
	static int SpawnDue(notnull LF_EventSceneCrew crew, float elapsedSeconds)
	{
		array<IEntity> spawns = {};
		int spawnCount = LF_EventWaypointComponent.FindSpawns(crew.m_sSceneId, spawns);
		int spawned = 0;
		while (!crew.IsFullySpawned() && crew.m_Plan.m_aNpcs[crew.m_iNext].m_fSpawnDelaySeconds <= elapsedSeconds)
		{
			int index = crew.m_iNext;
			crew.m_iNext++;
			if (spawnCount == 0)
			{
				LF_Debug.Error(FEATURE, string.Format("scene %1 lost its spawn markers; walker %2 not spawned", crew.m_sSceneId, index));
				continue;
			}

			IEntity person = SpawnPerson(crew, index, spawns[index % spawnCount], index / spawnCount);
			if (!person)
				continue;

			crew.m_aPeople.Insert(person.GetID());
			spawned++;
		}

		return spawned;
	}

	//------------------------------------------------------------------------------------------------
	//! The prefab a walker wears: a role id picks one of the role's outfits (the same one for the same
	//! scene and index), a `{GUID}path` is the outfit itself. "" when it is neither.
	static ResourceName OutfitFor(string roleOrOutfit, string seed)
	{
		if (roleOrOutfit.StartsWith("{"))
			return roleOrOutfit;

		return LF_Roles.PickOutfit(roleOrOutfit, seed);
	}

	//------------------------------------------------------------------------------------------------
	protected static IEntity SpawnPerson(LF_EventSceneCrew crew, int index, IEntity marker, int rank)
	{
		LF_EventSceneNpc npc = crew.m_Plan.m_aNpcs[index];
		ResourceName outfit = OutfitFor(npc.m_sRole, crew.m_sSceneId + "_" + index);
		if (outfit.IsEmpty())
		{
			LF_Debug.Error(FEATURE, string.Format("scene %1 walker %2: role or outfit '%3' is unknown; not spawned", crew.m_sSceneId, index, npc.m_sRole));
			return null;
		}

		vector transform[4];
		marker.GetWorldTransform(transform);
		// Walkers on one marker line up sideways: 0, +1, -1, +2 ... spacings.
		float side = Math.Ceil(rank / 2.0) * SPACING_M;
		if (rank % 2 == 0)
			side = -side;
		vector at = transform[3] + transform[0] * side;

		IEntity person = LF_Utils.SpawnEntityPrefab(outfit, at, marker.GetYawPitchRoll());
		if (!person)
		{
			LF_Debug.Error(FEATURE, string.Format("scene %1 walker %2: %3 did not spawn", crew.m_sSceneId, index, outfit));
			return null;
		}

		LF_Debug.Info(FEATURE, string.Format("scene %1 walker %2 spawned at %3 (%4)", crew.m_sSceneId, index, marker.GetName(), outfit));
		return person;
	}

	//------------------------------------------------------------------------------------------------
	//! The plan's parked vehicle beside the first spawn marker, by prefab slot (#964). Nobody drives
	//! it: it spawns empty and stays where it stands.
	protected static void SpawnVehicle(LF_EventSceneCrew crew, IEntity marker)
	{
		string slot = crew.m_Plan.m_sVehicleSlot;
		if (slot.IsEmpty())
			return;

		ResourceName prefab = LF_PrefabSlots.Get(slot);
		if (prefab.IsEmpty())
		{
			LF_Debug.Error(FEATURE, string.Format("scene %1: vehicle slot '%2' resolves to no prefab; no vehicle", crew.m_sSceneId, slot));
			return;
		}

		vector transform[4];
		marker.GetWorldTransform(transform);
		IEntity vehicle = LF_Utils.SpawnEntityPrefab(prefab, transform[3] + transform[0] * VEHICLE_SIDE_M, marker.GetYawPitchRoll());
		if (!vehicle)
		{
			LF_Debug.Error(FEATURE, string.Format("scene %1: vehicle %2 did not spawn", crew.m_sSceneId, prefab));
			return;
		}

		crew.m_VehicleId = vehicle.GetID();
		LF_Debug.Info(FEATURE, string.Format("scene %1 parked vehicle %2", crew.m_sSceneId, prefab));
	}

	//------------------------------------------------------------------------------------------------
	//! The eyes of every player with a body: where they look from, and the body a trace must skip.
	static void CollectPlayerEyes(notnull array<vector> eyes, notnull array<IEntity> bodies)
	{
		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!playerManager)
			return;

		array<int> players = {};
		playerManager.GetPlayers(players);
		foreach (int playerId : players)
		{
			IEntity body = playerManager.GetPlayerControlledEntity(playerId);
			if (!body)
				continue;

			eyes.Insert(body.GetOrigin() + EYE_OFFSET);
			bodies.Insert(body);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! An eye sees the entity when it is in range and the line to it is clear: nothing in the way, or
	//! the first thing hit is the scene's own (a walker in front of another still shows the crowd).
	static bool Sees(vector eye, IEntity body, IEntity target, notnull array<IEntity> sceneEntities)
	{
		vector point = target.GetOrigin() + BODY_OFFSET;
		if (vector.Distance(eye, point) > SIGHT_RANGE_M)
			return false;

		TraceParam trace = new TraceParam();
		trace.Start = eye;
		trace.End = point;
		trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
		trace.LayerMask = EPhysicsLayerPresets.Projectile;
		if (body)
			trace.Exclude = body;

		float fraction = GetGame().GetWorld().TraceMove(trace, null);
		if (trace.TraceEnt && sceneEntities.Contains(trace.TraceEnt))
			return true;

		return fraction >= 0.99;
	}

	//------------------------------------------------------------------------------------------------
	//! Ends the crew's scene when nobody sees any of its entities: deletes them and frees the scene
	//! slot. `eyes` (with `bodies` beside them, null entries allowed) stands in for the players; null
	//! reads the connected players. \return true when removed, false (logged) while someone sees one,
	//! so the caller tries again later.
	static bool TryDespawn(notnull LF_EventSceneCrew crew, array<vector> eyes = null, array<IEntity> bodies = null)
	{
		array<vector> lookers = {};
		array<IEntity> lookerBodies = {};
		if (eyes)
		{
			lookers.Copy(eyes);
			if (bodies)
				lookerBodies.Copy(bodies);
		}
		else
		{
			CollectPlayerEyes(lookers, lookerBodies);
		}

		array<IEntity> entities = {};
		crew.GetEntities(entities);
		foreach (int i, vector eye : lookers)
		{
			IEntity body;
			if (i < lookerBodies.Count())
				body = lookerBodies[i];

			foreach (IEntity entity : entities)
			{
				if (!Sees(eye, body, entity, entities))
					continue;

				LF_Debug.Info(FEATURE, string.Format("scene %1 despawn waits: %2 is in sight", crew.m_sSceneId, LF_Utils.GetPrefabName(entity)));
				return false;
			}
		}

		GetGame().GetCallqueue().Remove(crew.Tick);
		foreach (IEntity entity : entities)
			SCR_EntityHelper.DeleteEntityAndChildren(entity);

		crew.m_aPeople.Clear();
		crew.m_VehicleId = EntityID.INVALID;
		LF_Debug.Info(FEATURE, string.Format("scene %1 despawned %2 entities out of sight", crew.m_sSceneId, entities.Count()));
		LF_EventScenePlanner.Finish(crew.m_sSceneId);
		if (s_Running == crew)
			s_Running = null;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		if (s_Running && GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(s_Running.Tick);

		s_Running = null;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_EventSceneSpawn's statics (Core/LF_StaticReset.c). A running scene's crew
//! is dropped; the world that held its entities is gone with it.
class LF_EventSceneSpawn_StaticResetHook : LF_StaticResetHook
{
	protected static bool s_bRegistered = LF_StaticReset.Register(new LF_EventSceneSpawn_StaticResetHook());

	override string Owner()
	{
		return "LF_EventSceneSpawn";
	}

	override void Apply()
	{
		LF_EventSceneSpawn.ResetStatics();
	}
}
