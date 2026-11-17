// red-proof: 2026-11-17 `tools\cli test --no-build --red LF_Test_SaveNewerVersion,LF_Test_PersistStoreStaticReset`: RED PROOF OK, with the version left at the current one `the save claims the newer version (expected 49, got 48)`, `the unread newer payloads count as a read failure` and `a newer save pauses autosave so it is not overwritten` fail. Green with the fix (--only).
// The store of a full persistence-tier run is past 8 KB, and a spliced Enforce string that size came back cut (2026-11-18: 8204 of 9388 chars, so 'the newer save parses' failed in the suite and passed alone); the newer save is now written to a profile file in pieces and loaded from it. Observed red: `--tier persistence` FAIL persist/save-newer-version on the string splice, green on the file.
// tier: PERSISTENCE
//! A save written by a newer build (a rollback, or a merge the main guard reverted after it saved)
//! carries a version above this build's payload list. Its known payloads still load, but the load
//! counts as damaged so autosave pauses: saving over it would drop the payloads this build cannot read.
[LF_TestInfo(LF_TestTier.PERSISTENCE)]
class LF_Test_SaveNewerVersion : LF_Test
{
	protected static const string UID = "newer-save-bank-uid";
	protected static const string DIGITS = "0123456789";
	protected static const string NEWER_FILE = "$profile:lf-test-save-newer.json";
	//! An Enforce string built past about 8 KB comes back cut, so the save is written in pieces this size.
	protected static const int CHUNK = 2048;

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
		FileIO.DeleteFile(NEWER_FILE);
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
		JsonSaveContext saveContext = new JsonSaveContext(false);
		LF_PersistenceComponentSerializer.SerializeStore(persistence, saveContext);
		string saved = saveContext.SaveToString();
		SetBank(UID, 4);

		// The newer build appended one payload and raised the version by one.
		int newer = LF_TestSaveVersion.CURRENT + 1;
		int written = LF_TestSaveVersion.CURRENT;
		if (!RedProof())
			written = newer;

		ctx.True(WriteWithVersion(saved, written, NEWER_FILE), "the newer save is written to the profile");
		JsonLoadContext header = new JsonLoadContext(false);
		ctx.True(header.LoadFromFile(NEWER_FILE), "the newer save parses");
		int version;
		header.ReadValue("version", version);
		ctx.Equal(newer, version, "the save claims the newer version");

		JsonLoadContext loadContext = new JsonLoadContext(false);
		loadContext.LoadFromFile(NEWER_FILE);
		ctx.True(LF_PersistenceComponentSerializer.DeserializeStore(persistence, loadContext), "the newer save loads");
		ctx.Equal(310, Bank(UID), "the payloads this build knows still load");
		ctx.True(LF_PersistenceComponentSerializer.GetReadFailures() > 0, "the unread newer payloads count as a read failure");
		ctx.True(LF_PersistenceComponentSerializer.IsLoadDamaged(), "a newer save pauses autosave so it is not overwritten");
	}

	//------------------------------------------------------------------------------------------------
	//! Writes the save text to a file with its "version" number replaced. The text is written in pieces:
	//! a spliced string past about 8 KB comes back cut, and the store of a full test run is bigger.
	protected static bool WriteWithVersion(string saved, int version, string path)
	{
		int key = saved.IndexOf("\"version\"");
		if (key < 0)
			return false;

		int start = -1;
		int end = -1;
		for (int i = key + 9; i < saved.Length(); i++)
		{
			string c = saved.Get(i);
			bool digit = DIGITS.Contains(c);
			if (digit && start < 0)
				start = i;

			if (!digit && start >= 0)
			{
				end = i;
				break;
			}
		}

		if (start < 0 || end < 0)
			return false;

		FileHandle handle = FileIO.OpenFile(path, FileMode.WRITE);
		if (!handle)
			return false;

		handle.Write(saved.Substring(0, start));
		handle.Write(version.ToString());
		for (int from = end; from < saved.Length(); from += CHUNK)
		{
			handle.Write(saved.Substring(from, Math.Min(CHUNK, saved.Length() - from)));
		}

		handle.Close();
		return true;
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
