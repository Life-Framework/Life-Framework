// red-proof: Observed 2026-11-22 with `tools/cli test --no-build --red LF_Test_JailBreakLayers,LF_Test_JailBreakFree,LF_Test_JailBail`: RED PROOF OK with the payment skipped: `the bail asked is the quote (expected 300, got 0)`, `exactly the bail left the bank (expected 700, got 1000)`, `the sentence ended`. The unhooked run passes. 2026-11-27 (crime playtest): on the unfixed code `--only` failed `a bail release queues no 'Sentence served' toast (expected 0, got 1)`; green after the fix.
//! Bail (LF_JailBail): the quote per started minute with its floor and cap, refusals that take
//! nothing (bail off, not in custody, a bank short of the bail), and a paid bail that takes the exact
//! amount once and ends the sentence.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_JailBail : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "police/jail-bail";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		// The quote: started minutes, floor, cap.
		ctx.Equal(0, LF_JailBailRules.Quote(0, 100, 250, 20000), "nothing left, nothing to pay");
		ctx.Equal(250, LF_JailBailRules.Quote(30, 100, 250, 20000), "half a minute pays the minimum");
		ctx.Equal(300, LF_JailBailRules.Quote(121, 100, 250, 20000), "a started minute counts whole");
		ctx.Equal(1000, LF_JailBailRules.Quote(6000, 100, 250, 1000), "the cap holds");

		string prisonerUid = "jail-bail-prisoner";
		string freeUid = "jail-bail-free";
		LF_JailBreakConfig config = LF_JailBreakConfig.CreateDefault();
		config.m_iBailPerMinute = 100;
		config.m_iBailMinimum = 250;
		config.m_iBailMaximum = 20000;
		LF_JailBreakConfig.UseForTest(config);
		LF_Jail.Configure(40, 200, 1, 4);
		LF_Jail.SetCellOrigin("56 0 74");

		LF_PlayerAccountManager.GetOrCreate(prisonerUid).SetWantedLevel(2);
		LF_BankAccount bank = LF_ATMManager.GetOrCreate(prisonerUid);
		bank.SetBalance(299);
		ctx.True(LF_Jail.Imprison(prisonerUid, 150, 2, null), "the prisoner serves 150 s");
		ctx.Equal(300, LF_JailBail.QuoteFor(prisonerUid), "three started minutes are 300");

		int bail;
		ctx.Equal(LF_EJailBailResult.NOT_IN_CUSTODY, LF_JailBail.Pay(freeUid, bail), "a free player has no bail to pay");
		ctx.Equal(LF_EJailBailResult.TOO_POOR, LF_JailBail.Pay(prisonerUid, bail), "a bank one short is refused");
		ctx.Equal(299, bank.GetBalance(), "a refused bail takes nothing");
		ctx.True(LF_Jail.IsServing(prisonerUid), "a refused bail leaves the sentence");

		config.m_bBailEnabled = false;
		bank.SetBalance(1000);
		ctx.Equal(LF_EJailBailResult.OFF, LF_JailBail.Pay(prisonerUid, bail), "bail switched off is refused");
		ctx.Equal(1000, bank.GetBalance(), "bail off takes nothing");
		config.m_bBailEnabled = true;

		// RedProof(): the bail is never paid, so the money and sentence assertions fail.
		int toastsBefore = LF_Jail.ReleaseToastsQueued();
		if (!RedProof())
			ctx.Equal(LF_EJailBailResult.PAID, LF_JailBail.Pay(prisonerUid, bail), "the bail is paid");

		// The bail toast already says "You are free to go"; a "Sentence served" toast after it is wrong (playtest 2026-11-27).
		ctx.Equal(toastsBefore, LF_Jail.ReleaseToastsQueued(), "a bail release queues no 'Sentence served' toast");

		ctx.Equal(300, bail, "the bail asked is the quote");
		ctx.Equal(700, bank.GetBalance(), "exactly the bail left the bank");
		ctx.False(LF_Jail.IsServing(prisonerUid), "the sentence ended");
		ctx.Equal(LF_EJailBailResult.NOT_IN_CUSTODY, LF_JailBail.Pay(prisonerUid, bail), "a second payment is refused");
		ctx.Equal(700, bank.GetBalance(), "nothing is taken twice");

		// A served sentence still says so.
		LF_PlayerAccountManager.GetOrCreate(prisonerUid).SetWantedLevel(1);
		ctx.True(LF_Jail.Imprison(prisonerUid, 30, 1, null), "the prisoner serves again");
		ctx.True(LF_Jail.IsServing(prisonerUid), "the second sentence runs");
		int servedBefore = LF_Jail.ReleaseToastsQueued();
		LF_Jail.Advance(prisonerUid, 3600);
		ctx.False(LF_Jail.IsServing(prisonerUid), "the second sentence is over");
		ctx.Equal(servedBefore + 1, LF_Jail.ReleaseToastsQueued(), "a served sentence queues the 'Sentence served' toast");

		// Clean up what this test made.
		bank.SetBalance(0);
		LF_JailBreakConfig.Reload();
		LF_Jail.SyncPlacedCell();
	}
}
