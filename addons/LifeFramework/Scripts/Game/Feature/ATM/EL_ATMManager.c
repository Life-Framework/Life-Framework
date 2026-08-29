class EL_ATMManager : Managed
{
	protected static EL_ATMManager s_Instance;
	protected ref map<string, ref EL_BankAccount> m_mAccounts = new map<string, ref EL_BankAccount>();

	//------------------------------------------------------------------------------------------------
	//! Persistence seam: export every cached bank account as a record for the serializer.
	//! \return Array of bank account records (never null), one per cached account.
	static array<ref EL_BankAccountRecord> ExportAll()
	{
		EL_ATMManager instance = GetInstance();
		if (!instance)
			instance = new EL_ATMManager();

		array<ref EL_BankAccountRecord> records = new array<ref EL_BankAccountRecord>();
		foreach (string accountId, EL_BankAccount account : instance.m_mAccounts)
		{
			if (!account)
				continue;

			records.Insert(EL_BankAccountRecord.Create(account.GetPersistentId(), account.GetBalance()));
		}

		return records;
	}

	//------------------------------------------------------------------------------------------------
	//! Persistence seam: apply records back into the cache (idempotent). Existing ids are
	//! overwritten so a re-apply to a live session converges on the saved state.
	//! \param records Records read from the save.
	static void ApplyAll(notnull array<ref EL_BankAccountRecord> records)
	{
		EL_ATMManager instance = GetInstance();
		if (!instance)
			instance = new EL_ATMManager();

		foreach (EL_BankAccountRecord record : records)
		{
			if (!record)
				continue;

			EL_BankAccount account = EL_BankAccount.Create(record.m_sPersistentId);
			account.SetBalance(record.m_iBalance);
			instance.AddAccount(account);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Returns the cached account for an id, creating and caching a new one when absent.
	//! \param accountId Account id (character persistence id).
	//! \return The account for the id (never null).
	static EL_BankAccount GetOrCreate(string accountId)
	{
		EL_ATMManager instance = GetInstance();
		if (!instance)
			instance = new EL_ATMManager();

		EL_BankAccount account = instance.m_mAccounts.Get(accountId);
		if (!account)
			account = instance.CreateAccount(accountId);

		return account;
	}

	//------------------------------------------------------------------------------------------------
	static EL_ATMManager GetInstance()
	{
		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	//! Clears the cache and drops the singleton. Used by the test suite between
	//! cases and by the game mode teardown.
	static void Reset()
	{
		if (s_Instance)
			s_Instance.m_mAccounts.Clear();
		s_Instance = null;
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
};