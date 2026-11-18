// red-proof: observed 2026-11-18 with `tools\cli test --no-build --red LF_Test_EveronFurnitureHome`: RED PROOF OK (RedProof() skips the catalogue lookup, so every item fails "the general store catalogue lists ..."). Green in the pack leg with the prices.
//! shop/furniture-home: the fourteen vanilla-prop furniture items a 1989 villager furnishes a home with
//! (beds, sofa, armchair, coffee table, rug, television, radio, plant, mirror, picture, stove, bookcase,
//! workbench) are stocked by the general store at a price above their sell-back, place a permanent prop
//! that carries the owner stamp and the Pick up action, and answer to a house-owner-only policy row
//! inside an owned property. The mirror and the picture hang above the floor; the rest stand on it.
//! Lives in the pack because it needs the pack's price overlay (pack leg).
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronFurnitureHome : LF_Test
{
	protected static const vector SPOT = "100 0 66"; // inside the test annex (x/z 18..114), never on the town

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/furniture-home";
	}

	//------------------------------------------------------------------------------------------------
	protected static void Items(notnull array<ResourceName> items)
	{
		items.Insert("{3D1A650192443A05}Prefabs/Placeables/Furniture/Furniture_Bed_Item.et");
		items.Insert("{C852BF2548DC9F10}Prefabs/Placeables/Furniture/Furniture_BedDouble_Item.et");
		items.Insert("{9E1400ACB573DB0B}Prefabs/Placeables/Furniture/Furniture_Sofa_Item.et");
		items.Insert("{D4C9091800ACED97}Prefabs/Placeables/Furniture/Furniture_Armchair_Item.et");
		items.Insert("{9C4541040DD4C49A}Prefabs/Placeables/Furniture/Furniture_CoffeeTable_Item.et");
		items.Insert("{7020780C1A759A92}Prefabs/Placeables/Furniture/Furniture_Rug_Item.et");
		items.Insert("{83014311A81CBAE5}Prefabs/Placeables/Furniture/Furniture_Tv_Item.et");
		items.Insert("{253DBC7D9ACD8DEB}Prefabs/Placeables/Furniture/Furniture_Radio_Item.et");
		items.Insert("{1E56C8013F719C43}Prefabs/Placeables/Furniture/Furniture_Plant_Item.et");
		items.Insert("{19A401060FDD6B93}Prefabs/Placeables/Furniture/Furniture_Mirror_Item.et");
		items.Insert("{204F1A5C4E4137DE}Prefabs/Placeables/Furniture/Furniture_Picture_Item.et");
		items.Insert("{2FBFFB568731A3AF}Prefabs/Placeables/Furniture/Furniture_Stove_Item.et");
		items.Insert("{183AE088E111C537}Prefabs/Placeables/Furniture/Furniture_Bookcase_Item.et");
		items.Insert("{B54C355FF2C0C37D}Prefabs/Placeables/Furniture/Furniture_Workbench_Item.et");
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_PlacementPickupAction FindPickUp(IEntity entity)
	{
		BaseActionsManagerComponent manager = BaseActionsManagerComponent.Cast(entity.FindComponent(BaseActionsManagerComponent));
		if (!manager)
			return null;

		array<BaseUserAction> actions = {};
		manager.GetActionsList(actions);
		foreach (BaseUserAction action : actions)
		{
			LF_PlacementPickupAction pickUp = LF_PlacementPickupAction.Cast(action);
			if (pickUp)
				return pickUp;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_ShopManagerComponent manager = LF_ShopManagerComponent.GetInstance();
		ctx.NotNull(manager, "the shop manager exists");
		if (!manager)
			return;

		array<ResourceName> items = {};
		Items(items);
		ctx.True(items.Count() == 14, "fourteen home furniture items are listed");
		int n;
		foreach (ResourceName prefab : items)
		{
			CheckStocked(ctx, manager, prefab);
			CheckPlaceable(ctx, prefab, n);
			n++;
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckStocked(LF_TestContext ctx, LF_ShopManagerComponent manager, ResourceName prefab)
	{
		LF_ShopCatalogItem entry = null;
		if (!RedProof())
			entry = manager.GetItem(prefab);

		ctx.NotNull(entry, "the general store catalogue lists " + prefab);
		if (!entry)
			return;

		ctx.False(entry.m_bHidden, "not hidden from the shop: " + prefab);
		ctx.True(entry.m_iSellPrice > 0, string.Format("has a sell-back value (%1): %2", entry.m_iSellPrice, prefab));
		ctx.True(entry.m_iBuyPrice > entry.m_iSellPrice, string.Format("costs more (%1) than it sells back for (%2), no profit loop: %3", entry.m_iBuyPrice, entry.m_iSellPrice, prefab));

		LF_ShopPricesConfig prices = manager.GetConfig();
		bool hasRule = false;
		if (prices && prices.m_aPriceRules)
		{
			foreach (LF_ShopPriceRule rule : prices.m_aPriceRules)
			{
				if (rule && rule.m_Prefab == prefab)
					hasRule = true;
			}
		}

		ctx.True(hasRule, "has its own general store price rule: " + prefab);
	}

	//------------------------------------------------------------------------------------------------
	//! The item spawns with a placeable that names a placed prefab, a permanent lifetime, a floor or wall snap
	//! and a real footprint; its kind needs a house owner inside an owned property; the placed prefab carries the
	//! owner stamp and the Pick up action.
	protected void CheckPlaceable(LF_TestContext ctx, ResourceName prefab, int n)
	{
		vector at = SPOT + Vector(n * 3, 0, 0);
		at[1] = GetGame().GetWorld().GetSurfaceY(at[0], at[2]);
		IEntity item = LF_Utils.SpawnEntityPrefab(prefab, at);
		ctx.True(item != null, "the item spawns: " + prefab);
		if (!item)
			return;

		LF_PlaceableComponent placeable = LF_Component<LF_PlaceableComponent>.Find(item);
		ctx.True(placeable != null, "the item has a placeable component: " + prefab);
		if (!placeable)
		{
			LF_TestCharacterFixture.Delete(item);
			return;
		}

		string kind = placeable.GetKind();
		ctx.True(placeable.GetLifetime() == LF_EPlacementLifetime.PERMANENT, kind + " stays until picked up");
		LF_EPlacementSnap snap = placeable.GetSnap();
		ctx.True(snap == LF_EPlacementSnap.FLOOR || snap == LF_EPlacementSnap.WALL, kind + " snaps to the floor or a wall");
		vector mins = placeable.GetFootprintMins();
		vector maxs = placeable.GetFootprintMaxs();
		ctx.True(maxs[0] > mins[0] && maxs[1] > mins[1] && maxs[2] > mins[2], kind + " has a footprint with volume");
		bool hung = kind == "Furniture_Mirror" || kind == "Furniture_Picture";
		if (hung)
			ctx.True(snap == LF_EPlacementSnap.WALL && placeable.GetHangHeight() > 1, kind + " hangs on a wall above the floor");
		else
			ctx.True(placeable.GetHangHeight() == 0, kind + " stands on the floor");

		string minRank;
		ctx.True(LF_PlacementPolicy.RuleFor(kind, LF_EPlacementWho.ANYONE, minRank) == LF_EPlacementWho.HOUSE_OWNER_OR_KEY, kind + " is for a house owner or key holder");
		float radius;
		ctx.True(LF_PlacementPolicy.WhereFor(kind, radius) == LF_EPlacementWhere.OWNED_PROPERTY, kind + " goes inside an owned property");

		ResourceName placedPrefab = placeable.GetPlacedPrefab();
		LF_TestCharacterFixture.Delete(item);
		ctx.True(!placedPrefab.IsEmpty(), kind + " names its placed prefab");
		if (placedPrefab.IsEmpty())
			return;

		vector placedAt = at + "0 0 6";
		IEntity placed = LF_Utils.SpawnEntityPrefab(placedPrefab, placedAt);
		ctx.True(placed != null, kind + " placed prefab spawns");
		if (!placed)
			return;

		ctx.True(LF_Component<LF_PlacedOwnerComponent>.Find(placed) != null, kind + " placed prop carries the owner stamp");
		ctx.True(FindPickUp(placed) != null, kind + " placed prop carries the Pick up action");
		LF_TestCharacterFixture.Delete(placed);
	}
}
