// red-proof: observed 2026-11-18 with `tools\cli test --no-build --red LF_Test_ChainCookFail`: RED PROOF OK, with ServerBlow skipped "the event carries the capped heat (expected 100, got 0)" and "the worker beside the station is hurt" fail; green with it. #2882 (safe zone, wall, system off), same command 2026-11-18: RED PROOF OK, with the hook on "the station blows with a wall in the way (expected 2, got 1)", "the queued blast is gone" and "the ruined batch is dropped" fail; green with it.

class LF_TestChainRuinRecorder : LF_EventSubscriber
{
	int m_iRuined;
	float m_fHeat;
	string m_sStationKey;

	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnChainBatchRuined ruined = LF_OnChainBatchRuined.Cast(busEvent);
		if (ruined)
		{
			m_iRuined++;
			m_fHeat = ruined.m_fHeat;
			m_sStationKey = ruined.m_sStationKey;
		}

		return true;
	}
}

//! A worked batch can fail (#2855). Barrel 1 of the Annex_Chains_Site runs the cider press step with
//! its mistake fields turned up for the test (100 % chance, 2 mistakes): the first work tick is a
//! strike that shows the off-colour cue, the second ruins the batch, and the station refuses work
//! while the blast is queued. When it blows (ServerBlow, which the queued call runs after the delay)
//! the batch is gone with no product, the worker beside it is hurt, the site heat is at its cap and
//! the bus hears LF_OnChainBatchRuined. With the fields back at 0 the same barrel runs as before.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_ChainCookFail : LF_Test
{
	protected static const ResourceName APPLE = "{C9D661E5B0714711}Prefabs/Items/Food/Apple.et";
	protected static const ResourceName JUICE = "{1DBD99E6347DE1AD}Prefabs/Items/Drinks/AppleJuice.et";
	protected static const string SITE = "Annex_Chains_Site";
	protected static const ResourceName WALL = "{419130031F0009C9}Prefabs/Structures/Walls/Brick/BrickWall_01/BrickWall_01_4m.et";

	override string GetName()
	{
		return "chains/cook-fail";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_ChainStationComponent barrel = LF_ChainStations.Find(LF_TestAnnex.CHAINS_BARREL_1);
		ctx.NotNull(barrel, "barrel 1 of the site is registered");
		LF_ChainDef chain = LF_ChainCatalog.FindChain("cider");
		ctx.NotNull(chain, "the cider chain is loaded");
		if (!barrel || !chain)
			return;

		LF_ChainStepDef step = chain.FindStep(barrel.GetStepId());
		ctx.NotNull(step, "the barrel's step exists");
		if (!step)
			return;

		ctx.Equal(0, step.m_iMistakeChancePct, "the cider press has no mistake chance in config");
		ctx.Equal(0, step.m_iMaxMistakes, "and no mistake limit");

		IEntity worker = LF_TestCharacterFixture.Spawn(ctx);
		if (!worker)
			return;

		LF_TestChainRuinRecorder recorder = new LF_TestChainRuinRecorder();
		LF_EventBus.Subscribe(LF_OnChainBatchRuined, recorder);
		barrel.Import(null);
		LF_ChainSites.ClearHeat(SITE);

		int chance = step.m_iMistakeChancePct;
		int max = step.m_iMaxMistakes;
		int delay = step.m_iRuinDelayMs;
		step.m_iMistakeChancePct = 100;
		step.m_iMaxMistakes = 2;
		step.m_iRuinDelayMs = 3000;

		Ruin(ctx, barrel, worker, recorder);
		LoadCancelsBlast(ctx, barrel, worker, recorder);
		SafeZoneSpares(ctx, barrel, worker, recorder);
		WallShields(ctx, barrel, worker, recorder);
		SystemOffCancels(ctx, barrel, worker, recorder);

		step.m_iMistakeChancePct = chance;
		step.m_iMaxMistakes = max;
		step.m_iRuinDelayMs = delay;

		Clean(ctx, barrel, worker);

		LF_EventBus.Unsubscribe(LF_OnChainBatchRuined, recorder);
		barrel.Import(null);
		LF_ChainSites.ClearHeat(SITE);
		LF_TestCharacterFixture.Delete(worker);
	}

	//------------------------------------------------------------------------------------------------
	protected void Ruin(LF_TestContext ctx, LF_ChainStationComponent barrel, IEntity worker, LF_TestChainRuinRecorder recorder)
	{
		InventoryStorageManagerComponent bag = LF_InventoryUtils.GetResponsibleStorageManager(worker);
		ctx.NotNull(bag, "the worker has an inventory");
		if (!bag)
			return;

		worker.SetOrigin(barrel.GetOwner().GetOrigin() + "0.8 0 0");
		worker.Update();
		LF_InventoryUtils.AddAmount(bag, APPLE, 4);
		int juiceBefore = LF_InventoryUtils.GetAmount(bag, JUICE);

		ctx.Equal(LF_EChainStation.OK, barrel.ServerLoad(worker, 2000), "the worker loads the barrel");
		ctx.Equal(0, LF_InventoryUtils.GetAmount(bag, APPLE), "the load took the apples");

		ctx.Equal(LF_EChainStation.OK, barrel.ServerWork(worker, 2000), "the first work tick lands");
		ctx.Equal(1, barrel.GetProgress().GetWork(), "a strike still counts the work");
		ctx.Equal(2, barrel.GetCue(), "one strike from ruin shows the thick off-colour smoke");
		ctx.False(barrel.IsRuinPending(), "one strike does not ruin the batch");

		ctx.Equal(LF_EChainStation.OK, barrel.ServerWork(worker, 2002), "the second work tick lands");
		ctx.True(barrel.IsRuinPending(), "the second strike ruins the batch and queues the blast");
		ctx.Equal(1, barrel.GetProgress().GetWork(), "the ruining tick adds no work");
		ctx.Equal(LF_EChainStation.WRONG_STATE, barrel.ServerWork(worker, 2004), "a ruined batch takes no more work");
		ctx.Equal(0, recorder.m_iRuined, "nothing blows before the delay");

		SCR_CharacterDamageManagerComponent damage = SCR_CharacterDamageManagerComponent.Cast(worker.FindComponent(SCR_CharacterDamageManagerComponent));
		HitZone health;
		if (damage)
			health = damage.GetDefaultHitZone();
		ctx.NotNull(health, "the worker has a health hit zone");
		float before = 0;
		if (health)
			before = health.GetHealth();

		// The queued call runs ServerBlow after the delay; the test runs it now.
		if (!RedProof())
			barrel.ServerBlow();

		ctx.False(barrel.IsRuinPending(), "the blast is no longer queued");
		ctx.Equal(LF_StationProgress.EMPTY, barrel.GetPercent(), "the blast empties the station");
		ctx.Equal(0, barrel.GetCue(), "the cue clears with the batch");
		ctx.Equal(juiceBefore, LF_InventoryUtils.GetAmount(bag, JUICE), "a ruined batch gives no product");
		ctx.Equal(0, LF_InventoryUtils.GetAmount(bag, APPLE), "and its inputs do not come back");
		ctx.EqualFloat(100, LF_ChainSites.GetHeat(SITE, 2005), 0.01, "the site heat jumps to the cap");
		ctx.Equal(1, recorder.m_iRuined, "the bus heard the batch ruined");
		ctx.EqualFloat(100, recorder.m_fHeat, 0.01, "the event carries the capped heat");
		ctx.EqualStr(LF_TestAnnex.CHAINS_BARREL_1, recorder.m_sStationKey, "the event names the station");
		if (health)
			ctx.True(health.GetHealth() < before, "the worker beside the station is hurt");

		barrel.ServerBlow();
		ctx.Equal(1, recorder.m_iRuined, "a second blow call does nothing");
	}

	//------------------------------------------------------------------------------------------------
	//! A save applied while a blast is queued (#2881 review) cancels it: the restored batch is kept
	//! and a late blow call does nothing.
	protected void LoadCancelsBlast(LF_TestContext ctx, LF_ChainStationComponent barrel, IEntity worker, LF_TestChainRuinRecorder recorder)
	{
		InventoryStorageManagerComponent bag = LF_InventoryUtils.GetResponsibleStorageManager(worker);
		if (!bag)
			return;

		int ruinedBefore = recorder.m_iRuined;
		LF_InventoryUtils.AddAmount(bag, APPLE, 4);
		ctx.Equal(LF_EChainStation.OK, barrel.ServerLoad(worker, 2500), "a second batch loads");
		LF_ChainStationRow saved = barrel.Export();
		barrel.ServerWork(worker, 2502);
		barrel.ServerWork(worker, 2504);
		ctx.True(barrel.IsRuinPending(), "the second batch is ruined and its blast queued");

		barrel.Import(saved);
		ctx.False(barrel.IsRuinPending(), "applying the save cancels the queued blast");
		barrel.ServerBlow();
		ctx.Equal(ruinedBefore, recorder.m_iRuined, "no blast after the save is applied");
		ctx.True(barrel.GetProgress().IsLoaded(), "the restored batch is still in the station");
		barrel.Import(null);
	}

	//------------------------------------------------------------------------------------------------
	//! Loads the barrel and works it into a ruin with the blast queued; false when it did not get there.
	protected bool QueueRuin(LF_TestContext ctx, LF_ChainStationComponent barrel, IEntity worker, float now)
	{
		InventoryStorageManagerComponent bag = LF_InventoryUtils.GetResponsibleStorageManager(worker);
		if (!bag)
			return false;

		barrel.Import(null);
		LF_ChainSites.ClearHeat(SITE);
		LF_InventoryUtils.AddAmount(bag, APPLE, 4);
		ctx.Equal(LF_EChainStation.OK, barrel.ServerLoad(worker, now), "a batch loads");
		barrel.ServerWork(worker, now + 2);
		barrel.ServerWork(worker, now + 4);
		ctx.True(barrel.IsRuinPending(), "the batch is ruined and its blast queued");
		return barrel.IsRuinPending();
	}

	//------------------------------------------------------------------------------------------------
	protected float Health(IEntity worker)
	{
		SCR_CharacterDamageManagerComponent damage = SCR_CharacterDamageManagerComponent.Cast(worker.FindComponent(SCR_CharacterDamageManagerComponent));
		if (!damage || !damage.GetDefaultHitZone())
			return 0;

		return damage.GetDefaultHitZone().GetHealth();
	}

	//------------------------------------------------------------------------------------------------
	//! The blast has no instigator, so a safe zone blocks it for a worker standing inside (#2882); it
	//! still blows, empties the station and heats the site.
	protected void SafeZoneSpares(LF_TestContext ctx, LF_ChainStationComponent barrel, IEntity worker, LF_TestChainRuinRecorder recorder)
	{
		worker.SetOrigin(barrel.GetOwner().GetOrigin() + "0.8 0 0");
		worker.Update();
		if (!QueueRuin(ctx, barrel, worker, 2600))
			return;

		int ruinedBefore = recorder.m_iRuined;
		float before = Health(worker);
		ctx.True(LF_Zones.RegisterCircle("test-chain-safe", LF_ZoneRule.SAFE, barrel.GetOwner().GetOrigin(), 20), "a safe zone covers the station");
		if (!RedProof())
			barrel.ServerBlow();

		LF_Zones.Unregister("test-chain-safe");
		ctx.Equal(ruinedBefore + 1, recorder.m_iRuined, "the station still blows in a safe zone");
		ctx.EqualFloat(before, Health(worker), 0.01, "a worker inside a safe zone takes no blast damage");
		barrel.Import(null);
	}

	//------------------------------------------------------------------------------------------------
	//! A wall between the still and the worker shields them (#2882). Two walls cross at the midpoint so
	//! the line is blocked whichever way the prefab's long side runs.
	protected void WallShields(LF_TestContext ctx, LF_ChainStationComponent barrel, IEntity worker, LF_TestChainRuinRecorder recorder)
	{
		vector stillAt = barrel.GetOwner().GetOrigin();
		worker.SetOrigin(stillAt + "2.5 0 0");
		worker.Update();
		IEntity wallA = LF_Utils.SpawnEntityPrefab(WALL, stillAt + "1.25 0 0");
		IEntity wallB = LF_Utils.SpawnEntityPrefab(WALL, stillAt + "1.25 0 0");
		ctx.NotNull(wallA, "the first wall spawns");
		ctx.NotNull(wallB, "the second wall spawns");
		if (wallA && wallB)
		{
			wallB.SetAngles("0 90 0");
			wallA.Update();
			wallB.Update();
			if (QueueRuin(ctx, barrel, worker, 2700))
			{
				int ruinedBefore = recorder.m_iRuined;
				float before = Health(worker);
				if (!RedProof())
					barrel.ServerBlow();

				ctx.Equal(ruinedBefore + 1, recorder.m_iRuined, "the station blows with a wall in the way");
				ctx.EqualFloat(before, Health(worker), 0.01, "a worker behind a wall takes no blast damage");
			}
		}

		if (wallA)
			LF_TestCharacterFixture.Delete(wallA);

		if (wallB)
			LF_TestCharacterFixture.Delete(wallB);

		barrel.Import(null);
	}

	//------------------------------------------------------------------------------------------------
	//! A blast queued before an admin switches the Chains system off does not fire (#2882): nothing is
	//! hurt, no heat, no bus event; the ruined batch is gone.
	protected void SystemOffCancels(LF_TestContext ctx, LF_ChainStationComponent barrel, IEntity worker, LF_TestChainRuinRecorder recorder)
	{
		worker.SetOrigin(barrel.GetOwner().GetOrigin() + "0.8 0 0");
		worker.Update();
		if (!QueueRuin(ctx, barrel, worker, 2800))
			return;

		int ruinedBefore = recorder.m_iRuined;
		float before = Health(worker);
		if (!LF_Systems.SwitchOff(LF_ChainStationComponent.SYSTEM, "chains/cook-fail test"))
		{
			ctx.Fail("the Chains system switches off");
			return;
		}

		if (!RedProof())
			barrel.ServerBlow();

		LF_Systems.ClearRuntimeOff(LF_ChainStationComponent.SYSTEM);
		ctx.False(barrel.IsRuinPending(), "the queued blast is gone");
		ctx.Equal(ruinedBefore, recorder.m_iRuined, "no blast and no bus event while the system is off");
		ctx.EqualFloat(before, Health(worker), 0.01, "nobody is hurt");
		ctx.EqualFloat(0, LF_ChainSites.GetHeat(SITE, 2805), 0.01, "no heat is added");
		ctx.Equal(LF_StationProgress.EMPTY, barrel.GetPercent(), "the ruined batch is dropped");
		barrel.Import(null);
	}

	//------------------------------------------------------------------------------------------------
	//! With the mistake fields at 0 a full batch runs and collects exactly as before.
	protected void Clean(LF_TestContext ctx, LF_ChainStationComponent barrel, IEntity worker)
	{
		InventoryStorageManagerComponent bag = LF_InventoryUtils.GetResponsibleStorageManager(worker);
		if (!bag)
			return;

		LF_ChainSites.ClearHeat(SITE);
		LF_InventoryUtils.AddAmount(bag, APPLE, 4);
		int juiceBefore = LF_InventoryUtils.GetAmount(bag, JUICE);
		ctx.Equal(LF_EChainStation.OK, barrel.ServerLoad(worker, 3000), "a clean batch loads");

		int needed = barrel.GetProgress().GetNeeded();
		float now = 3000;
		for (int i = 0; i < needed; i++)
		{
			now += 2;
			barrel.ServerWork(worker, now);
		}

		ctx.Equal(LF_StationProgress.READY, barrel.GetPercent(), "every tick of a step with no mistake fields counts");
		ctx.Equal(0, barrel.GetCue(), "and shows no cue");
		ctx.Equal(LF_EChainStation.OK, barrel.ServerCollect(worker, now + 1), "the clean batch collects");
		ctx.Equal(juiceBefore + 1, LF_InventoryUtils.GetAmount(bag, JUICE), "the product arrives");
	}
}
