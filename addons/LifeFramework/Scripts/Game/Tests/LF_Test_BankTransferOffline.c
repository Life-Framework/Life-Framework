// red-proof: observed 2026-11-19 `tools/cli test --no-build --red LF_Test_BankTransferOffline` printed `RED PROOF OK (all 1 listed test(s) fail with the hook on)`: RedProof() skips the journal load after the restart, so the replayed id pays a second time and the balances read 0; `--only` then passed.
//! Bank transfer by account number, offline recipient included, exactly once (#3319, slice of #638). The
//! recipient's owner is not in the world: the account is a row in the bank cache, loaded from the save like
//! every other. The transfer takes amount plus fee from the sender and credits the amount; the ledger gets
//! one pair of rows with one transfer id; the bank, the ledger and the "bank_transfer" section go through a
//! save and a restart; a replayed id is refused; a credit that fails after the debit pays the sender back
//! once; and a server that died between debit and credit pays the sender back on load, once.
[LF_TestInfo(LF_TestTier.PERSISTENCE)]
class LF_Test_BankTransferOffline : LF_Test
{
	protected static const string SENDER = "test-bt-sender";
	protected static const string OFFLINE = "test-bt-offline";
	//! A fixed clock, so the daily window is exact.
	protected static const int NOW = 1800000000;

	override string GetName()
	{
		return "bank/transfer-offline";
	}

	//------------------------------------------------------------------------------------------------
	protected LF_BankTransferConfig MakeConfig()
	{
		LF_BankTransferConfig config = LF_BankTransferConfig.CreateDefault();
		config.m_iFee = 5;
		config.m_iFeePercent = 0;
		config.m_iDailyLimit = 400;
		return config;
	}

	//------------------------------------------------------------------------------------------------
	//! One account's ledger rows, newest first.
	protected int Rows(string uid, array<ref LF_LedgerEntry> outRows)
	{
		LF_Ledger.Last(uid, LF_Ledger.ACCOUNT_BANK, 50, outRows);
		return outRows.Count();
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_SavePayload section = LF_SaveSections.Get(LF_BankTransferSavePayload.NAME);
		ctx.NotNull(section, "bank_transfer is a registered save section");
		if (!section)
			return;

		array<ref LF_BankAccountRecord> bankBefore = LF_ATMManager.ExportAll();
		array<ref LF_LedgerEntry> ledgerBefore = LF_Ledger.ExportAll();
		LF_ATMManager.Reset();
		LF_BankTransfer.ResetStatics();
		LF_Ledger.ApplyAll(null);
		LF_Ledger.EnsureSubscribed();

		LF_BankTransferConfig config = MakeConfig();
		LF_BankAccount sender = LF_ATMManager.GetOrCreate(SENDER);
		LF_BankAccount offline = LF_ATMManager.GetOrCreate(OFFLINE);
		sender.SetBalance(500);
		offline.SetBalance(100);

		// The transfer: $200 and a $5 fee. The recipient has no character in the world.
		LF_BankTransferOutcome sent = LF_BankTransfer.Execute(config, "tx-1", SENDER, OFFLINE, 200, NOW);
		ctx.True(sent.IsDone(), "the transfer goes");
		ctx.Equal(295, sender.GetBalance(), "the sender is down the amount plus the fee");
		ctx.Equal(300, offline.GetBalance(), "the offline recipient has the amount");

		array<ref LF_LedgerEntry> rows = {};
		ctx.Equal(1, Rows(SENDER, rows), "the sender has one ledger row");
		if (rows.Count() == 1)
		{
			ctx.Equal(-205, rows.Get(0).m_iDelta, "the sender's row is amount plus fee");
			ctx.EqualStr(LF_BankTransfer.REASON_OUT + "tx-1", rows.Get(0).m_sReason, "the sender's row carries the transfer id");
			ctx.EqualStr(OFFLINE, rows.Get(0).m_sCounterpartyUid, "the sender's row names the recipient");
		}

		ctx.Equal(1, Rows(OFFLINE, rows), "the recipient has one ledger row");
		if (rows.Count() == 1)
		{
			ctx.Equal(200, rows.Get(0).m_iDelta, "the recipient's row is the amount");
			ctx.EqualStr(LF_BankTransfer.REASON_IN + "tx-1", rows.Get(0).m_sReason, "the recipient's row carries the same transfer id");
			ctx.EqualStr(SENDER, rows.Get(0).m_sCounterpartyUid, "the recipient's row names the sender");
		}

		// Save, restart, load.
		string saved = Save(ctx, section);
		Restart();
		if (!RedProof())
			Load(ctx, section, saved);

		ctx.Equal(295, LF_ATMManager.GetOrCreate(SENDER).GetBalance(), "after the restart the sender is down amount plus fee once");
		ctx.Equal(300, LF_ATMManager.GetOrCreate(OFFLINE).GetBalance(), "after the restart the recipient has the amount once");
		ctx.Equal(1, Rows(SENDER, rows), "after the restart the sender still has one ledger row");
		ctx.Equal(1, Rows(OFFLINE, rows), "after the restart the recipient still has one ledger row");

		// A replayed id is refused, and loading the same save again changes nothing.
		LF_BankTransferOutcome replay = LF_BankTransfer.Execute(config, "tx-1", SENDER, OFFLINE, 200, NOW + 5);
		ctx.True(replay.m_eStatus == LF_EBankTransferStatus.REPLAY, "a replayed transfer id is refused after the restart");
		ctx.Equal(295, LF_ATMManager.GetOrCreate(SENDER).GetBalance(), "a replay takes nothing from the sender");
		ctx.Equal(300, LF_ATMManager.GetOrCreate(OFFLINE).GetBalance(), "a replay pays the recipient nothing");
		Load(ctx, section, saved);
		Load(ctx, section, saved);
		ctx.Equal(295, LF_ATMManager.GetOrCreate(SENDER).GetBalance(), "loading the save twice moves no money");

		// The rules still guard: your own number, an unknown number, no balance, the daily limit.
		ctx.True(LF_BankTransfer.Execute(config, "tx-own", SENDER, SENDER, 10, NOW + 6).m_eRefusal == LF_EBankTransferRefusal.OWN_ACCOUNT, "your own number is refused");
		ctx.True(LF_BankTransfer.Execute(config, "tx-none", SENDER, "test-bt-nobody", 10, NOW + 6).m_eRefusal == LF_EBankTransferRefusal.UNKNOWN_ACCOUNT, "an unknown number is refused");
		ctx.True(LF_BankTransfer.Execute(config, "tx-big", SENDER, OFFLINE, 250, NOW + 6).m_eRefusal == LF_EBankTransferRefusal.DAILY_LIMIT, "$200 sent plus $250 is over the $400 daily limit");
		ctx.True(LF_BankTransfer.Execute(config, "tx-short", SENDER, OFFLINE, 295, NOW + 6).m_eRefusal == LF_EBankTransferRefusal.NOT_ENOUGH_BALANCE, "the balance must cover amount plus fee");
		ctx.Equal(295, LF_ATMManager.GetOrCreate(SENDER).GetBalance(), "a refused transfer takes nothing");

		// A forced failure after the debit pays the sender back exactly once.
		LF_BankTransfer.FailNextCreditForTests();
		LF_BankTransferOutcome failed = LF_BankTransfer.Execute(config, "tx-2", SENDER, OFFLINE, 100, NOW + 10);
		ctx.True(failed.m_eStatus == LF_EBankTransferStatus.REFUNDED, "the failed credit is refunded");
		ctx.Equal(295, LF_ATMManager.GetOrCreate(SENDER).GetBalance(), "the sender is whole again after the refund");
		ctx.Equal(300, LF_ATMManager.GetOrCreate(OFFLINE).GetBalance(), "the recipient got nothing from the failed transfer");
		ctx.Equal(3, Rows(SENDER, rows), "the sender's ledger shows the debit and the refund");
		ctx.True(LF_Ledger.VerifyBank(SENDER, 295), "the ledger still explains the sender's balance");
		ctx.True(LF_BankTransfer.Execute(config, "tx-2", SENDER, OFFLINE, 100, NOW + 11).m_eStatus == LF_EBankTransferStatus.REPLAY, "a retry of the refunded id is not a second refund");
		string afterRefund = Save(ctx, section);
		Restart();
		Load(ctx, section, afterRefund);
		ctx.Equal(295, LF_ATMManager.GetOrCreate(SENDER).GetBalance(), "a reload after a refund refunds nothing twice");

		// The server dies between the debit and the credit: the save holds the debit and the DEBITED row.
		LF_BankTransfer.StopAfterDebitForTests();
		LF_BankTransfer.Execute(config, "tx-3", SENDER, OFFLINE, 100, NOW + 20);
		ctx.Equal(190, LF_ATMManager.GetOrCreate(SENDER).GetBalance(), "the debit went before the crash");
		string crashed = Save(ctx, section);
		Restart();
		Load(ctx, section, crashed);
		ctx.Equal(295, LF_ATMManager.GetOrCreate(SENDER).GetBalance(), "the load pays the sender back once");
		ctx.Equal(300, LF_ATMManager.GetOrCreate(OFFLINE).GetBalance(), "the recipient was never credited");
		Load(ctx, section, crashed);
		ctx.Equal(295, LF_ATMManager.GetOrCreate(SENDER).GetBalance(), "a second load of the crashed save pays nothing twice");
		ctx.True(LF_BankTransfer.Execute(config, "tx-3", SENDER, OFFLINE, 100, NOW + 21).m_eStatus == LF_EBankTransferStatus.REPLAY, "the crashed id cannot be replayed");

		LF_ATMManager.Reset();
		LF_BankTransfer.ResetStatics();
		LF_ATMManager.ApplyAll(bankBefore);
		LF_Ledger.ApplyAll(ledgerBefore);
	}

	//------------------------------------------------------------------------------------------------
	//! The save of the moment: the bank and ledger payloads are held as records (LF_BankSaveSnapshot), the
	//! journal section is written as the serializer writes it and returned as text.
	protected string Save(LF_TestContext ctx, LF_SavePayload section)
	{
		LF_BankSaveSnapshot.Take();
		JsonSaveContext context = new JsonSaveContext(false);
		section.WriteSection(context);
		string text = context.SaveToString();
		ctx.True(text.Contains("tx-1"), "the journal section carries the first transfer");
		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! The restart: no accounts, no ledger, no journal.
	protected void Restart()
	{
		LF_ATMManager.Reset();
		LF_BankTransfer.ResetStatics();
		LF_Ledger.ApplyAll(null);
	}

	//------------------------------------------------------------------------------------------------
	//! The load: the bank payload and the ledger payload first, then the section (sections apply last).
	protected void Load(LF_TestContext ctx, LF_SavePayload section, string text)
	{
		LF_BankSaveSnapshot.Apply();
		JsonLoadContext context = new JsonLoadContext(false);
		ctx.True(context.LoadFromString(text), "the journal section parses");
		ctx.True(section.ReadSection(context, section.Version()), "the journal section reads");
	}
}

//------------------------------------------------------------------------------------------------
//! The bank and ledger payloads of one save, held as records the way the store would carry them. Each
//! Take replaces the held snapshot; Apply hands it back to the managers.
class LF_BankSaveSnapshot
{
	protected static ref array<ref LF_BankAccountRecord> s_aBank;
	protected static ref array<ref LF_LedgerEntry> s_aLedger;

	//------------------------------------------------------------------------------------------------
	static void Take()
	{
		s_aBank = LF_ATMManager.ExportAll();
		s_aLedger = LF_Ledger.ExportAll();
	}

	//------------------------------------------------------------------------------------------------
	static void Apply()
	{
		if (s_aBank)
			LF_ATMManager.ApplyAll(s_aBank);

		LF_Ledger.ApplyAll(s_aLedger);
	}
}
