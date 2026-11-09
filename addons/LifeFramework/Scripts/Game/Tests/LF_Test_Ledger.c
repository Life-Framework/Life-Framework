// red-proof: PENDING
// tier: WORLD
//! The money ledger records every change that reaches the bus (docs/architecture/events.md):
//! a bank deposit and withdrawal through LF_BankAccount, a transfer that sums to zero, the
//! newest-first listing the incident log reads, and the rebuild-and-compare check on the bank.
class LF_Test_Ledger : LF_Test
{
	protected static const string UID_A = "ledger-test-a";
	protected static const string UID_B = "ledger-test-b";

	override string GetName()
	{
		return "money/ledger-records";
	}

	override void Run(LF_TestContext ctx)
	{
		array<ref LF_LedgerEntry> before = LF_Ledger.ExportAll();
		LF_Ledger.ApplyAll(null);
		ctx.True(LF_Ledger.EnsureSubscribed(), "the ledger subscribes to LF_OnMoneyMoved");

		// Bank changes publish from LF_BankAccount itself; the ledger is not called by the account.
		LF_BankAccount account = LF_BankAccount.Create(UID_A);
		account.Deposit(500, "test-deposit");
		ctx.True(account.Withdraw(200, "test-withdraw"), "the withdrawal is covered");
		ctx.Equal(2, LF_Ledger.Count(), "both bank changes are in the ledger");

		array<ref LF_LedgerEntry> last = {};
		LF_Ledger.Last(UID_A, "", 20, last);
		ctx.Equal(2, last.Count(), "the character lists two transactions");
		if (last.Count() == 2)
		{
			ctx.EqualStr("test-withdraw", last.Get(0).m_sReason, "newest first");
			ctx.Equal(-200, last.Get(0).m_iDelta, "the withdrawal is a debit");
			ctx.Equal(300, last.Get(0).m_iBalanceAfter, "the balance after the withdrawal");
			ctx.Equal(500, last.Get(1).m_iDelta, "the deposit is a credit");
			ctx.EqualStr(LF_Ledger.ACCOUNT_BANK, last.Get(1).m_sAccount, "the bank side is recorded as bank");
			ctx.True(last.Get(1).m_iId < last.Get(0).m_iId, "ids increase");
		}

		// A refused withdrawal changes nothing and records nothing.
		ctx.False(account.Withdraw(9999, "test-refused"), "an overdraft is refused");
		ctx.Equal(2, LF_Ledger.Count(), "a refused change is not in the ledger");

		// Rebuild from the log and compare with the stored balance.
		bool consistent;
		ctx.Equal(300, LF_Ledger.RebuildBalance(UID_A, LF_Ledger.ACCOUNT_BANK, consistent), "the balance rebuilds from the log");
		ctx.True(consistent, "the chain is consistent");
		ctx.True(LF_Ledger.VerifyBank(UID_A, account.GetBalance()), "the log matches the stored balance");
		ctx.False(LF_Ledger.VerifyBank(UID_A, 301), "a stored balance the log does not explain fails the check");

		// A hand-over: B's debit and A's credit sum to zero and name each other.
		LF_EventBus.Publish(LF_OnMoneyMoved.CreateFull(UID_B, LF_Ledger.ACCOUNT_CASH, 70, false, "give-cash", 30, UID_A));
		LF_EventBus.Publish(LF_OnMoneyMoved.CreateFull(UID_A, LF_Ledger.ACCOUNT_CASH, 70, true, "give-cash", 70, UID_B));
		array<ref LF_LedgerEntry> cashA = {};
		array<ref LF_LedgerEntry> cashB = {};
		LF_Ledger.Last(UID_A, LF_Ledger.ACCOUNT_CASH, 20, cashA);
		LF_Ledger.Last(UID_B, LF_Ledger.ACCOUNT_CASH, 20, cashB);
		ctx.Equal(1, cashA.Count(), "the receiver has one cash line");
		ctx.Equal(1, cashB.Count(), "the giver has one cash line");
		if (cashA.Count() == 1 && cashB.Count() == 1)
		{
			int received = cashA.Get(0).m_iDelta;
			int given = cashB.Get(0).m_iDelta;
			ctx.Equal(70, received, "the receiver is credited");
			ctx.Equal(0, received + given, "the transfer sums to zero");
			ctx.EqualStr(UID_B, cashA.Get(0).m_sCounterpartyUid, "the receiver names the giver");
			ctx.EqualStr(UID_A, cashB.Get(0).m_sCounterpartyUid, "the giver names the receiver");
		}

		// An event without an account side (an old-style publisher) is not a ledger line.
		LF_EventBus.Publish(LF_OnMoneyMoved.Create(UID_A, 5, "legacy"));
		ctx.Equal(4, LF_Ledger.Count(), "a side-less money event adds no line");

		LF_Ledger.ApplyAll(before);
	}
}
