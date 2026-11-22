// red-proof: PENDING (game busy): batched with LF_Test_WelcomeGuide in one `--red` run.
//! Welcome guide in a world: the server's first-spawn hook marks the guide pending even when the player
//! has no controller (the client asks again at join), and every page is built from kit parts in order:
//! title, page line, heading, two paragraphs, then the buttons. Only the first page has "Set waypoint",
//! the first has no Previous, the last no Next, and every page has "Don't show again" and Close.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_WelcomeGuideWorld : LF_Test
{
	static const string ID = "test-welcome-guide-world";
	//! No player has this id, so OnFirstSpawn finds no controller.
	static const int NO_PLAYER = 987654;

	override string GetName()
	{
		return "quests/welcome-guide-world";
	}

	override void Run(LF_TestContext ctx)
	{
		array<ref LF_WelcomeGuideRow> saved = LF_WelcomeGuide.ExportAll();
		LF_WelcomeGuide.ApplyAll(null);

		// RedProof(): the first-spawn hook is skipped, so the guide is not pending (`--red`).
		if (!RedProof())
			LF_WelcomeGuide.OnFirstSpawn(NO_PLAYER, ID);

		ctx.True(LF_WelcomeGuide.IsPending(ID), "a first spawn with no controller still marks the guide pending");
		LF_WelcomeGuide.ApplyAll(saved);

		ctx.Equal(5, LF_WelcomeGuideSheet.PageCount(), "the guide has five pages");
		for (int page = 0; page < LF_WelcomeGuideSheet.PageCount(); page++)
		{
			array<ref LF_UIPart> parts = LF_WelcomeGuideSheet.BuildParts(page);
			string label = "page " + (page + 1).ToString() + ": ";
			ctx.True(parts.Count() >= 8, label + "has the title, page line, heading, two paragraphs and buttons");
			if (parts.Count() < 8)
				continue;

			ctx.True(parts[0].GetKind() == LF_EUIPartKind.TITLE, label + "starts with the title");
			ctx.True(parts[2].GetKind() == LF_EUIPartKind.HEADING && !parts[2].GetText().IsEmpty(), label + "has its heading");
			array<string> keys = LF_WelcomeGuideSheet.PageKeys(page);
			ctx.Equal(3, keys.Count(), label + "has a heading key and two paragraph keys");
			if (page > 0)
				ctx.True(keys[0] != LF_WelcomeGuideSheet.PageKeys(page - 1)[0], label + "has its own heading");
			ctx.True(parts[3].GetKind() == LF_EUIPartKind.RICH && parts[4].GetKind() == LF_EUIPartKind.RICH, label + "has two paragraphs");
			ctx.True(parts[3].IsInBody() && parts[4].IsInBody(), label + "the paragraphs scroll in the body");
			ctx.True(parts[parts.Count() - 1].GetKind() == LF_EUIPartKind.CLOSE, label + "ends with Close");
			ctx.EqualStr(LF_WelcomeGuideSheet.PART_DISMISS, parts[parts.Count() - 2].GetName(), label + "has Don't show again above Close");

			bool waypoint = Has(parts, LF_WelcomeGuideSheet.PART_WAYPOINT);
			ctx.True(waypoint == (page == 0), label + "Set waypoint only on the first page");

			LF_UIPart nav = Find(parts, LF_WelcomeGuideSheet.PART_NAV);
			ctx.NotNull(nav, label + "has the page buttons");
			if (!nav)
				continue;

			ctx.True((nav.GetKeyNames().Find(LF_WelcomeGuideSheet.PART_PREV) != -1) == (page > 0), label + "Previous everywhere but the first page");
			ctx.True((nav.GetKeyNames().Find(LF_WelcomeGuideSheet.PART_NEXT) != -1) == (page < LF_WelcomeGuideSheet.PageCount() - 1), label + "Next everywhere but the last page");
		}

		ctx.Equal(0, LF_WelcomeGuideSheet.BuildParts(LF_WelcomeGuideSheet.PageCount()).Count(), "a page past the end builds nothing");
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_UIPart Find(array<ref LF_UIPart> parts, string name)
	{
		foreach (LF_UIPart part : parts)
		{
			if (part && part.GetName() == name)
				return part;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool Has(array<ref LF_UIPart> parts, string name)
	{
		return Find(parts, name) != null;
	}
}
