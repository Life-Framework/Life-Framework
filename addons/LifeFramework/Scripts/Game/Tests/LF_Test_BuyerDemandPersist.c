// red-proof: observed 2026-11-19 `tools\cli test --no-build --red LF_Test_BuyerDemandSell,LF_Test_BuyerDemandPersist`: RED PROOF OK (both fail with the hook on); green with --only. RedProof() skips the sales and the load.
//! Buyer purses ride the real save path (#3599, the "buyerdemand" save section): a purse left part-spent
//! comes back with its units and stamp after a save and load, loading twice gives the same book, a purse
//! over a lowered cap loads held to the cap, and a save without the section leaves every buyer full.
//! Across a restart (`tools\cli wt e2e`): every non-reload run leaves the e2e buyer's plank purse at
//! E2E_LEFT; the reload boot finds it.
[LF_TestInfo(LF_TestTier.PERSISTENCE)]
class LF_Test_BuyerDemandPersist : LF_Test
{
	protected static const ResourceName PLANK = "{45D1B36F99DD2FC3}Prefabs/Resources/WoodCutting/WoodPlank.et";
	protected static const ResourceName BOARD = "{D2A8639FA0CA09EF}Prefabs/Resources/WoodCutting/HardwoodBoard.et";
	protected static const string BUYER = "persist-timber";
	protected static const string E2E_BUYER = "e2e-timber";
	//! A fixed clock, so the stamps are exact.
	protected static const int NOW = 1800000000;
	//! Units the e2e save boot leaves in the e2e buyer's purse.
	static const int E2E_LEFT = 7;

	override string GetName()
	{
		return "market/buyer-demand-persist";
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_BuyerDemandConfig MakeConfig(int plankCap)
	{
		LF_BuyerDemandConfig config = new LF_BuyerDemandConfig();
		config.m_aRows = {};
		config.m_aRows.Insert(LF_BuyerDemandRow.Make(BUYER, PLANK, plankCap, 20, 8));
		config.m_aRows.Insert(LF_BuyerDemandRow.Make(BUYER, BOARD, 24, 8, 12));
		config.Sanitize();
		return config;
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
		// The reload boot: the e2e purse came back from the real save, when it carried the section.
		if (LF_E2E.RequiresSection(LF_BuyerDemandSavePayload.NAME))
			VerifyE2E(ctx);

		return;
#endif

		ctx.NotNull(LF_SaveSections.Get(LF_BuyerDemandSavePayload.NAME), "buyerdemand is a registered save section");

		LF_BuyerDemandConfig.UseForTest(MakeConfig(40));
		LF_BuyerDemandBook.ResetStatics();

		// Two buyers' goods spent through the public mutator the trader uses: 15 planks, 4 boards.
		LF_BuyerDemandSale planks = LF_BuyerDemandBook.Sell(BUYER, PLANK, 15, NOW);
		LF_BuyerDemandSale boards = LF_BuyerDemandBook.Sell(BUYER, BOARD, 4, NOW + 60);
		ctx.True(planks && planks.m_iAccepted == 15, "15 planks are taken");
		ctx.True(boards && boards.m_iAccepted == 4, "4 boards are taken");
		CheckPurse(ctx, PLANK, 25, NOW, "after the sales");
		CheckPurse(ctx, BOARD, 20, NOW + 60, "after the sales");

		JsonSaveContext saveContext = new JsonSaveContext(false);
		ctx.True(LF_PersistenceComponentSerializer.SerializeStore(persistence, saveContext) == ESerializeResult.OK, "the store serializes");
		string saved = saveContext.SaveToString();
		ctx.True(saved.Contains(LF_SaveSections.OBJECT_PREFIX + LF_BuyerDemandSavePayload.NAME), "the save carries the buyerdemand section");
		ctx.Equal(0, LF_PersistenceComponentSerializer.PayloadSinceVersion(LF_BuyerDemandSavePayload.NAME), "buyerdemand is not a legacy positional payload");

		// Restart: the book is empty.
		LF_BuyerDemandBook.ResetStatics();
		// RedProof(): the load is skipped, so every read-back assertion fails (`--red`).
		if (!RedProof())
			ReadSection(saved, ctx);

		CheckPurse(ctx, PLANK, 25, NOW, "after the load");
		CheckPurse(ctx, BOARD, 20, NOW + 60, "after the load");

		// Loading twice replaces, never accumulates; a live purse the save lacks is gone.
		LF_BuyerDemandBook.Restore(E2E_BUYER, BOARD, LF_BuyerPurse.Make(LF_BuyerDemandRow.Make(E2E_BUYER, BOARD, 9, 1, 1), 3, NOW + 120));
		ReadSection(saved, ctx);
		ReadSection(saved, ctx);
		CheckPurse(ctx, PLANK, 25, NOW, "after a second load");
		ctx.Equal(2, LF_BuyerDemandBook.Entries().Count(), "the book holds the two saved purses only");

		// The config lowers the plank cap under the saved units: they load held to the cap.
		LF_BuyerDemandConfig.UseForTest(MakeConfig(10));
		LF_BuyerDemandBook.ResetStatics();
		ReadSection(saved, ctx);
		CheckPurse(ctx, PLANK, 10, NOW, "held to the lowered cap");

		// The reload refills for the time the server was down: a purse stamped an hour back holds 20 more.
		LF_BuyerDemandConfig.UseForTest(MakeConfig(40));
		LF_BuyerDemandBook.ResetStatics();
		ReadSection(saved, ctx);
		ctx.Equal(25, LF_BuyerDemandBook.RemainingAt(BUYER, PLANK, NOW), "at the save's own clock it still holds 25");
		ctx.Equal(40, LF_BuyerDemandBook.RemainingAt(BUYER, PLANK, NOW + 3600), "an hour after the save it has refilled by 20, held to its cap of 40");

		// A save without this section (an older save) on a boot: no stored purses, every buyer full.
		LF_BuyerDemandBook.ResetStatics();
		JsonSaveContext oldContext = new JsonSaveContext(false);
		array<string> omit = {LF_BuyerDemandSavePayload.NAME};
		LF_PersistenceComponentSerializer.SerializeStoreOmitting(persistence, oldContext, omit);
		string oldSave = oldContext.SaveToString();
		ctx.False(oldSave.Contains(LF_SaveSections.OBJECT_PREFIX + LF_BuyerDemandSavePayload.NAME), "the older save has no buyerdemand section");
		JsonLoadContext oldLoad = new JsonLoadContext(false);
		ctx.True(oldLoad.LoadFromString(oldSave), "the old save parses");
		LF_PersistenceComponentSerializer.DeserializeStore(persistence, oldLoad);
		ctx.Equal(0, LF_BuyerDemandBook.Entries().Count(), "a pre-section save leaves no stored purses");
		ctx.Equal(40, LF_BuyerDemandBook.RemainingAt(BUYER, PLANK, NOW), "an unsaved buyer stands full");

		LF_BuyerDemandConfig.Reload();
		LF_BuyerDemandBook.ResetStatics();

		// Every non-reload run (the e2e save boot among them) leaves the e2e purse at E2E_LEFT.
		SeedE2E(ctx);
	}

	//------------------------------------------------------------------------------------------------
	protected static void CheckPurse(LF_TestContext ctx, ResourceName good, int remaining, int stamp, string when)
	{
		LF_BuyerPurse p = LF_BuyerDemandBook.Snapshot(BUYER, good);
		ctx.NotNull(p, string.Format("%1 has a stored purse %2", good, when));
		if (!p)
			return;

		ctx.Equal(remaining, p.m_iRemaining, string.Format("%1 units left %2", good, when));
		ctx.Equal(stamp, p.m_iStamp, string.Format("%1 refill stamp %2", good, when));
	}

	//------------------------------------------------------------------------------------------------
	//! Reads the buyerdemand section of a whole-store JSON save, as LF_SaveSections.ReadAll does.
	protected static void ReadSection(string saved, LF_TestContext ctx)
	{
		LF_SavePayload section = LF_SaveSections.Get(LF_BuyerDemandSavePayload.NAME);
		if (!section)
			return;

		JsonLoadContext context = new JsonLoadContext(false);
		ctx.True(context.LoadFromString(saved), "the save parses");
		ctx.True(context.StartObject(LF_SaveSections.OBJECT_PREFIX + LF_BuyerDemandSavePayload.NAME), "the buyerdemand section opens");
		int savedVersion;
		context.ReadValue(LF_SaveSections.VERSION_KEY, savedVersion);
		ctx.Equal(section.Version(), savedVersion, "the section carries its own version");
		ctx.True(section.ReadSection(context, savedVersion), "the buyerdemand section reads");
		context.EndObject();
	}

	//------------------------------------------------------------------------------------------------
	//! wt e2e save boot: the e2e buyer's plank purse holds E2E_LEFT in the live book. The buyer has no
	//! config row, so the purse is also what proves a row-less purse survives a restart.
	static void SeedE2E(LF_TestContext ctx)
	{
		LF_BuyerDemandRow row = LF_BuyerDemandRow.Make(E2E_BUYER, PLANK, 20, 0, 8);
		LF_BuyerDemandBook.Restore(E2E_BUYER, PLANK, LF_BuyerPurse.Make(row, E2E_LEFT, System.GetUnixTime()));
		LF_BuyerPurse p = LF_BuyerDemandBook.Snapshot(E2E_BUYER, PLANK);
		ctx.True(p && p.m_iRemaining == E2E_LEFT, "e2e plank purse is set before the save");
	}

	//------------------------------------------------------------------------------------------------
	//! wt e2e reload boot: the same purse after the restart.
	static void VerifyE2E(LF_TestContext ctx)
	{
		LF_BuyerPurse p = LF_BuyerDemandBook.Snapshot(E2E_BUYER, PLANK);
		ctx.NotNull(p, "e2e plank purse is back after the restart");
		if (!p)
			return;

		ctx.Equal(E2E_LEFT, p.m_iRemaining, "e2e plank purse units survive the restart");
		ctx.True(p.m_iStamp > 0, "e2e plank purse stamp survives the restart");
		LF_Debug.Info(LF_BuyerDemandBook.FEATURE, string.Format("e2e restart: %1 holds %2, stamped %3", E2E_BUYER, p.m_iRemaining, p.m_iStamp));
	}
}
