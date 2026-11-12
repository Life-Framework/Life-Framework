// red-proof: in LF_ShopPricesConfig.ApplyOwner replace the FindRule lookup with null and
// config/merge-fields fails "food sell price the owner left out stays shipped (expected 4, got 50)"
// and config/merge-keyed fails "two new keys are added, two existing keys are not (expected 5,
// got 7)". In LF_ConfigReader.BeginRecord check only for ABSENT instead of OBJECT and
// config/merge-reject ends in "ENGINE (F): Crashed" on "Prefabs/Weapons/": 5; make KindOf report
// lists as OBJECT and it fails "no kind but an object becomes a rule (expected 3, got 4)".
// In LF_ShopPricesConfig.ApplyOwner drop the ReadString("prefab") line and config/merge-prefab fails
// "the owner prefab is set (expected {C9D661E5B0714711}..., got )". All observed 2026-11-12.

//------------------------------------------------------------------------------------------------
//! Shared fixture: a core shops config built in script, shaped like Configs/Shop/ShopPrices.conf.
class LF_Test_ConfigMergeFixture
{
	static const string FOOD = "Prefabs/Items/Food/";
	static const string WEAPONS = "Prefabs/Weapons/";
	static const string M9 = "Handgun_M9";
	static const string APPLE = "{C9D661E5B0714711}Prefabs/Items/Food/Apple.et";
	static const string APPLE_MOVED = "{C9D661E5B0714711}";
	static const string PLUM = "{C3E35D690317B5BE}Prefabs/Items/Food/Plum.et";

	//------------------------------------------------------------------------------------------------
	static LF_ShopPricesConfig MakeCore()
	{
		LF_ShopPricesConfig config = new LF_ShopPricesConfig();
		config.m_iDefaultBuyPrice = 50;
		config.m_iDefaultSellPrice = 25;
		config.m_aPriceRules = new array<ref LF_ShopPriceRule>();
		config.m_aPriceRules.Insert(Rule(FOOD, 8, 4));
		config.m_aPriceRules.Insert(Rule(WEAPONS, 800, 400));
		config.m_aPriceRules.Insert(Rule(M9, 500, 250));
		config.m_aExtraItemPrefabs = new array<ResourceName>();
		config.m_aExtraItemPrefabs.Insert(APPLE);
		return config;
	}

	//------------------------------------------------------------------------------------------------
	static LF_ShopPriceRule Rule(string find, int buy, int sell)
	{
		LF_ShopPriceRule rule = LF_ShopPriceRule.Create(find, 0, SCR_EArsenalItemMode.DEFAULT);
		rule.m_iBuyPrice = buy;
		rule.m_iSellPrice = sell;
		return rule;
	}

	//------------------------------------------------------------------------------------------------
	//! Installs an owner document and applies it to a fresh core config.
	static LF_ShopPricesConfig Merge(string ownerJson)
	{
		LF_ConfigService.Reset();
		LF_ConfigService.LoadOwnerString(ownerJson, "test");
		LF_ShopPricesConfig config = MakeCore();
		config.ApplyLayers(string.Empty);
		return config;
	}

	//------------------------------------------------------------------------------------------------
	static LF_ShopPriceRule Find(LF_ShopPricesConfig config, string find)
	{
		return LF_ShopPriceResolver.FindRule(config.m_aPriceRules, LF_ShopPriceResolver.RuleKey(find, 0, SCR_EArsenalItemMode.DEFAULT));
	}
};

//------------------------------------------------------------------------------------------------
//! An owner field overrides only itself; every field the owner leaves out keeps the shipped value.
// tier: LOGIC
class LF_Test_ConfigMergeFields : LF_Test
{
	override string GetName()
	{
		return "config/merge-fields";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_ShopPricesConfig config = LF_Test_ConfigMergeFixture.Merge("{\"schemaVersion\":1,\"shops\":{\"defaultBuyPrice\":60,\"priceRules\":{\"Prefabs/Items/Food/\":{\"buyPrice\":10}}}}");
		ctx.Equal(1, LF_ConfigService.GetOwnerSchemaVersion(), "schemaVersion is read");
		ctx.Equal(60, config.m_iDefaultBuyPrice, "owner default buy price wins");
		ctx.Equal(25, config.m_iDefaultSellPrice, "default sell price the owner left out stays shipped");

		LF_ShopPriceRule food = LF_Test_ConfigMergeFixture.Find(config, LF_Test_ConfigMergeFixture.FOOD);
		ctx.NotNull(food, "food rule is still there");
		if (food)
		{
			ctx.Equal(10, food.m_iBuyPrice, "owner food buy price wins");
			ctx.Equal(4, food.m_iSellPrice, "food sell price the owner left out stays shipped");
		}

		ctx.Equal(3, config.m_aPriceRules.Count(), "overriding a rule adds none");
		LF_ShopPriceRule weapons = LF_Test_ConfigMergeFixture.Find(config, LF_Test_ConfigMergeFixture.WEAPONS);
		ctx.True(weapons && weapons.m_iBuyPrice == 800, "rules the owner left out stay shipped");

		int buy;
		int sell;
		bool hidden;
		LF_ShopPriceResolver.ResolvePrices(config.m_aPriceRules, "Prefabs/Items/Food/Apple.et", 0, SCR_EArsenalItemMode.DEFAULT, config.m_iDefaultBuyPrice, config.m_iDefaultSellPrice, buy, sell, hidden);
		ctx.Equal(10, buy, "the shop resolves the owner price");
		ctx.Equal(4, sell, "the shop resolves the shipped sell price");
		LF_ShopPriceResolver.ResolvePrices(config.m_aPriceRules, "Prefabs/Props/Crate.et", 0, SCR_EArsenalItemMode.DEFAULT, config.m_iDefaultBuyPrice, config.m_iDefaultSellPrice, buy, sell, hidden);
		ctx.Equal(60, buy, "an unruled item takes the owner default");

		LF_ConfigService.Reset();
	}
};

//------------------------------------------------------------------------------------------------
//! Rules merge by key, never by list position: the owner may list them in any order, an existing
//! key keeps its place, a new key is appended (so it wins the walk), extra prefabs are added.
// tier: LOGIC
class LF_Test_ConfigMergeKeyed : LF_Test
{
	override string GetName()
	{
		return "config/merge-keyed";
	}

	override void Run(LF_TestContext ctx)
	{
		string json = "{\"shops\":{\"priceRules\":{"
			+ "\"Handgun_M9\":{\"sellPrice\":260},"
			+ "\"Prefabs/Items/Food/\":{\"hidden\":true},"
			+ "\"Prefabs/Items/LF_New/\":{\"buyPrice\":5},"
			+ "\"type:64\":{\"buyPrice\":3}"
			+ "},\"extraItemPrefabs\":[\"" + LF_Test_ConfigMergeFixture.APPLE + "\",\"" + LF_Test_ConfigMergeFixture.PLUM + "\"]}}";
		LF_ShopPricesConfig config = LF_Test_ConfigMergeFixture.Merge(json);

		ctx.Equal(5, config.m_aPriceRules.Count(), "two new keys are added, two existing keys are not");
		if (config.m_aPriceRules.Count() != 5)
			return;

		ctx.EqualStr(LF_Test_ConfigMergeFixture.FOOD, config.m_aPriceRules[0].m_sFind, "an existing key keeps its position");
		ctx.EqualStr(LF_Test_ConfigMergeFixture.M9, config.m_aPriceRules[2].m_sFind, "the owner's order does not move shipped rules");
		ctx.Equal(500, config.m_aPriceRules[2].m_iBuyPrice, "M9 buy price the owner left out stays shipped");
		ctx.Equal(260, config.m_aPriceRules[2].m_iSellPrice, "M9 sell price is the owner's");
		ctx.True(config.m_aPriceRules[0].m_bHidden, "the owner can hide a shipped rule");

		LF_ShopPriceRule added = config.m_aPriceRules[3];
		ctx.EqualStr("Prefabs/Items/LF_New/", added.m_sFind, "a new key is appended after the shipped rules");
		ctx.Equal(5, added.m_iBuyPrice, "the new rule has the owner's buy price");
		ctx.Equal(50, added.m_iSellPrice, "a new rule's missing sell price is the .conf attribute default");

		LF_ShopPriceRule byType = config.m_aPriceRules[4];
		ctx.EqualStr("type:64,mode:2", LF_ShopPriceResolver.RuleKey(byType.m_sFind, byType.m_eItemType, byType.m_eItemMode), "a rule without find is keyed by type and mode");
		ctx.Equal(3, byType.m_iBuyPrice, "the type rule has the owner's buy price");

		int buy;
		int sell;
		bool hidden;
		LF_ShopPriceResolver.ResolvePrices(config.m_aPriceRules, "Prefabs/Weapons/Handguns/M9/Handgun_M9.et", 0, SCR_EArsenalItemMode.DEFAULT, config.m_iDefaultBuyPrice, config.m_iDefaultSellPrice, buy, sell, hidden);
		ctx.Equal(500, buy, "M9 still resolves its shipped buy price");
		ctx.Equal(260, sell, "M9 resolves the owner's sell price");
		LF_ShopPriceResolver.ResolvePrices(config.m_aPriceRules, "Prefabs/Items/Food/Apple.et", 0, SCR_EArsenalItemMode.DEFAULT, config.m_iDefaultBuyPrice, config.m_iDefaultSellPrice, buy, sell, hidden);
		ctx.True(hidden, "food is hidden by the owner");

		ctx.Equal(2, config.m_aExtraItemPrefabs.Count(), "extra prefabs are added once each");
		ctx.True(config.m_aExtraItemPrefabs.Contains(LF_Test_ConfigMergeFixture.PLUM), "the owner's extra prefab is listed");

		LF_ConfigService.Reset();
	}
};

//------------------------------------------------------------------------------------------------
//! A field of the wrong type is refused and logged, the lower layer's value stays, and the rest of
//! the section still applies. A record that is not an object, or whose key does not parse, is
//! skipped whole. A number where a record belongs used to be the engine-crash case (StartObject on
//! a number), so this test also proves the reader checks the kind first.
// tier: LOGIC
class LF_Test_ConfigMergeReject : LF_Test
{
	override string GetName()
	{
		return "config/merge-reject";
	}

	override void Run(LF_TestContext ctx)
	{
		string json = "{\"shops\":{\"defaultBuyPrice\":\"cheap\",\"defaultSellPrice\":30,\"priceRules\":{"
			+ "\"Prefabs/Weapons/\":5,"
			+ "\"type:rifles\":{\"buyPrice\":1},"
			+ "\"Prefabs/Items/Food/\":{\"buyPrice\":\"x\",\"sellPrice\":6}"
			+ "}}}";
		LF_ConfigService.Reset();
		LF_ConfigService.LoadOwnerString(json, "test");
		LF_ShopPricesConfig config = LF_Test_ConfigMergeFixture.MakeCore();
		LF_ConfigReader reader = LF_ConfigService.OpenOwnerSection(LF_ShopPricesConfig.SECTION);
		ctx.NotNull(reader, "the section opens");
		if (!reader)
			return;

		config.ApplyOwner(reader);
		ctx.Equal(50, config.m_iDefaultBuyPrice, "a wrong-type default keeps the shipped value");
		ctx.Equal(30, config.m_iDefaultSellPrice, "the next field still applies");
		ctx.Equal(3, config.m_aPriceRules.Count(), "a record with an unparseable key adds nothing");
		LF_ShopPriceRule weapons = LF_Test_ConfigMergeFixture.Find(config, LF_Test_ConfigMergeFixture.WEAPONS);
		ctx.True(weapons && weapons.m_iBuyPrice == 800, "a number in place of a record changes no rule");
		LF_ShopPriceRule food = LF_Test_ConfigMergeFixture.Find(config, LF_Test_ConfigMergeFixture.FOOD);
		ctx.NotNull(food, "the food rule is still there");
		if (food)
		{
			ctx.Equal(8, food.m_iBuyPrice, "a wrong-type field in a record keeps the shipped value");
			ctx.Equal(6, food.m_iSellPrice, "the rest of that record still applies");
		}
		ctx.Equal(4, reader.GetRejectedCount(), "each refused field and record is counted");

		// Every kind in place of a record, and in place of the section: none crashes, none applies.
		LF_ShopPricesConfig kinds = LF_Test_ConfigMergeFixture.Merge("{\"shops\":{\"priceRules\":{\"a\":1.5,\"b\":\"x\",\"c\":true,\"d\":null,\"e\":[1]},\"defaultBuyPrice\":61}}");
		ctx.Equal(3, kinds.m_aPriceRules.Count(), "no kind but an object becomes a rule");
		ctx.Equal(61, kinds.m_iDefaultBuyPrice, "the section still applies around bad records");
		kinds = LF_Test_ConfigMergeFixture.Merge("{\"shops\":[1]}");
		ctx.Equal(50, kinds.m_iDefaultBuyPrice, "a list in place of the section changes nothing");
		kinds = LF_Test_ConfigMergeFixture.Merge("{\"shops\":{\"extraItemPrefabs\":null}}");
		ctx.Equal(1, kinds.m_aExtraItemPrefabs.Count(), "null in place of a string list keeps the shipped list");

		LF_ShopPricesConfig notList = LF_Test_ConfigMergeFixture.Merge("{\"shops\":{\"priceRules\":[{\"find\":\"x\"}],\"defaultSellPrice\":31}}");
		ctx.Equal(3, notList.m_aPriceRules.Count(), "priceRules that is a list, not an object, leaves the rules alone");
		ctx.Equal(31, notList.m_iDefaultSellPrice, "the section still applies around it");

		LF_ConfigService.Reset();
	}
};

//------------------------------------------------------------------------------------------------
//! Without a usable owner layer every value stays shipped: no file, a malformed file, no section,
//! or a section that is not an object.
// tier: LOGIC
class LF_Test_ConfigMergeNoOwner : LF_Test
{
	override string GetName()
	{
		return "config/merge-no-owner";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_ConfigService.Reset();
		ctx.False(LF_ConfigService.LoadOwnerString("{\"shops\":{\"defaultBuyPrice\":60,", "test"), "a malformed document is refused");
		ctx.False(LF_ConfigService.HasOwnerLayer(), "a malformed document leaves no owner layer");
		LF_ShopPricesConfig config = LF_Test_ConfigMergeFixture.MakeCore();
		config.ApplyLayers(string.Empty);
		ctx.Equal(50, config.m_iDefaultBuyPrice, "a malformed document changes nothing");

		LF_ConfigService.LoadOwnerString("{\"schemaVersion\":1,\"jobs\":{}}", "test");
		ctx.True(LF_ConfigService.HasOwnerLayer(), "a document without shops is an owner layer");
		ctx.True(LF_ConfigService.OpenOwnerSection(LF_ShopPricesConfig.SECTION) == null, "a missing section opens nothing");

		config = LF_Test_ConfigMergeFixture.Merge("{\"shops\":5}");
		ctx.Equal(50, config.m_iDefaultBuyPrice, "a section that is not an object changes nothing");
		ctx.Equal(3, config.m_aPriceRules.Count(), "a section that is not an object adds no rules");

		LF_ConfigService.Reset();
	}
};

//------------------------------------------------------------------------------------------------
//! An owner rule can set `prefab`: the rule then matches that prefab by GUID, whatever path either
//! name carries, and merges field by field (the prefab on an existing rule leaves its prices alone).
// tier: LOGIC
class LF_Test_ConfigMergePrefab : LF_Test
{
	override string GetName()
	{
		return "config/merge-prefab";
	}

	override void Run(LF_TestContext ctx)
	{
		string apple = LF_Test_ConfigMergeFixture.APPLE;
		string json = "{\"shops\":{\"priceRules\":{"
			+ "\"Apple\":{\"prefab\":\"" + apple + "\",\"buyPrice\":3},"
			+ "\"Handgun_M9\":{\"prefab\":\"{AAAAAAAAAAAAAAAA}Prefabs/Weapons/M9.et\"},"
			+ "\"Prefabs/Weapons/\":{\"prefab\":5}"
			+ "}}}";
		LF_ShopPricesConfig config = LF_Test_ConfigMergeFixture.Merge(json);
		ctx.Equal(4, config.m_aPriceRules.Count(), "one new key is added");
		if (config.m_aPriceRules.Count() != 4)
			return;

		LF_ShopPriceRule added = config.m_aPriceRules[3];
		ctx.EqualStr(apple, added.m_Prefab, "the owner prefab is set");
		ctx.Equal(3, added.m_iBuyPrice, "the new rule keeps the owner's price");

		LF_ShopPriceRule m9 = config.m_aPriceRules[2];
		ctx.EqualStr("{AAAAAAAAAAAAAAAA}Prefabs/Weapons/M9.et", m9.m_Prefab, "an existing rule takes the prefab");
		ctx.Equal(500, m9.m_iBuyPrice, "the prices it left out stay shipped");
		ctx.True(config.m_aPriceRules[1].m_Prefab.IsEmpty(), "a wrong-type prefab changes nothing");

		int buy;
		int sell;
		bool hidden;
		LF_ShopPriceResolver.ResolvePrices(config.m_aPriceRules, LF_Test_ConfigMergeFixture.APPLE_MOVED, 0, SCR_EArsenalItemMode.DEFAULT, config.m_iDefaultBuyPrice, config.m_iDefaultSellPrice, buy, sell, hidden);
		ctx.Equal(3, buy, "the prefab rule matches by GUID after the prefab moved");

		LF_ConfigService.Reset();
	}
};
