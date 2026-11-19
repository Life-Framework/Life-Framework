// red-proof: `tools\cli test --no-build --red LF_Test_FuelPumpCover` (RedProof() skips spawning the LF pump, so the vanilla pump is never covered and the first assertion fails).
//! A town fit puts `LF_FuelPump` on the exact spot of a vanilla pump the map carries (#3337): the vanilla pump under it
//! must give no free refuel while the fuel config is on, a vanilla pump with no LF pump on it stays free, and with the
//! config off the vanilla pump is untouched.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_FuelPumpCover : LF_Test
{
	static const ResourceName LF_PUMP = "{3328A0000000F001}Prefabs/Props/Fuel/LF_FuelPump.et";
	static const ResourceName VANILLA_PUMP = "{A4D0B331414901A5}Prefabs/Structures/Commercial/FuelStations/FuelStation_E_01/HouseAddons/FuelStation_E_01_tankpistol.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "fuel/pump-cover";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_FuelStationComponent.ResetStatics();
		LF_FuelConfig.UseForTest(LF_FuelConfig.Make(true, 200, 100, 100, 100, 50, 1));

		IEntity anchor = LF_TestCharacterFixture.Spawn(ctx);
		if (!anchor)
			return;

		vector spot = anchor.GetOrigin() + "6 0 0";
		IEntity covered = LF_Utils.SpawnEntityPrefab(VANILLA_PUMP, spot);
		IEntity alone = LF_Utils.SpawnEntityPrefab(VANILLA_PUMP, spot + "0 0 12");
		IEntity lfPump;
		if (!RedProof())
			lfPump = LF_Utils.SpawnEntityPrefab(LF_PUMP, spot);

		ctx.NotNull(covered, "the vanilla pump spawns");
		ctx.NotNull(alone, "the second vanilla pump spawns");
		ctx.True(LF_FuelStationComponent.ReplacesVanilla(covered), "the vanilla pump under an LF pump gives no free refuel");
		ctx.False(LF_FuelStationComponent.ReplacesVanilla(alone), "a vanilla pump with no LF pump on it stays vanilla");

		// Config off: the core default, the vanilla pump is left alone even under an LF pump.
		LF_FuelConfig.UseForTest(LF_FuelConfig.Make(false, 200, 100, 100, 100, 50, 1));
		ctx.False(LF_FuelStationComponent.ReplacesVanilla(covered), "with fuel stock off the covered pump is vanilla again");

		LF_FuelConfig.Reload();
		LF_FuelStationComponent.ResetStatics();
		if (lfPump)
			LF_TestCharacterFixture.Delete(lfPump);
		if (covered)
			LF_TestCharacterFixture.Delete(covered);
		if (alone)
			LF_TestCharacterFixture.Delete(alone);
		LF_TestCharacterFixture.Delete(anchor);
	}
}
