// red-proof: perturb a boundary, then run the fast tier. For shop-prices/rule-match remove the
// type-0 wildcard (or the mode DEFAULT wildcard) in EL_ShopPriceResolver.RuleMatches and the
// wildcard assertions go red; for shop-prices/override make ResolvePrices stop at the first match
// and the later-overrides-earlier assertion goes red; for shop-prices/hidden make a hidden match
// continue the walk and the hidden-is-terminal assertion goes red. For the catalog test, skip the
// m_aExtraItemPrefabs merge in EL_ShopManagerComponent.BuildExtraItems and catalog/extra-items goes
// red; drop the currency hidden rule from Configs/Shop/ShopPrices.conf and catalog/hidden goes red.

// tier: LOGIC
class EL_Test_ShopPriceRuleMatch : EL_Test
{
	override string GetName()
	{
		return "shop-prices/rule-match";
	}

	override void Run(EL_TestContext ctx)
	{
		ref EL_ShopPriceRule byName = new EL_ShopPriceRule();
		byName.m_sFind = "Apple";
		byName.m_iBuyPrice = 10;
		byName.m_iSellPrice = 5;
		ctx.True(EL_ShopPriceResolver.RuleMatches(byName, "Prefabs/Items/Food/Apple.et", SCR_EArsenalItemType.RIFLE, SCR_EArsenalItemMode.DEFAULT), "name filter wins over type/mode");
		ctx.False(EL_ShopPriceResolver.RuleMatches(byName, "Prefabs/Items/Food/Plum.et", SCR_EArsenalItemType.RIFLE, SCR_EArsenalItemMode.DEFAULT), "name filter rejects other prefabs");

		ref EL_ShopPriceRule byType = new EL_ShopPriceRule();
		byType.m_eItemType = SCR_EArsenalItemType.RIFLE;
		byType.m_eItemMode = SCR_EArsenalItemMode.DEFAULT;
		ctx.True(EL_ShopPriceResolver.RuleMatches(byType, "Prefabs/Weapons/M16A2.et", SCR_EArsenalItemType.RIFLE, SCR_EArsenalItemMode.AMMUNITION), "DEFAULT mode matches any mode");
		ctx.False(EL_ShopPriceResolver.RuleMatches(byType, "Prefabs/Items/Food/Apple.et", SCR_EArsenalItemType.HEAL, SCR_EArsenalItemMode.CONSUMABLE), "non-matching type is rejected");

		ref EL_ShopPriceRule byTypeAndMode = new EL_ShopPriceRule();
		byTypeAndMode.m_eItemType = SCR_EArsenalItemType.HEAL;
		byTypeAndMode.m_eItemMode = SCR_EArsenalItemMode.CONSUMABLE;
		ctx.True(EL_ShopPriceResolver.RuleMatches(byTypeAndMode, "Prefabs/Items/Medical/Bandage.et", SCR_EArsenalItemType.HEAL, SCR_EArsenalItemMode.CONSUMABLE), "exact type and mode match");
		ctx.False(EL_ShopPriceResolver.RuleMatches(byTypeAndMode, "Prefabs/Weapons/M16A2.et", SCR_EArsenalItemType.RIFLE, SCR_EArsenalItemMode.AMMUNITION), "type and mode both required");

		ref EL_ShopPriceRule anyType = new EL_ShopPriceRule();
		anyType.m_eItemType = 0;
		anyType.m_eItemMode = SCR_EArsenalItemMode.DEFAULT;
		ctx.True(EL_ShopPriceResolver.RuleMatches(anyType, "Prefabs/Items/Food/Apple.et", SCR_EArsenalItemType.PISTOL, SCR_EArsenalItemMode.AMMUNITION), "type 0 matches any type");
	}
};

//------------------------------------------------------------------------------------------------
// tier: LOGIC
class EL_Test_ShopPriceResolve : EL_Test
{
	override string GetName()
	{
		return "shop-prices/resolve";
	}

	override void Run(EL_TestContext ctx)
	{
		int buyPrice;
		int sellPrice;
		bool hidden;

		EL_ShopPriceResolver.ResolvePrices(null, "Prefabs/Items/Food/Apple.et", SCR_EArsenalItemType.HEAL, SCR_EArsenalItemMode.CONSUMABLE, 50, 25, buyPrice, sellPrice, hidden);
		ctx.Equal(50, buyPrice, "no rules uses the default buy price");
		ctx.Equal(25, sellPrice, "no rules uses the default sell price");
		ctx.False(hidden, "no rules never hides");

		ref array<ref EL_ShopPriceRule> rules = new array<ref EL_ShopPriceRule>();

		ref EL_ShopPriceRule food = new EL_ShopPriceRule();
		food.m_sFind = "Prefabs/Items/Food/";
		food.m_iBuyPrice = 8;
		food.m_iSellPrice = 4;
		rules.Insert(food);

		ref EL_ShopPriceRule apple = new EL_ShopPriceRule();
		apple.m_sFind = "Apple.et";
		apple.m_iBuyPrice = 12;
		apple.m_iSellPrice = 6;
		rules.Insert(apple);

		EL_ShopPriceResolver.ResolvePrices(rules, "Prefabs/Items/Food/Apple.et", SCR_EArsenalItemType.HEAL, SCR_EArsenalItemMode.CONSUMABLE, 50, 25, buyPrice, sellPrice, hidden);
		ctx.Equal(12, buyPrice, "specific rule overrides the category rule");
		ctx.Equal(6, sellPrice, "sell price follows the same rule");
		ctx.False(hidden, "matched rules do not hide");

		EL_ShopPriceResolver.ResolvePrices(rules, "Prefabs/Items/Food/Plum.et", SCR_EArsenalItemType.HEAL, SCR_EArsenalItemMode.CONSUMABLE, 50, 25, buyPrice, sellPrice, hidden);
		ctx.Equal(8, buyPrice, "category rule still prices other food");

		EL_ShopPriceResolver.ResolvePrices(rules, "Prefabs/Tools/Axe.et", SCR_EArsenalItemType.EQUIPMENT, SCR_EArsenalItemMode.DEFAULT, 50, 25, buyPrice, sellPrice, hidden);
		ctx.Equal(50, buyPrice, "unmatched item falls back to the default buy price");
		ctx.Equal(25, sellPrice, "unmatched item falls back to the default sell price");
	}
};

//------------------------------------------------------------------------------------------------
// tier: LOGIC
class EL_Test_ShopPriceHidden : EL_Test
{
	override string GetName()
	{
		return "shop-prices/hidden";
	}

	override void Run(EL_TestContext ctx)
	{
		int buyPrice;
		int sellPrice;
		bool hidden;

		ref array<ref EL_ShopPriceRule> rules = new array<ref EL_ShopPriceRule>();

		ref EL_ShopPriceRule hiddenRule = new EL_ShopPriceRule();
		hiddenRule.m_sFind = "Prefabs/Items/Currencies/";
		hiddenRule.m_bHidden = true;
		rules.Insert(hiddenRule);

		ref EL_ShopPriceRule later = new EL_ShopPriceRule();
		later.m_sFind = "MoneyStack";
		later.m_iBuyPrice = 1;
		later.m_iSellPrice = 1;
		rules.Insert(later);

		EL_ShopPriceResolver.ResolvePrices(rules, "Prefabs/Items/Currencies/MoneyStack.et", SCR_EArsenalItemType.EQUIPMENT, SCR_EArsenalItemMode.DEFAULT, 50, 25, buyPrice, sellPrice, hidden);
		ctx.True(hidden, "a hidden match hides the item");
		ctx.Equal(50, buyPrice, "hidden leaves the default buy price in place");
		ctx.Equal(25, sellPrice, "hidden leaves the default sell price in place");
	}
};

//------------------------------------------------------------------------------------------------
// tier: WORLD
class EL_Test_ShopCatalog : EL_Test
{
	override string GetName()
	{
		return "shop-prices/catalog";
	}

	override void Run(EL_TestContext ctx)
	{
		EL_ShopManagerComponent manager = EL_ShopManagerComponent.GetInstance();
		ctx.True(manager != null, "shop manager present on the game mode");
		if (!manager)
			return;

		array<ref EL_ShopCatalogItem> catalog = manager.GetCatalogItems();
		ctx.True(catalog.Count() > 0, "catalog has priced items");

		ResourceName apple = "{C9D661E5B0714711}Prefabs/Items/Food/Apple.et";
		ctx.Equal(8, manager.GetBuyPrice(apple), "apple buy price comes from the food rule");
		ctx.Equal(4, manager.GetSellPrice(apple), "apple sell price comes from the food rule");

		ResourceName moneyStack = "{5439738849229352}Prefabs/Items/Currencies/MoneyStack.et";
		ctx.True(manager.IsHidden(moneyStack), "money is not sold in any shop");

		ResourceName m9 = "{1353C6EAD1DCFE43}Prefabs/Weapons/Handguns/M9/Handgun_M9.et";
		ctx.Equal(500, manager.GetBuyPrice(m9), "M9 buy price overrides the broad weapon rule");
		ctx.Equal(250, manager.GetSellPrice(m9), "M9 sell price overrides the broad weapon rule");
	}
};