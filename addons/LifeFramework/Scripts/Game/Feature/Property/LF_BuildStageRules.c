//------------------------------------------------------------------------------------------------
//! Result of one deposit try (LF_BuildStageRules.Deposit). A refused deposit leaves m_sDeposited as
//! it was; an overflow names the item and how many of it did not fit.
class LF_BuildDepositResult
{
	bool m_bAccepted;
	int m_iReason; // 0 when accepted, else an LF_BuildStageRules.REFUSED_* code
	string m_sDeposited; // the "prefab=count;..." string after the try
	string m_sOverflowPrefab;
	int m_iOverflow;
}

//------------------------------------------------------------------------------------------------
//! The build-stage rules (#2648, slice of #747). Pure: no entities, no time, no network. Given a
//! stage def (LF_PropertyStageDef, #2647) and what a site holds (LF_PropertyRecord.m_sDeposited and
//! m_iWorkDone, #2646) they say what is missing, whether the stage is done and who may deposit.
//! Deposited items are counts, never entities: "prefab=count;prefab=count" as the record stores them.
//! Only the owner and the invited key holders may deposit or work on a site (grief rule, #807).
class LF_BuildStageRules
{
	static const int REFUSED_NOT_ALLOWED = 1;
	static const int REFUSED_BAD_COUNT = 2;
	static const int REFUSED_OVERFLOW = 3;

	//------------------------------------------------------------------------------------------------
	//! Stage `stage` (1-based) of a tier from the loaded config, or null.
	static LF_PropertyStageDef Stage(string tierId, int stage)
	{
		LF_PropertyConfig config = LF_PropertyConfig.Get();
		if (!config)
			return null;

		LF_PropertyTierDef tier = config.FindTier(tierId);
		if (!tier || !tier.m_aStages || stage < 1 || stage > tier.m_aStages.Count())
			return null;

		return tier.m_aStages[stage - 1];
	}

	//------------------------------------------------------------------------------------------------
	//! Reads "prefab=count;..." into parallel arrays. Malformed pieces (no "=", empty prefab, a count
	//! that is not a positive number) are skipped; a prefab named twice adds up.
	static void Parse(string deposited, array<string> prefabs, array<int> counts)
	{
		prefabs.Clear();
		counts.Clear();
		if (deposited.IsEmpty())
			return;

		array<string> pieces = new array<string>();
		deposited.Split(";", pieces, true);
		foreach (string piece : pieces)
		{
			array<string> pair = new array<string>();
			piece.Split("=", pair, true);
			if (pair.Count() != 2)
				continue;

			int count = pair[1].ToInt();
			if (count < 1)
				continue;

			int at = prefabs.Find(pair[0]);
			if (at < 0)
			{
				prefabs.Insert(pair[0]);
				counts.Insert(count);
			}
			else
			{
				counts[at] = counts[at] + count;
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Writes parallel arrays as "prefab=count;..."; entries with a count under 1 are dropped.
	static string Format(array<string> prefabs, array<int> counts)
	{
		string result;
		for (int i = 0; i < prefabs.Count(); i++)
		{
			if (counts[i] < 1)
				continue;

			if (!result.IsEmpty())
				result += ";";

			result += prefabs[i] + "=" + counts[i].ToString();
		}

		return result;
	}

	//------------------------------------------------------------------------------------------------
	//! How many of `prefab` the deposit string holds; 0 when absent.
	static int CountOf(string deposited, string prefab)
	{
		array<string> prefabs = new array<string>();
		array<int> counts = new array<int>();
		Parse(deposited, prefabs, counts);
		int at = prefabs.Find(prefab);
		if (at < 0)
			return 0;

		return counts[at];
	}

	//------------------------------------------------------------------------------------------------
	//! How many of `prefab` the stage consumes in all; 0 when it needs none of it.
	static int Needed(LF_PropertyStageDef def, string prefab)
	{
		if (!def || !def.m_aRequirements)
			return 0;

		int needed;
		foreach (LF_PropertyStageReqDef req : def.m_aRequirements)
		{
			if (req && req.m_Prefab == prefab && req.m_iCount > 0)
				needed += req.m_iCount;
		}

		return needed;
	}

	//------------------------------------------------------------------------------------------------
	//! How many of `prefab` the stage still lacks; 0 when met or not needed.
	static int Missing(LF_PropertyStageDef def, string deposited, string prefab)
	{
		return Math.Max(0, Needed(def, prefab) - CountOf(deposited, prefab));
	}

	//------------------------------------------------------------------------------------------------
	//! What the stage still lacks as "prefab=count;..." in requirement order; empty when items are met.
	static string MissingText(LF_PropertyStageDef def, string deposited)
	{
		if (!def || !def.m_aRequirements)
			return "";

		array<string> prefabs = new array<string>();
		array<int> counts = new array<int>();
		foreach (LF_PropertyStageReqDef req : def.m_aRequirements)
		{
			if (!req || req.m_Prefab.IsEmpty() || prefabs.Find(req.m_Prefab) >= 0)
				continue;

			prefabs.Insert(req.m_Prefab);
			counts.Insert(Missing(def, deposited, req.m_Prefab));
		}

		return Format(prefabs, counts);
	}

	//------------------------------------------------------------------------------------------------
	//! True when every required item is fully deposited (also when the stage needs no items).
	static bool ItemsMet(LF_PropertyStageDef def, string deposited)
	{
		if (!def)
			return false;

		return MissingText(def, deposited).IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	//! Work units the stage still needs after `workDone`; never negative.
	static int WorkRemaining(LF_PropertyStageDef def, int workDone)
	{
		if (!def)
			return 0;

		return Math.Max(0, def.m_iWorkUnits - Math.Max(0, workDone));
	}

	//------------------------------------------------------------------------------------------------
	static bool WorkMet(LF_PropertyStageDef def, int workDone)
	{
		if (!def)
			return false;

		return WorkRemaining(def, workDone) == 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Rounds (one unit per worker per round) the rest of the stage's work takes with `workers` hands
	//! on it: the LF_StationProgress.Rounds maths over what is left.
	static int RoundsLeft(LF_PropertyStageDef def, int workDone, int workers)
	{
		return LF_StationProgress.Rounds(WorkRemaining(def, workDone), workers);
	}

	//------------------------------------------------------------------------------------------------
	//! Work done after adding `units`, clamped to what the stage needs. Work belongs to the site, so
	//! every worker's units land on the same number.
	static int WorkAfter(LF_PropertyStageDef def, int workDone, int units)
	{
		if (!def)
			return workDone;

		if (units < 1)
			return workDone;

		return Math.Min(Math.Max(0, workDone) + units, Math.Max(0, def.m_iWorkUnits));
	}

	//------------------------------------------------------------------------------------------------
	//! A stage is done only when its items and its work are both met.
	static bool IsDone(LF_PropertyStageDef def, string deposited, int workDone)
	{
		if (!def)
			return false;

		return ItemsMet(def, deposited) && WorkMet(def, workDone);
	}

	//------------------------------------------------------------------------------------------------
	//! The owner and the invited key holders may deposit or work; nobody else, never an empty key.
	static bool MayContribute(string ownerKey, array<string> keyHolders, string actorKey)
	{
		if (actorKey.IsEmpty())
			return false;

		if (actorKey == ownerKey)
			return true;

		if (keyHolders && keyHolders.Find(actorKey) >= 0)
			return true;

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Tries to deposit `count` of `prefab`. Refused when the actor may not contribute, the count is
	//! under 1, or the deposit passes what the stage needs (the overflow is named). Nothing is
	//! partly accepted: a refused try returns the string unchanged.
	static LF_BuildDepositResult Deposit(LF_PropertyStageDef def, string deposited, string prefab, int count, string ownerKey, array<string> keyHolders, string actorKey)
	{
		LF_BuildDepositResult result = new LF_BuildDepositResult();
		result.m_sDeposited = deposited;

		if (!MayContribute(ownerKey, keyHolders, actorKey))
		{
			result.m_iReason = REFUSED_NOT_ALLOWED;
			return result;
		}

		if (count < 1 || prefab.IsEmpty())
		{
			result.m_iReason = REFUSED_BAD_COUNT;
			return result;
		}

		int missing = Missing(def, deposited, prefab);
		if (count > missing)
		{
			result.m_iReason = REFUSED_OVERFLOW;
			result.m_sOverflowPrefab = prefab;
			result.m_iOverflow = count - missing;
			return result;
		}

		array<string> prefabs = new array<string>();
		array<int> counts = new array<int>();
		Parse(deposited, prefabs, counts);
		int at = prefabs.Find(prefab);
		if (at < 0)
		{
			prefabs.Insert(prefab);
			counts.Insert(count);
		}
		else
		{
			counts[at] = counts[at] + count;
		}

		result.m_bAccepted = true;
		result.m_sDeposited = Format(prefabs, counts);
		return result;
	}
}
