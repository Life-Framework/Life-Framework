//------------------------------------------------------------------------------------------------
//! One recorded money change (docs/architecture/events.md, Feature/Money/Feature.md).
//! Persisted as is, so the fields are plain values. The balance of the account after the change
//! is stored so the log can be checked against the stored balance.
//------------------------------------------------------------------------------------------------
class LF_LedgerEntry
{
	//! Increases by one per entry, never reused (the sequence continues after the highest loaded).
	int m_iId;
	//! Unix seconds on the server when the change happened.
	int m_iTime;
	string m_sPlayerUid;
	//! LF_Ledger.ACCOUNT_CASH or LF_Ledger.ACCOUNT_BANK.
	string m_sAccount;
	//! Signed: positive credits the account, negative debits it.
	int m_iDelta;
	//! The reason key of the path that moved it ("atm-deposit", "shop-buy", ...).
	string m_sReason;
	//! The other character of a hand-over; empty for a system source or sink.
	string m_sCounterpartyUid;
	int m_iBalanceAfter;

	//------------------------------------------------------------------------------------------------
	LF_LedgerEntry Copy()
	{
		LF_LedgerEntry copy = new LF_LedgerEntry();
		copy.m_iId = m_iId;
		copy.m_iTime = m_iTime;
		copy.m_sPlayerUid = m_sPlayerUid;
		copy.m_sAccount = m_sAccount;
		copy.m_iDelta = m_iDelta;
		copy.m_sReason = m_sReason;
		copy.m_sCounterpartyUid = m_sCounterpartyUid;
		copy.m_iBalanceAfter = m_iBalanceAfter;
		return copy;
	}
}

//------------------------------------------------------------------------------------------------
//! The bus subscriber that writes the ledger. Features never call the ledger: every cash and bank
//! change publishes LF_OnMoneyMoved from its choke point and this records it.
class LF_LedgerSubscriber : LF_EventSubscriber
{
	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnMoneyMoved moved = LF_OnMoneyMoved.Cast(busEvent);
		if (!moved)
			return false;

		return LF_Ledger.Record(moved);
	}
}

//------------------------------------------------------------------------------------------------
//! Server money ledger: an append-only log of every cash and bank change, oldest first, with
//! retention from LF_LedgerConfig. Persisted as the "ledger" payload.
//!
//! What it proves: for the bank, each entry's balance-after equals the one before it plus the
//! change, so the balance can be rebuilt from the log (RebuildBalance) and compared with the
//! stored balance (VerifyBank). Cash is items in a character's inventory, so a pick-up or a drop
//! outside LF_MoneyUtils is not in the log; its entries are a trail, not a chain.
class LF_Ledger
{
	static const string FEATURE = "Ledger";
	static const string ACCOUNT_CASH = "cash";
	static const string ACCOUNT_BANK = "bank";

	protected static ref array<ref LF_LedgerEntry> s_aEntries;
	protected static int s_iLastId;
	protected static ref LF_LedgerSubscriber s_Subscriber;

	//------------------------------------------------------------------------------------------------
	protected static array<ref LF_LedgerEntry> Entries()
	{
		if (!s_aEntries)
			s_aEntries = new array<ref LF_LedgerEntry>();

		return s_aEntries;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_aEntries = null;
		s_iLastId = 0;
		s_Subscriber = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: subscribes the ledger to LF_OnMoneyMoved once. Called when the game mode starts.
	//! \return True when the ledger is subscribed after the call.
	static bool EnsureSubscribed()
	{
		if (!Replication.IsServer())
			return false;

		if (s_Subscriber && LF_EventBus.SubscriberCount(LF_OnMoneyMoved) > 0)
			return true;

		s_Subscriber = new LF_LedgerSubscriber();
		return LF_EventBus.Subscribe(LF_OnMoneyMoved, s_Subscriber);
	}

	//------------------------------------------------------------------------------------------------
	//! Appends one change. Only the subscriber calls it.
	//! \return False when the event cannot be recorded (no character, no amount).
	static bool Record(notnull LF_OnMoneyMoved moved)
	{
		// An event built with Create (no account side) is not a ledger line.
		if (moved.m_sAccount != ACCOUNT_CASH && moved.m_sAccount != ACCOUNT_BANK)
			return true;

		if (moved.m_sPlayerUid.IsEmpty() || moved.m_iAmount <= 0)
		{
			LF_Debug.Warn(FEATURE, string.Format("not recorded: money event %1 has no character or no amount", moved.m_sSource));
			return false;
		}

		int perfToken = LF_Perf.Begin();
		LF_LedgerEntry entry = new LF_LedgerEntry();
		s_iLastId++;
		entry.m_iId = s_iLastId;
		entry.m_iTime = System.GetUnixTime();
		entry.m_sPlayerUid = moved.m_sPlayerUid;
		entry.m_sAccount = moved.m_sAccount;
		entry.m_iDelta = moved.m_iAmount;
		if (!moved.m_bCredit)
			entry.m_iDelta = -moved.m_iAmount;

		entry.m_sReason = moved.m_sSource;
		entry.m_sCounterpartyUid = moved.m_sCounterpartyUid;
		entry.m_iBalanceAfter = moved.m_iBalanceAfter;
		Entries().Insert(entry);
		LF_Debug.Info(FEATURE, string.Format("#%1 %2 %3 %4 (%5) balance %6 counterparty '%7'", entry.m_iId, entry.m_sPlayerUid, entry.m_sAccount, entry.m_iDelta, entry.m_sReason, entry.m_iBalanceAfter, entry.m_sCounterpartyUid));
		Prune();
		LF_Perf.End("Ledger.record", perfToken);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Drops the oldest entries beyond the configured count and age. Age 0 keeps by count only.
	protected static void Prune()
	{
		array<ref LF_LedgerEntry> all = Entries();
		int maxEntries = LF_LedgerConfig.MaxEntries();
		while (all.Count() > maxEntries)
		{
			all.RemoveOrdered(0);
		}

		int maxDays = LF_LedgerConfig.MaxAgeDays();
		if (maxDays <= 0)
			return;

		int oldest = System.GetUnixTime() - maxDays * 86400;
		while (all.Count() > 0 && all.Get(0).m_iTime < oldest)
		{
			all.RemoveOrdered(0);
		}
	}

	//------------------------------------------------------------------------------------------------
	static int Count()
	{
		return Entries().Count();
	}

	//------------------------------------------------------------------------------------------------
	//! The newest entries of one character, newest first (the incident log lists these).
	//! \param[in] account ACCOUNT_CASH, ACCOUNT_BANK, or empty for both.
	//! \param[in] limit Most entries returned.
	static void Last(string playerUid, string account, int limit, notnull array<ref LF_LedgerEntry> outEntries)
	{
		outEntries.Clear();
		array<ref LF_LedgerEntry> all = Entries();
		for (int i = all.Count() - 1; i >= 0 && outEntries.Count() < limit; i--)
		{
			LF_LedgerEntry entry = all.Get(i);
			if (entry.m_sPlayerUid != playerUid)
				continue;

			if (!account.IsEmpty() && entry.m_sAccount != account)
				continue;

			outEntries.Insert(entry.Copy());
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Rebuilds the balance of one account from the retained log: the opening balance implied by the
	//! oldest entry plus every change after it.
	//! \param[out] outConsistent False when an entry's balance-after does not equal the one before
	//!             it plus its change (a change happened that the log did not see).
	//! \return The rebuilt balance, or 0 when the log holds nothing for the account.
	static int RebuildBalance(string playerUid, string account, out bool outConsistent)
	{
		outConsistent = true;
		bool first = true;
		int balance = 0;
		foreach (LF_LedgerEntry entry : Entries())
		{
			if (entry.m_sPlayerUid != playerUid || entry.m_sAccount != account)
				continue;

			if (first)
				balance = entry.m_iBalanceAfter - entry.m_iDelta;

			first = false;
			balance += entry.m_iDelta;
			if (balance != entry.m_iBalanceAfter)
			{
				outConsistent = false;
				balance = entry.m_iBalanceAfter;
			}
		}

		return balance;
	}

	//------------------------------------------------------------------------------------------------
	//! Compares the rebuilt bank balance with the stored one. A character with no bank entries has
	//! nothing to compare and passes when the stored balance is zero.
	//! \return True when the log is a consistent chain that ends at the stored balance.
	static bool VerifyBank(string playerUid, int storedBalance)
	{
		bool consistent;
		int rebuilt = RebuildBalance(playerUid, ACCOUNT_BANK, consistent);
		bool ok = consistent && rebuilt == storedBalance;
		if (!ok)
			LF_Debug.Error(FEATURE, string.Format("bank of %1 does not match the ledger: stored %2, rebuilt %3, chain consistent %4", playerUid, storedBalance, rebuilt, consistent));

		return ok;
	}

	//------------------------------------------------------------------------------------------------
	//! \return A copy of the log for the save, oldest first.
	static array<ref LF_LedgerEntry> ExportAll()
	{
		array<ref LF_LedgerEntry> records = new array<ref LF_LedgerEntry>();
		foreach (LF_LedgerEntry entry : Entries())
		{
			records.Insert(entry.Copy());
		}

		return records;
	}

	//------------------------------------------------------------------------------------------------
	//! Replaces the log (idempotent: a second pass leaves the same log). The id sequence continues
	//! after the highest loaded. Retention applies at the next change.
	static void ApplyAll(array<ref LF_LedgerEntry> records)
	{
		s_aEntries = new array<ref LF_LedgerEntry>();
		s_iLastId = 0;
		if (!records)
			return;

		foreach (LF_LedgerEntry record : records)
		{
			if (!record || record.m_sPlayerUid.IsEmpty() || record.m_iId <= 0)
			{
				LF_Debug.Warn(FEATURE, "load skipped an entry with no character or id");
				continue;
			}

			s_aEntries.Insert(record.Copy());
			s_iLastId = Math.Max(s_iLastId, record.m_iId);
		}
	}
}
