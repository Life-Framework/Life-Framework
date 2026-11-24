//------------------------------------------------------------------------------------------------
//! A felled tree falls over and leaves its logs (design docs/features/gathering-processing-resources.md
//! "Tree fall"). LF_TreeFallPlan says where each piece goes; this file puts them in the world.
//!  - Server: the fell (LF_FellAction.TryFell) broadcasts the fall direction (away from the player) and,
//!    when the trunk lands (LF_TreeFallPlan.FALL_MS later), drops the real log items along the fallen
//!    line, one per log the yield gives, each probed down to the ground and left to its own rigid body.
//!    The logs are ordinary replicated items: picked up once through the inventory, streamed to a late
//!    joiner, never saved. A loose-log book deletes the ones nobody took after LOG_LIFE_S and holds the
//!    server to MAX_LOOSE_LOGS lying logs; a log a player picked up or moved leaves the book.
//!  - Every rendering peer (client, listen host): a local copy of the tree's mesh tips over about its foot,
//!    lands, bounces once and is replaced by the logs; a few vanilla branch props stay where the crown hit
//!    and despawn after DEBRIS_LIFE_MS. Nothing of this is replicated; a dedicated server draws nothing.
class LF_TreeFallAnim : Managed
{
	EntityID m_Visual;
	vector m_vX;
	vector m_vY;
	vector m_vZ;
	vector m_vFoot;
	vector m_vAxis;
	float m_fYaw;
	float m_fHeight;
	float m_fStartMs;
	int m_iSeed;
	bool m_bHardwood;
	int m_iSize;
}

//------------------------------------------------------------------------------------------------
class LF_TreeDebris : Managed
{
	EntityID m_Id;
	float m_fDieMs;
}

//------------------------------------------------------------------------------------------------
class LF_TreeFall
{
	static const string FEATURE = "Lumber";
	//! Seconds a dropped log may lie untouched before the sweep deletes it.
	static const int LOG_LIFE_S = 1800;
	//! Most logs lying loose on the server at once; past it the oldest lying ones go.
	static const int MAX_LOOSE_LOGS = 160;	// lf-fixed: a server perf cap on loose props, far above what a crew fells in 30 min
	static const int SWEEP_MS = 30000;	// lf-fixed: the loose-log sweep cadence, seconds of precision on a 30 min life
	//! Ground probe: from this far above the planned spot, this far down.
	static const float PROBE_UP_M = 3;
	static const float PROBE_REACH_M = 8;	// lf-fixed: ground probe geometry
	//! Lift over the probed ground the log spawns at; its rigid body settles the rest.
	static const float LOG_CLEAR_M = 0.05;

	//! The fall is drawn only for a player this close to the tree.
	static const float VIEW_M = 400;
	static const int ANIM_MS = 15;	// lf-fixed: the fall animation frame step
	//! The trunk stays down this long after the bounce, so the logs have arrived before it goes.
	static const int HOLD_MS = 250;	// lf-fixed: animation timing tied to the replication of the logs
	static const int DEBRIS_LIFE_MS = 180000;	// lf-fixed: cosmetic branch props, how long they read as fresh, not a server tunable
	static const int MAX_DEBRIS = 40;	// lf-fixed: a client perf cap on local props
	static const int DEBRIS_SWEEP_MS = 5000;	// lf-fixed: the branch prop despawn cadence

	//! Vanilla branch meshes (Prefabs/Vegetation/Tree/Debris and the tree branch prefabs).
	static const ResourceName BRANCH_CON = "{2DCF56614243A58C}Assets/Vegetation/Debris/Branches_Trash/BranchTrash_con_01.xob";
	static const ResourceName BRANCH_DEC = "{1EB5936FE2A30B89}Assets/Vegetation/Debris/Branches_Trash/BranchTrash_dec_01.xob";
	static const ResourceName PINE_BRANCH = "{6DDF91D971719A19}Assets/Vegetation/Tree/Pinus_Sylvestris/T_Pinus_sylvestris_02_Branch.xob";
	static const ResourceName HORNBEAM_BRANCH = "{86F97F6639047FCE}Assets/Vegetation/Tree/Carpinus_Betulus/t_carpinus_betulus_branch.xob";
	static const ResourceName TWIGS_SPARSE = "{C7A2FAD16A775C95}Assets/Clutter/Vegetation/Debris/Branches/c_branches_debris_sparse_01.xob";
	static const ResourceName TWIGS_LARGE = "{9A844E0B508D2E33}Assets/Clutter/Vegetation/Debris/Branches/c_branches_debris_large_01.xob";

	protected static ref LF_LooseLogBook s_Book;
	protected static bool s_bSweeping;
	protected static ref array<ref LF_TreeFallAnim> s_aAnims;
	protected static ref array<ref LF_TreeDebris> s_aDebris;
	protected static bool s_bAnimating;
	protected static bool s_bDebrisTicking;

	//------------------------------------------------------------------------------------------------
	static LF_LooseLogBook Book()
	{
		if (!s_Book)
			s_Book = new LF_LooseLogBook();

		return s_Book;
	}

	//------------------------------------------------------------------------------------------------
	//! Client count of the fall copies in the air and the branch props lying (harness, tests).
	static int AnimCount()
	{
		if (!s_aAnims)
			return 0;

		return s_aAnims.Count();
	}

	//------------------------------------------------------------------------------------------------
	static int DebrisCount()
	{
		if (!s_aDebris)
			return 0;

		return s_aDebris.Count();
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		ArmaReforgerScripted game = GetGame();
		if (game && game.GetCallqueue())
		{
			game.GetCallqueue().Remove(Sweep);
			game.GetCallqueue().Remove(Step);
			game.GetCallqueue().Remove(DebrisTick);
			game.GetCallqueue().Remove(DropLater);
		}

		s_Book = null;
		s_bSweeping = false;
		s_aAnims = null;
		s_aDebris = null;
		s_bAnimating = false;
		s_bDebrisTicking = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Deletes every falling copy and branch prop this peer drew (a test cleans up with it).
	static void ClearLocal()
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;

		if (s_aAnims)
		{
			foreach (LF_TreeFallAnim anim : s_aAnims)
			{
				IEntity visual = world.FindEntityByID(anim.m_Visual);
				if (visual)
					SCR_EntityHelper.DeleteEntityAndChildren(visual);
			}

			s_aAnims.Clear();
		}

		if (s_aDebris)
		{
			foreach (LF_TreeDebris debris : s_aDebris)
			{
				DeleteDebris(world, debris);
			}

			s_aDebris.Clear();
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Harness: the nearest standing map tree within `radius` that passes every felling rule, the axe,
	//! the reach and stamina aside (a tree in a garden by a house or on a verge does not count).
	static IEntity NearestFellable(vector from, float radius, LF_TreeWoodClass onlyClass = LF_TreeWoodClass.NONE)
	{
		BaseWorld world = GetGame().GetWorld();
		LF_TreeFellingConfig config = LF_TreeFellingConfig.Get();
		if (!world || !config)
			return null;

		LF_FelledTreeQuery query = new LF_FelledTreeQuery();
		world.QueryEntitiesBySphere(from, radius, query.Collect, null, EQueryEntitiesFlags.ALL);
		IEntity best;
		float bestGap = float.MAX;
		foreach (IEntity entity : query.m_aEntities)
		{
			if (!Tree.Cast(entity) || LF_FelledTreesReplication.IsHidden(entity))
				continue;

			float gap = vector.Distance(entity.GetOrigin(), from);
			if (gap >= bestGap)
				continue;

			LF_FellFacts facts = new LF_FellFacts();
			facts.m_bHasAxe = true;
			facts.m_sPrefab = LF_FellAction.TreePrefabOf(entity);
			if (facts.m_sPrefab.IsEmpty())
				continue;

			if (onlyClass != LF_TreeWoodClass.NONE && LF_TreeFelling.KindOf(facts.m_sPrefab, config).m_eClass != onlyClass)
				continue;

			vector at = entity.GetOrigin();
			facts.m_bInSafeZone = LF_Zones.IsSafe(at);
			facts.m_fRoadDistanceM = LF_FelledTrees.RoadDistance(at, config.m_fRoadClearM);
			facts.m_fBuildingDistanceM = LF_FelledTrees.BuildingDistance(at);
			if (LF_TreeFelling.Check(facts, config) != LF_FellRefusal.OK)
				continue;

			bestGap = gap;
			best = entity;
		}

		return best;
	}

	//------------------------------------------------------------------------------------------------
	//! The tree's height from its world bounds.
	static float HeightOf(notnull IEntity tree)
	{
		vector mins;
		vector maxs;
		tree.GetWorldBounds(mins, maxs);
		return LF_TreeFallPlan.ClampHeight(maxs[1] - mins[1]);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: tells every rendering peer to draw the fall of the tree a key names, away along `yawDeg`.
	static void Broadcast(string key, float yawDeg)
	{
		LF_GameModeRoleplay mode = LF_GameModeRoleplay.Cast(GetGame().GetGameMode());
		if (!mode)
		{
			LF_Debug.Warn(FEATURE, "tree fall: no roleplay game mode to broadcast on; the tree just disappears");
			return;
		}

		mode.LF_TreeFallBroadcast(key, yawDeg);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: drops the logs when the trunk lands, `delayMs` from now (0: at once, and `logs` holds them).
	static void ScheduleDrop(ResourceName prefab, vector foot, float yawDeg, float heightM, int count, int delayMs, out array<IEntity> logs)
	{
		logs = {};
		ArmaReforgerScripted game = GetGame();
		if (delayMs <= 0 || !game || !game.GetCallqueue())
		{
			DropLogs(prefab, foot, yawDeg, heightM, count, logs);
			return;
		}

		game.GetCallqueue().CallLater(DropLater, delayMs, false, prefab, foot, yawDeg, heightM, count);
	}

	//------------------------------------------------------------------------------------------------
	protected static void DropLater(ResourceName prefab, vector foot, float yawDeg, float heightM, int count)
	{
		if (!GetGame().GetWorld())
			return;

		array<IEntity> logs = {};
		DropLogs(prefab, foot, yawDeg, heightM, count, logs);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: one log item per count along the fallen line, each turned along it, its bottom on the probed
	//! ground. Every log joins the loose-log book. Fills `logs` with what spawned.
	static void DropLogs(ResourceName prefab, vector foot, float yawDeg, float heightM, int count, notnull array<IEntity> logs)
	{
		vector mins;
		vector maxs;
		if (!LogBounds(prefab, foot, mins, maxs))
		{
			LF_Debug.Warn(FEATURE, "tree fall: log bounds not read, a 2 m log is assumed: " + prefab);
			mins = Vector(-0.2, -0.2, -1);
			maxs = Vector(0.2, 0.2, 1);
		}

		// The long side of the mesh lies along the line.
		float lenZ = maxs[2] - mins[2];
		float lenX = maxs[0] - mins[0];
		float length = lenZ;
		float turn = 0;
		if (lenX > lenZ)
		{
			length = lenX;
			turn = 90;
		}

		vector centre = (mins + maxs) * 0.5;
		array<ref LF_TreeFallSpot> spots = LF_TreeFallPlan.LogSpots(foot, yawDeg, heightM, length, count);
		int nowS = LF_FelledTrees.NowS();
		foreach (LF_TreeFallSpot spot : spots)
		{
			vector angles = Vector(spot.m_fYawDeg + turn, 0, 0);
			vector mat[4];
			Math3D.AnglesToMatrix(angles, mat);
			vector offset = mat[0] * centre[0] + mat[2] * centre[2];
			vector along = LF_TreeFallPlan.Dir(spot.m_fYawDeg) * (length * 0.5);
			float ground = GroundY(spot.m_vPos);
			ground = Math.Max(ground, GroundY(spot.m_vPos + along));
			ground = Math.Max(ground, GroundY(spot.m_vPos - along));

			vector origin = spot.m_vPos - offset;
			origin[1] = ground - mins[1] + LOG_CLEAR_M;
			IEntity log = LF_Utils.SpawnEntityPrefab(prefab, origin, angles);
			if (!log)
			{
				LF_Debug.Error(FEATURE, "log prefab did not spawn: " + prefab);
				continue;
			}

			logs.Insert(log);
			Book().Add(log.GetID(), log.GetOrigin(), nowS);
		}

		LF_Debug.Info(FEATURE, string.Format("tree fall: %1 of %2 logs dropped along yaw %3 (%4 loose on the server)", logs.Count(), count, Math.Round(yawDeg), Book().Count()));
		EnsureSweeping();
	}

	//------------------------------------------------------------------------------------------------
	//! The local mesh bounds of a log prefab, read from a short-lived local copy far above `near`.
	protected static bool LogBounds(ResourceName prefab, vector near, out vector mins, out vector maxs)
	{
		IEntity probe = LF_Utils.SpawnEntityPrefab(prefab, near + "0 400 0", "0 0 0", false);
		if (!probe)
			return false;

		probe.GetBounds(mins, maxs);
		SCR_EntityHelper.DeleteEntityAndChildren(probe);
		return maxs[2] - mins[2] > 0.1 || maxs[0] - mins[0] > 0.1;
	}

	//------------------------------------------------------------------------------------------------
	//! The highest surface under a point: the ground or a rock, root or wall a trace meets, never another
	//! item or a tree. Falls back to the terrain.
	static float GroundY(vector at)
	{
		BaseWorld world = GetGame().GetWorld();
		float terrain = world.GetSurfaceY(at[0], at[2]);
		vector from = Vector(at[0], Math.Max(at[1], terrain) + PROBE_UP_M, at[2]);
		IEntity hit;
		float y = LF_PlaceSnap.Down(null, from, PROBE_REACH_M, -1, hit);
		if (y < -1e8)
			return terrain;

		return Math.Max(y, terrain);
	}

	//------------------------------------------------------------------------------------------------
	protected static void EnsureSweeping()
	{
		if (s_bSweeping || !Replication.IsServer())
			return;

		ArmaReforgerScripted game = GetGame();
		if (!game || !game.GetCallqueue())
			return;

		s_bSweeping = true;
		game.GetCallqueue().CallLater(Sweep, SWEEP_MS, true);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Sweep()
	{
		SweepAt(LF_FelledTrees.NowS());
		if (Book().Count() > 0)
			return;

		s_bSweeping = false;
		ArmaReforgerScripted game = GetGame();
		if (game && game.GetCallqueue())
			game.GetCallqueue().Remove(Sweep);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: deletes the logs that lay untouched past LOG_LIFE_S, forgets the ones players took, then
	//! deletes the oldest lying ones past MAX_LOOSE_LOGS. Public for the tests. \return how many it deleted.
	static int SweepAt(int nowS, int lifeS = LOG_LIFE_S, int cap = MAX_LOOSE_LOGS)
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world || !Replication.IsServer())
			return 0;

		int deleted;
		int forgotten;
		array<ref LF_LooseLog> keep = {};
		foreach (LF_LooseLog log : Book().m_aLogs)
		{
			IEntity entity = world.FindEntityByID(log.m_Id);
			vector at = log.m_vAt;
			if (entity)
				at = entity.GetOrigin();

			// The first sweep after the drop records where the log came to rest, so a log that rolled down a
			// slope while it settled still counts as lying where it fell (and is swept in time).
			if (entity && !log.m_bSettled && !IsTaken(entity))
			{
				log.m_vAt = at;
				log.m_bSettled = true;
			}

			LF_LooseLogFate fate = LF_LooseLogBook.Fate(log, entity != null, IsTaken(entity), at, nowS, lifeS);
			if (fate == LF_LooseLogFate.KEEP)
			{
				keep.Insert(log);
				continue;
			}

			if (fate == LF_LooseLogFate.DELETE)
			{
				SCR_EntityHelper.DeleteEntityAndChildren(entity);
				deleted++;
				continue;
			}

			forgotten++;
		}

		Book().m_aLogs = keep;
		int over = Book().OverCap(cap);
		for (int i = 0; i < over; i++)
		{
			IEntity oldest = world.FindEntityByID(Book().m_aLogs[0].m_Id);
			if (oldest && !IsTaken(oldest))
			{
				SCR_EntityHelper.DeleteEntityAndChildren(oldest);
				deleted++;
			}

			Book().m_aLogs.RemoveOrdered(0);
		}

		if (deleted > 0 || forgotten > 0)
			LF_Debug.Info(FEATURE, string.Format("loose logs: %1 deleted, %2 taken by players, %3 still lying", deleted, forgotten, Book().Count()));

		return deleted;
	}

	//------------------------------------------------------------------------------------------------
	//! True when a log is in a hand, on a shoulder, in a container or under any parent.
	static bool IsTaken(IEntity log)
	{
		if (!log)
			return false;

		if (log.GetParent())
			return true;

		InventoryItemComponent item = InventoryItemComponent.Cast(log.FindComponent(InventoryItemComponent));
		return item != null && item.GetParentSlot() != null;
	}

	//------------------------------------------------------------------------------------------------
	//! A rendering peer: draws the fall of the tree a key names. A tree that is not streamed in, or far
	//! from the local player, is not drawn (its logs still arrive as items).
	static bool ClientFall(string key, float yawDeg)
	{
		if (System.IsConsoleApp())
			return false;

		ArmaReforgerScripted game = GetGame();
		BaseWorld world = game.GetWorld();
		IEntity tree = LF_FelledTreesReplication.FindTree(key);
		if (!world || !tree)
		{
			LF_Debug.Info(FEATURE, "tree fall: not drawn, the tree is not streamed in here: " + key);
			return false;
		}

		IEntity viewer = LF_Utils.GetLocalCharacter();
		if (viewer && vector.Distance(viewer.GetOrigin(), tree.GetOrigin()) > VIEW_M)
			return false;

		VObject mesh = tree.GetVObject();
		if (!mesh)
		{
			LF_Debug.Warn(FEATURE, "tree fall: the tree has no mesh to draw falling: " + key);
			return false;
		}

		vector mat[4];
		tree.GetWorldTransform(mat);
		EntitySpawnParams params();
		params.TransformMode = ETransformMode.WORLD;
		for (int i = 0; i < 4; i++)
		{
			params.Transform[i] = mat[i];
		}

		IEntity visual = game.SpawnEntity(GenericEntity, world, params);
		if (!visual)
		{
			LF_Debug.Error(FEATURE, "tree fall: the falling copy did not spawn: " + key);
			return false;
		}

		visual.SetObject(mesh, "");
		visual.SetFlags(EntityFlags.VISIBLE, false);

		LF_TreeFallAnim anim = new LF_TreeFallAnim();
		anim.m_Visual = visual.GetID();
		anim.m_vX = mat[0];
		anim.m_vY = mat[1];
		anim.m_vZ = mat[2];
		anim.m_vFoot = mat[3];
		vector dir = LF_TreeFallPlan.Dir(yawDeg);
		anim.m_vAxis = LF_TreeFallPlan.Cross(vector.Up, dir);
		anim.m_fYaw = yawDeg;
		anim.m_fHeight = HeightOf(tree);
		anim.m_fStartMs = world.GetWorldTime();
		anim.m_iSeed = key.Hash();
		LF_TreeKind kind = LF_TreeFelling.KindOf(LF_FellAction.TreePrefabOf(tree), LF_TreeFellingConfig.Get());
		anim.m_bHardwood = kind.m_eClass == LF_TreeWoodClass.HARDWOOD;
		anim.m_iSize = kind.m_iSize;

		if (!s_aAnims)
			s_aAnims = new array<ref LF_TreeFallAnim>();

		s_aAnims.Insert(anim);
		LF_Debug.Info(FEATURE, string.Format("tree fall: drawing %1 falling along yaw %2", key, Math.Round(yawDeg)));
		if (!s_bAnimating)
		{
			s_bAnimating = true;
			game.GetCallqueue().CallLater(Step, ANIM_MS, true);
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static void Step()
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world || !s_aAnims || s_aAnims.IsEmpty())
		{
			s_bAnimating = false;
			GetGame().GetCallqueue().Remove(Step);
			return;
		}

		float now = world.GetWorldTime();
		for (int i = s_aAnims.Count() - 1; i >= 0; i--)
		{
			LF_TreeFallAnim anim = s_aAnims[i];
			IEntity visual = world.FindEntityByID(anim.m_Visual);
			int elapsed = now - anim.m_fStartMs;
			if (!visual || elapsed >= LF_TreeFallPlan.FALL_MS + LF_TreeFallPlan.BOUNCE_MS + HOLD_MS)
			{
				if (visual)
					SCR_EntityHelper.DeleteEntityAndChildren(visual);

				SpawnDebris(anim);
				s_aAnims.Remove(i);
				continue;
			}

			float angle = LF_TreeFallPlan.AngleAt(elapsed);
			vector mat[4];
			mat[0] = LF_TreeFallPlan.Rotate(anim.m_vX, anim.m_vAxis, angle);
			mat[1] = LF_TreeFallPlan.Rotate(anim.m_vY, anim.m_vAxis, angle);
			mat[2] = LF_TreeFallPlan.Rotate(anim.m_vZ, anim.m_vAxis, angle);
			mat[3] = anim.m_vFoot;
			visual.SetWorldTransform(mat);
			visual.Update();
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The branch meshes a tree of this wood leaves, the big branch first.
	static array<ResourceName> DebrisMeshes(bool hardwood)
	{
		array<ResourceName> meshes = {};
		if (hardwood)
		{
			meshes.Insert(HORNBEAM_BRANCH);
			meshes.Insert(BRANCH_DEC);
			meshes.Insert(TWIGS_LARGE);
			meshes.Insert(BRANCH_DEC);
			meshes.Insert(TWIGS_LARGE);
			return meshes;
		}

		meshes.Insert(PINE_BRANCH);
		meshes.Insert(BRANCH_CON);
		meshes.Insert(TWIGS_SPARSE);
		meshes.Insert(BRANCH_CON);
		meshes.Insert(TWIGS_SPARSE);
		return meshes;
	}

	//------------------------------------------------------------------------------------------------
	//! Local branch props where the crown landed, each set down on the ground; the oldest go past MAX_DEBRIS.
	protected static void SpawnDebris(notnull LF_TreeFallAnim anim)
	{
		ArmaReforgerScripted game = GetGame();
		BaseWorld world = game.GetWorld();
		array<ResourceName> meshes = DebrisMeshes(anim.m_bHardwood);
		int count = LF_TreeFallPlan.DebrisCount(anim.m_iSize);
		array<ref LF_TreeFallSpot> spots = LF_TreeFallPlan.DebrisSpots(anim.m_vFoot, anim.m_fYaw, anim.m_fHeight, count, anim.m_iSeed);
		if (!s_aDebris)
			s_aDebris = new array<ref LF_TreeDebris>();

		float now = world.GetWorldTime();
		int spawned;
		foreach (int i, LF_TreeFallSpot spot : spots)
		{
			ResourceName meshName = meshes[i - (i / meshes.Count()) * meshes.Count()];
			Resource model = Resource.Load(meshName);
			if (!model || !model.IsValid())
			{
				LF_Debug.Error(FEATURE, "tree fall: branch mesh does not load: " + meshName);
				continue;
			}

			EntitySpawnParams params();
			params.TransformMode = ETransformMode.WORLD;
			Math3D.AnglesToMatrix(Vector(spot.m_fYawDeg, 0, 0), params.Transform);
			vector at = spot.m_vPos;
			at[1] = GroundY(at);
			params.Transform[3] = at;
			IEntity prop = game.SpawnEntity(GenericEntity, world, params);
			if (!prop)
				continue;

			prop.SetObject(model.GetResource().ToVObject(), "");
			prop.SetFlags(EntityFlags.VISIBLE, false);
			// The mesh's own bottom on the ground, not its pivot.
			vector mins;
			vector maxs;
			prop.GetBounds(mins, maxs);
			at[1] = at[1] - mins[1];
			prop.SetOrigin(at);
			prop.Update();

			LF_TreeDebris debris = new LF_TreeDebris();
			debris.m_Id = prop.GetID();
			debris.m_fDieMs = now + DEBRIS_LIFE_MS;
			s_aDebris.Insert(debris);
			spawned++;
		}

		while (s_aDebris.Count() > MAX_DEBRIS)
		{
			DeleteDebris(world, s_aDebris[0]);
			s_aDebris.RemoveOrdered(0);
		}

		LF_Debug.Info(FEATURE, string.Format("tree fall: landed, %1 branch props (%2 lying here)", spawned, s_aDebris.Count()));
		if (!s_bDebrisTicking && !s_aDebris.IsEmpty())
		{
			s_bDebrisTicking = true;
			game.GetCallqueue().CallLater(DebrisTick, DEBRIS_SWEEP_MS, true);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static void DeleteDebris(BaseWorld world, LF_TreeDebris debris)
	{
		IEntity prop = world.FindEntityByID(debris.m_Id);
		if (prop)
			SCR_EntityHelper.DeleteEntityAndChildren(prop);
	}

	//------------------------------------------------------------------------------------------------
	protected static void DebrisTick()
	{
		BaseWorld world = GetGame().GetWorld();
		if (world && s_aDebris)
		{
			float now = world.GetWorldTime();
			for (int i = s_aDebris.Count() - 1; i >= 0; i--)
			{
				if (s_aDebris[i].m_fDieMs > now)
					continue;

				DeleteDebris(world, s_aDebris[i]);
				s_aDebris.Remove(i);
			}
		}

		if (world && s_aDebris && !s_aDebris.IsEmpty())
			return;

		s_bDebrisTicking = false;
		GetGame().GetCallqueue().Remove(DebrisTick);
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_TreeFall's statics (LF_StaticReset finds it by reflection).
class LF_TreeFall_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_TreeFall";
	}

	override void Apply()
	{
		LF_TreeFall.ResetStatics();
	}
}

//------------------------------------------------------------------------------------------------
//! The game mode carries the fall: one reliable broadcast per fell (the tree key and the yaw it falls along).
modded class LF_GameModeRoleplay
{
	//------------------------------------------------------------------------------------------------
	//! Server: every rendering peer draws the fall; a listen host draws it here (a broadcast skips its sender).
	void LF_TreeFallBroadcast(string key, float yawDeg)
	{
		Rpc(RpcDo_LF_TreeFall, key, yawDeg);
		if (!System.IsConsoleApp())
			RpcDo_LF_TreeFall(key, yawDeg);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_LF_TreeFall(string key, float yawDeg)
	{
		LF_TreeFall.ClientFall(key, yawDeg);
	}
}

//------------------------------------------------------------------------------------------------
//! Harness `treefall` (dev gate on the server): the motion proof of a fell.
//!   treefall fell [radius|key]  the server fells the tree a key from `find` names (or the nearest one) through
//!                           LF_FellAction.TryFell as if the axe were in hand; the reply names the key and refusal
//!   treefall find [radius] [softwood|hardwood]  the nearest standing tree that passes every felling rule but the axe and reach: `key=<key> pos=<x> <y> <z>`
//!   treefall logs [radius]  the nearest loose log: `logs=<n> pos=<x> <y> <z>` (walk to it, then `pickup WoodLog`)
//!   treefall state          this peer's falling copies and branch props: `anims=<n> debris=<n>`
//!   treefall census         counts every map tree of the world by prefab family and how many an axe can fell
//!                           by placed path and by ancestry; the result is `tree census` lines in the log (LF_TreeCensus)
modded class LF_AgentHarnessVerbs
{
	override bool Handle(LF_AgentCommand cmd)
	{
		if (cmd.Verb != "treefall")
			return super.Handle(cmd);

		if (cmd.Arg1 == "state")
		{
			LF_AgentHarness.Answer(cmd.Id, "ok", string.Format("anims=%1 debris=%2", LF_TreeFall.AnimCount(), LF_TreeFall.DebrisCount()));
			return true;
		}

		if (cmd.Arg1 == "census")
		{
			if (LF_TreeCensus.Start())
				LF_AgentHarness.Answer(cmd.Id, "ok", "census started; read `tree census` lines in the log");
			else
				LF_AgentHarness.Answer(cmd.Id, "error", "census not started (no world, or one is running)");

			return true;
		}

		SCR_ChimeraCharacter character = LF_Utils.GetLocalCharacter();
		if (cmd.Arg1 == "find" && character)
		{
			float findRadius = 80;
			if (!cmd.Arg2.IsEmpty())
				findRadius = cmd.Arg2.ToFloat();

			LF_TreeWoodClass onlyClass = LF_TreeWoodClass.NONE;
			if (cmd.Arg3 == "softwood")
				onlyClass = LF_TreeWoodClass.SOFTWOOD;
			else if (cmd.Arg3 == "hardwood")
				onlyClass = LF_TreeWoodClass.HARDWOOD;

			IEntity fellable = LF_TreeFall.NearestFellable(character.GetOrigin(), findRadius, onlyClass);
			if (!fellable)
			{
				LF_AgentHarness.Answer(cmd.Id, "error", string.Format("no fellable tree (rules passed, axe and reach aside) within %1 m", findRadius));
				return true;
			}

			vector at = fellable.GetOrigin();
			LF_AgentHarness.Answer(cmd.Id, "ok", string.Format("key=%1 pos=%2 %3 %4", LF_FellAction.KeyOfTree(fellable), at[0], at[1], at[2]));
			return true;
		}

		if (cmd.Arg1 == "logs" && character)
		{
			// The nearest loose log within a radius, so a proof can walk to it (the logs lie along the fall line, past pickup reach).
			float logRadius = 30;
			if (!cmd.Arg2.IsEmpty())
				logRadius = cmd.Arg2.ToFloat();

			LF_FelledTreeQuery logQuery = new LF_FelledTreeQuery();
			GetGame().GetWorld().QueryEntitiesBySphere(character.GetOrigin(), logRadius, logQuery.Collect, null, EQueryEntitiesFlags.ALL);
			IEntity nearestLog;
			float nearestGap = float.MAX;
			int logCount = 0;
			foreach (IEntity candidate : logQuery.m_aEntities)
			{
				ResourceName candidateName = LF_Utils.GetPrefabName(candidate);
				if (!LF_Utils.SamePrefab(candidateName, LF_FellAction.LOG_PREFAB) && !LF_Utils.SamePrefab(candidateName, LF_FellAction.HARDWOOD_LOG_PREFAB))
					continue;

				logCount++;
				float logGap = vector.Distance(candidate.GetOrigin(), character.GetOrigin());
				if (logGap < nearestGap)
				{
					nearestGap = logGap;
					nearestLog = candidate;
				}
			}

			if (!nearestLog)
			{
				LF_AgentHarness.Answer(cmd.Id, "error", string.Format("no loose log within %1 m", logRadius));
				return true;
			}

			vector logAt = nearestLog.GetOrigin();
			LF_AgentHarness.Answer(cmd.Id, "ok", string.Format("logs=%1 pos=%2 %3 %4", logCount, logAt[0], logAt[1], logAt[2]));
			return true;
		}

		if (cmd.Arg1 != "fell" || !character)
		{
			LF_AgentHarness.Answer(cmd.Id, "error", "usage: treefall find [radius] [softwood|hardwood] | logs [radius] | fell [radius|key] | state (find and fell need a character)");
			return true;
		}

		// A key from `find` names one tree; a number is a radius for the nearest one.
		string target = cmd.Arg2;
		if (target.IsEmpty())
			target = "12";

		character.LF_AskHarnessTreeFall(cmd.Id, target);
		return true;
	}
}

//------------------------------------------------------------------------------------------------
modded class SCR_ChimeraCharacter
{
	//------------------------------------------------------------------------------------------------
	void LF_AskHarnessTreeFall(int commandId, string target)
	{
		if (Replication.IsServer())
		{
			LF_HarnessTreeFallDo(commandId, target);
			return;
		}

		Rpc(RpcAsk_LF_HarnessTreeFall, commandId, target);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_HarnessTreeFall(int commandId, string target)
	{
		if (!LF_DevGate.CheckHarnessRequest(this, "treefall"))
		{
			LF_HarnessReply(commandId, "error", LF_AgentHarness.REFUSED_DEV_GATE);
			return;
		}

		LF_HarnessTreeFallDo(commandId, target);
	}

	//------------------------------------------------------------------------------------------------
	protected void LF_HarnessTreeFallDo(int commandId, string target)
	{
		IEntity tree;
		if (target.Length() > 256)
		{
			LF_HarnessReply(commandId, "error", "treefall: target too long");
			return;
		}

		if (target.Contains(":"))
		{
			tree = LF_FelledTreesReplication.FindTree(target);
		}
		else
		{
			int noPrefab;
			tree = LF_FelledTreesReplication.NearestTree(GetOrigin(), target.ToFloat(), noPrefab);
		}

		if (!tree)
		{
			LF_HarnessReply(commandId, "error", "treefall: no standing map tree for " + target);
			return;
		}

		string key = LF_FellAction.KeyOfTree(tree);
		array<IEntity> logs = {};
		LF_FellRefusal refusal = LF_FellAction.TryFell(this, tree, true, logs);
		LF_HarnessReply(commandId, "ok", string.Format("key=%1 refusal=%2", key, typename.EnumToString(LF_FellRefusal, refusal)));
	}
}
