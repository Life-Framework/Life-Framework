// red-proof: for crime/rob-reward drop the EL_ATMManager.CreateAccount(uid) call
// and the Deposit assertion goes red (Deposit returns false on an uncreated id);
// break the expected balance 200 or wanted 1 and the round trip goes red. For
// crime/rob-wanted-clamp remove the clamp in EL_PlayerAccount.SetWantedLevel and
// the clamped-at-5 assertion goes red.

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

		// The DebugWorld RobberyPoint_Money carries m_iMoneyAmount 200; drive the
		// reward outcome directly at the manager seam because an emulated robber has
		// no PlayerController (GetPlayerUid returns "") and the police-on-duty gate
		// needs real connected players.
		const int MONEY_AMOUNT = 200;
		string uid = "test-rob-reward";

		ctx.True(Replication.IsServer(), "rob reward test runs on the server (LOGIC tier boots the dedicated server)");

		EL_ATMManager atmManager = EL_ATMManager.GetInstance();
		ctx.NotNull(atmManager.CreateAccount(uid), "robber bank account is pre-created under the fixed id");
		ctx.NotNull(atmManager.GetAccount(uid), "pre-created bank account is reachable");

		EL_PlayerAccount account = EL_PlayerAccountManager.GetOrCreate(uid);
		ctx.NotNull(account, "robber civilian account is created under the fixed id");

		// Replicates EL_RobAction.PerformAction for the emulated robber.
		ctx.True(atmManager.Deposit(uid, MONEY_AMOUNT), "robbery deposit is accepted on the pre-created account");
		account.IncreaseWantedLevel(1);
		EL_PlayerAccountManager.GetInstance().SaveAndReleaseAccount(account);

		EL_BankAccount bank = atmManager.GetAccount(uid);
		ctx.NotNull(bank, "bank account is reachable after the deposit");
		if (bank)
			ctx.Equal(MONEY_AMOUNT, bank.GetBalance(), "robber bank balance reflects the 200 robbery payout");

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

		// Stack six robberies the way PerformAction does: deposit plus wanted bump.
		for (int i = 0; i < 6; i++)
		{
			atmManager.Deposit(uid, MONEY_AMOUNT);
			account.IncreaseWantedLevel(1);
		}

		EL_BankAccount bank = atmManager.GetAccount(uid);
		ctx.NotNull(bank, "bank account exists after repeated robberies");
		if (bank)
			ctx.Equal(1200, bank.GetBalance(), "six robberies pay 200 each");

		EL_PlayerAccount cached = EL_PlayerAccountManager.GetInstance().GetFromCache(uid);
		ctx.NotNull(cached, "account exists after repeated robberies");
		if (cached)
			ctx.Equal(5, cached.GetWantedLevel(), "wanted level clamps at 5 after repeated robberies");
	}
};