// red-proof: RED-OK everon/post-office-needs, observed 2026-11-27 (#5062) with `tools\cli test --no-build --red LF_Test_EveronPostOfficeNeeds`: with the hook on the test looks for the old "level 2" / "level 3" wording, so `the sign names the fuel delivery level` and `the sign names the taxi level` fail; green with `--only`.
//! #5062: the post office sign's Needs card names the levels the counter really asks for. The figures are read from
//! `LF_CivilianJob.RequiredLevel`, so a moved gate fails this test instead of misleading a player at the sign.
[LF_TestInfo(LF_TestTier.LOGIC, pack: true)]
class LF_Test_EveronPostOfficeNeeds : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/post-office-needs";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		int fuel = LF_CivilianJob.RequiredLevel(LF_EJobType.FUEL_DELIVERY);
		int taxi = LF_CivilianJob.RequiredLevel(LF_EJobType.TAXI);
		string needs = LF_Terms.Text("#LF-Sign_PostOffice_Needs");
		ctx.False(needs.IsEmpty(), "the post office Needs card has text");

		string fuelPhrase = string.Format("Fuel delivery needs work level %1", fuel);
		string taxiPhrase = string.Format("taxi work level %1", taxi);
		if (RedProof())
		{
			fuelPhrase = "Fuel delivery needs work level 2";
			taxiPhrase = "taxi work level 3";
		}

		ctx.True(needs.Contains(fuelPhrase), "the sign names the fuel delivery level");
		ctx.True(needs.Contains(taxiPhrase), "the sign names the taxi level");
	}
}
