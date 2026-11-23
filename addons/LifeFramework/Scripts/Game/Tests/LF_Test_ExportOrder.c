// red-proof: 2026-11-23 `tools/cli test --no-build --red LF_Test_ExportOrder`: RED-OK (4 assertions); RedProof() skips the book tick, so "a fresh book opens three orders (expected 3, got 0)" fails.
//! Export orders (#4410): the book opens three orders at once and a fourth only after one closes, at the
//! next 30 game minutes; a level-9 hauler is refused in words; the full count pays once (a second hand-in
//! pays nothing); a short count pays nothing and keeps the order; a missed deadline closes the order and
//! lowers the taker's town standing.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_ExportOrder : LF_Test
{
	protected static const string HAULER = "test-export-hauler";
	protected static const string ROOKIE = "test-export-rookie";
	protected static const string LATE = "test-export-late";
	protected static const ResourceName GOODS = "{BCD1E8CD7F975074}Prefabs/Resources/Mining/IronBar.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "jobs/export-order";
	}

	//------------------------------------------------------------------------------------------------
	protected LF_ExportOrderConfig MakeConfig()
	{
		LF_ExportOrderConfig config = LF_ExportOrderConfig.CreateDefault();
		LF_ExportOrderTemplate iron = new LF_ExportOrderTemplate();
		iron.m_sId = "iron";
		iron.m_Goods = GOODS;
		iron.m_sGoodsName = "iron bars";
		iron.m_sDestination = "the steamer";
		iron.m_iCount = 20;
		iron.m_iWholesale = 40;
		iron.m_iGameMinutes = 180;
		config.m_aTemplates.Insert(iron);
		return config;
	}

	//------------------------------------------------------------------------------------------------
	protected void Tick(int minute)
	{
		if (!RedProof())
			LF_ExportOrderBook.Tick(minute);
	}

	//------------------------------------------------------------------------------------------------
	protected void Clean()
	{
		LF_ExportOrderBook.ResetStatics();
		LF_ExportOrderConfig.Reload();
		LF_Reputation.Forget(HAULER);
		LF_Reputation.Forget(ROOKIE);
		LF_Reputation.Forget(LATE);
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		Clean();
		LF_ExportOrderConfig.UseForTest(MakeConfig());

		Tick(600);
		array<ref LF_ExportOrder> orders = LF_ExportOrderBook.List();
		ctx.Equal(3, orders.Count(), "a fresh book opens three orders");
		Tick(630);
		ctx.Equal(3, orders.Count(), "a full book opens no fourth");
		if (orders.Count() < 3)
		{
			Clean();
			return;
		}

		LF_ExportOrder first = orders[0];
		ctx.Equal(920, first.m_iPay, "20 iron bars at $40 wholesale pay 15 percent over: $920");
		ctx.EqualStr("13:00", first.DeadlineText(), "an order opened at 10:00 with three hours is due by 13:00");

		// The level gate, in words.
		string refused = LF_ExportOrderBook.Take(ROOKIE, 9, first.m_iId);
		ctx.EqualStr(LF_ExportOrderBook.REFUSE_LEVEL, refused, "a level-9 hauler is refused with the level reason");
		ctx.True(refused.StartsWith("#"), "the refusal is a string key the player reads");
		ctx.True(!first.IsTaken(), "a refused take leaves the order open");

		// Take, short hand-in, full hand-in, second hand-in.
		ctx.EqualStr("", LF_ExportOrderBook.Take(HAULER, 10, first.m_iId), "a level-10 hauler takes the order");
		ctx.EqualStr(LF_ExportOrderBook.REFUSE_TAKEN, LF_ExportOrderBook.Take(ROOKIE, 12, first.m_iId), "a taken order is refused to another hauler");
		ctx.Equal(0, LF_ExportOrderBook.HandIn(HAULER, 19), "a short count pays nothing");
		ctx.True(LF_ExportOrderBook.HeldBy(HAULER) == first, "a short count keeps the order");
		ctx.Equal(920, LF_ExportOrderBook.HandIn(HAULER, 20), "the full count pays the order");
		ctx.Equal(0, LF_ExportOrderBook.HandIn(HAULER, 20), "a second hand-in pays nothing");
		ctx.Equal(2, orders.Count(), "the paid order closed");

		// A fourth opens only at the next 30 game minutes after the close.
		Tick(650);
		ctx.Equal(2, orders.Count(), "no new order before the next 30 game minutes");
		Tick(660);
		ctx.Equal(3, orders.Count(), "a new order opens 30 game minutes on");
		ctx.Equal(4, orders[2].m_iId, "the new order is the fourth opened");

		// A missed deadline frees the slot and costs standing, not money.
		LF_ExportOrder late = orders[0];
		ctx.EqualStr("", LF_ExportOrderBook.Take(LATE, 15, late.m_iId), "another hauler takes an order");
		int before = LF_Reputation.Points(LATE, "TOWN");
		Tick(late.m_iDeadlineMinute);
		ctx.True(LF_ExportOrderBook.HeldBy(LATE) == null, "the missed order is freed from its taker");
		ctx.True(LF_ExportOrderBook.Find(late.m_iId) == null, "the missed order closes");
		ctx.True(LF_Reputation.Points(LATE, "TOWN") < before, "the missed deadline lowers town standing");
		ctx.Equal(0, LF_ExportOrderBook.HandIn(LATE, 20), "a missed order pays nothing afterwards");

		Clean();
	}
}
