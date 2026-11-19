// red-proof: RED-OK residents/harness-name (fails with the hook on, 8 assertions; RedProof() skips the id parse)
//! residents/harness-name: the agent harness names a spawned resident `Resident_<id>` (#3650). The id is cut off the
//! prefix; other names, the bare prefix and a resident that is not live give nothing.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_ResidentHarnessName : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "residents/harness-name";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		string id;
		if (!RedProof())
			id = LF_ResidentHarness.IdOf("Resident_night_shift");

		ctx.True(id == "night_shift", "a Resident_ name carries the resident id");
		ctx.True(LF_ResidentHarness.IdOf("Resident_").IsEmpty(), "the bare prefix names no one");
		ctx.True(LF_ResidentHarness.IdOf("night_shift").IsEmpty(), "a name without the prefix is not a resident name");
		ctx.True(LF_ResidentHarness.IdOf("Player2").IsEmpty(), "a Player<id> name is not a resident name");
		ctx.True(LF_ResidentHarness.IdOf("Residential_door").IsEmpty(), "a name that only starts like the prefix is not one");
		ctx.True(LF_ResidentHarness.Named("Resident_no_such_resident_3650") == null, "a resident that is not live resolves to null");
		ctx.True(LF_ResidentHarness.Named("Fit_some_door") == null, "a plain name never resolves as a resident");
	}
}
