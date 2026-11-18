// red-proof: Observed 2026-11-18 with `tools\cli test --no-build --red LF_Test_SetHome` (the owner's perform skipped): `red: the owner's spawn house is A (expected 'set-home-house-a', got '')`, `RED PROOF OK`. Green with the action. Review 2026-11-18: `a house sold and bought back is not the spawn again (expected '', got 'set-home-house-b')` before the LF_HouseManagerComponent.SetOwner hook, green after.
// tier: WORLD
//! property/set-home (#2651): "Make this my spawn" at the door of a house the player owns. The owner
//! sets it and LF_Properties.SpawnHouseOf returns the house; a second house clears the first; a key
//! holder or a stranger neither sees it nor gets it by a forged perform; the flag survives the
//! "properties" export and apply.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_SetHome : LF_Test
{
	protected static const ResourceName BUILDING = "{74676598DF38F201}Prefabs/Structures/Core/EL_Building_Base.et";
	protected static const ResourceName DOOR = "{99D61E9EDBDF3CC8}Prefabs/Structures/BuildingParts/Doors/EL_Door_Base.et";
	protected static const ResourceName KEY = "{3735389521255DBE}Prefabs/Items/HouseKeyItem.et";
	protected static const string HOUSE_A = "set-home-house-a";
	protected static const string HOUSE_B = "set-home-house-b";
	protected static const string OWNER = "set-home-owner";
	protected static const string HOLDER = "set-home-holder";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "property/set-home";
	}

	//------------------------------------------------------------------------------------------------
	protected LF_SetHomeAction SetHomeAction(IEntity door)
	{
		BaseActionsManagerComponent manager = BaseActionsManagerComponent.Cast(door.FindComponent(BaseActionsManagerComponent));
		if (!manager)
			return null;

		array<BaseUserAction> actions = {};
		manager.GetActionsList(actions);
		foreach (BaseUserAction action : actions)
		{
			LF_SetHomeAction setHome = LF_SetHomeAction.Cast(action);
			if (setHome)
				return setHome;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		array<ref LF_HouseLockRecord> housesBefore = LF_Houses.ExportAll();
		array<ref LF_PropertyRecord> propertiesBefore = LF_Properties.ExportAll();
		LF_Houses.ApplyAll(null);
		LF_Properties.ApplyAll(null);

		IEntity buildingA = LF_Utils.SpawnEntityPrefab(BUILDING, "72 0 72");
		IEntity doorA = LF_Utils.SpawnEntityPrefab(DOOR, "72 0 72");
		IEntity buildingB = LF_Utils.SpawnEntityPrefab(BUILDING, "80 0 72");
		IEntity doorB = LF_Utils.SpawnEntityPrefab(DOOR, "80 0 72");
		IEntity key = LF_Utils.SpawnEntityPrefab(KEY, "72 0 73");
		IEntity holder = LF_TestCharacterFixture.Spawn(ctx);
		IEntity stranger = LF_TestCharacterFixture.Spawn(ctx);
		IEntity owner = LF_TestCharacterFixture.Spawn(ctx);
		ctx.NotNull(buildingA, "house A spawns");
		ctx.NotNull(buildingB, "house B spawns");
		ctx.NotNull(doorA, "door A spawns");
		ctx.NotNull(doorB, "door B spawns");
		ctx.NotNull(key, "the house key spawns");
		if (buildingA && buildingB && doorA && doorB && key && holder && stranger && owner)
		{
			buildingA.AddChild(doorA, -1);
			buildingB.AddChild(doorB, -1);
			RunHouses(ctx, buildingA, doorA, buildingB, doorB, key, holder, stranger, owner);
		}

		LF_TestCharacterFixture.Delete(holder);
		LF_TestCharacterFixture.Delete(stranger);
		LF_TestCharacterFixture.Delete(owner);
		LF_TestCharacterFixture.Delete(key);
		// A door added as a child is deleted with its building.
		if (buildingA)
			LF_TestCharacterFixture.Delete(buildingA);
		else
			LF_TestCharacterFixture.Delete(doorA);
		if (buildingB)
			LF_TestCharacterFixture.Delete(buildingB);
		else
			LF_TestCharacterFixture.Delete(doorB);
		LF_Properties.ApplyAll(propertiesBefore);
		LF_Houses.ApplyAll(housesBefore);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunHouses(LF_TestContext ctx, IEntity buildingA, IEntity doorA, IEntity buildingB, IEntity doorB, IEntity key, IEntity holder, IEntity stranger, IEntity owner)
	{
		LF_HouseManagerComponent houseA = LF_Component<LF_HouseManagerComponent>.Find(buildingA);
		LF_HouseManagerComponent houseB = LF_Component<LF_HouseManagerComponent>.Find(buildingB);
		LF_KeyComponent keyComponent = LF_Component<LF_KeyComponent>.Find(key);
		LF_SetHomeAction actionA = SetHomeAction(doorA);
		LF_SetHomeAction actionB = SetHomeAction(doorB);
		ctx.NotNull(houseA, "house A carries a house manager");
		ctx.NotNull(houseB, "house B carries a house manager");
		ctx.NotNull(keyComponent, "the key carries LF_KeyComponent");
		ctx.NotNull(actionA, "door A has the set-home action");
		ctx.NotNull(actionB, "door B has the set-home action");
		if (!houseA || !houseB || !keyComponent || !actionA || !actionB)
			return;

		ctx.True(houseA.BindIdentifier(HOUSE_A), "house A binds a stable identifier");
		ctx.True(houseB.BindIdentifier(HOUSE_B), "house B binds a stable identifier");
		keyComponent.SetHouseIdentifier(HOUSE_A);
		InventoryStorageManagerComponent storage = LF_InventoryUtils.GetResponsibleStorageManager(holder);
		ctx.True(storage && storage.TryInsertItem(key, EStoragePurpose.PURPOSE_DEPOSIT), "the key goes into the holder's pockets");
		LF_Houses.BindCharacter(owner, OWNER);
		LF_Houses.BindCharacter(holder, HOLDER);
		houseA.SetOwner(OWNER);
		houseB.SetOwner(OWNER);
		ctx.EqualStr("", LF_Properties.SpawnHouseOf(OWNER), "the owner starts with no spawn house");

		// Who sees it.
		ctx.True(actionA.CanBeShownScript(owner), "the owner sees 'Make this my spawn'");
		ctx.False(actionA.CanBeShownScript(holder), "a key holder does not see it");
		ctx.False(actionA.CanBeShownScript(stranger), "a stranger does not see it");

		// Forged performs and direct calls by others are refused.
		actionA.PerformAction(doorA, holder);
		actionA.PerformAction(doorA, stranger);
		ctx.EqualStr("", LF_Properties.SpawnHouseOf(HOLDER), "a key holder's forged perform sets nothing");
		ctx.EqualStr("", LF_Properties.SpawnHouseOf(OWNER), "forged performs do not set the owner's spawn");
		ctx.False(LF_Properties.SetSpawnHouse(HOLDER, HOUSE_A), "the book refuses a key holder");
		ctx.False(LF_Properties.SetSpawnHouse("", HOUSE_A), "the book refuses an unknown caller");

		// The owner sets house A, then house B clears it.
		if (!RedProof())
			actionA.PerformAction(doorA, owner);
		ctx.EqualStr(HOUSE_A, LF_Properties.SpawnHouseOf(OWNER), "the owner's spawn house is A");
		ctx.False(actionA.CanBeShownScript(owner), "house A no longer offers the action once it is the spawn");
		ctx.True(actionB.CanBeShownScript(owner), "house B still offers it");
		actionB.PerformAction(doorB, owner);
		ctx.EqualStr(HOUSE_B, LF_Properties.SpawnHouseOf(OWNER), "the owner's spawn house moves to B");
		LF_PropertyRecord rowA = LF_Properties.Get(HOUSE_A);
		ctx.True(rowA && !rowA.m_bOwnerSpawnsHere, "setting B clears the flag on A");

		// The flag round-trips through the "properties" payload.
		array<ref LF_PropertyRecord> rows = LF_Properties.ExportAll();
		LF_Properties.ApplyAll(null);
		ctx.EqualStr("", LF_Properties.SpawnHouseOf(OWNER), "an empty book has no spawn house");
		LF_Properties.ApplyAll(rows);
		ctx.EqualStr(HOUSE_B, LF_Properties.SpawnHouseOf(OWNER), "the spawn house is back after apply");

		// A sold house no longer counts.
		houseB.SetOwner(HOLDER);
		ctx.EqualStr("", LF_Properties.SpawnHouseOf(OWNER), "a house sold to someone else is not the old owner's spawn");
		ctx.EqualStr("", LF_Properties.SpawnHouseOf(HOLDER), "the buyer does not inherit the spawn flag");
		// Buying it back in the same session does not bring the old choice back: a sale cleared it.
		houseB.SetOwner(OWNER);
		ctx.EqualStr("", LF_Properties.SpawnHouseOf(OWNER), "a house sold and bought back is not the spawn again");
		houseB.SetOwner(HOLDER);
		LF_Properties.ApplyAll(LF_Properties.ExportAll());
		ctx.EqualStr("", LF_Properties.SpawnHouseOf(HOLDER), "the buyer does not inherit it through a save");
	}
}
