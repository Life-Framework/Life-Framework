// red-proof: #2506. On main (e7c05ca7) the native moves below skip the InsertItem override and the
// crate's storage accepted the apple, so it sat in the crate unpaid. Observed 2026-11-18 on the unfixed
// code: FAIL trader/crate-direct-insert (vicinity move, stays with seller, direct insert); green with
// LF_TraderCrateGuard in the next run.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_TraderCrateDirectInsert : LF_Test
{
	protected static const ResourceName CHARACTER_PREFAB = "{9B5BB216CC7FF18E}Prefabs/Characters/Core/Character_Roleplay.et";
	protected static const ResourceName JACKET_PREFAB = "{9F546CCA2582D16F}Prefabs/Characters/Uniforms/Jacket_M88.et";
	protected static const ResourceName APPLE_PREFAB = "{C9D661E5B0714711}Prefabs/Items/Food/Apple.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "trader/crate-direct-insert";
	}

	//------------------------------------------------------------------------------------------------
	//! A character at `position` with a jacket (its pockets hold the apples) and 100 cash.
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
	override void Run(LF_TestContext ctx)
	{
		IEntity traderOwner = GetGame().GetWorld().FindEntityByName(LF_TestAnnex.APPLE_TRADE_CONTAINER);
		ctx.NotNull(traderOwner, "Annex_AppleTradeContainer resolves in the DebugWorld");
		if (!traderOwner)
			return;

		BaseInventoryStorageComponent crate = LF_Component<BaseInventoryStorageComponent>.Find(traderOwner);
		ctx.NotNull(crate, "the trader crate has a storage");
		ctx.NotNull(LF_Component<LF_TraderManagerComponent>.Find(traderOwner), "the crate carries the trader manager");
		if (!crate)
			return;

		IEntity seller = SpawnSeller(ctx, traderOwner.GetOrigin() + Vector(1.5, 0.1, 0));
		if (!seller)
			return;

		InventoryStorageManagerComponent manager = LF_InventoryUtils.GetResponsibleStorageManager(seller);
		ctx.Equal(2, LF_InventoryUtils.AddAmount(seller, APPLE_PREFAB, 2), "seller gets 2 apples");
		array<IEntity> carried = LF_InventoryUtils.FindItemsByPrefab(manager, APPLE_PREFAB);
		ctx.Equal(1, carried.Count(), "the apples are one stack");
		if (ctx.FailureCount() > 0)
			return;

		IEntity apples = carried[0];

		// A vicinity move: the native move to the crate's storage, never the InsertItem override.
		manager.TryMoveItemToStorage(apples, crate);
		ctx.False(crate.Contains(apples), "a vicinity move does not put the apples in the trader crate");
		ctx.True(manager.Contains(apples), "the refused apples stay with the seller");
		ctx.Equal(100, LF_MoneyUtils.GetCash(seller), "a refused vicinity move pays nothing");

		// A direct insert of a loose apple into the crate's storage.
		IEntity loose = LF_Utils.SpawnEntityPrefab(APPLE_PREFAB, seller.GetOrigin() + Vector(0.5, 0, 0));
		ctx.NotNull(loose, "a loose apple spawns on the ground");
		if (loose)
		{
			manager.TryInsertItemInStorage(loose, crate);
			ctx.False(crate.Contains(loose), "a direct storage insert does not put the apple in the trader crate");
			ctx.Equal(100, LF_MoneyUtils.GetCash(seller), "a refused direct insert pays nothing");
			SCR_EntityHelper.DeleteEntityAndChildren(loose);
		}

		// The server sale still sells: 2 apples at 5.
		if (manager.Contains(apples))
		{
			ctx.True(LF_TraderSale.Sell(seller, apples, traderOwner, true) == LF_ETraderSale.SOLD, "the server sale still sells the apples");
			ctx.Equal(110, LF_MoneyUtils.GetCash(seller), "the seller is paid 10 cash (2 apples at 5)");
			ctx.Equal(0, LF_InventoryUtils.GetAmount(seller, APPLE_PREFAB), "the sold apples left the seller");
		}
		else if (apples)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(apples);
		}

		SCR_EntityHelper.DeleteEntityAndChildren(seller);
	}
};
