// red-proof: RedProof() skips the fitting call, so `the owner is fitted`, `the compartment is found` and `the price is taken` fail (run `tools\cli test --no-build --red LF_Test_EveronChopFit`).
//! #3776 in game, under the Everon 1989 header (pack leg): the owner of a car in the chop shop's drop
//! area pays the pack's fitting price and the car gets its hidden compartment. A stranger, a car with
//! no table row, a car that already has one and a short purse are each refused and nothing is taken.
//! The test deletes what it spawns.
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronChopFit : LF_Test
{
	static const ResourceName UAZ = "{C8B16ABCA57679A2}Prefabs/Vehicles/Wheeled/UAZ469/UAZ469_covered_CIV_blue.et";
	static const ResourceName AMBULANCE = "{D8C109C681E7AAF1}Prefabs/Vehicles/Wheeled/M997/LF_M997_Ambulance.et";
	static const string OWNER = "lf-test-chopfit-owner";
	static const string STRANGER = "lf-test-chopfit-stranger";
	static const int UAZ_PRICE = 320;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/chop-fit";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		ctx.Equal(UAZ_PRICE, LF_ChopFitRules.PriceFor(UAZ), "the pack prices the UAZ469 at 320");
		ctx.Equal(-1, LF_ChopFitRules.PriceFor(AMBULANCE), "the pack does not fit an ambulance");

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
		LF_Utils.SetUidForTests(worker, OWNER);
		Drain(worker);
		int paid;

		// A car the pack does not fit: refused, nothing taken.
		IEntity van = LF_Utils.SpawnEntityPrefab(AMBULANCE, yard + "3 0.5 0");
		LF_VehicleLockComponent vanLock = LF_Component<LF_VehicleLockComponent>.Find(van);
		if (van && vanLock)
		{
			vanLock.SetOwnerUid(OWNER);
			LF_MoneyUtils.AddCash(worker, 1000);
			ctx.Equal(LF_EFitCheck.UNSUPPORTED, LF_ChopFit.Serve(shop, worker, paid), "a car with no table row is refused");
			ctx.Equal(1000, LF_MoneyUtils.GetCash(worker), "a refused fitting takes no cash");
			ctx.True(LF_VehicleCompartment.Find(van) == null, "a refused car gets no compartment");
		}
		else
		{
			LF_Debug.Warn("ChopShop", "chop-fit test: the ambulance has no lock component, the unsupported check is skipped");
		}

		if (van)
			SCR_EntityHelper.DeleteEntityAndChildren(van);

		Drain(worker);
		IEntity car = LF_Utils.SpawnEntityPrefab(UAZ, yard + "3 0.5 0");
		LF_VehicleLockComponent carLock = LF_Component<LF_VehicleLockComponent>.Find(car);
		ctx.NotNull(car, "a UAZ469 spawns in the drop area");
		ctx.NotNull(carLock, "the car carries the lock component");
		if (car && carLock)
			Check(ctx, shop, worker, car, carLock);

		if (car)
			SCR_EntityHelper.DeleteEntityAndChildren(car);

		LF_Utils.SetUidForTests(null, "");
		LF_TestCharacterFixture.Delete(worker);
	}

	//------------------------------------------------------------------------------------------------
	protected void Drain(IEntity worker)
	{
		int cash = LF_MoneyUtils.GetCash(worker);
		if (cash > 0)
			LF_MoneyUtils.RemoveCash(worker, cash);
	}

	//------------------------------------------------------------------------------------------------
	protected void Check(LF_TestContext ctx, LF_ChopShopComponent shop, IEntity worker, IEntity car, LF_VehicleLockComponent carLock)
	{
		int paid;

		// A stranger's car.
		carLock.SetOwnerUid(STRANGER);
		LF_MoneyUtils.AddCash(worker, 1000);
		ctx.Equal(LF_EFitCheck.NOT_OWNER, LF_ChopFit.Serve(shop, worker, paid), "a car the worker does not own is refused");
		ctx.Equal(1000, LF_MoneyUtils.GetCash(worker), "a refused fitting takes no cash");
		ctx.True(LF_VehicleCompartment.Find(car) == null, "a stranger's car gets no compartment");

		// The owner's car, with a purse one short of the price.
		carLock.SetOwnerUid(OWNER);
		Drain(worker);
		LF_MoneyUtils.AddCash(worker, UAZ_PRICE - 1);
		ctx.Equal(LF_EFitCheck.SHORT_PURSE, LF_ChopFit.Serve(shop, worker, paid), "a purse one short of the price is refused");
		ctx.Equal(UAZ_PRICE - 1, LF_MoneyUtils.GetCash(worker), "a short purse keeps its cash");
		ctx.True(LF_VehicleCompartment.Find(car) == null, "a car the owner could not pay for gets no compartment");

		// The owner pays and the car is fitted.
		LF_MoneyUtils.AddCash(worker, 1000 - UAZ_PRICE + 1);
		LF_EFitCheck result = LF_EFitCheck.NO_VEHICLE;
		paid = 0;
		if (!RedProof())
			result = LF_ChopFit.Serve(shop, worker, paid);

		ctx.Equal(LF_EFitCheck.NONE, result, "the owner is fitted");
		ctx.Equal(UAZ_PRICE, paid, "the service reports the price it took");
		ctx.Equal(1000 - UAZ_PRICE, LF_MoneyUtils.GetCash(worker), "the price is taken in cash");
		ctx.True(LF_VehicleCompartment.Find(car) != null, "the compartment is found on the car");

		// Already fitted: refused, nothing taken.
		ctx.Equal(LF_EFitCheck.ALREADY_FITTED, LF_ChopFit.Serve(shop, worker, paid), "a car that already has a compartment is refused");
		ctx.Equal(1000 - UAZ_PRICE, LF_MoneyUtils.GetCash(worker), "a second fitting takes no cash");

		// Too far from the bench.
		worker.SetOrigin(shop.GetOwner().GetOrigin() + "0 0.1 30");
		ctx.Equal(LF_EFitCheck.TOO_FAR, LF_ChopFit.Serve(shop, worker, paid), "a worker away from the bench is refused");
	}
}
