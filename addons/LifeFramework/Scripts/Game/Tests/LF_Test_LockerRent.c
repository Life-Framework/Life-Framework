// red-proof: Observed 2026-11-18 with `tools\cli test --no-build --red LF_Test_LockerRent,LF_Test_LockerPersist`: `RED PROOF OK`, the first rent skipped failed `- a second rent at the same site is refused (expected 1, got 0)`. Green with the rent.
// tier: WORLD
//! lockers/rent (#2659): renting at a locker bank takes the rent once and records the locker; a second
//! rent at the same site is refused and takes nothing; short cash is refused with nothing taken; out of
//! reach is refused; the same character may still rent at another site. The real entry (RentNearest)
//! refuses a character standing at no bank.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_LockerRent : LF_Test
{
	protected static const string KEY = "locker-rent-test-a";
	protected static const string SITE = "locker-rent-test-site";
	protected static const string OTHER_SITE = "locker-rent-test-site-2";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "lockers/rent";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		array<ref LF_LockerRentalRow> kept = LF_LockerBank.ExportAll();
		LF_LockerBank.ResetStatics();

		IEntity renter = LF_TestCharacterFixture.Spawn(ctx);
		if (renter)
		{
			Check(ctx, renter);
			LF_TestCharacterFixture.Delete(renter);
		}

		LF_LockerBank.ApplyAll(kept);
	}

	//------------------------------------------------------------------------------------------------
	protected void Check(LF_TestContext ctx, IEntity renter)
	{
		int price = 100;
		int days = 7;
		int now = 1800000000;
		vector at = renter.GetOrigin();
		vector bank = at + "1 0 0";
		float reach = 2.5;

		LF_MoneyUtils.RemoveCash(renter, LF_MoneyUtils.GetCash(renter));
		ctx.Equal(250, LF_MoneyUtils.AddCash(renter, 250), "seed 250 cash");

		LF_ELockerBankResult first = LF_ELockerBankResult.NOT_SERVER;
		if (!RedProof())
			first = LF_LockerBank.RentAt(now, KEY, renter, at, SITE, bank, reach, price, days);
		ctx.Equal(LF_ELockerBankResult.RENTED, first, "the first rent is taken");
		ctx.Equal(150, LF_MoneyUtils.GetCash(renter), "the rent took the price once");
		ctx.True(LF_LockerBank.IsRenting(SITE, KEY), "the character rents a locker at the site");
		ctx.Equal(now + days * LF_LockerRules.SECONDS_PER_DAY, LF_LockerBank.PaidUntil(SITE, KEY), "paid one period ahead");

		ctx.Equal(LF_ELockerBankResult.ALREADY_RENTED, LF_LockerBank.RentAt(now + 10, KEY, renter, at, SITE, bank, reach, price, days), "a second rent at the same site is refused");
		ctx.Equal(150, LF_MoneyUtils.GetCash(renter), "the refused second rent takes nothing");
		ctx.Equal(now + days * LF_LockerRules.SECONDS_PER_DAY, LF_LockerBank.PaidUntil(SITE, KEY), "the refused rent leaves paid-until alone");

		vector far = at + "10 0 0";
		ctx.Equal(LF_ELockerBankResult.OUT_OF_REACH, LF_LockerBank.RentAt(now, KEY, renter, at, OTHER_SITE, far, reach, price, days), "a bank out of reach is refused");
		ctx.Equal(150, LF_MoneyUtils.GetCash(renter), "out of reach takes nothing");
		ctx.False(LF_LockerBank.IsRenting(OTHER_SITE, KEY), "out of reach records no locker");

		ctx.Equal(LF_ELockerBankResult.CANNOT_AFFORD, LF_LockerBank.RentAt(now, KEY, renter, at, OTHER_SITE, bank, reach, 151, days), "short cash is refused");
		ctx.Equal(150, LF_MoneyUtils.GetCash(renter), "short cash takes nothing");
		ctx.False(LF_LockerBank.IsRenting(OTHER_SITE, KEY), "short cash records no locker");

		ctx.Equal(LF_ELockerBankResult.RENTED, LF_LockerBank.RentAt(now, KEY, renter, at, OTHER_SITE, bank, reach, price, days), "the same character rents at another site");
		ctx.Equal(50, LF_MoneyUtils.GetCash(renter), "the second site took its rent");
		ctx.Equal(2, LF_LockerBank.Count(), "two rentals held");

		ctx.Equal(LF_ELockerBankResult.RENTED, LF_LockerBank.RentAt(now, "locker-rent-test-b", renter, at, "locker-rent-test-free", bank, reach, 50, 0), "a period of 0 days rents too");
		ctx.Equal(LF_LockerRules.NEVER, LF_LockerBank.PaidUntil("locker-rent-test-free", "locker-rent-test-b"), "a period of 0 days never lapses");

		ctx.Equal(LF_ELockerBankResult.BAD_CALLER, LF_LockerBank.RentAt(now, "", renter, at, SITE, bank, reach, price, days), "no character key is refused");

		// The real entry finds the bank from the character's own position: none stands at the spawn point.
		LF_MoneyUtils.AddCash(renter, 1000);
		int before = LF_MoneyUtils.GetCash(renter);
		if (!LF_LockerBankComponent.FindNearest(at))
		{
			ctx.Equal(LF_ELockerBankResult.OUT_OF_REACH, LF_LockerBank.RentNearest(renter), "the real entry refuses a character at no bank");
			ctx.Equal(before, LF_MoneyUtils.GetCash(renter), "the refused real entry takes nothing");
		}
	}
}
