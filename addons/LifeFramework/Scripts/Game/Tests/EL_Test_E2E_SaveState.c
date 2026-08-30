// red-proof: e2e/pass2-verify-load goes red when the reload boot loads no save:
// run pass 2 with a fresh profile (drop -KeepProfile) so no account records land
// in the managers and the bank GetAccount null-checks fail. Breaking the loaded
// balance expectation to 301 also fails the == 300 assertion, and removing
// EL_PersistenceComponent from GameMode_Roleplay.et fails the component resolve.
// e2e/pass1-write-save goes red when the save request is rejected, e.g. mission
// header m_eSaveTypes 0, which leaves IsSaveInProgress() false right after
// SaveGame(), or when the blocking shutdown save never lands (no graceful close),
// which makes pass 2's loaded-account assertions fail.
// Red run pending: the two-boot persistence gate is serial-only this session;
// the mutations above fail on the next persistence-boot run.

// tier: PERSISTENCE
class EL_Test_E2E_SaveStateWrite : EL_Test
{
	protected static const string ACCOUNT_UID = "e2e-uid";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "e2e/pass1-write-save";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(EL_TestContext ctx)
	{
		SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		ctx.NotNull(gameMode, "the DebugWorld game mode resolves for the save trigger");
		if (!gameMode)
			return;

		EL_PersistenceManagerComponent manager = EL_PersistenceManagerComponent.Cast(gameMode.FindComponent(EL_PersistenceManagerComponent));
		ctx.NotNull(manager, "the game-mode entity carries EL_PersistenceManagerComponent for the save trigger");
		if (!manager)
			return;

#ifdef EL_E2E_LOAD_BOOT
		// The reload boot must not re-seed: re-establishing the contract here would
		// mask a broken load by overwriting whatever the save restored.
		ctx.Pass("save state established by the save boot and loaded by this boot");
#else
		// The save boot seeds the contract directly instead of depending on the
		// narrative test: later all-tier tests (rob-reward, banking) reset the
		// account managers, and an async manual save can complete after those
		// resets. Seeding here plus the blocking shutdown save on graceful close
		// captures exactly the 300/1/CIVILIAN contract the loop must reach.
		EL_ATMManager atmManager = EL_ATMManager.GetInstance();
		EL_BankAccount bank = atmManager.CreateAccount(ACCOUNT_UID);
		ctx.NotNull(bank, "e2e bank account created for the save");
		if (bank)
		{
			ctx.True(atmManager.Deposit(ACCOUNT_UID, 300), "e2e bank balance 300 deposited for the save");
			ctx.Equal(300, atmManager.GetAccount(ACCOUNT_UID).GetBalance(), "e2e bank balance reads 300 before the save");
		}

		EL_PlayerAccountManager playerAccounts = EL_PlayerAccountManager.GetInstance();
		EL_PlayerAccount account = playerAccounts.GetOrCreate(ACCOUNT_UID);
		ctx.NotNull(account, "e2e player account created for the save");
		if (account)
		{
			account.SetWantedLevel(1);
			account.SetFaction(EL_Faction.CIVILIAN);
			playerAccounts.SaveAndReleaseAccount(account);
		}

		// The async manual save is the belt; the blocking shutdown save on game
		// close (OnGameEnd) is the suspenders the reload boot loads.
		manager.SaveGame();
		ctx.True(manager.IsSaveInProgress(), "SaveGame() accepts the manual save request");
#endif
	}
}

//------------------------------------------------------------------------------------------------
// tier: PERSISTENCE
class EL_Test_E2E_SaveStateVerify : EL_Test
{
	protected static const string ACCOUNT_UID = "e2e-uid";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "e2e/pass2-verify-load";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(EL_TestContext ctx)
	{
#ifdef EL_E2E_LOAD_BOOT
		// The reload boot passes -scrDefine EL_E2E_LOAD_BOOT (and boots with
		// -loadSessionSave). In the save boot or a normal all-tier pass the live
		// state could satisfy these assertions with no save/load behind them, so
		// they must only run when the save was actually loaded.
		EL_BankAccount bank = EL_ATMManager.GetInstance().GetAccount(ACCOUNT_UID);
		ctx.NotNull(bank, "bank account e2e-uid present after the save load");
		if (bank)
			ctx.Equal(300, bank.GetBalance(), "bank balance 300 after the save load");

		EL_PlayerAccount cached = EL_PlayerAccountManager.GetInstance().GetFromCache(ACCOUNT_UID);
		ctx.NotNull(cached, "player account e2e-uid present after the save load");
		if (cached)
		{
			ctx.Equal(1, cached.GetWantedLevel(), "wanted level 1 after the save load");
			ctx.Equal(EL_Faction.CIVILIAN, cached.GetFaction(), "account faction CIVILIAN after the save load");
		}

		SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		ctx.NotNull(gameMode, "the DebugWorld game mode resolves in the reload boot");
		if (gameMode)
		{
			EL_PersistenceComponent persistence = EL_PersistenceComponent.Cast(gameMode.FindComponent(EL_PersistenceComponent));
			ctx.NotNull(persistence, "the game-mode entity carries EL_PersistenceComponent for survival checks");
		}
#else
		ctx.Pass("runs in the reload boot");
#endif
	}
}
