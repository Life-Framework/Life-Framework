//------------------------------------------------------------------------------------------------
//! Composition placement (#1940, chapter On patrol #1014, epic #927 section 3 of
//! docs/roadmap/building-foundation.md). A composition is a set of props (cones and barriers for a
//! police roadblock) placed as one: one ghost drawn from its pieces, one footprint (each piece's
//! own box, so a set over a slope is judged piece by piece), one server-checked confirm that
//! spawns the whole set with every piece on the ground, and one pick-up that removes the set.
//!   Plan      (both sides) the verdict of a set at a spot and each piece's grounded transform
//!   Confirm   (server)     who, rate limit, malformed proposal, reach, the plan again, then spawn all
//!                          pieces (all or none) and remember them as one set
//!   PickUp    (server)     removes every piece of the set a piece belongs to
//! The ghost (LF_PlacementCompositionGhost) is client-local and never replicated. Nothing persists;
//! a roadblock is temporary (docs/roadmap/building-foundation.md section 5).
//! It reuses LF_PlacementRules and LF_PlacementGeometry unchanged and does not touch
//! LF_PlacementService (its single-prefab confirm stays the spike strip's path).
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! One prop of a composition: the prefab, where it lies in the set's frame (x across, z forward;
//! its height is the ground's), the turn from the set's heading and its footprint box (minimum
//! corner and maximum corner in the piece's own frame, bottom lifted off the surface so the ground
//! it lies on does not count).
class LF_PlacementPiece
{
	ResourceName m_sPrefab;
	vector m_vOffset;
	float m_fYaw;
	vector m_vMins = "-0.3 0.1 -0.3";
	vector m_vMaxs = "0.3 0.8 0.3";

	//------------------------------------------------------------------------------------------------
	static LF_PlacementPiece Create(ResourceName prefab, vector offset, float yaw, vector mins, vector maxs)
	{
		LF_PlacementPiece piece = new LF_PlacementPiece();
		piece.m_sPrefab = prefab;
		piece.m_vOffset = offset;
		piece.m_fYaw = yaw;
		piece.m_vMins = mins;
		piece.m_vMaxs = maxs;
		return piece;
	}
}

//------------------------------------------------------------------------------------------------
//! A composition: a name for the logs, its pieces, and how the set is placed. Built in script
//! (`AddPiece`) or read from a composition prefab's child props (`FromPrefab`).
class LF_PlacementComposition
{
	string m_sName;
	ref array<ref LF_PlacementPiece> m_aPieces = {};
	//! Metres between the player and the set's origin, at most (the plain reach; the proposal may be
	//! LF_PlacementCompositions.TOLERANCE_METRES further when the server sees it).
	float m_fReach = 4;
	LF_EPlacementWho m_eWho;

	//------------------------------------------------------------------------------------------------
	void AddPiece(ResourceName prefab, vector offset, float yaw = 0, vector mins = "-0.3 0.1 -0.3", vector maxs = "0.3 0.8 0.3")
	{
		m_aPieces.Insert(LF_PlacementPiece.Create(prefab, offset, yaw, mins, maxs));
	}

	//------------------------------------------------------------------------------------------------
	int Count()
	{
		return m_aPieces.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! A composition read from `parent`, a prefab whose child props are the pieces: each child's
	//! ancestor prefab, its `coords` (x across, z forward) and `angleY` in the parent. The pieces
	//! take the default box. Empty (and an error logged) when the prefab does not resolve or has no
	//! children.
	static LF_PlacementComposition FromPrefab(ResourceName parent)
	{
		LF_PlacementComposition composition = new LF_PlacementComposition();
		composition.m_sName = parent;
		Resource resource = Resource.Load(parent);
		if (!resource || !resource.IsValid())
		{
			LF_Debug.Error(LF_PlacementRules.FEATURE, string.Format("composition skipped: %1 does not resolve", parent));
			return composition;
		}

		IEntitySource source = resource.GetResource().ToEntitySource();
		if (!source)
		{
			LF_Debug.Error(LF_PlacementRules.FEATURE, string.Format("composition skipped: %1 is not an entity prefab", parent));
			return composition;
		}

		for (int i = 0; i < source.GetNumChildren(); i++)
		{
			IEntitySource child = source.GetChild(i);
			if (!child)
				continue;

			ResourceName prefab = child.GetResourceName();
			if (prefab.IsEmpty())
				continue;

			vector coords;
			float angle;
			child.Get("coords", coords);
			child.Get("angleY", angle);
			composition.AddPiece(prefab, coords, angle);
		}

		if (composition.Count() == 0)
			LF_Debug.Error(LF_PlacementRules.FEATURE, string.Format("composition skipped: %1 has no child props", parent));

		return composition;
	}
}

//------------------------------------------------------------------------------------------------
//! What Plan measured for a composition at one spot: the worst verdict, the first reason, and for
//! each piece its probe verdict and its transform on the ground.
class LF_PlacementCompositionPlan
{
	LF_EPlacementVerdict m_eVerdict = LF_EPlacementVerdict.NO_GROUND;
	string m_sReason;
	ref array<LF_EPlacementVerdict> m_aVerdicts = {};
	ref array<vector> m_aTransforms = {};

	//------------------------------------------------------------------------------------------------
	int Count()
	{
		return m_aVerdicts.Count();
	}

	//------------------------------------------------------------------------------------------------
	void GetTransform(int piece, out vector transform[4])
	{
		for (int i = 0; i < 4; i++)
			transform[i] = m_aTransforms[piece * 4 + i];
	}

	//------------------------------------------------------------------------------------------------
	void Add(LF_EPlacementVerdict verdict, vector transform[4])
	{
		m_aVerdicts.Insert(verdict);
		for (int i = 0; i < 4; i++)
			m_aTransforms.Insert(transform[i]);
	}
}

//------------------------------------------------------------------------------------------------
//! A set that was placed: its pieces by EntityID (never an IEntity, a piece can be destroyed).
class LF_PlacedComposition
{
	string m_sName;
	ref array<EntityID> m_aIds = {};
}

//------------------------------------------------------------------------------------------------
class LF_PlacementCompositions
{
	//! Proposal slack over the reach when the server sees it (see LF_PlacementService.TOLERANCE_METRES).
	static const float TOLERANCE_METRES = 1.5; // lf-fixed: same latency slack as the single-prefab confirm
	//! How far from a piece a player may be to pick the set up.
	static const float PICKUP_REACH = 4; // lf-fixed: arm's length plus latency slack, not a gameplay knob

	protected static ref array<ref LF_PlacedComposition> s_aPlaced = {};
	protected static string s_sLastRefusal;

	//------------------------------------------------------------------------------------------------
	static string LastRefusal()
	{
		return s_sLastRefusal;
	}

	//------------------------------------------------------------------------------------------------
	//! How many placed sets this machine remembers.
	static int Count()
	{
		return s_aPlaced.Count();
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_aPlaced.Clear();
		s_sLastRefusal = "";
	}

	//------------------------------------------------------------------------------------------------
	//! A world transform at `spot`'s origin, heading `spot[2]` (horizontal), for one piece of the set:
	//! the offset across and ahead, and the piece's own turn. Level; Plan puts it on the ground.
	static void PieceSpot(LF_PlacementPiece piece, vector spot[4], out vector transform[4])
	{
		vector forward = LF_PlacementGeometry.Flat(spot[2]);
		if (forward == vector.Zero)
			forward = "0 0 1";

		vector right = Vector(forward[2], 0, -forward[0]);
		vector centre = spot[3] + right * piece.m_vOffset[0] + forward * piece.m_vOffset[2];
		vector heading = LF_PlacementGeometry.HeadingOf(LF_PlacementGeometry.YawOf(forward) + piece.m_fYaw);
		LF_PlacementGeometry.Transform(centre - heading * 0.5, centre + heading * 0.5, transform);
	}

	//------------------------------------------------------------------------------------------------
	//! Server and client: the set at `spot` (origin and horizontal heading). Every piece is probed
	//! like a single placement (ground at its two ends and middle, the step between them, the box
	//! over it, what it would lie on) and put on the ground in the plan. The plan's verdict is the
	//! first piece verdict that is not VALID, so one piece on a kerb makes the whole set TOO_STEEP.
	//! `ignore` (the carried item) stays out of the traces.
	static LF_EPlacementVerdict Plan(IEntity player, notnull LF_PlacementComposition composition, IEntity ignore, vector spot[4], notnull LF_PlacementCompositionPlan plan)
	{
		plan.m_aVerdicts.Clear();
		plan.m_aTransforms.Clear();
		plan.m_sReason = "";
		plan.m_eVerdict = LF_EPlacementVerdict.VALID;
		if (composition.Count() == 0)
		{
			plan.m_eVerdict = LF_EPlacementVerdict.NO_GROUND;
			plan.m_sReason = "the composition has no pieces";
			return plan.m_eVerdict;
		}

		array<IEntity> exclude = {};
		if (player)
			exclude.Insert(player);
		if (ignore)
			exclude.Insert(ignore);

		foreach (LF_PlacementPiece piece : composition.m_aPieces)
		{
			vector level[4];
			PieceSpot(piece, spot, level);
			vector transform[4];
			LF_PlacementProbe probe = new LF_PlacementProbe();
			LF_EPlacementVerdict verdict = ProbePiece(piece, level, exclude, transform, probe);
			plan.Add(verdict, transform);
			if (verdict != LF_EPlacementVerdict.VALID && plan.m_eVerdict == LF_EPlacementVerdict.VALID)
			{
				plan.m_eVerdict = verdict;
				plan.m_sReason = string.Format("piece %1 (%2): %3", plan.Count(), FilePath.StripExtension(FilePath.StripPath(piece.m_sPrefab)), LF_PlacementRules.Reason(verdict, probe, LF_PlacementService.MAX_STEP));
			}
		}

		LF_Debug.Log(LF_PlacementRules.FEATURE, string.Format("composition %1 at %2: %3 pieces, %4", composition.m_sName, spot[3], plan.Count(), LF_PlacementRules.Name(plan.m_eVerdict)));
		return plan.m_eVerdict;
	}

	//------------------------------------------------------------------------------------------------
	//! The probes at one piece's level spot: the same measurements as LF_PlacementService.ProbeAt
	//! (its ground sampler is protected, so this file keeps its own small copy).
	protected static LF_EPlacementVerdict ProbePiece(LF_PlacementPiece piece, vector spot[4], array<IEntity> exclude, out vector transform[4], notnull LF_PlacementProbe probe)
	{
		vector axis = LF_PlacementGeometry.Flat(spot[2]);
		if (axis == vector.Zero)
			axis = "0 0 1";

		vector centre = spot[3];
		vector start = centre + axis * piece.m_vMins[2];
		vector end = centre + axis * piece.m_vMaxs[2];
		LF_PlacementGeometry.Transform(centre - axis * 0.5, centre + axis * 0.5, transform);

		float startY;
		float endY;
		float middleY;
		IEntity startSurface;
		IEntity endSurface;
		IEntity middleSurface;
		probe.m_bGround = GroundY(start, exclude, startY, startSurface) && GroundY(end, exclude, endY, endSurface) && GroundY(centre, exclude, middleY, middleSurface);
		if (!probe.m_bGround)
			return LF_PlacementRules.Judge(probe, LF_PlacementService.MAX_STEP);

		probe.m_fLowest = Math.Min(startY, Math.Min(endY, middleY));
		probe.m_fHighest = Math.Max(startY, Math.Max(endY, middleY));
		start[1] = startY;
		end[1] = endY;
		LF_PlacementGeometry.Transform(start, end, transform);
		vector placed = start - transform[2] * piece.m_vMins[2];
		placed[1] = Math.Max(placed[1], middleY);
		transform[3] = placed;

		probe.m_sBlocker = LF_PlacementRules.FindBlocker(transform, piece.m_vMins, piece.m_vMaxs, exclude);
		array<IEntity> surfaces = {startSurface, middleSurface, endSurface};
		foreach (IEntity surface : surfaces)
		{
			if (probe.m_sBlocker.IsEmpty())
				probe.m_sBlocker = LF_PlacementRules.SurfaceBlocker(surface, null);
		}

		return LF_PlacementRules.Judge(probe, LF_PlacementService.MAX_STEP);
	}

	//------------------------------------------------------------------------------------------------
	//! The higher of the terrain and whatever stands on it under pos; `surface` is that entity.
	protected static bool GroundY(vector pos, array<IEntity> exclude, out float y, out IEntity surface)
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return false;

		y = world.GetSurfaceY(pos[0], pos[2]);
		TraceParam trace = new TraceParam();
		trace.Start = Vector(pos[0], pos[1] + 2, pos[2]);
		trace.End = Vector(pos[0], pos[1] - 3, pos[2]);
		trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
		trace.LayerMask = EPhysicsLayerPresets.Projectile;
		trace.ExcludeArray = exclude;
		float fraction = world.TraceMove(trace, null);
		surface = null;
		if (fraction < 1)
		{
			float hitY = trace.Start[1] + (trace.End[1] - trace.Start[1]) * fraction;
			if (hitY > y)
			{
				y = hitY;
				surface = trace.TraceEnt;
			}
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: `player` places the set at `proposal`, the spot their ghost showed (origin and horizontal
	//! heading). Refused with the reason logged (and toasted) unless they may place it, the proposal is
	//! well formed and within reach of where they stand, and every piece probes VALID there. On
	//! success every piece stands on the ground; if one fails to spawn none stays.
	//! \return the placed set, or null.
	static LF_PlacedComposition Confirm(IEntity player, notnull LF_PlacementComposition composition, vector proposal[4], IEntity ignore = null)
	{
		if (!Replication.IsServer())
			return Refuse(player, "refused: not the server", "");

		if (!player)
			return Refuse(player, "refused: no player", "");

		if (composition.Count() == 0)
			return Refuse(player, "refused: the composition has no pieces", "");

		string toastKey;
		string why = LF_PlacementPolicy.Check(player, composition.m_sName, composition.m_eWho, toastKey);
		if (!why.IsEmpty())
			return Refuse(player, string.Format("refused: %1 %2", LF_PoliceRanks.ResolveUid(player), why), toastKey);

		// Before the probes, so a flood of proposals costs no traces.
		if (!LF_RateLimit.AllowCharacter(player, LF_ERequestKind.SPAWN, "composition confirm"))
		{
			s_sLastRefusal = "refused: rate limit";
			return null;
		}

		string problem = LF_PlacementGeometry.ProposalProblem(proposal);
		if (!problem.IsEmpty())
			return Refuse(player, "refused: malformed proposal: " + problem, "");

		string offBy;
		vector playerTransform[4];
		player.GetWorldTransform(playerTransform);
		if (!LF_PlacementGeometry.WithinReach(playerTransform[3], proposal, composition.m_fReach + TOLERANCE_METRES, LF_PlacementService.MAX_RISE, offBy))
			return Refuse(player, "refused: the proposal is " + offBy, "#LF-Placement_Moved");

		LF_PlacementCompositionPlan plan = new LF_PlacementCompositionPlan();
		LF_EPlacementVerdict verdict = Plan(player, composition, ignore, proposal, plan);
		if (verdict == LF_EPlacementVerdict.BLOCKED)
			return Refuse(player, "refused: " + plan.m_sReason, "#LF-Placement_Blocked");

		if (verdict != LF_EPlacementVerdict.VALID)
			return Refuse(player, "refused: " + plan.m_sReason, "#LF-Placement_NoGround");

		LF_PlacedComposition placed = new LF_PlacedComposition();
		placed.m_sName = composition.m_sName;
		for (int i = 0; i < composition.Count(); i++)
		{
			vector transform[4];
			plan.GetTransform(i, transform);
			IEntity piece = SpawnAt(composition.m_aPieces[i].m_sPrefab, transform);
			if (!piece)
			{
				Remove(placed);
				return Refuse(player, string.Format("refused: spawn failed for %1", composition.m_aPieces[i].m_sPrefab), "");
			}

			placed.m_aIds.Insert(piece.GetID());
		}

		s_aPlaced.Insert(placed);
		LF_Debug.Info(LF_PlacementRules.FEATURE, string.Format("placed composition %1 (%2 pieces) for %3 at %4", composition.m_sName, placed.m_aIds.Count(), LF_PoliceRanks.ResolveUid(player), proposal[3]));
		return placed;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: `player` picks up the set `piece` belongs to: every piece is removed. Refused (logged)
	//! when they are further than PICKUP_REACH from that piece or the entity belongs to no set.
	//! \return how many pieces were removed (0 when refused).
	static int PickUp(IEntity player, IEntity piece)
	{
		if (!Replication.IsServer() || !player || !piece)
		{
			s_sLastRefusal = "pick-up refused: not the server, or no player or piece";
			LF_Debug.Warn(LF_PlacementRules.FEATURE, s_sLastRefusal);
			return 0;
		}

		LF_PlacedComposition placed = Find(piece);
		if (!placed)
		{
			s_sLastRefusal = string.Format("pick-up refused: %1 belongs to no placed composition", LF_PlacementRules.Describe(piece));
			LF_Debug.Warn(LF_PlacementRules.FEATURE, s_sLastRefusal);
			return 0;
		}

		vector playerTransform[4];
		player.GetWorldTransform(playerTransform);
		float distance = vector.Distance(playerTransform[3], piece.GetOrigin());
		if (distance > PICKUP_REACH)
		{
			s_sLastRefusal = string.Format("pick-up refused: the piece is %1 m from the player (most %2)", distance, PICKUP_REACH);
			LF_Debug.Warn(LF_PlacementRules.FEATURE, s_sLastRefusal);
			return 0;
		}

		int removed = Remove(placed);
		s_aPlaced.RemoveItem(placed);
		LF_Debug.Info(LF_PlacementRules.FEATURE, string.Format("picked up composition %1: %2 pieces removed", placed.m_sName, removed));
		return removed;
	}

	//------------------------------------------------------------------------------------------------
	//! The placed set `piece` belongs to, or null.
	static LF_PlacedComposition Find(IEntity piece)
	{
		if (!piece)
			return null;

		EntityID id = piece.GetID();
		foreach (LF_PlacedComposition placed : s_aPlaced)
		{
			if (placed.m_aIds.Find(id) != -1)
				return placed;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Deletes every piece of `placed` still in the world; returns how many it deleted.
	protected static int Remove(LF_PlacedComposition placed)
	{
		BaseWorld world = GetGame().GetWorld();
		int removed = 0;
		foreach (EntityID id : placed.m_aIds)
		{
			IEntity piece;
			if (world)
				piece = world.FindEntityByID(id);

			if (!piece)
				continue;

			SCR_EntityHelper.DeleteEntityAndChildren(piece);
			removed++;
		}

		return removed;
	}

	//------------------------------------------------------------------------------------------------
	protected static IEntity SpawnAt(ResourceName prefab, vector transform[4])
	{
		Resource resource = Resource.Load(prefab);
		if (!resource || !resource.IsValid())
		{
			LF_Debug.Error(LF_PlacementRules.FEATURE, string.Format("prefab does not resolve: %1", prefab));
			return null;
		}

		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		for (int i = 0; i < 4; i++)
			params.Transform[i] = transform[i];

		return GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), params);
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_PlacedComposition Refuse(IEntity player, string message, string toastKey)
	{
		s_sLastRefusal = message;
		LF_Debug.Warn(LF_PlacementRules.FEATURE, message);
		if (toastKey.IsEmpty() || !player || !GetGame() || !GetGame().GetPlayerManager())
			return null;

		int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(player);
		if (playerId > 0)
			LF_NotificationManagerComponent.NotifyPlayer(playerId, "#LF-Placement_Title", toastKey, 3.0, LF_ENotificationType.WARNING);

		return null;
	}
}

//------------------------------------------------------------------------------------------------
//! Client: one ghost for a whole composition, built from its pieces: one local mesh entity per piece
//! in Conflict's build colours (LF_PlacementGhost's materials), moved together to the plan's piece
//! transforms, green when the plan is VALID and red when not. Never replicated.
class LF_PlacementCompositionGhost
{
	protected static ref array<EntityID> s_aGhosts = {};
	protected static string s_sName;
	protected static bool s_bValidShown;

	//------------------------------------------------------------------------------------------------
	//! Shows (or moves) the ghost of `composition` at `plan`'s piece transforms.
	//! \return false with an error logged when a piece has no mesh to show.
	static bool Show(notnull LF_PlacementComposition composition, notnull LF_PlacementCompositionPlan plan)
	{
		if (plan.Count() != composition.Count())
			return false;

		bool valid = plan.m_eVerdict == LF_EPlacementVerdict.VALID;
		if (Ghosts().Count() != composition.Count() || s_sName != composition.m_sName)
		{
			Hide();
			if (!Spawn(composition))
			{
				Hide();
				return false;
			}

			s_bValidShown = !valid;
		}

		array<IEntity> ghosts = Ghosts();
		if (ghosts.Count() != composition.Count())
		{
			Hide();
			return false;
		}

		for (int i = 0; i < ghosts.Count(); i++)
		{
			vector transform[4];
			plan.GetTransform(i, transform);
			ghosts[i].SetWorldTransform(transform);
			ghosts[i].Update();
		}

		if (valid != s_bValidShown)
		{
			ResourceName material = LF_PlacementGhost.INVALID_MATERIAL;
			if (valid)
				material = LF_PlacementGhost.VALID_MATERIAL;

			foreach (IEntity ghost : ghosts)
				SCR_Global.SetMaterial(ghost, material);

			s_bValidShown = valid;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	static int Count()
	{
		return Ghosts().Count();
	}

	//------------------------------------------------------------------------------------------------
	//! The ghost entities still in the world, by id (a ghost can be deleted under us).
	protected static array<IEntity> Ghosts()
	{
		array<IEntity> ghosts = {};
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return ghosts;

		foreach (EntityID id : s_aGhosts)
		{
			IEntity ghost = world.FindEntityByID(id);
			if (ghost)
				ghosts.Insert(ghost);
		}

		return ghosts;
	}

	//------------------------------------------------------------------------------------------------
	static void Hide()
	{
		foreach (IEntity ghost : Ghosts())
			SCR_EntityHelper.DeleteEntityAndChildren(ghost);

		s_aGhosts.Clear();
		s_sName = "";
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		Hide();
	}

	//------------------------------------------------------------------------------------------------
	protected static bool Spawn(LF_PlacementComposition composition)
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return false;

		foreach (LF_PlacementPiece piece : composition.m_aPieces)
		{
			BaseContainer meshComponent = LF_Utils.GetPrefabMeshComponent(piece.m_sPrefab);
			ResourceName meshPath;
			if (meshComponent)
				meshComponent.Get("Object", meshPath);

			Resource mesh;
			if (!meshPath.IsEmpty())
				mesh = Resource.Load(meshPath);

			if (!mesh || !mesh.IsValid())
			{
				LF_Debug.Error(LF_PlacementRules.FEATURE, string.Format("composition ghost skipped: %1 has no mesh to show", piece.m_sPrefab));
				return false;
			}

			IEntity ghost = GetGame().SpawnEntity(GenericEntity, world);
			if (!ghost)
			{
				LF_Debug.Error(LF_PlacementRules.FEATURE, "composition ghost skipped: the entity did not spawn");
				return false;
			}

			ghost.SetObject(mesh.GetResource().ToVObject(), "");
			s_aGhosts.Insert(ghost.GetID());
		}

		s_sName = composition.m_sName;
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_PlacementCompositions's statics (registered at script load; Core/LF_StaticReset.c).
class LF_PlacementCompositions_StaticResetHook : LF_StaticResetHook
{
	protected static bool s_bRegistered = LF_StaticReset.Register(new LF_PlacementCompositions_StaticResetHook());

	override string Owner()
	{
		return "LF_PlacementCompositions";
	}

	override void Apply()
	{
		LF_PlacementCompositions.ResetStatics();
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_PlacementCompositionGhost's statics (registered at script load; Core/LF_StaticReset.c).
class LF_PlacementCompositionGhost_StaticResetHook : LF_StaticResetHook
{
	protected static bool s_bRegistered = LF_StaticReset.Register(new LF_PlacementCompositionGhost_StaticResetHook());

	override string Owner()
	{
		return "LF_PlacementCompositionGhost";
	}

	override void Apply()
	{
		LF_PlacementCompositionGhost.ResetStatics();
	}
}
