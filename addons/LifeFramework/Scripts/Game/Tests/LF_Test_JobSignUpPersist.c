// red-proof: observed 2026-11-08 with `tools\cli wt test job-persist --no-build --only
// LF_Test_JobSignUpPersist`, the LF_CivilianJob.ApplySignUps call in the serializer's "jobsignups"
// read commented out (the sign-up saved but never restored, the #183 restart bug):
//   [LFTEST] SUMMARY tier=only passed=0 failed=1 total=1
//   [LFTEST]   - the courier is still signed up for parcels after the load (expected 5, got 0)
//   [LFTEST]   - the driver is still signed up for fuel after the load (expected 6, got 0)
//   [LFTEST]   - the restored courier can start a run without signing up again
// Restored; green.

//------------------------------------------------------------------------------------------------
// tier: PERSISTENCE
//! A signed-up delivery job survives a save and load (found by the #183 journey: PARCEL ->
//! UNEMPLOYED after a restart). The career level that gates the job rides the progression
//! payload; the sign-up rides "jobsignups" (save version 11). An active run is not saved.
class LF_Test_JobSignUpPersist : LF_Test
{
	protected static const string COURIER = "signup-courier";
	protected static const string DRIVER = "signup-driver";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "persist/job-signup-roundtrip";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_PersistenceComponent persistence = LF_TestProgressionKit.Persistence(ctx);
		if (!persistence)
			return;

		LF_Progression.Forget(COURIER);
		LF_Progression.Forget(DRIVER);
		array<ref LF_ProgressionRecord> progressionBefore = LF_Progression.ExportAll();
		array<ref LF_JobSignUpRecord> signUpsBefore = LF_CivilianJob.ExportSignUps();

		// The courier signs up at the counter; the driver reaches level 2, signs up for fuel, and
		// is mid-run at the save.
		ctx.True(LF_CivilianJob.SignUp(COURIER, LF_EJobType.PARCEL), "the courier signs up for parcels");
		LF_TestProgressionKit.SeedCourier(ctx, DRIVER);
		ctx.True(LF_CivilianJob.SignUp(DRIVER, LF_EJobType.FUEL_DELIVERY), "the level 2 driver signs up for fuel");
		ctx.True(LF_CivilianJob.StartRun(DRIVER, LF_EJobType.FUEL_DELIVERY), "the driver starts a fuel run");

		JsonSaveContext saveContext = new JsonSaveContext(false);
		ctx.True(LF_PersistenceComponentSerializer.SerializeStore(persistence, saveContext) == ESerializeResult.OK, "the store serializes");
		string saved = saveContext.SaveToString();
		ctx.True(saved.Contains("FUEL_DELIVERY"), "the save carries the fuel sign-up by name");

		// Restart: the session holds neither career nor sign-up.
		LF_Progression.Forget(COURIER);
		LF_Progression.Forget(DRIVER);
		ctx.Equal(LF_EJobType.UNEMPLOYED, LF_CivilianJob.GetSignedUpJob(COURIER), "the wiped courier is unemployed");

		JsonLoadContext loadContext = new JsonLoadContext(false);
		ctx.True(loadContext.LoadFromString(saved), "the save parses");
		int version;
		loadContext.ReadValue("version", version);
		ctx.Equal(27, version, "the save is written at version 27 (job sign-ups are payload 11)");
		LF_PersistenceComponentSerializer.ReadPersistencePayload(persistence, loadContext, "progression");
		LF_PersistenceComponentSerializer.ReadPersistencePayload(persistence, loadContext, "introductions");
		LF_PersistenceComponentSerializer.ReadPersistencePayload(persistence, loadContext, "jobsignups");

		ctx.Equal(LF_EJobType.PARCEL, LF_CivilianJob.GetSignedUpJob(COURIER), "the courier is still signed up for parcels after the load");
		ctx.Equal(LF_EJobType.FUEL_DELIVERY, LF_CivilianJob.GetSignedUpJob(DRIVER), "the driver is still signed up for fuel after the load");
		ctx.False(LF_CivilianJob.HasActiveRun(DRIVER), "the driver's run did not survive the restart");
		ctx.True(LF_CivilianJob.StartRun(COURIER, LF_EJobType.PARCEL), "the restored courier can start a run without signing up again");

		// A second apply converges and leaves the live run alone.
		JsonLoadContext again = new JsonLoadContext(false);
		again.LoadFromString(saved);
		again.ReadValue("version", version);
		LF_PersistenceComponentSerializer.ReadPersistencePayload(persistence, again, "progression");
		LF_PersistenceComponentSerializer.ReadPersistencePayload(persistence, again, "introductions");
		LF_PersistenceComponentSerializer.ReadPersistencePayload(persistence, again, "jobsignups");
		ctx.Equal(LF_EJobType.PARCEL, LF_CivilianJob.GetSignedUpJob(COURIER), "a second load keeps the courier on parcels");
		ctx.True(LF_CivilianJob.HasActiveRun(COURIER), "a second load does not end the courier's run");

		// Tolerance: an unknown job name, a job that is not delivery work, and a job above the
		// player's career level each leave the player unemployed.
		LF_Progression.Forget(COURIER);
		array<ref LF_JobSignUpRecord> broken = new array<ref LF_JobSignUpRecord>();
		broken.Insert(LF_JobSignUpRecord.Create(COURIER, "LF_REMOVED_JOB"));
		broken.Insert(LF_JobSignUpRecord.Create(COURIER, "MINER"));
		broken.Insert(LF_JobSignUpRecord.Create(COURIER, "TAXI"));
		broken.Insert(LF_JobSignUpRecord.Create("", "PARCEL"));
		broken.Insert(null);
		LF_CivilianJob.ApplySignUps(broken);
		ctx.Equal(LF_EJobType.UNEMPLOYED, LF_CivilianJob.GetSignedUpJob(COURIER), "unknown, non-delivery, and over-level sign-ups stay unemployed");

		LF_Progression.Forget(COURIER);
		LF_Progression.Forget(DRIVER);
		LF_Progression.ApplyAll(progressionBefore);
		LF_CivilianJob.ApplySignUps(signUpsBefore);
	}
}
