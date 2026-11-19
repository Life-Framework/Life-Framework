// red-proof: `tools\cli test --no-build --red LF_Test_EveronFuelPumpStation` (RedProof() skips the pump spawn, so no pump carries a station id and the first checks fail).
//! everon/fuel-pump-station (#3337): the pump the Saint-Philippe forecourt places (`LF_FuelPump`) names a station id
//! that the pack fuel overlay lists. A pump whose id has no row still sells, but once the overlay lists any station
//! its saved litres are dropped on load, so the stock would refill on every restart (#3855 review). Every pump the pack
//! places must use saint-philippe, saint-pierre or morton; the forecourt layer sets no id of its own.
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronFuelPumpStation : LF_Test
{
	protected static const ResourceName PUMP = "{3328A0000000F001}Prefabs/Props/Fuel/LF_FuelPump.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/fuel-pump-station";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_FuelConfig.Reload();
		LF_FuelConfig config = LF_FuelConfig.Get();
		ctx.NotNull(config, "the fuel config loads under the pack header");
		if (!config)
			return;

		ctx.True(config.m_aStations && config.m_aStations.Count() > 0, "the pack overlay lists its stations");

		IEntity pump;
		if (!RedProof())
			pump = LF_Utils.SpawnEntityPrefab(PUMP, Vector(0, 500, 0));

		ctx.NotNull(pump, "the forecourt pump prefab spawns");
		if (!pump)
			return;

		LF_FuelStationComponent station = LF_FuelStationComponent.Cast(pump.FindComponent(LF_FuelStationComponent));
		ctx.NotNull(station, "the pump carries a fuel station component");
		if (station)
		{
			string id = station.GetStationId();
			ctx.EqualStr("saint-philippe", id, "the forecourt pump is the Saint-Philippe station");
			ctx.NotNull(config.FindStation(id), "the pack fuel overlay lists the pump's station id: " + id);
		}

		SCR_EntityHelper.DeleteEntityAndChildren(pump);
	}
}
