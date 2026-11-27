// red-proof: on the unfixed code (the drop a fixed world-axis -1.8 m from the saw) the saw east of the
// operator by 1 m dropped its boards 2.78 m from the operator, past the 1.5 m this test allows; the same
// run is green once the drop follows the operator's side (`LF_ProcessDrop`).

//! The saw drops its boards where the operator can pick them up (11-27 dry run 3: boards landed 2.78 m from
//! the saw against a 3.0 m reach, 0.22 m to spare). The operator stands on any side of the saw, up to the
//! action radius; the boards land within DROP_REACH_M of them every time, and the smelter's bar output
//! (a 1.6 m offset) follows the same rule.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_ProcessDropReach : LF_Test
{
	protected static const ResourceName SAWMILL = "{61418A72BA63D059}Prefabs/Processor/Sawmill.et";
	protected static const ResourceName HARD_LOG = "{FC7D57CC9C0798D7}Prefabs/Resources/WoodCutting/WoodLog_Hardwood.et";
	protected static const ResourceName BOARD = "{D2A8639FA0CA09EF}Prefabs/Resources/WoodCutting/HardwoodBoard.et";
	//! The farthest a drop may land from the operator: a short step.
	protected static const float DROP_REACH_M = 1.5;
	protected static const float SWEEP_RADIUS_M = 8;
	//! The woodlot saw's heading in the Saint Philippe layer.
	protected static const vector SAW_ANGLES = "-11 0 0";

	protected ref array<IEntity> m_aFound = {};

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "processing/drop-reach";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		IEntity user = LF_TestCharacterFixture.Spawn(ctx);
		ctx.NotNull(user, "a character spawns");
		if (!user)
			return;

		// Where the operator stands from the saw: the harness spot (1 m east), each other side, and the far
		// end of the 3 m action radius.
		array<vector> sides = { "1 0 0", "-1 0 0", "0 0 1", "0 0 -1", "2.5 0 0", "0 0 -2.5", "-2.5 0 0" };
		foreach (vector fromSaw : sides)
			CheckSide(ctx, user, fromSaw);

		LF_TestCharacterFixture.Delete(user);
		CheckSmelterPoint(ctx);
	}

	//------------------------------------------------------------------------------------------------
	//! The smelter and furnace bars drop 1.6 m out (`m_vDropOffset 0 0 1.6`): on the operator's side, so a
	//! player 1 m or 2.5 m away on any side stands within reach of them, and the same call with no user keeps
	//! the prefab's own offset.
	protected void CheckSmelterPoint(LF_TestContext ctx)
	{
		vector furnace = "100 5 100";
		vector offset = "0 0 1.6";
		array<vector> operators = { "101 5 100", "99 5 100", "100 5 101", "100 5 97.5", "102.1 5 101.6" };
		foreach (vector operatorAt : operators)
		{
			vector drop = LF_ProcessDrop.Point(furnace, operatorAt, offset);
			float away = vector.Distance(drop, operatorAt);
			ctx.True(away <= DROP_REACH_M, string.Format("a bar drops %1 m from the operator at %2", away, operatorAt.ToString()));
			ctx.True(Math.AbsFloat(vector.Distance(drop, furnace) - 1.6) < 0.01, "a bar drops 1.6 m out from the furnace, operator at " + operatorAt.ToString());
		}

		ctx.True((furnace + offset).ToString() == LF_ProcessDrop.Point(furnace, furnace, offset).ToString(), "an operator on the machine has no side: the prefab offset");
		ctx.True(furnace.ToString() == LF_ProcessDrop.Point(furnace, "101 5 100", "0 0 0").ToString(), "a zero offset drops at the machine origin");
	}

	//------------------------------------------------------------------------------------------------
	//! One operator spot: a saw placed so the operator stands \p fromSaw from it, one hardwood log sawn.
	protected void CheckSide(LF_TestContext ctx, IEntity user, vector fromSaw)
	{
		string label = "operator at " + fromSaw.ToString() + " from the saw";
		vector at = user.GetOrigin() - fromSaw;
		at[1] = GetGame().GetWorld().GetSurfaceY(at[0], at[2]);
		IEntity sawmill = LF_Utils.SpawnEntityPrefab(SAWMILL, at, SAW_ANGLES);
		ctx.NotNull(sawmill, "the sawmill spawns: " + label);
		if (!sawmill)
			return;

		LF_ProcessAction process = FindAction(sawmill, "sawmill-boards");
		ctx.NotNull(process, "the sawmill offers the boards action: " + label);
		if (process)
		{
			ctx.Equal(1, LF_InventoryUtils.AddAmount(user, HARD_LOG, 1, true), "one log enters the inventory: " + label);
			if (!RedProof())
				process.PerformAction(sawmill, user);

			m_aFound.Clear();
			GetGame().GetWorld().QueryEntitiesBySphere(user.GetOrigin(), SWEEP_RADIUS_M, Visit);
			ctx.True(m_aFound.Count() > 0, "boards come out: " + label);
			foreach (IEntity board : m_aFound)
			{
				float distance = vector.Distance(board.GetOrigin(), user.GetOrigin());
				ctx.True(distance <= DROP_REACH_M, string.Format("a board lands %1 m from the operator (at most %2): %3", distance, DROP_REACH_M, label));
			}
		}

		LF_InventoryUtils.RemoveAmount(user, HARD_LOG, 1);
		LF_TestLooseSweep sweep = new LF_TestLooseSweep();
		sweep.Delete(user.GetOrigin(), SWEEP_RADIUS_M, BOARD);
		SCR_EntityHelper.DeleteEntityAndChildren(sawmill);
	}

	//------------------------------------------------------------------------------------------------
	protected bool Visit(IEntity entity)
	{
		if (!entity.GetParent() && LF_Utils.GetPrefabName(entity) == BOARD)
			m_aFound.Insert(entity);

		return true;
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
}
