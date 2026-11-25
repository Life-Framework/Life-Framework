// red-proof: RED-OK jobs/workplace-job-restart 2026-11-25 `tools\cli test --no-build --red LF_Test_WorkplaceJobRestart`: with the section load skipped, "the miner comes back a miner after the restart (body job PARCEL)", the X-6 dry-run symptom. Green: `--only` passed, 36 assertions.
//! jobs/workplace-job-restart (#4959, the "workplacejobs" save section): a player who signed up as a courier
//! and then took the miner job at the mine desk came back from a restart as a courier (X-6 dry run,
//! 2026-11-25): the desk job lived only on the body, which a save does not keep, and the saved delivery
//! sign-up went back on the restored body. Now the desk job rides the save and goes on the restored body
//! after the sign-up. Loading twice gives the same job; a later delivery sign-up wins over the desk job;
//! a save without the section brings no desk job back. Across a restart (`tools\cli wt e2e`): every
//! non-reload run leaves a MINER record for E2E_UID, and the reload boot finds it.
[LF_TestInfo(LF_TestTier.PERSISTENCE)]
class LF_Test_WorkplaceJobRestart : LF_Test
{
	protected static const string UID = "test-workplacejob-a";
	static const string E2E_UID = "test-workplacejob-e2e";

	override string GetName()
	{
		return "jobs/workplace-job-restart";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		LF_PersistenceComponent persistence;
		if (gameMode)
			persistence = LF_PersistenceComponent.Cast(gameMode.FindComponent(LF_PersistenceComponent));

		ctx.NotNull(persistence, "the game mode carries LF_PersistenceComponent");
		if (!persistence)
			return;

#ifdef LF_E2E_LOAD_BOOT
		if (LF_E2E.RequiresSection(LF_WorkplaceJobsSavePayload.NAME))
			VerifyE2E(ctx);

		return;
#endif

		ctx.NotNull(LF_SaveSections.Get(LF_WorkplaceJobsSavePayload.NAME), "workplacejobs is a registered save section");
		ctx.Equal(0, LF_PersistenceComponentSerializer.PayloadSinceVersion(LF_WorkplaceJobsSavePayload.NAME), "workplacejobs is not a legacy positional payload");

		LF_Progression.Forget(UID);
		LF_CivilianJob.ForgetCareer(UID);
		IEntity body = LF_TestCharacterFixture.Spawn(ctx);
		LF_PlayerJobComponent jobs;
		if (body)
			jobs = LF_Component<LF_PlayerJobComponent>.Find(body);

		ctx.NotNull(jobs, "the fixture body carries LF_PlayerJobComponent");
		if (jobs)
			RunRestart(ctx, persistence, body, jobs);

		LF_Utils.SetUidForTests(null, "");
		LF_CivilianJob.ForgetCareer(UID);
		LF_Progression.Forget(UID);
		LF_TestCharacterFixture.Delete(body);
		// Every non-reload run (the e2e save boot among them) leaves the e2e miner for the shutdown save.
		LF_WorkplaceJobs.Note(E2E_UID, LF_EJobType.MINER);
		ctx.True(LF_WorkplaceJobs.Get(E2E_UID) == LF_EJobType.MINER, "e2e miner is recorded before the save");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunRestart(LF_TestContext ctx, LF_PersistenceComponent persistence, IEntity body, LF_PlayerJobComponent jobs)
	{
		LF_Utils.SetUidForTests(body, UID);

		// Hour 1: the parcel courier. Hour 3: the miner job at the mine desk, the newer one.
		LF_TestProgressionKit.SeedCourier(ctx, UID);
		ctx.True(LF_CivilianJob.SignUp(UID, LF_EJobType.PARCEL), "the player signs up as a parcel courier");
		ctx.Equal(LF_EJobType.MINER, LF_WorkplaceJob.Toggle(body, LF_EJobType.MINER), "the mine desk takes the player on as a miner");
		ctx.True(jobs.GetJob() == LF_EJobType.MINER, "the body holds the miner job before the save");

		JsonSaveContext saveContext = new JsonSaveContext(false);
		ctx.True(LF_PersistenceComponentSerializer.SerializeStore(persistence, saveContext) == ESerializeResult.OK, "the store serializes");
		string saved = saveContext.SaveToString();
		ctx.True(saved.Contains(LF_SaveSections.OBJECT_PREFIX + LF_WorkplaceJobsSavePayload.NAME), "the save carries the workplacejobs section");

		// The restart: no desk jobs in memory, and the save brings the body back with no job on it.
		LF_WorkplaceJobs.ResetStatics();
		jobs.SetJob(LF_EJobType.UNEMPLOYED);
		// RedProof(): the load is skipped, so the restored body gets only the courier sign-up (`--red`).
		if (!RedProof())
			ReadSection(saved, ctx);

		LF_SpawnLogic.AdoptLivingBody(UID, body);
		ctx.True(jobs.GetJob() == LF_EJobType.MINER, string.Format("the miner comes back a miner after the restart (body job %1)", typename.EnumToString(LF_EJobType, jobs.GetJob())));

		ReadSection(saved, ctx);
		ReadSection(saved, ctx);
		LF_SpawnLogic.AdoptLivingBody(UID, body);
		ctx.True(jobs.GetJob() == LF_EJobType.MINER, "a second load and hand-over keep the miner job");

		// A later delivery sign-up is the newer job: the restart brings the courier back, not the miner.
		ctx.True(LF_CivilianJob.SignUp(UID, LF_EJobType.PARCEL), "the miner signs up as a courier again");
		ctx.True(LF_WorkplaceJobs.Get(UID) == LF_EJobType.UNEMPLOYED, "the delivery sign-up clears the desk job");
		jobs.SetJob(LF_EJobType.UNEMPLOYED);
		LF_SpawnLogic.AdoptLivingBody(UID, body);
		ctx.True(jobs.GetJob() == LF_EJobType.PARCEL, "after a later sign-up the restored body is the courier");

		// A save without the section (an older save) brings no desk job back.
		LF_WorkplaceJobs.Note(UID, LF_EJobType.MINER);
		JsonSaveContext oldContext = new JsonSaveContext(false);
		array<string> omit = {LF_WorkplaceJobsSavePayload.NAME};
		LF_PersistenceComponentSerializer.SerializeStoreOmitting(persistence, oldContext, omit);
		string oldSave = oldContext.SaveToString();
		ctx.False(oldSave.Contains(LF_SaveSections.OBJECT_PREFIX + LF_WorkplaceJobsSavePayload.NAME), "the older save has no workplacejobs section");
		LF_WorkplaceJobs.ResetStatics();
		JsonLoadContext oldLoad = new JsonLoadContext(false);
		ctx.True(oldLoad.LoadFromString(oldSave), "the old save parses");
		LF_PersistenceComponentSerializer.DeserializeStore(persistence, oldLoad);
		ctx.True(LF_WorkplaceJobs.Get(UID) == LF_EJobType.UNEMPLOYED, "a pre-section save loads no desk job");
	}

	//------------------------------------------------------------------------------------------------
	//! Reads the workplacejobs section of a whole-store JSON save, as LF_SaveSections.ReadAll does.
	protected static void ReadSection(string saved, LF_TestContext ctx)
	{
		LF_SavePayload section = LF_SaveSections.Get(LF_WorkplaceJobsSavePayload.NAME);
		if (!section)
			return;

		JsonLoadContext context = new JsonLoadContext(false);
		ctx.True(context.LoadFromString(saved), "the save parses");
		ctx.True(context.StartObject(LF_SaveSections.OBJECT_PREFIX + LF_WorkplaceJobsSavePayload.NAME), "the workplacejobs section opens");
		int savedVersion;
		context.ReadValue(LF_SaveSections.VERSION_KEY, savedVersion);
		ctx.Equal(section.Version(), savedVersion, "the section carries its own version");
		ctx.True(section.ReadSection(context, savedVersion), "the workplacejobs section reads");
		context.EndObject();
	}

	//------------------------------------------------------------------------------------------------
	//! wt e2e reload boot: the e2e miner came back from the real save.
	static void VerifyE2E(LF_TestContext ctx)
	{
		LF_EJobType job = LF_WorkplaceJobs.Get(E2E_UID);
		ctx.True(job == LF_EJobType.MINER, "e2e miner is back after the restart");
		if (job == LF_EJobType.MINER)
			LF_Debug.Info(LF_WorkplaceJobs.FEATURE, "e2e restart: " + E2E_UID + " is still a MINER");
	}
}
