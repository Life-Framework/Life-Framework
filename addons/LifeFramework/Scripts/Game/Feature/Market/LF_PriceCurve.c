//------------------------------------------------------------------------------------------------
//! Market price curve (#2844, framework #762): pure rules, no world and no statics. A good has a base,
//! a floor and a ceiling (whole cash units), a drop per unit sold (a percent of the base) and a recovery
//! per hour (a percent of the gap to the base). The caller holds the current price and the unix second
//! it last moved; these functions say what a sale pays and where the price stands after a gap.
//! Everything is int maths through LF_Utils.MaxInt/MinInt/ClampInt (never Math.Max/Min/Clamp, #2787).
class LF_PriceCurve
{
	static const int SECONDS_PER_HOUR = 3600;	// lf-fixed: seconds in one real hour

	//! Most units one bulk sale prices: keeps the sums inside an int. A larger stack is two calls.
	static const int MAX_BULK_UNITS = 100000;	// lf-fixed: int overflow guard for the closed-form sum

	//------------------------------------------------------------------------------------------------
	//! \return The cash one sold unit takes off the price: dropPerUnitPct of the base, rounded to the
	//! nearest unit, and at least 1 whenever the percent is above 0 so a small base still moves. 0 for no drop.
	static int DropPerUnit(int base, int dropPerUnitPct)
	{
		if (dropPerUnitPct <= 0 || base <= 0)
			return 0;

		int drop = (base * dropPerUnitPct + 50) / 100;
		return LF_Utils.MaxInt(drop, 1);
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

		// Units that still drop the price: the rest land on the floor. Bounded, so units * drop cannot overflow.
		int dropping = UnitsAboveFloor(price, drop, floorPrice);
		int moved = LF_Utils.MinInt(units, dropping);
		return LF_Utils.MaxInt(price - moved * drop, floorPrice);
	}

	//------------------------------------------------------------------------------------------------
	//! \return The total `units` sold in one go pay from `current`: unit k fetches the price after k
	//! earlier units, so the total equals the sum of `units` single sales. 0 for no units.
	static int BulkTotal(int current, int units, int drop, int floorPrice, int ceilingPrice)
	{
		if (units <= 0)
			return 0;

		units = LF_Utils.MinInt(units, MAX_BULK_UNITS);
		int price = ClampPrice(current, floorPrice, ceilingPrice);
		if (drop <= 0)
			return price * units;

		int dropping = LF_Utils.MinInt(UnitsAboveFloor(price, drop, floorPrice), units);
		// Units 0..dropping-1 fetch price - i * drop (an arithmetic run), the rest fetch the floor.
		int total = dropping * price - drop * (dropping * (dropping - 1) / 2);
		total += (units - dropping) * floorPrice;
		return total;
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
	//! to `base`, at least 1 unit while a gap is left, never past the base and held inside floor..ceiling.
	//! A gap beyond `maxHours` counts as `maxHours`. 0 percent or no time returns the price unchanged.
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
	//! small gap still closes, never more than the gap itself. The sign follows the gap.
	protected static int RecoverStep(int gap, int recoverPctPerHour)
	{
		int size = gap;
		if (size < 0)
			size = -size;

		int step = (size * recoverPctPerHour + 99) / 100;
		step = LF_Utils.ClampInt(step, 1, size);
		if (gap < 0)
			step = -step;

		return step;
	}
}
