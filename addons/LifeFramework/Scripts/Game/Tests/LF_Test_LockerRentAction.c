// red-proof: Observed 2026-11-27 in the batched `tools\cli test --no-build --red` run of the six round-5 action tests: `RED PROOF OK (all 6 listed test(s) fail with the hook on)`; here `the rent took the price once (expected 40, got 140)`, `one rental is held (expected 1, got 0)`. Green with `--only`.
// tier: WORLD
//! lockers/rent-action (#2659, coverage round 5): the "Rent a locker" prompt on a locker bank. The perform
//! finds the bank from the acting character's own position and takes the cash under their own record key:
//! out of reach of every bank nothing is taken and nothing is recorded; short cash takes nothing; with the
//! price the cash leaves and the rental is held; a second perform is refused and takes nothing; a forged
//! perform with no character moves nothing.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_LockerRentAction : LF_Test
{
	protected static const ResourceName BANK = "{271A5DA917F4DF47}Prefabs/Props/Lockers/LF_LockerBank.et";
	protected static const string KEY = "locker-rent-action-test";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "lockers/rent-action";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		array<ref LF_LockerRentalRow> kept = LF_LockerBank.ExportAll();
		LF_LockerBank.ResetStatics();

		IEntity renter = LF_TestCharacterFixture.Spawn(ctx);
		IEntity bank;
		if (renter)
		{
			LF_Utils.SetUidForTests(renter, KEY);
			bank = LF_Utils.SpawnEntityPrefab(BANK, renter.GetOrigin() + "1 0 0");
			ctx.NotNull(bank, "the locker bank spawns");
			if (bank)
				Check(ctx, renter, bank);
		}

		LF_Utils.SetUidForTests(null, "");
		if (bank)
			SCR_EntityHelper.DeleteEntityAndChildren(bank);
		if (renter)
			LF_TestCharacterFixture.Delete(renter);

		LF_LockerBank.ApplyAll(kept);
	}

	//------------------------------------------------------------------------------------------------
	protected void Check(LF_TestContext ctx, IEntity renter, IEntity bank)
	{
		LF_LockerRentAction action = LF_LockerRentAction.Cast(LF_TestActionFinder.Find(bank, LF_LockerRentAction));
		LF_LockerBankComponent component = LF_Component<LF_LockerBankComponent>.Find(bank);
		ctx.NotNull(action, "the bank carries the rent action");
		ctx.NotNull(component, "the bank carries LF_LockerBankComponent");
		if (!action || !component)
			return;

		string site = component.GetSiteId();
		int price = LF_LockerConfig.RentPrice();
		ctx.True(price > 0, "renting costs something");
		ctx.True(action.CanBeShownScript(renter), "with lockers on the prompt is offered");
		ctx.True(action.CanBePerformedScript(renter), "a character may perform it");
		ctx.False(action.CanBePerformedScript(null), "no character cannot perform it");

		LF_MoneyUtils.RemoveCash(renter, LF_MoneyUtils.GetCash(renter));
		ctx.Equal(price + 40, LF_MoneyUtils.AddCash(renter, price + 40), "seed the price and 40 over");

		// Out of reach of every bank: nothing is taken and nothing recorded.
		vector home = renter.GetOrigin();
		renter.SetOrigin(home + "40 0 0");
		action.PerformAction(bank, renter);
		ctx.Equal(price + 40, LF_MoneyUtils.GetCash(renter), "out of reach takes nothing");
		ctx.False(LF_LockerBank.IsRenting(site, KEY), "out of reach records no locker");
		renter.SetOrigin(home);

		// A forged perform with no character moves nothing.
		action.PerformAction(bank, null);
		ctx.False(LF_LockerBank.IsRenting(site, KEY), "a perform with no character records no locker");

		// Short cash: refused, nothing taken.
		LF_MoneyUtils.RemoveCash(renter, LF_MoneyUtils.GetCash(renter));
		LF_MoneyUtils.AddCash(renter, price - 1);
		action.PerformAction(bank, renter);
		ctx.Equal(price - 1, LF_MoneyUtils.GetCash(renter), "short cash takes nothing");
		ctx.False(LF_LockerBank.IsRenting(site, KEY), "short cash records no locker");

		// With the price: the rent is taken once and recorded under the character's own key.
		LF_MoneyUtils.AddCash(renter, 41);
		if (!RedProof())
			action.PerformAction(bank, renter);

		ctx.True(LF_LockerBank.IsRenting(site, KEY), "the character rents a locker at the bank's site");
		ctx.Equal(40, LF_MoneyUtils.GetCash(renter), "the rent took the price once");
		ctx.Equal(1, LF_LockerBank.Count(), "one rental is held");

		// A second perform is refused and takes nothing.
		action.PerformAction(bank, renter);
		ctx.Equal(40, LF_MoneyUtils.GetCash(renter), "a second perform takes nothing");
		ctx.Equal(1, LF_LockerBank.Count(), "still one rental");

		LF_MoneyUtils.RemoveCash(renter, LF_MoneyUtils.GetCash(renter));
	}
}
