// red-proof: observed 2026-11-18 `tools\cli test --no-build --red LF_Test_StallRent` printed `RED PROOF OK (all 1 listed test(s) fail with the hook on)` (the hook skips every Rent and Lapse call, so the rents, the names and the lapse all fail); `--only` then passed.
//! market/stall-rent (#2925): a player in reach rents a market stall for the pitch fee in carried cash on
//! market day. The fee is taken once and the stall shows the holder's name; a second stall for the same
//! player, a stall already rented, a short purse, a day that is not market day, a disabled market and a
//! renter out of reach all refuse with no cash moved; the rent lapses at the closing hour and the next
//! market day is free to rent again.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_StallRent : LF_Test
{
	protected static const ResourceName STALL = "{5E2A7D9C4B1F8306}Prefabs/Props/Stalls/LF_MarketStall.et";
	protected static const string UID_A = "lf-test-stall-a";
	protected static const string UID_B = "lf-test-stall-b";
	protected static const string UID_FAR = "lf-test-stall-far";
	//! Day 12 is day 5 of the week (12 % 7), the market day in `MakeConfig`.
	protected static const int MARKET_DAY = 12;
	protected static const int FEE = 50;
	protected static const string NAME = "Ada Stallholder";

	override string GetName()
	{
		return "market/stall-rent";
	}

	//------------------------------------------------------------------------------------------------
	//! Market day 5 of the week, 8 to 18 o'clock, pitch 50.
	protected LF_StallConfig MakeConfig(bool enabled)
	{
		return LF_StallConfig.Make(enabled, 5, 8, 18, FEE, 6, 1, 1000, 5);
	}

	//------------------------------------------------------------------------------------------------
	protected LF_StallComponent StallOf(IEntity prop)
	{
		if (!prop)
			return null;

		return LF_Component<LF_StallComponent>.Find(prop);
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_StallConfig.UseForTest(MakeConfig(true));
		LF_StallClock.SetForTest(MARKET_DAY, 10);
		LF_StallComponent.SetNameForTest(NAME);

		IEntity renter = LF_TestCharacterFixture.Spawn(ctx);
		IEntity other = LF_TestCharacterFixture.Spawn(ctx);
		IEntity far = LF_TestCharacterFixture.Spawn(ctx);
		IEntity propOne;
		IEntity propTwo;
		if (renter)
		{
			propOne = LF_Utils.SpawnEntityPrefab(STALL, renter.GetOrigin() + "1 0 0");
			propTwo = LF_Utils.SpawnEntityPrefab(STALL, renter.GetOrigin() + "0 0 1");
		}

		LF_StallComponent one = StallOf(propOne);
		LF_StallComponent two = StallOf(propTwo);
		ctx.NotNull(one, "the first stall prefab spawns with LF_StallComponent");
		ctx.NotNull(two, "the second stall prefab spawns with LF_StallComponent");
		if (!renter || !other || !far || !one || !two)
		{
			Cleanup(renter, other, far, propOne, propTwo);
			return;
		}

		far.SetOrigin(renter.GetOrigin() + "60 0 0");
		ctx.False(one.IsRented(), "a fresh stall is free");
		ctx.EqualStr("", one.GetHolderName(), "and names nobody");

		// Rent on market day: the fee once, the holder named.
		SetCash(ctx, renter, FEE * 2 + 20);
		ctx.Equal(LF_EStallResult.OK, Rent(one, renter, UID_A), "a renter with the fee rents on market day");
		ctx.Equal(FEE + 20,LF_MoneyUtils.GetCash(renter), "exactly the pitch fee was taken, once");
		ctx.True(one.IsRented(), "the stall is rented");
		ctx.EqualStr(UID_A, one.GetHolderKey(), "to the renter");
		ctx.EqualStr(NAME, one.GetHolderName(), "the stall shows the holder's name");

		// One stall per player: a second stall refuses, cash stays.
		int before = LF_MoneyUtils.GetCash(renter);
		ctx.Equal(LF_EStallResult.ALREADY_RENTED, Rent(two, renter, UID_A), "a second stall for the same player refuses");
		ctx.Equal(before, LF_MoneyUtils.GetCash(renter), "with no cash moved");
		ctx.False(two.IsRented(), "and the second stall stays free");
		ctx.Equal(LF_EStallResult.ALREADY_RENTED, Rent(one, renter, UID_A), "renting the same stall twice refuses");
		ctx.Equal(before, LF_MoneyUtils.GetCash(renter), "with no cash moved");

		// Somebody else's stall, a short purse, out of reach.
		SetCash(ctx, other, FEE * 2);
		ctx.Equal(LF_EStallResult.ALREADY_RENTED, Rent(one, other, UID_B), "a stall already rented refuses another player");
		ctx.Equal(FEE * 2, LF_MoneyUtils.GetCash(other), "with no cash moved");
		ctx.EqualStr(UID_A, one.GetHolderKey(), "and the holder is unchanged");
		SetCash(ctx, other, FEE - 1);
		ctx.Equal(LF_EStallResult.CANT_AFFORD, Rent(two, other, UID_B), "a renter one short of the fee refuses");
		ctx.Equal(FEE - 1, LF_MoneyUtils.GetCash(other), "with no cash moved");
		ctx.False(two.IsRented(), "and the stall stays free");
		SetCash(ctx, far, FEE * 2);
		ctx.Equal(LF_EStallResult.BAD_INPUT, Rent(two, far, UID_FAR), "a renter out of reach refuses");
		ctx.Equal(FEE * 2, LF_MoneyUtils.GetCash(far), "with no cash moved");

		// Not market day, outside the hours, market off: refused, no cash.
		SetCash(ctx, other, FEE * 2);
		LF_StallClock.SetForTest(MARKET_DAY + 1, 10);
		ctx.Equal(LF_EStallResult.CLOSED, Rent(two, other, UID_B), "a rent the day after market day refuses");
		ctx.Equal(FEE * 2, LF_MoneyUtils.GetCash(other), "with no cash moved");
		LF_StallClock.SetForTest(MARKET_DAY, 7);
		ctx.Equal(LF_EStallResult.CLOSED, Rent(two, other, UID_B), "a rent before opening refuses");
		LF_StallClock.SetForTest(MARKET_DAY, 10);
		LF_StallConfig.UseForTest(MakeConfig(false));
		ctx.Equal(LF_EStallResult.OFF, Rent(two, other, UID_B), "a disabled market refuses");
		ctx.Equal(FEE * 2, LF_MoneyUtils.GetCash(other), "with no cash moved");
		ctx.False(two.IsRented(), "and the stall stays free");
		LF_StallConfig.UseForTest(MakeConfig(true));

		// The rent lapses at the window's end, not before.
		LF_StallClock.SetForTest(MARKET_DAY, 17);
		ctx.False(Lapse(one), "the rent holds until the closing hour");
		ctx.True(one.IsRented(), "so the stall is still rented at 17");
		LF_StallClock.SetForTest(MARKET_DAY, 18);
		ctx.True(Lapse(one), "the rent lapses at the closing hour");
		ctx.False(one.IsRented(), "the stall is free again");
		ctx.EqualStr("", one.GetHolderName(), "and shows no name");

		// A new market day: the same player rents again, another stall.
		LF_StallClock.SetForTest(MARKET_DAY + 7, 9);
		before = LF_MoneyUtils.GetCash(renter);
		ctx.Equal(LF_EStallResult.OK, Rent(two, renter, UID_A), "next market day the same player rents again");
		ctx.Equal(before - FEE, LF_MoneyUtils.GetCash(renter), "for one more pitch fee");
		ctx.EqualStr(UID_A, two.GetHolderKey(), "on the other stall");

		LF_StallClock.ResetStatics();
		LF_StallConfig.Reload();
		Cleanup(renter, other, far, propOne, propTwo);
	}

	//------------------------------------------------------------------------------------------------
	//! The rent under test; the red proof skips it.
	protected LF_EStallResult Rent(LF_StallComponent stall, IEntity renter, string uid)
	{
		if (RedProof())
			return -1;

		return stall.Rent(renter, uid);
	}

	//------------------------------------------------------------------------------------------------
	//! The lapse under test; the red proof skips it.
	protected bool Lapse(LF_StallComponent stall)
	{
		if (RedProof())
			return false;

		return stall.Lapse();
	}

	//------------------------------------------------------------------------------------------------
	protected void SetCash(LF_TestContext ctx, IEntity character, int amount)
	{
		int carried = LF_MoneyUtils.GetCash(character);
		if (carried > 0)
			LF_MoneyUtils.RemoveCash(character, carried);

		if (amount > 0)
			LF_MoneyUtils.AddCash(character, amount);

		ctx.Equal(amount, LF_MoneyUtils.GetCash(character), string.Format("the character carries %1 to start", amount));
	}

	//------------------------------------------------------------------------------------------------
	protected void Cleanup(IEntity renter, IEntity other, IEntity far, IEntity propOne, IEntity propTwo)
	{
		LF_StallClock.ResetStatics();
		LF_StallComponent.ResetStatics();
		LF_StallConfig.Reload();
		LF_TestCharacterFixture.Delete(renter);
		LF_TestCharacterFixture.Delete(other);
		LF_TestCharacterFixture.Delete(far);
		LF_TestCharacterFixture.Delete(propOne);
		LF_TestCharacterFixture.Delete(propTwo);
	}
}
