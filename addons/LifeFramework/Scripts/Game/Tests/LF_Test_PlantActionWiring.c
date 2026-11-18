// red-proof: observed 2026-11-18 with `tools\cli test --no-build --only LF_Test_PlantActionWiring` before the prefabs carried the action: failed `cabbage seed carries one PLANT action (expected 1, got 0)`, `water bottle carries one WATER action`, `character carries one HARVEST action`, `character carries one INSPECT action`, `SUMMARY tier=only passed=1 failed=1`; with the actions on the prefabs, green. #3000 review: with HARVEST/INSPECT still on Character_Roleplay the new check failed `character carries no plant action ... (expected 0, got 2)`; removed, green.

//! farming/plant-action-wiring (#2818): a player holding seeds sees Plant, holding a water bottle sees
//! Water. Each seed prefab the crop catalogue names, the cabbage seed pack item and the core water bottle
//! resolve an `LF_PlantAction` of the right mode (#2664 built the action; this is the prefab wiring). The
//! player character carries none: its actions show to other players, never to the body's own player.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_PlantActionWiring : LF_Test
{
	protected static const ResourceName CABBAGE_SEED = "{F77BE55D8D3AEBA3}Prefabs/Items/Seeds/CabbageSeeds.et";
	protected static const ResourceName WATER_BOTTLE = "{A231A5F8D479B5DC}Prefabs/Items/Drinks/WaterBottle.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "farming/plant-action-wiring";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		array<LF_EPlantActionMode> plant = {LF_EPlantActionMode.PLANT};
		array<LF_EPlantActionMode> water = {LF_EPlantActionMode.WATER};

		CheckPrefab(ctx, CABBAGE_SEED, "cabbage seed", plant);
		CheckPrefab(ctx, WATER_BOTTLE, "water bottle", water);

		LF_CropCatalogue catalogue = LF_CropCatalogue.Get();
		ctx.NotNull(catalogue, "the crop catalogue loads");
		if (catalogue && catalogue.m_aCrops)
		{
			foreach (LF_CropRow row : catalogue.m_aCrops)
			{
				if (row && !row.m_SeedPrefab.IsEmpty())
					CheckPrefab(ctx, row.m_SeedPrefab, "seed of " + row.m_sCropId, plant);
			}
		}

		// HARVEST and INSPECT never sit on the character: the engine does not offer a player their own
		// body's actions, so there the prompt only showed to OTHER players looking at someone (#3000 review).
		IEntity character = LF_TestCharacterFixture.Spawn(ctx);
		if (character)
			CheckNone(ctx, character, "character");
		LF_TestCharacterFixture.Delete(character);
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckNone(LF_TestContext ctx, IEntity entity, string label)
	{
		ActionsManagerComponent actions = ActionsManagerComponent.Cast(entity.FindComponent(ActionsManagerComponent));
		if (!actions)
			return;

		array<BaseUserAction> list = {};
		actions.GetActionsList(list);
		int count;
		foreach (BaseUserAction base : list)
		{
			if (LF_PlantAction.Cast(base))
				count++;
		}

		ctx.Equal(0, count, label + " carries no plant action (others looking at it would see Harvest and Inspect)");
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckPrefab(LF_TestContext ctx, ResourceName prefab, string label, array<LF_EPlantActionMode> modes)
	{
		Resource res = Resource.Load(prefab);
		if (!res || !res.IsValid())
		{
			ctx.Fail(label + " prefab does not load: " + prefab);
			return;
		}

		EntitySpawnParams params();
		params.TransformMode = ETransformMode.WORLD;
		IEntity item = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
		if (!item)
		{
			ctx.Fail(label + " prefab does not spawn");
			return;
		}

		CheckActions(ctx, item, label, modes);
		SCR_EntityHelper.DeleteEntityAndChildren(item);
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckActions(LF_TestContext ctx, IEntity entity, string label, array<LF_EPlantActionMode> modes)
	{
		ActionsManagerComponent actions = ActionsManagerComponent.Cast(entity.FindComponent(ActionsManagerComponent));
		ctx.NotNull(actions, label + " has an actions manager");
		if (!actions)
			return;

		array<BaseUserAction> list = {};
		actions.GetActionsList(list);

		if (!ChimeraCharacter.Cast(entity))
		{
			int pickUps;
			foreach (BaseUserAction base : list)
			{
				if (SCR_PickUpItemAction.Cast(base))
					pickUps++;
			}

			ctx.True(pickUps > 0, label + " still has its pick-up action (the plant action is appended, not a replacement)");
		}

		foreach (LF_EPlantActionMode mode : modes)
		{
			int count;
			foreach (BaseUserAction base : list)
			{
				LF_PlantAction plant = LF_PlantAction.Cast(base);
				if (plant && plant.GetMode() == mode)
					count++;
			}

			ctx.Equal(1, count, label + " carries one " + typename.EnumToString(LF_EPlantActionMode, mode) + " action");
		}
	}
}
