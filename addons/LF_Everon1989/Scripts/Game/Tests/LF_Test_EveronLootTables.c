// red-proof: #3844: with no scrap rows in the tables (before this change) every `drops` / `holds the most` line fails on the unfixed conf; the pay guard stays green. On main (no Configs/Loot overlay) `tools\cli test --no-build --only LF_Test_EveronLootTables` fails every `has a loot table` line; RedProof() looks the kinds up under a missing name. #3345: before the three prefabs exist (and under RedProof) every `prefab spawns` line fails.
// red-proof (2026-11-27, no-copper-gold): the copper nugget row back in the dump table fails `dump drops no copper nugget (no copper on the island)`.
//! #3344 (slice of #1020): the 1989 pack's loot tables for homes, shop back rooms, sheds and the dump
//! (`Configs/Loot/LF_Loot_Everon1989.conf`, merged over the core crate table). Checked against the merged
//! pack config (pack leg): each table is rollable, every row's prefab loads and has a sell price, the dump
//! holds the most scrap and the shed the most tools, and an hour of searching, at the rate the cooldowns
//! allow, is worth well under an hour of the parcel job at sale prices (the maintainer rule: loot pays on
//! luck, never more than a legal hour).
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronLootTables : LF_Test
{
	static const ResourceName IRON_NUGGET = "{6853E2E2AE878611}Prefabs/Resources/Mining/IronNugget.et";
	static const ResourceName COPPER_NUGGET = "{1881C4F2BD43E3E1}Prefabs/Resources/Mining/CopperNugget.et";
	static const ResourceName ETOOL = "{0619993AE6CDBF85}Prefabs/Items/Equipment/Accessories/ETool_ALICE/ETool_ALICE.et";
	static const ResourceName AXE = "{9CAC68BA551417A4}Prefabs/Tools/Axe/Axe.et";
	static const ResourceName TOOL_CHEST = "{38318F0369D1959B}Prefabs/Props/Loot/LF_LootToolChest.et";
	static const ResourceName KITCHEN_CUPBOARD = "{51C99823106DB7C9}Prefabs/Props/Loot/LF_LootKitchenCupboard.et";
	//! #3844: the five scrap items of #3823 (Prefabs/Items/Scrap/).
	static const ResourceName SCRAP_METAL = "{EC59EAAEC5529F4A}Prefabs/Items/Scrap/LF_ScrapMetal.et";
	static const ResourceName COPPER_WIRE = "{8092D345F08C5F22}Prefabs/Items/Scrap/LF_CopperWireCoil.et";
	static const ResourceName RUBBER_SCRAP = "{0FC6120F0F62EFBA}Prefabs/Items/Scrap/LF_RubberScrap.et";
	static const ResourceName WORN_TOOL = "{507AAEC2A7BBFFBB}Prefabs/Items/Scrap/LF_WornHandTool.et";
	static const ResourceName OLD_RADIO = "{3F771F8119B03303}Prefabs/Items/Scrap/LF_OldRadio.et";
	static const ResourceName SCRAP_PILE = "{A42103AE580868E6}Prefabs/Props/Loot/LF_LootScrapPile.et";

	//! The same 6 minutes a parcel run takes as in `tools\cli economy` (an assumption, not measured).
	protected static const float PARCEL_MINUTES = 6;
	//! Containers of one kind a searcher can walk between inside an hour (the village's homes, sheds, dump).
	protected static const float CONTAINERS_IN_REACH = 12;
	//! A search takes a few seconds, but walking between containers caps the rate however short the cooldown.
	protected static const float MAX_SEARCHES_PER_HOUR = 60;
	//! Searching must pay at most this share of the parcel job's hour.
	protected static const float MAX_SHARE_OF_PARCEL = 0.5;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/loot-tables";
	}

	//------------------------------------------------------------------------------------------------
	//! What one item sells for. The shop manager prices an unlisted prefab at the config default (25), which is far
	//! above what a scrap item fetches, so the five scrap items (#3844) are valued at their catalogue sell price,
	//! the one the scrapyard pays; everything else keeps the shop manager's price.
	protected static int SaleValue(ResourceName prefab, LF_ShopManagerComponent manager)
	{
		array<ResourceName> scrapItems = {SCRAP_METAL, COPPER_WIRE, RUBBER_SCRAP, WORN_TOOL, OLD_RADIO};
		if (scrapItems.Contains(prefab))
		{
			LF_CatalogueItem item = LF_ItemCatalogue.ItemByPrefab(LF_ItemCatalogueConfig.Get(), prefab);
			if (item)
				return item.m_iSellPrice;
		}

		return manager.GetSellPrice(prefab);
	}

	//------------------------------------------------------------------------------------------------
	//! Expected sale value of one fill of the table at the shop's sell prices.
	protected static float ValuePerFill(LF_LootTableDef table, LF_ShopManagerComponent manager)
	{
		float total;
		float value;
		foreach (LF_LootRowDef row : table.m_aRows)
		{
			total += row.m_iWeight;
			value += row.m_iWeight * (row.m_iCountMin + row.m_iCountMax) * 0.5 * SaleValue(row.m_Prefab, manager);
		}

		if (total <= 0)
			return 0;

		return value / total * table.m_iRolls;
	}

	//------------------------------------------------------------------------------------------------
	//! Expected items of one prefab per fill.
	protected static float CountPerFill(LF_LootTableDef table, array<ResourceName> prefabs)
	{
		float total;
		float count;
		foreach (LF_LootRowDef row : table.m_aRows)
		{
			total += row.m_iWeight;
			if (prefabs.Contains(row.m_Prefab))
				count += row.m_iWeight * (row.m_iCountMin + row.m_iCountMax) * 0.5;
		}

		if (total <= 0)
			return 0;

		return count / total * table.m_iRolls;
	}

	//------------------------------------------------------------------------------------------------
	//! #3345: a loot prop carries LF_LootContainerComponent, names the kind its table is under, that table
	//! exists in the merged pack config, and one search of it puts items in its storage.
	protected void CheckContainerPrefab(LF_TestContext ctx, ResourceName prefab, string label, string expectedKind, vector origin)
	{
		IEntity prop = LF_Utils.SpawnEntityPrefab(prefab, origin);
		ctx.NotNull(prop, string.Format("%1 prefab spawns", label));
		if (!prop)
			return;

		LF_LootContainerComponent container = LF_Component<LF_LootContainerComponent>.Find(prop);
		ctx.NotNull(container, string.Format("%1 carries LF_LootContainerComponent", label));
		if (container)
		{
			ctx.EqualStr(expectedKind, container.GetKind(), string.Format("%1 names the %2 table", label, expectedKind));
			ctx.NotNull(LF_LootConfig.TableOf(container.GetKind()), string.Format("the pack defines the %1 table %2 names", expectedKind, label));
			ctx.NotNull(LF_Component<InventoryStorageManagerComponent>.Find(prop), string.Format("%1 has a storage manager", label));
			ctx.True(container.IsEmpty() && container.CanOfferSearch(), string.Format("%1 starts empty and offers Search", label));
			ctx.True(container.Search(1000, 12345) == LF_ELootSearch.FILLED, string.Format("the first search of %1 fills it", label));
			ctx.True(!container.IsEmpty(), string.Format("%1 holds items after a search", label));
		}

		ctx.NotNull(LF_Component<ActionsManagerComponent>.Find(prop), string.Format("%1 has the action manager that offers Search", label));
		SCR_EntityHelper.DeleteEntityAndChildren(prop);
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_ShopManagerComponent manager = LF_ShopManagerComponent.GetInstance();
		ctx.NotNull(manager, "the shop manager exists");
		if (!manager)
			return;

		IEntity anchor = GetGame().GetWorld().FindEntityByName(LF_TestAnnex.APPLE_TRADE_CONTAINER);
		ctx.NotNull(anchor, "Annex_AppleTradeContainer resolves in the DebugWorld");
		if (anchor)
		{
			vector at = anchor.GetOrigin() + Vector(4, 0.1, 4);
			ResourceName toolChest = TOOL_CHEST;
			ResourceName cupboard = KITCHEN_CUPBOARD;
			ResourceName scrapPile = SCRAP_PILE;
			if (RedProof())
			{
				toolChest = "";
				cupboard = "";
				scrapPile = "";
			}

			CheckContainerPrefab(ctx, toolChest, "the tool chest", "shed", at);
			CheckContainerPrefab(ctx, cupboard, "the kitchen cupboard", "home", at + Vector(3, 0, 0));
			CheckContainerPrefab(ctx, scrapPile, "the scrap pile", "dump", at + Vector(6, 0, 0));
		}

		int parcelPay = LF_CivilianJob.PayFor(LF_EJobType.PARCEL);
		float parcelHourly = parcelPay * 60 / PARCEL_MINUTES;
		ctx.True(parcelHourly > 0, string.Format("the parcel job pays (%1 an hour)", parcelHourly));

		array<string> kinds = {"home", "backroom", "shed", "dump"};
		array<ResourceName> scrap = {IRON_NUGGET};
		array<ResourceName> noCopper = {COPPER_NUGGET};	// the island has no copper rock: nothing drops a copper nugget
		array<ResourceName> tools = {ETOOL, AXE};
		array<ResourceName> metal = {SCRAP_METAL};
		array<ResourceName> rubber = {RUBBER_SCRAP};
		array<ResourceName> radio = {OLD_RADIO};
		array<ResourceName> wire = {COPPER_WIRE};
		array<ResourceName> wornTool = {WORN_TOOL};
		map<string, float> scrapPerFill = new map<string, float>();
		map<string, float> toolsPerFill = new map<string, float>();
		map<string, float> metalPerFill = new map<string, float>();
		map<string, float> rubberPerFill = new map<string, float>();
		map<string, float> radioPerFill = new map<string, float>();
		map<string, float> wirePerFill = new map<string, float>();
		map<string, float> wornToolPerFill = new map<string, float>();
		foreach (string kind : kinds)
		{
			string lookup = kind;
			if (RedProof())
				lookup = kind + "_missing";

			LF_LootTableDef table = LF_LootConfig.TableOf(lookup);
			ctx.NotNull(table, string.Format("%1 has a loot table", kind));
			if (!table)
				continue;

			ctx.EqualStr("", LF_LootRules.Validate(table), string.Format("%1 table is rollable", kind));
			foreach (LF_LootRowDef row : table.m_aRows)
			{
				Resource resource = Resource.Load(row.m_Prefab);
				ctx.True(resource && resource.IsValid(), string.Format("%1: %2 loads", kind, row.m_Prefab));
				ctx.True(manager.GetSellPrice(row.m_Prefab) > 0, string.Format("%1: %2 has a sell price", kind, row.m_Prefab));
			}

			float perFill = ValuePerFill(table, manager);
			float searches = CONTAINERS_IN_REACH * 3600;
			if (table.m_iRefillSeconds > 0)
				searches = searches / table.m_iRefillSeconds;

			if (searches > MAX_SEARCHES_PER_HOUR)
				searches = MAX_SEARCHES_PER_HOUR;

			float hourly = perFill * searches;
			ctx.True(perFill > 0, string.Format("%1 pays something (%2 a fill)", kind, perFill));
			ctx.True(hourly < parcelHourly * MAX_SHARE_OF_PARCEL, string.Format("%1: %2 searches an hour at %3 a fill is %4 an hour, under %5 percent of the parcel job's %6", kind, searches, perFill, hourly, MAX_SHARE_OF_PARCEL * 100, parcelHourly));
			scrapPerFill.Set(kind, CountPerFill(table, scrap));
			if (!RedProof())
				ctx.True(CountPerFill(table, noCopper) == 0, string.Format("%1 drops no copper nugget (no copper on the island)", kind));
			toolsPerFill.Set(kind, CountPerFill(table, tools));
			metalPerFill.Set(kind, CountPerFill(table, metal));
			rubberPerFill.Set(kind, CountPerFill(table, rubber));
			radioPerFill.Set(kind, CountPerFill(table, radio));
			wirePerFill.Set(kind, CountPerFill(table, wire));
			wornToolPerFill.Set(kind, CountPerFill(table, wornTool));
		}

		if (RedProof())
			return;

		ctx.True(scrapPerFill.Get("dump") > scrapPerFill.Get("shed"), "the dump holds more scrap than a shed");
		ctx.True(scrapPerFill.Get("dump") > scrapPerFill.Get("home"), "the dump holds more scrap than a home");
		ctx.True(scrapPerFill.Get("dump") > scrapPerFill.Get("backroom"), "the dump holds more scrap than a back room");
		ctx.True(toolsPerFill.Get("shed") > toolsPerFill.Get("home"), "a shed holds more tools than a home");
		ctx.True(toolsPerFill.Get("shed") > toolsPerFill.Get("dump"), "a shed holds more tools than the dump");

		// #3844: each scrap item drops where the card says; the dump holds most scrap, rubber and radios, sheds
		// the worn tools and wire, and a home an occasional radio.
		ctx.True(metalPerFill.Get("dump") > 0 && metalPerFill.Get("dump") > metalPerFill.Get("shed"), "scrap metal drops at the dump, more than in a shed");
		ctx.True(rubberPerFill.Get("dump") > 0, "rubber scrap drops at the dump");
		ctx.True(rubberPerFill.Get("dump") > rubberPerFill.Get("shed") && rubberPerFill.Get("dump") > rubberPerFill.Get("home"), "the dump holds the most rubber scrap");
		ctx.True(radioPerFill.Get("dump") > radioPerFill.Get("home"), "the dump holds more old radios than a home");
		ctx.True(radioPerFill.Get("home") > 0, "a home drops an old radio now and then");
		ctx.True(radioPerFill.Get("dump") > radioPerFill.Get("shed"), "the dump holds more old radios than a shed");
		ctx.True(wornToolPerFill.Get("shed") > 0, "a shed drops worn hand tools");
		ctx.True(wornToolPerFill.Get("shed") > wornToolPerFill.Get("dump") && wornToolPerFill.Get("shed") > wornToolPerFill.Get("home"), "a shed holds the most worn hand tools");
		ctx.True(wirePerFill.Get("shed") > 0, "a shed drops copper wire");
		ctx.True(wirePerFill.Get("shed") > wirePerFill.Get("home"), "a shed holds more copper wire than a home");
	}
}
