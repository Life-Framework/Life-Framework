// red-proof: observed 2026-11-18 (#3147): `tools\cli test --no-build --red LF_Test_CivilianRunPayConfig` (RedProof() hands PayFor a
// config with no rows, which is what the old constants-only PayFor always saw): RED-OK, `a fuel run pays its configured row
// (expected 85, got 60)`, `a taxi run pays its configured row (expected 110, got 80)`. Without the hook it passes.
//! #3147: what a delivery run pays comes from the jobs config's `runPays` rows. A config with its own
//! rows changes `LF_CivilianJob.PayFor` for each job; a missing row, or one below 1, keeps the core
//! default (the PAY_ constant) and logs `LF_Debug.Error`.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_CivilianRunPayConfig : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "jobs/civilian-run-pay-config";
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_JobsConfig MakeConfig(array<ref LF_JobRunPayDef> rows)
	{
		LF_JobsConfig config = new LF_JobsConfig();
		config.m_aTracks = {};
		config.m_aXpAwards = {};
		config.m_aLicenceGates = {};
		config.m_aRunPays = rows;
		return config;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		// A config with its own rows pays them.
		array<ref LF_JobRunPayDef> rows = {};
		rows.Insert(LF_JobRunPayDef.Make("PARCEL", 55));
		rows.Insert(LF_JobRunPayDef.Make("FUEL_DELIVERY", 85));
		rows.Insert(LF_JobRunPayDef.Make("TAXI", 110));
		if (!RedProof())
			LF_JobsConfig.UseForTest(MakeConfig(rows));
		else
			LF_JobsConfig.UseForTest(MakeConfig({}));

		ctx.Equal(55, LF_CivilianJob.PayFor(LF_EJobType.PARCEL), "a parcel run pays its configured row");
		ctx.Equal(85, LF_CivilianJob.PayFor(LF_EJobType.FUEL_DELIVERY), "a fuel run pays its configured row");
		ctx.Equal(110, LF_CivilianJob.PayFor(LF_EJobType.TAXI), "a taxi run pays its configured row");

		// A missing row keeps the core default for that job only.
		array<ref LF_JobRunPayDef> partial = {};
		partial.Insert(LF_JobRunPayDef.Make("FUEL_DELIVERY", 85));
		LF_JobsConfig.UseForTest(MakeConfig(partial));
		ctx.Equal(LF_CivilianJob.PAY_PARCEL, LF_CivilianJob.PayFor(LF_EJobType.PARCEL), "a parcel run with no row keeps the core default");
		ctx.Equal(85, LF_CivilianJob.PayFor(LF_EJobType.FUEL_DELIVERY), "the row that exists still applies");
		ctx.Equal(LF_CivilianJob.PAY_TAXI, LF_CivilianJob.PayFor(LF_EJobType.TAXI), "a taxi run with no row keeps the core default");

		// A row below 1 is refused: the core default pays.
		array<ref LF_JobRunPayDef> bad = {};
		bad.Insert(LF_JobRunPayDef.Make("PARCEL", 0));
		bad.Insert(LF_JobRunPayDef.Make("FUEL_DELIVERY", -5));
		LF_JobsConfig.UseForTest(MakeConfig(bad));
		ctx.Equal(LF_CivilianJob.PAY_PARCEL, LF_CivilianJob.PayFor(LF_EJobType.PARCEL), "a parcel row of 0 keeps the core default");
		ctx.Equal(LF_CivilianJob.PAY_FUEL, LF_CivilianJob.PayFor(LF_EJobType.FUEL_DELIVERY), "a fuel row below 0 keeps the core default");

		// The shipped core config pays what the constants say (the neutral defaults).
		LF_JobsConfig.Reload();
		LF_JobsConfig shipped = LF_JobsConfig.Get();
		ctx.NotNull(shipped, "the core jobs config loads");
		if (shipped)
		{
			ctx.Equal(LF_CivilianJob.PAY_PARCEL, shipped.RunPayFor("PARCEL", 1), "the core config pays the default parcel run");
			ctx.Equal(LF_CivilianJob.PAY_FUEL, shipped.RunPayFor("FUEL_DELIVERY", 1), "the core config pays the default fuel run");
			ctx.Equal(LF_CivilianJob.PAY_TAXI, shipped.RunPayFor("TAXI", 1), "the core config pays the default flat taxi run");
		}

		LF_JobsConfig.Reload();
	}
}
