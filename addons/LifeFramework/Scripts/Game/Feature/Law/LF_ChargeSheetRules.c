//------------------------------------------------------------------------------------------------
//! Why a charge sheet could not be signed (#4209).
enum LF_ChargeSheetRefusal
{
	NONE,
	NOT_ON_DUTY,		//!< only an on-duty officer signs a sheet (N7)
	NO_CHARGES,			//!< nothing ticked, or no ticked code is in the catalog
	OVERRIDE_RANK,		//!< "jail only" was asked by an officer below the override rank
	OVERRIDE_NO_TIME	//!< "jail only" was asked but no ticked code carries any time
}

//------------------------------------------------------------------------------------------------
//! What a charge sheet comes to (#4209): the codes that count, the fine after the rank cap, the time,
//! and whether the suspect may pay instead of going to the cell. Built by `LF_ChargeSheetRules`.
class LF_ChargeSheetResult : Managed
{
	LF_ChargeSheetRefusal m_eRefusal;

	//! The codes that count after stacking by fine, repeats already doubled (copies, not catalog rows).
	ref array<ref LF_CrimeEntry> m_aCounted = {};

	//! The stacked fine before the rank cap.
	int m_iFineUncapped;

	//! The fine the sheet asks for: the stacked total, capped by the officer's rank. 0 when jail only.
	int m_iFine;

	//! True when the rank cap cut the fine.
	bool m_bCapped;

	//! The time in the cell, stacked by sentence, repeats doubled, at most `MAX_SENTENCE_SECONDS`.
	int m_iSentenceSeconds;

	//! True when the sheet may offer "pay now": a fine above 0, no jail-only code, no override.
	bool m_bFineOffered;

	//! True when a ticked code is jail only, or the officer used the override.
	bool m_bJailOnly;

	//! True when the jail-only override was used (and logged).
	bool m_bOverridden;

	//! How many ticked codes were doubled as repeats.
	int m_iRepeats;

	//------------------------------------------------------------------------------------------------
	bool IsSigned()
	{
		return m_eRefusal == LF_ChargeSheetRefusal.NONE;
	}
}

//------------------------------------------------------------------------------------------------
//! The charge sheet's rules (#4209, police L3, decision N7). Pure: no account, no money moves, no RPC.
//! The booking desk (L5) calls `Evaluate` on the server and takes the money or starts the sentence.
//!
//! - **Stacking.** The law book's rules (`LF_LawCodeRules`): one code per non-empty stacking group
//!   counts, the higher one; the rest add. The fine stacks by fine, the time stacks by sentence.
//! - **Repeat.** A code the suspect was already charged with today counts double, fine and time. The
//!   caller passes those code ids; the doubled value takes part in the stacking comparison.
//! - **Rank cap.** The total fine is clamped to the officer's rank cap (the pack: $500 patrol to
//!   $5,000 lieutenant). A sheet never asks for more than the officer may write.
//! - **Jail only.** A jail-only code on the sheet removes "pay now". A Sergeant and up may switch any
//!   sheet to jail only (N7); the fine drops to 0, the time stays, and the switch is logged. A switch
//!   with no time to serve is refused.
//! - **Sentence cap.** Time adds up to `MAX_SENTENCE_SECONDS` (60 minutes).
class LF_ChargeSheetRules
{
	static const string FEATURE = "Law";

	//! The longest stay in the cell a sheet can give, however many codes stack.
	static const int MAX_SENTENCE_SECONDS = 3600;	// lf-fixed: the law book's 60-minute rule (M7 plan); a config field belongs to the police config, not this card

	//! A repeat offence counts this many times its code's fine and time.
	static const int REPEAT_FACTOR = 2;

	//! The ladder row whose rank and above may use the jail-only override. A ladder without it falls
	//! back to its top rank.
	static const string OVERRIDE_RANK_ID = "sergeant";

	//! The largest fine an int sum is allowed to reach before the cap (the ticket schema limit).
	protected static const int FINE_LIMIT = 10000000;	// lf-fixed: the schema limit, mirrors police fineCap

	//------------------------------------------------------------------------------------------------
	//! The lowest rank index that may switch a sheet to jail only: the `sergeant` row of the
	//! configured ladder, or the top rank when the ladder has no such row.
	static int OverrideMinRank()
	{
		array<ref LF_PoliceRank> ranks = {};
		LF_PoliceConfig.Ranks(ranks);
		foreach (int i, LF_PoliceRank rank : ranks)
		{
			if (rank && rank.m_sId == OVERRIDE_RANK_ID)
				return i;
		}

		return LF_Utils.MaxInt(ranks.Count() - 1, 0);
	}

	//------------------------------------------------------------------------------------------------
	//! True when an officer of this rank may switch a sheet to jail only.
	static bool CanOverride(int rank)
	{
		return LF_PoliceRanks.RankMeets(rank, OverrideMinRank());
	}

	//------------------------------------------------------------------------------------------------
	//! A copy of a code with its fine and time multiplied, so a repeat can win a stacking group.
	protected static LF_CrimeEntry Scaled(notnull LF_CrimeEntry code, int factor)
	{
		LF_CrimeEntry copy = LF_CrimeEntry.Make(code.m_sCrimeId, code.m_iWantedAdd, LF_Utils.ClampInt(code.m_iFine * factor, 0, FINE_LIMIT));
		copy.m_iSentenceSeconds = LF_Utils.ClampInt(code.m_iSentenceSeconds * factor, 0, 86400);
		copy.m_eLawSection = code.m_eLawSection;
		copy.m_sStackGroup = code.m_sStackGroup;
		copy.m_bJailOnly = code.m_bJailOnly;
		return copy;
	}

	//------------------------------------------------------------------------------------------------
	//! The charge sheet for an officer of a rank on the configured ladder: the cap is the rank's fine
	//! cap and the override needs `CanOverride(rank)`. See `EvaluateWith` for the rules.
	static LF_ChargeSheetResult Evaluate(notnull array<LF_CrimeEntry> codes, notnull array<string> repeatIds, int officerRank, bool officerOnDuty, bool jailOnlyOverride, string officerUid = "")
	{
		return EvaluateWith(codes, repeatIds, LF_PoliceRanks.FineCap(officerRank), officerOnDuty, jailOnlyOverride, CanOverride(officerRank), officerUid);
	}

	//------------------------------------------------------------------------------------------------
	//! The charge sheet with the cap and the override right given directly.
	//! \param[in] codes The ticked charges (catalog rows; null rows are skipped).
	//! \param[in] repeatIds Code ids the suspect was already charged with today; each counts double.
	//! \param[in] fineCap The officer's rank cap. Below 0 counts as 0.
	//! \param[in] officerOnDuty Only an on-duty officer signs.
	//! \param[in] jailOnlyOverride The officer asks to switch the sheet to jail only.
	//! \param[in] mayOverride The officer's rank allows the switch.
	//! \param[in] officerUid For the log line.
	static LF_ChargeSheetResult EvaluateWith(notnull array<LF_CrimeEntry> codes, notnull array<string> repeatIds, int fineCap, bool officerOnDuty, bool jailOnlyOverride, bool mayOverride, string officerUid = "")
	{
		LF_ChargeSheetResult result = new LF_ChargeSheetResult();
		if (!officerOnDuty)
		{
			result.m_eRefusal = LF_ChargeSheetRefusal.NOT_ON_DUTY;
			LF_Debug.Warn(FEATURE, string.Format("charge sheet refused: officer %1 is not on duty", officerUid));
			return result;
		}

		// Repeats are doubled before stacking, so a doubled lower code can beat a higher one.
		array<ref LF_CrimeEntry> keepAlive = {};
		array<LF_CrimeEntry> sheet = {};
		bool hasJailOnlyCode;
		array<string> seen = {};
		foreach (LF_CrimeEntry code : codes)
		{
			if (!code)
				continue;

			// A code ticked twice counts once: the desk builds this list from ids a client sends.
			if (seen.Contains(code.m_sCrimeId))
			{
				LF_Debug.Warn(FEATURE, string.Format("charge sheet: officer %1 ticked %2 twice, counted once", officerUid, code.m_sCrimeId));
				continue;
			}

			seen.Insert(code.m_sCrimeId);

			LF_CrimeEntry effective = code;
			if (repeatIds.Contains(code.m_sCrimeId))
			{
				effective = Scaled(code, REPEAT_FACTOR);
				keepAlive.Insert(effective);
				result.m_iRepeats++;
			}

			if (effective.m_bJailOnly)
				hasJailOnlyCode = true;

			sheet.Insert(effective);
		}

		if (sheet.IsEmpty())
		{
			result.m_eRefusal = LF_ChargeSheetRefusal.NO_CHARGES;
			LF_Debug.Warn(FEATURE, string.Format("charge sheet refused: officer %1 ticked no known charge", officerUid));
			return result;
		}

		int sentence = LF_Utils.ClampInt(LF_LawCodeRules.TotalSentenceSeconds(sheet), 0, MAX_SENTENCE_SECONDS);
		if (jailOnlyOverride)
		{
			if (!mayOverride)
			{
				result.m_eRefusal = LF_ChargeSheetRefusal.OVERRIDE_RANK;
				LF_Debug.Warn(FEATURE, string.Format("jail-only override refused: officer %1 is below the override rank", officerUid));
				return result;
			}

			if (sentence <= 0)
			{
				result.m_eRefusal = LF_ChargeSheetRefusal.OVERRIDE_NO_TIME;
				LF_Debug.Warn(FEATURE, string.Format("jail-only override refused: officer %1, no ticked charge carries time", officerUid));
				return result;
			}
		}

		array<LF_CrimeEntry> counted = {};
		LF_LawCodeRules.Counted(sheet, true, counted);
		int uncapped;
		foreach (LF_CrimeEntry winner : counted)
		{
			result.m_aCounted.Insert(winner);
			uncapped = LF_Utils.ClampInt(uncapped + winner.m_iFine, 0, FINE_LIMIT);
		}

		int cap = LF_Utils.ClampInt(fineCap, 0, FINE_LIMIT);
		result.m_iFineUncapped = uncapped;
		result.m_iFine = LF_Utils.ClampInt(uncapped, 0, cap);
		result.m_bCapped = result.m_iFine < uncapped;
		result.m_iSentenceSeconds = sentence;
		result.m_bJailOnly = hasJailOnlyCode;

		if (jailOnlyOverride)
		{
			result.m_bOverridden = true;
			result.m_bJailOnly = true;
			LF_Debug.Info(FEATURE, string.Format("jail-only override: officer %1 switched a $%2 sheet to %3 s in the cell", officerUid, result.m_iFine, sentence));
		}

		if (result.m_bJailOnly)
			result.m_iFine = 0;

		result.m_bFineOffered = !result.m_bJailOnly && result.m_iFine > 0;
		if (result.m_bCapped)
			LF_Debug.Info(FEATURE, string.Format("charge sheet: officer %1 rank cap $%2 cut the fine from $%3", officerUid, cap, uncapped));

		return result;
	}
}
