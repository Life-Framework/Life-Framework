// red-proof: `tools\cli test --no-build --red LF_Test_EveronNightRunRate` (RedProof() drops the pack jobs overlay): the three night rates read 1 instead of 1.25.
//! #3148 in game: under the Everon 1989 header (the pack leg of `tools\cli test`), the merged jobs
//! config gives the parcel, fuel and taxi runs a 1.25 night rate inside the shipped 21 to 5 window, so a
//! fuel run at $75 pays $94 at night ($19 extra).
[LF_TestInfo(LF_TestTier.LOGIC, pack: true)]
class LF_Test_EveronNightRunRate : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/night-run-rate";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_JobsConfig.Reload();
		if (RedProof())
			LF_JobsConfig.UseForTest(new LF_JobsConfig());

		LF_JobsConfig config = LF_JobsConfig.Get();
		ctx.NotNull(config, "the jobs config loads");
		if (!config)
			return;

		ctx.EqualFloat(1.25, config.NightRateFor("PARCEL"), 0.001, "the flagship parcel run has a 1.25 night rate");
		ctx.EqualFloat(1.25, config.NightRateFor("FUEL_DELIVERY"), 0.001, "the flagship fuel run has a 1.25 night rate");
		ctx.EqualFloat(1.25, config.NightRateFor("TAXI"), 0.001, "the flagship taxi run has a 1.25 night rate");

		int startHour, endHour;
		config.NightWindow(startHour, endHour);
		ctx.Equal(21, startHour, "the night starts at 21:00");
		ctx.Equal(5, endHour, "the night ends at 05:00");
		ctx.Equal(19, LF_CivilianJob.NightExtra(LF_CivilianJob.PayFor(LF_EJobType.FUEL_DELIVERY), config.NightRateFor("FUEL_DELIVERY")), "a $75 fuel run adds $19 at night");

		LF_JobsConfig.Reload();
	}
}
