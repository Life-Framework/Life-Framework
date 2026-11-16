// red-proof: observed 2026-11-16 (#1891), `tools\cli test --no-build --no-cache --only <the 10 classes>` on GridWorld before the grid fixtures: 10 of 15 failed on `... resolves in the DebugWorld` (Shop_GeneralStore_Debug, Shop_BlackMarket_Debug, FenceNpc are town entities); on Grid_shops_till fixtures 15 of 15 pass.
// red-proof: observed 2026-11-14 with `tools\cli test --no-build --only LF_Test_StoreStaff`: CollectStaff without the
// boundary check (s_pStaffStore.Contains) failed `- the shopkeeper outside is not`; Sell without the
// CarriesHeldApart check failed `- an unpaid copy cannot be sold back to the shop` and `- the unpaid apple
// stays in the pocket (expected 1, got 0)`; both restored, green.

// tier: WORLD
//! A store's staff (#1143): the shopkeeper prefab carries the text-only shopkeeper dialogue, which has
//! a line for the walk-out key; a shopkeeper standing inside a store's boundary is that store's staff,
//! one standing outside it is not, and a shoplifting walk-out makes the staff say the line. Goods
//! carried unpaid cannot be sold back to the shop (#1143 review).
class LF_Test_StoreStaff : LF_Test
{
	static const ResourceName SHOPKEEPER = "{AB73D0036F42E73E}Prefabs/Characters/Npcs/LF_Shopkeeper.et";
	static const ResourceName APPLE = "{C9D661E5B0714711}Prefabs/Items/Food/Apple.et";
	static const string STORE_COUNTER = "Grid_shops_till_Store";

	override string GetName()
	{
		return "store/staff-react";
	}

	override void Run(LF_TestContext ctx)
	{
		IEntity counter = GetGame().GetWorld().FindEntityByName(STORE_COUNTER);
		LF_StoreComponent store;
		if (counter)
			store = LF_Component<LF_StoreComponent>.Find(counter);

		ctx.NotNull(store, "the general store resolves");
		if (!store)
			return;

		IEntity inside = LF_Utils.SpawnEntityPrefab(SHOPKEEPER, counter.CoordToParent("1 0 2"));
		IEntity outside = LF_Utils.SpawnEntityPrefab(SHOPKEEPER, counter.CoordToParent("0 0 12"));
		ctx.NotNull(inside, "a shopkeeper spawns inside the store");
		ctx.NotNull(outside, "a shopkeeper spawns outside it");
		if (inside && outside)
			Check(ctx, store, inside, outside);

		SCR_EntityHelper.DeleteEntityAndChildren(inside);
		SCR_EntityHelper.DeleteEntityAndChildren(outside);
	}

	//------------------------------------------------------------------------------------------------
	protected void Check(LF_TestContext ctx, LF_StoreComponent store, IEntity inside, IEntity outside)
	{
		LF_DialogueConfig config = LF_DialogueComponent.ConfigOf(inside);
		ctx.NotNull(config, "the shopkeeper has a playable dialogue");
		if (!config)
			return;

		ctx.EqualStr("shopkeeper", config.m_sId, "it is the shopkeeper's dialogue");
		ctx.True(LF_DialogueFlow.SpokenFor(config, LF_Stores.KEY_STAFF_THIEF) != LF_Stores.KEY_STAFF_THIEF, "it has a line for a walk-out");
		ctx.True(store.Contains(inside.GetOrigin()) && !store.Contains(outside.GetOrigin()), "one stands inside the store, one outside");

		array<IEntity> staff = {};
		LF_Stores.StaffFor(store, LF_Stores.KEY_STAFF_THIEF, staff);
		ctx.True(staff.Find(inside) != -1, "the shopkeeper inside is the store's staff");
		ctx.True(staff.Find(outside) == -1, "the shopkeeper outside is not");

		// A walk-out: a shopper carrying a tagged apple leaves; the staff speak.
		IEntity thief = LF_TestCharacterFixture.Spawn(ctx);
		IEntity apple = LF_Utils.SpawnEntityPrefab(APPLE, store.GetOwner().GetOrigin() + "0 2 0");
		InventoryStorageManagerComponent storage;
		if (thief)
			storage = LF_InventoryUtils.GetResponsibleStorageManager(thief);

		if (thief && apple && storage && storage.TryInsertItem(apple))
		{
			LF_Stores.ClearUnpaid();
			LF_Stores.Tag(store, apple, APPLE, 8);
			LF_ShopComponent shop = store.GetShop();
			ctx.True(shop && !shop.SellItem(APPLE, 1, thief), "an unpaid copy cannot be sold back to the shop");
			ctx.Equal(1, LF_InventoryUtils.GetAmount(thief, APPLE), "the unpaid apple stays in the pocket");
			ctx.Equal(staff.Count(), LF_Stores.StaffReact(store, LF_Stores.KEY_STAFF_THIEF), "every member of staff speaks on a walk-out");
			ctx.True(LF_Stores.Shoplift(store, thief) > 0, "the walk-out is booked");
		}
		else
		{
			ctx.Fail("the test shopper could not carry the apple");
		}

		LF_Stores.ClearUnpaid();
		LF_TestCharacterFixture.Delete(thief);
	}
}
