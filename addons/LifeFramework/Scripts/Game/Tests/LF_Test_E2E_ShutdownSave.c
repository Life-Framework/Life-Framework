// red-proof: e2e/shutdown-save-loaded, observed 2026-11-12 with `tools\cli wt e2e e2e-shutdown`:
// LF_StaticReset.ResetAll() moved from the game mode destructor into the persistence manager's
// OnGameEnd (before the SHUTDOWN save) failed pass 1 with `e2e: FAILED in pass 1 (shutdown save):
// the feature statics were reset BEFORE the shutdown save, so it holds empty state`; pass 2 run
// anyway read `SUMMARY tier=persistence passed=28 failed=2 total=30`,
// `- the shutdown save kept the marker's ledger line (statics were not reset before it) (expected 1, got 0)`,
// `- e2e the playtime total is 2 h after the restart (expected 7200, got 0)`. Restored; green.

//------------------------------------------------------------------------------------------------
//! #830: tools\cli wt e2e must reload the engine's SHUTDOWN save, not the manual one. In the save
//! boot (LF_E2E_HOST_STOP) a marker account is opened only after the manual save has finished, so
//! only the shutdown save written on the host-stop close holds it. The reload boot finds it, with
//! its ledger line, or the reload did not load the shutdown save (or the save held reset statics).
// tier: PERSISTENCE
class LF_Test_E2E_ShutdownSave : LF_Test
{
	static const string MARKER_UID = "e2e-shutdown-uid";
	static const int MARKER_CASH = 77;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "e2e/shutdown-save-loaded";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
#ifdef LF_E2E_LOAD_BOOT
		LF_BankAccount bank = LF_ATMManager.GetInstance().GetAccount(MARKER_UID);
		ctx.NotNull(bank, "the shutdown-only marker account came back: the reload loaded the SHUTDOWN save, not the manual one");
		if (!bank)
			return;

		ctx.Equal(MARKER_CASH, bank.GetBalance(), "the shutdown-only marker account holds its deposit after the restart");
		array<ref LF_LedgerEntry> lines = {};
		LF_Ledger.Last(MARKER_UID, LF_Ledger.ACCOUNT_BANK, 5, lines);
		ctx.Equal(1, lines.Count(), "the shutdown save kept the marker's ledger line (statics were not reset before it)");
		if (bank.GetBalance() == MARKER_CASH && lines.Count() == 1)
			PrintFormat("[LF_Tests] e2e shutdown save loaded: marker account %1 holds %2 with its ledger line", MARKER_UID, MARKER_CASH);
#else
#ifdef LF_E2E_HOST_STOP
		SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		LF_PersistenceManagerComponent manager;
		if (gameMode)
			manager = LF_PersistenceManagerComponent.Cast(gameMode.FindComponent(LF_PersistenceManagerComponent));

		ctx.NotNull(manager, "the game mode carries LF_PersistenceManagerComponent for the shutdown marker");
		if (!manager)
			return;

		ctx.True(LF_ATMManager.GetInstance().GetAccount(MARKER_UID) == null, "no shutdown marker account before the manual save");
		manager.GetOnSaveFinished().Insert(OnManualSaveFinished);
		ctx.Pass("the shutdown marker waits for the manual save to finish");
#else
		ctx.Pass("only the wt e2e save boot (LF_E2E_HOST_STOP) seeds the shutdown marker");
#endif
#endif
	}

	//------------------------------------------------------------------------------------------------
	//! The manual save is done: open the marker now, so only the SHUTDOWN save can carry it. The
	//! harness waits for the printed line before it writes the host stop file.
	static void OnManualSaveFinished(bool success)
	{
		SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		LF_PersistenceManagerComponent manager;
		if (gameMode)
			manager = LF_PersistenceManagerComponent.Cast(gameMode.FindComponent(LF_PersistenceManagerComponent));

		if (manager)
			manager.GetOnSaveFinished().Remove(OnManualSaveFinished);

		if (!success)
		{
			Print("[LF_Tests] e2e shutdown marker not deposited: the manual save failed", LogLevel.ERROR);
			return;
		}

		LF_Ledger.EnsureSubscribed();
		LF_ATMManager atm = LF_ATMManager.GetInstance();
		atm.CreateAccount(MARKER_UID);
		if (!atm.Deposit(MARKER_UID, MARKER_CASH))
		{
			Print("[LF_Tests] e2e shutdown marker not deposited: the deposit was refused", LogLevel.ERROR);
			return;
		}

		PrintFormat("[LF_Tests] e2e shutdown marker deposited after the manual save: %1 holds %2", MARKER_UID, MARKER_CASH);
	}
}
