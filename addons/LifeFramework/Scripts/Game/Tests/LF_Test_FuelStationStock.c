// red-proof: observed 2026-11-19 with `tools\cli test --no-build --red LF_Test_FuelStationStock`: RED PROOF OK (all 1 listed test(s) fail with the hook on), first `the cash is spent (expected 0, got 30)`; RedProof() skips the sale, so the tank, stock and cash assertions fail; green with `--only`: passed=2 failed=0. Review 2026-11-19: the cuffed-buyer case failed on the unguarded Sell (`a cuffed buyer is refused (expected 9, got 0)`), green with the guard.
//! A fuel pump sells from its station's stock (#3328): a fill moves litres into a real vehicle's fuel tank, takes
//! the same litres out of the station and charges cash for them; an empty station refuses and charges nothing;
//! a buyer with no cash gets nothing; a full tank is refused; fuel stock off leaves the vanilla pump alone; and
//! the pump takes the vanilla free refuel away while it sells.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_FuelStationStock : LF_Test
{
	static const ResourceName PUMP = "{3328A0000000F001}Prefabs/Props/Fuel/LF_FuelPump.et";
	static const ResourceName CAR = "{54C3CC22DEBD57BE}Prefabs/Vehicles/Wheeled/S105/S105_beige.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "fuel/station-stock";
	}

	//------------------------------------------------------------------------------------------------
	//! Empties every tank of the car, so the fill has room to take.
	protected void DrainTanks(IEntity car)
	{
		SCR_FuelManagerComponent manager = SCR_FuelManagerComponent.Cast(car.FindComponent(SCR_FuelManagerComponent));
		if (!manager)
			return;

		array<BaseFuelNode> nodes = {};
		manager.GetFuelNodesList(nodes);
		foreach (BaseFuelNode node : nodes)
		{
			node.SetFuel(0);
		}
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_FuelStationComponent.ResetStatics();
		// 2.00 a litre (200 hundredths), 100 litres of stock at the 'test-station' id, no row for it in the table.
		LF_FuelConfig.UseForTest(LF_FuelConfig.Make(true, 200, 100, 100, 100, 50, 1));

		IEntity buyer = LF_TestCharacterFixture.Spawn(ctx);
		if (!buyer)
			return;

		IEntity pump = LF_Utils.SpawnEntityPrefab(PUMP, buyer.GetOrigin() + "6 0 0");
		IEntity car = LF_Utils.SpawnEntityPrefab(CAR, buyer.GetOrigin() + "6 0 3");
		ctx.NotNull(pump, "the pump prefab spawns");
		ctx.NotNull(car, "the car spawns");
		SCR_FuelManagerComponent manager;
		LF_FuelStationComponent station;
		if (car)
			manager = SCR_FuelManagerComponent.Cast(car.FindComponent(SCR_FuelManagerComponent));
		if (pump)
			station = LF_FuelStationComponent.Cast(pump.FindComponent(LF_FuelStationComponent));

		ctx.NotNull(manager, "the car has a fuel manager");
		ctx.NotNull(station, "the pump carries a fuel station component");
		if (!manager || !station)
		{
			Cleanup(buyer, pump, car);
			return;
		}

		DrainTanks(car);
		string id = "test-station";
		ctx.True(station.SellsFromStock(), "with the config on the pump sells from stock");
		ctx.True(LF_FuelStationComponent.ReplacesVanilla(pump), "and the vanilla free refuel is off at this pump");
		ctx.Equal(0, Math.Round(manager.GetTotalFuel()), "the drained car holds no fuel");
		ctx.True(LF_FuelStationComponent.VehicleRoom(car) >= 20, "the drained car has room for a fill");
		ctx.Equal(100, LF_FuelStationComponent.StockFor(id).m_iLitres, "the station starts with its 100 litres");

		// No cash: nothing moves.
		LF_FuelSaleResult broke = LF_FuelStationComponent.Sell(id, car, buyer);
		ctx.Equal(LF_EFuelSale.NO_MONEY, broke.m_eResult, "a buyer with no cash is refused");
		ctx.Equal(0, Math.Round(manager.GetTotalFuel()), "no cash, no fuel in the tank");
		ctx.Equal(100, LF_FuelStationComponent.StockFor(id).m_iLitres, "no cash, no litres out of the station");

		// 30 cash buys 15 litres at 2.00: the tank takes 15, the station drops by 15, the cash is spent.
		ctx.Equal(30, LF_MoneyUtils.AddCash(buyer, 30), "seed 30 cash");
		LF_FuelSaleResult sale;
		if (!RedProof())
			sale = LF_FuelStationComponent.Sell(id, car, buyer);
		else
			sale = new LF_FuelSaleResult();

		ctx.Equal(LF_EFuelSale.OK, sale.m_eResult, "a fill the buyer can pay goes ahead");
		ctx.Equal(15, sale.m_iLitres, "30 cash buys 15 litres");
		ctx.Equal(30, sale.m_iCash, "and costs 30");
		ctx.Equal(15, Math.Round(manager.GetTotalFuel()), "the 15 litres are in the car's tank");
		ctx.Equal(85, LF_FuelStationComponent.StockFor(id).m_iLitres, "the station dropped by the same 15 litres");
		ctx.Equal(0, LF_MoneyUtils.GetCash(buyer), "the cash is spent");

		// Rich buyer: the fill is cut by the tank's room or the stock, whichever is smaller, and charged for what moved.
		LF_MoneyUtils.AddCash(buyer, 100000);
		int room = LF_FuelStationComponent.VehicleRoom(car);
		int expected = LF_Utils.MinInt(room, 85);
		LF_FuelSaleResult top = LF_FuelStationComponent.Sell(id, car, buyer);
		ctx.Equal(LF_EFuelSale.OK, top.m_eResult, "a rich buyer tops the tank up");
		ctx.Equal(expected, top.m_iLitres, "the fill is the tank's room or the stock, whichever is less");
		ctx.Equal(expected * 2, top.m_iCash, "charged for the litres that moved only");
		ctx.Equal(85 - expected, LF_FuelStationComponent.StockFor(id).m_iLitres, "the station dropped by those litres");
		ctx.Equal(100000 - top.m_iCash, LF_MoneyUtils.GetCash(buyer), "the cash dropped by the charge");

		// A cuffed buyer is refused; nothing moves.
		SCR_ChimeraCharacter chimera = SCR_ChimeraCharacter.Cast(buyer);
		if (chimera)
		{
			DrainTanks(car);
			int cuffCash = LF_MoneyUtils.GetCash(buyer);
			int cuffStock = LF_FuelStationComponent.StockFor(id).m_iLitres;
			chimera.LF_SetRestrained(true);
			LF_FuelSaleResult cuffed = LF_FuelStationComponent.Sell(id, car, buyer);
			chimera.LF_SetRestrained(false);
			ctx.Equal(LF_EFuelSale.UNFIT, cuffed.m_eResult, "a cuffed buyer is refused");
			ctx.Equal(cuffCash, LF_MoneyUtils.GetCash(buyer), "a cuffed buyer pays nothing");
			ctx.Equal(cuffStock, LF_FuelStationComponent.StockFor(id).m_iLitres, "a cuffed buyer takes no litres");
			ctx.Equal(0, Math.Round(manager.GetTotalFuel()), "a cuffed buyer fills nothing");
		}

		// A full tank is refused; nothing is charged.
		FillTanks(manager);
		int cashBefore = LF_MoneyUtils.GetCash(buyer);
		int stockBefore = LF_FuelStationComponent.StockFor(id).m_iLitres;
		LF_FuelSaleResult full = LF_FuelStationComponent.Sell(id, car, buyer);
		ctx.Equal(LF_EFuelSale.TANK_FULL, full.m_eResult, "a full tank is refused");
		ctx.Equal(cashBefore, LF_MoneyUtils.GetCash(buyer), "a full tank costs nothing");
		ctx.Equal(stockBefore, LF_FuelStationComponent.StockFor(id).m_iLitres, "a full tank leaves the stock alone");

		// An empty station refuses, charges nothing and fills nothing.
		DrainTanks(car);
		LF_FuelStock stock = LF_FuelStationComponent.StockFor(id);
		stock.m_iLitres = 0;
		cashBefore = LF_MoneyUtils.GetCash(buyer);
		LF_FuelSaleResult dry = LF_FuelStationComponent.Sell(id, car, buyer);
		ctx.Equal(LF_EFuelSale.EMPTY, dry.m_eResult, "an empty station refuses");
		ctx.Equal(0, dry.m_iLitres, "an empty station moves no litres");
		ctx.Equal(cashBefore, LF_MoneyUtils.GetCash(buyer), "an empty station charges nothing");
		ctx.Equal(0, Math.Round(manager.GetTotalFuel()), "an empty station fills nothing");
		ctx.EqualStr("#LF-Fuel_Dry", LF_FuelStationComponent.RefusalKey(dry.m_eResult), "the refusal is the dry-tanks line");

		// Two pumps of one station id share one tank; another id has its own.
		ctx.Equal(0, LF_FuelStationComponent.StockFor(id).m_iLitres, "the same id is the same tank");
		ctx.Equal(100, LF_FuelStationComponent.StockFor("other-station").m_iLitres, "another station id has its own tank");

		// Fuel stock off: the pump does not sell, and the vanilla pump is left alone.
		LF_FuelConfig.UseForTest(LF_FuelConfig.Make(false, 200, 100, 100, 100, 50, 1));
		ctx.False(station.SellsFromStock(), "with the config off the pump does not sell from stock");
		ctx.False(LF_FuelStationComponent.ReplacesVanilla(pump), "and the vanilla free refuel stays on");
		ctx.Equal(LF_EFuelSale.OFF, LF_FuelStationComponent.Sell("other-station", car, buyer).m_eResult, "a sale with fuel stock off is refused");

		Cleanup(buyer, pump, car);
	}

	//------------------------------------------------------------------------------------------------
	protected void FillTanks(SCR_FuelManagerComponent manager)
	{
		array<BaseFuelNode> nodes = {};
		manager.GetFuelNodesList(nodes);
		foreach (BaseFuelNode node : nodes)
		{
			node.SetFuel(node.GetMaxFuel());
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void Cleanup(IEntity buyer, IEntity pump, IEntity car)
	{
		LF_FuelConfig.Reload();
		LF_FuelStationComponent.ResetStatics();
		if (car)
			LF_TestCharacterFixture.Delete(car);
		if (pump)
			LF_TestCharacterFixture.Delete(pump);
		if (buyer)
			LF_TestCharacterFixture.Delete(buyer);
	}
}
