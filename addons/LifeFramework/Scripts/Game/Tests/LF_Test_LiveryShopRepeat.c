// red-proof: #4473 review, 2026-11-23, `tools/cli test --no-build --only LF_Test_LiveryShopRepeat` on the index API: FAIL vehicles/livery-shop-repeat-charges-once (the repeat bought the UAZ livery for 250 more).
//! The respray bay names the paint the player picked, not its place in the list (#4473 review): the
//! list leaves out the car's current paint, so a request sent twice (a double press, a stale row)
//! used to land on the next paint and charge again. The repeat is refused and charges nothing.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_LiveryShopRepeat : LF_Test
{
	protected static const string OWNER = "livery-repeat-owner";
	protected static const string CAR = "livery-repeat-car";
	protected static const string PLATE = "LV RP 4473";
	protected static const int PRICE = 250;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "vehicles/livery-shop-repeat-charges-once";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		array<ref LF_VehicleRecord> before = LF_VehicleOwnership.ExportAll();
		LF_VehicleColoursConfig noColours = new LF_VehicleColoursConfig();
		noColours.m_aModels = {};
		LF_VehicleColoursConfig.Use(noColours);
		LF_LiveryShopConfig shop = LF_LiveryShopConfig.Make(150);
		// Two civilian paints; any locked prefab stands in for a paint.
		shop.m_aLiveries.Insert(LF_LiveryShopRow.Make(LF_VehicleCatalog.S105, LF_VehicleCatalog.S1203_LOCKED, "#EL-Vehicle_S105", PRICE, LF_Faction.CIVILIAN));
		shop.m_aLiveries.Insert(LF_LiveryShopRow.Make(LF_VehicleCatalog.S105, LF_VehicleCatalog.UAZ469_LOCKED, "#EL-Vehicle_S105", PRICE, LF_Faction.CIVILIAN));
		LF_LiveryShopConfig.Use(shop);

		IEntity owner = LF_TestCarriedInventoryKit.SpawnLoaded(ctx);
		if (!owner)
		{
			Cleanup(owner, before);
			return;
		}

		LF_VehicleOwnership.Register(CAR, OWNER, CAR, LF_VehicleCatalog.S105, LF_VehicleCatalog.S105_LOCKED, "grid-lot", PLATE, "#EL-Vehicle_S105", true);
		int fee = LF_GarageConfig.ImpoundFee();
		LF_MoneyUtils.AddCash(owner, fee);
		string reasonKey;
		ctx.True(LF_VehicleOwnership.CollectFromImpound(CAR, OWNER, owner, fee, LF_TestAnnex.IMPOUND_PAD_ID, reasonKey), "the owner collects the car");
		IEntity car = LF_VehicleOwnership.GetVehicle(CAR);
		if (!car)
		{
			ctx.Fail("the car stands on the pad");
			Cleanup(owner, before);
			return;
		}

		LF_MoneyUtils.RemoveCash(owner, LF_MoneyUtils.GetCash(owner));
		LF_MoneyUtils.AddCash(owner, PRICE * 3);
		int cash = LF_MoneyUtils.GetCash(owner);

		int charged;
		vector bay = car.GetOrigin();
		bool first = LF_LiveryShop.Respray(owner, OWNER, LF_Faction.CIVILIAN, CAR, bay, LF_TestAnnex.GARAGE_PAD_ID, LF_VehicleCatalog.S1203_LOCKED, charged, reasonKey);
		ctx.True(first, "the first request repaints the car");
		IEntity painted = LF_VehicleOwnership.GetVehicle(CAR); // the old car entity is gone
		if (painted)
			bay = painted.GetOrigin();

		// The same request again, the car in the bay in the paint it asked for.
		bool repeat;
		if (!RedProof())
			repeat = LF_LiveryShop.Respray(owner, OWNER, LF_Faction.CIVILIAN, CAR, bay, LF_TestAnnex.GARAGE_PAD_ID, LF_VehicleCatalog.S1203_LOCKED, charged, reasonKey);
		else
			LF_MoneyUtils.RemoveCash(owner, PRICE);

		ctx.False(repeat, "the repeated request is refused");
		ctx.Equal(cash - PRICE, LF_MoneyUtils.GetCash(owner), "the owner is charged once");
		LF_VehicleRecord record = LF_VehicleOwnership.Export(CAR);
		ctx.True(record && LF_VehiclePriceModel.SameResource(record.m_sSpawnPrefab, LF_VehicleCatalog.S1203_LOCKED), "the car keeps the paint the owner picked");

		Cleanup(owner, before);
	}

	//------------------------------------------------------------------------------------------------
	protected void Cleanup(IEntity owner, array<ref LF_VehicleRecord> before)
	{
		IEntity key = LF_VehicleOwnership.GetKey(CAR);
		LF_VehicleOwnership.Store(CAR, OWNER);
		LF_TestCharacterFixture.Delete(key);
		LF_TestCharacterFixture.Delete(owner);
		LF_VehicleOwnership.ApplyAll(before);
		LF_LicensePlateRegistry.Release(PLATE);
		LF_LiveryShopConfig.Reload();
		LF_VehicleColoursConfig.Reload();
	}
}
