// red-proof: observed 2026-11-16 with `tools\cli test --no-build --only LF_Test_DrinkQuenches` before the beer prefab carried a thirst amount: failed `a beer sip raises thirst (got 40)` (impairment already started), `SUMMARY tier=only passed=0 failed=1`; green after.

// tier: WORLD
//! #2159: one sip of beer quenches thirst and impairs the drinker. The engine takes one consumable
//! effect per item, so the impair effect carries a thirst amount and applies it itself.
class LF_Test_DrinkQuenches : LF_Test
{
	static const ResourceName BEER = "{4CBB1F9A04D15772}Prefabs/Items/Drinks/BeerBottle.et";
	static const string CHARACTER_ID = "test-drink-quenches";

	override string GetName()
	{
		return "drinks/quenches";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		IEntity character = LF_TestCharacterFixture.Spawn(ctx);
		if (!character)
			return;

		LF_CharacterSurvivalComponent survival = LF_CharacterSurvivalComponent.Cast(character.FindComponent(LF_CharacterSurvivalComponent));
		if (!survival)
		{
			ctx.Fail("character prefab lacks the survival component");
			LF_TestCharacterFixture.Delete(character);
			return;
		}

		survival.Init(CHARACTER_ID);
		LF_SurvivalStats stats = survival.GetSurvivalStats();
		if (!stats)
		{
			ctx.Fail("the survival component has no stats after Init");
			LF_TestCharacterFixture.Delete(character);
			return;
		}

		Resource res = Resource.Load(BEER);
		if (!res || !res.IsValid())
		{
			ctx.Fail("beer prefab does not load: " + BEER);
			LF_TestCharacterFixture.Delete(character);
			return;
		}

		EntitySpawnParams params();
		params.TransformMode = ETransformMode.WORLD;
		IEntity beer = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
		SCR_ConsumableItemComponent consumable;
		if (beer)
			consumable = SCR_ConsumableItemComponent.Cast(beer.FindComponent(SCR_ConsumableItemComponent));

		LF_ConsumableEffectImpair effect;
		if (consumable)
			effect = LF_ConsumableEffectImpair.Cast(consumable.GetConsumableEffect());

		if (!effect)
		{
			ctx.Fail("the beer has no impair consumable effect");
			if (beer)
				SCR_EntityHelper.DeleteEntityAndChildren(beer);
			LF_TestCharacterFixture.Delete(character);
			return;
		}

		stats.SetThirst(40);
		LF_Impairment.Clear(CHARACTER_ID, "test");
		ctx.True(LF_Impairment.GetLevel(CHARACTER_ID) == 0, "the drinker starts sober");

		// The act: one sip.
		effect.ApplyEffect(character, character, beer, null);

		ctx.True(LF_Impairment.GetLevel(CHARACTER_ID) > 0, "a beer sip starts impairment");
		ctx.True(stats.GetThirst() > 40, string.Format("a beer sip raises thirst (got %1)", stats.GetThirst()));
		ctx.True(stats.GetThirst() <= 100, "thirst stays clamped at 100");

		LF_Impairment.Clear(CHARACTER_ID, "test");
		SCR_EntityHelper.DeleteEntityAndChildren(beer);
		LF_TestCharacterFixture.Delete(character);
	}
}
