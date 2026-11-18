// red-proof: observed 2026-11-18 (#3147): `tools\cli test --no-build --red LF_Test_EveronRunPay` (RedProof() drops the pack overlay): RED-OK, `the flagship pays $85 for a fuel run (expected 85, got 60)`, `the flagship pays $110 for a flat taxi run (expected 110, got 80)`. With the rows in LF_Jobs_Everon1989.conf it passes.
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

		LF_JobsConfig.Reload();
	}
}
