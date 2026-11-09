// red-proof: observed 2026-11-08 with `tools\cli test --no-build --only LF_Test_JobClientOffers` (lf-testbox) and
// LF_JobStartRunAction.ClientRefusal always refusing, the way the old offer behaved on a client:
// [LFTEST]   - a signed-up courier can pick up a parcel on the client (expected '', got '#EL-Job_NotSignedUp')
// [LFTEST]   - a signed-up fuel driver can take a can on the client (expected '', got '#EL-Job_NotSignedUp')
// Restored; green.

//! A dedicated client cannot read the server's sign-up and run maps. The parcel counter, the drop,
//! and abandon decide on the client from the job replicated on LF_PlayerJobComponent and the cargo
//! in the player's own inventory. Before this, "Pick up parcel" read the server map and was greyed
//! ("Sign up at the workplace first") for every player on a hosted server (M1 RC QA, 2026-11-08).
// tier: LOGIC
class LF_Test_JobClientOffers : LF_Test
{
	override string GetName()
	{
		return "jobs/client-offers";
	}

	override void Run(LF_TestContext ctx)
	{
		ctx.EqualStr("", LF_JobStartRunAction.ClientRefusal(LF_EJobType.PARCEL, LF_EJobType.PARCEL), "a signed-up courier can pick up a parcel on the client");
		ctx.EqualStr("", LF_JobStartRunAction.ClientRefusal(LF_EJobType.FUEL_DELIVERY, LF_EJobType.FUEL_DELIVERY), "a signed-up fuel driver can take a can on the client");
		ctx.EqualStr("#EL-Job_NotSignedUp", LF_JobStartRunAction.ClientRefusal(LF_EJobType.UNEMPLOYED, LF_EJobType.PARCEL), "nobody signed up: the parcel is refused");
		ctx.EqualStr("#EL-Job_NotSignedUp", LF_JobStartRunAction.ClientRefusal(LF_EJobType.TAXI, LF_EJobType.PARCEL), "signed up for another job: the parcel is refused");

		ctx.True(LF_CivilianJob.ClientRunOpen(LF_EJobType.PARCEL, true), "a courier carrying the parcel can complete the run");
		ctx.False(LF_CivilianJob.ClientRunOpen(LF_EJobType.PARCEL, false), "a courier without the parcel has no run to complete");
		ctx.True(LF_CivilianJob.ClientRunOpen(LF_EJobType.TAXI, false), "a taxi fare has no cargo; the server decides");
		ctx.False(LF_CivilianJob.ClientRunOpen(LF_EJobType.UNEMPLOYED, true), "no delivery job: no run");
	}
}
