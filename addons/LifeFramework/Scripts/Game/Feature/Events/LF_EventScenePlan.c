//------------------------------------------------------------------------------------------------
//! One walker in a scene (#1953): the outfit or role it wears and the dialogue it opens. Built by
//! the planner from the scene's walker prefabs (or its shared role) and per-NPC dialogue list.
class LF_EventSceneNpc
{
	string m_sRole;
	string m_sDialogueId;
	float m_fSpawnDelaySeconds;

	//------------------------------------------------------------------------------------------------
	static LF_EventSceneNpc Make(string role, string dialogueId, float spawnDelaySeconds)
	{
		LF_EventSceneNpc npc = new LF_EventSceneNpc();
		npc.m_sRole = role;
		npc.m_sDialogueId = dialogueId;
		npc.m_fSpawnDelaySeconds = spawnDelaySeconds;
		return npc;
	}
}

//------------------------------------------------------------------------------------------------
//! A scene is data (#1953, shape of #1015 beat 1): the placed waypoints the walkers follow in order,
//! how many walk, what they wear, their timings, an optional dialogue id per NPC and an optional
//! parked vehicle. Waypoint ids are placed-component ids resolved through LF_Places, never names
//! or coordinates, so a pack overlays a scene without touching core. The event config hangs a scene
//! on a phase through a module id; the shell (#757) hands it to LF_EventScenePlanner.
[BaseContainerProps()]
class LF_EventScene
{
	[Attribute("", UIWidgets.EditBox, "Scene id, unique across the config. Logged on every skip")]
	string m_sSceneId;

	[Attribute("", UIWidgets.Auto, "Placed waypoint ids in walking order: the first is where the walkers appear, the last is the target")]
	ref array<string> m_aWaypointIds;

	[Attribute("3", UIWidgets.Auto, "How many NPCs walk. The planner trims it to the hard cap", params: "1 64")]
	int m_iNpcCount;

	[Attribute("", UIWidgets.EditBox, "Role or outfit id every walker gets (#586)")]
	string m_sRole;

	[Attribute("", UIWidgets.Auto, "Optional dialogue id per NPC, by index. Missing or empty entries mean no dialogue")]
	ref array<string> m_aDialogueIds;

	[Attribute("2", UIWidgets.Auto, "Seconds between two walkers appearing", params: "0 600")]
	float m_fSpawnStaggerSeconds;

	[Attribute("120", UIWidgets.Auto, "Seconds the walkers get to reach the target before the scene ends anyway", params: "1 3600")]
	float m_fWalkTimeoutSeconds;

	[Attribute("5", UIWidgets.Auto, "Seconds the walkers stay at the target before they despawn", params: "0 600")]
	float m_fDespawnDelaySeconds;

	[Attribute("", UIWidgets.EditBox, "Optional prefab slot id of a parked vehicle (#964). Empty means none")]
	string m_sVehicleSlot;

	[Attribute("", UIWidgets.ResourceNamePicker, "Optional walker prefabs, handed out in turn (walker 0 the first, walker 1 the second, round again). Set, they win over m_sRole (#2137)", "et")]
	ref array<ResourceName> m_aPrefabs;

	//------------------------------------------------------------------------------------------------
	//! The attribute defaults above, for a scene built in script (a field initializer would beat a config value of 0, #1077).
	static LF_EventScene CreateDefault()
	{
		LF_EventScene scene = new LF_EventScene();
		scene.m_iNpcCount = 3;
		scene.m_fSpawnStaggerSeconds = 2;
		scene.m_fWalkTimeoutSeconds = 120;
		scene.m_fDespawnDelaySeconds = 5;
		return scene;
	}

	//------------------------------------------------------------------------------------------------
	static LF_EventScene Make(string sceneId, notnull array<string> waypointIds, int npcCount, string role, array<string> dialogueIds = null)
	{
		LF_EventScene scene = CreateDefault();
		scene.m_sSceneId = sceneId;
		scene.m_aWaypointIds = {};
		scene.m_aWaypointIds.Copy(waypointIds);
		scene.m_iNpcCount = npcCount;
		scene.m_sRole = role;
		scene.m_aDialogueIds = {};
		if (dialogueIds)
			scene.m_aDialogueIds.Copy(dialogueIds);
		return scene;
	}
}

//------------------------------------------------------------------------------------------------
//! What the planner decided for one scene: either a plan to carry out (m_bGo) or why it was skipped.
//! Plain data, so the shell and the tests read it with no world.
class LF_EventScenePlan
{
	bool m_bGo;
	string m_sSceneId;
	string m_sSkipReason;
	ref array<string> m_aWaypointIds = {};
	ref array<ref LF_EventSceneNpc> m_aNpcs = {};
	string m_sVehicleSlot;
	float m_fWalkTimeoutSeconds;
	float m_fDespawnDelaySeconds;
	//! How many NPCs the scene asked for before the cap trimmed it.
	int m_iRequestedCount;
}

//------------------------------------------------------------------------------------------------
//! Pure planning for event scenes (#1953), no world: orders the waypoints as configured, caps the
//! NPC count, and skips (logged) when the AI budget cannot hold the walkers or another scene runs
//! (#1754). The shell reads the free AI budget and passes it in, so a scene never takes the slots a
//! job NPC needs. At most one scene runs at a time; a restart does not resume it (the static is
//! dropped at world end and the event moves to cooldown). Server only.
class LF_EventScenePlanner
{
	static const string FEATURE = "Events";
	//! Most NPCs any one scene may place, whatever its config asks for.
	static const int HARD_NPC_CAP = 12; // lf-fixed: engine-side safety ceiling on AI per scene; a config can only lower it (the npcCap argument)

	protected static string s_sRunningSceneId;

	//------------------------------------------------------------------------------------------------
	//! \return "" when the scene is usable, else the first reason it is not.
	static string Validate(LF_EventScene scene)
	{
		if (!scene)
			return "no scene";
		if (scene.m_sSceneId == "")
			return "empty scene id";
		if (!scene.m_aWaypointIds || scene.m_aWaypointIds.Count() == 0)
			return "no waypoints";
		foreach (string waypointId : scene.m_aWaypointIds)
		{
			if (waypointId == "")
				return "an empty waypoint id";
		}
		if (scene.m_aPrefabs)
		{
			foreach (ResourceName prefab : scene.m_aPrefabs)
			{
				if (prefab.IsEmpty())
					return "an empty walker prefab";
			}
		}
		if (scene.m_iNpcCount < 1)
			return "no NPCs";
		if (scene.m_fSpawnStaggerSeconds < 0 || scene.m_fWalkTimeoutSeconds <= 0 || scene.m_fDespawnDelaySeconds < 0)
			return "a bad time";
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! \return the id of the scene that is running, or "".
	static string RunningScene()
	{
		return s_sRunningSceneId;
	}

	//------------------------------------------------------------------------------------------------
	//! Decides one scene. `freeAiSlots` is how many AI the budget can still take; the walkers must all
	//! fit or the scene is skipped. Does not start anything: Begin does, so a caller can plan and look.
	//! The skip reason is logged and kept on the plan.
	static LF_EventScenePlan Plan(LF_EventScene scene, int freeAiSlots, int npcCap = HARD_NPC_CAP)
	{
		LF_EventScenePlan plan = new LF_EventScenePlan();
		string reason = Validate(scene);
		if (reason != "")
		{
			Skip(plan, "scene refused: " + reason);
			return plan;
		}

		plan.m_sSceneId = scene.m_sSceneId;
		if (s_sRunningSceneId != "")
		{
			Skip(plan, string.Format("scene %1 skipped: scene %2 is still running", scene.m_sSceneId, s_sRunningSceneId));
			return plan;
		}

		plan.m_iRequestedCount = scene.m_iNpcCount;
		int count = Math.Min(scene.m_iNpcCount, Math.Min(npcCap, HARD_NPC_CAP));
		if (count < 1)
		{
			Skip(plan, string.Format("scene %1 skipped: the NPC cap is %2", scene.m_sSceneId, npcCap));
			return plan;
		}
		if (freeAiSlots < count)
		{
			Skip(plan, string.Format("scene %1 skipped: AI budget used (%2 free, %3 needed)", scene.m_sSceneId, freeAiSlots, count));
			return plan;
		}

		plan.m_aWaypointIds.Copy(scene.m_aWaypointIds);
		for (int i = 0; i < count; i++)
		{
			string dialogueId;
			if (scene.m_aDialogueIds && i < scene.m_aDialogueIds.Count())
				dialogueId = scene.m_aDialogueIds[i];
			string wears = scene.m_sRole;
			if (scene.m_aPrefabs && !scene.m_aPrefabs.IsEmpty())
				wears = scene.m_aPrefabs[i % scene.m_aPrefabs.Count()];
			plan.m_aNpcs.Insert(LF_EventSceneNpc.Make(wears, dialogueId, i * scene.m_fSpawnStaggerSeconds));
		}
		plan.m_sVehicleSlot = scene.m_sVehicleSlot;
		plan.m_fWalkTimeoutSeconds = scene.m_fWalkTimeoutSeconds;
		plan.m_fDespawnDelaySeconds = scene.m_fDespawnDelaySeconds;
		plan.m_bGo = true;

		if (count < scene.m_iNpcCount)
			LF_Debug.Info(FEATURE, string.Format("scene %1 trimmed from %2 to %3 NPCs by the cap", scene.m_sSceneId, scene.m_iNpcCount, count));
		return plan;
	}

	//------------------------------------------------------------------------------------------------
	//! Marks the plan's scene as the one that runs. \return false (and logs) when the plan is a skip or
	//! another scene already runs, so a second Begin is refused even for a plan made before the first.
	static bool Begin(LF_EventScenePlan plan)
	{
		if (!plan || !plan.m_bGo)
			return false;
		if (s_sRunningSceneId != "")
		{
			LF_Debug.Info(FEATURE, string.Format("scene %1 not started: scene %2 is still running", plan.m_sSceneId, s_sRunningSceneId));
			return false;
		}
		s_sRunningSceneId = plan.m_sSceneId;
		LF_Debug.Info(FEATURE, string.Format("scene %1 starts: %2 NPCs over %3 waypoints", plan.m_sSceneId, plan.m_aNpcs.Count(), plan.m_aWaypointIds.Count()));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The scene is over (done, timed out or aborted). Only the running scene can end itself.
	static void Finish(string sceneId)
	{
		if (sceneId == "" || sceneId != s_sRunningSceneId)
			return;
		s_sRunningSceneId = "";
		LF_Debug.Info(FEATURE, string.Format("scene %1 finished", sceneId));
	}

	//------------------------------------------------------------------------------------------------
	protected static void Skip(LF_EventScenePlan plan, string reason)
	{
		plan.m_bGo = false;
		plan.m_sSkipReason = reason;
		LF_Debug.Info(FEATURE, reason);
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_sRunningSceneId = "";
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_EventScenePlanner's statics (Core/LF_StaticReset.c). A scene in progress
//! is not resumed after a restart.
class LF_EventScenePlanner_StaticResetHook : LF_StaticResetHook
{
	protected static bool s_bRegistered = LF_StaticReset.Register(new LF_EventScenePlanner_StaticResetHook());

	override string Owner()
	{
		return "LF_EventScenePlanner";
	}

	override void Apply()
	{
		LF_EventScenePlanner.ResetStatics();
	}
}
