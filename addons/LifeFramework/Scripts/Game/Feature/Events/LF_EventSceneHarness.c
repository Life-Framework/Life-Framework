//------------------------------------------------------------------------------------------------
//! Starts an event scene from a fixture conf in a session (#2173). LF_EventOverlay reads only the
//! core conf and the pack overlays, so an event that ships off (`m_bEnabled 0`) could not run in a
//! proof without a pack patch. `Start` reloads that list, lays the fixture (root class
//! LF_EventsConfig, the same overlay rules as a pack's) over it, hands the result to the shell and the
//! module loader, and optionally forces the hour the shell's conditions read. `Stop` puts the
//! files' own list back. Harness only: the verb below is dev gated on the server.
class LF_EventSceneHarness
{
	static const string FEATURE = "Events";
	static const string REFUSED_NO_CONF = "refused: no fixture conf named";
	static const string REFUSED_NOT_LOADED = "refused: the fixture conf does not load";
	static const string REFUSED_WRONG_ROOT = "refused: the fixture is not an LF_EventsConfig";

	//! The control walker stands this far behind the spawn marker, clear of the scene's line.
	static const float CONTROL_BEHIND_M = 3;

	protected static ResourceName s_sFixture;
	protected static EntityID s_ControlId = EntityID.INVALID;
	protected static vector s_vControlTarget;

	//------------------------------------------------------------------------------------------------
	//! Server: applies `fixture` over the event list and runs it. `hour` >= 0 forces the shell's hour.
	//! \return "ok: ..." or one of the REFUSED_* lines (logged).
	static string Start(ResourceName fixture, float hour = -1)
	{
		if (fixture.IsEmpty())
		{
			LF_Debug.Error(FEATURE, "eventscene: " + REFUSED_NO_CONF);
			return REFUSED_NO_CONF;
		}

		Resource resource = BaseContainerTools.LoadContainer(fixture);
		if (!resource || !resource.IsValid() || !resource.GetResource())
		{
			LF_Debug.Error(FEATURE, string.Format("eventscene: fixture %1 does not load; nothing changed", fixture));
			return REFUSED_NOT_LOADED;
		}

		BaseContainer root = resource.GetResource().ToBaseContainer();
		if (!root || root.GetClassName() != "LF_EventsConfig")
		{
			LF_Debug.Error(FEATURE, string.Format("eventscene: fixture %1 is not an LF_EventsConfig; nothing changed", fixture));
			return REFUSED_WRONG_ROOT;
		}

		LF_EventOverlay.Reload();
		LF_EventsConfig config = LF_EventOverlay.Get();
		if (!config)
			return "refused: the event config does not load";

		int changed = LF_EventOverlay.ApplyOverlay(config, root, fixture);
		LF_EventShell.Use(config);
		int modules = LF_EventModuleLoader.RegisterFrom(config);
		if (hour >= 0)
			LF_EventShell.ForceHour(hour);
		LF_EventShell.StartIfServer();
		s_sFixture = fixture;

		string line = string.Format("fixture %1 applied: %2 event(s) changed, %3 running, %4 module(s), hour %5", fixture, changed, LF_EventShell.RunCount(), modules, hour);
		LF_Debug.Info(FEATURE, "eventscene: " + line);
		return "ok: " + line;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: back to the files' own event list and the real clock. \return a one-line summary.
	static string Stop()
	{
		LF_EventOverlay.Reload();
		LF_EventsConfig config = LF_EventOverlay.Get();
		LF_EventShell.Use(config);
		LF_EventModuleLoader.RegisterFrom(config);
		LF_EventShell.ForceHour(-1);
		s_sFixture = "";
		LF_Debug.Info(FEATURE, string.Format("eventscene: fixture dropped, %1 event(s) running", LF_EventShell.RunCount()));
		return string.Format("ok: fixture dropped, %1 running", LF_EventShell.RunCount());
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the running scene's crew and walk, and `eventId`'s phase when one is named.
	static string Status(string eventId = "")
	{
		string phase = "";
		if (!eventId.IsEmpty())
			phase = string.Format(" event %1 phase '%2';", eventId, LF_EventShell.PhaseOf(eventId));

		string head = string.Format("fixture '%1';%2", s_sFixture, phase);
		LF_EventSceneCrew crew = LF_EventSceneSpawn.Running();
		if (!crew)
			return head + " no scene running";

		string line = string.Format("%1 scene %2: %3 spawned", head, crew.m_sSceneId, crew.m_aPeople.Count());
		LF_EventSceneWalk walk = LF_EventSceneWalk.Active();
		if (!walk || walk.m_Crew != crew)
		{
			if (!LF_EventSceneWalk.LastOver().IsEmpty())
				return string.Format("%1, not walking (walk over: %2)", line, LF_EventSceneWalk.LastOver());

			return line + ", not walking";
		}

		int walking = walk.m_aWalkers.Count() - walk.CountArrived() - walk.CountTimedOut();
		line = string.Format("%1, walking %2, arrived %3, timed out %4, %5 s", line, walking, walk.CountArrived(), walk.CountTimedOut(), walk.m_fElapsed);
		return line + AiText(walk) + ControlText();
	}

	//------------------------------------------------------------------------------------------------
	//! The walk's AI as the engine has it: the group's agents, waypoint, activation and LOD, and the
	//! first walker's agent and position. A walk whose walkers stand still is read from here.
	static string AiText(notnull LF_EventSceneWalk walk)
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return "";

		AIGroup group = AIGroup.Cast(world.FindEntityByID(walk.m_GroupId));
		if (!group)
			return "; ai: no group";

		string text = string.Format("; ai: group agents=%1 waypoint=%2 active=%3 lod=%4", group.GetAgentsCount(), group.GetCurrentWaypoint() != null, group.IsAIActivated(), group.GetLOD());
		ChimeraAIWorld aiWorld = ChimeraAIWorld.Cast(GetGame().GetAIWorld());
		if (aiWorld)
			text += string.Format(" dormancy=%1", aiWorld.IsGroupDormancyEnabled());

		foreach (LF_EventSceneWalker walker : walk.m_aWalkers)
		{
			IEntity person = world.FindEntityByID(walker.m_Id);
			if (!person)
				continue;

			AIControlComponent control = AIControlComponent.Cast(person.FindComponent(AIControlComponent));
			AIAgent agent;
			if (control)
				agent = control.GetControlAIAgent();
			if (agent)
				text += string.Format(" | walker at %1 agent active=%2 lod=%3 inGroup=%4", person.GetOrigin(), agent.IsAIActivated(), agent.GetLOD(), agent.GetParentGroup() == group);
			else
				text += string.Format(" | walker at %1 no agent", person.GetOrigin());
			break;
		}

		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! Control for a walk that does not move (#2174): spawns `prefab` (a stock vanilla civilian) beside
	//! the running scene's first spawn marker, in its own vanilla group with one move waypoint on the
	//! scene target, the same spawn and group calls the walk uses. Status reports how far it got.
	static string Control(ResourceName prefab)
	{
		LF_EventSceneCrew crew = LF_EventSceneSpawn.Running();
		if (!crew || prefab.IsEmpty())
			return "refused: control needs a running scene and a {GUID} prefab";

		array<IEntity> spawns = {};
		IEntity target = LF_EventWaypointComponent.FindTarget(crew.m_sSceneId);
		if (LF_EventWaypointComponent.FindSpawns(crew.m_sSceneId, spawns) == 0 || !target)
			return "refused: the scene has no spawn marker or target placed";

		vector transform[4];
		spawns[0].GetWorldTransform(transform);
		IEntity person = LF_Utils.SpawnEntityPrefab(prefab, transform[3] - transform[2] * CONTROL_BEHIND_M, spawns[0].GetYawPitchRoll());
		if (!person)
			return "refused: the control prefab did not spawn";

		AIGroup group = AIGroup.Cast(LF_Utils.SpawnEntityPrefab(LF_EventSceneWalk.GROUP_PREFAB, person.GetOrigin()));
		AIWaypoint waypoint = AIWaypoint.Cast(LF_Utils.SpawnEntityPrefab(LF_EventSceneWalk.MOVE_WAYPOINT_PREFAB, target.GetOrigin()));
		if (!group || !waypoint)
			return "refused: the control's group or waypoint did not spawn";

		waypoint.SetCompletionRadius(LF_EventSceneWalk.ARRIVE_M);
		group.AddWaypoint(waypoint);
		AIControlComponent control = AIControlComponent.Cast(person.FindComponent(AIControlComponent));
		if (!control || !control.GetControlAIAgent())
			return "refused: the control has no AI agent";

		control.ActivateAI();
		group.AddAgent(control.GetControlAIAgent());
		s_ControlId = person.GetID();
		s_vControlTarget = target.GetOrigin();
		string line = string.Format("control %1 spawned at %2, %3 m from the target", prefab, person.GetOrigin(), vector.DistanceXZ(person.GetOrigin(), s_vControlTarget));
		LF_Debug.Info(FEATURE, "eventscene: " + line);
		return "ok: " + line;
	}

	//------------------------------------------------------------------------------------------------
	//! The control's distance to the target, "" when none was spawned.
	static string ControlText()
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world || s_ControlId == EntityID.INVALID)
			return "";

		IEntity person = world.FindEntityByID(s_ControlId);
		if (!person)
			return "; control gone";

		return string.Format("; control at %1, %2 m from the target", person.GetOrigin(), vector.DistanceXZ(person.GetOrigin(), s_vControlTarget));
	}

	//------------------------------------------------------------------------------------------------
	static ResourceName Fixture()
	{
		return s_sFixture;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_sFixture = "";
		s_ControlId = EntityID.INVALID;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_EventSceneHarness's statics (Core/LF_StaticReset.c).
class LF_EventSceneHarness_StaticResetHook : LF_StaticResetHook
{
	protected static bool s_bRegistered = LF_StaticReset.Register(new LF_EventSceneHarness_StaticResetHook());

	override string Owner()
	{
		return "LF_EventSceneHarness";
	}

	override void Apply()
	{
		LF_EventSceneHarness.ResetStatics();
	}
}

//------------------------------------------------------------------------------------------------
//! Harness `eventscene start <{GUID}fixture.conf> [hour]` | `eventscene status [eventId]` |
//! `eventscene control <{GUID}prefab>` | `eventscene stop`. The server runs each under the dev gate (as `eventjip` does).
modded class LF_AgentHarnessVerbs
{
	override bool Handle(LF_AgentCommand cmd)
	{
		if (cmd.Verb != "eventscene")
			return super.Handle(cmd);

		SCR_ChimeraCharacter character = LF_Utils.GetLocalCharacter();
		bool known = cmd.Arg1 == "start" || cmd.Arg1 == "status" || cmd.Arg1 == "stop" || cmd.Arg1 == "control";
		if (!known || !character)
		{
			LF_AgentHarness.Answer(cmd.Id, "error", "usage: eventscene start <{GUID}fixture.conf> [hour] | status [eventId] | control <{GUID}prefab> | stop (needs a character)");
			return true;
		}

		float hour = -1;
		if (cmd.Arg1 == "start" && !cmd.Arg3.IsEmpty())
			hour = cmd.Arg3.ToFloat();

		character.LF_AskHarnessEventScene(cmd.Id, cmd.Arg1, cmd.Arg2, hour);
		return true;
	}
}

//------------------------------------------------------------------------------------------------
modded class SCR_ChimeraCharacter
{
	//------------------------------------------------------------------------------------------------
	void LF_AskHarnessEventScene(int commandId, string action, string arg, float hour)
	{
		if (Replication.IsServer())
		{
			LF_HarnessReplyEventScene(commandId, action, arg, hour);
			return;
		}

		Rpc(RpcAsk_LF_HarnessEventScene, commandId, action, arg, hour);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_HarnessEventScene(int commandId, string action, string arg, float hour)
	{
		if (!LF_DevGate.CheckHarnessRequest(this, "eventscene"))
		{
			LF_HarnessReply(commandId, "error", LF_AgentHarness.REFUSED_DEV_GATE);
			return;
		}

		LF_HarnessReplyEventScene(commandId, action, arg, hour);
	}

	//------------------------------------------------------------------------------------------------
	protected void LF_HarnessReplyEventScene(int commandId, string action, string arg, float hour)
	{
		string detail;
		if (action == "start")
			detail = LF_EventSceneHarness.Start(arg, hour);
		else if (action == "stop")
			detail = LF_EventSceneHarness.Stop();
		else if (action == "control")
			detail = LF_EventSceneHarness.Control(arg);
		else
			detail = LF_EventSceneHarness.Status(arg);

		string status = "ok";
		if (detail.StartsWith("refused"))
			status = "error";
		LF_HarnessReply(commandId, status, detail);
	}
}
