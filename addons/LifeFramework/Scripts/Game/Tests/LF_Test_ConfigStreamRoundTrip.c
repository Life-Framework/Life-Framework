// red-proof: drop `rule.m_Prefab = prefab;` from LF_ShopPricesConfig.ApplyStream and
// config/stream-roundtrip fails "rule 3 prefab (expected '{BCD1E8CD7F975074}Prefabs/Resources/
// Mining/IronBar.et', got '')" and "the client prices an apple at the owner's 13, not the shipped 8
// (expected 13, got 80)": the prefab rule turns into a match-all rule. Observed 2026-11-12.

//------------------------------------------------------------------------------------------------
//! The config stream round trip (#285): the server's effective shop values, serialised, received
//! and applied on a client that has only the shipped values, give the client the same values in
//! the same order (every rule field, the defaults and the extra prefabs), so the client's resolver
//! prices an apple at the owner's price. With no owner file, applying the stream changes nothing.
// tier: LOGIC
class LF_Test_ConfigStreamRoundTrip : LF_Test
{
	static const string IRON_BAR = "{BCD1E8CD7F975074}Prefabs/Resources/Mining/IronBar.et";

	override string GetName()
	{
		return "config/stream-roundtrip";
	}

	//------------------------------------------------------------------------------------------------
	//! The server: core plus a prefab rule and a type rule, then the owner file.
	protected LF_ShopPricesConfig MakeServer(string ownerJson)
	{
		LF_ConfigService.Reset();
		LF_ConfigStream.Reset();
		LF_ConfigService.LoadOwnerString(ownerJson, "test-server");
		LF_ShopPricesConfig config = ShippedCore();
		config.ApplyLayers(string.Empty);
		return config;
	}

	//------------------------------------------------------------------------------------------------
	//! The shipped values both sides start from.
	protected LF_ShopPricesConfig ShippedCore()
	{
		LF_ShopPricesConfig config = LF_Test_ConfigMergeFixture.MakeCore();
		LF_ShopPriceRule bar = LF_ShopPriceRule.Create(string.Empty, 0, SCR_EArsenalItemMode.DEFAULT);
		bar.m_Prefab = IRON_BAR;
		bar.m_iBuyPrice = 80;
		bar.m_iSellPrice = 45;
		config.m_aPriceRules.Insert(bar);
		LF_ShopPriceRule typed = LF_ShopPriceRule.Create(string.Empty, 64, 1);
		typed.m_bHidden = true;
		config.m_aPriceRules.Insert(typed);
		return config;
	}

	//------------------------------------------------------------------------------------------------
	//! A client: no owner layer, the stream received, the shipped core applied.
	protected LF_ShopPricesConfig MakeClient(LF_TestContext ctx, string payload, string label)
	{
		LF_ConfigService.Reset();
		LF_ConfigStream.Reset();
		LF_ConfigService.LoadOwnerString("{\"schemaVersion\":1}", "test-client");
		ctx.Equal(LF_EConfigStreamStatus.APPLIED, LF_ConfigStream.Receive(LF_ConfigStream.FORMAT_VERSION, payload.Hash(), payload), label + ": the client accepts the stream");
		LF_ShopPricesConfig config = ShippedCore();
		config.ApplyLayers(string.Empty);
		return config;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		string owner = "{\"schemaVersion\":1,\"shops\":{\"defaultSellPrice\":30,\"priceRules\":{"
			+ "\"Prefabs/Items/Food/\":{\"buyPrice\":13},"
			+ "\"Handgun_M9\":{\"sellPrice\":260},"
			+ "\"Prefabs/Items/LF_New/\":{\"buyPrice\":5,\"hidden\":true}"
			+ "},\"extraItemPrefabs\":[\"" + LF_Test_ConfigMergeFixture.PLUM + "\"]}}";
		LF_ShopPricesConfig server = MakeServer(owner);
		string payload = LF_ConfigStream.BuildPayload(server, true);
		ctx.True(payload.Length() > 0, "the server writes a payload");

		LF_ShopPricesConfig client = MakeClient(ctx, payload, "owner prices");
		ctx.Equal(server.m_iDefaultBuyPrice, client.m_iDefaultBuyPrice, "default buy price");
		ctx.Equal(30, client.m_iDefaultSellPrice, "the owner's default sell price reaches the client");
		ctx.Equal(server.m_aPriceRules.Count(), client.m_aPriceRules.Count(), "the client has every server rule");
		if (server.m_aPriceRules.Count() == client.m_aPriceRules.Count())
		{
			foreach (int i, LF_ShopPriceRule want : server.m_aPriceRules)
			{
				LF_ShopPriceRule got = client.m_aPriceRules[i];
				string at = string.Format("rule %1", i);
				ctx.EqualStr(want.m_Prefab, got.m_Prefab, at + " prefab");
				ctx.EqualStr(want.m_sFind, got.m_sFind, at + " find");
				ctx.Equal(want.m_eItemType, got.m_eItemType, at + " type");
				ctx.Equal(want.m_eItemMode, got.m_eItemMode, at + " mode");
				ctx.Equal(want.m_iBuyPrice, got.m_iBuyPrice, at + " buy price");
				ctx.Equal(want.m_iSellPrice, got.m_iSellPrice, at + " sell price");
				ctx.True(want.m_bHidden == got.m_bHidden, at + " hidden");
			}
		}
		ctx.Equal(2, client.m_aExtraItemPrefabs.Count(), "the owner's extra prefab reaches the client");
		ctx.EqualStr(payload, LF_ConfigStream.BuildPayload(client, true), "the client's values serialise to the server's payload");

		int buy;
		int sell;
		bool hidden;
		LF_ShopPriceResolver.ResolvePrices(client.m_aPriceRules, LF_Test_ConfigMergeFixture.APPLE, 0, SCR_EArsenalItemMode.DEFAULT, client.m_iDefaultBuyPrice, client.m_iDefaultSellPrice, buy, sell, hidden);
		ctx.Equal(13, buy, "the client prices an apple at the owner's 13, not the shipped 8");
		LF_ShopPriceResolver.ResolvePrices(client.m_aPriceRules, IRON_BAR, 0, SCR_EArsenalItemMode.DEFAULT, client.m_iDefaultBuyPrice, client.m_iDefaultSellPrice, buy, sell, hidden);
		ctx.Equal(80, buy, "a prefab rule survives the stream");

		ctx.Equal(LF_EConfigStreamStatus.UNCHANGED, LF_ConfigStream.Receive(LF_ConfigStream.FORMAT_VERSION, payload.Hash(), payload), "the same stream twice is unchanged");

		// No owner file: the server streams the shipped values and the client's values do not move.
		LF_ShopPricesConfig shipped = MakeServer("{\"schemaVersion\":1}");
		string shippedPayload = LF_ConfigStream.BuildPayload(shipped, false);
		LF_ShopPricesConfig plain = MakeClient(ctx, shippedPayload, "no owner file");
		ctx.EqualStr(LF_ConfigStream.BuildPayload(ShippedCore(), false), LF_ConfigStream.BuildPayload(plain, false), "with no owner file the stream changes nothing on the client");
		LF_ShopPriceResolver.ResolvePrices(plain.m_aPriceRules, LF_Test_ConfigMergeFixture.APPLE, 0, SCR_EArsenalItemMode.DEFAULT, plain.m_iDefaultBuyPrice, plain.m_iDefaultSellPrice, buy, sell, hidden);
		ctx.Equal(8, buy, "with no owner file an apple stays at the shipped 8");

		LF_ConfigStream.Reset();
		LF_ConfigService.Reset();
	}
};
