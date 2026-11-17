// red-proof: `tools\cli test --no-build --red LF_Test_PoliceCallPay` (2026-11-18): RED PROOF OK, with the Settle skipped:
// "the call is completed", "a completed call forgets its site", "a second fine pays nothing (expected 0, got 150)".
// tier: LOGIC
//! Police call pay (#2478, parent #234): a fine by another officer or outside the radius completes
//! nothing; the holder's fine inside the radius completes the call and pays the reward once. The site
//! comes from the police service call that follows the wanted change (a crime's real order).
class LF_Test_PoliceCallPay : LF_Test
{
	protected static const string SUSPECT = "police-pay-suspect";
	protected static const string SUSPECT_2 = "police-pay-suspect-2";
	protected static const string OFFICER_A = "police-pay-officer-a";
	protected static const string OFFICER_B = "police-pay-officer-b";
	protected static const int FINE = 1000;

	override string GetName()
	{
		return "police/call-pay";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_PoliceCalls.ResetStatics();
		LF_PoliceCallPay.ResetStatics();
		LF_WantedReports.ResetStatics();

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
		int reward = LF_PoliceCallPay.Reward();
		ctx.True(radius > 0, "the call radius is positive");
		ctx.True(reward > 0 && reward < FINE, "the call reward is positive and under the test fine");

		vector inside = site + Vector(radius * 0.5, 0, 0);
		vector outside = site + Vector(radius + 5, 0, 0);
		LF_TestCashWallet walletA = LF_TestCashWallet.Create(0, 1000000);
		LF_TestCashWallet walletB = LF_TestCashWallet.Create(0, 1000000);
		LF_TestCashWallet full = LF_TestCashWallet.Create(0, reward - 1);

		ctx.Equal(0, LF_PoliceCallPay.Settle(OFFICER_B, SUSPECT, inside, FINE, walletB), "a fine by another officer pays nothing");
		ctx.Equal(0, walletB.Carried(), "the other officer gets no cash");
		ctx.False(call.m_bClosed, "a fine by another officer leaves the call open");

		ctx.Equal(0, LF_PoliceCallPay.Settle(OFFICER_A, SUSPECT, outside, FINE, walletA), "the holder's fine outside the radius pays nothing");
		ctx.False(call.m_bClosed, "a fine outside the radius leaves the call open");

		ctx.Equal(0, LF_PoliceCallPay.Settle(OFFICER_A, SUSPECT, inside, FINE, null), "no wallet pays nothing");
		ctx.Equal(0, LF_PoliceCallPay.Settle(OFFICER_A, SUSPECT, inside, FINE, full), "a wallet without room for the reward pays nothing");
		ctx.Equal(0, full.Carried(), "the cash that fitted is taken back");
		ctx.False(call.m_bClosed, "a fine that could not pay leaves the call open");

		int paid;
		if (!RedProof())
			paid = LF_PoliceCallPay.Settle(OFFICER_A, SUSPECT, inside, FINE, walletA);

		ctx.Equal(reward, paid, "the holder's fine inside the radius pays the reward");
		ctx.Equal(reward, walletA.Carried(), "the officer carries the reward");
		ctx.True(call.m_bClosed, "the call is completed");
		ctx.False(LF_PoliceCallPay.HasSite(SUSPECT), "a completed call forgets its site");

		ctx.Equal(0, LF_PoliceCallPay.Settle(OFFICER_A, SUSPECT, inside, FINE, walletA), "a second fine pays nothing");
		ctx.Equal(reward, walletA.Carried(), "the reward is paid once");

		// A second suspect: a cleared wanted level forgets the site and stops the pay; a small fine caps it.
		LF_PoliceCall second = LF_PoliceCalls.Post(LF_PoliceCalls.KIND_FINE, SUSPECT_2, "test site", LF_PoliceRanks.PATROL);
		ctx.True(second != null && LF_PoliceCalls.Take(second.m_iId, OFFICER_A, senior), "officer A takes a second call");
		LF_WantedReports.Add(SUSPECT_2, LF_WantedReports.OFFENCE_ROBBERY, "", "", "test site", false);
		LF_PoliceCallPay.OnServiceCall(LF_OnServiceCall.Create(SUSPECT_2, "police", "test site", LF_WantedReports.OFFENCE_ROBBERY, "radio", site, {}));
		LF_PoliceCallPay.OnWantedChanged(LF_OnWantedChanged.Create(SUSPECT_2, 2, 0));
		ctx.False(LF_PoliceCallPay.HasSite(SUSPECT_2), "a cleared wanted level forgets the site");
		LF_PoliceCallPay.RecordSite(SUSPECT_2, site);
		LF_WantedReports.ClearFor(SUSPECT_2);
		LF_TestCashWallet walletC = LF_TestCashWallet.Create(0, 1000000);
		ctx.Equal(0, LF_PoliceCallPay.Settle(OFFICER_A, SUSPECT_2, inside, FINE, walletC), "a suspect with no open report pays nothing");
		LF_WantedReports.Add(SUSPECT_2, LF_WantedReports.OFFENCE_ROBBERY, "", "", "test site", false);
		ctx.Equal(reward / 2, LF_PoliceCallPay.Settle(OFFICER_A, SUSPECT_2, inside, reward / 2, walletC), "the reward never exceeds the fine");

		// A call with no kept position uses the centre of its grid square.
		vector centre;
		ctx.True(LF_PoliceCallPay.GridCentre(LF_WantedReports.GridPlace(Vector(1234, 0, 5678)), centre), "a grid place resolves");
		ctx.True(vector.DistanceXZ(centre, Vector(1250, 0, 5650)) < 0.01, "the grid place resolves to its square's centre");
		ctx.False(LF_PoliceCallPay.GridCentre("#SomeSign_Title", centre), "a sign title has no grid position");

		LF_PoliceCalls.ResetStatics();
		LF_PoliceCallPay.ResetStatics();
		LF_WantedReports.ResetStatics();
		// The reset dropped the game mode's subscriptions; put them back for later tests.
		LF_PoliceCalls.Install();
		LF_PoliceCallPay.Install();
	}
}
