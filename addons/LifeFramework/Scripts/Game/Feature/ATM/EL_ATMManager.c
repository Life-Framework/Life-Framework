class EL_ATMManager : Managed
{
	//! Largest single deposit or withdrawal the server accepts. Guards the
	//! account balance against hostile oversized RPC payloads.
	static const int MAX_TRANSACTION_AMOUNT = 1000000;

	protected static ref EL_ATMManager s_Instance;
	protected ref map<string, ref EL_BankAccount> m_mAccounts = new map<string, ref EL_BankAccount>();

	//------------------------------------------------------------------------------------------------
	//! Whether an amount is a valid single transaction. Rejects zero, negative
	//! and oversized amounts at the RPC boundary.
	static bool IsValidAmount(int amount)
	{
		if (amount <= 0)
			return false;
		return amount <= MAX_TRANSACTION_AMOUNT;
	}

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
		if (!s_Instance)
			s_Instance = new EL_ATMManager();

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
		if (!account || account.GetPersistentId().IsEmpty())
		{
			EL_Debug.Error("ATM", "account registration rejected: empty account id");
			return;
		}

		m_mAccounts.Set(account.GetPersistentId(), account);
	}

	//------------------------------------------------------------------------------------------------
	EL_BankAccount CreateAccount(string accountId)
	{
		if (accountId.IsEmpty())
		{
			EL_Debug.Error("ATM", "account creation rejected: empty account id");
			return null;
		}

		EL_BankAccount existing = m_mAccounts.Get(accountId);
		if (existing)
			return existing;

		EL_BankAccount account = EL_BankAccount.Create(accountId);
		AddAccount(account);
		return account;
	}

	//------------------------------------------------------------------------------------------------
	bool Deposit(string accountId, int amount)
	{
		if (!Replication.IsServer())
		{
			EL_Debug.Warn("ATM", "deposit rejected: not running on the server");
			return false;
		}

		if (!IsValidAmount(amount))
		{
			EL_Debug.Warn("ATM", string.Format("deposit rejected: invalid amount %1", amount));
			return false;
		}

		EL_BankAccount account = GetAccount(accountId);
		if (account)
		{
			account.Deposit(amount);
			EL_Debug.Log("ATM", string.Format("deposit account=%1 amount=%2", accountId, amount));
			return true;
		}

		EL_Debug.Log("ATM", string.Format("deposit failed: no account %1", accountId));
		return false;
	}

	//------------------------------------------------------------------------------------------------
	bool Withdraw(string accountId, int amount)
	{
		if (!Replication.IsServer())
		{
			EL_Debug.Warn("ATM", "withdraw rejected: not running on the server");
			return false;
		}

		if (!IsValidAmount(amount))
		{
			EL_Debug.Warn("ATM", string.Format("withdraw rejected: invalid amount %1", amount));
			return false;
		}

		EL_BankAccount account = GetAccount(accountId);
		if (account)
		{
			bool ok = account.Withdraw(amount);
			EL_Debug.Log("ATM", string.Format("withdraw account=%1 amount=%2 ok=%3", accountId, amount, ok));
			return ok;
		}

		EL_Debug.Log("ATM", string.Format("withdraw failed: no account %1", accountId));
		return false;
	}
};
