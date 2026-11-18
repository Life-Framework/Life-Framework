// red-proof: observed 2026-11-18 (#3147, #3164): `toolsi test --no-build --red LF_Test_EveronRunPay` (RedProof() drops the pack jobs and taxi fare overlays): RED-OK, `the flagship pays $85 for a fuel run (expected 85, got 60)`, `a 2000 m flagship fare pays $100 (expected 100, got 160)`, `a flagship short hop pays the $90 floor (expected 90, got 80)`. With the pack rows it passes.
//! #3147 in game: under the Everon 1989 header (the pack leg of `tools\cli test`), the merged jobs
//! config pays the flagship's run values: the parcel stays $40, the fuel run pays $85 and the flat taxi
//! run $110. `LF_CivilianJob.PayFor` is what a finished run hands over.
[LF_TestInfo(LF_TestTier.LOGIC, pack: true)]
class LF_Test_EveronRunPay : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/run-pay";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		// The leg boots under the pack header; layer the config again so the pack overlay applies.
		LF_JobsConfig.Reload();
		if (RedProof())
			LF_JobsConfig.UseForTest(new LF_JobsConfig());

		ctx.Equal(40, LF_CivilianJob.PayFor(LF_EJobType.PARCEL), "the flagship pays $40 for a parcel run");
		ctx.Equal(85, LF_CivilianJob.PayFor(LF_EJobType.FUEL_DELIVERY), "the flagship pays $85 for a fuel run");
		ctx.Equal(110, LF_CivilianJob.PayFor(LF_EJobType.TAXI), "the flagship pays $110 for a flat taxi run");

		// The flagship fare table keeps a typical 2000 m fare near $100 (about $600/h, under police
		// entry duty pay) and a short hop above the $85 fuel run (lead decision on #3164).
		LF_TaxiFareConfig.Reload();
		if (RedProof())
			LF_TaxiFareConfig.UseForTest(LF_TaxiFareConfig.CreateDefault());

		LF_TaxiFareConfig fares = LF_TaxiFareConfig.Get();
		ctx.Equal(100, LF_TaxiFare.PriceFor(fares, 2000), "a 2000 m flagship fare pays $100");
		ctx.Equal(90, LF_TaxiFare.PriceFor(fares, 50), "a flagship short hop pays the $90 floor");
		ctx.True(LF_TaxiFare.PriceFor(fares, 50) > LF_CivilianJob.PayFor(LF_EJobType.FUEL_DELIVERY), "a short hop pays more than a fuel run");

		LF_TaxiFareConfig.Reload();
		LF_JobsConfig.Reload();
	}
}
