// red-proof: 2026-11-22 `node tools/cli.mjs test --no-build --red LF_Test_VehiclePadReach`: RED PROOF OK (all 1 listed test(s) fail with the hook on). The live failure it guards: island session 00:57:29 `pad roadmap-lot slot 1 ... is taken by ...S1203_cargo_brown.et at <4353.5,11.811,10652>`, then `spawn refused: no free pad for roadmap-lot`.
//! vehicles/pad-reach: a vehicle takes a lot pad only when its body can reach it. The numbers are
//! the island car lot from the 2026-11-21 release walk: the display van beside pad 0 holds it, the
//! cash van parked at its stop 36 m away and at the fuel station 211 m away does not.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_VehiclePadReach : LF_Test
{
	override string GetName()
	{
		return "vehicles/pad-reach";
	}

	override void Run(LF_TestContext ctx)
	{
		vector slot0 = "4389.97 15.742 10639.2";
		vector slot1 = "4389.97 15.742 10647.2";
		float radius = 3;

		bool display = true;
		bool parkedVan = false;
		bool farVan = false;
		bool onPad = true;
		if (!RedProof())
		{
			display = LF_VehicleSpawnPointComponent.IsNearPad("4384.86 14.7 10635.4", slot0, radius);
			parkedVan = LF_VehicleSpawnPointComponent.IsNearPad("4353.5 11.811 10652", slot1, radius);
			farVan = LF_VehicleSpawnPointComponent.IsNearPad("4601.06 18.113 10654.2", slot1, radius);
			onPad = LF_VehicleSpawnPointComponent.IsNearPad(slot1 + "0.5 0 1", slot1, radius);
		}
		else
		{
			display = false;
			parkedVan = true;
			farVan = true;
			onPad = false;
		}

		ctx.True(display, "the display van 6.5 m from pad 0 still holds it");
		ctx.False(parkedVan, "the cash van at its stop 36 m away does not hold pad 1");
		ctx.False(farVan, "the cash van at the fuel station 211 m away does not hold pad 1");
		ctx.True(onPad, "a car standing on the pad holds it");
	}
}
