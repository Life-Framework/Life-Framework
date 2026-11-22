//------------------------------------------------------------------------------------------------
//! How law-book codes combine on a charge sheet (#4202). Pure functions over catalog entries: no
//! account, no money, no world. The charge sheet (L3) calls them to total and to grey out.
//!
//! - Two codes in the same non-empty stacking group do not add: the higher one counts, the other is
//!   merged into it (speeding 16 to 30 over swallows speeding 1 to 15 over).
//! - Codes in different groups add. A code with no group stacks with nothing, so it always adds.
//! - A jail-only code refuses a fine: it adds nothing to the fine total and `CanFine` says no. Its
//!   time still counts.
class LF_LawCodeRules
{
	//------------------------------------------------------------------------------------------------
	//! False for a jail-only code (or null): the officer may not offer a fine for it.
	static bool CanFine(LF_CrimeEntry code)
	{
		return code && !code.m_bJailOnly;
	}

	//------------------------------------------------------------------------------------------------
	//! True when two codes sit in the same non-empty stacking group, so only the higher counts.
	static bool SameGroup(LF_CrimeEntry a, LF_CrimeEntry b)
	{
		return a && b && !a.m_sStackGroup.IsEmpty() && a.m_sStackGroup == b.m_sStackGroup;
	}

	//------------------------------------------------------------------------------------------------
	//! The value a code contributes: its fine (0 when jail only) or its sentence seconds.
	protected static int ValueOf(LF_CrimeEntry code, bool byFine)
	{
		if (byFine)
		{
			if (!CanFine(code))
				return 0;

			return code.m_iFine;
		}

		return code.m_iSentenceSeconds;
	}

	//------------------------------------------------------------------------------------------------
	//! The codes that count after stacking, in the order each group or ungrouped code first appeared.
	//! `byFine` picks what "higher" means (the fine, or the sentence) and drops jail-only codes. A code
	//! left out is one the sheet greys out as merged.
	//! \param[in] codes The ticked charges.
	//! \param[in] byFine True to stack by fine, false to stack by sentence.
	//! \param[out] outCounted Cleared, then filled with the winners.
	static void Counted(notnull array<LF_CrimeEntry> codes, bool byFine, notnull array<LF_CrimeEntry> outCounted)
	{
		outCounted.Clear();
		map<string, int> groupAt = new map<string, int>();
		foreach (LF_CrimeEntry code : codes)
		{
			if (!code)
				continue;

			if (byFine && !CanFine(code))
				continue;

			if (code.m_sStackGroup.IsEmpty())
			{
				outCounted.Insert(code);
				continue;
			}

			int index;
			if (!groupAt.Find(code.m_sStackGroup, index))
			{
				groupAt.Set(code.m_sStackGroup, outCounted.Count());
				outCounted.Insert(code);
				continue;
			}

			if (ValueOf(code, byFine) > ValueOf(outCounted[index], byFine))
				outCounted.Set(index, code);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The fine for these codes together: stacked by group, jail-only codes left out.
	static int TotalFine(notnull array<LF_CrimeEntry> codes)
	{
		array<LF_CrimeEntry> counted = {};
		Counted(codes, true, counted);
		int total;
		foreach (LF_CrimeEntry code : counted)
		{
			total += code.m_iFine;
		}

		return total;
	}

	//------------------------------------------------------------------------------------------------
	//! The jail time in seconds for these codes together: stacked by group, jail-only codes included.
	static int TotalSentenceSeconds(notnull array<LF_CrimeEntry> codes)
	{
		array<LF_CrimeEntry> counted = {};
		Counted(codes, false, counted);
		int total;
		foreach (LF_CrimeEntry code : counted)
		{
			total += code.m_iSentenceSeconds;
		}

		return total;
	}
}
