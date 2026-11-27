// red-proof: Observed 2026-11-27 with `tools\cli test --no-build --red LF_Test_FuelBuyAction,LF_Test_LootSearchAction,LF_Test_LockerRentAction,LF_Test_StallRentAction,LF_Test_StallBuyAction,LF_Test_LostAndFoundAction`: `RED PROOF OK (all 6 listed test(s) fail with the hook on)`; here `the cash is spent (expected 0, got 30)`, `the 15 litres are in the car's tank (expected 15, got 0)`. Green with `--only`.
// tier: WORLD
//! fuel/buy-action (#3328, coverage round 5): the "Fill the tank" prompt on a pump. It is offered only
//! while the station sells from stock. The perform runs the whole sale on the server through the pump's own
//! station: with no vehicle at the pump nothing is charged and no litre leaves the station; with a car there
//! and cash in hand the cash is spent, the litres reach the tank and the station's stock drops by the same
//! amount; with no cash left the next perform moves nothing; with fuel stock off the pump is a plain pump.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_FuelBuyAction : LF_Test
{
	static const ResourceName PUMP = "{3328A0000000F001}Prefabs/Props/Fuel/LF_FuelPump.et";
	static const ResourceName CAR = "{54C3CC22DEBD57BE}Prefabs/Vehicles/Wheeled/S105/S105_beige.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "fuel/buy-action";
	}

	//------------------------------------------------------------------------------------------------
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
		// 2.00 a litre, 100 litres of stock at every station id.
		LF_FuelConfig.UseForTest(LF_FuelConfig.Make(true, 200, 100, 100, 100, 50, 1));

		IEntity buyer = LF_TestCharacterFixture.Spawn(ctx);
		if (!buyer)
			return;

		IEntity pump = LF_Utils.SpawnEntityPrefab(PUMP, buyer.GetOrigin() + "6 0 0");
		ctx.NotNull(pump, "the pump prefab spawns");
		IEntity car;
		LF_FuelBuyAction action = LF_FuelBuyAction.Cast(LF_TestActionFinder.Find(pump, LF_FuelBuyAction));
		LF_FuelStationComponent station;
		SCR_FuelManagerComponent manager;
		if (pump)
			station = LF_FuelStationComponent.Cast(pump.FindComponent(LF_FuelStationComponent));

		ctx.NotNull(action, "the pump carries the fill-the-tank action");
		ctx.NotNull(station, "the pump carries a fuel station component");
		if (!action || !station)
		{
			Cleanup(buyer, pump, car);
			return;
		}

		string id = station.GetStationId();
		ctx.True(action.CanBeShownScript(buyer), "with fuel stock on the prompt is offered");
		ctx.True(action.CanBePerformedScript(buyer), "and it can be performed");
		ctx.Equal(100, LF_FuelStationComponent.StockFor(id).m_iLitres, "the station starts with its 100 litres");

		LF_MoneyUtils.RemoveCash(buyer, LF_MoneyUtils.GetCash(buyer));
		ctx.Equal(30, LF_MoneyUtils.AddCash(buyer, 30), "seed 30 cash");

		// No car at the pump: nothing is charged and no litre leaves the station.
		action.PerformAction(pump, buyer);
		ctx.Equal(30, LF_MoneyUtils.GetCash(buyer), "no vehicle at the pump, no charge");
		ctx.Equal(100, LF_FuelStationComponent.StockFor(id).m_iLitres, "no vehicle at the pump, no litre out of the station");

		// A car at the pump: 30 cash buys 15 litres. (Spawned there: a moved car is not found until the next frame.)
		car = LF_Utils.SpawnEntityPrefab(CAR, pump.GetOrigin() + "0 0 3");
		ctx.NotNull(car, "the car spawns");
		if (car)
			manager = SCR_FuelManagerComponent.Cast(car.FindComponent(SCR_FuelManagerComponent));

		ctx.NotNull(manager, "the car has a fuel manager");
		if (!manager)
		{
			Cleanup(buyer, pump, car);
			return;
		}

		DrainTanks(car);
		if (!RedProof())
			action.PerformAction(pump, buyer);

		ctx.Equal(0, LF_MoneyUtils.GetCash(buyer), "the cash is spent");
		ctx.Equal(15, Math.Round(manager.GetTotalFuel()), "the 15 litres are in the car's tank");
		ctx.Equal(85, LF_FuelStationComponent.StockFor(id).m_iLitres, "the station dropped by the same 15 litres");

		// Nothing left in the purse: the next perform moves nothing.
		action.PerformAction(pump, buyer);
		ctx.Equal(0, LF_MoneyUtils.GetCash(buyer), "a broke buyer is charged nothing");
		ctx.Equal(15, Math.Round(manager.GetTotalFuel()), "a broke buyer fills nothing");
		ctx.Equal(85, LF_FuelStationComponent.StockFor(id).m_iLitres, "a broke buyer takes no litres");

		// No buyer is a forged perform: nothing moves.
		action.PerformAction(pump, null);
		ctx.Equal(85, LF_FuelStationComponent.StockFor(id).m_iLitres, "a perform with no buyer takes no litres");

		// Fuel stock off: the pump is a plain pump, the prompt hides and a perform sells nothing.
		LF_FuelConfig.UseForTest(LF_FuelConfig.Make(false, 200, 100, 100, 100, 50, 1));
		LF_MoneyUtils.AddCash(buyer, 30);
		ctx.False(action.CanBeShownScript(buyer), "with fuel stock off the prompt is hidden");
		ctx.False(action.CanBePerformedScript(buyer), "and cannot be performed");
		action.PerformAction(pump, buyer);
		ctx.Equal(30, LF_MoneyUtils.GetCash(buyer), "with fuel stock off a perform charges nothing");
		ctx.Equal(15, Math.Round(manager.GetTotalFuel()), "with fuel stock off a perform fills nothing");

		LF_MoneyUtils.RemoveCash(buyer, LF_MoneyUtils.GetCash(buyer));
		Cleanup(buyer, pump, car);
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
