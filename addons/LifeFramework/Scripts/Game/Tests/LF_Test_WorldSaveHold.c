// red-proof: observed 2026-11-27 with `tools\cli test --no-build --red LF_Test_WorldSaveHold` (RED PROOF OK): RedProof() skips the Hold calls and the body, its 3 worn items and the re-registered group stay in the world save. The ground truth is the real island restart in the PR: an event walk saved, the reload VMEd in SCR_AIGroupSerializer.Deserialize and spawned the townsfolk twice; with the holds, no VME and one of each.

//! Session NPCs and their AI stay out of the engine's world save (walker-group-restart). The vanilla
//! persistence config our config inherits saves every AIGroup, AIWaypoint and Character_Base
//! character; a real restart with an event walk running VMEd in SCR_AIGroupSerializer.Deserialize
//! (no faction manager in our game mode) and put a second copy of every resident on the square.
//! LF_WorldSaveHold untracks the walk group, its waypoint and a resident body with its clothes, and
//! holds them again before a save when the engine registered them again.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_WorldSaveHold : LF_Test
{
	override string GetName()
	{
		return "persist/world-save-hold";
	}

	//------------------------------------------------------------------------------------------------
	//! The hold under test; the red proof skips it.
	protected void Hold(IEntity entity)
	{
		if (RedProof())
			return;

		LF_WorldSaveHold.Hold(entity);
	}

	//------------------------------------------------------------------------------------------------
	//! How many children of `entity` (at any depth) the world save still writes.
	protected static int SavedChildren(IEntity entity)
	{
		int saved;
		IEntity child = entity.GetChildren();
		while (child)
		{
			if (LF_WorldSaveHold.IsSaved(child))
				saved++;

			saved += SavedChildren(child);
			child = child.GetSibling();
		}

		return saved;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		PersistenceSystem system = PersistenceSystem.GetInstance();
		ctx.NotNull(system, "the world has the engine's persistence system");
		if (!system)
			return;

		vector at = "0 0 0";
		IEntity anchor = LF_TestCharacterFixture.Spawn(ctx);
		if (anchor)
			at = anchor.GetOrigin() + "4 0 4";

		IEntity group = LF_Utils.SpawnEntityPrefab(LF_EventSceneWalk.GROUP_PREFAB, at);
		IEntity waypoint = LF_Utils.SpawnEntityPrefab(LF_EventSceneWalk.MOVE_WAYPOINT_PREFAB, at + "2 0 0");
		IEntity body = LF_Utils.SpawnEntityPrefab(LF_ResidentSpawner.DEFAULT_PREFAB, at + "-2 0 0");
		ctx.NotNull(group, "the walk's Group_Base spawns");
		ctx.NotNull(waypoint, "the walk's move waypoint spawns");
		ctx.NotNull(body, "a resident body spawns");
		if (!group || !waypoint || !body)
		{
			Cleanup(anchor, group, waypoint, body);
			return;
		}

		// The engine tracks them as spawned (the vanilla AIGroup, AIWaypoint and Character_Base rules).
		ctx.True(system.IsTracked(group) || system.StartTracking(group), "the vanilla config saves an AI group");
		ctx.True(system.IsTracked(waypoint) || system.StartTracking(waypoint), "the vanilla config saves an AI waypoint");
		ctx.True(system.IsTracked(body) || system.StartTracking(body), "the vanilla config saves a resident body");

		Hold(group);
		Hold(waypoint);
		Hold(body);
		ctx.False(LF_WorldSaveHold.IsSaved(group), "the walk group is not in the world save");
		ctx.False(LF_WorldSaveHold.IsSaved(waypoint), "the walk waypoint is not in the world save");
		ctx.False(LF_WorldSaveHold.IsSaved(body), "the resident body is not in the world save");
		ctx.Equal(0, SavedChildren(body), "nothing the resident wears is in the world save");

		// The engine registered one again (lazily, after the spawn): the hold before the save takes it out.
		system.StartTracking(group, false);
		int retaken = LF_WorldSaveHold.HoldAgain();
		ctx.True(retaken >= 1 || RedProof(), "the hold before the save found the group registered again");
		ctx.False(LF_WorldSaveHold.IsSaved(group), "after the hold before the save the group is out again");

		Cleanup(anchor, group, waypoint, body);
	}

	//------------------------------------------------------------------------------------------------
	protected void Cleanup(IEntity anchor, IEntity group, IEntity waypoint, IEntity body)
	{
		LF_TestCharacterFixture.Delete(anchor);
		LF_TestCharacterFixture.Delete(group);
		LF_TestCharacterFixture.Delete(waypoint);
		LF_TestCharacterFixture.Delete(body);
	}
}
