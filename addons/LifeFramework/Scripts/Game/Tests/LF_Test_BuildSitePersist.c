// red-proof: observed 2026-11-18 `tools\cli test --no-build --red LF_Test_BuildSitePersist` printed `RED PROOF OK` with `the site shows stage 1's shell from the loaded row (load 2) (expected 1, got 0)` (the hook skips the store read); `--only` then passed=4 with LF_Test_BuildSite, LF_Test_PropertyPersist, LF_Test_PlotBuy.
//! property/build-site-persist (#2650): a site part way through stage 2 (every item in, some work
//! done) goes through the real store write and read; the deposits, the work and the stage come back,
//! a second load is the same, and the site's mirror and shell follow the loaded record. Across a
//! restart (`tools\cli wt e2e`): the save boot stores a mid-stage site row; the reload boot finds it.
[LF_TestInfo(LF_TestTier.PERSISTENCE)]
class LF_Test_BuildSitePersist : LF_Test
{
	protected static const string PLOT = "plot-a";
	protected static const string OWNER = "lf-test-build-owner";
	protected static const string E2E_PLOT = "build-site-e2e-plot";
	protected static const string E2E_DEPOSITED = "{D58674633332DC86}Prefabs/Items/Resources/CementBag.et=2";
	protected static const int WORK = 3;

	override string GetName()
	{
		return "property/build-site-persist";
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
		if (LF_SaveStamp.BootSaveVersion() < LF_PersistenceComponentSerializer.PayloadSinceVersion("properties"))
			return;

		LF_PropertyRecord restored = LF_Properties.Get(E2E_PLOT);
		ctx.NotNull(restored, "the build site row is back after the restart");
		if (restored)
		{
			ctx.Equal(1, restored.m_iBuildStage, "the site keeps its stage after the restart");
			ctx.Equal(WORK, restored.m_iWorkDone, "the site keeps its work after the restart");
			ctx.EqualStr(E2E_DEPOSITED, restored.m_sDeposited, "the site keeps its deposits after the restart");
		}

		// The row is this test's own; drop it and keep the other tests' e2e rows.
		array<ref LF_PropertyRecord> others = {};
		foreach (LF_PropertyRecord row : LF_Properties.ExportAll())
		{
			if (row.m_sPropertyId != E2E_PLOT)
				others.Insert(row);
		}

		LF_Properties.ApplyAll(others);
		return;
#endif

		LF_PropertyConfig.Reload();
		LF_TestLegacySnapshot kept = LF_TestLegacySnapshot.Take();
		array<ref LF_PropertyRecord> before = LF_Properties.ExportAll();
		LF_Properties.ApplyAll(null);

		IEntity character;
		IEntity sign;
		LF_BuildSiteComponent site = LF_Test_BuildSite.SetUp(ctx, character, sign);
		string tier = LF_PropertyConfig.PlotTierId(PLOT);
		LF_PropertyStageDef def = LF_BuildStageRules.Stage(tier, 2);
		ctx.NotNull(def, "the example plot's tier has a stage 2");
		if (!site || !def || def.m_iWorkUnits <= WORK)
		{
			Cleanup(character, sign, before, kept);
			return;
		}

		// Mid stage through the site: every item in, WORK units of work.
		if (def.m_aRequirements)
		{
			foreach (LF_PropertyStageReqDef req : def.m_aRequirements)
			{
				if (!LF_Test_BuildSite.DepositAll(ctx, site, character, req.m_Prefab, req.m_iCount))
				{
					Cleanup(character, sign, before, kept);
					return;
				}
			}
		}

		for (int unit = 1; unit <= WORK; unit++)
		{
			ctx.Equal(LF_EBuildSite.OK, site.ServerWork(character, OWNER, unit * 5), string.Format("work unit %1 lands", unit));
		}

		string deposited = LF_Properties.Get(PLOT).m_sDeposited;
		ctx.True(LF_BuildStageRules.ItemsMet(def, deposited), "every item of stage 2 is in before the save");

		JsonSaveContext saveContext = new JsonSaveContext(false);
		LF_PersistenceComponentSerializer.SerializeStore(persistence, saveContext);
		string saved = saveContext.SaveToString();

		for (int pass = 1; pass <= 2; pass++)
		{
			string tag = string.Format(" (load %1)", pass);
			// The live book is emptied before each load, so only the save can bring the row back.
			LF_Properties.ApplyAll(null);
			site.Sync();
			ctx.Equal(0, site.GetShellStage(), "with no row the site shows no shell" + tag);
			JsonLoadContext loadContext = new JsonLoadContext(false);
			ctx.True(loadContext.LoadFromString(saved), "the save parses" + tag);
			if (!RedProof())
				LF_PersistenceComponentSerializer.DeserializeStore(persistence, loadContext);

			LF_PropertyRecord loaded = LF_Properties.Get(PLOT);
			ctx.NotNull(loaded, "the site row is back" + tag);
			if (loaded)
			{
				ctx.Equal(1, loaded.m_iBuildStage, "the site keeps its stage" + tag);
				ctx.Equal(WORK, loaded.m_iWorkDone, "the site keeps its work" + tag);
				ctx.EqualStr(deposited, loaded.m_sDeposited, "the site keeps its deposits" + tag);
			}

			site.Sync();
			ctx.Equal(WORK, site.GetWorkDone(), "the mirror shows the loaded work" + tag);
			ctx.Equal(1, site.GetShellStage(), "the site shows stage 1's shell from the loaded row" + tag);
			ctx.NotNull(site.GetShell(), "stage 1's shell stands again" + tag);
		}

		// The loaded site carries on: the rest of the work finishes the stage.
		for (int rest = WORK + 1; rest <= def.m_iWorkUnits; rest++)
		{
			site.ServerWork(character, OWNER, 100 + rest * 5);
		}

		LF_PropertyRecord finished = LF_Properties.Get(PLOT);
		ctx.True(finished && finished.m_iBuildStage == 2, "the loaded site finishes stage 2");
		Cleanup(character, sign, before, kept);
#ifdef LF_E2E_HOST_STOP
		// The save boot: the shutdown save carries a site row part way through stage 2.
		LF_PropertyRecord e2e = LF_Properties.Create(E2E_PLOT);
		LF_Properties.SetOwner(E2E_PLOT, OWNER);
		LF_Properties.SetStage(E2E_PLOT, 1);
		e2e.m_iWorkDone = WORK;
		e2e.m_sDeposited = E2E_DEPOSITED;
		ctx.NotNull(LF_Properties.Get(E2E_PLOT), "the save boot stored the e2e site");
#endif
	}

	//------------------------------------------------------------------------------------------------
	protected void Cleanup(IEntity character, IEntity sign, array<ref LF_PropertyRecord> before, LF_TestLegacySnapshot kept)
	{
		LF_TestCharacterFixture.Delete(sign);
		LF_TestCharacterFixture.Delete(character);
		LF_Properties.ApplyAll(before);
		if (kept)
			kept.Restore();
	}
}
