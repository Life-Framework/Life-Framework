// red-proof: RED-OK everon/vehicle-colours, observed 2026-11-23 with `tools/cli test --no-build --red LF_Test_EveronVehicleColours` (the hook swaps the pack-loaded colours for an empty config): no model has a colour row, the S105 shows no colour options and `the live car is the dark red prefab` fails. Hook off: green (pack leg).
//! #4618 (epic #1826), pack leg: under the Everon 1989 header the colours config lists the vanilla paint
//! variants of every civilian car the lots sell, each one loads and the locked children carry the lock.
//! A civilian S105 at the respray bay sees the colour rows at the respray price; paying for one keeps
//! the car's owner, plate and cargo and puts the new paint on the road.
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronVehicleColours : LF_Test
{
	protected static const string OWNER = "everon-colours-owner";
	protected static const string CAR = "everon-colours-car";
	protected static const string PLATE = "EV CL 4618";
	protected static const int COLOUR_ROWS_S105 = 8;
	protected static const ResourceName S105_DARKRED = "{47B64E73871753A7}Prefabs/Vehicles/Colours/LF_S105_DarkRed.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/vehicle-colours";
	}

	//------------------------------------------------------------------------------------------------
	//! One model's row: the colours it must sell, base colour included, and its locked base colour.
	protected void CheckModel(LF_TestContext ctx, LF_VehicleColoursConfig colours, string label, ResourceName catalog, ResourceName locked, int count)
	{
		LF_VehicleColourSet row = colours.SetFor(catalog);
		ctx.NotNull(row, label + " has a colour row");
		if (!row)
			return;

		ctx.Equal(count, row.m_aColours.Count(), label + " sells its vanilla colours");
		ctx.True(LF_VehiclePriceModel.SameResource(row.m_aColours[0].m_SpawnPrefab, locked), label + ": colour 0 is the locked car sold today");
		foreach (int i, LF_VehicleColour colour : row.m_aColours)
		{
			ctx.True(colour.m_sNameKey.IndexOf("Colour_") == 4, string.Format("%1 colour %2 names itself", label, i));
			ctx.True(colour.m_iSwatch > 0, string.Format("%1 colour %2 has a swatch", label, i));
			Resource resource = Resource.Load(colour.m_SpawnPrefab);
			ctx.True(resource && resource.IsValid(), string.Format("%1 colour %2 loads: %3", label, i, colour.m_SpawnPrefab));
			if (i > 0)
				ctx.False(LF_VehiclePriceModel.SameResource(colour.m_SpawnPrefab, locked), string.Format("%1 colour %2 is its own prefab", label, i));
		}

		// The last colour is spawned: a thin child of the locked car still carries the lock.
		LF_VehicleColour last = row.m_aColours[row.m_aColours.Count() - 1];
		IEntity spot = LF_TestAnnex.Find(LF_TestAnnex.GARAGE);
		ctx.NotNull(spot, "the annex garage is placed");
		if (!spot)
			return;

		IEntity entity = LF_Utils.SpawnEntityPrefab(last.m_SpawnPrefab, spot.GetOrigin() + "0 6 0");
		ctx.NotNull(entity, label + ": the last colour spawns");
		if (entity)
		{
			ctx.NotNull(LF_Component<LF_VehicleLockComponent>.Find(entity), label + ": the colour car carries the lock");
			LF_TestCharacterFixture.Delete(entity);
		}
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		array<ref LF_VehicleRecord> before = LF_VehicleOwnership.ExportAll();
		array<ref LF_StorageSnapshotRecord> snapshotsBefore = LF_StorageSnapshot.ExportAll();
		LF_VehicleColoursConfig.Reload();
		LF_LiveryShopConfig.Reload();
		LF_VehicleColoursConfig colours = LF_VehicleColoursConfig.Get();
		ctx.NotNull(colours, "the colours config loads under the pack header");
		if (!colours)
			return;

		// The act the assertions guard: the pack's overlay is what the lots and the bay read.
		if (RedProof())
		{
			colours = new LF_VehicleColoursConfig();
			colours.m_aModels = {};
			LF_VehicleColoursConfig.Use(colours);
		}

		CheckModel(ctx, colours, "S105", LF_VehicleCatalog.S105, LF_VehicleCatalog.S105_LOCKED, COLOUR_ROWS_S105 + 1);
		CheckModel(ctx, colours, "Dana", LF_VehicleCatalog.DANA2101, LF_VehicleCatalog.DANA2101_LOCKED, 9);
		CheckModel(ctx, colours, "Sputnik", LF_VehicleCatalog.SPUTNIK_T601, LF_VehicleCatalog.SPUTNIK_T601_LOCKED, 5);
		CheckModel(ctx, colours, "S1203", LF_VehicleCatalog.S1203, LF_VehicleCatalog.S1203_LOCKED, 5);
		CheckModel(ctx, colours, "UAZ469", LF_VehicleCatalog.UAZ469, LF_VehicleCatalog.UAZ469_LOCKED, 4);
		ctx.Equal(1, colours.CountFor(LF_VehicleCatalog.UAZ452), "a model with no vanilla colour to offer sells in one colour");

		RespraySection(ctx, before, snapshotsBefore);
		LF_VehicleColoursConfig.Reload();
		LF_LiveryShopConfig.Reload();
	}

	//------------------------------------------------------------------------------------------------
	//! A civilian S105 at the bay: the colour rows, then a paid respray that keeps owner, plate and cargo.
	protected void RespraySection(LF_TestContext ctx, array<ref LF_VehicleRecord> before, array<ref LF_StorageSnapshotRecord> snapshotsBefore)
	{
		IEntity owner = LF_TestCarriedInventoryKit.SpawnLoaded(ctx);
		if (!owner)
			return;

		LF_VehicleOwnership.Register(CAR, OWNER, CAR, LF_VehicleCatalog.S105, LF_VehicleCatalog.S105_LOCKED, "grid-lot", PLATE, "#EL-Vehicle_S105", true);
		int fee = LF_GarageConfig.ImpoundFee();
		LF_MoneyUtils.AddCash(owner, fee);
		string reasonKey;
		ctx.True(LF_VehicleOwnership.CollectFromImpound(CAR, OWNER, owner, fee, LF_TestAnnex.IMPOUND_PAD_ID, reasonKey), "the owner collects the S105");
		IEntity car = LF_VehicleOwnership.GetVehicle(CAR);
		ctx.NotNull(car, "the S105 stands on the pad");
		if (!car || !LF_Test_VehicleStoreCargo.LoadApple(ctx, CAR))
		{
			Cleanup(owner, before, snapshotsBefore);
			return;
		}

		array<ref LF_LiveryOption> options = {};
		LF_LiveryShop.OptionsFor(CAR, options);
		int colourRows;
		bool darkRed;
		foreach (LF_LiveryOption option : options)
		{
			if (option.m_bLivery)
				continue;

			colourRows++;
			ctx.Equal(LF_LiveryShopConfig.Get().m_iResprayPrice, option.m_iPrice, "a colour row costs the respray price");
			ctx.True(option.m_eFaction == LF_Faction.CIVILIAN, "a colour row is open to a civilian");
			if (LF_VehiclePriceModel.SameResource(option.m_SpawnPrefab, S105_DARKRED))
				darkRed = true;
		}

		ctx.Equal(COLOUR_ROWS_S105, colourRows, "a civilian S105 sees every colour but its own");
		ctx.True(darkRed, "dark red is one of them");

		int price = LF_LiveryShopConfig.Get().m_iResprayPrice;
		LF_MoneyUtils.RemoveCash(owner, LF_MoneyUtils.GetCash(owner));
		LF_MoneyUtils.AddCash(owner, price + 20);
		vector bay = car.GetOrigin();
		int charged;
		bool done = LF_LiveryShop.Respray(owner, OWNER, LF_Faction.CIVILIAN, CAR, bay, LF_TestAnnex.GARAGE_PAD_ID, S105_DARKRED, charged, reasonKey);
		ctx.True(done, "the civilian pays for dark red");
		ctx.Equal(price, charged, "the respray price is charged");
		ctx.Equal(20, LF_MoneyUtils.GetCash(owner), "once");

		LF_VehicleRecord record = LF_VehicleOwnership.Export(CAR);
		ctx.True(record && LF_VehiclePriceModel.SameResource(record.m_sSpawnPrefab, S105_DARKRED), "the record spawns dark red");
		ctx.True(record && record.m_sOwnerUid == OWNER && record.m_sPlate == PLATE, "owner and plate are kept");
		ctx.True(record && LF_VehiclePriceModel.SameResource(record.m_sPrefab, LF_VehicleCatalog.S105), "the garage record is still the S105");
		IEntity painted = LF_VehicleOwnership.GetVehicle(CAR);
		ctx.True(painted && LF_VehiclePriceModel.SameResource(painted.GetPrefabData().GetPrefabName(), S105_DARKRED), "the live car is the dark red prefab");
		LF_VehicleOwnership.ApplyCargo(CAR);
		ctx.Equal(1, LF_Test_VehicleStoreCargo.CountInCar(CAR, LF_Test_VehicleStoreCargo.APPLE_PREFAB), "the cargo is kept");

		// Now dark red is its own paint: the list offers the other eight, beige back among them.
		array<ref LF_LiveryOption> after = {};
		LF_LiveryShop.OptionsFor(CAR, after);
		bool beige;
		foreach (LF_LiveryOption left : after)
		{
			if (LF_VehiclePriceModel.SameResource(left.m_SpawnPrefab, LF_VehicleCatalog.S105_LOCKED))
				beige = true;
		}

		ctx.True(beige, "beige is on offer again once the car is dark red");
		Cleanup(owner, before, snapshotsBefore);
	}

	//------------------------------------------------------------------------------------------------
	protected void Cleanup(IEntity owner, array<ref LF_VehicleRecord> before, array<ref LF_StorageSnapshotRecord> snapshotsBefore)
	{
		if (LF_VehicleOwnership.GetVehicle(CAR))
			LF_VehicleOwnership.Store(CAR, OWNER);

		LF_TestCharacterFixture.Delete(LF_VehicleOwnership.GetKey(CAR));
		LF_TestCharacterFixture.Delete(LF_VehicleOwnership.GetVehicle(CAR));
		LF_TestCharacterFixture.Delete(owner);
		LF_VehicleOwnership.ApplyAll(before);
		LF_StorageSnapshot.ApplyAll(snapshotsBefore);
		LF_LicensePlateRegistry.Release(PLATE);
	}
}
