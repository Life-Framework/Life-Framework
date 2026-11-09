class LF_BankAccount : Managed
{
	protected string m_sPersistentId;
	protected int m_iBalance;

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
	int Charge(int amount, bool allowDebt)
	{
		if (amount <= 0)
			return 0;

		int charged = amount;
		if (!allowDebt)
			charged = Math.Max(Math.Min(amount, m_iBalance), 0);

		m_iBalance -= charged;
		return charged;
	}

	//------------------------------------------------------------------------------------------------
	void Deposit(int amount)
	{
		if (amount > 0)
			m_iBalance += amount;
	}

	//------------------------------------------------------------------------------------------------
	bool Withdraw(int amount)
	{
		if (amount > 0 && m_iBalance >= amount)
		{
			m_iBalance -= amount;
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