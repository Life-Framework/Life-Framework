// red-proof: observed 2026-11-19 `tools\cli test --no-build --tier persistence --red LF_Test_StallRentPersist`: RED-OK (27 assertions), `the fee of a stall held at the restart is back in the bank (expected 50, got 0)`; `--only` then passed. RedProof() skips the boot load.
//! market/stall-rent-persist (#3279): a stall rent rides the real save ("stall_rents" section). A pitch held at the
//! save is paid back into the holder's bank on the next boot, once; a load in the session that took the rent pays
//! nothing, a rent that lapsed before the save is not saved, and a save without the section refunds nothing.
//! Across a restart (`tools\cli wt e2e`): every non-reload run holds an e2e pitch at the save; the reload boot finds
//! the fee back in the e2e holder's bank.
[LF_TestInfo(LF_TestTier.PERSISTENCE)]
class LF_Test_StallRentPersist : LF_Test
{
	protected static const string HOLDER = "persist-stall-holder";
	protected static const string LAPSED = "persist-stall-lapsed";
	protected static const string E2E_HOLDER = "e2e-stall-holder";
	protected static const int FEE = 50;
	protected static const int DAY = 12;
	protected static const int NOW = 1800000000;
	protected static const int E2E_BANK = 2000;

	override string GetName()
	{
		return "market/stall-rent-persist";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		LF_PersistenceComponent persistence;
		if (gameMode)
			persistence = LF_PersistenceComponent.Cast(gameMode.FindComponent(LF_PersistenceComponent));

		ctx.NotNull(persistence, "the game mode carries LF_PersistenceComponent");
		if (!persistence)
			return;

#ifdef LF_E2E_LOAD_BOOT
		// The reload boot: the e2e pitch's fee came back into the bank from the real save.
		if (LF_E2E.RequiresSection(LF_StallSavePayload.NAME))
			VerifyE2E(ctx);

		return;
#endif

		ctx.NotNull(LF_SaveSections.Get(LF_StallSavePayload.NAME), "stall_rents is a registered save section");
		ctx.Equal(0, LF_PersistenceComponentSerializer.PayloadSinceVersion(LF_StallSavePayload.NAME), "stall_rents is not a legacy positional payload");

		LF_StallRentBook.ResetStatics();
		LF_BankAccount bank = LF_ATMManager.GetOrCreate(HOLDER);
		int bankBefore = bank.GetBalance();
		LF_BankAccount lapsedBank = LF_ATMManager.GetOrCreate(LAPSED);
		int lapsedBefore = lapsedBank.GetBalance();

		// Two pitches are paid; one of them lapses before the save and is no longer held.
		string held = LF_StallRentBook.Record(HOLDER, DAY, FEE, NOW);
		string gone = LF_StallRentBook.Record(LAPSED, DAY, FEE, NOW + 5);
		ctx.True(!held.IsEmpty() && !gone.IsEmpty(), "both rents are recorded");
		LF_StallRentBook.Release(gone);
		ctx.Equal(1, LF_StallRentBook.ActiveCount(), "one pitch is held");

		JsonSaveContext saveContext = new JsonSaveContext(false);
		ctx.True(LF_PersistenceComponentSerializer.SerializeStore(persistence, saveContext) == ESerializeResult.OK, "the store serializes");
		string saved = saveContext.SaveToString();
		ctx.True(saved.Contains(LF_SaveSections.OBJECT_PREFIX + LF_StallSavePayload.NAME), "the save carries the stall_rents section");

		// A load in the session that took the rent (a re-apply) pays nothing.
		ReadSection(saved, ctx);
		ctx.Equal(bankBefore, bank.GetBalance(), "a re-apply in the session that took the rent refunds nothing");

		// Restart: the session and its stalls are gone.
		LF_StallRentBook.ResetStatics();
		// RedProof(): the boot load is skipped, so the refund never arrives (`--red`).
		if (!RedProof())
			ReadSection(saved, ctx);

		ctx.Equal(bankBefore + FEE, bank.GetBalance(), "the fee of a stall held at the restart is back in the bank");
		ctx.Equal(lapsedBefore, lapsedBank.GetBalance(), "a pitch that lapsed before the save is not refunded");
		ReadSection(saved, ctx);
		ctx.Equal(bankBefore + FEE, bank.GetBalance(), "a second load refunds nothing twice");
		ctx.Equal(0, LF_StallRentBook.ActiveCount(), "the refunded rent is not held again");
		ctx.Equal(0, LF_StallRentBook.Export().Count(), "the next save holds no refunded rent");

		// A save without this section (an older save) on a boot refunds nothing.
		LF_StallRentBook.ResetStatics();
		JsonSaveContext oldContext = new JsonSaveContext(false);
		array<string> omit = {LF_StallSavePayload.NAME};
		LF_PersistenceComponentSerializer.SerializeStoreOmitting(persistence, oldContext, omit);
		string oldSave = oldContext.SaveToString();
		ctx.False(oldSave.Contains(LF_SaveSections.OBJECT_PREFIX + LF_StallSavePayload.NAME), "the older save has no stall_rents section");
		JsonLoadContext oldLoad = new JsonLoadContext(false);
		ctx.True(oldLoad.LoadFromString(oldSave), "the old save parses");
		LF_PersistenceComponentSerializer.DeserializeStore(persistence, oldLoad);
		ctx.Equal(bankBefore + FEE, bank.GetBalance(), "a pre-section save refunds nothing");

		bank.SetBalance(bankBefore);
		lapsedBank.SetBalance(lapsedBefore);
		LF_StallRentBook.ResetStatics();

		// Every non-reload run (the e2e save boot among them) holds the e2e pitch at the shutdown save.
		SeedE2E(ctx);
	}

	//------------------------------------------------------------------------------------------------
	//! Reads the stall_rents section of a whole-store JSON save, as LF_SaveSections.ReadAll does.
	protected static void ReadSection(string saved, LF_TestContext ctx)
	{
		LF_SavePayload section = LF_SaveSections.Get(LF_StallSavePayload.NAME);
		if (!section)
			return;

		JsonLoadContext context = new JsonLoadContext(false);
		ctx.True(context.LoadFromString(saved), "the save parses");
		ctx.True(context.StartObject(LF_SaveSections.OBJECT_PREFIX + LF_StallSavePayload.NAME), "the stall_rents section opens");
		int savedVersion;
		context.ReadValue(LF_SaveSections.VERSION_KEY, savedVersion);
		ctx.Equal(section.Version(), savedVersion, "the section carries its own version");
		ctx.True(section.ReadSection(context, savedVersion), "the stall_rents section reads");
		context.EndObject();
	}

	//------------------------------------------------------------------------------------------------
	//! wt e2e save boot: the e2e holder has a known bank balance and a pitch held at the shutdown save.
	static void SeedE2E(LF_TestContext ctx)
	{
		LF_BankAccount bank = LF_ATMManager.GetOrCreate(E2E_HOLDER);
		bank.SetBalance(E2E_BANK);
		string token = LF_StallRentBook.Record(E2E_HOLDER, DAY, FEE, System.GetUnixTime());
		ctx.False(token.IsEmpty(), "the e2e pitch is held before the save");
		ctx.Equal(1, LF_StallRentBook.ActiveCount(), "the e2e pitch is the one held rent");
	}

	//------------------------------------------------------------------------------------------------
	//! wt e2e reload boot: the fee is back in the bank, once.
	static void VerifyE2E(LF_TestContext ctx)
	{
		LF_BankAccount bank = LF_ATMManager.GetOrCreate(E2E_HOLDER);
		ctx.Equal(E2E_BANK + FEE, bank.GetBalance(), "e2e: the pitch fee held at the restart is back in the holder's bank");
		LF_Debug.Info(LF_StallConfig.FEATURE, string.Format("e2e restart: %1's bank holds %2", E2E_HOLDER, bank.GetBalance()));
	}
}
