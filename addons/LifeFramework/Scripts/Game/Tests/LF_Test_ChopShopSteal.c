// red-proof: 2026-11-10 `tools/cli test --no-build --only LF_Test_ChopShopSteal` with the payout expectation set to 301 failed `- the worker is paid 30 percent of the S105's 1000 (expected 301, got 300)`. Restored; green.
// tier: WORLD
//! The chop shop loop at the real bench (#601): a car nobody stole is refused with the line that says
//! what counts as hot, and is left untouched (not locked, not started on a job, still there). The same
//! car marked stolen (what a hotwire does) loads, takes its work, pays the worker and is deleted.
class LF_Test_ChopShopSteal : LF_Test
{
	override string GetName()
	{
		return "chopshop/steal-and-chop";
	}

	override void Run(LF_TestContext ctx)
	{
		IEntity bench = GetGame().GetWorld().FindEntityByName(LF_TestAnnex.CHOP_SHOP);
		LF_ChopShopComponent shop = LF_Component<LF_ChopShopComponent>.Find(bench);
		ctx.NotNull(shop, "the playtest bench is placed");
		if (!shop)
			return;

		IEntity worker = LF_TestCharacterFixture.Spawn(ctx);
		if (!worker)
			return;

		vector yard = bench.GetOrigin();
		worker.SetOrigin(yard + "0 0.1 1.5");
		IEntity car = LF_Utils.SpawnEntityPrefab(LF_VehicleCatalog.S105_LOCKED, yard + "3 0.5 0");
		ctx.NotNull(car, "a locked-ignition car spawns in the drop area");
		LF_VehicleLockComponent carLock = LF_Component<LF_VehicleLockComponent>.Find(car);
		ctx.NotNull(carLock, "the car carries the lock component");
		if (!car || !carLock)
		{
			LF_TestCharacterFixture.Delete(worker);
			return;
		}

		EntityID carId = car.GetID();
		float now = 100000;
		bool wasLocked = carLock.IsVehicleLocked();

		// A car nobody stole: refused with the explaining line, and left as it was.
		ctx.False(carLock.IsStolen(), "a car nobody took without its key is not hot");
		ctx.Equal(LF_EChopCheck.NOT_STOLEN, shop.ServerLoad(worker, now), "a car that is not stolen is refused");
		ctx.EqualStr("#LF-Chop_NotStolen", LF_ChopShopRules.RefusalKey(LF_EChopCheck.NOT_STOLEN), "the refusal uses the explaining line");
		ctx.Equal(LF_StationProgress.EMPTY, shop.GetPercent(), "a refused car starts no job");
		ctx.Equal(wasLocked, carLock.IsVehicleLocked(), "a refused car is not locked by the yard");
		ctx.True(GetGame().GetWorld().FindEntityByID(carId) != null, "a refused car is still there");

		// The same car after a hotwire: loaded, worked, paid, gone.
		carLock.SetStolen(true);
		ctx.Equal(LF_EChopCheck.NONE, shop.ServerLoad(worker, now), "a stolen car is taken");
		ctx.True(carLock.IsVehicleLocked(), "a car on the bench is locked");
		int cashBefore = LF_MoneyUtils.GetCash(worker);
		for (int i = 0; i < LF_ChopShopConfig.WorkUnits(); i++)
		{
			now += 10;
			ctx.Equal(LF_EChopCheck.NONE, shop.ServerWork(worker, now), "a unit of work is taken");
		}

		now += 10;
		ctx.Equal(LF_EChopCheck.NONE, shop.ServerCollect(worker, now), "a finished job pays");
		ctx.Equal(cashBefore + 300, LF_MoneyUtils.GetCash(worker), "the worker is paid 30 percent of the S105's 1000");
		ctx.True(GetGame().GetWorld().FindEntityByID(carId) == null, "the chopped car is deleted");
		ctx.Equal(LF_StationProgress.EMPTY, shop.GetPercent(), "the bench is empty again");

		LF_TestCharacterFixture.Delete(worker);
	}
}
