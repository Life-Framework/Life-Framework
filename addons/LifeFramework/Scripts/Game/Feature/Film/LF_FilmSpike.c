// LF_FilmSpike.c - research spike for #391 (video pipeline). Harness verb `spike`:
//   spike cam <x y z> <radius> <seconds>   client: orbit a manual camera around a point while the player stays controlled
//   spike ai                               server (via the player's character): AI civilian walks to the store till, performs the till action
// Dev-only (LF_DevGate on the server leg). Logs as [ELDebug:FilmSpike].

class LF_FilmSpike
{
	protected static const string FEATURE = "FilmSpike";
	protected static const ResourceName AI_CHARACTER = "{DCB41B3746FDD1BE}Prefabs/Characters/Factions/OPFOR/USSR_Army/Character_USSR_Rifleman.et";
	protected static const ResourceName AI_GROUP = "{000CD338713F2B5A}Prefabs/AI/Groups/Group_Base.et";
	protected static const ResourceName WAYPOINT = "{750A8D1695BD6998}Prefabs/AI/Waypoints/AIWaypoint_Move.et";

	protected static EntityID s_AiId = EntityID.INVALID;
	protected static EntityID s_TillId = EntityID.INVALID;
	protected static vector s_vTarget;
	protected static vector s_vLast;
	protected static float s_fStart;
	protected static bool s_bActed;
	protected static int s_iTicks;
	protected static AIGroup s_Group;

	//------------------------------------------------------------------------------------------------
	static string StartCamera(vector centre, float radius, float seconds)
	{
		LF_FilmCameraPath path = new LF_FilmCameraPath();
		int steps = 8;
		for (int i = 0; i <= steps; i++)
		{
			float a = i * Math.PI2 / steps;
			vector pos = centre + Vector(Math.Sin(a) * radius, 6, Math.Cos(a) * radius);
			path.Add(i * seconds / steps, pos, centre + Vector(0, 1.5, 0), 60);
		}

		if (!LF_FilmCameraRunner.Play(path))
			return "error:play_rejected";

		return "camera pass started";
	}

	//------------------------------------------------------------------------------------------------
	static string Start(string mode, vector near)
	{
		if (mode == "puppet")
			return StartPuppet(near, "AppleNpc");

		return StartAi(near);
	}

	//------------------------------------------------------------------------------------------------
	//! Server only.
	static string StartAi(vector near)
	{
		if (!Replication.IsServer())
			return "error:not_server";

		IEntity till = LF_ActionSite.FindNear(near, 80, "LF_StoreTillAction");
		if (!till)
			return "error:no_till_within_80m";

		s_TillId = till.GetID();
		s_vTarget = till.GetOrigin();
		vector spawn = s_vTarget + Vector(14, 0, 0);
		spawn[1] = GetGame().GetWorld().GetSurfaceY(spawn[0], spawn[2]);

		LF_Debug.Info(FEATURE, string.Format("AIWorld present in map: %1", GetGame().GetAIWorld() != null));
		if (!GetGame().GetAIWorld())
		{
			IEntity aiWorld = LF_Utils.SpawnEntityPrefab("Prefabs/AI/SCR_AIWorld.et", Vector(0, 0, 0));
			LF_Debug.Info(FEATURE, string.Format("spawned a runtime SCR_AIWorld: %1, now present: %2", aiWorld != null, GetGame().GetAIWorld() != null));
		}

		IEntity groupEnt = LF_Utils.SpawnEntityPrefab(AI_GROUP, spawn);
		s_Group = AIGroup.Cast(groupEnt);
		IEntity ai = LF_Utils.SpawnEntityPrefab(AI_CHARACTER, spawn);
		if (!s_Group || !ai)
		{
			LF_Debug.Error(FEATURE, string.Format("spawn failed group=%1 ai=%2", s_Group != null, ai != null));
			return "error:spawn_failed";
		}

		AIControlComponent control = AIControlComponent.Cast(ai.FindComponent(AIControlComponent));
		AIAgent agent;
		if (control)
			agent = control.GetControlAIAgent();
		LF_Debug.Info(FEATURE, string.Format("ai spawned at %1 control=%2 agent=%3", spawn, control != null, agent != null));
		if (!agent)
			return "error:no_ai_agent";

		s_Group.AddAgent(agent);
		IEntity wpEnt = LF_Utils.SpawnEntityPrefab(WAYPOINT, s_vTarget);
		AIWaypoint wp = AIWaypoint.Cast(wpEnt);
		if (!wp)
			return "error:no_waypoint";

		wp.SetCompletionRadius(2);
		s_Group.AddWaypoint(wp);
		s_AiId = ai.GetID();
		s_vLast = spawn;
		s_bActed = false;
		s_fStart = GetGame().GetWorld().GetWorldTime();
		GetGame().GetCallqueue().CallLater(AiTick, 1000, true);
		LF_Debug.Info(FEATURE, string.Format("waypoint set at %1", s_vTarget));
		return "ai walking to till";
	}

	//------------------------------------------------------------------------------------------------
	//! Server only: drive an existing character without AI or navmesh, through its CharacterControllerComponent.
	static string StartPuppet(vector near, string npcName)
	{
		if (!Replication.IsServer())
			return "error:not_server";

		IEntity till = LF_ActionSite.FindNear(near, 80, "LF_StoreTillAction");
		vector spot = Vector(near[0] + 14, near[1], near[2]);
		IEntity npc = LF_Utils.SpawnEntityPrefab("{9B5BB216CC7FF18E}Prefabs/Characters/Core/Character_Roleplay.et", spot);
		if (!till || !npc)
			return string.Format("error:till=%1 npc=%2", till != null, npc != null);

		s_TillId = till.GetID();
		s_AiId = npc.GetID();
		s_vTarget = till.GetOrigin();
		s_vLast = npc.GetOrigin();
		s_bActed = false;
		s_fStart = GetGame().GetWorld().GetWorldTime();
		AIControlComponent control = AIControlComponent.Cast(npc.FindComponent(AIControlComponent));
		LF_Debug.Info(FEATURE, string.Format("puppet %1 at %2 aiControl=%3 active=%4", npcName, npc.GetOrigin(), control != null, control && control.IsAIActivated()));
		GetGame().GetCallqueue().CallLater(PuppetTick, 100, true);
		return "puppet walking";
	}

	//------------------------------------------------------------------------------------------------
	protected static void PuppetTick()
	{
		BaseWorld world = GetGame().GetWorld();
		ChimeraCharacter npc = ChimeraCharacter.Cast(world.FindEntityByID(s_AiId));
		if (!npc)
		{
			LF_Debug.Error(FEATURE, string.Format("puppet is not a ChimeraCharacter: %1", world.FindEntityByID(s_AiId)));
			GetGame().GetCallqueue().Remove(PuppetTick);
			return;
		}

		CharacterControllerComponent controller = npc.GetCharacterController();
		if (!controller)
		{
			LF_Debug.Error(FEATURE, "puppet has no character controller");
			GetGame().GetCallqueue().Remove(PuppetTick);
			return;
		}

		vector pos = npc.GetOrigin();
		vector delta = s_vTarget - pos;
		delta[1] = 0;
		float dist = delta.Length();
		float t = (world.GetWorldTime() - s_fStart) / 1000.0;
		if (dist > 2.5 && t < 90)
		{
			controller.SetHeadingAngle(Math.Atan2(delta[0], delta[2]));
			controller.SetMovement(2, Vector(0, 0, 1));
			s_iTicks++;
			if (s_iTicks >= 10)
				{
				s_iTicks = 0;
				LF_Debug.Info(FEATURE, string.Format("puppet t=%1 pos=%2 dist=%3", t, pos, dist));
			}
			return;
		}

		controller.SetMovement(0, Vector(0, 0, 0));
		GetGame().GetCallqueue().Remove(PuppetTick);
		LF_Debug.Info(FEATURE, string.Format("puppet stopped t=%1 pos=%2 dist=%3", t, pos, dist));
		AiTick();
	}

	//------------------------------------------------------------------------------------------------
	protected static void AiTick()
	{
		BaseWorld world = GetGame().GetWorld();
		IEntity ai = world.FindEntityByID(s_AiId);
		IEntity till = world.FindEntityByID(s_TillId);
		if (!ai || !till)
		{
			LF_Debug.Error(FEATURE, "ai or till gone");
			GetGame().GetCallqueue().Remove(AiTick);
			return;
		}

		vector pos = ai.GetOrigin();
		float dist = vector.Distance(pos, s_vTarget);
		float t = (world.GetWorldTime() - s_fStart) / 1000.0;
		float step = vector.Distance(pos, s_vLast);
		s_vLast = pos;
		LF_Debug.Info(FEATURE, string.Format("t=%1 ai=%2 dist=%3 step=%4", t, pos, dist, step));
		if (s_bActed)
			return;

		if (dist > 3.5 && t < 120 && s_Group)
			return;

		s_bActed = true;
		BaseActionsManagerComponent manager = BaseActionsManagerComponent.Cast(till.FindComponent(BaseActionsManagerComponent));
		array<BaseUserAction> actions = {};
		if (manager)
			manager.GetActionsList(actions);
		foreach (BaseUserAction action : actions)
		{
			ScriptedUserAction scripted = ScriptedUserAction.Cast(action);
			if (!scripted)
				continue;

			bool can = scripted.CanBePerformedScript(ai);
			LF_Debug.Info(FEATURE, string.Format("action %1 canPerform=%2 uid='%3'", action.Type(), can, LF_Utils.GetPlayerUid(ai)));
			if (action.Type().ToString() == "LF_StoreTillAction")
			{
				scripted.PerformAction(till, ai);
				LF_Debug.Info(FEATURE, "till action performed by the AI on cue");
			}
		}

		GetGame().GetCallqueue().Remove(AiTick);
	}
}

modded class SCR_ChimeraCharacter
{
	//------------------------------------------------------------------------------------------------
	void LF_AskSpikeAi(int commandId, string mode = "ai")
	{
		if (Replication.IsServer())
		{
			LF_AgentHarness.Reply(commandId, "ok", LF_FilmSpike.Start(mode, GetOrigin()));
			return;
		}

		Rpc(RpcAsk_LF_SpikeAi, commandId, mode);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_SpikeAi(int commandId, string mode)
	{
		string result = "error:dev_gate";
		if (LF_DevGate.CheckHarnessRequest(this, "spike"))
			result = LF_FilmSpike.Start(mode, GetOrigin());

		Rpc(RpcDo_LF_SpikeAi, commandId, result);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_LF_SpikeAi(int commandId, string result)
	{
		LF_AgentHarness.Reply(commandId, "ok", result);
	}
}
