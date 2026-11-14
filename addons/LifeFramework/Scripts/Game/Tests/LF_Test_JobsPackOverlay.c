// red-proof: observed 2026-11-14 with `tools\cli test --no-build --only LF_Test_JobsPackOverlay`: dropping the IsVariableSetDirectly check on m_aLevelThresholds in LF_JobTrackList.PatchFromContainer failed `- pack patch: thresholds the pack leaves out stay core (expected 20, got 1)`, and a no-op RemoveAt failed `- pack remove: the LUMBERJACK track is gone`. Restored; green.

//------------------------------------------------------------------------------------------------
//! A stand-in for the D4 pack source: overlays the jobs core config with a test fixture.
class LF_Test_JobsPackSourceStub : LF_ConfigPackSource
{
	static const ResourceName OVERLAY = "{0CD5E0931EF94553}Configs/Tests/LF_Test_JobsOverlay.conf";

	override void GetOverlays(ResourceName coreConf, notnull array<ResourceName> overlays)
	{
		if (coreConf == LF_JobsConfig.CONFIG_PATH)
			overlays.Insert(OVERLAY);
	}

	override string GetName()
	{
		return "test-jobs-stub";
	}
};

//------------------------------------------------------------------------------------------------
//! #1106: a pack overlay of LF_JobsConfig on the real core Jobs/LF_Jobs.conf. The fixture patches
//! MINER's cap, removes LUMBERJACK, adds FISH and replaces MEDIC (tracks), patches `gather`, removes
//! `fruit_pick` and adds `fish_cast` (xpAwards), and adds a MINER_GOLD gate (licenceGates). The owner
//! then overrides what the pack added.
// tier: WORLD
class LF_Test_JobsPackOverlay : LF_Test
{
	override string GetName()
	{
		return "config/jobs-pack-overlay";
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_JobsConfig LoadCore()
	{
		Resource resource = BaseContainerTools.LoadContainer(LF_JobsConfig.CONFIG_PATH);
		if (!resource || !resource.IsValid())
			return null;
		return LF_JobsConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(resource.GetResource().ToBaseContainer()));
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_ConfigService.Reset();
		LF_ConfigService.LoadOwnerString("{\"schemaVersion\":1}", "test");
		LF_ConfigService.SetPackSource(new LF_Test_JobsPackSourceStub());

		LF_JobsConfig core = LoadCore();
		ctx.NotNull(core, "core LF_Jobs.conf loads");
		if (!core)
		{
			LF_ConfigService.Reset();
			return;
		}

		ctx.NotNull(core.FindTrack("LUMBERJACK"), "core has the LUMBERJACK track");
		ctx.NotNull(core.FindAward("fruit_pick"), "core has the fruit_pick award");
		LF_JobTrackDef coreMiner = core.FindTrack("MINER");
		ctx.NotNull(coreMiner, "core has the MINER track");
		if (!coreMiner)
		{
			LF_ConfigService.Reset();
			return;
		}
		int minerLevel = coreMiner.LevelFor(1000000);

		// Pack over core.
		LF_JobsConfig packed = LoadCore();
		packed.ApplyLayers();
		LF_JobTrackDef miner = packed.FindTrack("MINER");
		ctx.NotNull(miner, "pack patch keeps the MINER track");
		if (miner)
		{
			ctx.Equal(777, miner.m_iDailyXpCap, "pack patch: the cap the pack sets wins over core");
			ctx.Equal(minerLevel, miner.LevelFor(1000000), "pack patch: thresholds the pack leaves out stay core");
		}
		ctx.True(packed.FindTrack("LUMBERJACK") == null, "pack remove: the LUMBERJACK track is gone");
		LF_JobTrackDef fish = packed.FindTrack("FISH");
		ctx.NotNull(fish, "pack add: the FISH track is added");
		if (fish)
			ctx.Equal(3, fish.LevelFor(10), "pack add: the pack's thresholds (5, 10) apply");
		LF_JobTrackDef medic = packed.FindTrack("MEDIC");
		ctx.NotNull(medic, "pack replace keeps the MEDIC track");
		if (medic)
			ctx.Equal(2, medic.LevelFor(1000000), "pack replace: the pack's single threshold replaces the core ladder");

		LF_JobXpAwardDef gather = packed.FindAward("gather");
		ctx.NotNull(gather, "pack patch keeps the gather award");
		if (gather)
		{
			ctx.Equal(9, gather.m_iXp, "pack patch: the xp the pack sets wins over core");
			ctx.EqualStr("FARMER", gather.m_sJob, "pack patch: the job the pack leaves out stays core");
		}
		ctx.True(packed.FindAward("fruit_pick") == null, "pack remove: the fruit_pick award is gone");
		LF_JobXpAwardDef cast = packed.FindAward("fish_cast");
		ctx.NotNull(cast, "pack add: the fish_cast award is added");
		if (cast)
			ctx.EqualStr("FISH", cast.m_sJob, "pack add: the new award feeds the pack's job");

		LF_JobLicenceGateDef gold = packed.FindLicenceGate("MINER_GOLD");
		ctx.NotNull(gold, "pack add: the MINER_GOLD gate is added");
		if (gold)
			ctx.Equal(3, gold.m_iMinLevel, "pack add: the gate carries the pack's level");

		// Owner over pack over core.
		LF_ConfigService.LoadOwnerString("{\"jobs\":{\"tracks\":{\"MINER\":{\"dailyXpCap\":5},\"FISH\":{\"op\":\"remove\"}},\"licenceGates\":{\"MINER_GOLD\":{\"op\":\"patch\",\"minLevel\":6}}}}", "test");
		LF_JobsConfig layered = LoadCore();
		layered.ApplyLayers();
		miner = layered.FindTrack("MINER");
		if (miner)
			ctx.Equal(5, miner.m_iDailyXpCap, "owner patch wins over the pack's cap");
		ctx.True(layered.FindTrack("FISH") == null, "the owner removes a track the pack added");
		gold = layered.FindLicenceGate("MINER_GOLD");
		if (gold)
		{
			ctx.Equal(6, gold.m_iMinLevel, "owner patch wins over the pack's gate level");
			ctx.EqualStr("MINER", gold.m_sJob, "the gate's job stays the pack's where the owner is silent");
		}

		LF_ConfigService.Reset();
	}
};
