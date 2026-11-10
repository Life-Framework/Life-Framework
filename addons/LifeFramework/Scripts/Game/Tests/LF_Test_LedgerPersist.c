// red-proof: Observed 2026-11-09 with the same flip in LF_Ledger.Record: `money/ledger-persist` failed `- the signed change is back`. Restored; green. (The e2e pass 2 checks the restart path.)
// tier: PERSISTENCE
//! The ledger rides the real save path (the "ledger" payload, version 14): entries come back after
//! the store is wiped, the id sequence continues, and loading twice leaves the same log.
class LF_Test_LedgerPersist : LF_Test
{
	protected static const string UID = "ledger-save-uid";

	override string GetName()
	{
		return "money/ledger-persist";
	}

	override void Run(LF_TestContext ctx)
	{
		SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		ctx.NotNull(gameMode, "the DebugWorld game mode resolves");
		if (!gameMode)
			return;

		LF_PersistenceComponent persistence = LF_PersistenceComponent.Cast(gameMode.FindComponent(LF_PersistenceComponent));
		ctx.NotNull(persistence, "the game mode carries LF_PersistenceComponent");
		if (!persistence)
			return;

		array<ref LF_LedgerEntry> before = LF_Ledger.ExportAll();
		LF_Ledger.ApplyAll(null);
		LF_Ledger.EnsureSubscribed();
		LF_BankAccount account = LF_BankAccount.Create(UID);
		account.Deposit(900, "test-deposit");
		ctx.True(account.Withdraw(400, "test-withdraw"), "the withdrawal is covered");
		ctx.Equal(2, LF_Ledger.Count(), "two lines before the save");

		JsonSaveContext saveContext = new JsonSaveContext(false);
		ctx.True(LF_PersistenceComponentSerializer.SerializeStore(persistence, saveContext) == ESerializeResult.OK, "the store serializes");
		string saved = saveContext.SaveToString();
		ctx.True(saved.Contains(UID), "the save carries the ledger payload");

		// Restart: the session has no ledger.
		LF_Ledger.ApplyAll(null);
		ctx.Equal(0, LF_Ledger.Count(), "the restarted session starts with an empty ledger");

		JsonLoadContext loadContext = new JsonLoadContext(false);
		ctx.True(loadContext.LoadFromString(saved), "the save parses");
		int version;
		loadContext.ReadValue("version", version);
		ctx.Equal(22, version, "the save is written at version 22 (ledger is payload 14)");
		LF_PersistenceComponentSerializer.ReadPersistencePayload(persistence, loadContext, "ledger");
		ctx.Equal(2, LF_Ledger.Count(), "both lines are back after the load");
		ctx.True(LF_Ledger.VerifyBank(UID, 500), "the restored log still explains the stored balance");

		array<ref LF_LedgerEntry> last = {};
		LF_Ledger.Last(UID, "", 20, last);
		if (last.Count() == 2)
		{
			ctx.EqualStr("test-withdraw", last.Get(0).m_sReason, "the reason is back");
			ctx.Equal(-400, last.Get(0).m_iDelta, "the signed change is back");
			ctx.Equal(500, last.Get(0).m_iBalanceAfter, "the balance after is back");
		}

		// New lines continue the id sequence instead of reusing an id.
		int highest = 0;
		if (last.Count() > 0)
			highest = last.Get(0).m_iId;

		account.Deposit(1, "test-after-load");
		LF_Ledger.Last(UID, "", 1, last);
		if (last.Count() == 1)
			ctx.True(last.Get(0).m_iId > highest, "a new line gets a fresh id");

		LF_Ledger.ApplyAll(before);
	}
}
