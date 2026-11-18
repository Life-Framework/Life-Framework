// red-proof: #2835, 2026-11-18 `tools/cli test --no-build --only LF_Test_ServerInsertScopeAborted` against a shim with the
// new signatures but the old global flag: FAIL - with an aborted insert left open, a locked car still refuses
// another item, - the other item is not in the locked car, - the sweep closes every open scope (expected 0,
// got 1), - after the sweep the aborted item gets no bypass either. Green with the per-item scope and sweep.
//! inventory/server-insert-scope-aborted (#2835): the server's own insert opened a global bypass
//! (LF_ServerInsertScope) that the locked-car gate and the hideout stash honour. A script error
//! between Begin and End left it on until restart, so a locked car took any player's item. Now the
//! scope is keyed to the one item being inserted and a frame-end sweep closes whatever an aborted
//! insert left open.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_ServerInsertScopeAborted : LF_Test
{
	protected static const ResourceName APPLE = "{C9D661E5B0714711}Prefabs/Items/Food/Apple.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "inventory/server-insert-scope-aborted";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_ServerInsertScope.ResetStatics();
		IEntity car = LF_Utils.SpawnEntityPrefab(LF_VehicleCatalog.S105_LOCKED, "150 0 140");
		ctx.True(car != null, "the lockable car spawns");
		if (!car)
			return;

		LF_VehicleLockComponent vehicleLock = LF_Component<LF_VehicleLockComponent>.Find(car);
		SCR_UniversalInventoryStorageComponent cargo = SCR_UniversalInventoryStorageComponent.Cast(car.FindComponent(SCR_UniversalInventoryStorageComponent));
		InventoryStorageManagerComponent carManager = InventoryStorageManagerComponent.Cast(car.FindComponent(InventoryStorageManagerComponent));
		IEntity aborted = LF_Utils.SpawnEntityPrefab(APPLE, car.GetOrigin() + "0 2 0");
		IEntity player = LF_Utils.SpawnEntityPrefab(APPLE, car.GetOrigin() + "0 2 0");
		IEntity server = LF_Utils.SpawnEntityPrefab(APPLE, car.GetOrigin() + "0 2 0");
		if (!vehicleLock || !cargo || !carManager || !aborted || !player || !server)
		{
			ctx.Fail("fixture: car or apples did not resolve");
			Cleanup(car, aborted, player, server);
			return;
		}

		vehicleLock.SetLocked(true);

		// The server's own insert, Begin and End around it, still passes the lock (#2809).
		EntityID scope = LF_ServerInsertScope.Begin(server);
		ctx.True(carManager.TryInsertItemInStorage(server, cargo), "the server's own insert passes a locked car");
		LF_ServerInsertScope.End(scope);

		// An insert that dies between Begin and End: End never runs.
		LF_ServerInsertScope.Begin(aborted);
		ctx.True(LF_ServerInsertScope.IsSweepQueued(), "opening the scope queues the frame-end sweep");
		ctx.False(carManager.TryInsertItemInStorage(player, cargo), "with an aborted insert left open, a locked car still refuses another item");
		ctx.False(cargo.Contains(player), "the other item is not in the locked car");

		// The frame end closes what the aborted insert left open.
		LF_ServerInsertScope.SweepAborted();
		ctx.Equal(0, LF_ServerInsertScope.OpenCount(), "the sweep closes every open scope");
		ctx.False(carManager.TryInsertItemInStorage(aborted, cargo), "after the sweep the aborted item gets no bypass either");

		Cleanup(car, aborted, player, server);
		LF_ServerInsertScope.ResetStatics();
	}

	//------------------------------------------------------------------------------------------------
	protected void Cleanup(IEntity car, IEntity a, IEntity b, IEntity c)
	{
		LF_TestCharacterFixture.Delete(a);
		LF_TestCharacterFixture.Delete(b);
		LF_TestCharacterFixture.Delete(c);
		LF_TestCharacterFixture.Delete(car);
	}
}
