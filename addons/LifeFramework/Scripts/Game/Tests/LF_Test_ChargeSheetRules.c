// red-proof: observed 2026-11-23 with `tools/cli test --no-build --red LF_Test_ChargeSheetRules`: RED-OK, RedProof() returns the raw sum of fines and times with no rules, so stacking, repeat doubling, the rank cap, jail only, the sentence cap, the override and the refusals all fail. Green with the hook off.
//! #4209: a charge sheet stacks by the law book, doubles a repeat, caps the fine by rank ($500 patrol to
//! $5,000 lieutenant in the pack), drops "pay now" for a jail-only code, and lets only a Sergeant and up
//! switch a sheet to jail only; an off-duty officer, an empty sheet and an override with no time refuse.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_ChargeSheetRules : LF_Test
{
	protected static const string GROUP_SPEED = "test-sheet-speed";
	protected static const int CAP_PATROL = 500;
	protected static const int CAP_LIEUTENANT = 5000;

	override string GetName()
	{
		return "law/charge-sheet-rules";
	}

	//------------------------------------------------------------------------------------------------
	protected LF_CrimeEntry Code(string id, int fine, int sentenceSeconds, string group, bool jailOnly)
	{
		LF_CrimeEntry code = LF_CrimeEntry.Make(id, 0, fine);
		code.m_iSentenceSeconds = sentenceSeconds;
		code.m_sStackGroup = group;
		code.m_bJailOnly = jailOnly;
		return code;
	}

	//------------------------------------------------------------------------------------------------
	protected LF_ChargeSheetResult Sheet(notnull array<LF_CrimeEntry> codes, int cap, array<string> repeats = null, bool overrideJail = false, bool mayOverride = false, bool onDuty = true)
	{
		array<string> given = {};
		if (repeats)
			given.Copy(repeats);

		// RedProof: the rules are skipped and the raw sum comes back, so every rule assertion fails.
		if (RedProof())
		{
			LF_ChargeSheetResult raw = new LF_ChargeSheetResult();
			foreach (LF_CrimeEntry c : codes)
			{
				raw.m_iFine += c.m_iFine;
				raw.m_iSentenceSeconds += c.m_iSentenceSeconds;
			}

			raw.m_bFineOffered = true;
			return raw;
		}

		return LF_ChargeSheetRules.EvaluateWith(codes, given, cap, onDuty, overrideJail, mayOverride, "test-officer");
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_CrimeEntry speedLow = Code("test-sheet-speed-low", 75, 0, GROUP_SPEED, false);
		LF_CrimeEntry speedHigh = Code("test-sheet-speed-high", 150, 0, GROUP_SPEED, false);
		LF_CrimeEntry theft = Code("test-sheet-theft", 200, 300, "", false);
		LF_CrimeEntry reckless = Code("test-sheet-reckless", 250, 0, "", false);
		LF_CrimeEntry robbery = Code("test-sheet-robbery", 0, 900, "", true);
		LF_CrimeEntry vinTamper = Code("test-sheet-vin", 600, 600, "", false);
		LF_CrimeEntry hostage = Code("test-sheet-hostage", 0, 1500, "", true);
		LF_CrimeEntry murder = Code("test-sheet-murder", 0, 1800, "", true);

		// Stacking: one group counts once (the higher), the rest add.
		array<LF_CrimeEntry> codes = {speedLow, speedHigh, theft};
		LF_ChargeSheetResult r = Sheet(codes, CAP_LIEUTENANT);
		ctx.Equal(350, r.m_iFine, "stacking: the speeding group counts its higher code once, theft adds");
		ctx.Equal(300, r.m_iSentenceSeconds, "stacking: the time is the theft's alone");
		ctx.True(r.m_bFineOffered, "stacking: a sheet with no jail-only code offers pay now");
		ctx.True(!r.m_bCapped, "stacking: under the cap nothing is cut");

		// Repeat: a code already charged today counts double, and the doubled code can win its group.
		array<string> repeats = {"test-sheet-speed-low", "test-sheet-theft"};
		r = Sheet(codes, CAP_LIEUTENANT, repeats);
		ctx.Equal(550, r.m_iFine, "repeat: the doubled low speeding ($150) ties the high one, theft doubles to $400");
		ctx.Equal(600, r.m_iSentenceSeconds, "repeat: the theft's time doubles");
		ctx.Equal(2, r.m_iRepeats, "repeat: two ticked codes were doubled");
		ctx.Equal(200, theft.m_iFine, "repeat: the catalog row is not changed");
		repeats = {"test-sheet-speed-low"};
		array<LF_CrimeEntry> speeding = {speedLow, speedHigh};
		LF_CrimeEntry speedLowMore = Code("test-sheet-speed-low", 100, 0, GROUP_SPEED, false);
		array<LF_CrimeEntry> speedingMore = {speedLowMore, speedHigh};
		r = Sheet(speedingMore, CAP_LIEUTENANT, repeats);
		ctx.Equal(200, r.m_iFine, "repeat: a doubled low code ($200) beats the high one ($150) in its group");

		// Rank cap: patrol $500, lieutenant $5,000.
		array<LF_CrimeEntry> big = {theft, reckless, vinTamper};
		r = Sheet(big, CAP_PATROL);
		ctx.Equal(1050, r.m_iFineUncapped, "cap: the uncapped total is the sum");
		ctx.Equal(CAP_PATROL, r.m_iFine, "cap: a patrol officer's sheet asks at most $500");
		ctx.True(r.m_bCapped, "cap: the patrol cap cut the fine");
		r = Sheet(big, CAP_LIEUTENANT);
		ctx.Equal(1050, r.m_iFine, "cap: a lieutenant writes the full $1,050");
		repeats = {"test-sheet-theft", "test-sheet-reckless", "test-sheet-vin"};
		LF_CrimeEntry flying = Code("test-sheet-flying", 800, 0, "", false);
		LF_CrimeEntry weapon = Code("test-sheet-weapon", 800, 0, "", false);
		LF_CrimeEntry hitRun = Code("test-sheet-hitrun", 600, 600, "", false);
		LF_CrimeEntry extra = Code("test-sheet-extra", 1000, 0, "", false);
		array<LF_CrimeEntry> huge = {theft, reckless, vinTamper, flying, weapon, hitRun};
		r = Sheet(huge, CAP_LIEUTENANT, repeats);
		ctx.Equal(4300, r.m_iFineUncapped, "cap: the doubled sheet sums to $4,300 before the cap");
		huge.Insert(extra);
		r = Sheet(huge, CAP_LIEUTENANT, repeats);
		ctx.Equal(CAP_LIEUTENANT, r.m_iFine, "cap: even a lieutenant's sheet stops at $5,000");
		r = Sheet(big, -5);
		ctx.Equal(0, r.m_iFine, "cap: a negative cap counts as $0");

		// A jail-only code removes pay now; its time counts.
		array<LF_CrimeEntry> withRobbery = {theft, robbery};
		r = Sheet(withRobbery, CAP_LIEUTENANT);
		ctx.True(!r.m_bFineOffered, "jail only: a robbery on the sheet removes pay now");
		ctx.True(r.m_bJailOnly, "jail only: the sheet is jail only");
		ctx.Equal(0, r.m_iFine, "jail only: the sheet asks no fine");
		ctx.Equal(1200, r.m_iSentenceSeconds, "jail only: robbery and theft time add");

		// Sentence cap: 60 minutes.
		array<LF_CrimeEntry> longSheet = {murder, hostage, robbery};
		r = Sheet(longSheet, CAP_LIEUTENANT);
		ctx.Equal(LF_ChargeSheetRules.MAX_SENTENCE_SECONDS, r.m_iSentenceSeconds, "sentence: 70 minutes of charges stop at 60");

		// Override: Sergeant and up switches to jail only, logged; below that it is refused.
		array<LF_CrimeEntry> fineable = {theft, reckless};
		r = Sheet(fineable, CAP_LIEUTENANT, null, true, true);
		ctx.True(r.IsSigned(), "override: a sergeant's switch is signed");
		ctx.True(r.m_bOverridden, "override: the sheet records the switch");
		ctx.True(!r.m_bFineOffered, "override: pay now is gone");
		ctx.Equal(0, r.m_iFine, "override: the fine drops to 0");
		ctx.Equal(300, r.m_iSentenceSeconds, "override: the theft's time stays");
		r = Sheet(fineable, CAP_LIEUTENANT, null, true, false);
		ctx.True(r.m_eRefusal == LF_ChargeSheetRefusal.OVERRIDE_RANK, "override: a patrol officer's switch is refused");
		ctx.True(!r.m_bOverridden, "override: a refused switch changes nothing");
		r = Sheet(speeding, CAP_LIEUTENANT, null, true, true);
		ctx.True(r.m_eRefusal == LF_ChargeSheetRefusal.OVERRIDE_NO_TIME, "override: a sheet with no time cannot go to the cell");

		// Who signs: only on duty; something must be ticked.
		r = Sheet(fineable, CAP_LIEUTENANT, null, false, false, false);
		ctx.True(r.m_eRefusal == LF_ChargeSheetRefusal.NOT_ON_DUTY, "signing: an off-duty officer cannot sign");
		array<LF_CrimeEntry> empty = {};
		r = Sheet(empty, CAP_LIEUTENANT);
		ctx.True(r.m_eRefusal == LF_ChargeSheetRefusal.NO_CHARGES, "signing: an empty sheet is refused");
		array<LF_CrimeEntry> twice = {theft, theft};
		r = Sheet(twice, CAP_LIEUTENANT);
		ctx.Equal(200, r.m_iFine, "signing: a code ticked twice counts once");
		ctx.Equal(300, r.m_iSentenceSeconds, "signing: a code ticked twice adds its time once");
		array<string> theftRepeat = {"test-sheet-theft"};
		r = Sheet(twice, CAP_LIEUTENANT, theftRepeat);
		ctx.Equal(400, r.m_iFine, "signing: a repeat ticked twice is doubled once");

		// The ladder wrapper: patrol's cap comes from the configured ladder, and patrol cannot override.
		array<string> none = {};
		r = LF_ChargeSheetRules.Evaluate(big, none, LF_PoliceRanks.PATROL, true, false, "test-officer");
		if (RedProof())
			r.m_iFine = -1;
		ctx.Equal(LF_Utils.MinInt(1050, LF_PoliceRanks.FineCap(LF_PoliceRanks.PATROL)), r.m_iFine, "ladder: the patrol rank's own cap applies");
		ctx.True(!LF_ChargeSheetRules.CanOverride(LF_PoliceRanks.PATROL) || LF_PoliceRanks.TopRank() == LF_PoliceRanks.PATROL, "ladder: patrol is below the override rank");
		ctx.True(LF_ChargeSheetRules.CanOverride(LF_PoliceRanks.TopRank()), "ladder: the top rank may override");
	}
}
