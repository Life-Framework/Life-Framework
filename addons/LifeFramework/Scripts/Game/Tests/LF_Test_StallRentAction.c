// red-proof: Observed 2026-11-27 in the batched `tools\cli test --no-build --red` run of the six round-5 action tests: `RED PROOF OK (all 6 listed test(s) fail with the hook on)`; here `the stall is rented`, `the fee left the purse (expected 0, got 50)`. Green with `--only`.
// tier: WORLD
//! market/stall-rent-action (#2925, coverage round 5): the "Rent stall" prompt on a market stall. The perform
//! runs on the server under the acting character's own record key: after the closing hour nothing is taken;
//! one short of the fee takes nothing; with the fee the stall is rented to that key and the fee leaves the
//! purse; a second renter is refused and keeps their cash; one player holds only one stall a day, so a
//! second stall is refused for the holder; a forged perform with no character moves nothing.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_StallRentAction : LF_Test
{
	protected static const ResourceName STALL = "{5E2A7D9C4B1F8306}Prefabs/Props/Stalls/LF_MarketStall.et";
	protected static const string UID_A = "lf-test-stall-rent-a";
	protected static const string UID_B = "lf-test-stall-rent-b";
	//! Day 12 is day 5 of the week (12 % 7), the market day in `MakeConfig`.
	protected static const int MARKET_DAY = 12;
	protected static const int FEE = 50;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "market/stall-rent-action";
	}

	//------------------------------------------------------------------------------------------------
	protected void SetCash(IEntity character, int amount)
	{
		int carried = LF_MoneyUtils.GetCash(character);
		if (carried > 0)
			LF_MoneyUtils.RemoveCash(character, carried);

		if (amount > 0)
			LF_MoneyUtils.AddCash(character, amount);
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_StallConfig.UseForTest(LF_StallConfig.Make(true, 5, 8, 18, FEE, 6, 1, 1000, 5));
		LF_StallClock.SetForTest(MARKET_DAY, 10);

		IEntity first = LF_TestCharacterFixture.Spawn(ctx);
		IEntity second = LF_TestCharacterFixture.Spawn(ctx);
		IEntity propA;
		IEntity propB;
		if (first)
		{
			propA = LF_Utils.SpawnEntityPrefab(STALL, first.GetOrigin() + "1 0 0");
			propB = LF_Utils.SpawnEntityPrefab(STALL, first.GetOrigin() + "1 0 1.5");
		}

		LF_StallComponent stallA;
		LF_StallComponent stallB;
		LF_StallRentAction rentA;
		LF_StallRentAction rentB;
		if (propA)
		{
			stallA = LF_Component<LF_StallComponent>.Find(propA);
			rentA = LF_StallRentAction.Cast(LF_TestActionFinder.Find(propA, LF_StallRentAction));
		}

		if (propB)
		{
			stallB = LF_Component<LF_StallComponent>.Find(propB);
			rentB = LF_StallRentAction.Cast(LF_TestActionFinder.Find(propB, LF_StallRentAction));
		}

		ctx.NotNull(stallA, "the first stall spawns with LF_StallComponent");
		ctx.NotNull(stallB, "the second stall spawns with LF_StallComponent");
		ctx.NotNull(rentA, "the first stall carries the rent action");
		ctx.NotNull(rentB, "the second stall carries the rent action");
		if (!first || !second || !stallA || !stallB || !rentA || !rentB)
		{
			Cleanup(first, second, propA, propB);
			return;
		}

		second.SetOrigin(first.GetOrigin());
		ctx.True(rentA.CanBeShownScript(first), "with the market on the rent prompt is offered");
		ctx.True(rentA.CanBePerformedScript(first), "a free stall in market hours can be rented");
		ctx.False(stallA.IsRented(), "the stall starts free");

		// After closing: nothing is taken.
		LF_Utils.SetUidForTests(first, UID_A);
		SetCash(first, FEE);
		LF_StallClock.SetForTest(MARKET_DAY, 18);
		ctx.False(rentA.CanBePerformedScript(first), "after the closing hour the prompt cannot be performed");
		rentA.PerformAction(propA, first);
		ctx.False(stallA.IsRented(), "a perform after closing rents nothing");
		ctx.Equal(FEE, LF_MoneyUtils.GetCash(first), "a perform after closing takes nothing");
		LF_StallClock.SetForTest(MARKET_DAY, 10);

		// One short of the fee: nothing is taken.
		SetCash(first, FEE - 1);
		rentA.PerformAction(propA, first);
		ctx.False(stallA.IsRented(), "a renter one short rents nothing");
		ctx.Equal(FEE - 1, LF_MoneyUtils.GetCash(first), "a renter one short loses nothing");

		// A forged perform with no character moves nothing.
		rentA.PerformAction(propA, null);
		ctx.False(stallA.IsRented(), "a perform with no character rents nothing");

		// With the fee: rented to that key, the fee leaves the purse.
		SetCash(first, FEE);
		if (!RedProof())
			rentA.PerformAction(propA, first);

		ctx.True(stallA.IsRented(), "the stall is rented");
		ctx.EqualStr(UID_A, stallA.GetHolderKey(), "the stall is rented to the performing character's key");
		ctx.Equal(0, LF_MoneyUtils.GetCash(first), "the fee left the purse");
		ctx.False(rentA.CanBePerformedScript(second), "a rented stall cannot be rented again");

		// Another player is refused and keeps their cash.
		LF_Utils.SetUidForTests(second, UID_B);
		SetCash(second, FEE);
		rentA.PerformAction(propA, second);
		ctx.EqualStr(UID_A, stallA.GetHolderKey(), "a second renter does not take the stall over");
		ctx.Equal(FEE, LF_MoneyUtils.GetCash(second), "the refused second renter keeps their cash");

		// One stall a day: the holder cannot rent the neighbouring stall too.
		LF_Utils.SetUidForTests(first, UID_A);
		SetCash(first, FEE);
		rentB.PerformAction(propB, first);
		ctx.False(stallB.IsRented(), "the holder cannot rent a second stall the same day");
		ctx.Equal(FEE, LF_MoneyUtils.GetCash(first), "the refused holder keeps their cash");

		SetCash(first, 0);
		SetCash(second, 0);
		Cleanup(first, second, propA, propB);
	}

	//------------------------------------------------------------------------------------------------
	protected void Cleanup(IEntity first, IEntity second, IEntity propA, IEntity propB)
	{
		LF_Utils.SetUidForTests(null, "");
		LF_StallTrade.ResetStatics();
		LF_StallClock.ResetStatics();
		LF_StallConfig.Reload();
		LF_TestCharacterFixture.Delete(first);
		LF_TestCharacterFixture.Delete(second);
		LF_TestCharacterFixture.Delete(propA);
		LF_TestCharacterFixture.Delete(propB);
	}
}
