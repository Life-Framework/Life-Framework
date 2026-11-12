// red-proof: drop `vehicles.WriteStream(ctx);` from LF_ConfigStream.BuildPayload and
// config/stream-vehicles fails "the owner's buy percent reaches the client (expected 120, got 100)"
// and "the client has every server vehicle (expected 3, got 2)". Observed 2026-11-12.

//------------------------------------------------------------------------------------------------
//! The vehicles section of the config stream (#285): the server's effective vehicle prices
//! (owner layer included), serialised, received and applied on a client that has only the shipped
//! values, give the client the same percents and the same vehicles in the same order, so a price
//! board reads the owner's price. A bad value in the section changes nothing on the client.
// tier: LOGIC
class LF_Test_ConfigStreamVehicles : LF_Test
{
	override string GetName()
	{
		return "config/stream-vehicles";
	}

	//------------------------------------------------------------------------------------------------
	//! The shipped values both sides start from: the S105 at 1000 (with its locked child) and the UAZ at 2500.
	protected LF_VehiclePricesConfig ShippedCore()
	{
		LF_VehiclePricesConfig config = new LF_VehiclePricesConfig();
		config.m_iBuyPercent = 100;
		config.m_iSellBackPercent = 50;
		config.m_iChopPercent = 30;
		config.m_iChopValueCap = 3000;
		config.m_iImpoundPercent = 10;
		config.m_iGaragePercent = 0;
		config.m_aVehicles = {};
		config.m_aVehicles.Insert(LF_VehiclePriceEntry.Make(LF_VehicleCatalog.S105, LF_VehicleCatalog.S105_LOCKED, 1000));
		LF_VehiclePriceEntry uaz = LF_VehiclePriceEntry.Make(LF_VehicleCatalog.UAZ469, LF_VehicleCatalog.UAZ469_LOCKED, 2500);
		uaz.m_iImpoundOverride = 250;
		config.m_aVehicles.Insert(uaz);
		return config;
	}

	//------------------------------------------------------------------------------------------------
	protected LF_VehiclePricesConfig MakeServer(string ownerJson)
	{
		LF_ConfigService.Reset();
		LF_ConfigStream.Reset();
		LF_ConfigService.LoadOwnerString(ownerJson, "test-server");
		LF_VehiclePricesConfig config = ShippedCore();
		config.ApplyLayers();
		return config;
	}

	//------------------------------------------------------------------------------------------------
	//! A client: no owner layer, the stream received, the shipped core applied.
	protected LF_VehiclePricesConfig MakeClient(LF_TestContext ctx, string payload, string label)
	{
		LF_ConfigService.Reset();
		LF_ConfigStream.Reset();
		LF_ConfigService.LoadOwnerString("{\"schemaVersion\":1}", "test-client");
		ctx.Equal(LF_EConfigStreamStatus.APPLIED, LF_ConfigStream.Receive(LF_ConfigStream.FORMAT_VERSION, payload.Hash(), payload), label + ": the client accepts the stream");
		LF_VehiclePricesConfig config = ShippedCore();
		config.ApplyLayers();
		return config;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		string s105 = LF_VehiclePriceModel.GuidOf(LF_VehicleCatalog.S105);
		string newGuid = "A1B2C3D4E5F60718";
		string owner = "{\"schemaVersion\":1,\"vehicles\":{\"buyPercent\":120,\"chopValueCap\":0,\"vehicles\":{"
			+ "\"" + s105 + "\":{\"basePrice\":1500,\"garageOverride\":5},"
			+ "\"" + newGuid + "\":{\"basePrice\":777,\"spawnPrefab\":\"{A1B2C3D4E5F60719}Prefabs/Vehicles/LF_New.et\"}"
			+ "}}}";
		LF_VehiclePricesConfig server = MakeServer(owner);
		ctx.Equal(3, server.m_aVehicles.Count(), "the server has the owner's added vehicle");
		string payload = LF_ConfigStream.BuildPayload(null, true, server);
		ctx.True(payload.Length() > 0, "the server writes a payload");

		LF_VehiclePricesConfig client = MakeClient(ctx, payload, "owner prices");
		ctx.Equal(120, client.m_iBuyPercent, "the owner's buy percent reaches the client");
		ctx.Equal(0, client.m_iChopValueCap, "the owner's chop cap reaches the client");
		ctx.Equal(server.m_aVehicles.Count(), client.m_aVehicles.Count(), "the client has every server vehicle");
		if (server.m_aVehicles.Count() == client.m_aVehicles.Count())
		{
			foreach (int i, LF_VehiclePriceEntry want : server.m_aVehicles)
			{
				LF_VehiclePriceEntry got = client.m_aVehicles[i];
				string at = string.Format("vehicle %1", i);
				ctx.EqualStr(want.m_Prefab, got.m_Prefab, at + " prefab");
				ctx.EqualStr(want.m_SpawnPrefab, got.m_SpawnPrefab, at + " spawn prefab");
				ctx.Equal(want.m_iBasePrice, got.m_iBasePrice, at + " base price");
				ctx.Equal(want.m_iBuyOverride, got.m_iBuyOverride, at + " buy override");
				ctx.Equal(want.m_iSellBackOverride, got.m_iSellBackOverride, at + " sell-back override");
				ctx.Equal(want.m_iChopOverride, got.m_iChopOverride, at + " chop override");
				ctx.Equal(want.m_iImpoundOverride, got.m_iImpoundOverride, at + " impound override");
				ctx.Equal(want.m_iGarageOverride, got.m_iGarageOverride, at + " garage override");
			}
		}
		ctx.EqualStr(payload, LF_ConfigStream.BuildPayload(null, true, client), "the client's values serialise to the server's payload");

		ctx.Equal(1800, client.PriceFor(LF_EVehiclePriceAction.BUY, LF_VehicleCatalog.S105), "the client prices the S105 from the owner's 1500 base at 120 percent");
		ctx.Equal(3000, client.PriceFor(LF_EVehiclePriceAction.BUY, LF_VehicleCatalog.UAZ469), "a vehicle the owner left out scales with the owner's percent");
		ctx.Equal(250, client.PriceFor(LF_EVehiclePriceAction.IMPOUND, LF_VehicleCatalog.UAZ469), "a shipped override survives the stream");

		// The format and hash checks still guard the vehicles section.
		ctx.Equal(LF_EConfigStreamStatus.UNCHANGED, LF_ConfigStream.Receive(LF_ConfigStream.FORMAT_VERSION, payload.Hash(), payload), "the same stream twice is unchanged");
		ctx.Equal(LF_EConfigStreamStatus.BAD_HASH, LF_ConfigStream.Receive(LF_ConfigStream.FORMAT_VERSION, payload.Hash() + 1, payload), "a vehicles payload that does not match its hash is refused");
		ctx.Equal(LF_EConfigStreamStatus.BAD_FORMAT, LF_ConfigStream.Receive(LF_ConfigStream.FORMAT_VERSION + 1, payload.Hash(), payload), "another format is refused");

		// No owner file: the stream carries the shipped values and the client does not move.
		LF_VehiclePricesConfig shipped = MakeServer("{\"schemaVersion\":1}");
		string shippedPayload = LF_ConfigStream.BuildPayload(null, false, shipped);
		LF_VehiclePricesConfig plain = MakeClient(ctx, shippedPayload, "no owner file");
		ctx.EqualStr(LF_ConfigStream.BuildPayload(null, false, ShippedCore()), LF_ConfigStream.BuildPayload(null, false, plain), "with no owner file the stream changes nothing on the client");
		ctx.Equal(1000, plain.PriceFor(LF_EVehiclePriceAction.BUY, LF_VehicleCatalog.S105), "with no owner file the S105 stays at the shipped 1000");

		// A bad value in the section: the client keeps its shipped vehicle prices.
		string bad = "{\"stream\":1,\"schemaVersion\":1,\"owner\":true,\"vehicles\":{\"buyPercent\":\"x\",\"vehicles\":{\"0\":{\"prefab\":\"{A1B2C3D4E5F60718}Prefabs/Vehicles/LF_New.et\",\"basePrice\":99}}}}";
		LF_ConfigService.Reset();
		LF_ConfigStream.Reset();
		LF_ConfigService.LoadOwnerString("{\"schemaVersion\":1}", "test-client");
		ctx.Equal(LF_EConfigStreamStatus.APPLIED, LF_ConfigStream.Receive(LF_ConfigStream.FORMAT_VERSION, bad.Hash(), bad), "a well-formed stream with a bad value arrives");
		LF_VehiclePricesConfig rejected = ShippedCore();
		rejected.ApplyLayers();
		ctx.Equal(100, rejected.m_iBuyPercent, "a bad field anywhere keeps the shipped vehicle percents");
		ctx.Equal(2, rejected.m_aVehicles.Count(), "a bad field anywhere keeps the shipped vehicle list");

		LF_ConfigStream.Reset();
		LF_ConfigService.Reset();
	}
};
