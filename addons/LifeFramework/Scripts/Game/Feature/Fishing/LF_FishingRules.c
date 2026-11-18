//------------------------------------------------------------------------------------------------
//! Where a strike lands against the open bite (#2927).
enum LF_EFishStrike
{
	EARLY,	//! before the bite: a miss that costs the bait
	HIT,	//! inside the window plus the latency slack
	LATE	//! after it: the same miss
}

//------------------------------------------------------------------------------------------------
//! What one landed strike gives: the species, the size and the prefab to hand over.
class LF_FishCatch
{
	LF_FishSpeciesDef m_Species;
	int m_iSize;			//! 0 small, 1 medium, 2 large
	ResourceName m_Prefab;	//! empty when the species has no prefab for that size: the caller logs and gives nothing
	string m_sTable;		//! the table the roll used
	bool m_bOpenWater;		//! true when the open-water table was used (no spot, or an emptied one)
}

//------------------------------------------------------------------------------------------------
//! Fishing rules (#2927, docs/architecture/fishing.md section 2): pure and static, no world, no
//! statics. Every random draw comes in as a roll in 0..1, so a test fixes the outcome and the server
//! passes `Math.RandomFloat01()`. Times are seconds on the caller's clock. A spot's stock is an
//! `LF_GatherStock` (the ore-node pattern: it thins per catch and regrows on time; nothing saves it).
class LF_FishingRules
{
	//------------------------------------------------------------------------------------------------
	//! \return Seconds from the cast to the bite. A roll of 0 is the shortest wait, 1 the longest, scaled
	//! by the rod and bait speed (2 halves it) and slowed at an emptied spot, then always held inside the
	//! config's min and max. Null rod or bait count as speed 1.
	static float BiteSeconds(LF_FishingConfig config, LF_FishingRodDef rod, LF_FishingBaitDef bait, bool spotEmpty, float roll)
	{
		float span = config.m_fBiteMaxSeconds - config.m_fBiteMinSeconds;
		float seconds = config.m_fBiteMinSeconds + span * Math.Clamp(roll, 0, 1);

		float speed = 1;
		if (rod && rod.m_fBiteSpeed > 0)
			speed *= rod.m_fBiteSpeed;
		if (bait && bait.m_fBiteSpeed > 0)
			speed *= bait.m_fBiteSpeed;

		seconds /= speed;
		if (spotEmpty)
			seconds *= config.m_fEmptySpotSlowdown;

		return Math.Clamp(seconds, config.m_fBiteMinSeconds, config.m_fBiteMaxSeconds);
	}

	//------------------------------------------------------------------------------------------------
	//! \return Where a strike lands. `secondsSinceBite` is the strike time minus the bite time (negative
	//! before the bite). The window runs from the bite to window plus slack, both ends inclusive.
	static LF_EFishStrike Strike(LF_FishingConfig config, float secondsSinceBite)
	{
		if (secondsSinceBite < 0)
			return LF_EFishStrike.EARLY;

		if (secondsSinceBite > config.m_fStrikeWindowSeconds + config.m_fStrikeSlackSeconds)
			return LF_EFishStrike.LATE;

		return LF_EFishStrike.HIT;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when a strike `secondsSinceBite` after the bite lands.
	static bool StrikeLands(LF_FishingConfig config, float secondsSinceBite)
	{
		return Strike(config, secondsSinceBite) == LF_EFishStrike.HIT;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when the cast `distance` metres out is inside the rod's range.
	static bool InCastRange(LF_FishingRodDef rod, float distance)
	{
		if (!rod)
			return false;

		return distance >= 0 && distance <= rod.m_fCastRange;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when `fisherLevel` may use the rod.
	static bool RodUsable(LF_FishingRodDef rod, int fisherLevel)
	{
		if (!rod)
			return false;

		return fisherLevel >= rod.m_iMinFisherLevel;
	}

	//------------------------------------------------------------------------------------------------
	//! Casts per minute: a player's cast times are kept in `castTimes` (seconds). Drops those older than
	//! a minute, refuses when the config's limit is already used up, else records `now`.
	//! \return True when the cast is allowed (and recorded).
	static bool TryCast(LF_FishingConfig config, notnull array<float> castTimes, float now)
	{
		for (int i = castTimes.Count() - 1; i >= 0; i--)
		{
			if (now - castTimes[i] >= 60)
				castTimes.Remove(i);
		}

		if (castTimes.Count() >= config.m_iCastsPerMinute)
			return false;

		castTimes.Insert(now);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! A spot's stock: full at the start, one fewer per catch, one back every `regrowSeconds`.
	static LF_GatherStock NewStock(int stock, float regrowSeconds, float nowSeconds)
	{
		return LF_GatherStock.Create(stock, regrowSeconds * 1000, nowSeconds * 1000);
	}

	//------------------------------------------------------------------------------------------------
	//! \return Fish left in the spot at `nowSeconds`, after regrowth.
	static int StockLeft(notnull LF_GatherStock stock, float nowSeconds)
	{
		return stock.Available(nowSeconds * 1000);
	}

	//------------------------------------------------------------------------------------------------
	//! Takes one fish out of the spot. False when it is empty.
	static bool TakeFish(notnull LF_GatherStock stock, float nowSeconds)
	{
		return stock.Take(nowSeconds * 1000);
	}

	//------------------------------------------------------------------------------------------------
	//! \return The table a cast rolls on: the spot's table while the spot still has fish, else the
	//! open-water table (no spot, an empty spot, or a spot table the config does not have). `openWater`
	//! says which. Null when even the open-water table is missing.
	static LF_FishTableDef ResolveTable(LF_FishingConfig config, string spotTable, int stockLeft, out bool openWater)
	{
		openWater = true;
		if (!spotTable.IsEmpty() && stockLeft > 0)
		{
			LF_FishTableDef spot = config.FindTable(spotTable);
			if (spot)
			{
				openWater = false;
				return spot;
			}

			LF_Debug.Warn(LF_FishingConfig.FEATURE, string.Format("spot table '%1' is not in the config: open water used", spotTable));
		}

		return config.FindTable(config.m_sOpenWaterTable);
	}

	//------------------------------------------------------------------------------------------------
	//! \return A species from `table`, weighted, for this water kind and hour of day (0 to 23) and level.
	//! A species that lives in other water, does not bite at that hour or needs a higher level is out, so
	//! a pond never gives an ocean fish. A species the bait boosts weighs `m_fBaitBoost` times more.
	//! Null when nothing is eligible. `roll` is 0..1.
	static LF_FishSpeciesDef PickSpecies(LF_FishingConfig config, LF_FishTableDef table, string water, int hour, int fisherLevel, LF_FishingBaitDef bait, float roll)
	{
		if (!table || !table.m_aSpecies)
			return null;

		array<LF_FishSpeciesDef> eligible = {};
		array<float> weights = {};
		float total = 0;
		foreach (LF_FishWeightDef entry : table.m_aSpecies)
		{
			if (!entry || entry.m_iWeight <= 0)
				continue;

			LF_FishSpeciesDef species = config.FindSpecies(entry.m_sSpecies);
			if (!species || !species.LivesIn(water) || !species.BitesAt(hour) || fisherLevel < species.m_iMinFisherLevel)
				continue;

			float weight = entry.m_iWeight;
			if (bait && bait.Boosts(species.m_sId))
				weight *= config.m_fBaitBoost;

			eligible.Insert(species);
			weights.Insert(weight);
			total += weight;
		}

		if (eligible.IsEmpty() || total <= 0)
			return null;

		float target = Math.Clamp(roll, 0, 0.999999) * total;
		float running = 0;
		for (int i = 0; i < eligible.Count(); i++)
		{
			running += weights[i];
			if (target < running)
				return eligible[i];
		}

		return eligible[eligible.Count() - 1];
	}

	//------------------------------------------------------------------------------------------------
	//! \return The size index (0 small, 1 medium, 2 large) from the table's size weights. `roll` is 0..1.
	static int PickSize(LF_FishTableDef table, float roll)
	{
		if (!table || !table.m_aSizeWeights || table.m_aSizeWeights.Count() != LF_FishingConfig.SIZE_COUNT)
			return 0;

		int total = 0;
		foreach (int weight : table.m_aSizeWeights)
			total += LF_Utils.MaxInt(weight, 0);

		if (total <= 0)
			return 0;

		float target = Math.Clamp(roll, 0, 0.999999) * total;
		float running = 0;
		for (int i = 0; i < LF_FishingConfig.SIZE_COUNT; i++)
		{
			running += LF_Utils.MaxInt(table.m_aSizeWeights[i], 0);
			if (target < running)
				return i;
		}

		return LF_FishingConfig.SIZE_COUNT - 1;
	}

	//------------------------------------------------------------------------------------------------
	//! The whole roll for one landed strike: table (spot or open water by stock), species, size, prefab.
	//! An open-water catch is always small. When the spot's table has nothing eligible at this hour or
	//! level, the open-water table is tried before giving up. Null when nothing can be caught (logged).
	static LF_FishCatch PickCatch(LF_FishingConfig config, string spotTable, int stockLeft, string water, int hour, int fisherLevel, LF_FishingBaitDef bait, float speciesRoll, float sizeRoll)
	{
		bool openWater;
		LF_FishTableDef table = ResolveTable(config, spotTable, stockLeft, openWater);
		LF_FishSpeciesDef species = PickSpecies(config, table, water, hour, fisherLevel, bait, speciesRoll);
		if (!species && !openWater)
		{
			openWater = true;
			table = config.FindTable(config.m_sOpenWaterTable);
			species = PickSpecies(config, table, water, hour, fisherLevel, bait, speciesRoll);
		}

		if (!table || !species)
		{
			LF_Debug.Warn(LF_FishingConfig.FEATURE, string.Format("nothing to catch in %1 water at hour %2 (level %3)", water, hour, fisherLevel));
			return null;
		}

		LF_FishCatch result = new LF_FishCatch();
		result.m_Species = species;
		result.m_sTable = table.m_sId;
		result.m_bOpenWater = openWater;
		result.m_iSize = 0;
		if (!openWater)
			result.m_iSize = PickSize(table, sizeRoll);

		if (species.m_aSizePrefabs && result.m_iSize < species.m_aSizePrefabs.Count())
			result.m_Prefab = species.m_aSizePrefabs[result.m_iSize];

		return result;
	}
}
