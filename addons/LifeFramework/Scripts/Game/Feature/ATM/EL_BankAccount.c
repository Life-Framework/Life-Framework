class EL_BankAccount : EPF_PersistentScriptedState
{
	protected int m_iBalance;

	//------------------------------------------------------------------------------------------------
	int GetBalance()
	{
		return m_iBalance;
	}

	//------------------------------------------------------------------------------------------------
	void SetBalance(int value)
	{
		m_iBalance = value;
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
	static EL_BankAccount Create(string accountId)
	{
		EL_BankAccount account();
		account.SetPersistentId(accountId);
		return account;
	}
};