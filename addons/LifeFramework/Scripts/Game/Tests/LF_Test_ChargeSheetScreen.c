// red-proof: RED-OK law/charge-sheet-screen, observed 2026-11-23 with `tools/cli test --no-build --red LF_Test_ChargeSheetScreen`: with the hook on nothing is ticked on the paper and the signature is skipped, so the rows, greyed rows, totals, the offer and `a signed sheet makes the booking offer (expected '', got 'skipped')` all fail. Green: `--only` passed. Row status (playtest 2026-11-25): on the unfixed code `--only LF_Test_ChargeSheetScreen` failed the two new assertions (got 'Over cap'); green after the fix. Row text fit (#5081, 2026-11-27): with the row status `%1, above your rank's limit` (29 characters) `--only LF_Test_ChargeSheetScreen` failed `the status fits the row's narrow status column ... got 29 characters` and the four-figure line (30); with `%1, over your limit` it passes.
//! law/charge-sheet-screen (#4426, police L4): the rows, greyed rows and total the screen model shows
//! equal LF_ChargeSheetRules for a fixture sheet (stacked and over-cap rows grey); Sign takes the ticked
//! codes only, drops an id the suspect has no open report for, and the booking offer is the server's
//! recompute: a forged total, rank or code changes nothing.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_ChargeSheetScreen : LF_Test
{
	protected static const string OFFICER_UID = "lf-test-sheet-officer";
	protected static const string SUSPECT_UID = "lf-test-sheet-suspect";
	protected static const string GROUP = "lf-test-sheet-group";

	override string GetName()
	{
		return "law/charge-sheet-screen";
	}

	//------------------------------------------------------------------------------------------------
	protected LF_CrimeEntry Code(string id, int fine, int seconds, string group)
	{
		LF_CrimeEntry code = LF_CrimeEntry.Make(id, 0, fine);
		code.m_iSentenceSeconds = seconds;
		code.m_sStackGroup = group;
		return code;
	}

	//------------------------------------------------------------------------------------------------
	protected LF_ChargeSheetRow RowOf(notnull LF_ChargeSheetModel model, string id)
	{
		foreach (LF_ChargeSheetRow row : model.m_aRows)
		{
			if (row.m_sCodeId == id)
				return row;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Two catalog codes that carry a fine, are not jail only and stack with nothing.
	protected bool PickCodes(notnull array<string> outIds)
	{
		LF_CrimeCatalogConfig config = LF_CrimeCatalogConfig.Get();
		if (!config || !config.m_aCrimes)
			return false;

		foreach (LF_CrimeEntry entry : config.m_aCrimes)
		{
			if (!entry || entry.m_sCrimeId.IsEmpty() || entry.m_bJailOnly || entry.m_iFine <= 0 || !entry.m_sStackGroup.IsEmpty())
				continue;

			outIds.Insert(entry.m_sCrimeId);
			if (outIds.Count() == 2)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected void RunModel(LF_TestContext ctx)
	{
		// A fixture sheet: A and B share a stacking group (the higher, A, counts), C stands alone.
		LF_CrimeEntry a = Code("lf-test-sheet-a", 300, 600, GROUP);
		LF_CrimeEntry b = Code("lf-test-sheet-b", 200, 300, GROUP);
		LF_CrimeEntry c = Code("lf-test-sheet-c", 150, 180, "");
		array<LF_CrimeEntry> open = {a, b, c};
		array<string> all = {a.m_sCrimeId, b.m_sCrimeId, c.m_sCrimeId};
		array<string> none = {};

		// RedProof: nothing is ticked on the paper, so every row, total and greyed-row assertion fails.
		array<string> ticks = all;
		if (RedProof())
			ticks = none;

		LF_ChargeSheetModel model = LF_ChargeSheetModel.Build(open, ticks, 500);
		LF_ChargeSheetResult rules = LF_ChargeSheetRules.EvaluateWith(open, none, 500, true, false, false);
		ctx.Equal(3, model.m_aRows.Count(), "one row per open code");
		ctx.Equal(rules.m_iFine, model.m_iFine, "the foot's fine is the rules' fine");
		ctx.Equal(rules.m_iSentenceSeconds, model.m_iSeconds, "the foot's time is the rules' time");
		ctx.Equal(rules.m_bFineOffered, model.m_bPayable, "pay now follows the rules");
		ctx.True(model.m_bSignable, "a ticked sheet can be signed");
		ctx.Equal(LF_EChargeRow.TICKED, RowOf(model, a.m_sCrimeId).m_eState, "the higher code of the group counts");
		ctx.Equal(LF_EChargeRow.STACKED, RowOf(model, b.m_sCrimeId).m_eState, "the lower code of the group is greyed as stacked");
		ctx.True(RowOf(model, b.m_sCrimeId).IsGreyed(), "a stacked row is greyed");
		ctx.Equal(LF_EChargeRow.TICKED, RowOf(model, c.m_sCrimeId).m_eState, "a code that stacks with nothing counts");
		ctx.EqualStr(string.Empty, LF_ChargeSheetScreen.RowAction(RowOf(model, b.m_sCrimeId)), "a greyed row has no button");
		ctx.EqualStr(LF_Terms.Text("#LF-ChargeSheet_Untick"), LF_ChargeSheetScreen.RowAction(RowOf(model, a.m_sCrimeId)), "a counted row can be unticked");

		// The rank cap cuts the fine, and the row that carries it past the cap is greyed.
		LF_ChargeSheetModel capped = LF_ChargeSheetModel.Build(open, all, 400);
		LF_ChargeSheetResult cappedRules = LF_ChargeSheetRules.EvaluateWith(open, none, 400, true, false, false);
		ctx.True(cappedRules.m_bCapped, "the fixture sheet is capped at 400");
		ctx.Equal(cappedRules.m_iFine, capped.m_iFine, "a capped total is the rules' capped total");
		ctx.True(capped.m_bCapped, "the model shows the cap cut the fine");
		ctx.Equal(LF_EChargeRow.OVER_CAP, RowOf(capped, c.m_sCrimeId).m_eState, "the row past the cap is greyed");
		string cappedStatus = LF_ChargeSheetScreen.RowStatus(RowOf(capped, c.m_sCrimeId), 400);
		ctx.True(cappedStatus.Contains("150"), "the greyed row shows its own fine, not a bare 'Over cap' (got '" + cappedStatus + "')");
		ctx.EqualStr(LF_Terms.Text("#LF-ChargeSheet_OverCap", LF_Terms.Text("#LF-ChargeSheet_Money", "150")), cappedStatus, "and says it is over the limit, with the money text (got '" + cappedStatus + "')");
		ctx.True(cappedStatus.Contains("$150"), "the greyed row's fine has its dollar sign (got '" + cappedStatus + "')");
		ctx.True(cappedStatus.Length() <= 24, "the status fits the row's narrow status column, which clipped 'above your rank's limit' mid-word (#5081; got " + cappedStatus.Length() + " characters: '" + cappedStatus + "')");

		// A four-figure fine still fits.
		string bigStatus = LF_Terms.Text("#LF-ChargeSheet_OverCap", LF_Terms.Text("#LF-ChargeSheet_Money", "9999"));
		ctx.True(bigStatus.Length() <= 25, "a four-figure fine still fits the status column (got " + bigStatus.Length() + " characters: '" + bigStatus + "')");

		// Untick A: B now counts. Untick all: nothing to sign.
		array<string> noA = {b.m_sCrimeId, c.m_sCrimeId};
		LF_ChargeSheetModel swapped = LF_ChargeSheetModel.Build(open, noA, 500);
		ctx.Equal(LF_EChargeRow.TICKED, RowOf(swapped, b.m_sCrimeId).m_eState, "with A unticked, B counts");
		ctx.Equal(LF_EChargeRow.OPEN, RowOf(swapped, a.m_sCrimeId).m_eState, "an unticked row is open");
		LF_ChargeSheetModel empty = LF_ChargeSheetModel.Build(open, none, 500);
		ctx.False(empty.m_bSignable, "nothing ticked: nothing to sign");
		ctx.Equal(0, empty.m_iFine, "nothing ticked: no fine");

		// What Sign sends: the ticked ids, never a total.
		array<string> sent = {};
		model.TickedIds(sent);
		ctx.Equal(3, sent.Count(), "Sign sends the ticked ids, stacked ones included, so the server can show the same paper");
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_BookingDesk.ResetStatics();
		LF_ChargeSheetDesk.ResetStatics();
		LF_WantedReports.ClearFor(SUSPECT_UID);
		RunModel(ctx);

		array<string> ids = {};
		if (!PickCodes(ids))
		{
			ctx.Fail("the crime catalog has no two plain fine codes");
			return;
		}

		LF_PlayerAccount officer = LF_PlayerAccountManager.GetOrCreate(OFFICER_UID);
		officer.SetFaction(LF_Faction.POLICE);
		officer.SetOnDuty(true);
		officer.SetPoliceRank(0);
		LF_PlayerAccount suspect = LF_PlayerAccountManager.GetOrCreate(SUSPECT_UID);
		suspect.SetWantedLevel(2);
		foreach (string id : ids)
			LF_WantedReports.Add(SUSPECT_UID, id, "", "", "", true);

		if (!LF_JailBooking.IsCellKnown() || !LF_Jail.HasCell())
		{
			ctx.Fail("the test world has no jail cell for the desk to serve");
			return;
		}

		LF_Jail.Configure(60, 300, 1, 8);
		vector desk = LF_Jail.GetCellOrigin() + Vector(2, 0, 0);
		vector atDesk = desk + Vector(1, 0, 0);

		// No sheet open: a signature is refused in words.
		ctx.EqualStr("#LF-ChargeSheet_NoSheet", LF_ChargeSheetDesk.Sign(OFFICER_UID, atDesk, ids[0], 0, null, null), "no open sheet, no signature");

		// Open: the codes and the rank cap are the server's.
		int cap;
		string sheetIds;
		ctx.EqualStr(string.Empty, LF_ChargeSheetDesk.Prepare(OFFICER_UID, desk, SUSPECT_UID, null, cap, sheetIds), "an on-duty officer opens the sheet");
		ctx.Equal(LF_PoliceRanks.FineCap(0), cap, "the cap on the paper is the officer's server-side rank cap");
		array<string> shown = {};
		sheetIds.Split(",", shown, true);
		ctx.Equal(2, shown.Count(), "both open reports are on the paper");

		// Sign with only the first code plus a code the suspect never had: the extra is dropped.
		array<LF_CrimeEntry> one = {};
		array<string> oneId = {ids[0]};
		LF_LawCode.Resolve(oneId, one);
		array<string> noRepeats = {};
		LF_ChargeSheetResult expected = LF_ChargeSheetRules.Evaluate(one, noRepeats, 0, true, false, OFFICER_UID);

		string refusal = "skipped";
		if (!RedProof())
			refusal = LF_ChargeSheetDesk.Sign(OFFICER_UID, atDesk, ids[0] + ",lf-test-forged-code", 999999, null, null);

		ctx.EqualStr(string.Empty, refusal, "a signed sheet makes the booking offer");
		LF_BookingOffer offer = LF_BookingDesk.OfferFor(SUSPECT_UID);
		ctx.NotNull(offer, "the suspect has an open offer");
		if (offer)
		{
			ctx.Equal(1, offer.m_aCodes.Count(), "only the ticked, open code is charged");
			ctx.Equal(expected.m_iFine, offer.m_iFine, "the offer's fine is the rules' fine, not the forged total");
			ctx.Equal(expected.m_iSentenceSeconds, offer.m_iSheetSeconds, "the offer's time is the rules' time");

			LF_ChargeSheetModel preview = LF_ChargeSheetModel.Build(one, oneId, cap);
			ctx.Equal(preview.m_iFine, offer.m_iFine, "the paper's total and the offer agree");
		}

		ctx.True(LF_ChargeSheetDesk.PendingFor(OFFICER_UID) == null, "a signed sheet is used up");
		LF_BookingDesk.Close(SUSPECT_UID);

		// Only forged ids: nothing is charged.
		LF_ChargeSheetDesk.Prepare(OFFICER_UID, desk, SUSPECT_UID, null, cap, sheetIds);
		ctx.EqualStr("#LF-ChargeSheet_NoneTicked", LF_ChargeSheetDesk.Sign(OFFICER_UID, atDesk, "lf-test-forged-code", 1, null, null), "a signature naming no open code is refused");
		ctx.True(LF_BookingDesk.OfferFor(SUSPECT_UID) == null, "a refused signature makes no offer");

		// An off-duty officer's sheet is never made.
		officer.SetOnDuty(false);
		ctx.EqualStr("#LF-BookingDesk_NotOnDuty", LF_ChargeSheetDesk.Prepare(OFFICER_UID, desk, SUSPECT_UID, null, cap, sheetIds), "an off-duty officer cannot open a sheet");
		officer.SetOnDuty(true);

		LF_BookingDesk.Close(SUSPECT_UID);
		LF_WantedReports.ClearFor(SUSPECT_UID);
		suspect.SetWantedLevel(0);
		officer.SetOnDuty(false);
		LF_BookingDesk.ResetStatics();
		LF_ChargeSheetDesk.ResetStatics();
	}
}
