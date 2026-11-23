// red-proof: 2026-11-23 (#4531) the M923A1 name assertion failed on the pack patch without m_sNameKey (blank name seen in a session), passed with it; see the PR.
// red-proof: RED-OK everon/truck-yard, observed 2026-11-23 with `tools\cli test --no-build --red LF_Test_EveronTruckYard` (the hook forgets every row's lot id and capacity class): `the truck yard lists exactly the two trucks`, `the truck yard sells the Ural` and `the truck yard sells the M923A1` fail (the yard is empty, so the run stops there). The car-lot and licence assertions run only past that point; they are not red-proofed separately (follow-up). Hook off: green (--only, pack leg).
//! #4417 (0.3.0 W20), pack leg: under the Everon 1989 header the price rows put the trucks on the truck
//! yard. The car lot's list holds no row of capacity class `truck`; the truck yard's list holds the Ural
//! and the M923A1 at their prices; buying a truck without the truck licence is refused in words.
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronTruckYard : LF_Test
{
	protected static const string UID = "lf-test-everon-truck-yard";
	protected static const string YARD = "truck-yard";
	protected static const ResourceName URAL = "{A2E23CB8DDCFE82E}Prefabs/Vehicles/Wheeled/Ural4320/Ural4320_transport_CIV_base.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/truck-yard";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_VehiclePricesConfig.Reload();
		LF_VehiclePricesConfig config = LF_VehiclePricesConfig.Get();
		ctx.NotNull(config, "the vehicle prices load under the pack header");
		if (!config)
			return;

		if (RedProof())
			config.ClearLotRulesForTest();

		// The car lot (no lot id): sells cars and vans, no truck row.
		IEntity lotEntity = GetGame().GetWorld().FindEntityByName(LF_TestAnnex.VEHICLE_LOT);
		LF_VehicleLotComponent lot;
		if (lotEntity)
			lot = LF_Component<LF_VehicleLotComponent>.Find(lotEntity);

		ctx.NotNull(lot, "the world places a car lot");
		if (lot)
		{
			ctx.True(lot.GetOfferCount() > 0, "the car lot still sells cars and vans");
			bool truckOnCarLot;
			for (int i = 0; i < lot.GetOfferCount(); i++)
			{
				LF_VehicleOffer carOffer = lot.GetOfferAt(i);
				if (carOffer && carOffer.CapacityClassIn(config) == LF_VehicleLotComponent.CLASS_TRUCK)
					truckOnCarLot = true;
			}

			ctx.False(truckOnCarLot, "the car lot's list holds no row of capacity class truck");
		}

		// The truck yard: the trucks, at their prices.
		array<ref LF_VehicleOffer> yard = {};
		LF_VehicleLotComponent.CollectOffers(YARD, null, config, yard);
		ctx.Equal(2, yard.Count(), "the truck yard lists exactly the two trucks");

		LF_VehicleOffer ural = LF_VehicleLotComponent.OfferIn(yard, URAL);
		LF_VehicleOffer m923 = LF_VehicleLotComponent.OfferIn(yard, LF_VehicleCatalog.M923);
		ctx.NotNull(ural, "the truck yard sells the Ural");
		ctx.NotNull(m923, "the truck yard sells the M923A1");
		if (!ural || !m923)
		{
			LF_VehiclePricesConfig.Reload();
			return;
		}

		ctx.Equal(9500, ural.PriceIn(config), "the Ural costs $9,500");
		ctx.Equal(12000, m923.PriceIn(config), "the M923A1 costs $12,000");
		ctx.EqualStr(LF_VehicleLotComponent.CLASS_TRUCK, ural.CapacityClassIn(config), "the Ural is class truck");
		ctx.EqualStr(LF_VehicleLotComponent.CLASS_TRUCK, m923.CapacityClassIn(config), "the M923A1 is class truck");
		ctx.EqualStr("Cargo truck", WidgetManager.Translate(m923.GetNameKey()), "the M923A1 row has a name on the sheet (it showed blank, #4531)");

		// No truck licence: refused in words, with the check fee.
		LF_Progression.Forget(UID);
		string reason = LF_VehicleLotComponent.TruckRefusal(ural, UID, config);
		ctx.EqualStr(LF_VehicleLotComponent.KEY_NEEDS_TRUCK, reason, "a buyer without the truck licence is refused for a truck");
		ctx.EqualStr(LF_MoneyFormat.FormatMoney(LF_DrivingCheck.FEE), LF_VehicleLotComponent.ReasonArg(reason), "the refusal carries the truck check fee");

		// With the licence: not refused.
		LF_Progression.NoteFlag(UID, LF_DrivingCheck.LICENCE_FLAG);
		ctx.EqualStr("", LF_VehicleLotComponent.TruckRefusal(ural, UID, config), "with the truck licence the yard sells the truck");
		LF_Progression.Forget(UID);
		LF_VehiclePricesConfig.Reload();
	}
}
