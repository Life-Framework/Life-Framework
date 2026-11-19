// red-proof: 2026-11-19, `tools cli test --no-build --red LF_Test_EscortNoDriving`: `RED-OK police/escort-no-driving (fails with the hook on, 8 assertions)`; the hook drops the escort hold from the seat rule.
//! #3431 review: a suspect held for escort is put in the police car's cargo seat uncuffed, so the driver's
//! seat rule must treat the escort hold like cuffs, or the suspect slides over and drives the police car off.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_EscortNoDriving : LF_Test
{
	override string GetName()
	{
		return "police/escort-no-driving";
	}

	override void Run(LF_TestContext ctx)
	{
		string officerUid = "escort-drive-officer";
		string suspectUid = "escort-drive-suspect";

		LF_PlayerAccount officer = LF_PlayerAccountManager.GetOrCreate(officerUid);
		officer.SetFaction(LF_Faction.POLICE);
		officer.SetOnDuty(true);
		LF_PlayerAccount suspect = LF_PlayerAccountManager.GetOrCreate(suspectUid);
		suspect.SetWantedLevel(2);

		ctx.False(LF_CuffEnforcement.HeldFromDriving(false, suspectUid), "a free suspect may drive");

		ctx.True(LF_JailEscort.Escort(officerUid, suspectUid), "the officer holds the suspect");
		bool held = LF_CuffEnforcement.HeldFromDriving(false, suspectUid);
		if (RedProof())
			held = false;

		ctx.True(held, "an escorted suspect, cuffed or not, may not drive");
		ctx.True(LF_CuffEnforcement.RefusesSeat(held, true), "so the driver's seat is refused");
		ctx.False(LF_CuffEnforcement.RefusesSeat(held, false), "a cargo seat stays allowed");
		ctx.True(LF_CuffEnforcement.HeldFromDriving(true, "nobody-held"), "cuffs alone still refuse the driver's seat");

		LF_JailEscort.ResetStatics();
	}
}
