// red-proof: observed 2026-11-24 with `tools/cli test --only LF_Test_EveronBookingJailOnly` on the unfixed LF_BookingDesk.Offer: `the jail-only code is on the offer, so its licence ban can be booked` and `murder's rule takes the firearm permit and the driving licence (expected 2, got 0)`. Fixed: the same run passed.
//! everon/booking-jail-only: under the Everon 1989 header (the pack leg) murder is a jail-only code. The signed
//! sheet's code list is what the licence seizure reads (#4428), so a jail-only code that rules a licence ban
//! (murder takes the firearm permit and the driving licence) has to be on the offer. It was counted by fine,
//! which skips jail-only codes, so the cell path of a murder sheet seized nothing (found by the island
//! booking-desk session, 2026-11-24, #4699).
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronBookingJailOnly : LF_Test
{
	protected static const string OFFICER_UID = "lf-test-jailonly-officer";
	protected static const string SUSPECT_UID = "lf-test-jailonly-suspect";
	protected static const string MURDER = "#LF-Offence_Murder";

	override string GetName()
	{
		return "everon/booking-jail-only";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_BookingDesk.ResetStatics();
		LF_WantedReports.ClearFor(SUSPECT_UID);

		array<string> ids = {MURDER};
		array<LF_CrimeEntry> rows = {};
		LF_LawCode.Resolve(ids, rows);
		if (rows.IsEmpty() || !rows[0].m_bJailOnly)
		{
			ctx.Fail("the pack's catalog has no jail-only murder row to test with");
			return;
		}

		LF_PlayerAccount officer = LF_PlayerAccountManager.GetOrCreate(OFFICER_UID);
		officer.SetFaction(LF_Faction.POLICE);
		officer.SetOnDuty(true);
		officer.SetPoliceRank(0);
		LF_PlayerAccount suspect = LF_PlayerAccountManager.GetOrCreate(SUSPECT_UID);
		suspect.SetWantedLevel(3);
		LF_WantedReports.Add(SUSPECT_UID, MURDER, "", "", "", true);

		if (!LF_JailBooking.IsCellKnown() || !LF_Jail.HasCell())
		{
			ctx.Fail("the test world has no jail cell for the desk to serve");
			return;
		}

		LF_Jail.Configure(60, 300, 1, 8);
		vector desk = LF_Jail.GetCellOrigin() + Vector(2, 0, 0);
		vector atDesk = desk + Vector(1, 0, 0);
		string refusal = "skipped";
		if (!RedProof())
			refusal = LF_BookingDesk.Offer(OFFICER_UID, atDesk, desk, SUSPECT_UID);

		ctx.EqualStr(string.Empty, refusal, "the murder sheet is signed");
		LF_BookingOffer offer = LF_BookingDesk.OfferFor(SUSPECT_UID);
		ctx.NotNull(offer, "the suspect has an open offer");
		if (offer)
		{
			ctx.False(offer.m_bPayable, "a jail-only sheet offers no fine");
			ctx.True(offer.m_aCodes.Contains(MURDER), "the jail-only code is on the offer, so its licence ban can be booked");
			array<ref LF_LicenceBan> bans = {};
			LF_LicenceSeizureRules.BansFor(offer.m_aCodes, bans);
			ctx.Equal(2, bans.Count(), "murder's rule takes the firearm permit and the driving licence");
		}

		LF_BookingDesk.ResetStatics();
		LF_WantedReports.ClearFor(SUSPECT_UID);
		suspect.SetWantedLevel(0);
	}
}
