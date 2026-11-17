// red-proof: 2026-11-17 `tools\cli test --no-build --red LF_Test_SaveNewerVersion,LF_Test_PersistStoreStaticReset`: RED PROOF OK, with the version left at the current one `the save claims the newer version (expected 49, got 48)`, `the unread newer payloads count as a read failure` and `a newer save pauses autosave so it is not overwritten` fail. Green with the fix (--only). #2517: the save is now written at the newer version directly; under --red
// the version stays current and `the unread newer payloads count as a read failure` fails.
// tier: PERSISTENCE
//! A save written by a newer build (a rollback, or a merge the main guard reverted after it saved)
//! carries a version above this build's payload list. Its known payloads still load, but the load
//! counts as damaged so autosave pauses: saving over it would drop the payloads this build cannot read.
class LF_Test_SaveNewerVersion : LF_Test
{
	protected static const string UID = "newer-save-bank-uid";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "persist/save-newer-version";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		// A full load re-applies every payload: keep what the tests after this one found.
		LF_TestLegacySnapshot keptOrgs = LF_TestLegacySnapshot.Take();
		array<ref LF_StorageSnapshotRecord> liveSnapshots = LF_StorageSnapshot.ExportAll();
		RunKept(ctx);
		LF_PersistenceComponentSerializer.ClearLoadDamaged();
		LF_SaveStamp.ResetForLoad();
		LF_StorageSnapshot.ApplyAll(liveSnapshots);
		keptOrgs.Restore();
		SetBank(UID, 0);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunKept(LF_TestContext ctx)
	{
		SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		LF_PersistenceComponent persistence;
		if (gameMode)
			persistence = LF_PersistenceComponent.Cast(gameMode.FindComponent(LF_PersistenceComponent));

		ctx.NotNull(persistence, "the game mode carries LF_PersistenceComponent");
		if (!persistence)
			return;

		SetBank(UID, 310);

		// The newer build appended one payload and raised the version by one. Built through the real
		// payload writers, not by editing the save text: in a full suite the edited text came back cut to
		// about 8 KB and no longer parsed (#2517).
		int newer = LF_TestSaveVersion.CURRENT + 1;
		if (RedProof())
			newer = LF_TestSaveVersion.CURRENT;

		JsonSaveContext saveContext = new JsonSaveContext(false);
		WriteStoreAtVersion(persistence, saveContext, newer);
		string saved = saveContext.SaveToString();
		SetBank(UID, 4);

		JsonLoadContext header = new JsonLoadContext(false);
		ctx.True(header.LoadFromString(saved), "the newer save parses");
		int version;
		header.ReadValue("version", version);
		ctx.Equal(newer, version, "the save claims the newer version");

		JsonLoadContext loadContext = new JsonLoadContext(false);
		loadContext.LoadFromString(saved);
		ctx.True(LF_PersistenceComponentSerializer.DeserializeStore(persistence, loadContext), "the newer save loads");
		ctx.Equal(310, Bank(UID), "the payloads this build knows still load");
		ctx.True(LF_PersistenceComponentSerializer.GetReadFailures() > 0, "the unread newer payloads count as a read failure");
		ctx.True(LF_PersistenceComponentSerializer.IsLoadDamaged(), "a newer save pauses autosave so it is not overwritten");
	}

	//------------------------------------------------------------------------------------------------
	//! Every payload this build has, under a save header that claims another version. SaveContext.Write
	//! keys each value by the variable's name in JSON, so the prefix carries the serializer's names.
	protected static void WriteStoreAtVersion(notnull LF_PersistenceComponent persistence, notnull SaveContext saveContext, int version)
	{
		saveContext.WriteValue("version", version);
		array<ref LF_PlayerAccountRecord> accountRecords = LF_PlayerAccountManager.ExportAll();
		if (!accountRecords)
			accountRecords = new array<ref LF_PlayerAccountRecord>();

		saveContext.Write(accountRecords);
		array<ref LF_BankAccountRecord> bankRecords = LF_ATMManager.ExportAll();
		if (!bankRecords)
			bankRecords = new array<ref LF_BankAccountRecord>();

		saveContext.Write(bankRecords);
		array<string> order = new array<string>();
		LF_PersistenceComponentSerializer.GetPayloadOrder(order);
		foreach (string payload : order)
		{
			LF_PersistenceComponentSerializer.WritePersistencePayload(persistence, saveContext, payload);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static void SetBank(string key, int balance)
	{
		LF_ATMManager atm = LF_ATMManager.GetInstance();
		if (!atm)
			return;

		LF_BankAccount bank = atm.GetAccount(key);
		if (!bank)
			bank = atm.CreateAccount(key);

		if (bank && bank.GetBalance() > 0)
			atm.Withdraw(key, bank.GetBalance());

		if (balance > 0)
			atm.Deposit(key, balance);
	}

	//------------------------------------------------------------------------------------------------
	protected static int Bank(string key)
	{
		LF_BankAccount bank = LF_ATMManager.GetInstance().GetAccount(key);
		if (!bank)
			return -1;

		return bank.GetBalance();
	}
}
