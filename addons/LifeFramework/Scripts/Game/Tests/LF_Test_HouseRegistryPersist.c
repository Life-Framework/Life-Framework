// red-proof: Observed 2026-11-17 with `tools\cli test --no-build --red LF_Test_HouseRegistryPersist,LF_Test_HouseCarriedKeyPersist`: `RED-OK houses/registry-persist` with the payload read skipped, `- the house keeps its owner after the load (expected 'registry-persist-owner', got '')`, `- the row of a house that is not in the world is kept`. Green with the read.
// tier: PERSISTENCE
//! houses/registry-persist (#2410): the "houses" payload round trips a house's lock state and owner
//! under its stable identifier, mirrors both onto its doors, keeps the row of a house that is not in
//! the world, applies a row that loads before its house registers, re-syncs a door that drifted, and a
//! re-applied load is idempotent. Across a restart (`tools\cli wt e2e`): the save boot stores a locked,
//! owned house; the reload boot finds it.
class LF_Test_HouseRegistryPersist : LF_Test
{
	protected static const ResourceName BUILDING = "{74676598DF38F201}Prefabs/Structures/Core/EL_Building_Base.et";
	protected static const ResourceName DOOR = "{99D61E9EDBDF3CC8}Prefabs/Structures/BuildingParts/Doors/EL_Door_Base.et";
	protected static const string HOUSE = "registry-persist-house";
	protected static const string ABSENT = "registry-persist-absent";
	protected static const string OWNER = "registry-persist-owner";
	protected static const string E2E_HOUSE = "houses-e2e-house";
	protected static const string E2E_OWNER = "houses-e2e-owner";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "houses/registry-persist";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		LF_PersistenceComponent persistence;
		if (gameMode)
			persistence = LF_PersistenceComponent.Cast(gameMode.FindComponent(LF_PersistenceComponent));
		ctx.NotNull(persistence, "the game mode carries LF_PersistenceComponent");
		if (!persistence)
			return;

#ifdef LF_E2E_LOAD_BOOT
		// --legacy-save and --pre-move load a save older than the "houses" payload: it has no rows.
		if (LF_SaveStamp.BootSaveVersion() < LF_PersistenceComponentSerializer.PayloadSinceVersion("houses"))
		{
			ctx.True(LF_Houses.Get(E2E_HOUSE) == null, "a save older than the houses payload loads no house row");
			return;
		}

		// The reload boot: what the save boot stored came back from the real save.
		LF_HouseLockRecord restored = LF_Houses.Get(E2E_HOUSE);
		ctx.NotNull(restored, "the house row is back after the restart");
		if (restored)
		{
			ctx.True(restored.m_bLocked, "the house is still locked after the restart");
			ctx.EqualStr(E2E_OWNER, restored.m_sOwnerKey, "the house keeps its owner after the restart");
		}

		// The row is this test's own; it is not saved again.
		LF_Houses.ApplyAll(null);
		return;
#endif

		ctx.True(LF_PersistenceComponentSerializer.PayloadSinceVersion("houses") > 0, "houses is a registered payload");

		array<ref LF_HouseLockRecord> before = LF_Houses.ExportAll();
		LF_Houses.ApplyAll(null);

		IEntity building = LF_Utils.SpawnEntityPrefab(BUILDING, "66 0 66");
		IEntity door = LF_Utils.SpawnEntityPrefab(DOOR, "66 0 66");
		if (building && door)
		{
			building.AddChild(door, -1);
			RunHouse(ctx, persistence, building, door);
		}
		else
		{
			ctx.Fail("the building and its door spawn");
		}

		// The door is the building's child once added, so deleting the building deletes it.
		if (building)
			LF_TestCharacterFixture.Delete(building);
		else
			LF_TestCharacterFixture.Delete(door);
		LF_Houses.ApplyAll(before);
#ifdef LF_E2E_HOST_STOP
		// The save boot: the shutdown save carries one locked, owned house.
		LF_Houses.Remember(LF_HouseLockRecord.Create(E2E_HOUSE, true, E2E_OWNER));
		ctx.NotNull(LF_Houses.Get(E2E_HOUSE), "the save boot stored the e2e house");
#endif
	}

	//------------------------------------------------------------------------------------------------
	protected void RunHouse(LF_TestContext ctx, LF_PersistenceComponent persistence, IEntity building, IEntity door)
	{
		LF_HouseManagerComponent house = LF_Component<LF_HouseManagerComponent>.Find(building);
		LF_LockComponent doorLock = LF_Component<LF_LockComponent>.Find(door);
		ctx.NotNull(house, "the building carries LF_HouseManagerComponent");
		ctx.NotNull(doorLock, "the door carries a lock");
		if (!house || !doorLock)
			return;

		ctx.False(house.HasStableIdentifier(), "a house with no editor identifier is not saved");
		ctx.True(house.IsHouseLocked() == doorLock.IsLocked(), "the house starts as its door starts");
		ctx.True(house.BindIdentifier(HOUSE), "the house is bound to a stable identifier");
		ctx.True(LF_Houses.Find(HOUSE) == house, "the bound house is registered");

		// A second house with the same identifier is refused and never writes the first one's row.
		IEntity twin = LF_Utils.SpawnEntityPrefab(BUILDING, "67 0 67");
		LF_HouseManagerComponent twinHouse;
		if (twin)
			twinHouse = LF_Component<LF_HouseManagerComponent>.Find(twin);
		ctx.NotNull(twinHouse, "a second building spawns");
		if (twinHouse)
		{
			ctx.False(twinHouse.BindIdentifier(HOUSE), "a second house cannot take a used identifier");
			ctx.False(twinHouse.HasStableIdentifier(), "and is not saved");
			twinHouse.SetOwner("twin-owner");
			ctx.True(LF_Houses.Get(HOUSE).m_sOwnerKey != "twin-owner", "the twin's owner never reaches the first house's row");
			ctx.True(LF_Houses.Find(HOUSE) == house, "the first house stays registered");
		}
		LF_TestCharacterFixture.Delete(twin);
		ctx.EqualStr(HOUSE, doorLock.GetHouseIdentifier(), "the door carries the house identifier");

		house.SetOwner(OWNER);
		house.SetHouseLocked(true);
		ctx.True(doorLock.IsLocked(), "the door is locked with the house");
		ctx.EqualStr(OWNER, doorLock.GetOwnerKey(), "the door carries the owner");

		// A breach opens the door; locking the already-locked house closes it again.
		doorLock.Unlock();
		house.LockAllDoors();
		ctx.True(doorLock.IsLocked(), "a drifted door is locked again by the next lock");

		LF_Houses.Remember(LF_HouseLockRecord.Create(ABSENT, true, "absent-owner"));

		JsonSaveContext saveContext = new JsonSaveContext(false);
		LF_PersistenceComponentSerializer.WritePersistencePayload(persistence, saveContext, "houses");
		string saved = saveContext.SaveToString();
		ctx.True(saved.Contains(OWNER), "the save carries the owner");

		// A fresh session: the house is unowned and open, the book is empty.
		house.SetOwner("");
		house.SetHouseLocked(false);
		LF_Houses.ApplyAll(null);
		ctx.True(LF_Houses.Get(ABSENT) == null, "the restarted book has no absent house");

		JsonLoadContext loadContext = new JsonLoadContext(false);
		ctx.True(loadContext.LoadFromString(saved), "the save parses");
		if (!RedProof())
			LF_PersistenceComponentSerializer.ReadPersistencePayload(persistence, loadContext, "houses");

		ctx.True(house.IsHouseLocked(), "the house is locked after the load");
		ctx.EqualStr(OWNER, house.GetOwnerKey(), "the house keeps its owner after the load");
		ctx.True(doorLock.IsLocked(), "the door is locked after the load");
		ctx.EqualStr(OWNER, doorLock.GetOwnerKey(), "the door carries the owner after the load");
		ctx.NotNull(LF_Houses.Get(ABSENT), "the row of a house that is not in the world is kept");

		// Loading the same rows twice is idempotent.
		int count = LF_Houses.ExportAll().Count();
		LF_Houses.ApplyAll(LF_Houses.ExportAll());
		ctx.Equal(count, LF_Houses.ExportAll().Count(), "re-applying the rows changes nothing");
		ctx.True(house.IsHouseLocked(), "and the house is still locked");

		// A row that loads before its house: the house takes it when it registers.
		array<ref LF_HouseLockRecord> rows = LF_Houses.ExportAll();
		house.SetOwner("");
		house.SetHouseLocked(false);
		LF_Houses.Unregister(house);
		LF_Houses.ApplyAll(rows);
		ctx.False(house.IsHouseLocked(), "an unregistered house is not touched by the load");
		LF_Houses.Register(house);
		ctx.True(house.IsHouseLocked(), "the house takes its saved lock when it registers");
		ctx.EqualStr(OWNER, house.GetOwnerKey(), "and its saved owner");

		// A row with no identifier is refused.
		ctx.False(LF_Houses.Remember(LF_HouseLockRecord.Create("", true, OWNER)), "a row with no identifier is refused");
		ctx.False(house.ApplyLockState(LF_HouseLockRecord.Create(ABSENT, false)), "a row for another house is not applied");
		ctx.True(house.IsHouseLocked(), "and leaves the house as it was");
	}
}
