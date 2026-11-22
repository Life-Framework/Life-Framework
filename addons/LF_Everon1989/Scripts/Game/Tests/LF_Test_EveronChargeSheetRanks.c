// red-proof: RedProof() swaps in the built-in two-rank ladder (no `sergeant` row), so `the ladder has a sergeant row` and every override assertion fail (run `tools\cli test --no-build --red LF_Test_EveronChargeSheetRanks`).
//! #4284 (follow-up to #4209): under the Everon 1989 header (the pack leg of `tools\cli test`) the police
//! ladder has a `sergeant` row at rank 3, so a Sergeant and up may switch a charge sheet to jail only,
//! while rank 2 (senior patrol) is refused. The core ladder has no `sergeant` row, so a renamed or
//! dropped pack row is caught here and not by `LF_Test_ChargeSheetRules`.
[LF_TestInfo(LF_TestTier.LOGIC, pack: true)]
class LF_Test_EveronChargeSheetRanks : LF_Test
{
	protected static const string SERGEANT_ID = "sergeant";
	protected static const int SERGEANT_RANK = 3;
	protected static const string THEFT_ID = "test-everon-sheet-theft";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/charge-sheet-ranks";
	}

	//------------------------------------------------------------------------------------------------
	//! The ladder index of a row id, or -1.
	protected int RankIndex(string id)
	{
		array<ref LF_PoliceRank> ranks = {};
		LF_PoliceConfig.Ranks(ranks);
		foreach (int i, LF_PoliceRank rank : ranks)
		{
			if (rank && rank.m_sId == id)
				return i;
		}

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	//! A one-code sheet with time in the cell, so the override has something to switch to.
	protected LF_ChargeSheetResult Sign(int rank)
	{
		LF_CrimeEntry theft = LF_CrimeEntry.Make(THEFT_ID, 0, 200);
		theft.m_iSentenceSeconds = 300;
		array<LF_CrimeEntry> codes = {theft};
		array<string> none = {};
		return LF_ChargeSheetRules.Evaluate(codes, none, rank, true, true, "test-officer");
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		// The act: the running header's overlays layered over the core ladder.
		LF_PoliceConfig.Reload();
		if (RedProof())
			LF_PoliceConfig.UseForTest(null);

		int sergeant = RankIndex(SERGEANT_ID);
		ctx.Equal(SERGEANT_RANK, sergeant, "the Everon ladder has a sergeant row at rank 3");
		ctx.True(LF_PoliceRanks.RankCount() > SERGEANT_RANK + 1, "the ladder keeps a rung above the sergeant");
		ctx.Equal(SERGEANT_RANK, LF_ChargeSheetRules.OverrideMinRank(), "the jail-only override starts at the sergeant row");

		// Rank 3 and up may switch a sheet to jail only; rank 2 and below may not.
		ctx.True(LF_ChargeSheetRules.CanOverride(SERGEANT_RANK), "a sergeant may use the jail-only override");
		ctx.True(LF_ChargeSheetRules.CanOverride(LF_PoliceRanks.TopRank()), "the top rank may use the jail-only override");
		ctx.False(LF_ChargeSheetRules.CanOverride(SERGEANT_RANK - 1), "rank 2 may not use the jail-only override");
		ctx.False(LF_ChargeSheetRules.CanOverride(LF_PoliceRanks.PATROL), "a patrol officer may not use the jail-only override");

		// The same through a signed sheet: the sergeant's switch is signed and logged, rank 2's is refused.
		LF_ChargeSheetResult sheet = Sign(SERGEANT_RANK);
		ctx.True(sheet.IsSigned(), "a sergeant's jail-only sheet is signed");
		ctx.True(sheet.m_bOverridden, "the sergeant's sheet records the override");
		ctx.Equal(0, sheet.m_iFine, "the sergeant's jail-only sheet asks no fine");
		ctx.Equal(300, sheet.m_iSentenceSeconds, "the sergeant's jail-only sheet keeps the time");

		sheet = Sign(SERGEANT_RANK - 1);
		ctx.True(sheet.m_eRefusal == LF_ChargeSheetRefusal.OVERRIDE_RANK, "rank 2's jail-only switch is refused");
		ctx.False(sheet.m_bOverridden, "a refused switch changes nothing");

		LF_PoliceConfig.Reload();
	}
}
