// LF_BankTransfer.c: sends money from one bank account to another by number, exactly once (#3319, #638).
// Server only. The rules (LF_BankTransferRules, #3318) decide; this moves the money and keeps the journal
// that makes a retry, a crash or a restart pay at most once. Nothing calls Send yet: the transfer RPC and
// the terminal screen are a later card (money review there too).

//! How one Send ended.
enum LF_EBankTransferStatus
{
	//! The amount and fee left the sender and the amount reached the recipient.
	DONE,
	//! The rules said no (see LF_BankTransferOutcome.m_eRefusal); nothing moved.
	REFUSED,
	//! This transfer id was already used; nothing moved.
	REPLAY,
	//! The credit failed after the debit; the sender was paid back once.
	REFUNDED,
	//! Not on the server, the bank is switched off, or the id or the numbers are empty; nothing moved.
	REJECTED
}

//------------------------------------------------------------------------------------------------
//! The result of one Send, for the caller to turn into a toast.
class LF_BankTransferOutcome : Managed
{
	LF_EBankTransferStatus m_eStatus = LF_EBankTransferStatus.REJECTED;
	//! Set when m_eStatus is REFUSED.
	LF_EBankTransferRefusal m_eRefusal = LF_EBankTransferRefusal.NONE;
	int m_iAmount;
	int m_iFee;

	//------------------------------------------------------------------------------------------------
	bool IsDone()
	{
		return m_eStatus == LF_EBankTransferStatus.DONE;
	}

	//------------------------------------------------------------------------------------------------
	static LF_BankTransferOutcome Of(LF_EBankTransferStatus status)
	{
		LF_BankTransferOutcome outcome();
		outcome.m_eStatus = status;
		return outcome;
	}
}

//------------------------------------------------------------------------------------------------
//! One transfer in the journal: what moved, between whom, and how far it got. Saved in the
//! "bank_transfer" section.
class LF_BankTransferRow : Managed
{
	static const int ROW_VERSION = 1;
	//! The sender is debited and the recipient is not credited yet. A save never holds this state unless the
	//! server died between the two steps; the load pays the sender back.
	static const int STATE_DEBITED = 1;
	//! Both sides are done.
	static const int STATE_DONE = 2;
	//! The sender was paid back; the recipient got nothing.
	static const int STATE_REFUNDED = 3;

	int m_iVersion;
	string m_sId;
	string m_sFrom;
	string m_sTo;
	int m_iAmount;
	int m_iFee;
	//! Unix seconds on the server when the transfer started.
	int m_iTime;
	int m_iState;

	//------------------------------------------------------------------------------------------------
	static LF_BankTransferRow Create(string id, string from, string to, int amount, int fee, int time)
	{
		LF_BankTransferRow row();
		row.m_iVersion = ROW_VERSION;
		row.m_sId = id;
		row.m_sFrom = from;
		row.m_sTo = to;
		row.m_iAmount = amount;
		row.m_iFee = fee;
		row.m_iTime = time;
		row.m_iState = STATE_DEBITED;
		return row;
	}

	//------------------------------------------------------------------------------------------------
	LF_BankTransferRow Copy()
	{
		LF_BankTransferRow copy = LF_BankTransferRow.Create(m_sId, m_sFrom, m_sTo, m_iAmount, m_iFee, m_iTime);
		copy.m_iState = m_iState;
		return copy;
	}
}

//------------------------------------------------------------------------------------------------
//! The transfer entry point and its journal. An offline recipient is an account in the bank cache like any
//! other (the save loads every account), so offline and online are the same path. One transfer id is one
//! transfer: the journal row exists before the first coin moves and a repeated id is refused. The debit,
//! the credit and the refund are single synchronous steps on the server, so a save never sees half of
//! one; the DEBITED state is the safety net for a crash between the two steps.
class LF_BankTransfer
{
	static const string FEATURE = "BankTransfer";
	//! Ledger reasons; the transfer id follows the colon, so the debit and credit rows of one transfer
	//! share it.
	static const string REASON_OUT = "bank-transfer-out:";
	static const string REASON_IN = "bank-transfer-in:";
	static const string REASON_REFUND = "bank-transfer-refund:";
	//! The window the daily limit counts over.
	static const int DAY_SECONDS = 86400; // lf-fixed: what "a day" means for the limit, a real 24 h
	//! How long a transfer id is remembered, so a late replay is still refused.
	static const int RETAIN_SECONDS = 7 * 86400; // lf-fixed: bounds the save section, not a gameplay knob
	static const int MAX_ROWS = 4000; // lf-fixed: memory bound on the journal

	protected static ref array<ref LF_BankTransferRow> s_aRows;
	protected static int s_iSequence;
	//! Test seam: the next credit fails, as if the recipient's account could not take it.
	protected static bool s_bFailNextCredit;
	//! Test seam: the next Send stops right after the debit, as if the server died there.
	protected static bool s_bStopAfterDebit;

	//------------------------------------------------------------------------------------------------
	protected static array<ref LF_BankTransferRow> Rows()
	{
		if (!s_aRows)
			s_aRows = new array<ref LF_BankTransferRow>();

		return s_aRows;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_aRows = null;
		s_iSequence = 0;
		s_bFailNextCredit = false;
		s_bStopAfterDebit = false;
	}

	//------------------------------------------------------------------------------------------------
	static void FailNextCreditForTests()
	{
		s_bFailNextCredit = true;
	}

	//------------------------------------------------------------------------------------------------
	static void StopAfterDebitForTests()
	{
		s_bStopAfterDebit = true;
	}

	//------------------------------------------------------------------------------------------------
	//! A fresh transfer id for the server to hand a new request: the sender, the time and a counter.
	static string NewTransferId(string fromAccount)
	{
		s_iSequence++;
		return string.Format("%1-%2-%3", fromAccount, System.GetUnixTime(), s_iSequence);
	}

	//------------------------------------------------------------------------------------------------
	//! \return The journal row of a transfer id, or null.
	static LF_BankTransferRow Find(string transferId)
	{
		foreach (LF_BankTransferRow row : Rows())
		{
			if (row.m_sId == transferId)
				return row;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! What one account has sent since a time: amounts of finished transfers, fees and refunded ones not counted.
	static int SentSince(string fromAccount, int since)
	{
		int total = 0;
		foreach (LF_BankTransferRow row : Rows())
		{
			if (row.m_sFrom == fromAccount && row.m_iState != LF_BankTransferRow.STATE_REFUNDED && row.m_iTime >= since)
				total += row.m_iAmount;
		}

		return total;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: sends money by account number. The client names only the number and the amount; the id
	//! is the server's (NewTransferId) or a retry's, and a used id is refused.
	static LF_BankTransferOutcome Send(string transferId, string fromAccount, string toAccount, int amount)
	{
		return Execute(LF_BankTransferConfig.GetOrDefaults(), transferId, fromAccount, toAccount, amount, System.GetUnixTime());
	}

	//------------------------------------------------------------------------------------------------
	//! Send with the config and the clock handed in (the test path).
	static LF_BankTransferOutcome Execute(LF_BankTransferConfig config, string transferId, string fromAccount, string toAccount, int amount, int now)
	{
		if (!Replication.IsServer())
		{
			LF_Debug.Warn(FEATURE, "transfer rejected: not running on the server");
			return LF_BankTransferOutcome.Of(LF_EBankTransferStatus.REJECTED);
		}

		if (transferId.IsEmpty() || fromAccount.IsEmpty() || toAccount.IsEmpty())
		{
			LF_Debug.Warn(FEATURE, "transfer rejected: empty transfer id or account number");
			return LF_BankTransferOutcome.Of(LF_EBankTransferStatus.REJECTED);
		}

		if (LF_Systems.Refuses(LF_ATMManager.SYSTEM, "transfer " + transferId))
			return LF_BankTransferOutcome.Of(LF_EBankTransferStatus.REJECTED);

		if (Find(transferId))
		{
			LF_Debug.Warn(FEATURE, string.Format("transfer %1 refused: the id was already used", transferId));
			return LF_BankTransferOutcome.Of(LF_EBankTransferStatus.REPLAY);
		}

		LF_ATMManager bank = LF_ATMManager.GetInstance();
		LF_BankAccount from = bank.GetAccount(fromAccount);
		LF_BankAccount to = bank.GetAccount(toAccount);
		int balance;
		if (from)
			balance = from.GetBalance();

		int sentToday = SentSince(fromAccount, now - DAY_SECONDS);
		LF_BankTransferVerdict verdict = LF_BankTransferRules.Evaluate(config, fromAccount, toAccount, to != null, amount, balance, sentToday);
		if (!verdict.IsAllowed() || !from || !to)
		{
			LF_BankTransferOutcome refused = LF_BankTransferOutcome.Of(LF_EBankTransferStatus.REFUSED);
			refused.m_eRefusal = verdict.m_eRefusal;
			if (verdict.IsAllowed())
				refused.m_eRefusal = LF_EBankTransferRefusal.NOT_ENOUGH_BALANCE;

			LF_Debug.Info(FEATURE, string.Format("transfer %1 refused: %2", transferId, typename.EnumToString(LF_EBankTransferRefusal, refused.m_eRefusal)));
			return refused;
		}

		// The journal row comes first, so an id is never usable twice even if a later step stops.
		LF_BankTransferRow row = LF_BankTransferRow.Create(transferId, fromAccount, toAccount, verdict.m_iAmount, verdict.m_iFee, now);
		Rows().Insert(row);
		Prune(now);

		if (!from.Withdraw(verdict.TotalCharge(), REASON_OUT + transferId, toAccount))
		{
			Rows().RemoveItem(row);
			LF_Debug.Warn(FEATURE, string.Format("transfer %1 refused: the debit did not go", transferId));
			LF_BankTransferOutcome noMoney = LF_BankTransferOutcome.Of(LF_EBankTransferStatus.REFUSED);
			noMoney.m_eRefusal = LF_EBankTransferRefusal.NOT_ENOUGH_BALANCE;
			return noMoney;
		}

		if (s_bStopAfterDebit)
		{
			s_bStopAfterDebit = false;
			LF_Debug.Warn(FEATURE, string.Format("transfer %1 stopped after the debit (test)", transferId));
			return LF_BankTransferOutcome.Of(LF_EBankTransferStatus.REJECTED);
		}

		LF_BankTransferOutcome outcome = LF_BankTransferOutcome.Of(LF_EBankTransferStatus.DONE);
		outcome.m_iAmount = verdict.m_iAmount;
		outcome.m_iFee = verdict.m_iFee;
		if (s_bFailNextCredit)
		{
			s_bFailNextCredit = false;
			Refund(row, from);
			outcome.m_eStatus = LF_EBankTransferStatus.REFUNDED;
			return outcome;
		}

		to.Deposit(verdict.m_iAmount, REASON_IN + transferId, fromAccount);
		row.m_iState = LF_BankTransferRow.STATE_DONE;
		LF_Debug.Log(FEATURE, string.Format("transfer %1: %2 sent %3 (fee %4) to %5", transferId, fromAccount, verdict.m_iAmount, verdict.m_iFee, toAccount));
		return outcome;
	}

	//------------------------------------------------------------------------------------------------
	//! Pays the sender back what a debited transfer took, once: the state moves with the payment.
	protected static void Refund(notnull LF_BankTransferRow row, notnull LF_BankAccount from)
	{
		if (row.m_iState != LF_BankTransferRow.STATE_DEBITED)
			return;

		row.m_iState = LF_BankTransferRow.STATE_REFUNDED;
		from.Deposit(row.m_iAmount + row.m_iFee, REASON_REFUND + row.m_sId, row.m_sTo);
		LF_Debug.Warn(FEATURE, string.Format("transfer %1 refunded %2 to %3", row.m_sId, row.m_iAmount + row.m_iFee, row.m_sFrom));
	}

	//------------------------------------------------------------------------------------------------
	//! Drops rows past the retention window, then the oldest beyond the cap.
	protected static void Prune(int now)
	{
		array<ref LF_BankTransferRow> all = Rows();
		for (int i = all.Count() - 1; i >= 0; i--)
		{
			LF_BankTransferRow row = all.Get(i);
			if (row.m_iState != LF_BankTransferRow.STATE_DEBITED && row.m_iTime < now - RETAIN_SECONDS)
				all.RemoveOrdered(i);
		}

		while (all.Count() > MAX_ROWS)
		{
			all.RemoveOrdered(0);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! \return A copy of the journal for the save, oldest first.
	static array<ref LF_BankTransferRow> ExportRows()
	{
		array<ref LF_BankTransferRow> rows = new array<ref LF_BankTransferRow>();
		foreach (LF_BankTransferRow row : Rows())
		{
			rows.Insert(row.Copy());
		}

		return rows;
	}

	//------------------------------------------------------------------------------------------------
	//! Replaces the journal with the saved one and pays back every transfer the save holds as DEBITED.
	//! The load runs right after the bank payload replaced every balance with the saved ones, and those
	//! hold the debit, so the refund is right on every load of the same save: a second load ends in the
	//! same balances, not in a double refund.
	//! \return How many senders were paid back.
	static int ApplyRows(notnull array<ref LF_BankTransferRow> saved)
	{
		array<ref LF_BankTransferRow> merged = new array<ref LF_BankTransferRow>();
		int refunded;
		foreach (LF_BankTransferRow record : saved)
		{
			if (!record || record.m_sId.IsEmpty())
				continue;

			LF_BankTransferRow row = record.Copy();
			if (row.m_iState == LF_BankTransferRow.STATE_DEBITED)
			{
				Refund(row, LF_ATMManager.GetOrCreate(row.m_sFrom));
				refunded++;
			}

			merged.Insert(row);
		}

		s_aRows = merged;
		return refunded;
	}
}

//------------------------------------------------------------------------------------------------
//! Save section "bank_transfer" (#3319): the transfer journal, which keeps a used transfer id used across a
//! restart and pays back a sender whose transfer the server died in the middle of. Sections apply after the
//! bank payload, so the refund lands on the loaded balance.
class LF_BankTransferSavePayload : LF_SavePayload
{
	static const string NAME = "bank_transfer";

	//------------------------------------------------------------------------------------------------
	override string Name()
	{
		return NAME;
	}

	//------------------------------------------------------------------------------------------------
	override int Version()
	{
		return 1;
	}

	//------------------------------------------------------------------------------------------------
	override void WriteSection(notnull SaveContext context)
	{
		array<ref LF_BankTransferRow> rows = LF_BankTransfer.ExportRows();
		context.WriteValue("rows", rows);
		LF_Debug.Log("Persistence", string.Format("saved %1 bank transfer(s)", rows.Count()));
	}

	//------------------------------------------------------------------------------------------------
	override bool ReadSection(notnull LoadContext context, int savedVersion)
	{
		array<ref LF_BankTransferRow> rows = new array<ref LF_BankTransferRow>();
		if (!context.ReadValue("rows", rows))
		{
			LF_Debug.Warn(LF_BankTransfer.FEATURE, "bank transfer journal failed to read; kept the live journal");
			return false;
		}

		int refunded = LF_BankTransfer.ApplyRows(rows);
		LF_Debug.Info("Persistence", string.Format("read %1 bank transfer(s), %2 sender(s) paid back", rows.Count(), refunded));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! A save from before transfers has no journal.
	override void OnAbsent()
	{
		LF_Debug.Info(LF_BankTransfer.FEATURE, "save has no bank transfer journal; starting empty");
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_BankTransfer's statics (LF_StaticReset finds it by reflection).
class LF_BankTransfer_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_BankTransfer";
	}

	override void Apply()
	{
		LF_BankTransfer.ResetStatics();
	}
}
