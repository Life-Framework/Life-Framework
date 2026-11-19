// red-proof: before the pack overlay exists (`Configs/Fuel/LF_Fuel_Everon1989.conf`), `tools\cli test --no-build --only LF_Test_EveronFuelHaul` fails "the header lists the fuel overlay" and every value reads as the core default (fuel off, no tanker prefab, no station rows); the observed run is in the PR body.
//! #3331 in game: under the Everon 1989 header (the pack leg of `tools\cli test`), the fuel overlay is listed
//! and picked up by the core fuel config. It turns fuel stock on, names the depot's wholesale price, the
//! delivery pay, the tanker (a vanilla civilian Ural tanker) and each station's cap and starting stock. A
//! full haul's margin per hour, by this test's own arithmetic, sits in the civilian band under the duty pay
//! (1.5 to 2.5 times below police, `tools\cli economy`) and above the garbage round's hour.
[LF_TestInfo(LF_TestTier.LOGIC, pack: true)]
class LF_Test_EveronFuelHaul : LF_Test
{
	static const ResourceName OVERLAY = "{3331A0000000F001}Configs/Fuel/LF_Fuel_Everon1989.conf";

	//! Assumed minutes for one haul: load at the depot, drive, pump into the station, drive back. The
	//! economy command's per-run minutes are assumptions too.
	static const int HAUL_MINUTES = 60;
	//! Police patrol pay per hour (`tools\cli economy` duty pay); civilian jobs sit 1.5 to 2.5 times below.
	static const int POLICE_PER_HOUR = 900;
	//! The garbage round's hour: about an hour of parcels (card #3326, `tools\cli economy` PARCEL).
	static const int GARBAGE_PER_HOUR = 400;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/fuel-haul";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_MissionHeader header = LF_MissionHeader.GetCurrent();
		ctx.NotNull(header, "the pack leg runs under an LF mission header");
		if (header && header.m_aLF_ConfigOverlays)
			ctx.True(header.m_aLF_ConfigOverlays.Find(OVERLAY) != -1, "the header lists the fuel overlay");
		else
			ctx.Fail("the header lists no config overlays");

		array<ref Resource> resources = {};
		array<ResourceName> names = {};
		LF_ConfigOverlay.LoadPackOverlays(LF_FuelConfig.CONFIG_PATH, LF_FuelConfig, "fuel", resources, names);
		ctx.True(names.Find(OVERLAY) != -1, "the fuel loader picks up its overlay");

		LF_FuelConfig config;
		if (!RedProof())
		{
			LF_FuelConfig.Reload();
			config = LF_FuelConfig.Get();
		}

		ctx.NotNull(config, "the fuel config loads");
		if (!config)
			return;

		ctx.True(config.m_bEnabled, "the pack turns fuel stock on");
		ctx.EqualStr("", config.Problem(), "the merged table is usable");
		ctx.EqualStr("{4A59DAEFE645E8A0}Prefabs/Vehicles/Wheeled/Ural4320/Ural4320_tanker_CIV_orange.et", config.m_TankerPrefab, "the tanker is the vanilla civilian Ural tanker");
		ctx.True(config.m_iWholesalePerLitre > 0, "the depot charges for a load");
		ctx.True(config.m_iWholesalePerLitre < config.m_iDeliveryPayPerLitre, "a litre pays more than it cost at the depot");

		// Every station takes a full haul with its starting stock in the tanks, so the first delivery lands whole.
		ctx.True(config.m_aStations && config.m_aStations.Count() == 3, "the pack lists three stations");
		if (config.m_aStations)
		{
			foreach (LF_FuelStationRow row : config.m_aStations)
			{
				ctx.True(row.m_iCapacity - row.m_iStartLitres >= config.m_iLitresPerDelivery, string.Format("station %1 has room for a full haul", row.m_sId));
				LF_FuelStock stock = config.NewStationFor(row.m_sId);
				ctx.Equal(row.m_iCapacity, stock.m_iCapacity, string.Format("station %1 is made at its own cap", row.m_sId));
				ctx.Equal(row.m_iStartLitres, stock.m_iLitres, string.Format("station %1 starts with its own litres", row.m_sId));
			}
		}

		// A full haul by the test's own arithmetic: hundredths per litre, litres, cash units.
		int scale = LF_FuelStockRules.PRICE_SCALE;
		int buyIn = config.m_iLitresPerDelivery * config.m_iWholesalePerLitre / scale;
		int pay = config.m_iLitresPerDelivery * config.m_iDeliveryPayPerLitre / scale;
		int margin = pay - buyIn;
		int perHour = margin * 60 / HAUL_MINUTES;
		ctx.True(buyIn <= 1000, "the buy-in is affordable from the $1000 start");
		ctx.True(margin * 3 >= pay - 5 && margin * 3 <= pay + 5, "the margin is about a third of the pay");
		ctx.True(perHour > GARBAGE_PER_HOUR, string.Format("a haul's margin per hour ($%1) beats the garbage round ($%2)", perHour, GARBAGE_PER_HOUR));
		ctx.True(perHour * 10 <= GARBAGE_PER_HOUR * 12, string.Format("and only a little above it ($%1)", perHour));
		ctx.True(perHour * 250 >= POLICE_PER_HOUR * 100 && perHour * 150 <= POLICE_PER_HOUR * 100, string.Format("the hour sits 1.5 to 2.5 times below the duty pay ($%1 of $%2)", perHour, POLICE_PER_HOUR));
	}
}
