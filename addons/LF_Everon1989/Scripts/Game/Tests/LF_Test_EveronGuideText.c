// red-proof: RED-OK everon/guide-text, observed 2026-11-22 with `tools\cli test --no-build --red LF_Test_EveronGuideText` (pack leg): with the hook on the guide shows the raw row, so `the guide's money paragraph names the starting bank` and `has its figure filled in` fail; green with `--only`.
//! The Everon pack's first-2-hours text (review A4, A5, A9): the welcome guide's money paragraph names the
//! live starting bank ($2,000 under this header) in the HUD's format and never a figure of its own, page 1
//! teaches the route clerk, car, orchard, press, market, page 2 sends the player to F2 What to do next, and
//! the clerk has one name (the town clerk) on the waypoint, the Talk row, the guide and the map.
[LF_TestInfo(LF_TestTier.LOGIC, pack: true)]
class LF_Test_EveronGuideText : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/guide-text";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_CharacterConfig.Reload();
		int bank = LF_CharacterConfig.StartingBank(1000);
		string money = LF_WelcomeGuideSheet.BodyText(LF_WelcomeGuideSheet.KEY_MONEY);
		if (RedProof())
			money = WidgetManager.Translate(LF_WelcomeGuideSheet.KEY_MONEY);

		ctx.EqualStr("$2,000", LF_MoneyFormat.FormatMoney(bank), "the pack's opening bank is the figure the HUD shows");
		ctx.True(money.Contains("$2,000"), "the guide's money paragraph names the starting bank");
		ctx.False(money.Contains("%1"), "the guide's money paragraph has its figure filled in");
		ctx.False(money.Contains("$1,000"), "the guide no longer says $1,000");

		string route = LF_Terms.Text("#LF-WelcomeGuide_Body1b");
		ctx.True(route.Contains("garage") && route.Contains("orchard") && route.Contains("juice") && route.Contains("market"), "page 1 teaches the real route: garage, orchard, juice, market");
		string next = LF_Terms.Text("#LF-WelcomeGuide_Body2b");
		ctx.True(next.Contains("F2") && next.Contains("What to do next"), "page 2 points at F2 What to do next");
		ctx.False(next.Contains("courier run"), "the guide no longer sends the newcomer to the courier run first");

		ctx.True(LF_Terms.Text("#LF-Arrival_ClerkMark").Contains("town clerk"), "the clerk's waypoint says the town clerk");
		ctx.True(LF_Terms.Text("#LF-Quest_Action_Clerk").Contains("town clerk"), "the Talk row says the town clerk");
		ctx.True(LF_Terms.Text("#LF-Quest_Giver_Clerk").Contains("Town clerk"), "the clerk's label says the town clerk");
		ctx.True(LF_Terms.Text("#LF-MapPoi_Clerk").Contains("Town clerk"), "the map names the town clerk");
		ctx.False(LF_Terms.Text("#LF-WelcomeGuide_Body1a").Contains("registry clerk"), "the guide says the town clerk");
		ctx.True(LF_Terms.Text("#LF-WelcomeGuide_Body1a").Contains("town clerk"), "the guide names the town clerk");
	}
}
