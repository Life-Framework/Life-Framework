// red-proof: observed 2026-11-10 with `tools\cli test --no-build --only LF_Test_ChainStations`: a wrong expected value failed `work from two workers adds up on the barrel (expected 1, got 2)` and `SUMMARY tier=only passed=0 failed=3` (run with its two siblings); restored, green.

class LF_TestChainRecorder : LF_EventSubscriber
{
	int m_iLoad;
	int m_iReady;
	int m_iCollect;
	int m_iWork;
	float m_fLastHeat;

	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnChainStep step = LF_OnChainStep.Cast(busEvent);
		if (step)
		{
			if (step.m_sKind == "load")
				m_iLoad++;
			else if (step.m_sKind == "ready")
				m_iReady++;
			else if (step.m_sKind == "collect")
				m_iCollect++;

			return true;
		}

		LF_OnChainWork work = LF_OnChainWork.Cast(busEvent);
		if (work)
		{
			m_iWork++;
			m_fLastHeat = work.m_fHeat;
		}

		return true;
	}
}

// tier: WORLD
//! The three placed barrels of the Chains_Site (#417) on DebugWorld, driven through the server
//! methods the held actions call: loading takes the step's inputs, work from two workers adds up on
//! one barrel, a worker who asks faster than a hold can finish is refused, someone out of reach is
//! refused, the site heats, the bus hears the load, ready and collect, and collecting hands over
//! the product. The test empties the barrels, cools the site, and deletes its characters.
class LF_Test_ChainStations : LF_Test
{
	protected static const ResourceName APPLE = "{C9D661E5B0714711}Prefabs/Items/Food/Apple.et";
	protected static const ResourceName JUICE = "{1DBD99E6347DE1AD}Prefabs/Items/Drinks/AppleJuice.et";
	protected static const string SITE = "Chains_Site";

	override string GetName()
	{
		return "stations/world";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_ChainStationComponent barrelA = LF_ChainStations.Find("Chains_Barrel_1");
		LF_ChainStationComponent barrelB = LF_ChainStations.Find("Chains_Barrel_2");
		LF_ChainStationComponent barrelC = LF_ChainStations.Find("Chains_Barrel_3");
		ctx.NotNull(barrelA, "barrel 1 of the site is registered");
		ctx.NotNull(barrelB, "barrel 2 of the site is registered");
		ctx.NotNull(barrelC, "barrel 3 of the site is registered");
		ctx.True(LF_ChainCatalog.IsEnabled("cider"), "the example chain is enabled");
		if (ctx.FailureCount() > 0)
			return;

		IEntity workerOne = LF_TestCharacterFixture.Spawn(ctx);
		IEntity workerTwo = LF_TestCharacterFixture.Spawn(ctx);
		if (!workerOne || !workerTwo)
		{
			LF_TestCharacterFixture.Delete(workerOne);
			LF_TestCharacterFixture.Delete(workerTwo);
			return;
		}

		LF_TestChainRecorder recorder = new LF_TestChainRecorder();
		LF_EventBus.Subscribe(LF_OnChainStep, recorder);
		LF_EventBus.Subscribe(LF_OnChainWork, recorder);

		RunBarrel(ctx, barrelA, workerOne, workerTwo, recorder);

		LF_EventBus.Unsubscribe(LF_OnChainStep, recorder);
		LF_EventBus.Unsubscribe(LF_OnChainWork, recorder);

		barrelA.Import(null);
		LF_ChainSites.ClearHeat(SITE);
		LF_TestCharacterFixture.Delete(workerOne);
		LF_TestCharacterFixture.Delete(workerTwo);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunBarrel(LF_TestContext ctx, LF_ChainStationComponent barrel, IEntity one, IEntity two, LF_TestChainRecorder recorder)
	{
		vector spot = barrel.GetOwner().GetOrigin();
		InventoryStorageManagerComponent bagOne = LF_InventoryUtils.GetResponsibleStorageManager(one);
		InventoryStorageManagerComponent bagTwo = LF_InventoryUtils.GetResponsibleStorageManager(two);
		ctx.NotNull(bagOne, "worker one has an inventory");
		ctx.NotNull(bagTwo, "worker two has an inventory");
		if (!bagOne || !bagTwo)
			return;

		LF_InventoryUtils.AddAmount(bagOne, APPLE, 8);
		int applesBefore = LF_InventoryUtils.GetAmount(bagOne, APPLE);
		ctx.Equal(8, applesBefore, "worker one carries 8 apples");

		// Out of reach: the characters spawn at the spawn point, far from the site.
		ctx.Equal(LF_EChainStation.TOO_FAR, barrel.ServerLoad(one, 1000), "loading from out of reach is refused");
		ctx.Equal(8, LF_InventoryUtils.GetAmount(bagOne, APPLE), "a refused load takes nothing");

		one.SetOrigin(spot + "0.8 0 0");
		two.SetOrigin(spot + "-0.8 0 0");

		ctx.Equal(LF_EChainStation.WRONG_STATE, barrel.ServerWork(one, 1000), "an empty barrel takes no work");
		ctx.Equal(LF_EChainStation.MISSING_INPUTS, barrel.ServerLoad(two, 1000), "a worker with no apples cannot load");

		ctx.Equal(LF_EChainStation.OK, barrel.ServerLoad(one, 1000), "worker one loads the barrel");
		ctx.Equal(4, LF_InventoryUtils.GetAmount(bagOne, APPLE), "the load took the step's 4 apples");
		ctx.Equal(0, barrel.GetPercent(), "a loaded barrel reads 0");
		ctx.Equal(1, recorder.m_iLoad, "the bus heard the load");
		ctx.Equal(LF_EChainStation.WRONG_STATE, barrel.ServerLoad(one, 1000), "a loaded barrel refuses a second batch");

		// Two workers on one barrel: both land; one asking again at once is refused.
		ctx.Equal(LF_EChainStation.OK, barrel.ServerWork(one, 1000), "worker one works");
		ctx.Equal(LF_EChainStation.OK, barrel.ServerWork(two, 1000), "worker two works the same moment");
		ctx.Equal(2, barrel.GetProgress().GetWork(), "work from two workers adds up on the barrel");
		ctx.Equal(LF_EChainStation.TOO_FAST, barrel.ServerWork(one, 1000.4), "a repeat faster than a hold is refused");
		ctx.Equal(2, barrel.GetProgress().GetWork(), "a refused repeat adds nothing");
		ctx.True(LF_ChainSites.GetHeat(SITE, 1000) > 0, "working warms the site");
		ctx.True(recorder.m_fLastHeat > 0, "the work event carries the heat");

		// Alternate until the 8 units are in: 6 more, three rounds of two workers.
		float now = 1000;
		for (int round = 0; round < 3; round++)
		{
			now += 2;
			ctx.Equal(LF_EChainStation.OK, barrel.ServerWork(one, now), "worker one keeps working");
			ctx.Equal(LF_EChainStation.OK, barrel.ServerWork(two, now), "worker two keeps working");
		}

		ctx.Equal(LF_StationProgress.READY, barrel.GetPercent(), "8 units from two workers finish the barrel in 4 rounds");
		ctx.Equal(1, recorder.m_iReady, "the bus heard the barrel become ready");
		ctx.Equal(8, recorder.m_iWork, "the bus heard each accepted unit of work");
		ctx.Equal(LF_EChainStation.WRONG_STATE, barrel.ServerWork(one, now + 2), "a ready barrel takes no more work");

		int juiceBefore = LF_InventoryUtils.GetAmount(bagTwo, JUICE);
		ctx.Equal(LF_EChainStation.OK, barrel.ServerCollect(two, now + 3), "worker two collects the product");
		ctx.Equal(juiceBefore + 1, LF_InventoryUtils.GetAmount(bagTwo, JUICE), "the collector holds the juice");
		ctx.Equal(LF_StationProgress.EMPTY, barrel.GetPercent(), "a collected barrel is empty again");
		ctx.Equal(1, recorder.m_iCollect, "the bus heard the collect");
		ctx.Equal(LF_EChainStation.WRONG_STATE, barrel.ServerCollect(two, now + 4), "an empty barrel gives nothing twice");

		// The site fades once nobody works.
		float hot = LF_ChainSites.GetHeat(SITE, now);
		ctx.True(hot > 0, "the site is warm right after the work");
		ctx.True(LF_ChainSites.GetHeat(SITE, now + 30) < hot, "the site fades while nobody works");
		ctx.True(LF_ChainSites.GetRange(SITE, now) > 0, "a warm site has a smell range");
	}
}
