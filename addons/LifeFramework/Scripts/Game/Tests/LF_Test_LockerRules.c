// red-proof: Observed 2026-11-17 on main without the rules: `tools\cli test --no-build --only LF_Test_LockerRules` failed (scripts do not compile: LF_LockerRules, LF_LockerConfig and their enums do not exist). The same run passes with them.
// tier: LOGIC
//! #2658 (slice L1 of #1038, specs #753, #776): the locker rules. One locker per character per site;
//! rent paid until a moment, then a grace period, then the contents move to the site's lost-and-found,
//! where reclaiming costs the configured fee. `rentPeriodDays 0` never lapses. Times are UTC seconds.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_LockerRules : LF_Test
{
	override string GetName()
	{
		return "lockers/rules";
	}

	override void Run(LF_TestContext ctx)
	{
		int DAY = 86400;
		int now = 1000000;

		// Rent: once per character per site; the price must be in hand.
		ctx.Equal(LF_ELockerRentResult.OK, LF_LockerRules.CheckRent(false, 100, 100), "a character with no locker here and the price in hand may rent");
		ctx.Equal(LF_ELockerRentResult.ALREADY_RENTED, LF_LockerRules.CheckRent(true, 1000, 100), "a second rent at the same site is refused");
		ctx.Equal(LF_ELockerRentResult.CANNOT_AFFORD, LF_LockerRules.CheckRent(false, 99, 100), "a renter short of the price is refused");
		ctx.Equal(LF_ELockerRentResult.OK, LF_LockerRules.CheckRent(false, 0, 0), "a free rent needs no cash");

		// Paid-until: a fresh rent runs a period from now; a renewal stacks on the unexpired time.
		int paid = LF_LockerRules.PaidUntil(0, now, 7);
		ctx.Equal(now + 7 * DAY, paid, "a fresh rent is paid one period from now");
		ctx.Equal(paid + 7 * DAY, LF_LockerRules.PaidUntil(paid, now, 7), "a renewal before the end adds a period to the paid-until moment");
		ctx.Equal(paid + 7 * DAY, LF_LockerRules.PaidUntil(now - DAY, paid, 7), "a renewal after the end counts from now");

		// Lapse moment: paid-until plus grace.
		int lapse = LF_LockerRules.LapseMoment(paid, 7, 2);
		ctx.Equal(paid + 2 * DAY, lapse, "the lapse moment is paid-until plus the grace period");
		ctx.Equal(paid, LF_LockerRules.LapseMoment(paid, 7, 0), "no grace lapses at paid-until");

		// State over time.
		ctx.Equal(LF_ELockerState.ACTIVE, LF_LockerRules.StateAt(paid - 1, paid, 7, 2), "before paid-until the rent is active");
		ctx.Equal(LF_ELockerState.GRACE, LF_LockerRules.StateAt(paid, paid, 7, 2), "at paid-until the grace period starts");
		ctx.Equal(LF_ELockerState.GRACE, LF_LockerRules.StateAt(lapse - 1, paid, 7, 2), "one second before the lapse moment is still grace");
		ctx.Equal(LF_ELockerState.LAPSED, LF_LockerRules.StateAt(lapse, paid, 7, 2), "at the lapse moment the rent has lapsed");

		// A lapsed locker reads "move to lost-and-found"; the others do not.
		ctx.True(LF_LockerRules.MoveToLostAndFound(lapse, paid, 7, 2), "a lapsed locker reads move to lost-and-found");
		ctx.False(LF_LockerRules.MoveToLostAndFound(lapse - 1, paid, 7, 2), "a locker in grace stays where it is");
		ctx.False(LF_LockerRules.MoveToLostAndFound(paid - 1, paid, 7, 2), "an active locker stays where it is");

		// rentPeriodDays 0 never lapses, however late.
		ctx.Equal(LF_ELockerState.ACTIVE, LF_LockerRules.StateAt(now + 100000 * DAY, paid, 0, 2), "rentPeriodDays 0 is always active");
		ctx.False(LF_LockerRules.MoveToLostAndFound(now + 100000 * DAY, paid, 0, 2), "rentPeriodDays 0 never moves to lost-and-found");
		ctx.Equal(LF_LockerRules.NEVER, LF_LockerRules.LapseMoment(paid, 0, 2), "rentPeriodDays 0 has no lapse moment");
		ctx.Equal(LF_LockerRules.NEVER, LF_LockerRules.PaidUntil(0, now, 0), "rentPeriodDays 0 has no paid-until moment");

		// Reclaim: the fee must be in hand.
		ctx.Equal(LF_ELockerReclaimResult.OK, LF_LockerRules.CheckReclaim(50, 50), "a reclaimer with the fee in hand may reclaim");
		ctx.Equal(LF_ELockerReclaimResult.CANNOT_AFFORD, LF_LockerRules.CheckReclaim(49, 50), "a reclaimer short of the fee is refused");
		ctx.Equal(LF_ELockerReclaimResult.OK, LF_LockerRules.CheckReclaim(0, 0), "a free reclaim needs no cash");

		// Config: the decided defaults, and the reclaim fee comes from it.
		ctx.Equal(100, LF_LockerConfig.RentPrice(), "rentPrice defaults to 100");
		ctx.Equal(7, LF_LockerConfig.RentPeriodDays(), "rentPeriodDays defaults to 7");
		ctx.Equal(2, LF_LockerConfig.GracePeriodDays(), "gracePeriodDays defaults to 2");
		ctx.Equal(50, LF_LockerConfig.ReclaimFee(), "reclaimFee defaults to 50");
		ctx.EqualFloat(60, LF_LockerConfig.WeightCapacity(), 0.001, "weightCapacity defaults to 60 kg");

		LF_ConfigService.Reset();
		LF_ConfigService.LoadOwnerString("{\"schemaVersion\":1,\"lockers\":{\"reclaimFee\":80,\"rentPeriodDays\":0,\"rentPrice\":-5}}", "test:lockers");
		LF_LockerConfig.Reload();
		ctx.Equal(80, LF_LockerConfig.ReclaimFee(), "the owner's reclaimFee is read");
		ctx.Equal(0, LF_LockerConfig.RentPeriodDays(), "the owner's rentPeriodDays 0 is read");
		ctx.Equal(100, LF_LockerConfig.RentPrice(), "a negative rentPrice keeps the shipped 100");
		ctx.True(LF_ConfigService.IsKnownTopLevelKey(LF_LockerConfig.SECTION), "lockers is a known top-level key");

		LF_ConfigService.Reset();
		LF_LockerConfig.Reload();
	}
}
