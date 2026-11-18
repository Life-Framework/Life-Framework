// red-proof: observed 2026-11-18 with `tools\cli test --no-build --red LF_Test_FurnitureStorage,LF_Test_FurnitureStoragePersist,LF_Test_EveronFurnitureStorageStock` (RED PROOF OK, all three fail with the hook on): LF_Test_EveronFurnitureStorageStock: RED-OK (RedProof() skips the catalogue lookup, so every item fails "the general store catalogue lists ..."). Green in the pack leg.
//! shop/furniture-storage-stock: the seven storage furniture items (wardrobe, dresser, cupboard, chest, crate,
//! fridge, kitchen unit) are stocked by the 1989 general store at a price above their sell-back, each item
//! spawns a permanent placeable for a house owner with its own storage placed prefab. Pack leg: it needs the
//! pack's price overlay.
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronFurnitureStorageStock : LF_Test
{
	protected static const vector SPOT = "100 0 66"; // inside the test annex (x/z 18..114), never on the town

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/furniture-storage-stock";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_ShopManagerComponent manager = LF_ShopManagerComponent.GetInstance();
		ctx.NotNull(manager, "the shop manager exists");
		if (!manager)
			return;

		array<ResourceName> items = {};
		items.Insert("{0E97A9B7154F433C}Prefabs/Placeables/Furniture/Furniture_Wardrobe_Item.et");
		items.Insert("{941DBD8448A871E1}Prefabs/Placeables/Furniture/Furniture_Dresser_Item.et");
		items.Insert("{5D85C77ADE2060FB}Prefabs/Placeables/Furniture/Furniture_Cupboard_Item.et");
		items.Insert("{6032CD1DDDFC74E6}Prefabs/Placeables/Furniture/Furniture_Chest_Item.et");
		items.Insert("{8D53E1A331522CD1}Prefabs/Placeables/Furniture/Furniture_Crate_Item.et");
		items.Insert("{1822D7B7126462CB}Prefabs/Placeables/Furniture/Furniture_Fridge_Item.et");
		items.Insert("{826F18460FD13AE8}Prefabs/Placeables/Furniture/Furniture_KitchenUnit_Item.et");
		ctx.Equal(7, items.Count(), "seven storage furniture items are listed");
		int n;
		foreach (ResourceName prefab : items)
		{
			LF_ShopCatalogItem entry = null;
			if (!RedProof())
				entry = manager.GetItem(prefab);

			ctx.NotNull(entry, "the general store catalogue lists " + prefab);
			if (entry)
			{
				ctx.False(entry.m_bHidden, "not hidden from the shop: " + prefab);
				ctx.True(entry.m_iSellPrice > 0 && entry.m_iBuyPrice > entry.m_iSellPrice, string.Format("buy %1 beats sell-back %2, no profit loop: %3", entry.m_iBuyPrice, entry.m_iSellPrice, prefab));
			}

			vector at = SPOT + Vector(n * 3, 0, 0);
			at[1] = GetGame().GetWorld().GetSurfaceY(at[0], at[2]);
			IEntity item = LF_Utils.SpawnEntityPrefab(prefab, at);
			LF_PlaceableComponent placeable = LF_Component<LF_PlaceableComponent>.Find(item);
			ctx.True(placeable != null, "the item has a placeable component: " + prefab);
			if (placeable)
			{
				ctx.True(placeable.GetLifetime() == LF_EPlacementLifetime.PERMANENT, placeable.GetKind() + " stays until picked up");
				string minRank;
				ctx.True(LF_PlacementPolicy.RuleFor(placeable.GetKind(), LF_EPlacementWho.ANYONE, minRank) == LF_EPlacementWho.HOUSE_OWNER_OR_KEY, placeable.GetKind() + " is for a house owner or key holder");
				IEntity placed = LF_Utils.SpawnEntityPrefab(placeable.GetPlacedPrefab(), at + "0 0 6");
				ctx.True(placed != null && LF_PlacedStorage.ComponentOf(placed) != null, placeable.GetKind() + " places a prop with persistent storage");
				LF_TestCharacterFixture.Delete(placed);
			}

			LF_TestCharacterFixture.Delete(item);
			n++;
		}
	}
}
