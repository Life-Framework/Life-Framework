// red-proof: observed 2026-11-16 with `tools\cli test --no-build --red LF_Test_HandbookScreen`: `RED-OK rules/handbook-screen (fails with the hook on, 16 assertions)`; the hook skips building the page, so `a page builds` and every page assertion fail. Green without the hook.
// tier: LOGIC
//! Handbook screen (#1773): the parts the screen is built from, with no widgets. The list names its
//! rows by place so a pick maps back to a page, the second page's title is the one its part carries,
//! and the empty handbook says so instead of showing an empty list.
class LF_Test_HandbookScreen : LF_Test
{
	override string GetName()
	{
		return "rules/handbook-screen";
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_HandbookPage MakePage(string id, string title, string body1, string body2, int order)
	{
		LF_HandbookPage page = new LF_HandbookPage();
		page.m_sId = id;
		page.m_sTitleKey = title;
		page.m_aBodyKeys = new array<string>();
		page.m_aBodyKeys.Insert(body1);
		if (!body2.IsEmpty())
			page.m_aBodyKeys.Insert(body2);

		page.m_iOrder = order;
		return page;
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_UIPart FindPart(array<ref LF_UIPart> parts, string name)
	{
		foreach (LF_UIPart part : parts)
		{
			if (part.GetName() == name)
				return part;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		CheckRows(ctx);
		CheckList(ctx);
		CheckPage(ctx);
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckRows(LF_TestContext ctx)
	{
		ctx.EqualStr("HandbookPage0", LF_HandbookScreen.RowName(0), "the first row is HandbookPage0");
		ctx.Equal(1, LF_HandbookScreen.RowIndex(LF_HandbookScreen.RowName(1)), "a row name maps back to its place");
		ctx.Equal(12, LF_HandbookScreen.RowIndex("HandbookPage12"), "a two digit place maps back");
		ctx.Equal(-1, LF_HandbookScreen.RowIndex(LF_HandbookScreen.PART_LIST), "the list is not a row, though its name starts with the row prefix");
		ctx.Equal(-1, LF_HandbookScreen.RowIndex(LF_HandbookScreen.PART_CLOSE), "the Close button is not a row");
		ctx.Equal(-1, LF_HandbookScreen.RowIndex("HandbookPage1x"), "a name with a stray letter is not a row");
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckList(LF_TestContext ctx)
	{
		array<ref LF_UIPart> withPages = LF_HandbookScreen.BuildListParts(true);
		LF_UIPart list = FindPart(withPages, LF_HandbookScreen.PART_LIST);
		ctx.True(list != null && list.GetKind() == LF_EUIPartKind.LIST, "with pages the screen has a list");
		LF_UIPart close = FindPart(withPages, LF_HandbookScreen.PART_CLOSE);
		ctx.True(close != null && close.GetKind() == LF_EUIPartKind.CLOSE, "the list page has a Close button");
		ctx.True(FindPart(withPages, LF_HandbookScreen.PART_EMPTY) == null, "with pages there is no empty line");

		array<ref LF_UIPart> none = LF_HandbookScreen.BuildListParts(false);
		ctx.True(FindPart(none, LF_HandbookScreen.PART_LIST) == null, "without pages there is no list");
		LF_UIPart empty = FindPart(none, LF_HandbookScreen.PART_EMPTY);
		ctx.True(empty != null && empty.GetText() == "#LF-Handbook_Empty", "without pages one line says so");
		ctx.True(FindPart(none, LF_HandbookScreen.PART_CLOSE) != null, "the empty handbook can still be closed");
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckPage(LF_TestContext ctx)
	{
		LF_HandbookConfig config = new LF_HandbookConfig();
		config.m_aPages = new array<ref LF_HandbookPage>();
		config.m_aPages.Insert(MakePage("first", "#Test-Handbook_First", "#Test-Handbook_FirstBody", "", 10));
		config.m_aPages.Insert(MakePage("second", "#Test-Handbook_Second", "#Test-Handbook_SecondBodyA", "#Test-Handbook_SecondBodyB", 20));
		LF_Handbook.UseForTest(config);

		array<LF_HandbookPage> pages = {};
		LF_Handbook.GetPages(pages);
		ctx.Equal(2, pages.Count(), "the screen reads the config's two pages");
		if (pages.Count() != 2)
		{
			LF_Handbook.Reload();
			return;
		}

		// The row the flow clicks for the second page.
		LF_HandbookPage picked = LF_Handbook.Find(pages[LF_HandbookScreen.RowIndex(LF_HandbookScreen.RowName(1))].m_sId);
		ctx.True(picked != null && picked.m_sId == "second", "row HandbookPage1 is the second page");

		array<ref LF_UIPart> parts;
		// Red-proof hook: with --red the page is not built, so every assertion below must fail.
		if (!RedProof() && picked)
			parts = LF_HandbookScreen.BuildPageParts(picked);

		ctx.True(parts != null, "a page builds");
		if (parts)
		{
			ctx.Equal(6, parts.Count(), "a two paragraph page is a label, a title, two paragraphs, Contents and Close");
			LF_UIPart title = FindPart(parts, LF_HandbookScreen.PART_PAGE_TITLE);
			ctx.True(title != null && title.GetKind() == LF_EUIPartKind.TITLE, "the page has a title part");
			if (title)
				ctx.EqualStr("#Test-Handbook_Second", title.GetText(), "the title part carries the second page's title");

			LF_UIPart body = FindPart(parts, LF_HandbookScreen.PART_BODY + "1");
			ctx.True(body != null && body.GetText() == "#Test-Handbook_SecondBodyB", "paragraphs follow in reading order");
			LF_UIPart contents = FindPart(parts, LF_HandbookScreen.PART_CONTENTS);
			ctx.True(contents != null && contents.GetKind() == LF_EUIPartKind.BUTTON, "a page has a button back to the list");
			LF_UIPart close = FindPart(parts, LF_HandbookScreen.PART_CLOSE);
			ctx.True(close != null && close.GetKind() == LF_EUIPartKind.CLOSE, "a page has a Close button");
		}

		LF_Handbook.Reload();
	}
}
