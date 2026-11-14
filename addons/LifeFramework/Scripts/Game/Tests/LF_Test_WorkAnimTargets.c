// red-proof: 2026-11-10 `tools/cli test --no-build --only LF_Test_WorkAnimTargets` with the van-box start expectation changed to starts + 2 failed `- drilling the van box starts the work animation (expected 3, got 2)`. Restored; green.
// tier: WORLD
//! work/anim-targets (#513 follow-up): the working pose also starts for a locked vehicle's lockpick
//! attempt and for the drill on the cash van box, not only the door and the bank vault. Drives the
//! real Begin/Rob paths with a fixture character; broadcasts are silenced, the test counts the
//! starts the server decided.
class LF_Test_WorkAnimTargets : LF_Test
{
	override string GetName()
	{
		return "work/anim-targets";
	}

	override void Run(LF_TestContext ctx)
	{
		IEntity car = GetGame().GetWorld().FindEntityByName(LF_TestAnnex.LOCKPICK_CAR);
		ctx.True(car != null, "Annex_Lockpick_Car is placed");
		if (!car)
			return;

		LF_VehicleLockComponent carLock = LF_Component<LF_VehicleLockComponent>.Find(car);
		ctx.NotNull(carLock, "the car carries LF_VehicleLockComponent");
		if (!carLock)
			return;

		LF_WorkAnim.ResetStatics();
		LF_WorkAnim.SetSilent(true);
		IEntity worker = LF_TestCharacterFixture.Spawn(ctx);
		if (!worker)
		{
			LF_WorkAnim.SetSilent(false);
			return;
		}

		worker.SetOrigin(car.GetOrigin() + "2 0 0");
		InventoryStorageManagerComponent inventory = LF_InventoryUtils.GetResponsibleStorageManager(worker);
		IEntity pack = LF_Utils.SpawnEntityPrefab("{06B68C58B72EAAC6}Prefabs/Items/Equipment/Backpacks/Backpack_ALICE_Medium.et", worker.GetOrigin());
		ctx.True(pack != null && inventory.TryInsertItem(pack, EStoragePurpose.PURPOSE_LOADOUT_PROXY), "the worker wears a pack");
		ctx.Equal(1, LF_InventoryUtils.AddAmount(worker, LF_Lockpick.PICK_PREFAB, 1), "the worker carries a lockpick");

		bool wasLocked = carLock.IsVehicleLocked();
		carLock.SetLocked(true);
		LF_Lockpick.s_fSecondsOverride = 600;
		LF_Lockpick.s_fRollOverride = 0;
		ctx.True(LF_Lockpick.BeginAs(car, worker, "test-anim-car"), "the vehicle pick attempt starts");
		ctx.Equal(1, LF_WorkAnim.GetStartCount(), "picking the car starts the work animation");
		ctx.Equal(1, LF_WorkAnim.GetActiveCount(), "one animation runs for the picker");
		LF_Lockpick.CompleteNow(car);
		ctx.Equal(0, LF_WorkAnim.GetActiveCount(), "finishing the pick stops the animation");
		LF_Lockpick.s_fSecondsOverride = -1;
		LF_Lockpick.s_fRollOverride = -1;
		carLock.SetLocked(wasLocked);

		// The cash van box: a drill opens it and the robber works the lock.
		string vanId = "work-anim-van-box";
		LF_Bank.Configure(vanId, 3, 400, 0, 0, 0);
		LF_Bank.ConfigureTools(vanId, "drill,charge", 0);
		LF_Bank.ConfigureHeist(vanId, LF_BankLoot.KIND_BAGS, "", LF_WantedReports.OFFENCE_CASH_VAN, false, -1, 0);
		ctx.Equal(1, LF_InventoryUtils.AddAmount(worker, LF_Bank.DRILL_PREFAB, 1), "the worker carries a drill");
		int starts = LF_WorkAnim.GetStartCount();
		ctx.True(LF_Bank.Rob(vanId, "test-anim-van", worker, 0), "the drill opens the van box");
		ctx.Equal(starts + 1, LF_WorkAnim.GetStartCount(), "drilling the van box starts the work animation");
		LF_WorkAnim.End(worker, "test done");

		LF_Bank.Restock(vanId);
		LF_TestCharacterFixture.Delete(worker);
		LF_WorkAnim.ResetStatics();
		LF_WorkAnim.SetSilent(false);
	}
}
