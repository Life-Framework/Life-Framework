// red-proof: Observed 2026-11-08 with `tools\cli test --no-build --tier all`: turning the
// `if (removed != totalPrice)` refusal in LF_ShopComponent.BuyItem into `if (false && ...)` failed it
// with `[LFTEST] FAIL shop/buy-partial-funds`, `- a buyer with half the price is refused` and
// `- buyer keeps their cash after the refused purchase (expected 250, got 0)`. Restored; green.

//------------------------------------------------------------------------------------------------
//! A buyer with half the price cannot buy. Drives LF_ShopComponent.BuyItem on the DebugWorld
//! black market with a real character, so cash is MoneyStack and delivery is a real insert.
// tier: WORLD
class LF_Test_ShopBuyPartialFunds : LF_Test
{
	protected static const ResourceName M9_PREFAB = "{1353C6EAD1DCFE43}Prefabs/Weapons/Handguns/M9/Handgun_M9.et";

	override string GetName()
	{
		return "shop/buy-partial-funds";
	}

	override void Run(LF_TestContext ctx)
	{
		IEntity shopEntity = GetGame().GetWorld().FindEntityByName("Shop_BlackMarket_Debug");
		ctx.NotNull(shopEntity, "Shop_BlackMarket_Debug resolves in the DebugWorld");
		if (!shopEntity)
			return;

		LF_ShopComponent shop = LF_Component<LF_ShopComponent>.Find(shopEntity);
		ctx.NotNull(shop, "black market shop carries LF_ShopComponent");
		if (!shop)
			return;

		ctx.True(shop.IsItemSoldHere(M9_PREFAB), "the black market sells the M9");
		int price = shop.GetBuyPrice(M9_PREFAB);
		ctx.True(price >= 2, "the M9 has a buy price of at least 2");
		if (ctx.FailureCount() > 0)
			return;

		IEntity buyer = LF_TestCharacterFixture.Spawn(ctx);
		if (!buyer)
			return;

		int funds = price / 2;
		ctx.Equal(funds, LF_MoneyUtils.AddCash(buyer, funds), "buyer is seeded with half the price");
		if (ctx.FailureCount() > 0)
		{
			LF_TestCharacterFixture.Delete(buyer);
			return;
		}

		ctx.False(shop.BuyItem(M9_PREFAB, 1, buyer), "a buyer with half the price is refused");
		ctx.Equal(funds, LF_MoneyUtils.GetCash(buyer), "buyer keeps their cash after the refused purchase");
		ctx.False(LF_TestCharacterFixture.HasItem(buyer, M9_PREFAB), "no M9 is delivered to a buyer who could not pay");

		LF_TestCharacterFixture.Delete(buyer);
	}
};
