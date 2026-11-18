//------------------------------------------------------------------------------------------------
//! Market price curve (#2844, framework #762): pure rules, no world and no statics. A good has a base,
//! a floor and a ceiling (whole cash in the config), a drop per unit sold (a percent of the base) and a
//! recovery per hour (a percent of the gap to the base). The caller holds the current price and the unix
//! second it last moved; these functions say what a sale pays and where the price stands after a gap.
//!
//! The price state is kept in hundredths of a cash unit ("fine", PRICE_SCALE per cash), so a 2 percent
//! drop on a 5 cash good takes 0.10 off per unit instead of a whole 1 (#3028): the price moves smoothly
//! and a sack of apples does not hit the floor after two. Every function here that takes or returns a
//! price (current, drop, floor, ceiling, base) speaks fine units; ToFine/ToCash convert, and a payout
//! (BulkTotal) is whole cash, rounded half up from the exact fine sum.
//! Everything is int maths through LF_Utils.MaxInt/MinInt/ClampInt (never Math.Max/Min/Clamp, #2787);
//! products that could pass 2^31 split into whole cash and hundredths and stop at INT_MAX.
class LF_PriceCurve
{
	static const int SECONDS_PER_HOUR = 3600;	// lf-fixed: seconds in one real hour

	//! Fine units per cash unit.
	static const int PRICE_SCALE = 100;	// lf-fixed: hundredths, the finest a price steps

	//! Largest whole-cash price a good may use (config params cap it too): times PRICE_SCALE it fits an int.
	static const int MAX_CASH = 10000000;	// lf-fixed: 10 million cash, keeps a fine price inside an int

	//! Most units one bulk sale prices: keeps units * (units - 1) / 2 inside an int. A larger stack is two calls.
	static const int MAX_BULK_UNITS = 60000;	// lf-fixed: int overflow guard for the closed-form sum

	static const int INT_MAX = 2147483647;	// lf-fixed: largest int, the stop for the saturating sums

	//------------------------------------------------------------------------------------------------
	//! \return Whole cash as fine units, held to 0..MAX_CASH.
	static int ToFine(int cash)
	{
		return LF_Utils.ClampInt(cash, 0, MAX_CASH) * PRICE_SCALE;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Fine units as whole cash, rounded to the nearest (half up): what the board shows and a
	//! single unit pays.
	static int ToCash(int fine)
	{
		int clamped = LF_Utils.ClampInt(fine, 0, MAX_CASH * PRICE_SCALE);
		return (clamped + PRICE_SCALE / 2) / PRICE_SCALE;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The fine units one sold unit takes off the price: dropPerUnitPct of the base, exactly
	//! (base cash times the percent is already hundredths of a cash unit). 0 for no drop.
	static int DropPerUnit(int baseCash, int dropPerUnitPct)
	{
		if (dropPerUnitPct <= 0 || baseCash <= 0)
			return 0;

		int pct = LF_Utils.ClampInt(dropPerUnitPct, 0, 100);
		return LF_Utils.ClampInt(baseCash, 0, MAX_CASH) * pct;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The price held inside floor..ceiling. A floor above the ceiling returns the floor.
	static int ClampPrice(int price, int floorPrice, int ceilingPrice)
	{
		return LF_Utils.ClampInt(price, floorPrice, ceilingPrice);
	}

	//------------------------------------------------------------------------------------------------
	//! \return What the next unit fetches at `current`: the price itself, held inside floor..ceiling.
	static int NextPrice(int current, int floorPrice, int ceilingPrice)
	{
		return ClampPrice(current, floorPrice, ceilingPrice);
	}

	//------------------------------------------------------------------------------------------------
	//! \return The price after `units` sold from `current`: each unit takes `drop` off, never below the floor.
	static int PriceAfterSale(int current, int units, int drop, int floorPrice, int ceilingPrice)
	{
		int price = ClampPrice(current, floorPrice, ceilingPrice);
		if (units <= 0 || drop <= 0)
			return price;

		// Units that still drop the price: the rest land on the floor. Bounded, so moved * drop stays under 2^31.
		int dropping = UnitsAboveFloor(price, drop, floorPrice);
		int moved = LF_Utils.MinInt(units, dropping);
		return LF_Utils.MaxInt(price - moved * drop, floorPrice);
	}

	//------------------------------------------------------------------------------------------------
	//! \return The whole cash `units` sold in one go pay from `current`: unit k fetches the price after k
	//! earlier units, so the exact total equals the sum of `units` single sales in fine units; it is
	//! rounded to whole cash once. 0 for no units. Stops at INT_MAX rather than overflowing.
	static int BulkTotal(int current, int units, int drop, int floorPrice, int ceilingPrice)
	{
		if (units <= 0)
			return 0;

		units = LF_Utils.MinInt(units, MAX_BULK_UNITS);
		int price = ClampPrice(current, floorPrice, ceilingPrice);
		int cash;
		int rem;
		if (drop <= 0)
		{
			MulFine(price, units, cash, rem);
			return RoundedCash(cash, rem);
		}

		int dropping = LF_Utils.MinInt(UnitsAboveFloor(price, drop, floorPrice), units);
		// Units 0..dropping-1 fetch price - i * drop (an arithmetic run), the rest fetch the floor.
		int runCash;
		int runRem;
		MulFine(price, dropping, runCash, runRem);
		if (runCash >= INT_MAX)
			return INT_MAX;

		int lossCash;
		int lossRem;
		MulFine(drop, Triangle(dropping), lossCash, lossRem);
		int floorCash;
		int floorRem;
		MulFine(floorPrice, units - dropping, floorCash, floorRem);

		cash = runCash - lossCash;
		rem = runRem - lossRem + floorRem;
		cash = SatAdd(cash, floorCash);
		return RoundedCash(cash, rem);
	}

	//------------------------------------------------------------------------------------------------
	//! How many sold units fetch more than the floor: the run before the price meets it.
	protected static int UnitsAboveFloor(int price, int drop, int floorPrice)
	{
		if (price <= floorPrice || drop <= 0)
			return 0;

		return (price - floorPrice + drop - 1) / drop;
	}

	//------------------------------------------------------------------------------------------------
	//! \return 0 + 1 + ... + (n - 1), the number of drops the run of n units takes in total. n is at most
	//! MAX_BULK_UNITS, so the result fits an int.
	protected static int Triangle(int n)
	{
		if (n <= 1)
			return 0;

		if (n % 2 == 0)
			return (n / 2) * (n - 1);

		return n * ((n - 1) / 2);
	}

	//------------------------------------------------------------------------------------------------
	//! fine * n as whole cash and leftover hundredths (0..99), exact for any fine >= 0 and n >= 0; the
	//! cash part stops at INT_MAX.
	protected static void MulFine(int fine, int n, out int cash, out int rem)
	{
		cash = 0;
		rem = 0;
		if (fine <= 0 || n <= 0)
			return;

		int whole = fine / PRICE_SCALE;
		int part = fine % PRICE_SCALE;
		int nHigh = n / PRICE_SCALE;
		int nLow = n % PRICE_SCALE;
		cash = SatAdd(SatMul(whole, n), part * nHigh);
		int low = part * nLow;
		cash = SatAdd(cash, low / PRICE_SCALE);
		rem = low % PRICE_SCALE;
	}

	//------------------------------------------------------------------------------------------------
	//! \return cash plus rem hundredths as whole cash, rounded half up; never below 0.
	protected static int RoundedCash(int cash, int rem)
	{
		while (rem < 0)
		{
			rem += PRICE_SCALE;
			cash--;
		}

		while (rem >= PRICE_SCALE)
		{
			rem -= PRICE_SCALE;
			cash = SatAdd(cash, 1);
		}

		if (rem >= PRICE_SCALE / 2)
			cash = SatAdd(cash, 1);

		return LF_Utils.MaxInt(cash, 0);
	}

	//------------------------------------------------------------------------------------------------
	//! a * b for a, b >= 0, stopping at INT_MAX.
	protected static int SatMul(int a, int b)
	{
		if (a <= 0 || b <= 0)
			return 0;

		if (a > INT_MAX / b)
			return INT_MAX;

		return a * b;
	}

	//------------------------------------------------------------------------------------------------
	//! a + b for a >= 0 and b >= 0, stopping at INT_MAX.
	protected static int SatAdd(int a, int b)
	{
		if (b <= 0)
			return a;

		if (a > INT_MAX - b)
			return INT_MAX;

		return a + b;
	}

	//------------------------------------------------------------------------------------------------
	//! Whole recovery hours in `seconds`, capped at `maxHours`. A negative gap (the clock moved back) is 0.
	static int RecoveryHours(int seconds, int maxHours)
	{
		if (seconds <= 0 || maxHours <= 0)
			return 0;

		int hours = seconds / SECONDS_PER_HOUR;
		return LF_Utils.MinInt(hours, maxHours);
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when `seconds` is longer than `maxHours`: the caller moves its stamp to now (the
	//! price is as recovered as the cap allows) instead of by whole hours.
	static bool IsClamped(int seconds, int maxHours)
	{
		if (maxHours <= 0)
			return seconds > 0;

		return seconds / SECONDS_PER_HOUR > maxHours;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The price after `seconds` of recovery: each whole hour closes `recoverPctPerHour` of the gap
	//! to `base`, at least 1 fine unit while a gap is left, never past the base and held inside
	//! floor..ceiling. A gap beyond `maxHours` counts as `maxHours`. 0 percent or no time returns the
	//! price unchanged.
	static int Recover(int current, int base, int recoverPctPerHour, int seconds, int maxHours, int floorPrice, int ceilingPrice)
	{
		int price = ClampPrice(current, floorPrice, ceilingPrice);
		int hours = RecoveryHours(seconds, maxHours);
		if (hours <= 0 || recoverPctPerHour <= 0)
			return price;

		int target = ClampPrice(base, floorPrice, ceilingPrice);
		for (int h = 0; h < hours; h++)
		{
			int gap = target - price;
			if (gap == 0)
				break;

			price += RecoverStep(gap, recoverPctPerHour);
		}

		return price;
	}

	//------------------------------------------------------------------------------------------------
	//! One hour's move for a signed `gap` (target minus price): the percent of the gap, rounded up so a
	//! small gap still closes, never more than the gap itself. The sign follows the gap. Split so a gap of
	//! 10 million cash times 100 percent cannot overflow.
	protected static int RecoverStep(int gap, int recoverPctPerHour)
	{
		int size = gap;
		if (size < 0)
			size = -size;

		int pct = LF_Utils.ClampInt(recoverPctPerHour, 0, 100);
		int step = (size / PRICE_SCALE) * pct + ((size % PRICE_SCALE) * pct + PRICE_SCALE - 1) / PRICE_SCALE;
		step = LF_Utils.ClampInt(step, 1, size);
		if (gap < 0)
			step = -step;

		return step;
	}
}
