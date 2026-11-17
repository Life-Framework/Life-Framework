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
}

//------------------------------------------------------------------------------------------------
class LF_TownReachHarness
{
	static const string FEATURE = "TownReach";
	static const ResourceName WALKER_PREFAB = "{9B443DD4C1F6DCAD}Prefabs/Characters/Factions/CIV/Character_CIV_baseLoadout.et";
	static const int TICK_MS = 500; // lf-fixed: harness poll step
	static const int MAX_TICKS = 24; // lf-fixed: harness wait budget (12 s)
	static const float DEFAULT_RATIO = 3; // lf-fixed: the pipeline bar (everon-world.md 3.5), not a server setting
	static const float SNAP_HALF_M = 3;

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

		run.m_fStraight = vector.DistanceXZ(run.m_vFrom, run.m_vTo);

		AIGroup group = AIGroup.Cast(LF_Utils.SpawnEntityPrefab(LF_EventSceneWalk.GROUP_PREFAB, run.m_vFrom));
		if (!group)
			return "refused: the AI group did not spawn";

		run.m_Group = group.GetID();
		// Both ends must be on the navmesh: a point inside a closed building has no navmesh under it.
		AIPathfindingComponent pathfinding = AIPathfindingComponent.Cast(group.FindComponent(AIPathfindingComponent));
		if (!pathfinding)
		{
			Cleanup(run);
			return "refused: the AI group has no pathfinding component";
		}

		vector extents = Vector(SNAP_HALF_M, SNAP_HALF_M, SNAP_HALF_M);
		vector from;
		vector to;
		if (!pathfinding.GetClosestPositionOnNavmesh(run.m_vFrom, extents, from))
		{
			Cleanup(run);
			return Verdict(false, run, "from " + run.m_vFrom + " is off the navmesh (inside a closed building or open sea)");
		}

		if (!pathfinding.GetClosestPositionOnNavmesh(run.m_vTo, extents, to))
		{
			Cleanup(run);
			return Verdict(false, run, "to " + run.m_vTo + " is off the navmesh (inside a closed building or open sea)");
		}

		IEntity person = LF_Utils.SpawnEntityPrefab(WALKER_PREFAB, from);
		AIWaypoint waypoint = AIWaypoint.Cast(LF_Utils.SpawnEntityPrefab(LF_EventSceneWalk.MOVE_WAYPOINT_PREFAB, to));
		if (!person || !waypoint)
		{
			if (person)
				SCR_EntityHelper.DeleteEntityAndChildren(person);

			Cleanup(run);
			return "refused: the walker or the waypoint did not spawn";
		}

		run.m_Person = person.GetID();
		run.m_Waypoint = waypoint.GetID();
		waypoint.SetCompletionRadius(LF_EventSceneWalk.ARRIVE_M);
		AIControlComponent control = AIControlComponent.Cast(person.FindComponent(AIControlComponent));
		if (!control || !control.GetControlAIAgent())
		{
			Cleanup(run);
			return "refused: the walker has no AI agent";
		}

		control.ActivateAI();
		group.AddAgent(control.GetControlAIAgent());
		group.AddWaypoint(waypoint);

		s_Run = run;
		GetGame().GetCallqueue().CallLater(Tick, TICK_MS, true);
		LF_Debug.Info(FEATURE, string.Format("townreach started: %1 to %2, %3 m straight, ratio %4", from, to, run.m_fStraight, run.m_fRatio));
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Sum of the legs of the path the group's movement holds now; -1 when it holds none.
	protected static float PathLength(notnull LF_TownReachRun run)
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return -1;

		IEntity group = world.FindEntityByID(run.m_Group);
		if (!group)
			return -1;

		AIBaseMovementComponent movement = AIBaseMovementComponent.Cast(group.FindComponent(AIBaseMovementComponent));
		if (!movement)
			return -1;

		array<vector> points = {};
		movement.GetCurrentPath(points);
		if (points.Count() < 2)
			return -1;

		float length = 0;
		for (int i = 1; i < points.Count(); i++)
			length += vector.Distance(points[i - 1], points[i]);

		return length;
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
		float length = PathLength(s_Run);
		if (length < 0 && s_Run.m_iTicks < MAX_TICKS)
			return;

		string detail;
		bool pass = false;
		if (length < 0)
			detail = string.Format("no path after %1 s (%2 m straight)", s_Run.m_iTicks * TICK_MS / 1000, s_Run.m_fStraight);
		else if (length > s_Run.m_fRatio * Math.Max(s_Run.m_fStraight, 1))
			detail = string.Format("path %1 m is over %2 times the %3 m straight line", length, s_Run.m_fRatio, s_Run.m_fStraight);
		else
		{
			pass = true;
			detail = string.Format("path %1 m, straight %2 m", length, s_Run.m_fStraight);
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
