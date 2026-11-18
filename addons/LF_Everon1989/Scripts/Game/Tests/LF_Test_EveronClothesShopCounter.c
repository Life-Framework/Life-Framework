// red-proof: observed 2026-11-18 (#2315) with `tools\cli test --no-build --red LF_Test_EveronClothesShopCounter`: RedProof() skips the spawn, so `the clothes shop counter spawns` fails, RED-OK (2 assertions). Green without the hook (pack leg).
//! #2315 (first-hour step 6): the Saint-Philippe clothing counter is a working shop. The pack prefab
//! `LF_ClothesShopCounter` carries an `LF_ShopComponent` for the `clothing_rack` catalogue outlet and the
//! `LF_ShopAction` that opens its price list. Found by prefab, never by a town coordinate.
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronClothesShopCounter : LF_Test
{
	protected static const ResourceName COUNTER = "{37C56F70D97B0214}Prefabs/Props/Commercial/LF_ClothesShopCounter.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/clothes-shop-counter";
	}

	//------------------------------------------------------------------------------------------------
	protected static bool HasShopAction(IEntity counter)
	{
		BaseActionsManagerComponent manager = BaseActionsManagerComponent.Cast(counter.FindComponent(BaseActionsManagerComponent));
		if (!manager)
			return false;

		array<BaseUserAction> actions = {};
		manager.GetActionsList(actions);
		foreach (BaseUserAction action : actions)
		{
			if (LF_ShopAction.Cast(action))
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		// The act: load the counter's prefab.
		IEntity counter;
		if (!RedProof())
			counter = LF_Utils.SpawnEntityPrefab(COUNTER, "0 -50 0");

		ctx.True(counter != null, "the clothes shop counter spawns");
		if (!counter)
			return;

		LF_ShopComponent shop = LF_ShopComponent.Cast(counter.FindComponent(LF_ShopComponent));
		ctx.NotNull(shop, "the counter has a shop component");
		if (shop)
			ctx.EqualStr("clothing_rack", shop.GetOutletId(), "the counter sells the clothing_rack outlet");

		ctx.True(HasShopAction(counter), "the counter has the shop action");

		SCR_EntityHelper.DeleteEntityAndChildren(counter);
	}
}
