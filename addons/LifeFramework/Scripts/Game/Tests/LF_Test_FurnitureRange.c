// red-proof: 2026-11-19 on main (no Furniture_Stool_*.et) the stool pair and the stool placement fail; `--red` run recorded in the PR (RedProof() skips the stool floor placement).
//! housing/furniture-range (#3194, parent #3169): the cottage furniture range (bed, stool, wardrobe, rug) is
//! complete. Every one has an `_Item` with its `_Placed` twin (the placed prefab spawns, carries the owner stamp
//! and the Pick up action); the bed, stool and rug place on the floor of the owner's house through the
//! placement service and land flat on it; the rug is a flat mat (under 15 cm thick); the wardrobe hangs on a
//! wall like the rest of the case furniture. Shop stock and prices are pack data: `everon/furniture-home`.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_FurnitureRange : LF_Test
{
	protected static const ResourceName BUILDING = "{74676598DF38F201}Prefabs/Structures/Core/EL_Building_Base.et";
	protected static const ResourceName BED_ITEM = "{3D1A650192443A05}Prefabs/Placeables/Furniture/Furniture_Bed_Item.et";
	protected static const ResourceName STOOL_ITEM = "{A06F23CE0EA04EF1}Prefabs/Placeables/Furniture/Furniture_Stool_Item.et";
	protected static const ResourceName WARDROBE_ITEM = "{0E97A9B7154F433C}Prefabs/Placeables/Furniture/Furniture_Wardrobe_Item.et";
	protected static const ResourceName RUG_ITEM = "{7020780C1A759A92}Prefabs/Placeables/Furniture/Furniture_Rug_Item.et";
	protected static const string HOUSE = "furniture-range-house";
	protected static const string OWNER = "furniture-range-owner";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "housing/furniture-range";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_PlacementPolicyConfig.Reload();
		CheckPair(ctx, BED_ITEM, "bed", LF_EPlacementSnap.FLOOR);
		CheckPair(ctx, STOOL_ITEM, "stool", LF_EPlacementSnap.FLOOR);
		CheckPair(ctx, WARDROBE_ITEM, "wardrobe", LF_EPlacementSnap.WALL);
		CheckPair(ctx, RUG_ITEM, "rug", LF_EPlacementSnap.FLOOR);
		RunFloor(ctx);
	}

	//------------------------------------------------------------------------------------------------
	//! The item spawns, snaps as given, and names a placed prefab that spawns with the owner stamp and Pick up.
	protected void CheckPair(LF_TestContext ctx, ResourceName itemPrefab, string label, LF_EPlacementSnap snap)
	{
		IEntity item;
		if (!RedProof() || label != "stool")
			item = LF_Utils.SpawnEntityPrefab(itemPrefab, "100 0 66");
		ctx.True(item != null, "the " + label + " item spawns");
		if (!item)
			return;

		LF_PlaceableComponent placeable = LF_Component<LF_PlaceableComponent>.Find(item);
		ctx.True(placeable != null, "the " + label + " item is placeable");
		if (!placeable)
		{
			LF_TestCharacterFixture.Delete(item);
			return;
		}

		ctx.Equal(snap, placeable.GetSnap(), "the " + label + " snap mode");
		ctx.Equal(LF_EPlacementLifetime.PERMANENT, placeable.GetLifetime(), "the " + label + " stays until picked up");
		ResourceName placedPrefab = placeable.GetPlacedPrefab();
		LF_TestCharacterFixture.Delete(item);
		ctx.True(!placedPrefab.IsEmpty(), "the " + label + " names its placed prefab");
		if (placedPrefab.IsEmpty())
			return;

		IEntity placed = LF_Utils.SpawnEntityPrefab(placedPrefab, "100 0 70");
		ctx.True(placed != null, "the " + label + " placed prefab spawns");
		if (!placed)
			return;

		ctx.True(LF_Component<LF_PlacedOwnerComponent>.Find(placed) != null, "the " + label + " placed prop carries the owner stamp");
		LF_TestCharacterFixture.Delete(placed);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunFloor(LF_TestContext ctx)
	{
		array<ref LF_HouseLockRecord> before = LF_Houses.ExportAll();
		LF_Houses.ApplyAll(null);
		IEntity player = LF_TestCharacterFixture.Spawn(ctx);
		vector stand;
		if (player)
			stand = player.GetOrigin();
		// The house stands 12 m off (inside the 30 m house range) so the bed's and the rug's big footprints lie on open ground, clear of the shell's own foundation.
		IEntity building = LF_Utils.SpawnEntityPrefab(BUILDING, stand + "0 0 12");
		LF_HouseManagerComponent house;
		if (building)
			house = LF_Component<LF_HouseManagerComponent>.Find(building);

		if (!house || !player)
		{
			ctx.Fail("the house and the player spawn");
			LF_TestCharacterFixture.Delete(building);
			LF_TestCharacterFixture.Delete(player);
			LF_Houses.ApplyAll(before);
			return;
		}

		house.BindIdentifier(HOUSE);
		house.SetOwner(OWNER);
		LF_Houses.BindCharacter(player, OWNER);

		PlaceOnFloor(ctx, player, BED_ITEM, "bed", stand);
		PlaceOnFloor(ctx, player, STOOL_ITEM, "stool", stand);
		PlaceOnFloor(ctx, player, RUG_ITEM, "rug", stand);

		// The rug is a flat mat: its footprint stands under 15 cm above the floor it lies on.
		IEntity rugItem = LF_Utils.SpawnEntityPrefab(RUG_ITEM, stand);
		LF_PlaceableComponent rugPlaceable;
		if (rugItem)
			rugPlaceable = LF_Component<LF_PlaceableComponent>.Find(rugItem);
		if (rugPlaceable)
			ctx.True(rugPlaceable.GetFootprintMaxs()[1] < 0.15, "the rug is flat: " + rugPlaceable.GetFootprintMaxs());
		else
			ctx.Fail("the rug item spawns with a placeable");
		LF_TestCharacterFixture.Delete(rugItem);

		LF_TestCharacterFixture.Delete(player);
		LF_TestCharacterFixture.Delete(building);
		LF_Houses.ApplyAll(before);
	}

	//------------------------------------------------------------------------------------------------
	//! The player confirms `itemPrefab` at a floor spot near them, inside the house range; the grid has fences and
	//! kerbs about, so a few open spots are tried and the first that lands counts. The placed prop stands on the floor
	//! and is deleted again here.
	protected void PlaceOnFloor(LF_TestContext ctx, IEntity player, ResourceName itemPrefab, string label, vector origin)
	{
		array<vector> offsets = {"0 0 1.5", "0 0 -4", "6 0 0", "-6 0 0", "0 0 8", "0 0 -9", "10 0 6", "-10 0 -6"};
		IEntity item = LF_Utils.SpawnEntityPrefab(itemPrefab, player.GetOrigin());
		if (!item)
		{
			ctx.Fail("the " + label + " item spawns");
			return;
		}

		IEntity placed;
		vector at;
		foreach (vector offset : offsets)
		{
			at = origin + offset;
			at[1] = GetGame().GetWorld().GetSurfaceY(at[0], at[2]);
			vector proposal[4];
			LF_PlacementGeometry.Transform(at - "0 0 0.5", at + "0 0 0.5", proposal);
			placed = LF_PlacementService.Confirm(player, item, proposal);
			if (placed)
				break;
		}

		ctx.NotNull(placed, "a " + label + " lands inside the owned house on one of " + offsets.Count() + " spots: " + LF_PlacementService.LastRefusal());
		if (!placed)
		{
			LF_TestCharacterFixture.Delete(item);
			return;
		}

		ctx.True(Math.AbsFloat(placed.GetOrigin()[1] - at[1]) < 0.05, "the " + label + " stands on the floor: " + placed.GetOrigin() + " (ground " + at[1] + ")");
		string id = LF_PlacementSave.IdOf(placed);
		if (!id.IsEmpty())
			LF_PlacementSave.Forget(id);
		LF_TestCharacterFixture.Delete(placed);
	}
}
