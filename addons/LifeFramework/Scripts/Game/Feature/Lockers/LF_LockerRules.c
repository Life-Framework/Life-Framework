//------------------------------------------------------------------------------------------------
//! Why a rent was or was not allowed (#2658).
enum LF_ELockerRentResult
{
	OK,
	ALREADY_RENTED,	//!< the character already has a locker at this site
	CANNOT_AFFORD	//!< the price is not in hand
}

//------------------------------------------------------------------------------------------------
//! Where a rent stands at a moment.
enum LF_ELockerState
{
	ACTIVE,	//!< paid up: before the paid-until moment, or a period of 0 days
	GRACE,	//!< past paid-until, the contents are still in the locker
	LAPSED	//!< the grace is over: move the contents to lost-and-found
}

//------------------------------------------------------------------------------------------------
//! Why a reclaim from lost-and-found was or was not allowed.
enum LF_ELockerReclaimResult
{
	OK,
	CANNOT_AFFORD
}

//------------------------------------------------------------------------------------------------
//! The locker rules (#2658, specs #753, #776), pure: no entity, no clock, no config read. The caller
//! passes the moment ("now", UTC seconds) and the config values, so the rules test without a world.
//!
//! One locker per character per site: a site is a bank of lockers keyed by a stable site id, each
//! renter gets their own contents. Rent runs to a paid-until moment; after it comes a grace period;
//! at the lapse moment (paid-until plus grace) the contents move to the site's lost-and-found, where
//! reclaiming costs the configured fee. A rent period of 0 days never lapses.
class LF_LockerRules
{
	static const int SECONDS_PER_DAY = 86400;	// lf-fixed: a calendar fact, not a tunable

	//! The paid-until and lapse moment of a locker that never lapses (period 0).
	static const int NEVER = 0;

	//------------------------------------------------------------------------------------------------
	//! May a character rent a locker at a site? \param alreadyRentsHere The character holds a locker at this site.
	//! \param cash What the character holds. \param price The rent for one period.
	static LF_ELockerRentResult CheckRent(bool alreadyRentsHere, int cash, int price)
	{
		if (alreadyRentsHere)
			return LF_ELockerRentResult.ALREADY_RENTED;

		if (cash < price)
			return LF_ELockerRentResult.CANNOT_AFFORD;

		return LF_ELockerRentResult.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! The paid-until moment after paying one period. A renewal before the end adds the period to the
	//! unexpired time; one after the end counts from now. \return NEVER when the period is 0 days.
	static int PaidUntil(int currentPaidUntil, int now, int rentPeriodDays)
	{
		if (rentPeriodDays <= 0)
			return NEVER;

		int from = now;
		if (currentPaidUntil > now)
			from = currentPaidUntil;

		return from + rentPeriodDays * SECONDS_PER_DAY;
	}

	//------------------------------------------------------------------------------------------------
	//! The moment the contents move to lost-and-found: paid-until plus the grace. \return NEVER when the period is 0 days.
	static int LapseMoment(int paidUntil, int rentPeriodDays, int gracePeriodDays)
	{
		if (rentPeriodDays <= 0)
			return NEVER;

		int grace = gracePeriodDays;
		if (grace < 0)
			grace = 0;

		return paidUntil + grace * SECONDS_PER_DAY;
	}

	//------------------------------------------------------------------------------------------------
	//! Where a rent stands at `now`. A period of 0 days is always ACTIVE.
	static LF_ELockerState StateAt(int now, int paidUntil, int rentPeriodDays, int gracePeriodDays)
	{
		if (rentPeriodDays <= 0)
			return LF_ELockerState.ACTIVE;

		if (now < paidUntil)
			return LF_ELockerState.ACTIVE;

		if (now < LapseMoment(paidUntil, rentPeriodDays, gracePeriodDays))
			return LF_ELockerState.GRACE;

		return LF_ELockerState.LAPSED;
	}

	//------------------------------------------------------------------------------------------------
	//! True when the locker's contents should move to the site's lost-and-found at `now`.
	static bool MoveToLostAndFound(int now, int paidUntil, int rentPeriodDays, int gracePeriodDays)
	{
		return StateAt(now, paidUntil, rentPeriodDays, gracePeriodDays) == LF_ELockerState.LAPSED;
	}

	//------------------------------------------------------------------------------------------------
	//! May a character take their contents back from lost-and-found? \param fee The configured reclaim fee.
	static LF_ELockerReclaimResult CheckReclaim(int cash, int fee)
	{
		if (cash < fee)
			return LF_ELockerReclaimResult.CANNOT_AFFORD;

		return LF_ELockerReclaimResult.OK;
	}
}
