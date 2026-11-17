// red-proof: #2411, 2026-11-17, `tools/cli test --no-build --only LF_Test_VehicleLockedCargo,LF_Test_VehicleRetrieveKey` on the unfixed code
// (no LF_VehicleStorageLockGate): FAIL - a locked car takes no cargo, - nobody takes cargo out of a locked car,
// - the cargo is still in the locked car. Green with the gate (both locked takes log "take refused").
// tier: WORLD
//! A locked car's cargo is closed on the server (#2411): no inventory move puts an item in or
//! takes one out, whatever the client's UI showed. Unlocked, the same moves work.
class LF_Test_VehicleLockedCargo : LF_Test
{
	protected static const ResourceName APPLE = "{C9D661E5B0714711}Prefabs/Items/Food/Apple.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "vehicles/locked-cargo-server-gate";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		IEntity car = LF_Utils.SpawnEntityPrefab(LF_VehicleCatalog.S105_LOCKED, "140 0 140");
		ctx.True(car != null, "the lockable car spawns");
		if (!car)
			return;

		LF_VehicleLockComponent vehicleLock = LF_Component<LF_VehicleLockComponent>.Find(car);
		SCR_UniversalInventoryStorageComponent cargo = SCR_UniversalInventoryStorageComponent.Cast(car.FindComponent(SCR_UniversalInventoryStorageComponent));
		InventoryStorageManagerComponent carManager = InventoryStorageManagerComponent.Cast(car.FindComponent(InventoryStorageManagerComponent));
		ctx.True(vehicleLock != null && cargo != null && carManager != null, "the car has a lock, a cargo storage and an inventory manager");
		IEntity taker = LF_TestCarriedInventoryKit.SpawnLoaded(ctx);
		InventoryStorageManagerComponent takerManager;
		if (taker)
			takerManager = LF_InventoryUtils.GetResponsibleStorageManager(taker);

		IEntity stashed = LF_Utils.SpawnEntityPrefab(APPLE, car.GetOrigin() + "0 2 0");
		IEntity late = LF_Utils.SpawnEntityPrefab(APPLE, car.GetOrigin() + "0 2 0");
		if (!vehicleLock || !cargo || !carManager || !takerManager || !stashed || !late)
		{
			ctx.Fail("fixture: car, character or apples did not resolve");
			Cleanup(car, taker, stashed, late);
			return;
		}

		vehicleLock.SetLocked(false);
		ctx.True(carManager.TryInsertItemInStorage(stashed, cargo), "an open car takes cargo");

		vehicleLock.SetLocked(true);
		ctx.False(carManager.TryInsertItemInStorage(late, cargo), "a locked car takes no cargo");
		ctx.False(MoveInto(takerManager, stashed), "nobody takes cargo out of a locked car");
		ctx.True(cargo.Contains(stashed), "the cargo is still in the locked car");
		ctx.False(carManager.TryRemoveItemFromStorage(stashed, cargo), "the cargo cannot be dropped out of a locked car");

		vehicleLock.SetLocked(false);
		ctx.True(MoveInto(takerManager, stashed), "once unlocked, the cargo can be taken");
		ctx.False(cargo.Contains(stashed), "the taken apple left the car");

		Cleanup(car, taker, stashed, late);
	}

	//------------------------------------------------------------------------------------------------
	//! A real move into the character's inventory, the way an inventory drag does it: the manager
	//! picks a storage that takes the item, then moves it out of its current one.
	static bool MoveInto(InventoryStorageManagerComponent manager, IEntity item)
	{
		if (!manager || !item)
			return false;

		BaseInventoryStorageComponent to = manager.FindStorageForItem(item, EStoragePurpose.PURPOSE_DEPOSIT);
		if (!to)
			return false;

		return manager.TryMoveItemToStorage(item, to);
	}

	//------------------------------------------------------------------------------------------------
	protected void Cleanup(IEntity car, IEntity taker, IEntity stashed, IEntity late)
	{
		LF_TestCharacterFixture.Delete(stashed);
		LF_TestCharacterFixture.Delete(late);
		LF_TestCharacterFixture.Delete(taker);
		LF_TestCharacterFixture.Delete(car);
	}
}
