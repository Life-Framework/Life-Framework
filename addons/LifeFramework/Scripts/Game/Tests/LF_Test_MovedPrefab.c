// red-proof: observed 2026-11-12 with `tools\cli test --no-build --only LF_Test_MovedPrefab` on the sites before the GUID compare: `- the chop shop pays the moved S105 its listed value (expected 1000, got 800)`, `- the moved drill keeps its ShopPrices.conf price (expected 2500, got 50)`, `- a stored inventory row counts toward the moved prefab (expected 3, got 0)`, `- the moved name counts the carried apples (expected 3, got 1)` and seven more. Green after them.
//------------------------------------------------------------------------------------------------
//! prefab/moved-path (#323, M3 batch 0): a resource moved into the pack keeps its GUID, but
//! GetPrefabName() reports the new path (docs/architecture/pack-move-spike.md section 3). Every
//! string saved or configured before the move carries the old path. Each name below shares a real
//! GUID with a shipped prefab under a path that does not exist, the way a pre-move config or save
//! looks to a post-move entity (and the other way round). The lookups must still find the prefab.
// tier: WORLD
class LF_Test_MovedPrefab : LF_Test
{
	static const ResourceName APPLE = "{C9D661E5B0714711}Prefabs/Items/Food/Apple.et";
	static const ResourceName S105 = "{E1A105BE1A000001}Prefabs/Vehicles/Wheeled/S105/EL_S105_beige.et";
	static const ResourceName DRILL = "{4FF102B4CE9B0ED3}Prefabs/Items/Fence/LF_Drill.et";
	static const ResourceName PICKAXE = "{23341E2840F5ACB4}Prefabs/Tools/Pickaxe.et";
	static const ResourceName DRESSING = "{A81F501D3EF6F38E}Prefabs/Items/Medicine/FieldDressing_01/FieldDressing_US_01.et";
	static const ResourceName HOUSE_KEY = "{3735389521255DBE}Prefabs/Items/HouseKeyItem.et";
	static const string STORE_COUNTER = "Shop_GeneralStore_Debug";

	//------------------------------------------------------------------------------------------------
	//! The name the resource would report after a move to `path`: same GUID, new path.
	static ResourceName Moved(ResourceName original, string path)
	{
		return "{" + LF_Utils.PrefabGuid(original) + "}" + path;
	}

	override string GetName()
	{
		return "prefab/moved-path";
	}

	override void Run(LF_TestContext ctx)
	{
		ResourceName appleMoved = Moved(APPLE, "Prefabs/Moved/Food/Apple.et");
		ResourceName s105Moved = Moved(S105, "Prefabs/Moved/Vehicles/EL_S105_beige.et");
		ResourceName drillMoved = Moved(DRILL, "Prefabs/Moved/Fence/LF_Drill.et");
		ResourceName pickaxeMoved = Moved(PICKAXE, "Prefabs/Moved/Tools/Pickaxe.et");
		ResourceName dressingMoved = Moved(DRESSING, "Prefabs/Moved/Medicine/FieldDressing_US_01.et");
		ResourceName houseKeyMoved = Moved(HOUSE_KEY, "Prefabs/Moved/HouseKeyItem.et");
		ctx.True(appleMoved != APPLE && LF_Utils.SamePrefab(appleMoved, APPLE), "the moved apple differs in path and shares the GUID");

		// Config keyed by the old path, asked with the new one.
		ctx.Equal(1000, LF_ChopShopConfig.ValueOf(s105Moved), "the chop shop pays the moved S105 its listed value");

		LF_ShopManagerComponent shop = LF_ShopManagerComponent.GetInstance();
		ctx.NotNull(shop, "the shop manager is on the game mode");
		if (shop)
		{
			ctx.Equal(2500, shop.GetBuyPrice(drillMoved), "the moved drill keeps its ShopPrices.conf price");
			ctx.Equal(150, shop.GetBuyPrice(pickaxeMoved), "the moved pickaxe keeps its ShopPrices.conf price");
			ctx.Equal(8, shop.GetBuyPrice(appleMoved), "the moved apple finds its catalog row");
			ctx.True(shop.IsHidden(houseKeyMoved), "the moved house key stays out of every shop");
		}

		array<ref LF_TraderItem> tradable = {};
		LF_TraderItem row = new LF_TraderItem();
		row.m_ItemPrefab = APPLE;
		row.m_ValuePerItem = 7;
		tradable.Insert(row);
		ctx.Equal(7, LF_TraderManagerComponent.GetUnitValue(tradable, appleMoved), "a trader list from before the move prices the moved apple");

		// A save from before the move, read after it.
		LF_StorageSnapshotRecord saved = LF_StorageSnapshotRecord.Create("moved-path-test", LF_StorageSnapshot.SLOT_CARRIED);
		saved.m_aItems.Insert(LF_StorageItemRecord.Create(APPLE, 3, -1, 0, -1));
		ctx.Equal(3, saved.CountPrefab(appleMoved), "a stored inventory row counts toward the moved prefab");

		IEntity counter = GetGame().GetWorld().FindEntityByName(STORE_COUNTER);
		LF_StoreComponent store = null;
		if (counter)
			store = LF_Component<LF_StoreComponent>.Find(counter);

		ctx.NotNull(store, "the DebugWorld general store resolves");
		if (store)
			ctx.True(store.HasLine(appleMoved), "a store line configured before the move finds the moved apple");

		// Live items: the inventory search keys on the GUID.
		IEntity body = LF_TestCharacterFixture.Spawn(ctx);
		if (!body)
			return;

		ctx.Equal(3, LF_InventoryUtils.AddAmount(body, APPLE, 3), "3 apples added");
		ctx.Equal(3, LF_InventoryUtils.GetAmount(body, appleMoved), "the moved name counts the carried apples");
		ctx.Equal(1, LF_InventoryUtils.RemoveAmount(body, appleMoved, 1), "the moved name takes one apple");
		ctx.Equal(2, LF_InventoryUtils.GetAmount(body, APPLE), "two apples are left");

		// An item without LF_QuantityComponent (the revive dressing, a gather tool, a kit item) is
		// counted apart from the stack walk.
		ctx.Equal(2, LF_InventoryUtils.AddAmount(body, DRESSING, 2), "2 field dressings added");
		ctx.Equal(2, LF_InventoryUtils.GetAmount(body, DRESSING), "the shipped name counts the dressings");
		ctx.Equal(2, LF_InventoryUtils.GetAmount(body, dressingMoved), "the moved name counts the dressings");
		ctx.Equal(1, LF_InventoryUtils.RemoveAmount(body, dressingMoved, 1), "the moved name takes one dressing");
		ctx.Equal(1, LF_InventoryUtils.GetAmount(body, DRESSING), "one dressing is left");
		LF_TestCharacterFixture.Delete(body);
	}
}
