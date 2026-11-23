// red-proof: observed 2026-11-23 with the test command, `test --no-build --no-cache --red LF_Test_FenceVoice with its pack twin`: RED PROOF OK (RedProof() withholds the stand-in conversation, so the fence has no line and the refusal and sale assertions fail); green with the hook off. A first run failed on a second drill purchase (a one-per-buyer limit), so the quiet case moved before the record.
//! The fence speaks at his counter (#4620). The GridWorld fence's real buy path, a buyer with a real record:
//! a stranger turned away from the drill hears `refuse_drill`; at the record the drill sells and `sell_drill`
//! plays; a tool with no gate (the lockpick) sells in silence. The conversation is a hand-built stand-in so the
//! core leg needs no pack recording (the pack's real lines are checked in the pack leg).
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_FenceVoice : LF_Test
{
	protected static const string UID = "fence-voice-a";
	protected static const ResourceName LOCKPICK = "{6B1D3F5A92C47E08}Prefabs/Items/Fence/LF_Lockpick.et";

	override string GetName()
	{
		return "crime/fence-voice";
	}

	protected static LF_DialogueLine Line(string id, string key)
	{
		LF_DialogueLine line = new LF_DialogueLine();
		line.m_sId = id;
		line.m_sTextKey = key;
		return line;
	}

	protected LF_ShopStockConfig BuildStock()
	{
		LF_ShopStockItem drill = new LF_ShopStockItem();
		drill.m_Prefab = LF_FenceVoice.DRILL;
		drill.m_iPrice = 100;
		drill.m_iMinCrimes = 3;

		LF_ShopStockItem pick = new LF_ShopStockItem();
		pick.m_Prefab = LOCKPICK;
		pick.m_iPrice = 10;

		LF_ShopStock stock = new LF_ShopStock();
		stock.m_sId = "fence";
		stock.m_aItems = {drill, pick};

		LF_ShopStockConfig config = new LF_ShopStockConfig();
		config.m_aStocks = {stock};
		return config;
	}

	override void Run(LF_TestContext ctx)
	{
		// The decision table: which shop outcome speaks which line.
		ctx.EqualStr(LF_FenceVoice.LINE_REFUSE, LF_FenceVoice.LineFor(LF_EShopRefusal.NOT_TRUSTED, LF_FenceVoice.DRILL), "a record refusal speaks the refusal line");
		ctx.EqualStr(LF_FenceVoice.LINE_SELL, LF_FenceVoice.LineFor(LF_EShopRefusal.NONE, LF_FenceVoice.DRILL), "a drill sale speaks the sale line");
		ctx.EqualStr("", LF_FenceVoice.LineFor(LF_EShopRefusal.NONE, LOCKPICK), "a lockpick sale is silent");
		ctx.EqualStr("", LF_FenceVoice.LineFor(LF_EShopRefusal.NOT_ENOUGH_CASH, LF_FenceVoice.DRILL), "a poor buyer is not told he is a stranger");

		IEntity fence = GetGame().GetWorld().FindEntityByName("Grid_shops_till_Fence");
		ctx.NotNull(fence, "Grid_shops_till_Fence resolves in the GridWorld");
		LF_ShopComponent shop = LF_Component<LF_ShopComponent>.Find(fence);
		ctx.NotNull(shop, "the fence keeps a shop");
		IEntity buyer = LF_TestCharacterFixture.Spawn(ctx);
		if (!fence || !shop || !buyer)
		{
			LF_TestCharacterFixture.Delete(buyer);
			return;
		}

		LF_ShopStockConfig.Set(BuildStock());
		LF_DialogueConfig voice = new LF_DialogueConfig();
		voice.m_sId = "role_fence";
		voice.m_aLines = {Line(LF_FenceVoice.LINE_REFUSE, "#LF-Dialogue_RoleFenceEveron_RefuseDrill"), Line(LF_FenceVoice.LINE_SELL, "#LF-Dialogue_RoleFenceEveron_SellDrill")};
		LF_Utils.SetUidForTests(buyer, UID);
		array<ref LF_CrimeRecordRow> before = LF_CrimeRecord.ExportAll();
		LF_CrimeRecord.ApplyAll(null);
		LF_MoneyUtils.AddCash(buyer, 500);

		// A fence whose conversation holds neither line stays quiet; the refusal still stands.
		LF_FenceVoice.SetConfigForTests(new LF_DialogueConfig());
		int silent = LF_FenceVoice.SpokenCount();
		ctx.False(shop.BuyItem(LF_FenceVoice.DRILL, 1, buyer), "a stranger is refused with no line recorded");
		ctx.Equal(silent, LF_FenceVoice.SpokenCount(), "no recorded line: the fence stays quiet");
		if (!RedProof())
			LF_FenceVoice.SetConfigForTests(voice);

		// A stranger: refused, and the fence says why.
		int spoken = LF_FenceVoice.SpokenCount();
		ctx.False(shop.BuyItem(LF_FenceVoice.DRILL, 1, buyer), "a stranger is refused the drill");
		ctx.Equal(spoken + 1, LF_FenceVoice.SpokenCount(), "the refusal speaks once");
		ctx.EqualStr(LF_FenceVoice.LINE_REFUSE, LF_FenceVoice.LastLine(), "the refusal line is refuse_drill");
		ctx.Equal(500, LF_MoneyUtils.GetCash(buyer), "a refusal costs nothing");

		// A tool with no gate sells without a word.
		spoken = LF_FenceVoice.SpokenCount();
		ctx.True(shop.BuyItem(LOCKPICK, 1, buyer), "the lockpick has no gate and sells");
		ctx.Equal(spoken, LF_FenceVoice.SpokenCount(), "a lockpick sale is silent");

		// The record meets the gate: the drill sells and the fence says so.
		for (int i = 0; i < 3; i++)
			LF_CrimeRecord.Record(LF_OnCrimeCommitted.Create(UID, LF_WantedReports.OFFENCE_ROBBERY));

		spoken = LF_FenceVoice.SpokenCount();
		ctx.True(shop.BuyItem(LF_FenceVoice.DRILL, 1, buyer), "at the record the drill sells");
		ctx.Equal(spoken + 1, LF_FenceVoice.SpokenCount(), "the sale speaks once");
		ctx.EqualStr(LF_FenceVoice.LINE_SELL, LF_FenceVoice.LastLine(), "the sale line is sell_drill");

		LF_CrimeRecord.ApplyAll(before);
		LF_Utils.SetUidForTests(null, "");
		LF_FenceVoice.ResetStatics();
		LF_ShopStockConfig.Set(null);
		LF_TestCharacterFixture.Delete(buyer);
	}
}
