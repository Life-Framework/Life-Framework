class LF_ATMManager : Managed
{
	//! Largest single deposit or withdrawal the server accepts. Guards the
	//! account balance against hostile oversized RPC payloads.
	static const int MAX_TRANSACTION_AMOUNT = 1000000;

	protected static ref LF_ATMManager s_Instance;
	protected ref map<string, ref LF_BankAccount> m_mAccounts = new map<string, ref LF_BankAccount>();

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
	static array<ref LF_BankAccountRecord> ExportAll()
	{
		LF_ATMManager instance = GetInstance();
		array<ref LF_BankAccountRecord> records = new array<ref LF_BankAccountRecord>();
		foreach (string accountId, LF_BankAccount account : instance.m_mAccounts)
		{
			if (!account)
				continue;

			records.Insert(LF_BankAccountRecord.Create(account.GetPersistentId(), account.GetBalance()));
		}

		return records;
	}

	//------------------------------------------------------------------------------------------------
	//! Persistence seam: apply records back into the cache (idempotent). Existing ids are
	//! overwritten so a re-apply to a live session converges on the saved state.
	//! \param records Records read from the save.
	static void ApplyAll(notnull array<ref LF_BankAccountRecord> records)
	{
		LF_ATMManager instance = GetInstance();

		// The save is a complete snapshot. Remove accounts absent from it so a
		// reduced or empty load cannot resurrect stale balances.
		instance.m_mAccounts.Clear();

		foreach (LF_BankAccountRecord record : records)
		{
			if (!record || record.m_sPersistentId.IsEmpty())
				continue;

			LF_BankAccount account = LF_BankAccount.Create(record.m_sPersistentId);
			account.SetBalance(record.m_iBalance);
			instance.AddAccount(account);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Returns the cached account for an id, creating and caching a new one when absent.
	//! \param accountId Account id (character persistence id).
	//! \return The account for the id (never null).
	static LF_BankAccount GetOrCreate(string accountId)
	{
		LF_ATMManager instance = GetInstance();
		LF_BankAccount account = instance.m_mAccounts.Get(accountId);
		if (!account)
			account = instance.CreateAccount(accountId);

		return account;
	}

	//------------------------------------------------------------------------------------------------
	//! The one bank cache. Created on first use and never replaced: the save load fills this
	//! instance before any player connects, so a second instance would hide the loaded balances.
	static LF_ATMManager GetInstance()
	{
		if (!s_Instance)
			s_Instance = new LF_ATMManager();

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
	//! Does not register itself. Only GetInstance() sets the singleton, so a stray `new`
	//! cannot replace the cache the save load filled.
	void LF_ATMManager()
	{
	}

	//------------------------------------------------------------------------------------------------
	//! Only the registered instance clears the singleton. Releasing a stray instance must
	//! not drop the live cache.
	void ~LF_ATMManager()
	{
		if (s_Instance == this)
			s_Instance = null;
	}

	//------------------------------------------------------------------------------------------------
	LF_BankAccount GetAccount(string accountId)
	{
		return m_mAccounts.Get(accountId);
	}

	//------------------------------------------------------------------------------------------------
	void AddAccount(LF_BankAccount account)
	{
		if (!account || account.GetPersistentId().IsEmpty())
		{
			LF_Debug.Error("ATM", "account registration rejected: empty account id");
			return;
		}

		m_mAccounts.Set(account.GetPersistentId(), account);
	}

	//------------------------------------------------------------------------------------------------
	LF_BankAccount CreateAccount(string accountId)
	{
		if (accountId.IsEmpty())
		{
			LF_Debug.Error("ATM", "account creation rejected: empty account id");
			return null;
		}

		LF_BankAccount existing = m_mAccounts.Get(accountId);
		if (existing)
			return existing;

		LF_BankAccount account = LF_BankAccount.Create(accountId);
		AddAccount(account);
		return account;
	}

	//------------------------------------------------------------------------------------------------
	bool Deposit(string accountId, int amount)
	{
		if (!Replication.IsServer())
		{
			LF_Debug.Warn("ATM", "deposit rejected: not running on the server");
			return false;
		}

		if (!IsValidAmount(amount))
		{
			LF_Debug.Warn("ATM", string.Format("deposit rejected: invalid amount %1", amount));
			return false;
		}

		LF_BankAccount account = GetAccount(accountId);
		if (account)
		{
			account.Deposit(amount, "bank-deposit");
			LF_Debug.Log("ATM", string.Format("deposit account=%1 amount=%2", accountId, amount));
			return true;
		}

		LF_Debug.Log("ATM", string.Format("deposit failed: no account %1", accountId));
		return false;
	}

	//------------------------------------------------------------------------------------------------
	bool Withdraw(string accountId, int amount)
	{
		if (!Replication.IsServer())
		{
			LF_Debug.Warn("ATM", "withdraw rejected: not running on the server");
			return false;
		}

		if (!IsValidAmount(amount))
		{
			LF_Debug.Warn("ATM", string.Format("withdraw rejected: invalid amount %1", amount));
			return false;
		}

		LF_BankAccount account = GetAccount(accountId);
		if (account)
		{
			bool ok = account.Withdraw(amount, "bank-withdraw");
			LF_Debug.Log("ATM", string.Format("withdraw account=%1 amount=%2 ok=%3", accountId, amount, ok));
			return ok;
		}

		LF_Debug.Log("ATM", string.Format("withdraw failed: no account %1", accountId));
		return false;
	}
};
