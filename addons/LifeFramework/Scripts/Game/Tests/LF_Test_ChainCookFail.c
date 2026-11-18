// red-proof: observed 2026-11-18 with `tools\cli test --no-build --red LF_Test_ChainCookFail`: RED PROOF OK, with ServerBlow skipped "the event carries the capped heat (expected 100, got 0)" and "the worker beside the station is hurt" fail; green with it.

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
