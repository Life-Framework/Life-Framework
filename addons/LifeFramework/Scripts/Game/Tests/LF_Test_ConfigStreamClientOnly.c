// red-proof: write `ctx.WriteValue("persistence", 777)` in LF_ConfigStream.BuildPayload and
// config/stream-client-only fails "the payload's top level is the header and the shops section only
// (expected 'stream,schemaVersion,owner,shops', got 'stream,schemaVersion,owner,persistence,shops')"
// and "a server-only section does not leave the server". Observed 2026-11-12.

//------------------------------------------------------------------------------------------------
//! The config stream carries only what a client reads (#285): the header and the shops section,
//! written from the typed fields. Another section of the owner file, an unknown key in the shops
//! section and anything else in config.json never leave the server.
// tier: LOGIC
class LF_Test_ConfigStreamClientOnly : LF_Test
{
	override string GetName()
	{
		return "config/stream-client-only";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_ConfigService.Reset();
		LF_ConfigStream.Reset();
		LF_ConfigService.LoadOwnerString("{\"schemaVersion\":1,\"persistence\":{\"saveIntervalSeconds\":777},\"adminSecret\":\"hunter2\","
			+ "\"shops\":{\"defaultBuyPrice\":61,\"ownerNote\":\"server-only-note\"}}", "test");
		LF_ShopPricesConfig config = LF_Test_ConfigMergeFixture.MakeCore();
		config.ApplyLayers(string.Empty);
		ctx.Equal(61, config.m_iDefaultBuyPrice, "the owner value applies on the server");

		string payload = LF_ConfigStream.BuildPayload(config, LF_ConfigService.HasOwnerLayer());
		SCR_JsonLoadContext doc = new SCR_JsonLoadContext(false);
		ctx.True(doc.ImportFromString("{\"doc\":" + payload + "}"), "the payload parses");

		array<string> keys = {};
		LF_ConfigReader.ListKeys(doc, "doc", keys);
		ctx.EqualStr("stream,schemaVersion,owner,shops", JoinKeys(keys), "the payload's top level is the header and the shops section only");

		doc.StartObject("doc");
		LF_ConfigReader.ListKeys(doc, LF_ShopPricesConfig.SECTION, keys);
		ctx.EqualStr("defaultBuyPrice,defaultSellPrice,priceRules,extraItemPrefabs", JoinKeys(keys), "the shops section holds the client-read fields only");

		ctx.True(payload.IndexOf("777") == -1, "a server-only section does not leave the server");
		ctx.True(payload.IndexOf("hunter2") == -1, "an unknown top-level key does not leave the server");
		ctx.True(payload.IndexOf("server-only-note") == -1, "an unknown key in shops does not leave the server");

		LF_ConfigService.Reset();
	}

	//------------------------------------------------------------------------------------------------
	protected string JoinKeys(array<string> keys)
	{
		string joined;
		foreach (int i, string key : keys)
		{
			if (i > 0)
				joined += ",";
			joined += key;
		}
		return joined;
	}
};
