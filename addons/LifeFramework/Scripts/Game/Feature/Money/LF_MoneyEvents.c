//------------------------------------------------------------------------------------------------
//! Money events on the server bus (docs/architecture/events.md). Fields are plain values: uids and
//! ints, never IEntity or EntityID. Published on the server only, after the state change happened.
//! A new event of this feature is appended to this file, not to a central list.

//! Cash or bank balance moved. m_iAmount is positive; m_bCredit says the direction; m_sSource is
//! the reason key ("atm-deposit", "shop-buy", ...). One event per account side: an ATM deposit is
//! a cash debit and a bank credit. Published by LF_MoneyUtils (cash) and LF_BankAccount (bank).
class LF_OnMoneyMoved : LF_Event
{
	string m_sPlayerUid;
	int m_iAmount;
	string m_sSource;
	//! LF_Ledger.ACCOUNT_CASH or ACCOUNT_BANK. Empty on an event built with Create.
	string m_sAccount;
	//! True when the balance went up. Meaningful with m_sAccount.
	bool m_bCredit;
	//! The balance of m_sAccount after the change.
	int m_iBalanceAfter;
	//! The other character of a hand-over, empty for a system source or sink.
	string m_sCounterpartyUid;

	static LF_OnMoneyMoved CreateFull(string playerUid, string account, int amount, bool credit, string reason, int balanceAfter, string counterpartyUid)
	{
		LF_OnMoneyMoved e = LF_OnMoneyMoved.Create(playerUid, amount, reason);
		e.m_sAccount = account;
		e.m_bCredit = credit;
		e.m_iBalanceAfter = balanceAfter;
		e.m_sCounterpartyUid = counterpartyUid;
		return e;
	}

	static LF_OnMoneyMoved Create(string playerUid, int amount, string source)
	{
		LF_OnMoneyMoved e = new LF_OnMoneyMoved();
		e.m_sPlayerUid = playerUid;
		e.m_iAmount = amount;
		e.m_sSource = source;
		return e;
	}
}
