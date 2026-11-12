// red-proof: in LF_ShopPriceResolver.RuleKey make the prefab branch return "type:0,mode:2" and shop/rule-key fails
// "a prefab rule is keyed by its GUID (expected 'prefab:C9D661E5B0714711', got 'type:0,mode:2')". Observed 2026-11-12.
//------------------------------------------------------------------------------------------------
//! A prefab rule is addressed by "prefab:<GUID>", not by the type key every prefab rule shares:
//! two prefab rules get distinct keys, and an owner record for one prefab changes only that rule.
// tier: LOGIC
class LF_Test_ShopRuleKey : LF_Test
{
	static const string PREFAB_A = "{C9D661E5B0714711}Prefabs/Items/Food/Apple.et";
	static const string PREFAB_B = "{C3E35D690317B5BE}Prefabs/Items/Food/Plum.et";

	override string GetName()
	{
		return "shop/rule-key";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_ShopPricesConfig core = LF_Test_ConfigMergeFixture.MakeCore();
		LF_ShopPriceRule a = PrefabRule(PREFAB_A, 10, 5);
		LF_ShopPriceRule b = PrefabRule(PREFAB_B, 20, 10);
		core.m_aPriceRules.Insert(a);
		core.m_aPriceRules.Insert(b);

		string keyA = LF_ShopPriceResolver.RuleKey(a.m_Prefab, a.m_sFind, a.m_eItemType, a.m_eItemMode);
		string keyB = LF_ShopPriceResolver.RuleKey(b.m_Prefab, b.m_sFind, b.m_eItemType, b.m_eItemMode);
		ctx.EqualStr("prefab:C9D661E5B0714711", keyA, "a prefab rule is keyed by its GUID");
		ctx.True(keyA != keyB, "two prefab rules have distinct keys");
		ctx.True(LF_ShopPriceResolver.FindRule(core.m_aPriceRules, keyA) == a, "the key finds its own rule");

		LF_ConfigService.Reset();
		LF_ConfigService.LoadOwnerString("{\"shops\":{\"priceRules\":{\"" + keyB + "\":{\"buyPrice\":7}}}}", "test");
		core.ApplyLayers(string.Empty);

		ctx.Equal(7, b.m_iBuyPrice, "the owner record changes the rule it names");
		ctx.Equal(10, a.m_iBuyPrice, "the other prefab rule is untouched");
		ctx.Equal(5, core.m_aPriceRules.Count(), "no rule was added");

		LF_ConfigService.Reset();
	}

	//------------------------------------------------------------------------------------------------
	protected LF_ShopPriceRule PrefabRule(string prefab, int buy, int sell)
	{
		LF_ShopPriceRule rule = LF_Test_ConfigMergeFixture.Rule(string.Empty, buy, sell);
		rule.m_Prefab = prefab;
		return rule;
	}
};
