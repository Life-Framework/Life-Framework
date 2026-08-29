class EL_ATMManager : Managed
{
	protected static EL_ATMManager s_Instance;
	protected ref map<string, ref EL_BankAccount> m_mAccounts = new map<string, ref EL_BankAccount>();

	//------------------------------------------------------------------------------------------------
	static EL_ATMManager GetInstance()
	{
		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	void EL_ATMManager()
	{
		s_Instance = this;
	}

	//------------------------------------------------------------------------------------------------
	void ~EL_ATMManager()
	{
		s_Instance = null;
	}

	//------------------------------------------------------------------------------------------------
	EL_BankAccount GetAccount(string accountId)
	{
		return m_mAccounts.Get(accountId);
	}

	//------------------------------------------------------------------------------------------------
	void AddAccount(EL_BankAccount account)
	{
		m_mAccounts.Set(account.GetPersistentId(), account);
	}

	//------------------------------------------------------------------------------------------------
	EL_BankAccount CreateAccount(string accountId)
	{
		EL_BankAccount account = EL_BankAccount.Create(accountId);
		AddAccount(account);
		return account;
	}

	//------------------------------------------------------------------------------------------------
	bool Deposit(string accountId, int amount)
	{
		EL_BankAccount account = GetAccount(accountId);
		if (account)
		{
			account.Deposit(amount);
			return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	bool Withdraw(string accountId, int amount)
	{
		EL_BankAccount account = GetAccount(accountId);
		if (account)
		{
			return account.Withdraw(amount);
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Async loading of a bank account
	//! \param accountId The account ID
	//! \param callback Async callback to handle the result
	void LoadAccountAsync(string accountId, notnull EDF_DataCallbackSingle<EL_BankAccount> callback)
	{
		EL_BankAccount account = m_mAccounts.Get(accountId);
		if (account)
		{
			callback.Invoke(account);
			return;
		}

		auto processorCallback = EL_ATMManagerProcessorCallback.Create(accountId, callback);
		EPF_PersistentScriptedStateLoader<EL_BankAccount>.LoadAsync(accountId, processorCallback);
	}
};

class EL_ATMManagerProcessorCallback : EDF_DataCallbackSingle<EL_BankAccount>
{
	string m_sAccountId;
	ref EDF_DataCallbackSingle<EL_BankAccount> m_pCallback;

	//------------------------------------------------------------------------------------------------
	override void OnComplete(EL_BankAccount data, Managed context)
	{
		EL_BankAccount result = data;
		if (!result)
			result = EL_ATMManager.GetInstance().CreateAccount(m_sAccountId);

		if (m_pCallback)
			m_pCallback.Invoke(result);
	}

	//------------------------------------------------------------------------------------------------
	static EL_ATMManagerProcessorCallback Create(string accountId, EDF_DataCallbackSingle<EL_BankAccount> callback)
	{
		EL_ATMManagerProcessorCallback instance();
		instance.m_sAccountId = accountId;
		instance.m_pCallback = callback;
		return instance;
	}
};