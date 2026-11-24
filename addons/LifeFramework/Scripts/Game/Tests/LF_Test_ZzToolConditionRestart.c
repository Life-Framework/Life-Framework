// red-proof: the writer half of the #4821 restart proof; its red is LF_Test_AaToolConditionRestart failing in the reload boot when this writer is skipped (observed 2026-11-24: "expected 42.5, got -1" while a later test wiped the state).
//! persist/tool-condition-restart-write (#4821): the save boot of `wt e2e` leaves a worn pickaxe in a stored
//! snapshot ("toolcondition" section) and one on the ground (LF_ToolConditionComponentSerializer);
//! LF_Test_AaToolConditionRestart checks both first thing in the reload boot.
//! The class name sorts it last on purpose: tests run by class name, and several persistence tests load an
//! older save onto the live snapshot store (persist/character-slots, persist/held-ground-legacy,
//! persist/vehicle-state-legacy), which rightly drops tool conditions. Writing after them keeps the state in
//! the shutdown save.
[LF_TestInfo(LF_TestTier.PERSISTENCE)]
class LF_Test_ZzToolConditionRestart : LF_Test
{
	static const ResourceName PICKAXE = "{23341E2840F5ACB4}Prefabs/Tools/Pickaxe.et";
	static const string E2E_KEY = "e2e-tool-condition";
	static const string E2E_SLOT = "e2e_toolcondition";
	static const float E2E_CARRIED = 42.5; // lf-fixed: the stored snapshot's tool across the restart
	static const float E2E_GROUND = 41.5; // lf-fixed: the ground pickaxe across the restart

	protected ref array<LF_ToolConditionComponent> m_aFound;

	override string GetName()
	{
		return "persist/tool-condition-restart-write";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
#ifdef LF_E2E_LOAD_BOOT
		ctx.True(true, "the reload boot writes nothing; LF_Test_AaToolConditionRestart checked the restart");
		return;
#endif

		LF_StorageSnapshotRecord e2e = LF_StorageSnapshotRecord.Create(E2E_KEY, E2E_SLOT);
		LF_StorageItemRecord item = LF_StorageItemRecord.Create(PICKAXE, 1, -1, 0, -1);
		item.m_fToolCondition = E2E_CARRIED;
		e2e.m_aItems.Insert(item);
		LF_StorageSnapshot.Set(e2e);
		ctx.True(LF_StorageSnapshot.ExportToolConditions().Count() > 0, "the e2e snapshot is exported with its tool condition");

		LF_ToolConditionComponent ground = FindGroundTool(E2E_GROUND);
		if (!ground)
		{
			SCR_SpawnPoint point = SCR_SpawnPoint.GetRandomSpawnPointForFaction("CIVILIAN");
			if (!point)
				point = SCR_SpawnPoint.GetRandomSpawnPointDeathmatch();
			ctx.NotNull(point, "a spawn point resolves for the ground pickaxe");
			if (!point)
				return;

			vector spot = point.GetOrigin() + "14 0 14";
			spot[1] = GetGame().GetWorld().GetSurfaceY(spot[0], spot[2]) + 0.3;
			ground = LF_ToolConditionComponent.Of(LF_Utils.SpawnEntityPrefab(PICKAXE, spot));
			ctx.NotNull(ground, "the ground pickaxe spawns");
			if (!ground)
				return;

			ground.SetCondition(E2E_GROUND);
			LF_Debug.Info(LF_ToolWear.FEATURE, string.Format("e2e pickaxe left on the ground at %1, condition %2", spot, ground.GetCondition()));
		}

		ctx.True(FindGroundTool(E2E_GROUND) == ground, "the world query finds the ground pickaxe");
		// The engine does not track a tool spawned loose on the ground (an apple it does; observed 2026-11-24),
		// so a dropped tool is not in the world save at all, worn or not. A tool the save does hold (one in a
		// saved vehicle's storage, any tracked tool) goes through LF_ToolConditionComponentSerializer; tracking
		// this one stands in for that, and the restart proves the serializer.
		PersistenceSystem system = PersistenceSystem.GetInstance();
		ctx.NotNull(system, "the world has a persistence system");
		if (!system)
			return;

		if (!system.IsTracked(ground.GetOwner()))
			system.StartTracking(ground.GetOwner());
		ctx.True(system.IsTracked(ground.GetOwner()), "the world save tracks the ground pickaxe");
	}

	//------------------------------------------------------------------------------------------------
	//! A loose tool in the world at exactly this condition, or null. Logs every loose tool it sees.
	LF_ToolConditionComponent FindGroundTool(float condition)
	{
		m_aFound = {};
		GetGame().GetWorld().QueryEntitiesByAABB("-100000 -1000 -100000", "100000 5000 100000", CollectTool, null, EQueryEntitiesFlags.ALL);
		LF_Debug.Info(LF_ToolWear.FEATURE, string.Format("%1 loose tool(s) in the world", m_aFound.Count()));
		LF_ToolConditionComponent match;
		foreach (LF_ToolConditionComponent tool : m_aFound)
		{
			LF_Debug.Info(LF_ToolWear.FEATURE, string.Format("loose %1 at %2, condition %3", tool.GetKind(), tool.GetOwner().GetOrigin(), tool.GetCondition()));
			if (!match && Math.AbsFloat(tool.GetCondition() - condition) < 0.01)
				match = tool;
		}

		return match;
	}

	//------------------------------------------------------------------------------------------------
	protected bool CollectTool(IEntity entity)
	{
		LF_ToolConditionComponent tool = LF_ToolConditionComponent.Of(entity);
		if (tool && !entity.GetParent())
			m_aFound.Insert(tool);

		return true;
	}
}
