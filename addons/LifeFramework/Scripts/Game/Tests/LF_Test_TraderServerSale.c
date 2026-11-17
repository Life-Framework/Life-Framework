// red-proof: #2407. Before the fix the sale lived in the client-run InsertItem override, which
// paid through LF_MoneyUtils.AddCash (server only) and so refused every sale on a dedicated
// server; there was no server entry point to call, so this test could not exist on that code.
// The red proof is the batched hook run: `tools\cli test --no-build --red LF_Test_TraderServerSale`
// skips the sale in RedProof() and the sale assertions fail. Observed 2026-11-17:
// `RED-OK trader/server-sale (fails with the hook on, 36 assertions)`. Green in the same session.
// tier: WORLD
class LF_Test_TraderServerSale : LF_Test
{
	protected static const ResourceName CHARACTER_PREFAB = "{9B5BB216CC7FF18E}Prefabs/Characters/Core/Character_Roleplay.et";
	protected static const ResourceName JACKET_PREFAB = "{9F546CCA2582D16F}Prefabs/Characters/Uniforms/Jacket_M88.et";
	protected static const ResourceName APPLE_PREFAB = "{C9D661E5B0714711}Prefabs/Items/Food/Apple.et";
	protected static const ResourceName IRON_NUGGET_PREFAB = "{6853E2E2AE878611}Prefabs/Resources/Mining/IronNugget.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "trader/server-sale";
	}

	//------------------------------------------------------------------------------------------------
	//! A character at `position` with a jacket (its pockets are the deposit storage) and 100 cash.
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
			return null;

		ctx.True(storage.TryInsertItem(jacket, EStoragePurpose.PURPOSE_LOADOUT_PROXY), "jacket goes on the seller, giving it pockets");
		ctx.Equal(100, LF_MoneyUtils.AddCash(character, 100), "seller starts with 100 cash");
		return character;
	}

	//------------------------------------------------------------------------------------------------
	protected IEntity FindOne(LF_TestContext ctx, IEntity character, ResourceName prefab)
	{
		InventoryStorageManagerComponent storage = LF_InventoryUtils.GetResponsibleStorageManager(character);
		array<IEntity> items = LF_InventoryUtils.FindItemsByPrefab(storage, prefab);
		ctx.Equal(1, items.Count(), "seller carries one entity of " + prefab);
		if (items.Count() != 1)
			return null;

		return items[0];
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		IEntity traderOwner = GetGame().GetWorld().FindEntityByName(LF_TestAnnex.APPLE_TRADE_CONTAINER);
		ctx.NotNull(traderOwner, "Annex_AppleTradeContainer resolves in the DebugWorld");
		if (!traderOwner)
			return;

		LF_TraderManagerComponent trader = LF_Component<LF_TraderManagerComponent>.Find(traderOwner);
		ctx.NotNull(trader, "the container carries the trader manager");
		if (!trader)
			return;

		// A request from a client is untrusted: same entry the RPC calls, untrusted = true.
		IEntity far = SpawnSeller(ctx, traderOwner.GetOrigin() + Vector(LF_TraderSale.REACH + 40, 0.1, 0));
		IEntity near = SpawnSeller(ctx, traderOwner.GetOrigin() + Vector(1.5, 0.1, 0));
		if (!far || !near)
			return;

		ctx.Equal(2, LF_InventoryUtils.AddAmount(far, APPLE_PREFAB, 2), "far seller gets 2 apples");
		ctx.Equal(2, LF_InventoryUtils.AddAmount(near, APPLE_PREFAB, 2), "near seller gets 2 apples");
		ctx.Equal(1, LF_InventoryUtils.AddAmount(near, IRON_NUGGET_PREFAB, 1), "near seller gets an iron nugget the trader does not buy");
		IEntity farApples = FindOne(ctx, far, APPLE_PREFAB);
		IEntity nearApples = FindOne(ctx, near, APPLE_PREFAB);
		IEntity nugget = FindOne(ctx, near, IRON_NUGGET_PREFAB);
		if (ctx.FailureCount() > 0)
			return;

		// Out of reach: refused, nothing moves.
		ctx.True(LF_TraderSale.Sell(far, farApples, traderOwner, true) == LF_ETraderSale.OUT_OF_REACH, "a seller out of reach is refused");
		ctx.Equal(100, LF_MoneyUtils.GetCash(far), "the refused far seller keeps 100 cash");
		ctx.Equal(2, LF_InventoryUtils.GetAmount(far, APPLE_PREFAB), "the refused far seller keeps the apples");

		// Not carried: an item lying on the ground cannot be sold by a forged request.
		IEntity ground = LF_Utils.SpawnEntityPrefab(APPLE_PREFAB, near.GetOrigin() + Vector(0.5, 0, 0));
		ctx.NotNull(ground, "a loose apple spawns on the ground");
		if (ground)
		{
			ctx.True(LF_TraderSale.Sell(near, ground, traderOwner, true) == LF_ETraderSale.NOT_CARRIED, "an item the seller does not carry is refused");
			ctx.Equal(100, LF_MoneyUtils.GetCash(near), "no cash is paid for an item the seller does not carry");
			SCR_EntityHelper.DeleteEntityAndChildren(ground);
		}

		// Price list: the trader pays only for what it lists.
		ctx.True(LF_TraderSale.Sell(near, nugget, traderOwner, true) == LF_ETraderSale.NOT_BOUGHT, "an unlisted item is refused");
		ctx.Equal(1, LF_InventoryUtils.GetAmount(near, IRON_NUGGET_PREFAB), "the unlisted item stays with the seller");

		// Black market gate fails closed for a seller with no verified account.
		bool wasBlackMarket = trader.m_bBlackMarket;
		trader.m_bBlackMarket = true;
		ctx.True(LF_TraderSale.Sell(near, nearApples, traderOwner, true) == LF_ETraderSale.BLACK_MARKET, "the black market refuses an unverified seller");
		trader.m_bBlackMarket = wasBlackMarket;
		ctx.Equal(2, LF_InventoryUtils.GetAmount(near, APPLE_PREFAB), "the refused black-market seller keeps the apples");

		// The sale: the server pays the quoted price per unit and takes the stack.
		LF_ETraderSale outcome = LF_ETraderSale.NOT_SERVER;
		if (!RedProof())
			outcome = LF_TraderSale.Sell(near, nearApples, traderOwner, true);

		ctx.True(outcome == LF_ETraderSale.SOLD, "the server sells 2 apples to the trader");
		ctx.Equal(110, LF_MoneyUtils.GetCash(near), "the seller is paid 10 cash (2 apples at 5)");
		ctx.Equal(0, LF_InventoryUtils.GetAmount(near, APPLE_PREFAB), "the sold apples left the seller");

		SCR_EntityHelper.DeleteEntityAndChildren(far);
		SCR_EntityHelper.DeleteEntityAndChildren(near);
	}
};
