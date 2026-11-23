// red-proof: `tools\cli test --no-build --red LF_Test_PoliceCallPay` (2026-11-18): RED PROOF OK, with the Settle skipped:
// "the call is completed", "a completed call forgets its site", "a second fine pays nothing (expected 0, got 150)".
// #4319 (the $150 cash reward retired for the $15 duty bonus): 2026-11-23 `--red LF_Test_DutyBonusWiring,LF_Test_DutyBonusPersist,LF_Test_PoliceCallPay`:
// RED PROOF OK, with the Settle skipped: "the completed call earns the $15 duty bonus (expected 15, got 0)".
//! Police call pay (#2478, parent #234; bonus since #4319): a fine by another officer or outside the radius
//! completes nothing; the holder's fine inside the radius completes the call and earns the officer the $15
//! resolved-call duty bonus once, paid at clock-out, with no cash on the spot. The site comes from the police
//! service call that follows the wanted change (a crime's real order).
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_PoliceCallPay : LF_Test
{
	protected static const string SUSPECT = "police-pay-suspect";
	protected static const string SUSPECT_2 = "police-pay-suspect-2";
	protected static const string OFFICER_A = "police-pay-officer-a";
	protected static const string OFFICER_B = "police-pay-officer-b";

	override string GetName()
	{
		return "police/call-pay";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_PoliceCalls.ResetStatics();
		LF_PoliceCallPay.ResetStatics();
		LF_WantedReports.ResetStatics();
		LF_DutyBonus.ResetStatics();

		LF_PlayerAccount officerA = OnDutyOfficer(OFFICER_A);
		LF_PlayerAccount officerB = OnDutyOfficer(OFFICER_B);
		int senior = LF_AccountStateBits.Encode(true, LF_Faction.POLICE, true, 0, LF_PoliceRanks.SENIOR);
		LF_PoliceCall call = LF_PoliceCalls.Post(LF_PoliceCalls.KIND_FINE, SUSPECT, "test site", LF_PoliceRanks.PATROL);
		ctx.True(call != null, "a call is posted");
		if (!call)
			return;

		ctx.True(LF_PoliceCalls.Take(call.m_iId, OFFICER_A, senior), "officer A takes the call");

		// A crime's order: the wanted level rises first, the report and its police service call follow.
		vector site = Vector(1000, 20, 2000);
		LF_PoliceCallPay.OnWantedChanged(LF_OnWantedChanged.Create(SUSPECT, 0, 2));
		LF_WantedReports.Add(SUSPECT, LF_WantedReports.OFFENCE_ROBBERY, "", "", "test site", false);
		LF_PoliceCallPay.OnServiceCall(LF_OnServiceCall.Create(SUSPECT, "police", "test site", LF_WantedReports.OFFENCE_ROBBERY, "radio", site, {}));
		ctx.True(LF_PoliceCallPay.HasSite(SUSPECT), "the police service call keeps the site");

		float radius = LF_PoliceCallPay.Radius();
		ctx.True(radius > 0, "the call radius is positive");

		vector inside = site + Vector(radius * 0.5, 0, 0);
		vector outside = site + Vector(radius + 5, 0, 0);
		int bonus;

		ctx.False(LF_PoliceCallPay.Settle(OFFICER_B, SUSPECT, inside, bonus), "a fine by another officer completes nothing");
		ctx.Equal(0, LF_DutyBonus.Pending(officerB), "the other officer earns no bonus");
		ctx.False(call.m_bClosed, "a fine by another officer leaves the call open");

		ctx.False(LF_PoliceCallPay.Settle(OFFICER_A, SUSPECT, outside, bonus), "the holder's fine outside the radius completes nothing");
		ctx.False(call.m_bClosed, "a fine outside the radius leaves the call open");
		ctx.Equal(0, LF_DutyBonus.Pending(officerA), "a fine that completes nothing earns nothing");

		bool completed;
		if (!RedProof())
			completed = LF_PoliceCallPay.Settle(OFFICER_A, SUSPECT, inside, bonus);

		ctx.True(completed, "the holder's fine inside the radius completes the call");
		ctx.Equal(LF_DutyBonus.CALL_RESOLVED_AMOUNT, bonus, "the completed call earns the $15 duty bonus");
		ctx.Equal(LF_DutyBonus.CALL_RESOLVED_AMOUNT, LF_DutyBonus.Pending(officerA), "the bonus waits for clock-out");
		ctx.True(call.m_bClosed, "the call is completed");
		ctx.False(LF_PoliceCallPay.HasSite(SUSPECT), "a completed call forgets its site");

		ctx.False(LF_PoliceCallPay.Settle(OFFICER_A, SUSPECT, inside, bonus), "a second fine completes nothing");
		ctx.Equal(0, bonus, "a second fine earns nothing");
		ctx.Equal(LF_DutyBonus.CALL_RESOLVED_AMOUNT, LF_DutyBonus.Pending(officerA), "the bonus is earned once");

		// A second suspect: a cleared wanted level forgets the site and an unreported suspect completes nothing.
		LF_PoliceCall second = LF_PoliceCalls.Post(LF_PoliceCalls.KIND_FINE, SUSPECT_2, "test site", LF_PoliceRanks.PATROL);
		ctx.True(second != null && LF_PoliceCalls.Take(second.m_iId, OFFICER_A, senior), "officer A takes a second call");
		LF_WantedReports.Add(SUSPECT_2, LF_WantedReports.OFFENCE_ROBBERY, "", "", "test site", false);
		LF_PoliceCallPay.OnServiceCall(LF_OnServiceCall.Create(SUSPECT_2, "police", "test site", LF_WantedReports.OFFENCE_ROBBERY, "radio", site, {}));
		LF_PoliceCallPay.OnWantedChanged(LF_OnWantedChanged.Create(SUSPECT_2, 2, 0));
		ctx.False(LF_PoliceCallPay.HasSite(SUSPECT_2), "a cleared wanted level forgets the site");
		LF_PoliceCallPay.RecordSite(SUSPECT_2, site);
		LF_WantedReports.ClearFor(SUSPECT_2);
		ctx.False(LF_PoliceCallPay.Settle(OFFICER_A, SUSPECT_2, inside, bonus), "a suspect with no open report completes nothing");
		LF_WantedReports.Add(SUSPECT_2, LF_WantedReports.OFFENCE_ROBBERY, "", "", "test site", false);
		ctx.True(LF_PoliceCallPay.Settle(OFFICER_A, SUSPECT_2, inside, bonus), "the reported suspect's fine completes the second call");
		ctx.Equal(2 * LF_DutyBonus.CALL_RESOLVED_AMOUNT, LF_DutyBonus.Pending(officerA), "two calls, two bonuses");

		// A call with no kept position uses the centre of its grid square.
		vector centre;
		ctx.True(LF_PoliceCallPay.GridCentre(LF_WantedReports.GridPlace(Vector(1234, 0, 5678)), centre), "a grid place resolves");
		ctx.True(vector.DistanceXZ(centre, Vector(1250, 0, 5650)) < 0.01, "the grid place resolves to its square's centre");
		ctx.False(LF_PoliceCallPay.GridCentre("#SomeSign_Title", centre), "a sign title has no grid position");

		officerA.SetOnDuty(false);
		officerB.SetOnDuty(false);
		LF_PoliceCalls.ResetStatics();
		LF_PoliceCallPay.ResetStatics();
		LF_WantedReports.ResetStatics();
		LF_DutyBonus.ResetStatics();
		// The reset dropped the game mode's subscriptions; put them back for later tests.
		LF_PoliceCalls.Install();
		LF_PoliceCallPay.Install();
	}

	//------------------------------------------------------------------------------------------------
	protected LF_PlayerAccount OnDutyOfficer(string uid)
	{
		LF_PlayerAccount account = LF_PlayerAccountManager.GetOrCreate(uid);
		account.SetFaction(LF_Faction.POLICE);
		account.SetPoliceRank(LF_PoliceRanks.SENIOR);
		account.SetOnDuty(true);
		return account;
	}
}
