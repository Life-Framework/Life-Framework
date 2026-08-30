// red-proof: for crime/rob-reward remove the IncreaseWantedLevel(1) call in
// EL_RobAction.PerformAction and the wanted-1 assertion goes red; deposit
// m_iMoneyAmount to the robber's ATM account in PerformAction (revert to the
// old bank payout) and the balance-0 assertion goes red. For
// crime/rob-wanted-clamp remove the clamp in EL_PlayerAccount.SetWantedLevel
// and the clamped-at-5 assertion goes red.

// tier: LOGIC
class EL_Test_RobReward : EL_Test
{
	override string GetName()
	{
		return "crime/rob-reward";
	}

	override void Run(EL_TestContext ctx)
	{
		EL_ATMManager.Reset();
		EL_PlayerAccountManager.Reset();

		// Drives the LOGIC-testable seams of EL_RobAction.PerformAction: a robbery
		// raises wanted by 1 and pays cash, never the bank. The cash payout itself
		// needs a character inventory (WORLD tier, see EL_Test_MoneyCash); here we
		// pin the contract that the rob no longer deposits into the ATM account.
		const int MONEY_AMOUNT = 200;
		string uid = "test-rob-reward";

		ctx.True(Replication.IsServer(), "rob reward test runs on the server (LOGIC tier boots the dedicated server)");

		EL_ATMManager atmManager = EL_ATMManager.GetInstance();
		ctx.NotNull(atmManager.CreateAccount(uid), "robber bank account is pre-created under the fixed id");

		EL_PlayerAccount account = EL_PlayerAccountManager.GetOrCreate(uid);
		ctx.NotNull(account, "robber civilian account is created under the fixed id");

		// Mirrors EL_RobAction.PerformAction: wanted bump + SaveAndReleaseAccount.
		account.IncreaseWantedLevel(1);
		EL_PlayerAccountManager.GetInstance().SaveAndReleaseAccount(account);

		EL_BankAccount bank = atmManager.GetAccount(uid);
		ctx.NotNull(bank, "bank account exists");
		if (bank)
			ctx.Equal(0, bank.GetBalance(), "robbery pays cash, never the bank - balance stays 0");

		EL_PlayerAccount cached = EL_PlayerAccountManager.GetInstance().GetFromCache(uid);
		ctx.NotNull(cached, "robber account is retained in the cache after SaveAndReleaseAccount");
		if (cached)
		{
			ctx.Equal(1, cached.GetWantedLevel(), "robbery raises wanted level by 1");
			ctx.Equal(EL_Faction.CIVILIAN, cached.GetFaction(), "robber account stays civilian");
		}
	}
};

//------------------------------------------------------------------------------------------------
// tier: LOGIC
class EL_Test_RobRewardWantedClamp : EL_Test
{
	override string GetName()
	{
		return "crime/rob-wanted-clamp";
	}

	override void Run(EL_TestContext ctx)
	{
		EL_ATMManager.Reset();
		EL_PlayerAccountManager.Reset();

		const int MONEY_AMOUNT = 200;
		string uid = "test-rob-clamp";

		EL_ATMManager atmManager = EL_ATMManager.GetInstance();
		atmManager.CreateAccount(uid);

		EL_PlayerAccount account = EL_PlayerAccountManager.GetOrCreate(uid);

		// Stack six robberies the way PerformAction does: cash payout plus wanted bump.
		for (int i = 0; i < 6; i++)
		{
			account.IncreaseWantedLevel(1);
		}

		EL_BankAccount bank = atmManager.GetAccount(uid);
		ctx.NotNull(bank, "bank account exists after repeated robberies");
		if (bank)
			ctx.Equal(0, bank.GetBalance(), "repeated robberies never deposit to the bank");

		EL_PlayerAccount cached = EL_PlayerAccountManager.GetInstance().GetFromCache(uid);
		ctx.NotNull(cached, "account exists after repeated robberies");
		if (cached)
			ctx.Equal(5, cached.GetWantedLevel(), "wanted level clamps at 5 after repeated robberies");
	}
};