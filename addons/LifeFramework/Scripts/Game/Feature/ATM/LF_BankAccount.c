class LF_BankAccount : Managed
{
	protected string m_sPersistentId;
	protected int m_iBalance;

	//! The ledger reason of a bank change whose caller named none.
	static const string REASON_BANK = "bank";

	//------------------------------------------------------------------------------------------------
	//! Server: announces a balance change on the bus (the ledger records it). The account id is the
	//! character's player uid. A change on an account with no id, or off the server, is not announced.
	protected void PublishMoved(int amount, bool credit, string reason, string counterpartyUid)
	{
		if (amount <= 0 || m_sPersistentId.IsEmpty() || !Replication.IsServer())
			return;

		LF_EventBus.Publish(LF_OnMoneyMoved.CreateFull(m_sPersistentId, LF_Ledger.ACCOUNT_BANK, amount, credit, reason, m_iBalance, counterpartyUid));
	}

	//------------------------------------------------------------------------------------------------
	string GetPersistentId()
	{
		return m_sPersistentId;
	}

	//------------------------------------------------------------------------------------------------
	void SetPersistentId(string id)
	{
		m_sPersistentId = id;
	}

	//------------------------------------------------------------------------------------------------
	int GetBalance()
	{
		return m_iBalance;
	}

	//------------------------------------------------------------------------------------------------
	//! Sets the balance as stored. A negative value is debt (a hospital bill the player could not
	//! cover) and is kept, so a saved debt loads as debt.
	void SetBalance(int value)
	{
		m_iBalance = value;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True while the balance is below zero. Deposits pay the debt off first (they add to
	//!         the balance); withdrawals are refused until the balance covers them.
	bool IsInDebt()
	{
		return m_iBalance < 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Server-decided charge that may go into debt (the hospital bill). Withdraw stays the player
	//! path and never goes below zero.
	//! \param amount Amount to charge; zero or less charges nothing.
	//! \param allowDebt When false, only what the account holds is charged.
	//! \return The amount actually charged.
	int Charge(int amount, bool allowDebt, string reason = REASON_BANK)
	{
		if (amount <= 0)
			return 0;

		int charged = amount;
		if (!allowDebt)
			charged = Math.Max(Math.Min(amount, m_iBalance), 0);

		m_iBalance -= charged;
		PublishMoved(charged, false, reason, "");
		return charged;
	}

	//------------------------------------------------------------------------------------------------
	void Deposit(int amount, string reason = REASON_BANK, string counterpartyUid = "")
	{
		if (amount > 0)
		{
			m_iBalance += amount;
			PublishMoved(amount, true, reason, counterpartyUid);
		}
	}

	//------------------------------------------------------------------------------------------------
	bool Withdraw(int amount, string reason = REASON_BANK, string counterpartyUid = "")
	{
		if (amount > 0 && m_iBalance >= amount)
		{
			m_iBalance -= amount;
			PublishMoved(amount, false, reason, counterpartyUid);
			return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	static LF_BankAccount Create(string accountId)
	{
		LF_BankAccount account();
		account.SetPersistentId(accountId);
		return account;
	}
};