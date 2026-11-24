// red-proof: `tools\cli test --no-build --red LF_Test_LumberProcessing`: RedProof() skips the sawmill
// action, so every "log is used up" and "<n> <good> come out" check fails. Before this card the four
// new goods did not exist, so the first run failed "lumber prefab loads".

//! The sawmill's choices (#3264): every lumber good loads, each action turns one log into the right
//! outputs, a log of the wrong wood is refused, and a log is not a pocket item.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_LumberProcessing : LF_Test
{
	protected static const ResourceName SAWMILL = "{61418A72BA63D059}Prefabs/Processor/Sawmill.et";
	protected static const ResourceName SOFT_LOG = "{32F5A906D8A573BC}Prefabs/Resources/WoodCutting/WoodLog_01_Small.et";
	protected static const ResourceName HARD_LOG = "{FC7D57CC9C0798D7}Prefabs/Resources/WoodCutting/WoodLog_Hardwood.et";
	protected static const ResourceName PLANK = "{45D1B36F99DD2FC3}Prefabs/Resources/WoodCutting/WoodPlank.et";
	protected static const ResourceName KINDLING = "{6697C9B8CEB5234C}Prefabs/Resources/WoodCutting/WoodKindling.et";
	protected static const ResourceName POST = "{7EB79C84E76D667D}Prefabs/Resources/WoodCutting/FencePost.et";
	protected static const ResourceName BOARD = "{D2A8639FA0CA09EF}Prefabs/Resources/WoodCutting/HardwoodBoard.et";
	protected static const ResourceName FIREWOOD = "{F96D821A8E47FBBA}Prefabs/Resources/WoodCutting/FirewoodBundle.et";
	protected static const vector SPOT = "60 0 60";
	protected static const float SWEEP_RADIUS = 4;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "lumber/processing";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		array<ResourceName> prefabs = { SAWMILL, SOFT_LOG, HARD_LOG, PLANK, KINDLING, POST, BOARD, FIREWOOD };
		foreach (ResourceName path : prefabs)
		{
			Resource loaded = Resource.Load(path);
			ctx.True(loaded && loaded.IsValid(), "lumber prefab loads: " + path);
		}
		if (ctx.FailureCount() > 0)
			return;

		float y = GetGame().GetWorld().GetSurfaceY(SPOT[0], SPOT[2]);
		vector at = Vector(SPOT[0], y, SPOT[2]);

		// A log is carried in the hands, never stored: no pocket storage on either wood.
		CheckNotStorage(ctx, SOFT_LOG, at + "0 0 8");
		CheckNotStorage(ctx, HARD_LOG, at + "0 0 10");

		IEntity user = LF_TestCharacterFixture.Spawn(ctx);
		IEntity sawmill = LF_Utils.SpawnEntityPrefab(SAWMILL, at);
		ctx.NotNull(user, "a character spawns");
		ctx.NotNull(sawmill, "the sawmill spawns");
		if (user && sawmill)
			RunChecks(ctx, user, sawmill);

		LF_TestCharacterFixture.Delete(user);
		SCR_EntityHelper.DeleteEntityAndChildren(sawmill);
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckNotStorage(LF_TestContext ctx, ResourceName log, vector at)
	{
		IEntity entity = LF_Utils.SpawnEntityPrefab(log, at);
		ctx.NotNull(entity, "a log spawns: " + log);
		if (!entity)
			return;

		ctx.True(LF_Component<BaseInventoryStorageComponent>.Find(entity) == null, "a log carries no pocket storage: " + log);
		SCR_EntityHelper.DeleteEntityAndChildren(entity);
	}

	//------------------------------------------------------------------------------------------------
	protected LF_ProcessAction FindAction(IEntity sawmill, string machineId)
	{
		ActionsManagerComponent actions = LF_Component<ActionsManagerComponent>.Find(sawmill);
		if (!actions)
			return null;

		array<BaseUserAction> list = {};
		actions.GetActionsList(list);
		foreach (BaseUserAction base : list)
		{
			LF_ProcessAction process = LF_ProcessAction.Cast(base);
			if (process && process.m_sMachineId == machineId)
				return process;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Loose units of a good within the sweep radius of the sawmill (a plank bundle counts its planks, #4875);
	//! deletes them so the next action counts alone.
	protected int Sweep(IEntity sawmill, ResourceName good)
	{
		LF_TestLooseSweep sweep = new LF_TestLooseSweep();
		sweep.Delete(sawmill.GetOrigin(), SWEEP_RADIUS, good);
		return sweep.m_iUnits;
	}

	//------------------------------------------------------------------------------------------------
	//! One choice: one `input` log in, `outA` x `countA` (and `outB` x `countB` when set) out, and the log gone.
	protected void CheckChoice(LF_TestContext ctx, IEntity user, IEntity sawmill, string machineId, ResourceName input, ResourceName outA, int countA, ResourceName outB, int countB)
	{
		LF_ProcessAction process = FindAction(sawmill, machineId);
		ctx.NotNull(process, "the sawmill offers the " + machineId + " action");
		if (!process)
			return;

		ctx.Equal(1, LF_InventoryUtils.AddAmount(user, input, 1, true), machineId + ": one log enters the inventory");
		ctx.True(process.CanBePerformedScript(user), machineId + ": the right log can be sawn");

		if (!RedProof())
			process.PerformAction(sawmill, user);

		ctx.Equal(0, LF_InventoryUtils.GetAmount(user, input), machineId + ": the log is used up");
		ctx.Equal(countA, Sweep(sawmill, outA), machineId + ": " + countA.ToString() + " of the first good come out");
		if (!outB.IsEmpty())
			ctx.Equal(countB, Sweep(sawmill, outB), machineId + ": " + countB.ToString() + " of the second good come out");
	}

	//------------------------------------------------------------------------------------------------
	//! A log of the wrong wood: the action is refused, nothing is spawned, the log stays.
	protected void CheckRefused(LF_TestContext ctx, IEntity user, IEntity sawmill, string machineId, ResourceName wrongLog, ResourceName product)
	{
		LF_ProcessAction process = FindAction(sawmill, machineId);
		if (!process)
			return;

		ctx.Equal(1, LF_InventoryUtils.AddAmount(user, wrongLog, 1, true), machineId + ": the wrong log enters the inventory");
		ctx.True(!process.CanBePerformedScript(user), machineId + ": the wrong wood is refused");
		process.PerformAction(sawmill, user);
		ctx.Equal(1, LF_InventoryUtils.GetAmount(user, wrongLog), machineId + ": the wrong log is not taken");
		ctx.Equal(0, Sweep(sawmill, product), machineId + ": nothing comes out of the wrong wood");
		LF_InventoryUtils.RemoveAmount(user, wrongLog, 1);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunChecks(LF_TestContext ctx, IEntity user, IEntity sawmill)
	{
		CheckChoice(ctx, user, sawmill, "sawmill", SOFT_LOG, PLANK, 4, KINDLING, 1);
		CheckChoice(ctx, user, sawmill, "sawmill-posts", SOFT_LOG, POST, 2, "", 0);
		CheckChoice(ctx, user, sawmill, "sawmill-firewood", SOFT_LOG, FIREWOOD, 4, "", 0);
		CheckChoice(ctx, user, sawmill, "sawmill-boards", HARD_LOG, BOARD, 2, "", 0);
		CheckChoice(ctx, user, sawmill, "sawmill-hardwood-firewood", HARD_LOG, FIREWOOD, 5, "", 0);

		// Wrong wood in either direction.
		CheckRefused(ctx, user, sawmill, "sawmill-boards", SOFT_LOG, BOARD);
		CheckRefused(ctx, user, sawmill, "sawmill-hardwood-firewood", SOFT_LOG, FIREWOOD);
		CheckRefused(ctx, user, sawmill, "sawmill", HARD_LOG, PLANK);
		CheckRefused(ctx, user, sawmill, "sawmill-posts", HARD_LOG, POST);
	}
}
