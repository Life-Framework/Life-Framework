// red-proof: observed 2026-11-19 with `tools\cli test --no-build --red LF_Test_FoodConsume`: `RED PROOF OK`, with the cuff hook skipped it failed `a cuffed character gets no hunger back (expected 40, got 60)`, `the refusal says why: cuffed` and `a cuffed character may not throw food either`; `--only` passes (6 tests with the neighbours).
//! Eating and drinking (maintainer report 2026-11-19: "no eating or drinking action"). Every food and
//! drink carries the shared LF_ConsumableEffectEat, which gives the inventory Use (the vanilla
//! canteen path) and the use animation. The server side: a use restores hunger or thirst once per
//! item, and is refused while cuffed. The throw refuses by the same rules.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_FoodConsume : LF_Test
{
	static const ResourceName APPLE = "{C9D661E5B0714711}Prefabs/Items/Food/Apple.et";
	static const ResourceName WATER = "{A231A5F8D479B5DC}Prefabs/Items/Drinks/WaterBottle.et";
	static const string CHARACTER_ID = "test-food-consume";

	override string GetName()
	{
		return "survival/food-consume";
	}

	//------------------------------------------------------------------------------------------------
	protected LF_ConsumableEffectEat EffectOf(LF_TestContext ctx, IEntity item, string label)
	{
		SCR_ConsumableItemComponent consumable;
		if (item)
			consumable = SCR_ConsumableItemComponent.Cast(item.FindComponent(SCR_ConsumableItemComponent));

		LF_ConsumableEffectEat effect;
		if (consumable)
			effect = LF_ConsumableEffectEat.Cast(consumable.GetConsumableEffect());

		ctx.NotNull(effect, label + " carries the shared eat and drink effect");
		return effect;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		IEntity character = LF_TestCharacterFixture.Spawn(ctx);
		if (!character)
			return;

		SCR_ChimeraCharacter chimera = SCR_ChimeraCharacter.Cast(character);
		LF_CharacterSurvivalComponent survival = LF_CharacterSurvivalComponent.Cast(character.FindComponent(LF_CharacterSurvivalComponent));
		if (!chimera || !survival)
		{
			ctx.Fail("the test character lacks the chimera class or the survival component");
			LF_TestCharacterFixture.Delete(character);
			return;
		}

		survival.Init(CHARACTER_ID);
		LF_SurvivalStats stats = survival.GetSurvivalStats();
		IEntity apple = LF_Utils.SpawnEntityPrefab(APPLE, "0 0 0");
		IEntity water = LF_Utils.SpawnEntityPrefab(WATER, "0 0 0");
		LF_ConsumableEffectEat food = EffectOf(ctx, apple, "an apple");
		LF_ConsumableEffectEat drink = EffectOf(ctx, water, "a water bottle");
		if (!stats || !food || !drink)
		{
			ctx.Fail("no stats or no effect to test");
			SCR_EntityHelper.DeleteEntityAndChildren(apple);
			SCR_EntityHelper.DeleteEntityAndChildren(water);
			LF_TestCharacterFixture.Delete(character);
			return;
		}

		ctx.True(food.AllowsInventoryUseWithoutHitZone(), "food offers the inventory Use");
		ctx.True(drink.AllowsInventoryUseWithoutHitZone(), "a drink offers the inventory Use");

		stats.SetHunger(40);
		stats.SetThirst(40);

		// Cuffed: the use is refused at the start and again on the server when it completes.
		if (!RedProof())
			chimera.LF_SetRestrained(true);

		SCR_EConsumableFailReason why;
		ctx.False(food.CanApplyEffect(character, character, why), "a cuffed character may not start eating");
		ctx.EqualStr("#LF-Consume_Refused_Cuffed", LF_ConsumeRules.RefusalFor(character), "the refusal says why: cuffed");
		food.ApplyEffect(character, character, apple, null);
		ctx.EqualFloat(40, stats.GetHunger(), 0.001, "a cuffed character gets no hunger back");
		ctx.EqualStr("error #LF-Consume_Refused_Cuffed", LF_FoodThrow.Throw(character, "0 0 1"), "a cuffed character may not throw food either");
		chimera.LF_SetRestrained(false);

		// Free: the use starts, and the effect restores hunger once per item.
		ctx.True(food.CanApplyEffect(character, character, why), "a free character may start eating");
		ctx.EqualStr("", LF_ConsumeRules.RefusalFor(character), "nothing refuses a free character");
		food.ApplyEffect(character, character, apple, null);
		float afterFirst = stats.GetHunger();
		ctx.True(afterFirst > 40, string.Format("eating raises hunger (got %1)", afterFirst));
		food.ApplyEffect(character, character, apple, null);
		ctx.EqualFloat(afterFirst, stats.GetHunger(), 0.001, "the same item restores hunger once, not twice");

		drink.ApplyEffect(character, character, water, null);
		ctx.True(stats.GetThirst() > 40, string.Format("drinking raises thirst (got %1)", stats.GetThirst()));

		ctx.EqualStr("error no_food_held", LF_FoodThrow.Throw(character, "0 0 1"), "throwing with nothing in the hands does nothing");

		SCR_EntityHelper.DeleteEntityAndChildren(apple);
		SCR_EntityHelper.DeleteEntityAndChildren(water);
		LF_TestCharacterFixture.Delete(character);
	}
}
