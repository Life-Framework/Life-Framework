// red-proof: change an expected value (e.g. deposit 100 -> 101) or remove the
// amount>0 guard in EL_BankAccount.Withdraw, then run the fast tier. For
// bank/save-idempotent make ApplyAll Deposit onto a cached account instead of
// replacing it and the double-apply balance assertion goes red; for
// bank/save-empty make ExportAll return null and the non-null assertion goes
// red; for atm/manager-guards drop the IsValidAmount guard in the manager and
// the rejected-deposit assertions go red.

// tier: LOGIC
class EL_Test_BankAccountMath : EL_Test
{
	override string GetName()
	{
		return "bank/account-math";
	}

	override void Run(EL_TestContext ctx)
	{
		EL_BankAccount account = EL_BankAccount.Create("test-bank");

		ctx.Equal(0, account.GetBalance(), "fresh account starts at 0");

		account.Deposit(100);
		ctx.Equal(100, account.GetBalance(), "Deposit adds to the balance");

		account.Deposit(0);
		ctx.Equal(100, account.GetBalance(), "Deposit of zero is a no-op");

		account.Deposit(-50);
		ctx.Equal(100, account.GetBalance(), "Deposit of a negative amount is a no-op");

		ctx.False(account.Withdraw(200), "Withdraw above the balance fails");
		ctx.Equal(100, account.GetBalance(), "failed Withdraw leaves the balance unchanged");

		ctx.True(account.Withdraw(60), "Withdraw within the balance succeeds");
		ctx.Equal(40, account.GetBalance(), "successful Withdraw lowers the balance");

		ctx.False(account.Withdraw(0), "Withdraw of zero fails");
		ctx.False(account.Withdraw(-10), "Withdraw of a negative amount fails");
		ctx.True(account.GetBalance() >= 0, "balance never goes negative through Deposit/Withdraw");
	}
};

//------------------------------------------------------------------------------------------------
// tier: LOGIC
class EL_Test_BankSaveRoundtrip : EL_Test
{
	override string GetName()
	{
		return "bank/save-roundtrip";
	}

	override void Run(EL_TestContext ctx)
	{
		EL_ATMManager.Reset();
		EL_ATMManager manager = EL_ATMManager.GetInstance();

		EL_BankAccount account = manager.CreateAccount("test-bank-save");
		account.Deposit(750);

		ref array<ref EL_BankAccountRecord> records = EL_ATMManager.ExportAll();
		EL_ATMManager.Reset();
		EL_ATMManager.ApplyAll(records);

		EL_BankAccount restored = EL_ATMManager.GetInstance().GetAccount("test-bank-save");
		ctx.NotNull(restored, "bank account is restored from its records");
		if (!restored)
			return;

		ctx.Equal(750, restored.GetBalance(), "balance survives the save round trip");
		ctx.EqualStr("test-bank-save", restored.GetPersistentId(), "persistent id survives the round trip");
	}
};

//------------------------------------------------------------------------------------------------
// tier: LOGIC
class EL_Test_ATMManagerRegistry : EL_Test
{
	override string GetName()
	{
		return "atm/manager-registry";
	}

	override void Run(EL_TestContext ctx)
	{
		EL_ATMManager.Reset();
		EL_ATMManager manager = EL_ATMManager.GetInstance();
		ctx.True(manager.CreateAccount("") == null, "CreateAccount rejects an empty account id");

		EL_BankAccount created = manager.CreateAccount("test-atm-uid");
		ctx.NotNull(created, "CreateAccount returns a new account");
		ctx.True(manager.GetAccount("test-atm-uid") == created, "GetAccount finds the created account");

		ctx.True(manager.Deposit("test-atm-uid", 500), "Deposit on a known account succeeds");
		ctx.Equal(500, created.GetBalance(), "Deposit reaches the account");

		EL_BankAccount duplicate = manager.CreateAccount("test-atm-uid");
		ctx.True(duplicate == created, "CreateAccount returns the existing account for a duplicate id");
		ctx.Equal(500, duplicate.GetBalance(), "duplicate account creation preserves the existing balance");

		ctx.True(manager.Withdraw("test-atm-uid", 100), "Withdraw on a known account succeeds");
		ctx.Equal(400, created.GetBalance(), "Withdraw reaches the account");

		ctx.False(manager.Withdraw("missing-uid", 100), "Withdraw on an unknown account fails");
		ctx.True(manager.GetAccount("missing-uid") == null, "unknown account id returns null");

		EL_ATMManager.Reset();
	}
};

//------------------------------------------------------------------------------------------------
// tier: LOGIC
class EL_Test_BankSaveIdempotent : EL_Test
{
	override string GetName()
	{
		return "bank/save-idempotent";
	}

	override void Run(EL_TestContext ctx)
	{
		EL_ATMManager.Reset();
		EL_ATMManager manager = EL_ATMManager.GetInstance();

		EL_BankAccount account = manager.CreateAccount("test-bank-idem");
		account.Deposit(500);

		ref array<ref EL_BankAccountRecord> records = EL_ATMManager.ExportAll();
		EL_ATMManager.Reset();
		EL_ATMManager.ApplyAll(records);
		EL_ATMManager.ApplyAll(records);

		EL_BankAccount restored = EL_ATMManager.GetInstance().GetAccount("test-bank-idem");
		ctx.NotNull(restored, "account survives a double apply");
		if (!restored)
			return;

		ctx.Equal(500, restored.GetBalance(), "double apply does not compound the balance");
		ctx.Equal(1, EL_ATMManager.ExportAll().Count(), "double apply does not duplicate the account");
	}
};

//------------------------------------------------------------------------------------------------
// tier: LOGIC
class EL_Test_BankSaveEmpty : EL_Test
{
	override string GetName()
	{
		return "bank/save-empty";
	}

	override void Run(EL_TestContext ctx)
	{
		EL_ATMManager.Reset();

		ref array<ref EL_BankAccountRecord> records = EL_ATMManager.ExportAll();
		ctx.NotNull(records, "export with no accounts returns a non-null array");
		if (records)
			ctx.Equal(0, records.Count(), "export with no accounts returns an empty array");

		EL_ATMManager.ApplyAll(records);
		ctx.True(EL_ATMManager.GetInstance().GetAccount("missing-uid") == null, "applying empty records creates no accounts");
	}
};

//------------------------------------------------------------------------------------------------
// tier: LOGIC
class EL_Test_ATMManagerGuards : EL_Test
{
	override string GetName()
	{
		return "atm/manager-guards";
	}

	override void Run(EL_TestContext ctx)
	{
		EL_ATMManager.Reset();
		EL_ATMManager manager = EL_ATMManager.GetInstance();

		ctx.False(manager.Deposit("missing-uid", 100), "Deposit on an unknown account fails");
		ctx.False(manager.Withdraw("missing-uid", 100), "Withdraw on an unknown account fails");

		EL_BankAccount account = manager.CreateAccount("test-guards");
		account.Deposit(500);

		ctx.False(manager.Deposit("test-guards", 0), "Deposit of zero through the manager fails");
		ctx.False(manager.Deposit("test-guards", -1), "Deposit of a negative amount through the manager fails");
		ctx.False(manager.Deposit("test-guards", EL_ATMManager.MAX_TRANSACTION_AMOUNT + 1), "Deposit above the cap through the manager fails");
		ctx.Equal(500, account.GetBalance(), "rejected manager deposits leave the balance unchanged");

		ctx.False(manager.Withdraw("test-guards", 0), "Withdraw of zero through the manager fails");
		ctx.False(manager.Withdraw("test-guards", -1), "Withdraw of a negative amount through the manager fails");
		ctx.False(manager.Withdraw("test-guards", 501), "Withdraw above the balance through the manager fails");
		ctx.Equal(500, account.GetBalance(), "rejected manager withdrawals leave the balance unchanged");
	}
};

//------------------------------------------------------------------------------------------------
// tier: LOGIC
class EL_Test_BankApplySnapshot : EL_Test
{
	override string GetName()
	{
		return "bank/apply-snapshot";
	}

	override void Run(EL_TestContext ctx)
	{
		EL_ATMManager.Reset();
		EL_ATMManager manager = EL_ATMManager.GetInstance();
		EL_BankAccount stale = manager.CreateAccount("stale-bank");
		manager.Deposit("stale-bank", 900);

		ref array<ref EL_BankAccountRecord> emptyRecords = new array<ref EL_BankAccountRecord>();
		EL_ATMManager.ApplyAll(emptyRecords);

		ctx.True(EL_ATMManager.GetInstance().GetAccount("stale-bank") == null, "snapshot apply removes accounts absent from the save");
		ctx.Equal(0, EL_ATMManager.ExportAll().Count(), "empty snapshot exports no stale accounts");
	}
};
