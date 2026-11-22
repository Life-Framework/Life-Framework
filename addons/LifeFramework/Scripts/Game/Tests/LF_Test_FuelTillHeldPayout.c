// red-proof: 2026-11-22 `tools\cli test --no-build --red LF_Test_FuelTillHeldPayout` printed `RED-OK crime/fuel-till-held-payout (fails with the hook on, 14 assertions)`: RedProof() skips PayHeld.

//! rc 0.2.0 report: in the island cop-flow proof player B robbed the fuel till through the attendant and became wanted,
//! but his cash stayed at $100. The wanted level is reported when the hold begins; the till pays only when the 30 s hold
//! is kept (the proof put the weapon away at once, which ends it unpaid), and on a server whose dirty-money switch is on
//! (the Everon pack) it pays marked notes, never clean cash. This locks the payout itself: a kept hold on the attended
//! till pays its full amount, as cash with the switch off and as marked notes with it on.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_FuelTillHeldPayout : LF_Test
{
	static const ResourceName TILL = "{9D3A51E060EFE982}Prefabs/Props/Commercial/LF_FuelTill_Attended.et";
	static const int TILL_PAYOUT = 150;

	override string GetName()
	{
		return "crime/fuel-till-held-payout";
	}

	//------------------------------------------------------------------------------------------------
	protected LF_RobAction FindRob(IEntity entity)
	{
		if (!entity)
			return null;

		BaseActionsManagerComponent manager = BaseActionsManagerComponent.Cast(entity.FindComponent(BaseActionsManagerComponent));
		if (!manager)
			return null;

		array<BaseUserAction> actions = {};
		manager.GetActionsList(actions);
		foreach (BaseUserAction action : actions)
		{
			LF_RobAction rob = LF_RobAction.Cast(action);
			if (rob)
				return rob;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		IEntity user = LF_TestCharacterFixture.Spawn(ctx);
		IEntity tillEntity = LF_Utils.SpawnEntityPrefab(TILL, "78 1 78");
		LF_RobAction till = FindRob(tillEntity);
		ctx.NotNull(user, "a robber spawns");
		ctx.True(till != null, "the attended fuel till carries LF_RobAction");
		if (!user || !till)
		{
			LF_TestCharacterFixture.Delete(user);
			SCR_EntityHelper.DeleteEntityAndChildren(tillEntity);
			return;
		}

		ctx.True(till.IsDialogueOnly(), "the attended till is robbed through the attendant's dialogue");
		ctx.True(till.GetHoldSeconds() > 0, "and only after a hold, so the cash comes when the hold is kept");

		// Switch off: a kept hold pays clean cash.
		LF_LaunderConfig.SetCrimePaysMarkedForTests(false);
		int cashBefore = LF_MoneyUtils.GetCash(user);
		int markedBefore = LF_MarkedCash.Count(user);
		int paid;
		if (!RedProof())
			paid = till.PayHeld(user);

		ctx.Equal(TILL_PAYOUT, paid, "a kept hold on the fuel till pays its full amount");
		ctx.Equal(cashBefore + TILL_PAYOUT, LF_MoneyUtils.GetCash(user), "as clean cash when crime does not pay marked notes");
		ctx.Equal(markedBefore, LF_MarkedCash.Count(user), "and no marked notes");

		// Switch on (the Everon pack): the same hold pays marked notes, the carried cash does not move.
		LF_LaunderConfig.SetCrimePaysMarkedForTests(true);
		cashBefore = LF_MoneyUtils.GetCash(user);
		markedBefore = LF_MarkedCash.Count(user);
		paid = 0;
		if (!RedProof())
			paid = till.PayHeld(user);

		ctx.Equal(TILL_PAYOUT, paid, "with the dirty-money switch on the hold still pays its full amount");
		ctx.Equal(markedBefore + TILL_PAYOUT, LF_MarkedCash.Count(user), "as marked notes");
		ctx.Equal(cashBefore, LF_MoneyUtils.GetCash(user), "and the carried cash stays where it was");

		LF_LaunderConfig.ClearCrimePaysMarkedForTests();
		SCR_EntityHelper.DeleteEntityAndChildren(tillEntity);
		LF_TestCharacterFixture.Delete(user);
	}
}
