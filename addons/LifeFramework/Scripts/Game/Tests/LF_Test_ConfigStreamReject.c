// red-proof: skip the hash check in LF_ConfigStream.Receive and config/stream-reject fails "a
// payload that does not match its hash is refused (expected 3, got 0)"; skip the rejected-count
// check in LF_ShopPricesConfig.ApplyStream and it fails "a bad field anywhere keeps every shipped
// shop value (expected 50, got 61)". Observed 2026-11-12.

//------------------------------------------------------------------------------------------------
//! A config stream the client cannot trust fails loudly and changes nothing (#285): another
//! format, another schema, a payload that does not match its hash, text that is not JSON, and a
//! shop value of the wrong type each log LF_Debug.Error and leave the shipped prices in place.
// tier: LOGIC
class LF_Test_ConfigStreamReject : LF_Test
{
	override string GetName()
	{
		return "config/stream-reject";
	}

	//------------------------------------------------------------------------------------------------
	//! A client with no owner layer: the shipped core with whatever stream was accepted.
	protected LF_ShopPricesConfig ClientShops()
	{
		LF_ShopPricesConfig config = LF_Test_ConfigMergeFixture.MakeCore();
		config.ApplyLayers(string.Empty);
		return config;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_ConfigService.Reset();
		LF_ConfigStream.Reset();
		LF_ConfigService.LoadOwnerString("{\"schemaVersion\":1,\"shops\":{\"defaultBuyPrice\":61}}", "test-server");
		LF_ShopPricesConfig server = LF_Test_ConfigMergeFixture.MakeCore();
		server.ApplyLayers(string.Empty);
		string payload = LF_ConfigStream.BuildPayload(server, true);
		int hash = payload.Hash();

		LF_ConfigService.Reset();
		LF_ConfigService.LoadOwnerString("{\"schemaVersion\":1}", "test-client");

		ctx.Equal(LF_EConfigStreamStatus.BAD_FORMAT, LF_ConfigStream.Receive(LF_ConfigStream.FORMAT_VERSION + 1, hash, payload), "another format is refused");
		ctx.Equal(LF_EConfigStreamStatus.BAD_HASH, LF_ConfigStream.Receive(LF_ConfigStream.FORMAT_VERSION, hash + 1, payload), "a payload that does not match its hash is refused");
		string notJson = "{\"stream\":1,";
		ctx.Equal(LF_EConfigStreamStatus.BAD_JSON, LF_ConfigStream.Receive(LF_ConfigStream.FORMAT_VERSION, notJson.Hash(), notJson), "text that is not JSON is refused");
		string otherSchema = payload;
		otherSchema.Replace("\"schemaVersion\":1", "\"schemaVersion\":2");
		ctx.False(otherSchema == payload, "the fixture changed the schema");
		ctx.Equal(LF_EConfigStreamStatus.BAD_SCHEMA, LF_ConfigStream.Receive(LF_ConfigStream.FORMAT_VERSION, otherSchema.Hash(), otherSchema), "another schema is refused");
		ctx.False(LF_ConfigStream.HasReceived(), "nothing refused is kept");
		ctx.Equal(50, ClientShops().m_iDefaultBuyPrice, "after refused streams the client keeps the shipped default");

		// Parses, but one shop value is of the wrong type: the whole section keeps the shipped values.
		string badField = "{\"stream\":1,\"schemaVersion\":1,\"owner\":true,\"shops\":{\"defaultBuyPrice\":61,\"priceRules\":{\"0\":{\"find\":\"Prefabs/Items/Food/\",\"buyPrice\":\"x\",\"sellPrice\":4}}}}";
		ctx.Equal(LF_EConfigStreamStatus.APPLIED, LF_ConfigStream.Receive(LF_ConfigStream.FORMAT_VERSION, badField.Hash(), badField), "a well-formed stream is accepted");
		LF_ShopPricesConfig bad = ClientShops();
		ctx.Equal(50, bad.m_iDefaultBuyPrice, "a bad field anywhere keeps every shipped shop value");
		ctx.Equal(3, bad.m_aPriceRules.Count(), "a bad field keeps the shipped rules");

		// A good stream after the refused ones applies.
		ctx.Equal(LF_EConfigStreamStatus.APPLIED, LF_ConfigStream.Receive(LF_ConfigStream.FORMAT_VERSION, hash, payload), "the server's good stream is accepted");
		ctx.Equal(61, ClientShops().m_iDefaultBuyPrice, "the good stream applies");

		LF_ConfigStream.Reset();
		LF_ConfigService.Reset();
	}
};
