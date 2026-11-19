// red-proof: `tools\cli test --no-build --red LF_Test_FuelHaul` (RedProof() skips the depot load and the pump-in, so the cash, cargo and stock assertions fail); the observed run is in the PR body.
//! The fuel haul (#3330): at the depot the hauler pays the wholesale price for a load into the tanker's cargo
//! tank; at a station the pump-in moves only the litres the station has room for into its stock and pays the
//! delivery price for exactly those, the rest staying in the tanker. 500 l bought, a station with 300 l of
//! room: stock +300, paid for 300, 200 l still in the tanker. A full station refuses, a broke hauler gets no
//! load, a full tanker takes none, a cuffed hauler is refused, and with fuel stock off nothing moves.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_FuelHaul : LF_Test
{
	static const ResourceName DEPOT = "{3330A0000000F001}Prefabs/Props/Fuel/LF_FuelDepot.et";
	static const ResourceName PUMP = "{3328A0000000F001}Prefabs/Props/Fuel/LF_FuelPump.et";
	static const ResourceName TANKER = "{4A59DAEFE645E8A0}Prefabs/Vehicles/Wheeled/Ural4320/Ural4320_tanker_CIV_orange.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "fuel/haul";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_FuelStationComponent.ResetStatics();
		LF_FuelHaul.ResetStatics();
		// Station table: 1000 l capacity, 700 l at the start (300 l of room); price 2.00, delivery pay 1.00, a 500 l
		// load per delivery at 0.60 wholesale.
		LF_FuelConfig config = LF_FuelConfig.Make(true, 200, 100, 1000, 700, 500, 1);
		config.m_iWholesalePerLitre = 60;
		LF_FuelConfig.UseForTest(config);

		IEntity hauler = LF_TestCharacterFixture.Spawn(ctx);
		if (!hauler)
			return;

		IEntity depot = LF_Utils.SpawnEntityPrefab(DEPOT, hauler.GetOrigin() + "6 0 0");
		IEntity pump = LF_Utils.SpawnEntityPrefab(PUMP, hauler.GetOrigin() + "6 0 30");
		IEntity tanker = LF_Utils.SpawnEntityPrefab(TANKER, hauler.GetOrigin() + "6 0 4");
		ctx.NotNull(depot, "the depot prefab spawns");
		ctx.NotNull(pump, "the pump prefab spawns");
		ctx.NotNull(tanker, "the tanker spawns");
		LF_FuelDepotComponent depotComp;
		LF_FuelStationComponent station;
		if (depot)
			depotComp = LF_FuelDepotComponent.Cast(depot.FindComponent(LF_FuelDepotComponent));
		if (pump)
			station = LF_FuelStationComponent.Cast(pump.FindComponent(LF_FuelStationComponent));

		ctx.NotNull(depotComp, "the depot carries a depot component");
		ctx.NotNull(station, "the pump carries a station component");
		if (!tanker || !depotComp || !station)
		{
			Cleanup(hauler, depot, pump, tanker);
			return;
		}

		string id = station.GetStationId();
		LF_FuelHaul.TakeCargo(tanker, 100000);
		ctx.True(LF_FuelHaul.CargoRoom(tanker) >= 500, "the tanker's cargo tank has room for a 500 l load");
		ctx.Equal(0, LF_FuelHaul.CargoLitres(tanker), "the emptied tanker holds nothing");
		ctx.True(depotComp.LoadsTankers(), "with the config on the depot loads tankers");
		ctx.True(LF_FuelHaul.FindTankerNear(depot) == tanker, "the depot finds the tanker parked beside it");
		ctx.Equal(700, LF_FuelStationComponent.StockFor(id).m_iLitres, "the station starts with 700 of 1000 litres");

		// A hauler with no cash gets no load.
		LF_FuelHaulResult broke = LF_FuelHaul.Buy(tanker, hauler);
		ctx.Equal(LF_EFuelHaul.NO_MONEY, broke.m_eResult, "a hauler with no cash is refused a load");
		ctx.Equal(0, LF_FuelHaul.CargoLitres(tanker), "no cash, no litres in the tanker");

		// 500 l at 0.60 is 300 cash.
		ctx.Equal(1000, LF_MoneyUtils.AddCash(hauler, 1000), "seed 1000 cash");
		LF_FuelHaulResult load;
		if (!RedProof())
			load = depotComp.LoadNearby(hauler);
		else
			load = new LF_FuelHaulResult();

		ctx.Equal(LF_EFuelHaul.OK, load.m_eResult, "the load goes ahead");
		ctx.Equal(500, load.m_iLitres, "the load is 500 litres");
		ctx.Equal(300, load.m_iCash, "at the wholesale price that costs 300");
		ctx.Equal(500, LF_FuelHaul.CargoLitres(tanker), "500 litres are in the tanker");
		ctx.Equal(700, LF_MoneyUtils.GetCash(hauler), "the hauler paid 300");

		// A cuffed hauler is refused; nothing moves.
		SCR_ChimeraCharacter chimera = SCR_ChimeraCharacter.Cast(hauler);
		if (chimera)
		{
			chimera.LF_SetRestrained(true);
			LF_FuelHaulResult cuffed = LF_FuelHaul.PumpIn(id, tanker, hauler);
			chimera.LF_SetRestrained(false);
			ctx.Equal(LF_EFuelHaul.UNFIT, cuffed.m_eResult, "a cuffed hauler is refused");
			ctx.Equal(500, LF_FuelHaul.CargoLitres(tanker), "a cuffed hauler moves no litres");
			ctx.Equal(700, LF_FuelStationComponent.StockFor(id).m_iLitres, "a cuffed hauler leaves the station alone");
		}

		// Pump in: the station has 300 l of room, so 300 land, the hauler is paid for 300, 200 stay in the tanker.
		LF_FuelHaulResult drop;
		if (!RedProof())
			drop = LF_FuelHaul.PumpIn(id, tanker, hauler);
		else
			drop = new LF_FuelHaulResult();

		ctx.Equal(LF_EFuelHaul.OK, drop.m_eResult, "the pump-in goes ahead");
		ctx.Equal(300, drop.m_iLitres, "only the 300 litres of room land");
		ctx.Equal(300, drop.m_iCash, "the hauler is paid for the 300 that arrived");
		ctx.Equal(1000, LF_FuelStationComponent.StockFor(id).m_iLitres, "the station stock rose by 300 to its capacity");
		ctx.Equal(200, LF_FuelHaul.CargoLitres(tanker), "200 litres stay in the tanker");
		ctx.Equal(1000, LF_MoneyUtils.GetCash(hauler), "the hauler's cash rose by 300");
		ctx.Equal(200, drop.m_iCargoAfter, "the result names the litres left");

		// A full station refuses and changes nothing.
		int cashBefore = LF_MoneyUtils.GetCash(hauler);
		LF_FuelHaulResult full = LF_FuelHaul.PumpIn(id, tanker, hauler);
		ctx.Equal(LF_EFuelHaul.STATION_FULL, full.m_eResult, "a full station refuses the delivery");
		ctx.Equal(200, LF_FuelHaul.CargoLitres(tanker), "a refused delivery leaves the litres in the tanker");
		ctx.Equal(cashBefore, LF_MoneyUtils.GetCash(hauler), "a refused delivery pays nothing");
		ctx.Equal(1000, LF_FuelStationComponent.StockFor(id).m_iLitres, "a refused delivery leaves the station stock");
		ctx.EqualStr("#LF-FuelHaul_StationFull", LF_FuelHaul.RefusalKey(full.m_eResult), "the refusal is the full-station line");

		// An empty tanker has nothing to pump.
		LF_FuelHaul.TakeCargo(tanker, 100000);
		LF_FuelStationComponent.StockFor(id).m_iLitres = 100;
		LF_FuelHaulResult empty = LF_FuelHaul.PumpIn(id, tanker, hauler);
		ctx.Equal(LF_EFuelHaul.TANKER_EMPTY, empty.m_eResult, "an empty tanker is refused");

		// A full tanker takes no load and the hauler is not charged.
		LF_FuelHaul.AddCargo(tanker, 100000);
		cashBefore = LF_MoneyUtils.GetCash(hauler);
		LF_FuelHaulResult tankerFull = LF_FuelHaul.Buy(tanker, hauler);
		ctx.Equal(LF_EFuelHaul.TANKER_FULL, tankerFull.m_eResult, "a full tanker takes no load");
		ctx.Equal(cashBefore, LF_MoneyUtils.GetCash(hauler), "a full tanker costs nothing");

		// A part-full tanker is topped up by its room only, and charged for that.
		LF_FuelHaul.TakeCargo(tanker, 100000);
		LF_FuelHaul.AddCargo(tanker, LF_FuelHaul.CargoRoom(tanker) - 100);
		cashBefore = LF_MoneyUtils.GetCash(hauler);
		LF_FuelHaulResult top = LF_FuelHaul.Buy(tanker, hauler);
		ctx.Equal(LF_EFuelHaul.OK, top.m_eResult, "a nearly full tanker takes what fits");
		ctx.Equal(100, top.m_iLitres, "only the 100 litres of room load");
		ctx.Equal(60, top.m_iCash, "and are charged at 0.60 a litre");
		ctx.Equal(cashBefore - 60, LF_MoneyUtils.GetCash(hauler), "the hauler paid for those 100 only");

		// Fuel stock off: neither step does anything.
		LF_FuelConfig.UseForTest(LF_FuelConfig.Make(false, 200, 100, 1000, 700, 500, 1));
		ctx.False(depotComp.LoadsTankers(), "with the config off the depot does not load");
		ctx.Equal(LF_EFuelHaul.OFF, LF_FuelHaul.Buy(tanker, hauler).m_eResult, "a load with fuel stock off is refused");
		ctx.Equal(LF_EFuelHaul.OFF, LF_FuelHaul.PumpIn(id, tanker, hauler).m_eResult, "a pump-in with fuel stock off is refused");

		Cleanup(hauler, depot, pump, tanker);
	}

	//------------------------------------------------------------------------------------------------
	protected void Cleanup(IEntity hauler, IEntity depot, IEntity pump, IEntity tanker)
	{
		LF_FuelConfig.Reload();
		LF_FuelStationComponent.ResetStatics();
		LF_FuelHaul.ResetStatics();
		if (tanker)
			LF_TestCharacterFixture.Delete(tanker);
		if (depot)
			LF_TestCharacterFixture.Delete(depot);
		if (pump)
			LF_TestCharacterFixture.Delete(pump);
		if (hauler)
			LF_TestCharacterFixture.Delete(hauler);
	}
}
