// red-proof: #4473, 2026-11-23, `tools/cli test --no-build --red LF_Test_LiveryShop` (the paid Respray skipped):
// RED-OK vehicles/livery-shop-respray (fails with the hook on, 46 assertions); - the live car is the chosen variant.
//! The respray bay (WO-L9, #4473): the owner with the cash is charged once and the car comes back
//! as the chosen variant with its owner, key, plate, fuel and garage record; a stranger, a car
//! outside the bay, too little cash and a police livery for a civilian are refused in words and
//! charge nothing.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_LiveryShop : LF_Test
{
	protected static const string OWNER = "livery-shop-owner";
	protected static const string STRANGER = "livery-shop-stranger";
	protected static const string CAR = "livery-shop-car";
	protected static const string PLATE = "LV RY 4473";
	protected static const int CIVIL_PRICE = 250;
	protected static const int POLICE_PRICE = 300;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "vehicles/livery-shop-respray";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		array<ref LF_VehicleRecord> before = LF_VehicleOwnership.ExportAll();
		LF_VehicleColoursConfig noColours = new LF_VehicleColoursConfig();
		noColours.m_aModels = {};
		LF_VehicleColoursConfig.Use(noColours);
		LF_LiveryShopConfig.Reload();
		LF_LiveryShopConfig loaded = LF_LiveryShopConfig.Get();
		ctx.True(loaded && loaded.m_iResprayPrice > 0 && loaded.m_aLiveries, "the shop config loads from the addons' Configs/Vehicles files");
		LF_LiveryShopConfig shop = LF_LiveryShopConfig.Make(150);
		// Option 0: a civilian paint; option 1: a police livery. Any locked prefab stands in for a paint.
		shop.m_aLiveries.Insert(LF_LiveryShopRow.Make(LF_VehicleCatalog.S105, LF_VehicleCatalog.S1203_LOCKED, "#EL-Vehicle_S105", CIVIL_PRICE, LF_Faction.CIVILIAN));
		shop.m_aLiveries.Insert(LF_LiveryShopRow.Make(LF_VehicleCatalog.S105, LF_VehicleCatalog.UAZ469_LOCKED, "#EL-Vehicle_S105", POLICE_PRICE, LF_Faction.POLICE));
		LF_LiveryShopConfig.Use(shop);

		IEntity owner = LF_TestCarriedInventoryKit.SpawnLoaded(ctx);
		IEntity stranger = LF_TestCarriedInventoryKit.SpawnLoaded(ctx);
		if (!owner || !stranger)
		{
			Cleanup(owner, stranger, before);
			return;
		}

		LF_VehicleOwnership.Register(CAR, OWNER, CAR, LF_VehicleCatalog.S105, LF_VehicleCatalog.S105_LOCKED, "grid-lot", PLATE, "#EL-Vehicle_S105", true);
		int fee = LF_GarageConfig.ImpoundFee();
		LF_MoneyUtils.AddCash(owner, fee);
		string reasonKey;
		ctx.True(LF_VehicleOwnership.CollectFromImpound(CAR, OWNER, owner, fee, LF_TestAnnex.IMPOUND_PAD_ID, reasonKey), "the owner collects the car");
		IEntity car = LF_VehicleOwnership.GetVehicle(CAR);
		ctx.NotNull(car, "the car stands on the pad");
		if (!car)
		{
			Cleanup(owner, stranger, before);
			return;
		}

		vector bay = car.GetOrigin();
		SetFuelShare(car, 0.3);
		array<ref LF_LiveryOption> options = {};
		LF_LiveryShop.OptionsFor(CAR, options);
		ctx.Equal(2, options.Count(), "the bay offers the two liveries for the model");

		int charged;
		// Too little money: the owner carries less than the price.
		LF_MoneyUtils.RemoveCash(owner, LF_MoneyUtils.GetCash(owner));
		LF_MoneyUtils.AddCash(owner, CIVIL_PRICE - 1);
		ctx.False(LF_LiveryShop.Respray(owner, OWNER, LF_Faction.CIVILIAN, CAR, bay, LF_TestAnnex.GARAGE_PAD_ID, LF_VehicleCatalog.S1203_LOCKED, charged, reasonKey), "too little cash is refused");
		ctx.EqualStr("#LF-LiveryShop_NoCash", reasonKey, "and told so");
		ctx.Equal(CIVIL_PRICE - 1, LF_MoneyUtils.GetCash(owner), "and charged nothing");

		LF_MoneyUtils.AddCash(owner, POLICE_PRICE + 1);
		int cash = LF_MoneyUtils.GetCash(owner);

		// A stranger cannot repaint the owner's car.
		LF_MoneyUtils.AddCash(stranger, 1000);
		int strangerCash = LF_MoneyUtils.GetCash(stranger);
		ctx.False(LF_LiveryShop.Respray(stranger, STRANGER, LF_Faction.CIVILIAN, CAR, bay, LF_TestAnnex.GARAGE_PAD_ID, LF_VehicleCatalog.S1203_LOCKED, charged, reasonKey), "a stranger is refused");
		ctx.EqualStr("#EL-Vehicle_NotOwner", reasonKey, "the stranger is told it is not theirs");
		ctx.Equal(strangerCash, LF_MoneyUtils.GetCash(stranger), "the stranger is charged nothing");

		// The car is not in the bay.
		ctx.False(LF_LiveryShop.Respray(owner, OWNER, LF_Faction.CIVILIAN, CAR, bay + "60 0 0", LF_TestAnnex.GARAGE_PAD_ID, LF_VehicleCatalog.S1203_LOCKED, charged, reasonKey), "a car outside the bay is refused");
		ctx.EqualStr("#LF-LiveryShop_NotInBay", reasonKey, "the owner is told to bring it into the bay");

		// A police livery for a civilian.
		ctx.False(LF_LiveryShop.Respray(owner, OWNER, LF_Faction.CIVILIAN, CAR, bay, LF_TestAnnex.GARAGE_PAD_ID, LF_VehicleCatalog.UAZ469_LOCKED, charged, reasonKey), "a police livery is refused to a civilian");
		ctx.EqualStr("#LF-LiveryShop_WrongJob", reasonKey, "the civilian is told it is a service livery");
		ctx.Equal(cash, LF_MoneyUtils.GetCash(owner), "the refusals charged the owner nothing");
		ctx.True(car == LF_VehicleOwnership.GetVehicle(CAR), "the refusals left the car alone");

		// The paid respray.
		LF_VehicleLockComponent oldLock = LF_Component<LF_VehicleLockComponent>.Find(car);
		string keyId;
		if (oldLock)
			keyId = oldLock.GetKeyId();

		bool done;
		if (!RedProof())
			done = LF_LiveryShop.Respray(owner, OWNER, LF_Faction.CIVILIAN, CAR, bay, LF_TestAnnex.GARAGE_PAD_ID, LF_VehicleCatalog.S1203_LOCKED, charged, reasonKey);

		ctx.True(done, "the owner with the cash gets the respray");
		ctx.EqualStr("#LF-LiveryShop_Done", reasonKey, "and is told it is done");
		ctx.Equal(CIVIL_PRICE, charged, "the price is charged");
		ctx.Equal(cash - CIVIL_PRICE, LF_MoneyUtils.GetCash(owner), "once");

		LF_VehicleRecord record = LF_VehicleOwnership.Export(CAR);
		ctx.True(record && LF_VehiclePriceModel.SameResource(record.m_sSpawnPrefab, LF_VehicleCatalog.S1203_LOCKED), "the record spawns the new paint");
		ctx.True(record && record.m_sOwnerUid == OWNER && record.m_sPlate == PLATE && !record.m_bStored, "owner, plate and the street state are kept");
		ctx.True(record && LF_VehiclePriceModel.SameResource(record.m_sPrefab, LF_VehicleCatalog.S105), "the garage record is still the S105");

		IEntity painted = LF_VehicleOwnership.GetVehicle(CAR);
		ctx.True(painted && painted != car, "a new car stands in the new paint");
		ctx.True(painted && LF_VehiclePriceModel.SameResource(painted.GetPrefabData().GetPrefabName(), LF_VehicleCatalog.S1203_LOCKED), "the live car is the chosen variant");
		LF_VehicleLockComponent newLock;
		if (painted)
			newLock = LF_Component<LF_VehicleLockComponent>.Find(painted);

		ctx.True(newLock && newLock.GetVehicleIdentifier() == CAR && newLock.GetKeyId() == keyId, "the lock keeps the car's id and key");
		ctx.True(Math.AbsFloat(FuelShare(painted) - 0.3) < 0.02, "the fuel is kept");

		Cleanup(owner, stranger, before);
	}

	//------------------------------------------------------------------------------------------------
	protected void SetFuelShare(IEntity car, float share)
	{
		SCR_FuelManagerComponent manager = SCR_FuelManagerComponent.Cast(car.FindComponent(SCR_FuelManagerComponent));
		if (!manager)
			return;

		array<BaseFuelNode> nodes = {};
		manager.GetFuelNodesList(nodes);
		foreach (BaseFuelNode node : nodes)
		{
			node.SetFuel(node.GetMaxFuel() * share);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected float FuelShare(IEntity car)
	{
		if (!car)
			return -1;

		SCR_FuelManagerComponent manager = SCR_FuelManagerComponent.Cast(car.FindComponent(SCR_FuelManagerComponent));
		if (!manager || manager.GetTotalMaxFuel() <= 0)
			return -1;

		return manager.GetTotalFuel() / manager.GetTotalMaxFuel();
	}

	//------------------------------------------------------------------------------------------------
	protected void Cleanup(IEntity owner, IEntity stranger, array<ref LF_VehicleRecord> before)
	{
		IEntity key = LF_VehicleOwnership.GetKey(CAR);
		LF_VehicleOwnership.Store(CAR, OWNER);
		LF_TestCharacterFixture.Delete(key);
		LF_TestCharacterFixture.Delete(owner);
		LF_TestCharacterFixture.Delete(stranger);
		LF_VehicleOwnership.ApplyAll(before);
		LF_LicensePlateRegistry.Release(PLATE);
		LF_LiveryShopConfig.Reload();
		LF_VehicleColoursConfig.Reload();
	}
}
