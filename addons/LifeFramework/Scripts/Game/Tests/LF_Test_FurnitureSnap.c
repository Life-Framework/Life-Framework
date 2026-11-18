// red-proof: observed 2026-11-18 with `--red` (log test-20261118-025311): RED-OK housing/furniture (fails with the hook on, 15 assertions): the outside refusal and the hung-shelf check are skipped. Hook off: green.
//! housing/furniture (#2654, epic #747): a chair is placed on the floor of the house the player owns and
//! refused when they stand away from any house of theirs; a shelf lies flush on a wall face (gap under
//! 2 cm) and the server refuses a proposal hung off the wall. The house is spawned and bound here, the
//! wall is a vanilla brick wall; the spots are measured from their origins, never typed.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_FurnitureSnap : LF_Test
{
	protected static const ResourceName BUILDING = "{74676598DF38F201}Prefabs/Structures/Core/EL_Building_Base.et";
	protected static const ResourceName WALL = "{419130031F0009C9}Prefabs/Structures/Walls/Brick/BrickWall_01/BrickWall_01_4m.et";
	protected static const ResourceName CHAIR_ITEM = "{D77E0D49C10C35BB}Prefabs/Placeables/Furniture/Furniture_Chair_Item.et";
	protected static const ResourceName SHELF_ITEM = "{6FAE5088FA3B3519}Prefabs/Placeables/Furniture/Furniture_Shelf_Item.et";
	protected static const string HOUSE = "furniture-snap-house";
	protected static const string OWNER = "furniture-snap-owner";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "housing/furniture";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_PlacementPolicyConfig.Reload();
		RunFloor(ctx);
		RunWall(ctx);
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
		IEntity building = LF_Utils.SpawnEntityPrefab(BUILDING, stand);
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

		// Inside their own house: the chair lands, on the floor. (A character only moves through its own
		// spawn logic here, so the house is built where they stand.)
		vector inside = stand + "0 0 1.5";
		IEntity chair = Place(player, CHAIR_ITEM, inside);
		ctx.NotNull(chair, "a chair lands inside the owned house: " + LF_PlacementService.LastRefusal() + " (player " + player.GetOrigin() + ", house " + building.GetOrigin() + ", live houses " + LF_HouseManagerComponent.Live().Count() + ")");
		if (chair)
		{
			float floorY = GetGame().GetWorld().GetSurfaceY(inside[0], inside[2]);
			ctx.True(Math.AbsFloat(chair.GetOrigin()[1] - floorY) < 0.05, "the chair stands on the floor: " + chair.GetOrigin());
			Cleanup(chair);
		}

		// The house is 60 m away from them now: refused, and the item stays with them.
		building.SetOrigin(stand + "60 0 0");
		vector outside = inside;
		IEntity refused;
		if (!RedProof())
			refused = Place(player, CHAIR_ITEM, outside);
		ctx.True(refused == null, "a chair outside the owned house is refused");
		ctx.True(LF_PlacementService.LastRefusal().Contains("refused"), "the refusal is logged: " + LF_PlacementService.LastRefusal());
		if (refused)
			Cleanup(refused);

		LF_TestCharacterFixture.Delete(player);
		LF_TestCharacterFixture.Delete(building);
		LF_Houses.ApplyAll(before);
	}

	//------------------------------------------------------------------------------------------------
	//! Server path: the player confirms `itemPrefab` at the floor spot `at` (forward +z).
	protected IEntity Place(IEntity player, ResourceName itemPrefab, vector at)
	{
		IEntity item = LF_Utils.SpawnEntityPrefab(itemPrefab, player.GetOrigin());
		at[1] = GetGame().GetWorld().GetSurfaceY(at[0], at[2]);
		vector proposal[4];
		LF_PlacementGeometry.Transform(at - "0 0 0.5", at + "0 0 0.5", proposal);
		IEntity placed = LF_PlacementService.Confirm(player, item, proposal);
		if (!placed)
			LF_TestCharacterFixture.Delete(item);

		return placed;
	}

	//------------------------------------------------------------------------------------------------
	protected void Cleanup(IEntity placed)
	{
		string id = LF_PlacementSave.IdOf(placed);
		if (!id.IsEmpty())
			LF_PlacementSave.Forget(id);
		LF_TestCharacterFixture.Delete(placed);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunWall(LF_TestContext ctx)
	{
		IEntity player = LF_TestCharacterFixture.Spawn(ctx);
		vector wallAt = "0 0 0";
		if (player)
			wallAt = player.GetOrigin() - "0 0 3"; // the wall lies 3 m south of where they stand, its face turned to them
		wallAt[1] = GetGame().GetWorld().GetSurfaceY(wallAt[0], wallAt[2]);
		IEntity wall = LF_Utils.SpawnEntityPrefab(WALL, wallAt);
		IEntity item = LF_Utils.SpawnEntityPrefab(SHELF_ITEM, wallAt + "0 0 6");
		LF_PlaceableComponent placeable = LF_Component<LF_PlaceableComponent>.Find(item);
		if (!wall || !player || !placeable)
		{
			ctx.Fail("the wall, the player and the shelf item spawn");
			LF_TestCharacterFixture.Delete(wall);
			LF_TestCharacterFixture.Delete(player);
			LF_TestCharacterFixture.Delete(item);
			return;
		}

		ctx.Equal(LF_EPlacementSnap.WALL, placeable.GetSnap(), "the shelf snaps to a wall");
		ctx.Equal(LF_EPlacementLifetime.PERMANENT, placeable.GetLifetime(), "the shelf is permanent");

		// The ray runs from the south side towards the wall (-z): the wall's face normal is the heading.
		vector transform[4];
		LF_PlacementProbe probe = new LF_PlacementProbe();
		bool clamped;
		LF_EPlacementVerdict verdict = LF_PlacementFurniture.PlanWallRay(player, placeable, item, wallAt + "1 1 3", "0 0 -1", transform, probe, clamped);
		ctx.Equal(LF_EPlacementVerdict.VALID, verdict, "the shelf spot on the wall is valid: " + LF_PlacementRules.Reason(verdict, probe, LF_PlacementService.MAX_STEP));
		ctx.False(clamped, "the wall is within reach");
		ctx.True(vector.Dot(transform[2], "0 0 1") > 0.99, "the shelf faces out of the wall: " + transform[2]);

		// Flush: from its middle back along the heading, the wall face lies at the footprint's back edge, under 2 cm.
		float minZ = placeable.GetFootprintMins()[2];
		vector from = transform[3] + Vector(0, 0.5, 0);
		array<IEntity> exclude = {player, item};
		vector hit;
		vector normal;
		bool struck = LF_PlacementFurniture.Strike(from, transform[2] * -1, -minZ + 1, exclude, hit, normal);
		ctx.True(struck, "the wall is behind the placed shelf");
		float gap = LF_PlacementFurniture.GapOf(vector.Distance(from, hit), minZ);
		ctx.True(gap > -0.005 && gap < 0.02, "the shelf lies flush on the wall face (gap " + gap + " m)");

		// The server's check: the flush proposal passes, one hung 10 cm off the wall is refused.
		ctx.EqualStr("", LF_PlacementFurniture.WallProblem(placeable, transform, player, item), "a flush proposal passes the server's wall check");
		vector hung[4];
		for (int i = 0; i < 4; i++)
			hung[i] = transform[i];
		hung[3] = transform[3] + transform[2] * 0.1;
		string why;
		if (!RedProof())
			why = LF_PlacementFurniture.WallProblem(placeable, hung, player, item);
		ctx.True(why.Contains("off the wall"), "a shelf hung 10 cm off the wall is refused: " + why);

		LF_TestCharacterFixture.Delete(wall);
		LF_TestCharacterFixture.Delete(player);
		LF_TestCharacterFixture.Delete(item);
	}
}
