// red-proof: 2026-11-23 `tools/cli test --no-build --red LF_Test_FeedFromVehicle`: RED-OK (28 assertions); RedProof() skips the feed, so
// "the sawmill took one batch-cap worth of logs from the truck (expected 0, got 3)" and "every post is made once (expected 6, got 0)" fail.
//! Feed from the truck (#4191): a truck parked away from the sawmill is not offered and feeds nothing;
//! backed up to it, the sawmill's action reads "Feed from the truck", takes the logs from the truck
//! (never more than the truck holds, never more than the machine's batch cap), and every post it makes
//! is counted once: in the truck, with the worker or on the ground, nothing duplicated or lost.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_FeedFromVehicle : LF_Test
{
	protected static const ResourceName SAWMILL = "{61418A72BA63D059}Prefabs/Processor/Sawmill.et";
	protected static const ResourceName SOFT_LOG = "{32F5A906D8A573BC}Prefabs/Resources/WoodCutting/WoodLog_01_Small.et";
	protected static const ResourceName POST = "{7EB79C84E76D667D}Prefabs/Resources/WoodCutting/FencePost.et";
	protected static const string MACHINE = "sawmill-posts";
	protected static const vector SPOT = "112 0 80";
	protected static const int LOGS = 3;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "processing/feed-from-vehicle";
	}

	//------------------------------------------------------------------------------------------------
	protected LF_ProcessAction FindAction(IEntity sawmill)
	{
		ActionsManagerComponent actions = LF_Component<ActionsManagerComponent>.Find(sawmill);
		if (!actions)
			return null;

		array<BaseUserAction> list = {};
		actions.GetActionsList(list);
		foreach (BaseUserAction base : list)
		{
			LF_ProcessAction process = LF_ProcessAction.Cast(base);
			if (process && process.m_sMachineId == MACHINE)
				return process;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Spawns the truck facing +Z with its rear face \p gap metres in front of the sawmill.
	protected IEntity SpawnTruck(IEntity sawmill, float gap)
	{
		Resource resource = Resource.Load(LF_VehicleCatalog.M923);
		if (!resource || !resource.IsValid())
			return null;

		// Measure the box once from a probe, then place the real truck so its rear sits at the gap.
		vector at = sawmill.GetOrigin() + Vector(0, 0, gap + 8);
		IEntity truck = LF_Utils.SpawnEntityPrefab(LF_VehicleCatalog.M923, at);
		if (!truck)
			return null;

		vector mins, maxs;
		truck.GetBounds(mins, maxs);
		vector transform[4];
		truck.GetWorldTransform(transform);
		transform[3] = sawmill.GetOrigin() + Vector(0, 0.2, gap - mins[2]);
		truck.SetWorldTransform(transform);
		truck.Update();
		return truck;
	}

	//------------------------------------------------------------------------------------------------
	//! Posts anywhere: in the truck, with the worker, or loose near the sawmill and the truck (deleted).
	protected int PostsEverywhere(IEntity truck, IEntity worker, IEntity sawmill)
	{
		int count = LF_InventoryUtils.GetAmount(worker, POST);
		if (truck)
			count += LF_InventoryUtils.GetAmount(LF_FeedFromVehicle.StorageOf(truck), POST);

		LF_TestLooseSweep sweep = new LF_TestLooseSweep();
		count += sweep.Delete(sawmill.GetOrigin(), 14, POST);
		return count;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_ResourceMachineDef row = LF_ResourceChainCatalog.FindMachine(MACHINE);
		ctx.NotNull(row, "the sawmill posts machine row exists");
		if (!row)
			return;

		float y = GetGame().GetWorld().GetSurfaceY(SPOT[0], SPOT[2]);
		IEntity worker = LF_TestCharacterFixture.Spawn(ctx);
		IEntity sawmill = LF_Utils.SpawnEntityPrefab(SAWMILL, Vector(SPOT[0], y, SPOT[2]));
		ctx.NotNull(worker, "the worker spawns");
		ctx.NotNull(sawmill, "the sawmill spawns");
		if (worker && sawmill)
			RunChecks(ctx, row, worker, sawmill);

		LF_TestCharacterFixture.Delete(worker);
		SCR_EntityHelper.DeleteEntityAndChildren(sawmill);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunChecks(LF_TestContext ctx, LF_ResourceMachineDef row, IEntity worker, IEntity sawmill)
	{
		LF_ProcessAction process = FindAction(sawmill);
		ctx.NotNull(process, "the sawmill offers the posts action");
		if (!process)
			return;

		int perBatchIn = row.m_aInputs[0].m_iInputAmount;
		int perBatchOut = row.m_aOutputs[0].m_iOutputAmount;
		int batches = LF_Utils.MinInt(LOGS / perBatchIn, LF_ProcessAction.MaxBatches(row));
		ctx.True(batches >= 1, "fixture: the truck's logs cover a batch");
		ctx.Equal(0, LF_InventoryUtils.GetAmount(worker, SOFT_LOG), "fixture: the worker carries no log");

		// Out of range: the truck is parked 8 m off. No feed is offered and nothing leaves the truck.
		IEntity far = SpawnTruck(sawmill, 8);
		ctx.NotNull(far, "the far truck spawns");
		if (!far)
			return;

		ctx.Equal(LOGS, LF_InventoryUtils.AddAmount(LF_FeedFromVehicle.StorageOf(far), SOFT_LOG, LOGS, false), "fixture: the far truck is loaded");
		ctx.True(LF_FeedFromVehicle.DistanceToBox(far, sawmill.GetOrigin()) > LF_FeedFromVehicle.REACH, "the far truck is out of reach");
		ctx.True(process.FeedVehicleFor(sawmill, worker) == null, "out of range the feed is absent");
		process.CanBeShownScript(worker);
		string name;
		process.GetActionNameScript(name);
		ctx.True(name != "#LF-Process_FeedFromTruck", "out of range the action does not read Feed from the truck");
		process.PerformAction(sawmill, worker);
		ctx.Equal(LOGS, LF_InventoryUtils.GetAmount(LF_FeedFromVehicle.StorageOf(far), SOFT_LOG), "out of range the truck keeps its logs");
		ctx.Equal(0, PostsEverywhere(far, worker, sawmill), "out of range no post is made");
		SCR_EntityHelper.DeleteEntityAndChildren(far);

		// Backed up: the rear 1 m from the sawmill.
		IEntity truck = SpawnTruck(sawmill, 1);
		ctx.NotNull(truck, "the backed-up truck spawns");
		if (!truck)
			return;

		InventoryStorageManagerComponent cargo = LF_FeedFromVehicle.StorageOf(truck);
		ctx.Equal(LOGS, LF_InventoryUtils.AddAmount(cargo, SOFT_LOG, LOGS, false), "fixture: the truck is loaded");
		ctx.True(process.FeedVehicleFor(sawmill, worker) == truck, "a backed-up truck feeds the sawmill");
		ctx.True(process.CanBeShownScript(worker), "the action is shown");
		name = "";
		process.GetActionNameScript(name);
		ctx.EqualStr("#LF-Process_FeedFromTruck", name, "the action reads Feed from the truck");
		ctx.True(process.CanBePerformedScript(worker), "the truck's logs let the action run");

		// RedProof(): the feed is skipped, so the truck keeps its logs and no post appears.
		if (!RedProof())
			process.PerformAction(sawmill, worker);

		int left = LOGS - batches * perBatchIn;
		ctx.Equal(left, LF_InventoryUtils.GetAmount(cargo, SOFT_LOG), "the sawmill took one batch-cap worth of logs from the truck");
		ctx.Equal(0, LF_InventoryUtils.GetAmount(worker, SOFT_LOG), "no log moved to the worker");
		ctx.Equal(batches * perBatchOut, PostsEverywhere(truck, worker, sawmill), "every post is made once: nothing duplicated or lost");

		// Never more than it holds: feed until the truck is short, then it is refused and nothing changes.
		int guard = 0;
		while (LF_InventoryUtils.GetAmount(cargo, SOFT_LOG) >= perBatchIn && guard < LOGS)
		{
			process.PerformAction(sawmill, worker);
			guard++;
		}

		int finalLeft = LF_InventoryUtils.GetAmount(cargo, SOFT_LOG);
		ctx.True(finalLeft < perBatchIn, "the truck is fed down below one batch");
		ctx.False(process.CanBePerformedScript(worker), "a short truck cannot be fed");
		process.PerformAction(sawmill, worker);
		ctx.Equal(finalLeft, LF_InventoryUtils.GetAmount(cargo, SOFT_LOG), "a short truck gives nothing");
		ctx.True(LF_InventoryUtils.GetAmount(cargo, SOFT_LOG) >= 0, "the truck never goes below empty");
		PostsEverywhere(truck, worker, sawmill);

		SCR_EntityHelper.DeleteEntityAndChildren(truck);
	}
}
