// red-proof: 2026-11-18, `--only LF_Test_GatherToast` on main before the change: LF_GatherAction.TraderPrice, ToastText and LF_ProcessAction.ToastText did not exist, so the test could not compile. `--red LF_Test_GatherToast` keeps the market empty, so the price checks fail.
//! gather/toast (#3156): a finished gather tells the player what came in, the XP it paid and what a trader
//! pays; mining iron ore names the bar's price, a processed bar names its own, and an item no trader
//! buys leaves the price sentence out.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_GatherToast : LF_Test
{
	protected const string IRON_NUGGET = "{6853E2E2AE878611}Prefabs/Resources/Mining/IronNugget.et";
	protected const string IRON_BAR = "{BCD1E8CD7F975074}Prefabs/Resources/Mining/IronBar.et";
	protected const string APPLE = "{C9D661E5B0714711}Prefabs/Items/Food/Apple.et";

	override string GetName()
	{
		return "gather/toast";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_MarketConfig market = new LF_MarketConfig();
		market.m_bEnabled = false;
		market.m_aGoods = {};
		if (!RedProof())
			market.m_aGoods.Insert(LF_MarketGoodDef.Make(IRON_BAR, 50, 50, 50, 0, 0));
		LF_MarketConfig.UseForTest(market);

		// Raw ore: the price is the processed bar's.
		ResourceName soldAs;
		int orePrice = LF_GatherAction.TraderPrice(IRON_NUGGET, soldAs);
		ctx.Equal(50, orePrice, "iron ore is priced as the bar a furnace makes of it");
		ctx.EqualStr(IRON_BAR, soldAs, "the price is quoted for the bar");

		// A good with its own row is priced as itself.
		int barPrice = LF_GatherAction.TraderPrice(IRON_BAR, soldAs);
		ctx.Equal(50, barPrice, "a bar has its own price");
		ctx.EqualStr(IRON_BAR, soldAs, "a bar is quoted as itself");

		// No trader buys an apple here: no price.
		int applePrice = LF_GatherAction.TraderPrice(APPLE, soldAs);
		ctx.Equal(0, applePrice, "an item no trader buys has no price");
		ctx.True(soldAs.IsEmpty(), "nothing is quoted for it");

		// The sentence names the item, the amount, the XP and the price.
		string full = LF_GatherAction.ToastText("Iron ore", 1, 5, orePrice, "Iron bar");
		ctx.True(full.Contains("Iron ore"), "the toast names what came in: " + full);
		ctx.True(full.Contains("x1"), "the toast names the amount: " + full);
		ctx.True(full.Contains("+5 XP"), "the toast names the XP: " + full);
		ctx.True(full.Contains("$50"), "the toast names the trader price: " + full);
		ctx.True(full.Contains("Iron bar"), "the toast names what the price is for: " + full);

		// No XP paid (cooldown, capped, digging): no XP part.
		string noXp = LF_GatherAction.ToastText("Iron ore", 1, 0, orePrice, "Iron bar");
		ctx.False(noXp.Contains("XP"), "no XP paid leaves the XP part out: " + noXp);

		// No trader price: the price sentence is omitted.
		string noPrice = LF_GatherAction.ToastText("Apple", 2, 3, applePrice, "");
		ctx.True(noPrice.Contains("Apple"), "the unpriced toast still names the item: " + noPrice);
		ctx.True(noPrice.Contains("+3 XP"), "the unpriced toast keeps the XP: " + noPrice);
		ctx.False(noPrice.Contains("$"), "an item no trader buys omits the price: " + noPrice);

		// Processing: the toast shows what came out and its price.
		array<ref LF_ProcessingOutput> outputs = {};
		LF_ProcessingOutput bar = new LF_ProcessingOutput();
		bar.m_OutputPrefab = IRON_BAR;
		bar.m_iOutputAmount = 1;
		outputs.Insert(bar);
		string made = LF_ProcessAction.ToastText(outputs);
		ctx.True(made.Contains("x1"), "the process toast names the amount: " + made);
		ctx.True(made.Contains("$50"), "the process toast names the trader price: " + made);

		LF_ProcessingOutput apple = new LF_ProcessingOutput();
		apple.m_OutputPrefab = APPLE;
		apple.m_iOutputAmount = 1;
		array<ref LF_ProcessingOutput> unpriced = {};
		unpriced.Insert(apple);
		ctx.False(LF_ProcessAction.ToastText(unpriced).Contains("$"), "an unpriced product omits the price");

		LF_MarketConfig.Reload();
	}
}
