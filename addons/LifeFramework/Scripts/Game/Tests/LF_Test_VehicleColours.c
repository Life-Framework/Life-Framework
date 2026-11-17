// red-proof: observed 2026-11-16 with `tools\cli test --no-build --red LF_Test_VehicleColours,LF_Test_VehicleColoursOverlay` (RED-OK for both): with the colours config not in use every colour assertion failed, e.g. `- the S105 sells in three colours (expected 3, got 1)`, `- a colour child is chopped for its model payout (expected 300, got 240)`, and a dropped colour on a model the prices config does not list respawned as the saved name. Green with the hook off.
// tier: LOGIC
//! Buyable car colours (epic #1826): a colour index maps to its locked child and to price plus delta,
//! a model with no colour row sells as today, the server range check refuses an index the model
//! does not sell, a delta that crosses the cash ceiling pays from the bank, a colour child is chopped
//! as its model, and a respawn whose saved colour no longer loads comes back in the base colour.
class LF_Test_VehicleColours : LF_Test
{
	static const ResourceName S105_RED = "{D8C109C681E7AAF1}Prefabs/Vehicles/Wheeled/M997/LF_M997_Ambulance.et";
	static const ResourceName S105_WHITE = "{EFEB161F9BC2031D}Prefabs/Vehicles/Wheeled/UAZ469/LF_UAZ469_Police.et";
	//! A colour a pack dropped: it no longer loads.
	static const ResourceName S105_DROPPED = "Prefabs/Vehicles/Wheeled/S105/DroppedColour.et";
	static const ResourceName SPARE = "{54E3267F42BD54ED}Prefabs/Vehicles/Wheeled/UAZ469/LF_UAZ469_LEDLightBar.et";

	override string GetName()
	{
		return "vehicles/colours";
	}

	//------------------------------------------------------------------------------------------------
	//! S105 in three colours (base free, red +50, white free) and a spare model in two (base, blue +50).
	protected LF_VehicleColoursConfig MakeColours()
	{
		LF_VehicleColoursConfig config = new LF_VehicleColoursConfig();
		config.m_aModels = {};

		LF_VehicleColourSet s105 = LF_VehicleColourSet.Make(LF_VehicleCatalog.S105);
		s105.m_aColours.Insert(LF_VehicleColour.Make("ColourOne", 16119260, LF_VehicleCatalog.S105_LOCKED, 0));
		s105.m_aColours.Insert(LF_VehicleColour.Make("ColourTwo", 9109504, S105_RED, 50));
		s105.m_aColours.Insert(LF_VehicleColour.Make("ColourThree", 16777215, S105_WHITE, 0));
		config.m_aModels.Insert(s105);

		LF_VehicleColourSet spare = LF_VehicleColourSet.Make(SPARE);
		spare.m_aColours.Insert(LF_VehicleColour.Make("ColourOne", 8421504, LF_VehicleCatalog.DANA2101_LOCKED, 0));
		spare.m_aColours.Insert(LF_VehicleColour.Make("ColourTwo", 255, LF_VehicleCatalog.SPUTNIK_T601_LOCKED, 50));
		config.m_aModels.Insert(spare);
		return config;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_VehicleColoursConfig colours = MakeColours();
		// The act the assertions guard: the colours config is the one the lot reads.
		if (!RedProof())
			LF_VehicleColoursConfig.Use(colours);

		LF_VehicleOffer s105 = LF_VehicleOffer.MakeWithSpawn(LF_VehicleCatalog.S105, LF_VehicleCatalog.S105_LOCKED, 1000, "S105");
		LF_VehicleOffer uaz = LF_VehicleOffer.MakeWithSpawn(LF_VehicleCatalog.UAZ469, LF_VehicleCatalog.UAZ469_LOCKED, 2500, "UAZ");

		// A model with no colour row sells as today: one colour, its own prefab and price.
		ctx.Equal(1, uaz.ColourCount(), "no colour row: one colour");
		ctx.True(uaz.ColourValid(0), "no colour row: colour 0 is sold");
		ctx.False(uaz.ColourValid(1), "no colour row: colour 1 is refused");
		ctx.EqualStr(LF_VehicleCatalog.UAZ469_LOCKED, uaz.SpawnPrefabFor(0), "no colour row: the offer's own locked child");
		ctx.Equal(2500, uaz.PriceFor(0), "no colour row: the offer's own price");

		// A colour index maps to its prefab and to price plus delta.
		ctx.Equal(3, s105.ColourCount(), "the S105 sells in three colours");
		ctx.EqualStr(LF_VehicleCatalog.S105_LOCKED, s105.SpawnPrefabFor(0), "colour 0 is the colour sold today");
		ctx.EqualStr(S105_RED, s105.SpawnPrefabFor(1), "colour 1 spawns its own prefab");
		ctx.EqualStr(S105_WHITE, s105.SpawnPrefabFor(2), "colour 2 spawns its own prefab");
		ctx.Equal(1000, s105.PriceFor(0), "colour 0 costs the base price");
		ctx.Equal(1050, s105.PriceFor(1), "colour 1 costs the base price plus its delta");
		ctx.Equal(1000, s105.PriceFor(2), "a colour with no delta costs the base price");

		// The server range check: an index the model does not sell is refused.
		ctx.False(s105.ColourValid(-1), "a negative colour is refused");
		ctx.False(s105.ColourValid(3), "a colour at the count is refused");
		ctx.False(s105.ColourValid(1000000), "a huge colour is refused");

		// A delta that crosses the cash ceiling moves payment to the bank.
		LF_VehicleOffer spare = LF_VehicleOffer.MakeWithSpawn(SPARE, LF_VehicleCatalog.DANA2101_LOCKED, 480, "Spare");
		int cashDebit;
		int bankDebit;
		ctx.True(LF_VehicleOwnership.QuotePurchase(spare.PriceFor(0), 500, 500, 0, cashDebit, bankDebit), "base colour: paid in cash");
		ctx.Equal(480, cashDebit, "base colour: cash debited");
		ctx.False(LF_VehicleOwnership.QuotePurchase(spare.PriceFor(1), 500, 1000, 0, cashDebit, bankDebit), "blue crosses the ceiling: cash alone cannot pay");
		ctx.True(LF_VehicleOwnership.QuotePurchase(spare.PriceFor(1), 500, 0, 600, cashDebit, bankDebit), "blue crosses the ceiling: the bank pays");
		ctx.Equal(530, bankDebit, "blue crosses the ceiling: bank debited price plus delta");
		ctx.Equal(0, cashDebit, "blue crosses the ceiling: no cash taken");

		// The chop shop pays a colour child as its model, not the yard's fallback.
		int baseCar = LF_ChopShopRules.PayoutForPrefab(LF_VehicleCatalog.S105_LOCKED);
		ctx.Equal(baseCar, LF_ChopShopRules.PayoutForPrefab(S105_RED), "a colour child is chopped for its model's payout");
		ctx.Equal(baseCar, LF_ChopShopRules.PayoutForPrefab(S105_WHITE), "every colour is chopped alike");

		// A respawn whose saved colour no longer loads comes back in the base colour.
		LF_VehiclePricesConfig prices = LF_VehiclePricesConfig.Get();
		LF_VehicleColoursConfig shipped = LF_VehicleColoursConfig.Get();
		ctx.EqualStr(LF_VehicleCatalog.S105_LOCKED, LF_VehicleOwnership.RespawnNameFor(S105_DROPPED, LF_VehicleCatalog.S105, false, shipped, prices), "a dropped colour respawns in the base colour");
		ctx.EqualStr(LF_VehicleCatalog.DANA2101_LOCKED, LF_VehicleOwnership.RespawnNameFor(LF_VehicleCatalog.SPUTNIK_T601_LOCKED, SPARE, false, shipped, prices), "a model the prices config does not list respawns in the colours config's base colour");
		ctx.EqualStr(S105_RED, LF_VehicleOwnership.RespawnNameFor(S105_RED, LF_VehicleCatalog.S105, true, colours, prices), "a colour that loads respawns as saved");
		ctx.EqualStr(LF_VehicleCatalog.S105_LOCKED, LF_VehicleOwnership.RespawnNameFor(S105_DROPPED, LF_VehicleCatalog.S105, false, null, prices), "no colours config: the prices config's locked child");
		ctx.EqualStr(S105_DROPPED, LF_VehicleOwnership.RespawnNameFor(S105_DROPPED, SPARE, false, null, prices), "nothing better to offer: the saved name stays");

		LF_VehicleColoursConfig.Reload();
	}
}
