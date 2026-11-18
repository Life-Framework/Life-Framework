// red-proof: 2026-11-18, `tools\cli test --no-build --only LF_Test_JobRunCargoLost` on the unfixed code (the new helpers stubbed): FAIL jobs/run-cargo-lost
//   (the courier's death ends the run; the deposit is kept by the server (expected 0, got 40); a drop-off after death pays nothing (expected '#EL-Job_NoActiveRun', got '');
//   a refused start gets a toast). Green after the fix. RedProof() skips the death event.

//------------------------------------------------------------------------------------------------
//! jobs/run-cargo-lost (#3356): a courier who dies or drops the parcel mid-run is not locked out of delivery
//! work. The run map is server state; only Abandon, Complete and a failed start end a run, and Abandon was
//! offered only while the cargo was in the player's own inventory. A downed carrier drops the parcel, so
//! Abandon greyed and "Pick up parcel" failed silently ("already has a run") until a restart.
//!   1. A dedicated client offers Abandon for a delivery job on the body, cargo in hand or not.
//!   2. The character's death (LF_OnPlayerKilled) ends the run as an abandon: no pay, the deposit and the
//!      contract stake stay forfeited, the run map is cleared, and a second death pays or refunds nothing.
//!   3. A refused start tells the player why (a toast key), it never returns in silence.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_JobRunCargoLost : LF_Test
{
	protected static const string UID = "job-cargo-lost-courier";
	protected static const string OTHER_UID = "job-cargo-lost-bystander";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "jobs/run-cargo-lost";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_JobsConfig.Reload();
		LF_CivilianJob.ForgetCareer(UID);
		LF_CivilianJob.ForgetCareer(OTHER_UID);
		RunClientOffer(ctx);
		RunDeath(ctx);
		RunRefusalToast(ctx);
		LF_CivilianJob.ForgetCareer(UID);
		LF_CivilianJob.ForgetCareer(OTHER_UID);
		LF_JobsConfig.Reload();
	}

	//------------------------------------------------------------------------------------------------
	protected void RunClientOffer(LF_TestContext ctx)
	{
		ctx.True(LF_CivilianJob.ClientRunOpen(LF_EJobType.PARCEL), "a courier whose parcel is gone still sees Abandon");
		ctx.True(LF_CivilianJob.ClientRunOpen(LF_EJobType.FUEL_DELIVERY), "a fuel driver whose can is gone still sees Abandon");
		ctx.True(LF_CivilianJob.ClientRunOpen(LF_EJobType.PARCEL), "and a courier carrying the parcel does too");
		ctx.False(LF_CivilianJob.ClientRunOpen(LF_EJobType.UNEMPLOYED), "no delivery job on the body: no Abandon");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunDeath(LF_TestContext ctx)
	{
		ctx.True(LF_CivilianJob.SignUp(UID, LF_EJobType.PARCEL), "the courier signs up");
		ctx.True(LF_CivilianJob.StartRun(UID, LF_EJobType.PARCEL), "a run starts");
		LF_CivilianJob.RecordRunStart(UID, Vector(0, 0, 0), 100);
		LF_CivilianJob.RecordDeposit(UID, 40);

		LF_EventBus.Publish(LF_OnPlayerKilled.Create(OTHER_UID, ""));
		ctx.True(LF_CivilianJob.HasActiveRun(UID), "somebody else's death leaves the courier's run alone");
		ctx.Equal(40, LF_CivilianJob.GetDeposit(UID), "and the deposit");

		if (!RedProof())
			LF_EventBus.Publish(LF_OnPlayerKilled.Create(UID, ""));

		ctx.False(LF_CivilianJob.HasActiveRun(UID), "the courier's death ends the run");
		ctx.Equal(0, LF_CivilianJob.GetDeposit(UID), "the deposit is kept by the server, nothing is left to give back");
		ctx.Equal(0, LF_CivilianJob.ReleaseDeposit(UID), "so it can never be refunded");
		ctx.True(LF_CivilianJob.GetActiveJob(UID) == LF_EJobType.UNEMPLOYED, "the run map holds no job for them");
		ctx.EqualStr(LF_CivilianJob.KEY_NO_RUN, LF_CivilianJob.DropoffRefusal(UID, Vector(400, 0, 0), 1000, 0), "a drop-off after death pays nothing");
		ctx.True(LF_CivilianJob.StartRun(UID, LF_EJobType.PARCEL), "after respawn a new run starts (no restart needed)");

		LF_EventBus.Publish(LF_OnPlayerKilled.Create("npc", ""));
		ctx.True(LF_CivilianJob.HasActiveRun(UID), "an NPC's death changes nothing");
		ctx.False(LF_CivilianJob.HasCareer("npc"), "and creates no career for the label");
		LF_CivilianJob.AbandonRun(UID);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunRefusalToast(LF_TestContext ctx)
	{
		ctx.EqualStr("", LF_JobStartRunAction.StartRefusalKey(LF_ERunStart.STARTED), "a started run has nothing to refuse");
		ctx.False(LF_JobStartRunAction.StartRefusalKey(LF_ERunStart.REFUSED).IsEmpty(), "a refused start gets a toast");
		ctx.False(LF_JobStartRunAction.StartRefusalKey(LF_ERunStart.ALREADY_RUN).IsEmpty(), "a start with a run still open gets a toast");
		ctx.False(LF_JobStartRunAction.StartRefusalKey(LF_ERunStart.NO_CARGO).IsEmpty(), "a start with no room for the cargo gets a toast");
		ctx.False(LF_JobStartRunAction.StartRefusalKey(LF_ERunStart.NO_DEPOSIT).IsEmpty(), "a start short of the deposit gets a toast");

		ctx.True(LF_CivilianJob.SignUp(UID, LF_EJobType.PARCEL), "the courier is signed up");
		ctx.True(LF_CivilianJob.StartRun(UID, LF_EJobType.PARCEL), "a run is open");
		int deposit;
		ctx.True(LF_CivilianJob.BeginRun(null, UID, LF_EJobType.PARCEL, 0, deposit) == LF_ERunStart.REFUSED, "no body: refused");
		ctx.True(LF_CivilianJob.HasActiveRun(UID), "a refused start leaves the open run untouched");
		LF_CivilianJob.AbandonRun(UID);
	}
}
