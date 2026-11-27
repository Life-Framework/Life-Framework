// red-proof: RED-OK licence/seizure, observed 2026-11-19 with `tools\cli test --no-build --red LF_Test_LicenceSeizure`: with the booking and the arrest event skipped, `the arrest bans the permit for 7 days (expected 7, got 0)`, `the arrest drops the open application (expected -1, got 10001440)` and `the desk refuses the arrested buyer (expected 11, got 3)`. Green: `--only` passed=2. 2026-11-27 (crime playtest): on the unfixed code `--only` failed `a player with no permit is told they are barred, not that it was seized (expected '#LF-LicenceBarred_Firearm', got '#LF-LicenceSeized_Firearm')`; green after the fix.
//! An arrest seizes the licence and the desk says when it comes back (#3432): booking a conviction
//! for a mapped crime code takes the licence from the body and the saved record and bans it, the
//! arrested player is told, an open permit application is dropped, and the licence desk refuses the
//! buyer with the days left. A conviction with no mapped code changes nothing. The bus path (an
//! LF_OnArrested with the suspect's open reports) does the same on the saved record. Across a restart
//! (`tools\cli wt e2e`): the save boot books a real conviction (brandishing, a 7-day ban, the shortest mapped one) for E2E_UID; the reload boot finds the
//! licence still gone and the desk still refusing.
[LF_TestInfo(LF_TestTier.PERSISTENCE)]
class LF_Test_LicenceSeizure : LF_Test
{
	protected static const string UID = "test-licseize-a";
	protected static const string UID_BUS = "test-licseize-bus";
	protected static const string UID_UNMAPPED = "test-licseize-none";
	protected static const string UID_NOPERMIT = "test-licseize-nopermit";
	static const string E2E_UID = "test-licseize-e2e";
	protected static const string BRANDISHING = "#LF-Ticket_OffenceBrandishing";
	protected static const string UNMAPPED = "unmapped-offence";
	protected static const int T0 = 10000000;	// in-game minute the test pins the clock at
	protected static const int DAY = 1440;
	protected static const int FEE = 300;

	override string GetName()
	{
		return "licence/seizure";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
#ifdef LF_E2E_LOAD_BOOT
		if (LF_E2E.RequiresSection(LF_LicenceBansSavePayload.NAME))
			VerifyE2E(ctx);

		return;
#endif

		array<ref LF_LicenceBanRow> before = LF_LicenceBans.ExportAll(LF_LicenceBans.Now());
		LF_LicenceBans.ResetStatics();
		LF_LicenceDesk.SetNowForTest(T0);
		LF_LicencesConfig.SetDeskForTest(FEE, 1, 0);
		LF_Progression.Forget(UID);
		LF_Progression.Forget(UID_BUS);
		LF_Progression.Forget(UID_UNMAPPED);
		LF_Progression.Forget(UID_NOPERMIT);

		IEntity holder = LF_TestCharacterFixture.Spawn(ctx);
		LF_LicenseManagerComponent manager;
		if (holder)
			manager = LF_LicenseManagerComponent.Cast(holder.FindComponent(LF_LicenseManagerComponent));

		ctx.NotNull(manager, "the fixture body carries LF_LicenseManagerComponent");
		if (manager)
			RunBooking(ctx, holder, manager);

		RunBus(ctx);

		LF_Utils.SetUidForTests(null, "");
		LF_TestCharacterFixture.Delete(holder);
		LF_WantedReports.ClearFor(UID_BUS);
		LF_WantedReports.ClearFor(UID_UNMAPPED);
		LF_Progression.Forget(UID);
		LF_Progression.Forget(UID_BUS);
		LF_Progression.Forget(UID_UNMAPPED);
		LF_Progression.Forget(UID_NOPERMIT);
		LF_LicenceDesk.SetNowForTest(-1);
		LF_LicencesConfig.ResetStatics();
		// Hand every other test's ban back, then leave the e2e conviction for the shutdown save.
		LF_LicenceBans.ApplyAll(before, LF_LicenceBans.Now());
		SeedE2E(ctx);
	}

	//------------------------------------------------------------------------------------------------
	//! A booked conviction on a live body: unmapped first, then the mapped code.
	protected void RunBooking(LF_TestContext ctx, IEntity holder, LF_LicenseManagerComponent manager)
	{
		LF_Utils.SetUidForTests(holder, UID);
		array<LF_ELicenseType> held = {LF_ELicenseType.UNEMPLOYED, LF_ELicenseType.FIREARM_PERMIT, LF_ELicenseType.DRIVING_LICENCE};
		manager.SetUnlockedLicenses(held);
		LF_Progression.NoteLicenses(UID, held);
		LF_Progression.NoteFlag(UID, LF_LicenceDesk.DUE_FLAG + (T0 + DAY).ToString());
		ctx.True(LF_LicenceDesk.DueMinutes(UID) > 0, "the buyer has an open permit application");

		// A conviction with no mapped code changes nothing.
		array<string> unmapped = {UNMAPPED};
		ctx.Equal(0, LF_LicenceSeizure.Book(UID, unmapped, holder), "an unmapped conviction seizes nothing");
		ctx.True(manager.HasLicense(LF_ELicenseType.FIREARM_PERMIT), "an unmapped conviction leaves the permit on the body");
		ctx.True(SavedHas(UID, LF_ELicenseType.FIREARM_PERMIT), "an unmapped conviction leaves the permit in the saved licences");
		ctx.False(LF_LicenceBans.IsBanned(UID, LF_ELicenseType.FIREARM_PERMIT), "an unmapped conviction bans nothing");
		ctx.True(LF_LicenceDesk.DueMinutes(UID) > 0, "an unmapped conviction leaves the application open");

		// A mapped code takes the permit and bans it for the rule's days.
		array<string> mapped = {BRANDISHING, UNMAPPED};
		int seized;
		// RedProof(): the booking is skipped, so every read-back assertion below fails (`--red`).
		if (!RedProof())
			seized = LF_LicenceSeizure.Book(UID, mapped, holder);

		ctx.Equal(1, seized, "a brandishing conviction seizes one licence");
		ctx.False(manager.HasLicense(LF_ELicenseType.FIREARM_PERMIT), "the conviction takes the permit from the arrested body");
		ctx.False(SavedHas(UID, LF_ELicenseType.FIREARM_PERMIT), "the conviction takes the permit from the saved licences");
		ctx.True(manager.HasLicense(LF_ELicenseType.DRIVING_LICENCE), "the conviction keeps the licences its code does not name");
		ctx.True(LF_LicenceBans.IsBanned(UID, LF_ELicenseType.FIREARM_PERMIT), "the permit is banned");
		ctx.Equal(7, LF_LicenceSeizure.DaysLeft(UID, LF_ELicenseType.FIREARM_PERMIT), "the brandishing ban is 7 in-game days");
		ctx.Equal(-1, LF_LicenceDesk.DueMinutes(UID), "the conviction drops the open permit application");
		ctx.EqualStr(LF_LicenceSeizure.KEY_FIREARM, LF_LicenceSeizure.SeizedKey(LF_ELicenseType.FIREARM_PERMIT, 7), "the arrested player's line names the permit");
		ctx.EqualStr(LF_LicenceSeizure.KEY_FIREARM, LF_LicenceSeizure.LastToldKey(), "a held permit is told as seized");

		// The desk refuses with the days left and takes no cash.
		LF_TestCashWallet wallet = LF_TestCashWallet.Create(1000, 100000);
		int minutesLeft;
		LF_ELicenceDesk outcome = LF_LicenceDesk.VisitFor(holder, UID, wallet, minutesLeft);
		ctx.Equal(LF_ELicenceDesk.BANNED, outcome, "the desk refuses a banned buyer");
		ctx.Equal(7 * DAY, minutesLeft, "the desk knows the minutes left");
		ctx.EqualStr("7", LF_LicenceDesk.AnswerArg(outcome, minutesLeft), "the desk's answer carries the days left");
		ctx.EqualStr("#LF-LicenceDesk_Banned", LF_LicenceDesk.AnswerKey(outcome), "the desk's answer is the banned line");
		ctx.Equal(1000, wallet.Carried(), "the refusal took no cash");
		ctx.Equal(-1, LF_LicenceDesk.DueMinutes(UID), "the refused visit opened no application");

		// Two days on: still refused, 5 days left. The ban ends: the desk takes the application again.
		LF_LicenceDesk.SetNowForTest(T0 + 2 * DAY);
		outcome = LF_LicenceDesk.VisitFor(holder, UID, wallet, minutesLeft);
		ctx.Equal(LF_ELicenceDesk.BANNED, outcome, "two days on the buyer is still refused");
		ctx.EqualStr("5", LF_LicenceDesk.AnswerArg(outcome, minutesLeft), "two days on the desk says 5 days");
		LF_LicenceDesk.SetNowForTest(T0 + 7 * DAY);
		outcome = LF_LicenceDesk.VisitFor(holder, UID, wallet, minutesLeft);
		ctx.True(outcome != LF_ELicenceDesk.BANNED, "once the ban ends the desk no longer refuses on it");
		LF_LicenceDesk.SetNowForTest(T0);

		// A driving conviction bans the driving licence only.
		array<string> reckless = {"#LF-Ticket_OffenceRecklessDriving"};
		ctx.Equal(1, LF_LicenceSeizure.Book(UID, reckless, holder), "a reckless driving conviction seizes one licence");
		ctx.False(manager.HasLicense(LF_ELicenseType.DRIVING_LICENCE), "the conviction takes the driving licence");
		ctx.Equal(LF_ELicenceDesk.BANNED, LF_LicenceDesk.BuyDrivingFor(holder, UID, wallet), "the desk refuses to sell a banned driving licence");
		ctx.EqualStr("3", LF_LicenceDesk.DrivingAnswerArg(LF_ELicenceDesk.BANNED, UID), "the driving refusal says 3 days");

		// A conviction that bans a licence the player never held says so: "seized" is wrong for a player
		// with no permit (agent playtest 2026-11-27: an armed robber with no permit read "Your firearm permit has been seized").
		LF_Utils.SetUidForTests(holder, UID_NOPERMIT);
		array<LF_ELicenseType> none = {LF_ELicenseType.UNEMPLOYED};
		manager.SetUnlockedLicenses(none);
		LF_Progression.NoteLicenses(UID_NOPERMIT, none);
		array<string> brandishing = {BRANDISHING};
		ctx.Equal(1, LF_LicenceSeizure.Book(UID_NOPERMIT, brandishing, holder), "a conviction with no permit still bans the permit");
		ctx.True(LF_LicenceBans.IsBanned(UID_NOPERMIT, LF_ELicenseType.FIREARM_PERMIT), "the player with no permit is barred from one");
		ctx.EqualStr("#LF-LicenceBarred_Firearm", LF_LicenceSeizure.LastToldKey(), "a player with no permit is told they are barred, not that it was seized");
	}

	//------------------------------------------------------------------------------------------------
	//! The real arrest path: LF_OnArrested with the suspect's open crime reports, no live body.
	protected void RunBus(LF_TestContext ctx)
	{
		ctx.True(LF_LicenceSeizure.EnsureSubscribed(), "the seizure subscribes to LF_OnArrested");
		array<LF_ELicenseType> held = {LF_ELicenseType.UNEMPLOYED, LF_ELicenseType.FIREARM_PERMIT};
		LF_Progression.NoteLicenses(UID_BUS, held);
		LF_Progression.NoteLicenses(UID_UNMAPPED, held);
		LF_WantedReports.Add(UID_BUS, BRANDISHING, "", "", "grid 0-0", true);
		LF_WantedReports.Add(UID_UNMAPPED, UNMAPPED, "", "", "grid 0-0", true);
		LF_Progression.NoteFlag(UID_BUS, LF_LicenceDesk.DUE_FLAG + (T0 + DAY).ToString());

		if (!RedProof())
		{
			LF_EventBus.Publish(LF_OnArrested.Create(UID_UNMAPPED, 300, 2));
			LF_EventBus.Publish(LF_OnArrested.Create(UID_BUS, 300, 2));
		}

		ctx.True(SavedHas(UID_UNMAPPED, LF_ELicenseType.FIREARM_PERMIT), "an arrest for an unmapped offence leaves the permit");
		ctx.False(LF_LicenceBans.IsBanned(UID_UNMAPPED, LF_ELicenseType.FIREARM_PERMIT), "an arrest for an unmapped offence bans nothing");
		ctx.False(SavedHas(UID_BUS, LF_ELicenseType.FIREARM_PERMIT), "an arrest for brandishing takes the permit from the saved record");
		ctx.True(SavedHas(UID_BUS, LF_ELicenseType.UNEMPLOYED), "the arrest keeps the other licences");
		ctx.Equal(7, LF_LicenceSeizure.DaysLeft(UID_BUS, LF_ELicenseType.FIREARM_PERMIT), "the arrest bans the permit for 7 days");
		ctx.Equal(-1, LF_LicenceDesk.DueMinutes(UID_BUS), "the arrest drops the open application");
		LF_TestCashWallet wallet = LF_TestCashWallet.Create(1000, 100000);
		int minutesLeft;
		ctx.Equal(LF_ELicenceDesk.BANNED, LF_LicenceDesk.VisitFor(null, UID_BUS, wallet, minutesLeft), "the desk refuses the arrested buyer");
	}

	//------------------------------------------------------------------------------------------------
	protected static bool SavedHas(string uid, LF_ELicenseType licence)
	{
		array<LF_ELicenseType> saved = {};
		LF_Progression.GetLicenses(uid, saved);
		return saved.Contains(licence);
	}

	//------------------------------------------------------------------------------------------------
	//! wt e2e save boot: a real brandishing arrest (a 7-day ban) on the real game clock; the clock does not move across the restart (#3741), for the shutdown save.
	static void SeedE2E(LF_TestContext ctx)
	{
		array<LF_ELicenseType> held = {LF_ELicenseType.UNEMPLOYED, LF_ELicenseType.FIREARM_PERMIT};
		LF_Progression.NoteLicenses(E2E_UID, held);
		LF_WantedReports.Add(E2E_UID, BRANDISHING, "", "", "grid 0-0", true);
		LF_EventBus.Publish(LF_OnArrested.Create(E2E_UID, 300, 2));
		LF_WantedReports.ClearFor(E2E_UID);
		ctx.True(LF_LicenceBans.IsBanned(E2E_UID, LF_ELicenseType.FIREARM_PERMIT), "e2e conviction banned the permit before the save");
	}

	//------------------------------------------------------------------------------------------------
	//! wt e2e reload boot: the conviction came back from the real save, the permit still gone, the desk still refusing.
	static void VerifyE2E(LF_TestContext ctx)
	{
		ctx.True(LF_LicenceBans.IsBanned(E2E_UID, LF_ELicenseType.FIREARM_PERMIT), "e2e permit ban is back after the restart");
		ctx.False(SavedHas(E2E_UID, LF_ELicenseType.FIREARM_PERMIT), "e2e permit is still gone after the restart");
		LF_TestCashWallet wallet = LF_TestCashWallet.Create(1000, 100000);
		int minutesLeft;
		ctx.Equal(LF_ELicenceDesk.BANNED, LF_LicenceDesk.VisitFor(null, E2E_UID, wallet, minutesLeft), "the desk still refuses the convicted buyer after the restart");
		ctx.True(minutesLeft > 0, "the desk still says how long is left after the restart");
		LF_LicenceBans.ResetStatics();
	}
}
