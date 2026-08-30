class EL_BankAccount : Managed
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
	void SetBalance(int value)
	{
		m_iBalance = Math.Max(value, 0);
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