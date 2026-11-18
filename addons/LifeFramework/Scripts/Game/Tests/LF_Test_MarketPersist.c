// red-proof: observed 2026-11-18 `tools/cli test --no-build --red LF_Test_MarketPersist`: RED PROOF OK, first `CopperBar.et has a stored price after the load`. Restart: `tools/cli wt e2e slice-2846` green; with VerifyE2E expecting E2E_PRICE + 1 the reload boot failed market/persist (applied 1 of 1 row), so the check is live.
//! Market prices ride the real save path (#2846, the "market" save section): two goods moved by sales come
//! back with their price and stamp after a save and load, loading twice gives the same book, a good the
//! config dropped is dropped, a price under a raised floor loads clamped, and a save without the section
//! loads every good at its base. Across a restart (`tools\cli wt e2e`): every non-reload run leaves the
//! iron bar at E2E_PRICE; the reload boot finds it.
[LF_TestInfo(LF_TestTier.PERSISTENCE)]
class LF_Test_MarketPersist : LF_Test
{
	protected static const ResourceName COPPER = "{577A665F1662E7AD}Prefabs/Resources/Mining/CopperBar.et";
	protected static const ResourceName IRON = "{BCD1E8CD7F975074}Prefabs/Resources/Mining/IronBar.et";
	//! A fixed clock, so the stamps are exact.
	protected static const int NOW = 1800000000;
	//! The iron bar's price the e2e save boot leaves (inside the core config's 15..45).
	static const int E2E_PRICE = 22;

	override string GetName()
	{
		return "market/persist";
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_MarketConfig MakeConfig(bool withIron, int copperFloor)
	{
		LF_MarketConfig config = new LF_MarketConfig();
		config.m_bEnabled = true;
		config.m_iMaxRecoveryHours = 168;
		config.m_aGoods = {};
		config.m_aGoods.Insert(LF_MarketGoodDef.Make(COPPER, 40, copperFloor, 60, 5, 10));
		if (withIron)
			config.m_aGoods.Insert(LF_MarketGoodDef.Make(IRON, 30, 15, 45, 10, 10));

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
		// The reload boot: the e2e price came back from the real save, when it carried the section
		// (a --pre-move or --legacy-save ref build wrote none; LF_E2E logs the skip).
		if (LF_E2E.RequiresSection(LF_MarketSavePayload.NAME))
			VerifyE2E(ctx);

		return;
#endif

		ctx.NotNull(LF_SaveSections.Get(LF_MarketSavePayload.NAME), "market is a registered save section");

		LF_MarketConfig.UseForTest(MakeConfig(true, 20));
		LF_MarketBook.ResetStatics();

		// Sales move two goods through the public mutator the trader uses.
		int oldPrice;
		int copperAfter = LF_MarketBook.ApplySaleAt(COPPER, 3, NOW, oldPrice);
		ctx.True(copperAfter > 0 && copperAfter < oldPrice, "a copper sale moves its price down");
		int ironAfter = LF_MarketBook.ApplySaleAt(IRON, 2, NOW + 60, oldPrice);
		ctx.True(ironAfter > 0 && ironAfter < oldPrice, "an iron sale moves its price down");

		JsonSaveContext saveContext = new JsonSaveContext(false);
		ctx.True(LF_PersistenceComponentSerializer.SerializeStore(persistence, saveContext) == ESerializeResult.OK, "the store serializes");
		string saved = saveContext.SaveToString();
		ctx.True(saved.Contains(LF_SaveSections.OBJECT_PREFIX + LF_MarketSavePayload.NAME), "the save carries the market section");
		ctx.Equal(0, LF_PersistenceComponentSerializer.PayloadSinceVersion(LF_MarketSavePayload.NAME), "market is not a legacy positional payload");

		// Restart: the book is empty.
		LF_MarketBook.ResetStatics();
		// RedProof(): the load is skipped, so every read-back assertion fails (`--red`).
		if (!RedProof())
			ReadSection(saved, ctx);

		CheckPrice(ctx, COPPER, copperAfter, NOW, "after the load");
		CheckPrice(ctx, IRON, ironAfter, NOW + 60, "after the load");

		// Loading twice replaces, never accumulates; a live good the save lacks is gone.
		LF_MarketBook.Restore(IRON, LF_MarketPrice.Make(44, NOW + 120));
		ReadSection(saved, ctx);
		ReadSection(saved, ctx);
		CheckPrice(ctx, COPPER, copperAfter, NOW, "after a second load");
		CheckPrice(ctx, IRON, ironAfter, NOW + 60, "after a second load");
		ctx.Equal(2, LF_MarketBook.Entries().Count(), "the book holds the two saved goods only");

		// The config drops iron and raises the copper floor above the saved price.
		int raisedFloor = copperAfter + 3;
		LF_MarketConfig.UseForTest(MakeConfig(false, raisedFloor));
		LF_MarketBook.ResetStatics();
		ReadSection(saved, ctx);
		ctx.True(!LF_MarketBook.Snapshot(IRON), "a good the config no longer lists is dropped");
		CheckPrice(ctx, COPPER, raisedFloor, NOW, "clamped to the raised floor");

		// A save without this section (an older save) on a boot: every good stands at its base.
		LF_MarketConfig.UseForTest(MakeConfig(true, 20));
		LF_MarketBook.ResetStatics();
		JsonSaveContext oldContext = new JsonSaveContext(false);
		array<string> omit = {LF_MarketSavePayload.NAME};
		LF_PersistenceComponentSerializer.SerializeStoreOmitting(persistence, oldContext, omit);
		string oldSave = oldContext.SaveToString();
		ctx.False(oldSave.Contains(LF_SaveSections.OBJECT_PREFIX + LF_MarketSavePayload.NAME), "the older save has no market section");
		JsonLoadContext oldLoad = new JsonLoadContext(false);
		ctx.True(oldLoad.LoadFromString(oldSave), "the old save parses");
		LF_PersistenceComponentSerializer.DeserializeStore(persistence, oldLoad);
		ctx.True(!LF_MarketBook.Snapshot(COPPER), "a pre-section save leaves copper with no stored price");
		ctx.True(!LF_MarketBook.Snapshot(IRON), "a pre-section save leaves iron with no stored price");
		ctx.Equal(40, LF_MarketBook.CurrentPriceAt(COPPER, NOW), "copper quotes at its base");
		ctx.Equal(30, LF_MarketBook.CurrentPriceAt(IRON, NOW), "iron quotes at its base");

		LF_MarketConfig.Reload();
		LF_MarketBook.ResetStatics();

		// Every non-reload run (the e2e save boot among them) leaves the iron bar at E2E_PRICE.
		SeedE2E(ctx);
	}

	//------------------------------------------------------------------------------------------------
	protected static void CheckPrice(LF_TestContext ctx, ResourceName good, int price, int stamp, string when)
	{
		LF_MarketPrice p = LF_MarketBook.Snapshot(good);
		ctx.NotNull(p, string.Format("%1 has a stored price %2", good, when));
		if (!p)
			return;

		ctx.Equal(price, p.m_iPrice, string.Format("%1 price %2", good, when));
		ctx.Equal(stamp, p.m_iLastSaleUnix, string.Format("%1 last sale stamp %2", good, when));
	}

	//------------------------------------------------------------------------------------------------
	//! Reads the market section of a whole-store JSON save, as LF_SaveSections.ReadAll does.
	protected static void ReadSection(string saved, LF_TestContext ctx)
	{
		LF_SavePayload section = LF_SaveSections.Get(LF_MarketSavePayload.NAME);
		if (!section)
			return;

		JsonLoadContext context = new JsonLoadContext(false);
		ctx.True(context.LoadFromString(saved), "the save parses");
		ctx.True(context.StartObject(LF_SaveSections.OBJECT_PREFIX + LF_MarketSavePayload.NAME), "the market section opens");
		int savedVersion;
		context.ReadValue(LF_SaveSections.VERSION_KEY, savedVersion);
		ctx.Equal(section.Version(), savedVersion, "the section carries its own version");
		ctx.True(section.ReadSection(context, savedVersion), "the market section reads");
		context.EndObject();
	}

	//------------------------------------------------------------------------------------------------
	//! wt e2e save boot: the iron bar stands at E2E_PRICE in the live book.
	static void SeedE2E(LF_TestContext ctx)
	{
		LF_MarketBook.Restore(IRON, LF_MarketPrice.Make(E2E_PRICE, System.GetUnixTime()));
		LF_MarketPrice p = LF_MarketBook.Snapshot(IRON);
		ctx.True(p && p.m_iPrice == E2E_PRICE, "e2e iron price is set before the save");
	}

	//------------------------------------------------------------------------------------------------
	//! wt e2e reload boot: the same stored price after the restart (the core config lists iron).
	static void VerifyE2E(LF_TestContext ctx)
	{
		LF_MarketPrice p = LF_MarketBook.Snapshot(IRON);
		ctx.NotNull(p, "e2e iron price is back after the restart");
		if (!p)
			return;

		ctx.Equal(E2E_PRICE, p.m_iPrice, "e2e iron price survives the restart");
		ctx.True(p.m_iLastSaleUnix > 0, "e2e iron sale stamp survives the restart");
		LF_Debug.Info(LF_MarketBook.FEATURE, string.Format("e2e restart: %1 stored at %2, stamped %3", IRON, p.m_iPrice, p.m_iLastSaleUnix));
	}
}
