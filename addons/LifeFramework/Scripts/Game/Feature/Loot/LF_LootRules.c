//------------------------------------------------------------------------------------------------
//! Pure loot rules (#3341, slice of #1020): no world, no entity, no clock of its own. Rolls a loot
//! table (LF_LootConfig) from a seed, so a test can predict the result, and decides whether a
//! container may refill from the times the caller passes in. A bad table (no rows, every weight 0, an
//! empty prefab, a count range turned round, no rolls) logs LF_Debug.Error and rolls nothing.
//------------------------------------------------------------------------------------------------

//! One roll result: a prefab and how many of it.
class LF_LootDrop
{
	ResourceName m_Prefab;
	int m_iCount;
}

//------------------------------------------------------------------------------------------------
class LF_LootRules
{
	//! Park-Miller modulus (2^31 - 1) and multiplier; Schrage's split keeps every step inside an int.
	protected static const int RNG_MODULUS = 2147483647;
	protected static const int RNG_MULTIPLIER = 16807;
	protected static const int RNG_QUOTIENT = 127773;
	protected static const int RNG_REMAINDER = 2836;

	//! The most rolls one fill may take, so a typo in a config cannot spawn thousands of items.
	static const int MAX_ROLLS = 64; // lf-fixed: a safety cap against a config typo, not tuning

	//------------------------------------------------------------------------------------------------
	//! Why a table cannot be rolled, or empty when it can.
	static string Validate(LF_LootTableDef table)
	{
		if (!table)
			return "no loot table";

		if (table.m_sKind.IsEmpty())
			return "the table has no container kind";

		if (!table.m_aRows || table.m_aRows.IsEmpty())
			return "the table has no rows";

		if (table.m_iRolls < 1 || table.m_iRolls > MAX_ROLLS)
			return string.Format("fill size %1 is not between 1 and %2", table.m_iRolls, MAX_ROLLS);

		if (table.m_iRefillSeconds < 0)
			return string.Format("refill time %1 is below 0", table.m_iRefillSeconds);

		int total;
		foreach (int i, LF_LootRowDef row : table.m_aRows)
		{
			if (!row)
				return string.Format("row %1 is empty", i);

			if (row.m_Prefab.IsEmpty())
				return string.Format("row %1 has no prefab", i);

			if (row.m_iWeight < 0)
				return string.Format("row %1 has a negative weight", i);

			if (row.m_iCountMin < 1 || row.m_iCountMax < row.m_iCountMin)
				return string.Format("row %1 count range %2..%3 is not 1 or more, smallest first", i, row.m_iCountMin, row.m_iCountMax);

			total += row.m_iWeight;
		}

		if (total < 1)
			return "every row has weight 0, nothing could be picked";

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! The next state of the seeded generator. Any seed maps into 1..2147483646 first, so 0 and
	//! negative seeds work and never stall the generator.
	static int NextSeed(int state)
	{
		if (state < 0)
			state = -(state + 1);

		state = state % RNG_MODULUS;
		if (state == 0)
			state = 1;

		int hi = state / RNG_QUOTIENT;
		int lo = state % RNG_QUOTIENT;
		int next = RNG_MULTIPLIER * lo - RNG_REMAINDER * hi;
		if (next <= 0)
			next += RNG_MODULUS;

		return next;
	}

	//------------------------------------------------------------------------------------------------
	//! One roll of a validated table from `state`: fills `drop` and returns the state to roll from next.
	protected static int RollOnce(LF_LootTableDef table, int state, out LF_LootDrop drop)
	{
		drop = null;
		int total;
		foreach (LF_LootRowDef row : table.m_aRows)
		{
			total += row.m_iWeight;
		}

		state = NextSeed(state);
		int pick = state % total;
		LF_LootRowDef chosen;
		foreach (LF_LootRowDef candidate : table.m_aRows)
		{
			if (candidate.m_iWeight < 1)
				continue;

			if (pick < candidate.m_iWeight)
			{
				chosen = candidate;
				break;
			}

			pick -= candidate.m_iWeight;
		}

		if (!chosen)
			return state;

		int span = chosen.m_iCountMax - chosen.m_iCountMin + 1;
		int count = chosen.m_iCountMin;
		if (span > 1)
		{
			state = NextSeed(state);
			count += state % span;
		}

		drop = new LF_LootDrop();
		drop.m_Prefab = chosen.m_Prefab;
		drop.m_iCount = count;
		return state;
	}

	//------------------------------------------------------------------------------------------------
	//! Rolls the table m_iRolls times from `seed` into `drops` (cleared first), the same seed giving
	//! the same drops. A bad table logs LF_Debug.Error and leaves `drops` empty. Equal prefabs from
	//! separate rolls stay separate entries (the caller stacks them).
	//! \return How many drops were rolled.
	static int Roll(LF_LootTableDef table, int seed, notnull array<ref LF_LootDrop> drops)
	{
		drops.Clear();
		string problem = Validate(table);
		if (!problem.IsEmpty())
		{
			LF_Debug.Error("Loot", string.Format("bad loot table %1, nothing rolled: %2", KindOf(table), problem));
			return 0;
		}

		int state = seed;
		for (int i = 0; i < table.m_iRolls; i++)
		{
			LF_LootDrop drop;
			state = RollOnce(table, state, drop);
			if (drop)
				drops.Insert(drop);
		}

		return drops.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! May a container refill? `lastFillSeconds` is when it last filled and `nowSeconds` the current time,
	//! on the same clock (the caller's; this class reads none). A negative `lastFillSeconds` means it
	//! never filled, which is allowed. Refused before the refill time has passed, allowed at it.
	//! A bad table refuses and logs LF_Debug.Error.
	static bool CanRefill(LF_LootTableDef table, int lastFillSeconds, int nowSeconds)
	{
		string problem = Validate(table);
		if (!problem.IsEmpty())
		{
			LF_Debug.Error("Loot", string.Format("bad loot table %1, no refill: %2", KindOf(table), problem));
			return false;
		}

		if (lastFillSeconds < 0)
			return true;

		return nowSeconds - lastFillSeconds >= table.m_iRefillSeconds;
	}

	//------------------------------------------------------------------------------------------------
	//! Seconds until the container may refill; 0 when it may now or never filled, and for a missing table.
	static int SecondsUntilRefill(LF_LootTableDef table, int lastFillSeconds, int nowSeconds)
	{
		if (!table || lastFillSeconds < 0)
			return 0;

		int left = table.m_iRefillSeconds - (nowSeconds - lastFillSeconds);
		if (left < 0)
			return 0;

		return left;
	}

	//------------------------------------------------------------------------------------------------
	protected static string KindOf(LF_LootTableDef table)
	{
		if (!table)
			return "(none)";

		return table.m_sKind;
	}
}
