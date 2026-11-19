// red-proof: observed 2026-11-19 (#3783): `test --no-build --red LF_Test_EventSceneDrop` printed `RED-OK events/scene-drop (fails with the hook on, 5 assertions)`: RedProof() skips the first DropItem, so `the scene leaves its item` fails; `--only` then passed.
//! Event scene item drop (#3783): a scene that names an item prefab leaves exactly one on its `event_item`
//! spot. A later run removes the copy nobody took before it leaves a new one, so runs never pile up; a copy a
//! player moved away is theirs and stays. A scene with no spot leaves nothing and touches nothing. The plan
//! carries the prefab from the scene. The spot here is a fixture entity: the marker lookup is the same
//! `LF_Places` path the other scene markers use (`events/waypoints`).
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_EventSceneDrop : LF_Test
{
	static const ResourceName ITEM = "{968892A9535FE6A4}Prefabs/Items/Contraband/LF_ContrabandCrate.et";
	static const string SCENE = "test_drop_scene";

	override string GetName()
	{
		return "events/scene-drop";
	}

	//------------------------------------------------------------------------------------------------
	protected static void Remove(IEntity entity)
	{
		if (entity)
			SCR_EntityHelper.DeleteEntityAndChildren(entity);
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_EventSceneSpawn.ResetStatics();

		// A scene that names an item hands it to its plan.
		array<string> waypoints = {"test_drop_scene#0"};
		LF_EventScene scene = LF_EventScene.Make(SCENE, waypoints, 1, "townsfolk");
		scene.m_sDropPrefab = ITEM;
		LF_EventScenePlan plan = LF_EventScenePlanner.Plan(scene, 10);
		ctx.True(plan.m_bGo, "the scene plans");
		ctx.True(plan.m_sDropPrefab == ITEM, "the plan carries the scene's item prefab");
		LF_EventScenePlanner.ResetStatics();

		IEntity spot = LF_Utils.SpawnEntityPrefab(ITEM, "70 0 70");
		ctx.NotNull(spot, "the fixture spot spawns");
		if (!spot)
			return;

		// The act: the first run leaves one item on the spot.
		IEntity first;
		if (!RedProof())
			first = LF_EventSceneSpawn.DropItem(SCENE, ITEM, spot);

		ctx.NotNull(first, "the scene leaves its item");
		if (!first)
		{
			Remove(spot);
			return;
		}

		ctx.True(vector.Distance(first.GetOrigin(), spot.GetOrigin()) < 0.1, "the item stands on the spot");
		ctx.True(LF_EventSceneSpawn.DropId() == first.GetID(), "the spawner remembers the item it left");

		// A second run with the first one untouched replaces it: one item, never two.
		EntityID firstId = first.GetID();
		IEntity second = LF_EventSceneSpawn.DropItem(SCENE, ITEM, spot);
		ctx.NotNull(second, "the next run leaves its item");
		ctx.True(GetGame().GetWorld().FindEntityByID(firstId) == null, "the copy nobody took is removed");
		ctx.True(second && second.GetID() != firstId, "the new copy is a new entity");

		// A third run after the second copy was carried off: the carried one stays, a new one is left.
		if (second)
		{
			EntityID secondId = second.GetID();
			second.SetOrigin(spot.GetOrigin() + "6 0 0");
			IEntity third = LF_EventSceneSpawn.DropItem(SCENE, ITEM, spot);
			ctx.NotNull(third, "the run after a pick-up leaves its item");
			IEntity carried = GetGame().GetWorld().FindEntityByID(secondId);
			ctx.NotNull(carried, "a copy a player moved away stays");
			Remove(carried);
			Remove(third);
		}

		// No spot: nothing is left, and the remembered item is not touched.
		IEntity none = LF_EventSceneSpawn.DropItem(SCENE, ITEM, null);
		ctx.True(none == null, "a scene with no spot leaves nothing");

		LF_EventSceneSpawn.ResetStatics();
		ctx.True(LF_EventSceneSpawn.DropId() == EntityID.INVALID, "the world-end reset forgets the item");
		Remove(spot);
	}
}
