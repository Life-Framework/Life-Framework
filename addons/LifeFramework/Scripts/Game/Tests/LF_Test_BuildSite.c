// red-proof: observed 2026-11-18 `tools\cli test --no-build --red LF_Test_BuildSite,LF_Test_BuildSitePersist` printed `RED-OK property/build-site (fails with the hook on, 53 assertions)` (the hook skips the first cement deposit); `--only` then passed.
// tier: WORLD
//! property/build-site (#2650): the owner of plot-a stands at its sign with cement bags and gravel.
//! A stranger is refused; two cement bags deposit (they leave the inventory, the record counts 2);
//! work before the items are in is refused; the rest deposits, the work units go in (a second unit
//! inside the hold interval is refused) and the stage advances, the record starts the next stage
//! empty and the shell swaps from stage 1's prefab to stage 2's. The test puts back the book it found.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_BuildSite : LF_Test
{
	protected static const ResourceName SIGN = "{DA9D4FEB9BD67EA5}Prefabs/Props/Property/LF_PlotSign.et";
	protected static const string PLOT = "plot-a";
	protected static const string OWNER = "lf-test-build-owner";
	protected static const string STRANGER = "lf-test-build-stranger";

	override string GetName()
	{
		return "property/build-site";
	}

	//------------------------------------------------------------------------------------------------
	//! Spawns the owner and a plot-a sign beside them with a build site; null site on failure.
	static LF_BuildSiteComponent SetUp(LF_TestContext ctx, out IEntity character, out IEntity sign)
	{
		character = LF_TestCharacterFixture.Spawn(ctx);
		if (!character)
			return null;

		sign = LF_Utils.SpawnEntityPrefab(SIGN, character.GetOrigin() + "1 0 0");
		ctx.NotNull(sign, "the plot sign prefab spawns");
		if (!sign)
			return null;

		LF_PlotComponent plot = LF_Component<LF_PlotComponent>.Find(sign);
		LF_BuildSiteComponent site = LF_Component<LF_BuildSiteComponent>.Find(sign);
		ctx.NotNull(plot, "the plot sign carries LF_PlotComponent");
		ctx.NotNull(site, "the plot sign carries LF_BuildSiteComponent");
		if (!plot || !site)
			return null;

		plot.SetPlotKey(PLOT);
		LF_Properties.Create(PLOT);
		LF_Properties.SetOwner(PLOT, OWNER);
		LF_Properties.SetStage(PLOT, 1);
		return site;
	}

	//------------------------------------------------------------------------------------------------
	//! Hands the character `count` of the item a few at a time and deposits each batch.
	//! \return False when an item did not reach the inventory or a deposit was refused.
	static bool DepositAll(LF_TestContext ctx, LF_BuildSiteComponent site, IEntity character, ResourceName prefab, int count)
	{
		while (count > 0)
		{
			int batch = LF_Utils.MinInt(count, 2);
			int added = LF_InventoryUtils.AddAmount(character, prefab, batch);
			ctx.Equal(batch, added, "the owner picked up " + prefab);
			if (added != batch)
				return false;

			if (site.ServerDeposit(character, OWNER) != LF_EBuildSite.OK)
			{
				ctx.Fail("deposit of " + prefab + " refused");
				return false;
			}

			count -= batch;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The requirement of `def` whose prefab path holds `hint`, else the first.
	static LF_PropertyStageReqDef Requirement(LF_PropertyStageDef def, string hint)
	{
		if (!def || !def.m_aRequirements || def.m_aRequirements.IsEmpty())
			return null;

		foreach (LF_PropertyStageReqDef req : def.m_aRequirements)
		{
			if (req && req.m_Prefab.Contains(hint))
				return req;
		}

		return def.m_aRequirements[0];
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_PropertyConfig.Reload();
		array<ref LF_PropertyRecord> before = LF_Properties.ExportAll();
		LF_Properties.ApplyAll(null);

		IEntity character;
		IEntity sign;
		LF_BuildSiteComponent site = SetUp(ctx, character, sign);
		string tier = LF_PropertyConfig.PlotTierId(PLOT);
		LF_PropertyStageDef def = LF_BuildStageRules.Stage(tier, 2);
		ctx.NotNull(def, "the example plot's tier has a stage 2");
		LF_PropertyStageReqDef cement = Requirement(def, "Cement");
		ctx.NotNull(cement, "stage 2 needs an item");
		if (!site || !def || !cement || cement.m_iCount < 2)
		{
			Cleanup(character, sign, before);
			return;
		}

		ctx.Equal(LF_EBuildSite.REFUSED_NOT_ALLOWED, site.ServerDeposit(character, STRANGER), "a stranger may not deposit");

		// Two cement bags go in: consumed from the inventory, counted in the record.
		ctx.Equal(2, LF_InventoryUtils.AddAmount(character, cement.m_Prefab, 2), "the owner carries two cement bags");
		if (!RedProof())
			ctx.Equal(LF_EBuildSite.OK, site.ServerDeposit(character, OWNER), "the owner deposits the cement");

		LF_PropertyRecord record = LF_Properties.Get(PLOT);
		ctx.Equal(0, LF_InventoryUtils.GetAmount(character, cement.m_Prefab), "the cement left the inventory");
		ctx.Equal(2, LF_BuildStageRules.CountOf(record.m_sDeposited, cement.m_Prefab), "the record counts two cement bags");
		ctx.Equal(1, record.m_iBuildStage, "the stage has not advanced yet");
		ctx.Equal(1, site.GetShellStage(), "stage 1's shell is shown");
		IEntity oldShell = site.GetShell();
		ctx.NotNull(oldShell, "stage 1's shell stands");
		EntityID oldShellId = EntityID.INVALID;
		if (oldShell)
		{
			oldShellId = oldShell.GetID();
			ctx.EqualStr(LF_PropertyConfig.ShellPrefab(tier, 1), LF_Utils.GetPrefabName(oldShell), "the shell is stage 1's prefab");
		}

		ctx.Equal(LF_EBuildSite.REFUSED_ITEMS_FIRST, site.ServerWork(character, OWNER, 1), "work waits for the items");
		ctx.Equal(LF_EBuildSite.REFUSED_NO_ITEMS, site.ServerDeposit(character, OWNER), "a deposit with nothing carried is refused");

		// The rest of the items.
		foreach (LF_PropertyStageReqDef req : def.m_aRequirements)
		{
			int missing = LF_BuildStageRules.Missing(def, record.m_sDeposited, req.m_Prefab);
			if (missing > 0 && !DepositAll(ctx, site, character, req.m_Prefab, missing))
			{
				Cleanup(character, sign, before);
				return;
			}
		}

		ctx.True(LF_BuildStageRules.ItemsMet(def, record.m_sDeposited), "every item of stage 2 is in");
		ctx.True(site.GetMissing().IsEmpty(), "the mirror shows nothing missing");

		// The work: one unit per hold; a second unit inside the interval is refused.
		float now = 100;
		for (int unit = 1; unit <= def.m_iWorkUnits; unit++)
		{
			now += 5;
			ctx.Equal(LF_EBuildSite.OK, site.ServerWork(character, OWNER, now), string.Format("work unit %1 lands", unit));
			if (unit == 1)
			{
				ctx.Equal(LF_EBuildSite.REFUSED_TOO_FAST, site.ServerWork(character, OWNER, now), "a second unit inside the hold is refused");
				ctx.Equal(1, record.m_iWorkDone, "one unit of work is counted");
				ctx.Equal(1, site.GetWorkDone(), "the mirror shows one unit of work");
			}
		}

		ctx.Equal(2, record.m_iBuildStage, "the stage advanced");
		ctx.Equal(0, record.m_iWorkDone, "the next stage starts with no work");
		ctx.EqualStr("", record.m_sDeposited, "the next stage starts with no deposits");
		ctx.Equal(2, site.GetStage(), "the mirror shows stage 2");
		ctx.Equal(2, site.GetShellStage(), "stage 2's shell is shown");
		IEntity newShell = site.GetShell();
		ctx.NotNull(newShell, "stage 2's shell stands");
		if (newShell)
			ctx.EqualStr(LF_PropertyConfig.ShellPrefab(tier, 2), LF_Utils.GetPrefabName(newShell), "the shell swapped to stage 2's prefab");

		ctx.True(oldShellId != EntityID.INVALID && GetGame().GetWorld().FindEntityByID(oldShellId) == null, "stage 1's shell is gone");
		Cleanup(character, sign, before);
	}

	//------------------------------------------------------------------------------------------------
	protected void Cleanup(IEntity character, IEntity sign, array<ref LF_PropertyRecord> before)
	{
		LF_TestCharacterFixture.Delete(sign);
		LF_TestCharacterFixture.Delete(character);
		LF_Properties.ApplyAll(before);
	}
}
