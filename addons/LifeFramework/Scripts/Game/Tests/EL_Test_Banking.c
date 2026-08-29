// red-proof: change an expected value (e.g. deposit 100 -> 101) or remove the
// amount>0 guard in EL_BankAccount.Withdraw, then run the fast tier.

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

		EL_BankAccount created = manager.CreateAccount("test-atm-uid");
		ctx.NotNull(created, "CreateAccount returns a new account");
		ctx.True(manager.GetAccount("test-atm-uid") == created, "GetAccount finds the created account");

		ctx.True(manager.Deposit("test-atm-uid", 500), "Deposit on a known account succeeds");
		ctx.Equal(500, created.GetBalance(), "Deposit reaches the account");

		ctx.True(manager.Withdraw("test-atm-uid", 100), "Withdraw on a known account succeeds");
		ctx.Equal(400, created.GetBalance(), "Withdraw reaches the account");

		ctx.False(manager.Withdraw("missing-uid", 100), "Withdraw on an unknown account fails");
		ctx.True(manager.GetAccount("missing-uid") == null, "unknown account id returns null");

		EL_ATMManager.Reset();
	}
};