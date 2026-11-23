// red-proof: 2026-11-23 `tools\cli test --no-build --red LF_Test_BigJobLock`: RED PROOF OK, with the second job asking on a released lock and the load skipped (`the job running at the save is in its full cooldown after a second load (expected 7200, got 0)`). Green: `--only` passed.
//! The server-wide big-job lock (#4193, plan 0.3.0 Crime A6, decision N17): while one big job runs a
//! second is refused in words; a finished job's cooldown refuses it again until it runs out while
//! another big job may start; a lock never finished expires into its cooldown; the "bigjob" save
//! section brings the cooldowns back after a save and load, a running job as its full cooldown,
//! loading twice gives the same state. Across a restart (`tools\cli wt e2e`): every non-reload run
//! leaves E2E_JOB in its cooldown; the reload boot finds it still cooling down.
[LF_TestInfo(LF_TestTier.PERSISTENCE)]
class LF_Test_BigJobLock : LF_Test
{
	protected static const string PHARMACY = "test-bigjob-pharmacy";
	protected static const string BANK = "test-bigjob-bank";
	static const string E2E_JOB = "test-bigjob-e2e";
	protected static const string CREW_A = "test-bigjob-a";
	protected static const string CREW_B = "test-bigjob-b";
	protected static const int T0 = 1000;

	override string GetName()
	{
		return "crime/big-job-lock";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
#ifdef LF_E2E_LOAD_BOOT
		if (LF_E2E.RequiresSection(LF_BigJobLockSavePayload.NAME))
			VerifyE2E(ctx);

		return;
#endif

		SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		LF_PersistenceComponent persistence;
		if (gameMode)
			persistence = LF_PersistenceComponent.Cast(gameMode.FindComponent(LF_PersistenceComponent));

		ctx.NotNull(persistence, "the game mode carries LF_PersistenceComponent");
		ctx.NotNull(LF_SaveSections.Get(LF_BigJobLockSavePayload.NAME), "bigjob is a registered save section");

		LF_BigJobLock.ResetStatics();
		LF_BigJobLock.SetClockForTests(T0);
		RunLock(ctx);
		if (persistence)
			RunPersist(ctx, persistence);

		// Every non-reload run (the e2e save boot among them) leaves the e2e job in its cooldown.
		LF_BigJobLock.ResetStatics();
		ctx.Equal(LF_EBigJobLock.OK, LF_BigJobLock.TryBegin(E2E_JOB, CREW_A, LF_BigJobLock.COOLDOWN_BANK, LF_BigJobLock.Now()), "the e2e job starts");
		ctx.True(LF_BigJobLock.Finish(E2E_JOB, LF_BigJobLock.Now()), "the e2e job ends, its cooldown starts");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunLock(LF_TestContext ctx)
	{
		ctx.Equal(LF_EBigJobLock.OK, LF_BigJobLock.TryBegin(PHARMACY, CREW_A, LF_BigJobLock.COOLDOWN_PHARMACY, T0), "the pharmacy job starts on a free lock");
		ctx.True(LF_BigJobLock.RunningJob(T0) == PHARMACY, "the pharmacy holds the lock");

		// RedProof(): the lock is released before the second job asks, so the refusals below fail (`--red`).
		if (RedProof())
			LF_BigJobLock.ResetStatics();

		LF_EBigJobLock second = LF_BigJobLock.TryBegin(BANK, CREW_B, LF_BigJobLock.COOLDOWN_BANK, T0 + 60);
		ctx.Equal(LF_EBigJobLock.BUSY, second, "a second big job is refused while one runs");
		string words = LF_BigJobLock.RefusalText(second, BANK, T0 + 60);
		ctx.True(!words.IsEmpty() && !words.StartsWith("#"), string.Format("the refusal is in words: '%1'", words));
		ctx.Equal(LF_EBigJobLock.BUSY, LF_BigJobLock.TryBegin(PHARMACY, CREW_B, LF_BigJobLock.COOLDOWN_PHARMACY, T0 + 61), "the same job cannot start twice");

		ctx.False(LF_BigJobLock.Finish(BANK, T0 + 100), "finishing a job that is not running changes nothing");
		ctx.True(LF_BigJobLock.Finish(PHARMACY, T0 + 600), "the pharmacy job ends");
		ctx.True(LF_BigJobLock.RunningJob(T0 + 600).IsEmpty(), "the lock is free after the job ends");

		// The cooldown holds the same job and lets another big job start.
		int t1 = T0 + 600;
		ctx.Equal(LF_EBigJobLock.COOLDOWN, LF_BigJobLock.TryBegin(PHARMACY, CREW_B, LF_BigJobLock.COOLDOWN_PHARMACY, t1 + 60), "the pharmacy is refused during its cooldown");
		string cool = LF_BigJobLock.RefusalText(LF_EBigJobLock.COOLDOWN, PHARMACY, t1 + 60);
		ctx.True(cool.Contains("119"), string.Format("the cooldown refusal names the minutes left (119): '%1'", cool));
		ctx.Equal(LF_EBigJobLock.COOLDOWN, LF_BigJobLock.TryBegin(PHARMACY, CREW_B, LF_BigJobLock.COOLDOWN_PHARMACY, t1 + LF_BigJobLock.COOLDOWN_PHARMACY - 1), "still refused 1 s before the cooldown ends");
		ctx.Equal(LF_EBigJobLock.OK, LF_BigJobLock.TryBegin(BANK, CREW_B, LF_BigJobLock.COOLDOWN_BANK, t1 + 120), "the bank job may start while the pharmacy cools down");
		ctx.True(LF_BigJobLock.Finish(BANK, t1 + 900), "the bank job ends");
		ctx.Equal(LF_EBigJobLock.OK, LF_BigJobLock.TryBegin(PHARMACY, CREW_A, LF_BigJobLock.COOLDOWN_PHARMACY, t1 + LF_BigJobLock.COOLDOWN_PHARMACY), "the pharmacy starts once its cooldown ran out");

		// A lock never finished expires into its cooldown.
		int t2 = t1 + LF_BigJobLock.COOLDOWN_PHARMACY;
		ctx.True(LF_BigJobLock.RunningJob(t2 + LF_BigJobLock.MAX_RUN_SECONDS).IsEmpty(), "an unfinished lock expires after MAX_RUN_SECONDS");
		ctx.Equal(LF_BigJobLock.COOLDOWN_PHARMACY, LF_BigJobLock.CooldownLeft(PHARMACY, t2 + LF_BigJobLock.MAX_RUN_SECONDS), "the expired job is in its full cooldown");
		ctx.Equal(LF_EBigJobLock.BAD_INPUT, LF_BigJobLock.TryBegin("", CREW_A, 60, t2), "an empty job key is refused");
		LF_BigJobLock.ResetStatics();
	}

	//------------------------------------------------------------------------------------------------
	protected void RunPersist(LF_TestContext ctx, LF_PersistenceComponent persistence)
	{
		LF_BigJobLock.ResetStatics();
		// The bank ran and is cooling down, 1000 s in; the pharmacy is running at the save.
		LF_BigJobLock.SetClockForTests(T0);
		ctx.Equal(LF_EBigJobLock.OK, LF_BigJobLock.TryBegin(BANK, CREW_A, LF_BigJobLock.COOLDOWN_BANK, T0), "the bank job starts");
		ctx.True(LF_BigJobLock.Finish(BANK, T0), "the bank job ends");
		LF_BigJobLock.SetClockForTests(T0 + 1000);
		ctx.Equal(LF_EBigJobLock.OK, LF_BigJobLock.TryBegin(PHARMACY, CREW_B, LF_BigJobLock.COOLDOWN_PHARMACY, T0 + 1000), "the pharmacy job starts");

		JsonSaveContext saveContext = new JsonSaveContext(false);
		ctx.True(LF_PersistenceComponentSerializer.SerializeStore(persistence, saveContext) == ESerializeResult.OK, "the store serializes");
		string saved = saveContext.SaveToString();
		ctx.True(saved.Contains(LF_SaveSections.OBJECT_PREFIX + LF_BigJobLockSavePayload.NAME), "the save carries the bigjob section");

		// Restart: world time starts again at 50 and the lock is empty.
		LF_BigJobLock.ResetStatics();
		LF_BigJobLock.SetClockForTests(50);
		// RedProof(): the load is skipped, so every read-back assertion fails (`--red`).
		if (!RedProof())
			ReadSection(saved, ctx);

		CheckLoaded(ctx, "after the load");
		ReadSection(saved, ctx);
		ReadSection(saved, ctx);
		CheckLoaded(ctx, "after a second load");

		// A save without the section (an older save): nothing cools down.
		LF_BigJobLock.ResetStatics();
		JsonSaveContext oldContext = new JsonSaveContext(false);
		array<string> omit = {LF_BigJobLockSavePayload.NAME};
		LF_PersistenceComponentSerializer.SerializeStoreOmitting(persistence, oldContext, omit);
		JsonLoadContext oldLoad = new JsonLoadContext(false);
		ctx.True(oldLoad.LoadFromString(oldContext.SaveToString()), "the old save parses");
		LF_PersistenceComponentSerializer.DeserializeStore(persistence, oldLoad);
		ctx.Equal(0, LF_BigJobLock.CooldownLeft(BANK, LF_BigJobLock.Now()), "a pre-section save leaves no cooldown");
		LF_BigJobLock.ResetStatics();
	}

	//------------------------------------------------------------------------------------------------
	protected static void CheckLoaded(LF_TestContext ctx, string when)
	{
		ctx.True(LF_BigJobLock.RunningJob(50).IsEmpty(), "no job holds the lock " + when);
		ctx.Equal(LF_BigJobLock.COOLDOWN_BANK - 1000, LF_BigJobLock.CooldownLeft(BANK, 50), "the bank keeps its cooldown left " + when);
		ctx.Equal(LF_BigJobLock.COOLDOWN_PHARMACY, LF_BigJobLock.CooldownLeft(PHARMACY, 50), "the job running at the save is in its full cooldown " + when);
		ctx.Equal(LF_EBigJobLock.COOLDOWN, LF_BigJobLock.TryBegin(PHARMACY, CREW_A, LF_BigJobLock.COOLDOWN_PHARMACY, 51), "the pharmacy is refused " + when);
	}

	//------------------------------------------------------------------------------------------------
	//! Reads the bigjob section of a whole-store JSON save, as LF_SaveSections.ReadAll does.
	protected static void ReadSection(string saved, LF_TestContext ctx)
	{
		LF_SavePayload section = LF_SaveSections.Get(LF_BigJobLockSavePayload.NAME);
		if (!section)
			return;

		JsonLoadContext context = new JsonLoadContext(false);
		ctx.True(context.LoadFromString(saved), "the save parses");
		ctx.True(context.StartObject(LF_SaveSections.OBJECT_PREFIX + LF_BigJobLockSavePayload.NAME), "the bigjob section opens");
		int savedVersion;
		context.ReadValue(LF_SaveSections.VERSION_KEY, savedVersion);
		ctx.Equal(section.Version(), savedVersion, "the section carries its own version");
		ctx.True(section.ReadSection(context, savedVersion), "the bigjob section reads");
		context.EndObject();
	}

	//------------------------------------------------------------------------------------------------
	//! wt e2e reload boot: the e2e job is still in its cooldown after the restart.
	static void VerifyE2E(LF_TestContext ctx)
	{
		int left = LF_BigJobLock.CooldownLeft(E2E_JOB, LF_BigJobLock.Now());
		ctx.True(left > 0 && left <= LF_BigJobLock.COOLDOWN_BANK, string.Format("the e2e job's cooldown survives the restart (%1 s left)", left));
		ctx.Equal(LF_EBigJobLock.COOLDOWN, LF_BigJobLock.TryBegin(E2E_JOB, CREW_B, LF_BigJobLock.COOLDOWN_BANK, LF_BigJobLock.Now()), "the e2e job is refused after the restart");
		LF_Debug.Info(LF_BigJobLock.FEATURE, string.Format("e2e restart: big job %1 cools down for %2 s more", E2E_JOB, left));
		LF_BigJobLock.ResetStatics();
	}
}
