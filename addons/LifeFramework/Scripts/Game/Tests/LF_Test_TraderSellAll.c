// red-proof: new server entry points (LF_TraderSale.Quote and SellAll) with no failing state before
// them. Batched hook run: `tools\cli test --no-build --red LF_Test_TraderSellRows,LF_Test_TraderSellAll`
// skips the sell-all in RedProof() and the sale assertions fail. Observed 2026-11-22:
// `RED-OK trader/sell-all (fails with the hook on, 35 assertions)`. Green in the same session.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_TraderSellAll : LF_Test
{
	protected static const ResourceName CHARACTER_PREFAB = "{9B5BB216CC7FF18E}Prefabs/Characters/Core/Character_Roleplay.et";
	protected static const ResourceName JACKET_PREFAB = "{9F546CCA2582D16F}Prefabs/Characters/Uniforms/Jacket_M88.et";
	protected static const ResourceName APPLE_PREFAB = "{C9D661E5B0714711}Prefabs/Items/Food/Apple.et";
	protected static const ResourceName IRON_NUGGET_PREFAB = "{6853E2E2AE878611}Prefabs/Resources/Mining/IronNugget.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "trader/sell-all";
	}

	//------------------------------------------------------------------------------------------------
	//! A character at `position` with a jacket (pockets) and 100 cash.
	protected IEntity SpawnSeller(LF_TestContext ctx, vector position)
	{
		Resource res = Resource.Load(CHARACTER_PREFAB);
		ctx.True(res.IsValid(), "character prefab loads");
		if (!res.IsValid())
			return null;

		EntitySpawnParams params();
		params.TransformMode = ETransformMode.WORLD;
		Math3D.MatrixIdentity4(params.Transform);
		params.Transform[3] = position;

		IEntity character = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
		ctx.NotNull(character, "seller character spawns");
		if (!character)
			return null;

		InventoryStorageManagerComponent storage = LF_InventoryUtils.GetResponsibleStorageManager(character);
		IEntity jacket = LF_Utils.SpawnEntityPrefab(JACKET_PREFAB, character.GetOrigin());
		ctx.NotNull(storage, "seller has an inventory manager");
		ctx.NotNull(jacket, "jacket spawns");
		if (!storage || !jacket)
			return character;

		ctx.True(storage.TryInsertItem(jacket, EStoragePurpose.PURPOSE_LOADOUT_PROXY), "jacket goes on the seller");
		ctx.Equal(100, LF_MoneyUtils.AddCash(character, 100), "seller starts with 100 cash");
		return character;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		IEntity traderOwner = GetGame().GetWorld().FindEntityByName(LF_TestAnnex.APPLE_TRADE_CONTAINER);
		ctx.NotNull(traderOwner, "Annex_AppleTradeContainer resolves in the DebugWorld");
		if (!traderOwner)
			return;

		IEntity seller = SpawnSeller(ctx, traderOwner.GetOrigin() + Vector(1.5, 0.1, 0));
		IEntity far = SpawnSeller(ctx, traderOwner.GetOrigin() + Vector(LF_TraderSale.REACH + 40, 0.1, 0));
		if (!seller || !far || ctx.FailureCount() > 0)
		{
			LF_TestCharacterFixture.Delete(seller);
			LF_TestCharacterFixture.Delete(far);
			return;
		}

		ctx.Equal(3, LF_InventoryUtils.AddAmount(seller, APPLE_PREFAB, 3), "seller gets 3 apples");
		ctx.Equal(1, LF_InventoryUtils.AddAmount(seller, IRON_NUGGET_PREFAB, 1), "and an iron nugget the trader does not buy");
		ctx.Equal(2, LF_InventoryUtils.AddAmount(far, APPLE_PREFAB, 2), "far seller gets 2 apples");

		// The quote: one row for apples, priced from the trader's list (5 each), nothing for the nugget.
		array<string> goods = {};
		array<int> onePrices = {};
		array<int> allPrices = {};
		ctx.True(LF_TraderSale.Quote(seller, traderOwner, goods, onePrices, allPrices), "a seller in reach gets a quote");
		ctx.Equal(1, goods.Count(), "one good quoted: apples");
		if (goods.Count() == 1)
		{
			ctx.True(LF_Utils.SamePrefab(APPLE_PREFAB, goods[0]), "the quoted good is the apple");
			ctx.Equal(15, allPrices[0], "all 3 apples quote 15");
		}

		ctx.False(LF_TraderSale.Quote(far, traderOwner, goods, onePrices, allPrices), "a seller out of reach gets no quote");
		ctx.Equal(0, goods.Count(), "and an empty one");

		// Out of reach: sell all sells nothing.
		ctx.Equal(0, LF_TraderSale.SellAll(far, traderOwner, true), "sell all from out of reach pays nothing");
		ctx.Equal(2, LF_InventoryUtils.GetAmount(far, APPLE_PREFAB), "the far seller keeps the apples");
		ctx.Equal(100, LF_MoneyUtils.GetCash(far), "and the cash");

		// A Sell 1 row: one unit off the stack, the rest stays.
		array<IEntity> stacks = LF_InventoryUtils.FindItemsByPrefab(LF_InventoryUtils.GetResponsibleStorageManager(seller), APPLE_PREFAB);
		ctx.True(stacks.Count() > 0, "the seller carries an apple stack");
		if (stacks.Count() > 0)
			ctx.True(LF_TraderSale.Sell(seller, stacks[0], traderOwner, true, 1) == LF_ETraderSale.SOLD, "sell 1 sells one apple");
		ctx.Equal(105, LF_MoneyUtils.GetCash(seller), "one apple pays 5");
		ctx.Equal(2, LF_InventoryUtils.GetAmount(seller, APPLE_PREFAB), "two apples stay");

		// The sale.
		int paid;
		if (!RedProof())
			paid = LF_TraderSale.SellAll(seller, traderOwner, true);

		ctx.Equal(10, paid, "sell all pays 10 for the 2 apples left");
		ctx.Equal(115, LF_MoneyUtils.GetCash(seller), "the seller now has 115 cash");
		ctx.Equal(0, LF_InventoryUtils.GetAmount(seller, APPLE_PREFAB), "the apples left the seller");
		ctx.Equal(1, LF_InventoryUtils.GetAmount(seller, IRON_NUGGET_PREFAB), "the unlisted nugget stays");

		// Nothing left to sell: refused, nothing moves.
		ctx.Equal(0, LF_TraderSale.SellAll(seller, traderOwner, true), "a second sell all pays nothing");
		ctx.Equal(115, LF_MoneyUtils.GetCash(seller), "cash unchanged");

		LF_TestCharacterFixture.Delete(seller);
		LF_TestCharacterFixture.Delete(far);
	}
}
