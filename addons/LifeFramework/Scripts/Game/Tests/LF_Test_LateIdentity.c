// red-proof: observed 2026-11-13 with `tools\cli test --no-build --only LF_Test_LateIdentity`: dropping the
// listen-host wait in LF_Utils.ChooseIdentityFallback (the build 4 behaviour: a stand-in at once) failed
// `- no key while the identity can still arrive: the flow waits instead of opening a stand-in account
// (expected '', got '00bbbddd-08D5-...')`. Skipping LF_PlayerAccountManager.AdoptStandIn failed
// `- the stand-in account is now under the real identity` and `- the save holds no second account under
// the stand-in (expected 0, got 1)`. Restored; green.

// tier: PERSISTENCE
//! Playtest build 4 (launcher "Play on my own"): the host's platform identity was the NULL UUID
//! when the registration form opened and the real one about 15 s later. The account opened under a
//! name stand-in, the signed form looked it up under the real identity, found nothing, and the
//! player never spawned. Every -noBackend run keeps one identity for the whole session, so this
//! drives LF_PlayerIdentity.Resolve with the identity a listen host with the online service sees,
//! at the times it sees it, and takes the result through the real save path.
class LF_Test_LateIdentity : LF_Test
{
	protected static const int HOST = 9701;
	protected static const int LATE = 9702;
	protected static const int RETURNING = 9703;
	protected static const int BOTH = 9704;
	protected static const string HOST_REAL = "7a1d0c3e-5b2f-4c8a-9e61-0d4f2b7c9a11";
	protected static const string LATE_REAL = "7a1d0c3e-5b2f-4c8a-9e61-0d4f2b7c9a12";
	protected static const string RETURNING_REAL = "7a1d0c3e-5b2f-4c8a-9e61-0d4f2b7c9a13";
	protected static const string BOTH_REAL = "7a1d0c3e-5b2f-4c8a-9e61-0d4f2b7c9a14";
	protected static const string NULL_ID = "00000000-0000-0000-0000-000000000000";
	protected static const ResourceName PREFAB = "{9B5BB216CC7FF18E}Prefabs/Characters/Core/Character_Roleplay.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "persist/late-identity";
	}

	//------------------------------------------------------------------------------------------------
	//! A listen host with the online service: not dedicated, no -noBackend.
	protected static string ListenHost(int playerId, string identity, int nowMs)
	{
		return LF_PlayerIdentity.Resolve(playerId, identity, false, false, false, nowMs);
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_TestLegacySnapshot keptOrgs = LF_TestLegacySnapshot.Take();
		array<ref LF_StorageSnapshotRecord> liveSnapshots = LF_StorageSnapshot.ExportAll();

		FormOpensUnderTheRealKey(ctx);
		OfflineKeysUnchanged(ctx);
		StandInMovesOntoTheRealKey(ctx);
		ReturningPlayerKeepsTheirAccount(ctx);
		BothKeptWhenBothHoldCharacters(ctx);

		LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();
		array<string> uids = {HOST_REAL, LATE_REAL, RETURNING_REAL, BOTH_REAL};
		uids.Insert(LF_Utils.BuildLocalIdentity(LATE));
		uids.Insert(LF_Utils.BuildLocalIdentity(RETURNING));
		uids.Insert(LF_Utils.BuildLocalIdentity(BOTH));
		foreach (string uid : uids)
		{
			accounts.RemoveFromCache(uid);
		}

		LF_PlayerIdentity.Forget(HOST);
		LF_PlayerIdentity.Forget(LATE);
		LF_PlayerIdentity.Forget(RETURNING);
		LF_PlayerIdentity.Forget(BOTH);

		LF_StorageSnapshot.ApplyAll(liveSnapshots);
		keptOrgs.Restore();
	}

	//------------------------------------------------------------------------------------------------
	//! The reported path: the flow asks at 1 s (NULL UUID), the form is signed at 20 s (real id).
	protected void FormOpensUnderTheRealKey(LF_TestContext ctx)
	{
		int t0 = 1000000;
		string atFlow = ListenHost(HOST, NULL_ID, t0);
		ctx.EqualStr("", atFlow, "no key while the identity can still arrive: the flow waits instead of opening a stand-in account");
		ctx.EqualStr("", ListenHost(HOST, "", t0 + 5000), "an empty answer waits too");

		string atLaterFlow = ListenHost(HOST, HOST_REAL, t0 + 15000);
		ctx.EqualStr(HOST_REAL, atLaterFlow, "the flow opens the account under the real identity once it arrives");
		LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();
		accounts.AddAccount(LF_PlayerAccount.Create(atLaterFlow));

		string atSubmit = ListenHost(HOST, HOST_REAL, t0 + 20000);
		ctx.NotNull(accounts.GetAccount(atSubmit), "the signed form finds the account the flow opened");
	}

	//------------------------------------------------------------------------------------------------
	//! Where no identity ever arrives the keys stay what they were: Workbench and -noBackend listen
	//! hosts keep vanilla's lower-case polyfill, a -noBackend dedicated server our name stand-in.
	protected void OfflineKeysUnchanged(LF_TestContext ctx)
	{
		string polyfill = "00bbbddd-08d5-523b-0945-5d270ce956c8";
		ctx.EqualStr(polyfill, LF_PlayerIdentity.Resolve(HOST, polyfill, false, false, true, 0), "Workbench play keeps vanilla's polyfill key");
		ctx.EqualStr(LF_Utils.BuildLocalIdentity(HOST), LF_PlayerIdentity.Resolve(HOST, NULL_ID, true, false, true, 0), "a -noBackend dedicated server keys by name at once");
		ctx.EqualStr(HOST_REAL, LF_PlayerIdentity.Resolve(HOST, HOST_REAL, true, false, false, 0), "a real identity is the key as it is");
	}

	//------------------------------------------------------------------------------------------------
	//! The online service answers after the listen host gave up and keyed the player by name: the
	//! account, its character and the records under the character's key all follow, through a restart.
	protected void StandInMovesOntoTheRealKey(LF_TestContext ctx)
	{
		int t0 = 2000000;
		ctx.EqualStr("", ListenHost(LATE, NULL_ID, t0), "the late player waits at first");
		string standIn = ListenHost(LATE, NULL_ID, t0 + LF_PlayerIdentity.LISTEN_WAIT_MS);
		ctx.EqualStr(LF_Utils.BuildLocalIdentity(LATE), standIn, "after the wait the name keys the player");

		LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();
		LF_PlayerAccount account = LF_PlayerAccountManager.GetOrCreate(standIn);
		account.AddCharacter(LF_PlayerCharacter.Create(PREFAB, "Late", "Comer", 30), true);
		SetBank(standIn, 300);

		ctx.EqualStr(LATE_REAL, ListenHost(LATE, LATE_REAL, t0 + LF_PlayerIdentity.LISTEN_WAIT_MS + 9000), "the real identity is the key once it arrives");
		LF_PlayerAccount moved = accounts.GetAccount(LATE_REAL);
		ctx.True(moved == account, "the stand-in account is now under the real identity");
		ctx.Equal(0, CountUnder(standIn), "no second account is left under the stand-in");
		ctx.EqualStr(standIn, LF_PlayerAccountManager.ResolveCharacterKey(LATE_REAL), "the character keeps its record key, so GetPlayerUID still finds its records");

		LF_PersistenceComponent persistence = FindPersistence(ctx);
		if (!persistence)
			return;

		JsonSaveContext saveContext = new JsonSaveContext(false);
		ctx.True(LF_PersistenceComponentSerializer.SerializeStore(persistence, saveContext) == ESerializeResult.OK, "the store serializes");
		string saved = saveContext.SaveToString();

		// A restart: the account comes back empty and the balance is off.
		accounts.AddToCache(LF_PlayerAccount.Create(LATE_REAL));
		LF_ATMManager.GetInstance().Deposit(standIn, 1);

		JsonLoadContext loadContext = new JsonLoadContext(false);
		ctx.True(loadContext.LoadFromString(saved), "the save parses");
		ctx.True(LF_PersistenceComponentSerializer.DeserializeStore(persistence, loadContext), "the save loads");
		LF_PlayerAccount loaded = accounts.GetFromCache(LATE_REAL);
		ctx.NotNull(loaded, "the account loads under the real identity");
		if (!loaded)
			return;

		ctx.Equal(1, loaded.GetCharacterCount(), "with its one character");
		ctx.EqualStr(standIn, loaded.GetActiveCharacterKey(), "the character's record key survives the restart");
		LF_BankAccount bank = LF_ATMManager.GetInstance().GetAccount(standIn);
		ctx.True(bank && bank.GetBalance() == 300, "its bank balance (300) survives the restart");
		ctx.Equal(0, CountUnder(standIn), "the save holds no second account under the stand-in");
	}

	//------------------------------------------------------------------------------------------------
	//! A returning player (account and character saved under the real identity) whose identity was
	//! late: the empty account the flow opened under the stand-in goes, the saved one is played.
	protected void ReturningPlayerKeepsTheirAccount(LF_TestContext ctx)
	{
		LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();
		LF_PlayerAccount saved = LF_PlayerAccount.Create(RETURNING_REAL);
		saved.AddCharacter(LF_PlayerCharacter.Create(PREFAB, "Back", "Again", 41), true);
		accounts.AddToCache(saved);

		int t0 = 3000000;
		ListenHost(RETURNING, NULL_ID, t0);
		string standIn = ListenHost(RETURNING, NULL_ID, t0 + LF_PlayerIdentity.LISTEN_WAIT_MS);
		LF_PlayerAccountManager.GetOrCreate(standIn);

		ListenHost(RETURNING, RETURNING_REAL, t0 + LF_PlayerIdentity.LISTEN_WAIT_MS + 1000);
		LF_PlayerAccount played = accounts.GetAccount(RETURNING_REAL);
		ctx.True(played == saved, "the returning player's saved account is the one played");
		ctx.Equal(1, played.GetCharacterCount(), "with their character");
		ctx.Equal(0, CountUnder(standIn), "the empty stand-in account is dropped, not saved beside it");
	}

	//------------------------------------------------------------------------------------------------
	//! Both keys hold characters (an offline session under the name, a real one under the identity):
	//! nothing is merged or lost.
	protected void BothKeptWhenBothHoldCharacters(LF_TestContext ctx)
	{
		LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();
		LF_PlayerAccount real = LF_PlayerAccount.Create(BOTH_REAL);
		real.AddCharacter(LF_PlayerCharacter.Create(PREFAB, "Real", "One", 33), true);
		accounts.AddToCache(real);

		int t0 = 4000000;
		ListenHost(BOTH, NULL_ID, t0);
		string standIn = ListenHost(BOTH, NULL_ID, t0 + LF_PlayerIdentity.LISTEN_WAIT_MS);
		LF_PlayerAccount offline = LF_PlayerAccountManager.GetOrCreate(standIn);
		offline.AddCharacter(LF_PlayerCharacter.Create(PREFAB, "Offline", "One", 33), true);

		ListenHost(BOTH, BOTH_REAL, t0 + LF_PlayerIdentity.LISTEN_WAIT_MS + 1000);
		ctx.True(accounts.GetAccount(BOTH_REAL) == real, "the real account is played");
		ctx.Equal(1, real.GetCharacterCount(), "the real account keeps only its own character");
		ctx.Equal(1, CountUnder(standIn), "the offline account stays saved");
	}

	//------------------------------------------------------------------------------------------------
	//! Accounts the save would write under this uid.
	protected static int CountUnder(string uid)
	{
		int count = 0;
		foreach (LF_PlayerAccountRecord record : LF_PlayerAccountManager.ExportAll())
		{
			if (record && record.m_sPersistentId == uid)
				count++;
		}

		return count;
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_PersistenceComponent FindPersistence(LF_TestContext ctx)
	{
		SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		ctx.NotNull(gameMode, "the DebugWorld game mode resolves");
		if (!gameMode)
			return null;

		LF_PersistenceComponent persistence = LF_PersistenceComponent.Cast(gameMode.FindComponent(LF_PersistenceComponent));
		ctx.NotNull(persistence, "the game mode carries LF_PersistenceComponent");
		return persistence;
	}

	//------------------------------------------------------------------------------------------------
	protected static void SetBank(string key, int balance)
	{
		LF_ATMManager atm = LF_ATMManager.GetInstance();
		LF_BankAccount bank = atm.GetAccount(key);
		if (!bank)
			bank = atm.CreateAccount(key);

		if (bank && bank.GetBalance() > 0)
			atm.Withdraw(key, bank.GetBalance());

		atm.Deposit(key, balance);
	}
}
