// red-proof: observed 2026-11-23 `tools\cli test --no-build --red LF_Test_OutpostStandingAward`: RED PROOF OK; RedProof() leaves the outpost zone out, so the sale earns no flag and the officer is not refused. Green with --only (28 assertions).
//! outposts/standing-award (#4224, GA-O4, N27): a sale to a trader inside an outpost awards standing on the
//! one underworld track and marks the seller "known here" at that outpost; the flag is written in the
//! "outpoststanding" save section and reads back after the store is wiped, twice the same. An on-duty
//! officer is refused inside the zone in words (#4217) and keeps the goods. Restart: the e2e character's
//! flag survives `wt e2e`.
[LF_TestInfo(LF_TestTier.PERSISTENCE)]
class LF_Test_OutpostStandingAward : LF_Test
{
	static const string UID = "test-outpost-standing";
	static const string OUTPOST_ID = "test-o4";
	static const string E2E_KEY = "e2e-outpost-standing";
	static const string E2E_OUTPOST = "e2e-outpost";
	protected static const ResourceName PLANK = "{45D1B36F99DD2FC3}Prefabs/Resources/WoodCutting/WoodPlank.et";

	override string GetName()
	{
		return "outposts/standing-award";
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
		if (LF_E2E.RequiresSection(LF_OutpostStandingSavePayload.NAME))
			ctx.True(LF_OutpostStanding.IsKnownHere(E2E_KEY, E2E_OUTPOST), "the e2e character is still known at the outpost after the restart");

		return;
#endif

		ctx.NotNull(LF_SaveSections.Get(LF_OutpostStandingSavePayload.NAME), "outpoststanding is a registered save section");

		IEntity traderOwner = GetGame().GetWorld().FindEntityByName(LF_TestAnnex.APPLE_TRADE_CONTAINER);
		ctx.NotNull(traderOwner, "Annex_AppleTradeContainer resolves in the DebugWorld");
		LF_TraderManagerComponent trader;
		if (traderOwner)
			trader = LF_Component<LF_TraderManagerComponent>.Find(traderOwner);
		ctx.NotNull(trader, "the container carries the trader manager");
		if (!trader)
			return;

		string wasBuyer = trader.m_sBuyerId;
		trader.m_sBuyerId = string.Empty;
		LF_TraderItem plankItem = new LF_TraderItem();
		plankItem.m_ItemPrefab = PLANK;
		plankItem.m_ValuePerItem = 5;
		trader.m_aTradableItems.Insert(plankItem);

		// The outpost: its zone around the trader. RedProof() leaves it out, so no sale is at an outpost
		// and no officer is refused.
		string zone = LF_Outpost.ZONE_PREFIX + OUTPOST_ID;
		if (!RedProof())
			LF_Zones.RegisterCircle(zone, LF_ZoneRule.SAFE, traderOwner.GetOrigin(), 30);

		IEntity seller = LF_TestCharacterFixture.Spawn(ctx);
		if (seller)
		{
			LF_Utils.SetUidForTests(seller, UID);
			RunSales(ctx, persistence, seller, traderOwner);
			LF_Utils.SetUidForTests(null, string.Empty);
			LF_TestCharacterFixture.Delete(seller);
		}

		LF_Zones.Unregister(zone);
		trader.m_aTradableItems.RemoveItem(plankItem);
		trader.m_sBuyerId = wasBuyer;
		LF_OutpostStanding.Forget(UID);
		LF_Reputation.Forget(UID);
		LF_PlayerAccount account = LF_PlayerAccountManager.GetOrCreate(UID);
		account.SetOnDuty(false);
		account.SetFaction(LF_Faction.CIVILIAN);

		// Every non-reload run (the e2e save boot among them) leaves the e2e flag in the store.
		LF_OutpostStanding.RecordSale(E2E_KEY, E2E_OUTPOST);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunSales(LF_TestContext ctx, LF_PersistenceComponent persistence, IEntity seller, IEntity traderOwner)
	{
		LF_OutpostStanding.Forget(UID);
		LF_Reputation.Forget(UID);
		LF_PlayerAccount account = LF_PlayerAccountManager.GetOrCreate(UID);
		account.SetFaction(LF_Faction.CIVILIAN);
		account.SetOnDuty(false);

		ctx.Equal(2, LF_InventoryUtils.AddAmount(seller, PLANK, 2), "the seller gets 2 planks");
		if (ctx.FailureCount() > 0)
			return;

		// A civilian sale at the outpost: underworld standing and the flag.
		int before = LF_Reputation.Points(UID, LF_Reputation.FACTION_UNDERWORLD);
		ctx.True(SellOne(seller, traderOwner) == LF_ETraderSale.SOLD, "a civilian sells a plank at the outpost trader");
		ctx.True(LF_Reputation.Points(UID, LF_Reputation.FACTION_UNDERWORLD) > before, "the sale earns underworld standing");
		ctx.Equal(0, LF_Reputation.Points(UID, LF_Reputation.FACTION_TOWN), "the sale moves no other track");
		ctx.True(LF_OutpostStanding.IsKnownHere(UID, OUTPOST_ID), "the seller is known at this outpost");
		ctx.False(LF_OutpostStanding.IsKnownHere(UID, "other-outpost"), "the seller is not known at another outpost");

		// Save, wipe, load: the flag is back, and a second load gives the same.
		JsonSaveContext saveContext = new JsonSaveContext(false);
		ctx.True(LF_PersistenceComponentSerializer.SerializeStore(persistence, saveContext) == ESerializeResult.OK, "the store serializes");
		string saved = saveContext.SaveToString();
		ctx.True(saved.Contains(LF_SaveSections.OBJECT_PREFIX + LF_OutpostStandingSavePayload.NAME), "the save carries the outpoststanding section");
		ctx.Equal(0, LF_PersistenceComponentSerializer.PayloadSinceVersion(LF_OutpostStandingSavePayload.NAME), "outpoststanding is not a legacy positional payload");

		array<ref LF_OutpostKnownRow> keep = LF_OutpostStanding.ExportAll();
		LF_OutpostStanding.ApplyAll(null);
		ctx.False(LF_OutpostStanding.IsKnownHere(UID, OUTPOST_ID), "a fresh boot knows nobody");
		JsonLoadContext loadContext = new JsonLoadContext(false);
		ctx.True(loadContext.LoadFromString(saved), "the save parses");
		ReadSection(loadContext, ctx);
		ctx.True(LF_OutpostStanding.IsKnownHere(UID, OUTPOST_ID), "the flag is back after the load");
		JsonLoadContext again = new JsonLoadContext(false);
		again.LoadFromString(saved);
		ReadSection(again, ctx);
		ctx.True(LF_OutpostStanding.IsKnownHere(UID, OUTPOST_ID), "loading twice keeps the flag");
		ctx.Equal(keep.Count(), LF_OutpostStanding.ExportAll().Count(), "the load brings back every flag the store held");

		array<ref LF_OutpostKnownRow> bad = {};
		bad.Insert(LF_OutpostKnownRow.Create("", OUTPOST_ID));
		bad.Insert(LF_OutpostKnownRow.Create(UID, ""));
		ctx.Equal(0, LF_OutpostStanding.ApplyAll(bad), "rows with no character or no outpost are skipped");
		LF_OutpostStanding.ApplyAll(keep);

		// An on-duty officer is refused in words inside the zone and keeps the plank.
		account.SetFaction(LF_Faction.POLICE);
		account.SetOnDuty(true);
		ctx.True(LF_Outpost.RefusesService(seller, traderOwner.GetOrigin()), "the outpost trader refuses an on-duty officer");
		ctx.True(SellOne(seller, traderOwner) == LF_ETraderSale.OUTPOST_REFUSED, "the sale is refused as an outpost refusal");
		ctx.Equal(1, LF_InventoryUtils.GetAmount(seller, PLANK), "the refused officer keeps the plank");
		account.SetOnDuty(false);
		account.SetFaction(LF_Faction.CIVILIAN);
	}

	//------------------------------------------------------------------------------------------------
	protected LF_ETraderSale SellOne(IEntity seller, IEntity traderOwner)
	{
		InventoryStorageManagerComponent storage = LF_InventoryUtils.GetResponsibleStorageManager(seller);
		array<IEntity> items = LF_InventoryUtils.FindItemsByPrefab(storage, PLANK);
		if (items.Count() == 0)
			return LF_ETraderSale.NO_ITEM;

		return LF_TraderSale.Sell(seller, items[0], traderOwner, false);
	}

	//------------------------------------------------------------------------------------------------
	protected static void ReadSection(notnull JsonLoadContext context, LF_TestContext ctx)
	{
		LF_SavePayload section = LF_SaveSections.Get(LF_OutpostStandingSavePayload.NAME);
		if (!section)
			return;

		ctx.True(context.StartObject(LF_SaveSections.OBJECT_PREFIX + LF_OutpostStandingSavePayload.NAME), "the outpoststanding section opens");
		int savedVersion;
		context.ReadValue(LF_SaveSections.VERSION_KEY, savedVersion);
		ctx.Equal(section.Version(), savedVersion, "the section carries its own version");
		ctx.True(section.ReadSection(context, savedVersion), "the outpoststanding section reads");
		context.EndObject();
	}
}
