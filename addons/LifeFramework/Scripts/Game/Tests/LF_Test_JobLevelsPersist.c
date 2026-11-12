// red-proof: observed 2026-11-12 with `tools\cli test --no-build --only LF_Test_JobLevelsPersist`: LF_Progression.ApplyAll
// without LF_JobLevels.SeedFromLegacy failed `- the single level 3 + 40 xp migrates to 340 civilian xp (version 27 load 1)
// (expected 340, got 0)` and `- the career level 3 stays delivery level 3 (version 27 load 1) (expected 3, got 1)`.
// Restored; green. With the single level clamped at 20 again, RunHighLevel failed
// `- a single level 25 with 10 xp migrates to 30010 civilian xp (expected 30010, got 19010)` and
// `- the mirror writes single level 25 back (expected 25, got 20)`. Restored; green.
// tier: PERSISTENCE
//! Per-job XP (#749) rides the real save path: the "joblevels" payload, save version 28. The XP of
//! every job comes back after the store is wiped, a second load gives the same XP, and a version 27
//! save (no "joblevels" rows) migrates: the progression records' single level comes back as
//! CIVILIAN and the delivery career as DELIVERY, at the same levels, and every other job starts at
//! 0. Bad rows are skipped, a row for an unknown job is kept. The test puts back what it found.
class LF_Test_JobLevelsPersist : LF_Test
{
	protected static const string WORKER = "test-joblevels-persist-worker";
	protected static const string MINER = "test-joblevels-persist-miner";

	override string GetName()
	{
		return "persist/job-levels";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		ctx.NotNull(gameMode, "the DebugWorld game mode resolves");
		if (!gameMode)
			return;

		LF_PersistenceComponent persistence = LF_PersistenceComponent.Cast(gameMode.FindComponent(LF_PersistenceComponent));
		ctx.NotNull(persistence, "the game mode carries LF_PersistenceComponent");
		if (!persistence)
			return;

		LF_JobsConfig.Reload();
		LF_TestLegacySnapshot keptOrgs = LF_TestLegacySnapshot.Take();
		array<ref LF_JobXpRow> before = LF_JobLevels.ExportAll();
		LF_Progression.Forget(WORKER);
		LF_Progression.Forget(MINER);

		RunRoundTrip(ctx, persistence);
		RunVersion27(ctx, persistence);
		RunBadRows(ctx);
		RunHighLevel(ctx);

		LF_Progression.Forget(WORKER);
		LF_Progression.Forget(MINER);
		LF_JobLevels.ApplyAll(before);
		keptOrgs.Restore();
	}

	//------------------------------------------------------------------------------------------------
	//! A worker with XP in three jobs (one only "joblevels" holds) and a miner with one.
	protected void Seed(LF_TestContext ctx)
	{
		ctx.Equal(340, LF_JobLevels.AddXp(WORKER, LF_JobLevels.JOB_CIVILIAN, 340, "test"), "the worker earns 340 civilian xp");
		ctx.Equal(1720, LF_JobLevels.AddXp(WORKER, LF_JobLevels.JOB_DELIVERY, 1720, "test"), "the worker earns 1720 delivery xp");
		ctx.Equal(120, LF_JobLevels.AddXp(WORKER, LF_JobLevels.JOB_FARMER, 120, "test"), "the worker earns 120 farmer xp");
		ctx.Equal(650, LF_JobLevels.AddXp(MINER, "MINER", 650, "test"), "the miner earns 650 miner xp");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunRoundTrip(LF_TestContext ctx, LF_PersistenceComponent persistence)
	{
		Seed(ctx);

		JsonSaveContext saveContext = new JsonSaveContext(false);
		ctx.True(LF_PersistenceComponentSerializer.SerializeStore(persistence, saveContext) == ESerializeResult.OK, "the store serializes");
		string saved = saveContext.SaveToString();
		ctx.True(saved.Contains(MINER) && saved.Contains("m_iXp"), "the save carries the job xp rows");

		// Restart: the session has no job xp.
		LF_JobLevels.ApplyAll(null);
		ctx.Equal(1, LF_JobLevels.LevelOf(WORKER, LF_JobLevels.JOB_DELIVERY), "the restarted session starts at level 1");

		for (int pass = 1; pass <= 2; pass++)
		{
			JsonLoadContext loadContext = new JsonLoadContext(false);
			ctx.True(loadContext.LoadFromString(saved), "the save parses");
			int version;
			loadContext.ReadValue("version", version);
			ctx.Equal(28, version, "the save is written at version 28 (joblevels is payload 28)");
			ctx.True(LF_PersistenceComponentSerializer.DeserializeStore(persistence, loadContext), "the save loads");

			string tag = string.Format(" (load %1)", pass);
			ctx.Equal(340, LF_JobLevels.XpOf(WORKER, LF_JobLevels.JOB_CIVILIAN), "the civilian xp is back" + tag);
			ctx.Equal(3, LF_JobLevels.LevelOf(WORKER, LF_JobLevels.JOB_CIVILIAN), "the worker is civilian level 3" + tag);
			ctx.Equal(1720, LF_JobLevels.XpOf(WORKER, LF_JobLevels.JOB_DELIVERY), "the delivery xp is back" + tag);
			ctx.Equal(3, LF_CivilianJob.GetLevel(WORKER), "the worker is delivery level 3" + tag);
			ctx.True(LF_CivilianJob.CanSignUp(WORKER, LF_EJobType.TAXI), "delivery level 3 still opens the taxi" + tag);
			ctx.Equal(120, LF_JobLevels.XpOf(WORKER, LF_JobLevels.JOB_FARMER), "the farmer xp only joblevels holds is back" + tag);
			ctx.Equal(2, LF_JobLevels.LevelOf(WORKER, LF_JobLevels.JOB_FARMER), "the worker is farmer level 2" + tag);
			ctx.Equal(650, LF_JobLevels.XpOf(MINER, "MINER"), "the miner's xp is back" + tag);
			ctx.Equal(4, LF_JobLevels.LevelOf(MINER, "MINER"), "the miner is miner level 4" + tag);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void RunVersion27(LF_TestContext ctx, LF_PersistenceComponent persistence)
	{
		JsonSaveContext oldContext = new JsonSaveContext(false);
		LF_PersistenceComponentSerializer.SerializeStoreAsVersion(persistence, oldContext, 27);
		string oldSave = oldContext.SaveToString();
		ctx.False(oldSave.Contains("m_iXp"), "a version 27 save has no job xp rows");
		ctx.True(oldSave.Contains(WORKER), "the version 27 save carries the worker's progression record");

		for (int pass = 1; pass <= 2; pass++)
		{
			LF_JobLevels.ApplyAll(null);
			JsonLoadContext oldLoad = new JsonLoadContext(false);
			ctx.True(oldLoad.LoadFromString(oldSave), "the version 27 save parses");
			int version;
			oldLoad.ReadValue("version", version);
			ctx.Equal(27, version, "the old save is version 27");
			ctx.True(LF_PersistenceComponentSerializer.DeserializeStore(persistence, oldLoad), "the version 27 save loads");

			string tag = string.Format(" (version 27 load %1)", pass);
			ctx.Equal(340, LF_JobLevels.XpOf(WORKER, LF_JobLevels.JOB_CIVILIAN), "the single level 3 + 40 xp migrates to 340 civilian xp" + tag);
			ctx.Equal(3, LF_JobLevels.LevelOf(WORKER, LF_JobLevels.JOB_CIVILIAN), "the single level 3 stays civilian level 3" + tag);
			ctx.Equal(1720, LF_JobLevels.XpOf(WORKER, LF_JobLevels.JOB_DELIVERY), "the career level 3 + 220 xp migrates to 1720 delivery xp" + tag);
			ctx.Equal(3, LF_CivilianJob.GetLevel(WORKER), "the career level 3 stays delivery level 3" + tag);
			ctx.Equal(0, LF_JobLevels.XpOf(WORKER, LF_JobLevels.JOB_FARMER), "a job the old save never held starts at 0" + tag);
			ctx.Equal(0, LF_JobLevels.XpOf(MINER, "MINER"), "the miner's xp had no pre-#749 field" + tag);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void RunBadRows(LF_TestContext ctx)
	{
		array<ref LF_JobXpRow> rows = {};
		rows.Insert(LF_JobXpRow.Create(WORKER, LF_JobLevels.JOB_CIVILIAN, 340));
		rows.Insert(LF_JobXpRow.Create(WORKER, LF_JobLevels.JOB_CIVILIAN, 100));
		rows.Insert(LF_JobXpRow.Create(WORKER, "GONE_JOB", 70));
		rows.Insert(LF_JobXpRow.Create("", LF_JobLevels.JOB_CIVILIAN, 5));
		rows.Insert(LF_JobXpRow.Create(MINER, "", 5));
		rows.Insert(LF_JobXpRow.Create(MINER, LF_JobLevels.JOB_CIVILIAN, -1));
		rows.Insert(null);
		ctx.Equal(3, LF_JobLevels.ApplyAll(rows), "rows with no character, no job or negative xp are skipped");
		ctx.Equal(340, LF_JobLevels.XpOf(WORKER, LF_JobLevels.JOB_CIVILIAN), "a repeated row keeps the larger xp");
		ctx.Equal(70, LF_JobLevels.XpOf(WORKER, "GONE_JOB"), "a row for a job the config does not list is kept");
		ctx.Equal(1, LF_JobLevels.LevelOf(WORKER, "GONE_JOB"), "and reads as level 1");
		ctx.Equal(0, LF_JobLevels.XpOf(MINER, LF_JobLevels.JOB_CIVILIAN), "a negative row adds nothing");
		ctx.Equal(3, LF_JobLevels.ApplyAll(rows), "a second apply gives the same count");
		ctx.Equal(340, LF_JobLevels.XpOf(WORKER, LF_JobLevels.JOB_CIVILIAN), "a second apply does not add");
	}

	//------------------------------------------------------------------------------------------------
	//! #749 review: the single level had no top, so a level 25 record migrates whole (30010 CIVILIAN
	//! XP, not the 19000 of a level 20 clamp), and a career level 5 record keeps level 5.
	protected void RunHighLevel(LF_TestContext ctx)
	{
		LF_ProgressionRecord veteran = LF_ProgressionRecord.Create(WORKER);
		veteran.m_bHasLevel = true;
		veteran.m_iPlayerLevel = 25;
		veteran.m_fPlayerExperience = 10;
		veteran.m_iCareerLevel = 5;
		veteran.m_fCareerExperience = 30;
		array<ref LF_ProgressionRecord> records = {};
		records.Insert(veteran);

		ctx.Equal(1, LF_JobLevels.SeedFromLegacy(records), "the level 25 veteran is seeded");
		ctx.Equal(30010, LF_JobLevels.XpOf(WORKER, LF_JobLevels.JOB_CIVILIAN), "a single level 25 with 10 xp migrates to 30010 civilian xp");
		ctx.Equal(7530, LF_JobLevels.XpOf(WORKER, LF_JobLevels.JOB_DELIVERY), "a career level 5 with 30 xp migrates to 7530 delivery xp");
		ctx.Equal(5, LF_CivilianJob.GetLevel(WORKER), "the career level 5 stays delivery level 5");

		// The mirror written back is the same level, so a version 27 build would read it unchanged.
		int level;
		float xp;
		LF_JobLevels.XpToLegacyLevel(LF_JobLevels.XpOf(WORKER, LF_JobLevels.JOB_CIVILIAN), level, xp);
		ctx.Equal(25, level, "the mirror writes single level 25 back");
		ctx.EqualFloat(10, xp, 0.01, "with its 10 xp");
	}
}
