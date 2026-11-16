// red-proof: observed 2026-11-16 with `toolscli test --no-build --only LF_Test_AlcoholDrinks` before the prefabs existed: failed `beer prefab does not load`, `moonshine prefab does not load`, `the beer processor makes the beer bottle` (still the dangling {C3B9A61B460BE287} GUID) and `the still turns out the moonshine bottle` (still the crate), `SUMMARY tier=only passed=0 failed=1`; with the prefabs and repoints, green.

// tier: WORLD
//! Beer and moonshine are real drink items (#2001): both prefabs load with a mesh and carry the
//! impairment effect, the beer processor's output is the beer bottle, and the moonshine still
//! turns out the moonshine bottle.
class LF_Test_AlcoholDrinks : LF_Test
{
	static const ResourceName BEER = "{4CBB1F9A04D15772}Prefabs/Items/Drinks/BeerBottle.et";
	static const ResourceName MOONSHINE = "{5280F09CEB005042}Prefabs/Items/Drinks/Moonshine.et";
	static const ResourceName BEER_PROCESSOR = "{1303AC26D8473E33}Prefabs/Processing/Chains/Beer.et";

	override string GetName()
	{
		return "drinks/alcohol";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		CheckDrink(ctx, BEER, "beer");
		CheckDrink(ctx, MOONSHINE, "moonshine");
		CheckBeerProcessor(ctx);
		CheckStill(ctx);
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckDrink(LF_TestContext ctx, ResourceName prefab, string label)
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

		ctx.True(item.GetVObject() != null, label + " has a real mesh (a MeshObject without an .xob is invisible)");

		SCR_ConsumableItemComponent consumable = SCR_ConsumableItemComponent.Cast(item.FindComponent(SCR_ConsumableItemComponent));
		ctx.NotNull(consumable, label + " is a consumable item");
		if (consumable)
			ctx.NotNull(LF_ConsumableEffectImpair.Cast(consumable.GetConsumableEffect()), label + " carries the impair effect");

		SCR_EntityHelper.DeleteEntityAndChildren(item);
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckBeerProcessor(LF_TestContext ctx)
	{
		Resource res = Resource.Load(BEER_PROCESSOR);
		if (!res || !res.IsValid())
		{
			ctx.Fail("the beer processor does not load");
			return;
		}

		EntitySpawnParams params();
		params.TransformMode = ETransformMode.WORLD;
		IEntity processor = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
		if (!processor)
		{
			ctx.Fail("the beer processor does not spawn");
			return;
		}

		ActionsManagerComponent actions = ActionsManagerComponent.Cast(processor.FindComponent(ActionsManagerComponent));
		ctx.NotNull(actions, "the beer processor has an actions manager");
		if (actions)
		{
			array<BaseUserAction> list = {};
			actions.GetActionsList(list);
			int found;
			foreach (BaseUserAction base : list)
			{
				LF_ProcessAction process = LF_ProcessAction.Cast(base);
				if (!process || !process.m_aProcessingOutputs || process.m_aProcessingOutputs.IsEmpty())
					continue;

				found++;
				ResourceName output = process.m_aProcessingOutputs[0].m_OutputPrefab;
				ctx.EqualStr(BEER, output, "the beer processor makes the beer bottle");
				Resource outRes = Resource.Load(output);
				ctx.True(outRes && outRes.IsValid(), "the beer processor's output resolves");
			}

			ctx.Equal(1, found, "the beer processor has one process action");
		}

		SCR_EntityHelper.DeleteEntityAndChildren(processor);
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckStill(LF_TestContext ctx)
	{
		LF_ChainCatalog.ResetStatics();
		LF_ChainCatalog.Build(LF_ResourceChainCatalog.GetConfig());

		LF_ChainDef chain = LF_ChainCatalog.FindChain("moonshine");
		ctx.NotNull(chain, "the moonshine chain resolves");
		if (chain)
		{
			LF_ChainStepDef distill = chain.FindStep("distill");
			ctx.NotNull(distill, "the distill step exists");
			if (distill)
				ctx.EqualStr(MOONSHINE, distill.m_aOutputs[0].m_OutputPrefab, "the still turns out the moonshine bottle");
		}

		LF_ChainCatalog.ResetStatics();
	}
}
