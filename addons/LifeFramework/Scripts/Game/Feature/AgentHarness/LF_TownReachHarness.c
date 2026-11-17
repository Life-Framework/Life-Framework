// Town pipeline reachability proof (#2207, Everon world epic #2201, docs/architecture/everon-world.md 3.5).
//
// Harness verb `townreach <from> <to> [ratio]`: paths one vanilla civilian on the world's soldier navmesh
// (GM_Eden.nmn on the island) from `from` to `to` and answers `ok reach ...` or `error noreach ...`.
// A point is `<kind>:<id>` (an LF_Places entry, resolved here: this file holds no coordinates, no map, no
// entity or layer names) or `x,y,z` (a probe point the proof computed from a dossier).
// It fails when either end is off the navmesh (a point inside a closed building), when the group never
// gets a path, or when the path is longer than `ratio` (default 3) times the straight-line distance.
// The server runs it under the dev gate, as `eventscene` does.

//------------------------------------------------------------------------------------------------
class LF_TownReachRun
{
	int m_iCommandId;
	EntityID m_Asker = EntityID.INVALID;
	EntityID m_Group = EntityID.INVALID;
	EntityID m_Person = EntityID.INVALID;
	EntityID m_Waypoint = EntityID.INVALID;
	vector m_vFrom;
	vector m_vTo;
	float m_fRatio = 3;
	int m_iTicks;
	float m_fStraight;
	bool m_bWaitingForNavmesh;
	vector m_vLast;
	float m_fTravelled;
	float m_fStartDist;
}

//------------------------------------------------------------------------------------------------
class LF_TownReachHarness
{
	static const string FEATURE = "TownReach";
	static const ResourceName WALKER_PREFAB = "{9B443DD4C1F6DCAD}Prefabs/Characters/Factions/CIV/Character_CIV_baseLoadout.et";
	static const int TICK_MS = 500; // lf-fixed: harness poll step
	static const int MAX_TICKS = 24; // lf-fixed: harness wait budget (12 s)
	static const float DEFAULT_RATIO = 3; // lf-fixed: the pipeline bar (everon-world.md 3.5), not a server setting
	static const float SNAP_HALF_M = 1.5; // lf-fixed: a probe closer than this to the navmesh counts as on it
	static const float SNAP_HALF_Y_M = 3; // lf-fixed: ground height tolerance
	static const float MIN_WALK_M = 3; // lf-fixed: a walker that moved less than this found no path

	protected static ref LF_TownReachRun s_Run;

	//------------------------------------------------------------------------------------------------
	//! One point: `kind:id` through LF_Places, or `x,y,z`. False when it does not resolve.
	static bool ResolvePoint(string spec, out vector point)
	{
		array<string> parts = {};
		spec.Split(",", parts, true);
		if (parts.Count() == 3)
		{
			// A probe point states x and z; the ground height is the world's (a wrong y misses the navmesh).
			point = Vector(parts[0].ToFloat(), parts[1].ToFloat(), parts[2].ToFloat());
			BaseWorld world = GetGame().GetWorld();
			if (world)
				point[1] = world.GetSurfaceY(point[0], point[2]);

			return true;
		}

		parts.Clear();
		spec.Split(":", parts, true);
		if (parts.Count() != 2)
			return false;

		return LF_Places.FindOrigin(parts[0], parts[1], point);
	}

	//------------------------------------------------------------------------------------------------
	protected static bool OutsideBounds(vector point, vector mins, vector maxs)
	{
		return point[0] < mins[0] || point[0] > maxs[0] || point[2] < mins[2] || point[2] > maxs[2];
	}

	//------------------------------------------------------------------------------------------------
	//! Starts a run on the server. "" when it started, else the refusal (the caller answers it). The
	//! verdict arrives later through Finish.
	static string Start(IEntity asker, int commandId, string fromSpec, string toSpec, string ratioText)
	{
		if (s_Run)
			return "refused: a townreach run is already going";

		AIWorld aiWorld = GetGame().GetAIWorld();
		if (!aiWorld)
			return "refused: this world has no AIWorld (no navmesh)";

		LF_TownReachRun run = new LF_TownReachRun();
		run.m_iCommandId = commandId;
		if (asker)
			run.m_Asker = asker.GetID();

		if (!ResolvePoint(fromSpec, run.m_vFrom))
			return "refused: from '" + fromSpec + "' is neither kind:id in LF_Places nor x,y,z";

		if (!ResolvePoint(toSpec, run.m_vTo))
			return "refused: to '" + toSpec + "' is neither kind:id in LF_Places nor x,y,z";

		if (!ratioText.IsEmpty())
			run.m_fRatio = ratioText.ToFloat();

		if (run.m_fRatio <= 1)
			run.m_fRatio = DEFAULT_RATIO;

		// A point outside the world has no navmesh tile to wait for; refuse it here (the proof's off-map control).
		BaseWorld world = GetGame().GetWorld();
		if (world)
		{
			vector mins;
			vector maxs;
			world.GetBoundBox(mins, maxs);
			if (OutsideBounds(run.m_vFrom, mins, maxs))
				return "noreach from " + run.m_vFrom + " is off the navmesh (outside the world bounds)";

			if (OutsideBounds(run.m_vTo, mins, maxs))
				return "noreach to " + run.m_vTo + " is off the navmesh (outside the world bounds)";
		}

		run.m_fStraight = vector.DistanceXZ(run.m_vFrom, run.m_vTo);

		AIGroup group = AIGroup.Cast(LF_Utils.SpawnEntityPrefab(LF_EventSceneWalk.GROUP_PREFAB, run.m_vFrom));
		if (!group)
			return "refused: the AI group did not spawn";

		run.m_Group = group.GetID();
		if (!AIPathfindingComponent.Cast(group.FindComponent(AIPathfindingComponent)))
		{
			Cleanup(run);
			return "refused: the AI group has no pathfinding component";
		}

		// The island's soldier navmesh streams in by tile: a point far from every player and AI has no tile loaded yet,
		// which reads as "off the navmesh". Ask for both tiles, then poll for them (Tick, BeginWalk).
		aiWorld.RequestNavmeshLoad(run.m_vFrom);
		aiWorld.RequestNavmeshLoad(run.m_vTo);
		run.m_bWaitingForNavmesh = true;
		s_Run = run;
		GetGame().GetCallqueue().CallLater(Tick, TICK_MS, true);
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! A run ends before the walk: the verdict goes to the asker and the run's entities go.
	protected static void Abort(notnull LF_TownReachRun run, string line)
	{
		// s_Run holds the only strong reference: keep the run alive until the answer is sent.
		LF_TownReachRun keep = run;
		s_Run = null;
		GetGame().GetCallqueue().Remove(Tick);
		Cleanup(keep);
		Answer(keep, false, line);
	}

	//------------------------------------------------------------------------------------------------
	//! One poll of the navmesh wait: both ends snap to the navmesh, then the walker starts; an end whose tile is loaded and
	//! still does not snap is off the navmesh at once (a closed building); an end whose tile never loads fails at the budget.
	protected static void BeginWalk(notnull LF_TownReachRun run)
	{
		BaseWorld world = GetGame().GetWorld();
		IEntity group = null;
		if (world)
			group = world.FindEntityByID(run.m_Group);

		AIPathfindingComponent pathfinding = null;
		if (group)
			pathfinding = AIPathfindingComponent.Cast(group.FindComponent(AIPathfindingComponent));

		if (!pathfinding)
		{
			Abort(run, "refused: the AI group is gone");
			return;
		}

		vector extents = Vector(SNAP_HALF_M, SNAP_HALF_Y_M, SNAP_HALF_M);
		vector from;
		vector to;
		bool fromOk = pathfinding.GetClosestPositionOnNavmesh(run.m_vFrom, extents, from);
		bool toOk = pathfinding.GetClosestPositionOnNavmesh(run.m_vTo, extents, to);
		if (!fromOk || !toOk)
		{
			vector bad = run.m_vFrom;
			string which = "from ";
			if (fromOk)
			{
				bad = run.m_vTo;
				which = "to ";
			}

			NavmeshWorldComponent mesh = pathfinding.GetNavmeshComponent();
			bool tileLoaded;
			if (mesh)
				tileLoaded = mesh.IsTileLoaded(bad);

			if (!tileLoaded && run.m_iTicks < MAX_TICKS)
				return;

			string why = "the navmesh tile did not load in time";
			if (tileLoaded)
				why = "the tile is loaded, inside a closed building or open sea";

			Abort(run, "noreach " + which + bad + " is off the navmesh (" + why + ")");
			return;
		}

		IEntity person = LF_Utils.SpawnEntityPrefab(WALKER_PREFAB, from);
		AIWaypoint waypoint = AIWaypoint.Cast(LF_Utils.SpawnEntityPrefab(LF_EventSceneWalk.MOVE_WAYPOINT_PREFAB, to));
		if (!person || !waypoint)
		{
			if (person)
				SCR_EntityHelper.DeleteEntityAndChildren(person);

			Abort(run, "refused: the walker or the waypoint did not spawn");
			return;
		}

		run.m_Person = person.GetID();
		run.m_Waypoint = waypoint.GetID();
		waypoint.SetCompletionRadius(LF_EventSceneWalk.ARRIVE_M);
		AIControlComponent control = AIControlComponent.Cast(person.FindComponent(AIControlComponent));
		AIGroup aiGroup = AIGroup.Cast(group);
		if (!control || !control.GetControlAIAgent() || !aiGroup)
		{
			Abort(run, "refused: the walker has no AI agent");
			return;
		}

		control.ActivateAI();
		aiGroup.AddAgent(control.GetControlAIAgent());
		aiGroup.AddWaypoint(waypoint);

		run.m_bWaitingForNavmesh = false;
		run.m_iTicks = 0;
		run.m_vLast = from;
		run.m_fTravelled = 0;
		run.m_fStartDist = vector.DistanceXZ(from, to);
		LF_Debug.Info(FEATURE, string.Format("townreach started: %1 to %2, %3 m straight, ratio %4", from, to, run.m_fStraight, run.m_fRatio));
	}

	//------------------------------------------------------------------------------------------------
	//! Sum of the legs of the path one entity's movement component holds now; -1 when it holds none.
	protected static float PathLengthOf(IEntity entity, out int count)
	{
		count = 0;
		if (!entity)
			return -1;

		AIBaseMovementComponent movement = AIBaseMovementComponent.Cast(entity.FindComponent(AIBaseMovementComponent));
		if (!movement)
			return -1;

		array<vector> points = {};
		movement.GetCurrentPath(points);
		count = points.Count();
		if (count < 2)
			return -1;

		float length = 0;
		for (int i = 1; i < count; i++)
			length += vector.Distance(points[i - 1], points[i]);

		return length;
	}

	//------------------------------------------------------------------------------------------------
	//! The path the run's walker holds now, from the walker's own agent first, then its group; -1 when neither holds one.
	//! Every few ticks it logs what each held, so a "no path" verdict says which component was empty.
	protected static float PathLength(notnull LF_TownReachRun run)
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return -1;

		IEntity person = world.FindEntityByID(run.m_Person);
		IEntity agent;
		if (person)
		{
			AIControlComponent control = AIControlComponent.Cast(person.FindComponent(AIControlComponent));
			if (control)
				agent = control.GetControlAIAgent();
		}

		int agentPoints;
		int groupPoints;
		float agentLength = PathLengthOf(agent, agentPoints);
		float groupLength = PathLengthOf(world.FindEntityByID(run.m_Group), groupPoints);
		if (run.m_iTicks % 6 == 0)
		{
			float toTarget = -1;
			if (person)
				toTarget = vector.DistanceXZ(person.GetOrigin(), run.m_vTo);

			LF_Debug.Info(FEATURE, string.Format("townreach tick %1: agent %2 (%3 pts), group %4 (%5 pts), walker %6 m from the target", run.m_iTicks, agent != null, agentPoints, groupLength, groupPoints, toTarget));
		}

		if (agentLength >= 0)
			return agentLength;

		return groupLength;
	}

	//------------------------------------------------------------------------------------------------
	protected static void Tick()
	{
		if (!s_Run)
		{
			GetGame().GetCallqueue().Remove(Tick);
			return;
		}

		s_Run.m_iTicks++;
		if (s_Run.m_bWaitingForNavmesh)
		{
			BeginWalk(s_Run);
			return;
		}

		// The AI does not hold its path where a script can read it after the first moments (GetCurrentPath came back empty
		// on a walker that was making steady progress), so the walk itself is the proof: sample the walker, and compare the
		// distance it walked with the distance it closed.
		float held = PathLength(s_Run);
		BaseWorld world = GetGame().GetWorld();
		IEntity person;
		if (world)
			person = world.FindEntityByID(s_Run.m_Person);

		float toTarget = s_Run.m_fStraight;
		if (person)
		{
			vector pos = person.GetOrigin();
			s_Run.m_fTravelled += vector.DistanceXZ(pos, s_Run.m_vLast);
			s_Run.m_vLast = pos;
			toTarget = vector.DistanceXZ(pos, s_Run.m_vTo);
		}

		bool arrived = person && toTarget <= LF_EventSceneWalk.ARRIVE_M;
		if (!arrived && s_Run.m_iTicks < MAX_TICKS)
			return;

		float seconds = s_Run.m_iTicks * TICK_MS / 1000;
		float closed = s_Run.m_fStartDist - toTarget;
		float travelled = s_Run.m_fTravelled;
		string detail;
		bool pass = false;
		if (arrived)
		{
			if (travelled > s_Run.m_fRatio * Math.Max(s_Run.m_fStraight, 1))
				detail = string.Format("arrived, but the walk of %1 m is over %2 times the %3 m straight line", travelled, s_Run.m_fRatio, s_Run.m_fStraight);
			else
			{
				pass = true;
				detail = string.Format("arrived: walked %1 m, straight %2 m", travelled, s_Run.m_fStraight);
			}
		}
		else if (travelled < MIN_WALK_M)
			detail = string.Format("no path: the walker moved %1 m in %2 s (%3 m straight)", travelled, seconds, s_Run.m_fStraight);
		else if (closed <= 0 || travelled > s_Run.m_fRatio * closed)
			detail = string.Format("the walker detours: walked %1 m and closed %2 m of the %3 m straight line in %4 s (over %5 times)", travelled, closed, s_Run.m_fStraight, seconds, s_Run.m_fRatio);
		else
		{
			pass = true;
			detail = string.Format("path found: the walker closed %1 m of the %2 m straight line in %3 s, %4 m walked (held path %5 m)", closed, s_Run.m_fStraight, seconds, travelled, held);
		}

		LF_TownReachRun run = s_Run;
		s_Run = null;
		GetGame().GetCallqueue().Remove(Tick);
		string line = Verdict(pass, run, detail);
		Cleanup(run);
		Answer(run, pass, line);
	}

	//------------------------------------------------------------------------------------------------
	protected static string Verdict(bool pass, notnull LF_TownReachRun run, string detail)
	{
		string line;
		if (pass)
			line = "reach " + detail;
		else
			line = "noreach " + detail;

		LF_Debug.Info(FEATURE, "townreach: " + line);
		return line;
	}

	//------------------------------------------------------------------------------------------------
	//! The verdict to the asking character; an early failure answers from the RPC handler instead.
	static void Answer(notnull LF_TownReachRun run, bool pass, string line)
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;

		SCR_ChimeraCharacter asker = SCR_ChimeraCharacter.Cast(world.FindEntityByID(run.m_Asker));
		if (!asker)
			return;

		string status = "error";
		if (pass)
			status = "ok";

		asker.LF_HarnessReply(run.m_iCommandId, status, line);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Cleanup(notnull LF_TownReachRun run)
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;

		array<EntityID> ids = {run.m_Person, run.m_Waypoint, run.m_Group};
		foreach (EntityID id : ids)
		{
			IEntity entity = world.FindEntityByID(id);
			if (entity)
				SCR_EntityHelper.DeleteEntityAndChildren(entity);
		}
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_Run = null;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_TownReachHarness's statics (Core/LF_StaticReset.c).
class LF_TownReachHarness_StaticResetHook : LF_StaticResetHook
{
	protected static bool s_bRegistered = LF_StaticReset.Register(new LF_TownReachHarness_StaticResetHook());

	override string Owner()
	{
		return "LF_TownReachHarness";
	}

	override void Apply()
	{
		LF_TownReachHarness.ResetStatics();
	}
}

//------------------------------------------------------------------------------------------------
modded class LF_AgentHarnessVerbs
{
	override bool Handle(LF_AgentCommand cmd)
	{
		if (cmd.Verb != "townreach")
			return super.Handle(cmd);

		SCR_ChimeraCharacter character = LF_Utils.GetLocalCharacter();
		if (!character || cmd.Arg1.IsEmpty() || cmd.Arg2.IsEmpty())
		{
			LF_AgentHarness.Answer(cmd.Id, "error", "usage: townreach <kind:id|x,y,z> <kind:id|x,y,z> [ratio] (needs a character)");
			return true;
		}

		character.LF_AskHarnessTownReach(cmd.Id, cmd.Arg1, cmd.Arg2, cmd.Arg3);
		return true;
	}
}

//------------------------------------------------------------------------------------------------
modded class SCR_ChimeraCharacter
{
	//------------------------------------------------------------------------------------------------
	void LF_AskHarnessTownReach(int commandId, string fromSpec, string toSpec, string ratioText)
	{
		if (Replication.IsServer())
		{
			LF_HarnessReplyTownReach(commandId, fromSpec, toSpec, ratioText);
			return;
		}

		Rpc(RpcAsk_LF_HarnessTownReach, commandId, fromSpec, toSpec, ratioText);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_HarnessTownReach(int commandId, string fromSpec, string toSpec, string ratioText)
	{
		if (!LF_DevGate.CheckHarnessRequest(this, "townreach"))
		{
			LF_HarnessReply(commandId, "error", LF_AgentHarness.REFUSED_DEV_GATE);
			return;
		}

		LF_HarnessReplyTownReach(commandId, fromSpec, toSpec, ratioText);
	}

	//------------------------------------------------------------------------------------------------
	protected void LF_HarnessReplyTownReach(int commandId, string fromSpec, string toSpec, string ratioText)
	{
		string detail = LF_TownReachHarness.Start(this, commandId, fromSpec, toSpec, ratioText);
		if (detail.IsEmpty())
			return;

		// A refusal answers now; a started run answers from its tick; an off-navmesh end is a verdict.
		string status = "error";
		LF_HarnessReply(commandId, status, detail);
	}
}
