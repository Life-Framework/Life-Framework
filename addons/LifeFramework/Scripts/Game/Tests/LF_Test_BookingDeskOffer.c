// red-proof: RED-OK law/booking-desk-offer, observed 2026-11-23 with `tools\cli test --no-build --red LF_Test_BookingDeskOffer`: with the booking step skipped, `the offer went to the suspect (expected 'lf-test-desk-suspect', got '')`. Green: `--only` passed.
//! law/booking-desk-offer (#4420, police L5): an on-duty officer at the desk books a cuffed suspect
//! with two open reports; the offer's fine and time match LF_ChargeSheetRules for the officer's
//! server-side rank; the offer is keyed to the suspect only; a second booking while it is open is
//! refused in words; an off-duty or distant officer is refused and logged under [LFDebug:Law]; a
//! forged suspect id is refused.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_BookingDeskOffer : LF_Test
{
	protected static const string OFFICER_UID = "lf-test-desk-officer";
	protected static const string SUSPECT_UID = "lf-test-desk-suspect";

	override string GetName()
	{
		return "law/booking-desk-offer";
	}

	//------------------------------------------------------------------------------------------------
	//! Two catalog codes that carry a fine and are not jail only, so "pay now" is offered.
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

	override void Run(LF_TestContext ctx)
	{
		LF_BookingDesk.ResetStatics();
		LF_WantedReports.ClearFor(SUSPECT_UID);

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
		vector far = desk + Vector(LF_BookingDesk.DESK_METERS + 20, 0, 0);

		// Off duty: refused in words and logged under the Law feature.
		officer.SetOnDuty(false);
		ctx.EqualStr("#LF-BookingDesk_NotOnDuty", LF_BookingDesk.Offer(OFFICER_UID, atDesk, desk, SUSPECT_UID), "an off-duty officer cannot book");
		ctx.True(LF_BookingDesk.OfferFor(SUSPECT_UID) == null, "no offer from an off-duty officer");
		officer.SetOnDuty(true);

		// Away from the desk.
		ctx.EqualStr("#LF-BookingDesk_NotHere", LF_BookingDesk.Offer(OFFICER_UID, far, desk, SUSPECT_UID), "a distant officer cannot book");

		// A forged suspect id: nothing it names is a cuffed suspect beside the officer.
		IEntity officerBody = LF_TestCharacterFixture.Spawn(ctx);
		if (officerBody)
		{
			ctx.EqualStr("#LF-BookingDesk_NoSuspect", LF_BookingDesk.RequestBook(officerBody, officerBody.GetOrigin(), RplId.Invalid()), "an invalid suspect id is refused");
			ctx.EqualStr("#LF-BookingDesk_NoSuspect", LF_BookingDesk.RequestBook(officerBody, officerBody.GetOrigin(), Replication.FindId(officerBody)), "the officer's own id is refused");
			ctx.True(LF_BookingDesk.OfferFor(SUSPECT_UID) == null, "no offer from a forged id");
			LF_TestCharacterFixture.Delete(officerBody);
		}
		else
		{
			ctx.Fail("officer fixture did not spawn");
		}

		// The real booking.
		string refusal = "skipped";
		if (!RedProof())
			refusal = LF_BookingDesk.Offer(OFFICER_UID, atDesk, desk, SUSPECT_UID);

		ctx.EqualStr(string.Empty, refusal, "an on-duty officer at the desk books the suspect");
		LF_BookingOffer offer = LF_BookingDesk.OfferFor(SUSPECT_UID);
		ctx.NotNull(offer, "the suspect has an open offer");
		ctx.True(LF_BookingDesk.OfferFor(OFFICER_UID) == null, "the officer has no offer");
		ctx.EqualStr(SUSPECT_UID, LF_BookingDesk.LastOfferedTo(), "the offer went to the suspect");
		if (offer)
		{
			array<LF_CrimeEntry> codes = {};
			LF_LawCode.Resolve(ids, codes);
			array<string> none = {};
			LF_ChargeSheetResult expected = LF_ChargeSheetRules.Evaluate(codes, none, 0, true, false, OFFICER_UID);
			ctx.Equal(expected.m_iFine, offer.m_iFine, "the offer's fine is the sheet's fine for the officer's rank");
			ctx.Equal(expected.m_iSentenceSeconds, offer.m_iSheetSeconds, "the offer's time is the sheet's time");
			ctx.Equal(expected.m_bFineOffered, offer.m_bPayable, "pay now follows the sheet");
			ctx.Equal(2, offer.m_aCodes.Count(), "both open reports were charged");

			// "Take the time" serves what the label says: the jail's own arrest path at the offer's percent.
			IEntity suspectBody = LF_TestCharacterFixture.Spawn(ctx);
			if (suspectBody)
			{
				ctx.True(LF_Jail.Arrest(OFFICER_UID, SUSPECT_UID, suspectBody, offer.m_iPercent), "the jail takes the offer's booking");
				ctx.EqualFloat(offer.m_iServeSeconds, LF_Jail.GetRemaining(SUSPECT_UID), 0.5, "the time served is the time offered");
				ctx.True(LF_BookingDesk.OfferFor(SUSPECT_UID) == null, "a jailed suspect's offer is dropped");
				array<string> online = {SUSPECT_UID};
				LF_Jail.TickOnline(LF_Jail.GetRemaining(SUSPECT_UID) + 5, online);
				ctx.False(LF_Jail.IsServing(SUSPECT_UID), "the served suspect is released");
				LF_Jail.SyncPlacedCell();
				LF_TestCharacterFixture.Delete(suspectBody);
			}
			else
			{
				ctx.Fail("suspect fixture did not spawn");
			}
		}

		// A desk away from the jail cell opens no offer it cannot serve.
		vector farDesk = LF_Jail.GetCellOrigin() + Vector(LF_JailBooking.BOOKING_METERS + 40, 0, 0);
		ctx.EqualStr("#LF-JailBook_NotHere", LF_BookingDesk.Offer(OFFICER_UID, farDesk, farDesk, SUSPECT_UID), "a desk away from the cell is refused");

		// Reopen to prove a second booking is refused while an offer is open.
		suspect.SetWantedLevel(2);
		if (LF_WantedReports.CountFor(SUSPECT_UID) == 0)
		{
			foreach (string again : ids)
				LF_WantedReports.Add(SUSPECT_UID, again, "", "", "", true);
		}

		if (LF_BookingDesk.OfferFor(SUSPECT_UID) == null)
			LF_BookingDesk.Offer(OFFICER_UID, atDesk, desk, SUSPECT_UID);

		// A second booking while the offer is open.
		ctx.EqualStr("#LF-BookingDesk_OfferOpen", LF_BookingDesk.Offer(OFFICER_UID, atDesk, desk, SUSPECT_UID), "a second booking is refused while the offer is open");

		LF_BookingDesk.Close(SUSPECT_UID);
		ctx.True(LF_BookingDesk.OfferFor(SUSPECT_UID) == null, "closing drops the offer");

		LF_WantedReports.ClearFor(SUSPECT_UID);
		suspect.SetWantedLevel(0);
		officer.SetOnDuty(false);
		LF_BookingDesk.ResetStatics();
	}
}
