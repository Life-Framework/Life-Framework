// red-proof: 2026-11-18 `tools\cli test --no-build --red LF_Test_EveronStoreFood1989`: RED-OK everon/store-food-1989 (RedProof() skips the stock lookup, so the test fails); green with the stock and price overlays in the pack leg: `SUMMARY test ok tier=all legs=1 passed=2 failed=0`.
//! shop/store-food-1989: the Saint-Philippe general store sells eight 1989 food and drink items
//! (bread, canned meat, tinned sardines, soda, milk, instant coffee, chocolate, pickles). Each one is
//! on the `general_store` shelf stock at its price, has its own price rule with a sell-back below the
//! buy price (no profit loop), and eats or drinks: the prefab carries a survival consumable effect
//! that raises the stat it names.
//! Lives in the pack because it needs the pack's stock and price overlays (pack leg).
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronStoreFood1989 : LF_Test
{
	static const ResourceName BREAD = "{28AC0D92535C01B9}Prefabs/Items/Store1989/LF_Bread.et";
	static const ResourceName CANNED_MEAT = "{A0A5D34D6F8232B4}Prefabs/Items/Store1989/LF_CannedMeat.et";
	static const ResourceName CANNED_FISH = "{6FD0FBD61A6F7B0D}Prefabs/Items/Store1989/LF_CannedFish.et";
	static const ResourceName SODA = "{EE04C7556D2C9F49}Prefabs/Items/Store1989/LF_SodaBottle.et";
	static const ResourceName MILK = "{D77ECA12D070F735}Prefabs/Items/Store1989/LF_MilkBottle.et";
	static const ResourceName COFFEE = "{08514AE53A953403}Prefabs/Items/Store1989/LF_CoffeeTin.et";
	static const ResourceName CHOCOLATE = "{F407598417C980D2}Prefabs/Items/Store1989/LF_ChocolateBar.et";
	static const ResourceName PICKLES = "{843F6EC285385B9F}Prefabs/Items/Store1989/LF_PickleJar.et";
	static const string STORE_STOCK = "general_store";
	static const string CHARACTER_ID = "test-store-food-1989";
	static const int PRICE_MIN = 2;
	static const int PRICE_MAX = 12;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/store-food-1989";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_ShopManagerComponent manager = LF_ShopManagerComponent.GetInstance();
		ctx.NotNull(manager, "the shop manager exists");
		if (!manager)
			return;

		LF_ShopStock stock = null;
		if (!RedProof())
			stock = LF_ShopStockConfig.Get().Find(STORE_STOCK);

		ctx.NotNull(stock, "the general_store stock exists in the merged stock config");
		if (!stock)
			return;

		IEntity character = LF_TestCharacterFixture.Spawn(ctx);
		if (!character)
			return;

		LF_CharacterSurvivalComponent survival = LF_CharacterSurvivalComponent.Cast(character.FindComponent(LF_CharacterSurvivalComponent));
		LF_SurvivalStats stats;
		if (survival)
		{
			survival.Init(CHARACTER_ID);
			stats = survival.GetSurvivalStats();
		}

		if (!stats)
		{
			ctx.Fail("the test character has no survival stats");
			LF_TestCharacterFixture.Delete(character);
			return;
		}

		CheckItem(ctx, manager, stock, stats, character, "bread", BREAD, true, false);
		CheckItem(ctx, manager, stock, stats, character, "canned meat", CANNED_MEAT, true, false);
		CheckItem(ctx, manager, stock, stats, character, "tinned sardines", CANNED_FISH, true, false);
		CheckItem(ctx, manager, stock, stats, character, "soda", SODA, false, true);
		CheckItem(ctx, manager, stock, stats, character, "milk", MILK, false, true);
		CheckItem(ctx, manager, stock, stats, character, "instant coffee", COFFEE, false, true);
		CheckItem(ctx, manager, stock, stats, character, "chocolate bar", CHOCOLATE, true, false);
		CheckItem(ctx, manager, stock, stats, character, "pickles", PICKLES, true, false);

		LF_TestCharacterFixture.Delete(character);
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckItem(LF_TestContext ctx, LF_ShopManagerComponent manager, LF_ShopStock stock, LF_SurvivalStats stats, IEntity character, string label, ResourceName prefab, bool feeds, bool quenches)
	{
		// On the shelf stock, at a villager's price.
		int stockPrice = 0;
		foreach (LF_ShopStockItem row : stock.m_aItems)
		{
			if (row && row.m_Prefab == prefab)
				stockPrice = row.m_iPrice;
		}

		ctx.True(stockPrice > 0, string.Format("the %1 is on the general_store stock", label));
		ctx.True(stockPrice >= PRICE_MIN && stockPrice <= PRICE_MAX, string.Format("the %1 costs a villager's price, $2 to $12 (got %2)", label, stockPrice));

		// Priced by the pack overlay's own rule, visible, with a sell-back under the buy price.
		LF_ShopCatalogItem entry = null;
		if (!RedProof())
			entry = manager.GetItem(prefab);

		ctx.NotNull(entry, string.Format("the %1 is in the shop catalogue (the overlay lists it)", label));
		if (entry)
		{
			ctx.False(entry.m_bHidden, string.Format("the %1 is not hidden from the shop", label));
			ctx.True(entry.m_iBuyPrice == stockPrice, string.Format("the %1 buy price (%2) equals its stock price (%3)", label, entry.m_iBuyPrice, stockPrice));
			ctx.True(entry.m_iSellPrice > 0, string.Format("the %1 has a sell-back value (got %2)", label, entry.m_iSellPrice));
			ctx.True(entry.m_iBuyPrice > entry.m_iSellPrice, string.Format("the %1 costs more (%2) than it sells back for (%3): no profit loop", label, entry.m_iBuyPrice, entry.m_iSellPrice));
		}

		// It is consumable: the effect raises the stat it names.
		Resource res = Resource.Load(prefab);
		if (!res || !res.IsValid())
		{
			ctx.Fail(string.Format("the %1 prefab does not load: %2", label, prefab));
			return;
		}

		EntitySpawnParams params();
		params.TransformMode = ETransformMode.WORLD;
		IEntity item = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
		SCR_ConsumableItemComponent consumable;
		if (item)
			consumable = SCR_ConsumableItemComponent.Cast(item.FindComponent(SCR_ConsumableItemComponent));

		LF_ConsumableEffectSurvival effect;
		if (consumable)
			effect = LF_ConsumableEffectSurvival.Cast(consumable.GetConsumableEffect());

		ctx.NotNull(effect, string.Format("the %1 has a survival consumable effect", label));
		if (effect)
		{
			stats.SetHunger(20);
			stats.SetThirst(20);
			effect.ApplyEffect(character, character, item, null);
			if (feeds)
				ctx.True(stats.GetHunger() > 20, string.Format("eating the %1 raises hunger (got %2)", label, stats.GetHunger()));
			if (quenches)
				ctx.True(stats.GetThirst() > 20, string.Format("drinking the %1 raises thirst (got %2)", label, stats.GetThirst()));
		}

		if (item)
			SCR_EntityHelper.DeleteEntityAndChildren(item);
	}
}
