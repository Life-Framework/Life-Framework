// red-proof: observed 2026-11-16 with `tools\cli test --no-build --only LF_Test_HandbookPages`, the m_sTitleKey patch in LF_HandbookPageList.PatchFromContainer replaced by `if (false)`:
//   SUMMARY tier=only passed=0 failed=1 total=1 skipped=0
//   [LFTEST]   - patch: the title key is the overlay's (expected '#Test-Handbook_PatchedTitle', got '#LF-Handbook_ReadTitle')
// Restored; green.
// tier: LOGIC
//! Handbook pages load from config (#1772): the core file's pages in order, a pack overlay that adds
//! and patches a page by id, and a broken page skipped with one error line.
class LF_Test_HandbookPages : LF_Test
{
	static const ResourceName OVERLAY = "{DBA73A1F1423E7CA}Configs/Tests/LF_Test_HandbookOverlay.conf";

	override string GetName()
	{
		return "handbook/pages";
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_HandbookConfig CoreConfig()
	{
		Resource resource = BaseContainerTools.LoadContainer(LF_HandbookConfig.CONFIG_PATH);
		if (!resource || !resource.IsValid() || !resource.GetResource())
			return null;

		return LF_HandbookConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(resource.GetResource().ToBaseContainer()));
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_HandbookPage MakePage(string id, string title, string body, int order)
	{
		LF_HandbookPage page = new LF_HandbookPage();
		page.m_sId = id;
		page.m_sTitleKey = title;
		page.m_aBodyKeys = new array<string>();
		if (!body.IsEmpty())
			page.m_aBodyKeys.Insert(body);
		page.m_iOrder = order;
		return page;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_HandbookConfig core = CoreConfig();
		ctx.True(core != null, "the core handbook file loads");
		if (!core)
			return;

		array<LF_HandbookPage> pages = {};
		int skipped = LF_Handbook.Collect(core, pages);
		ctx.Equal(0, skipped, "the core pages are all usable");
		ctx.Equal(2, pages.Count(), "the core ships two pages");
		if (pages.Count() == 2)
		{
			ctx.EqualStr("how-to-read", pages[0].m_sId, "pages load in order: how to read first");
			ctx.EqualStr("find-the-rules", pages[1].m_sId, "then where to find the rules");
			ctx.EqualStr("#LF-Handbook_ReadTitle", pages[0].m_sTitleKey, "text is a localization key");
			ctx.Equal(2, pages[0].m_aBodyKeys.Count(), "a page keeps every body key");
		}

		CheckOrder(ctx);
		CheckOverlay(ctx);
		CheckBroken(ctx);
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckOrder(LF_TestContext ctx)
	{
		LF_HandbookConfig config = new LF_HandbookConfig();
		config.m_aPages = new array<ref LF_HandbookPage>();
		config.m_aPages.Insert(MakePage("late", "#t", "#b", 20));
		config.m_aPages.Insert(MakePage("floating-a", "#t", "#b", -1));
		config.m_aPages.Insert(MakePage("early", "#t", "#b", 5));
		config.m_aPages.Insert(MakePage("floating-b", "#t", "#b", -1));
		config.m_aPages.Insert(MakePage("tie", "#t", "#b", 20));
		array<LF_HandbookPage> pages = {};
		LF_Handbook.Collect(config, pages);
		string order;
		foreach (LF_HandbookPage page : pages)
		{
			order += page.m_sId + " ";
		}

		ctx.EqualStr("early late tie floating-a floating-b ", order, "lowest order first, ties and unnumbered pages keep file order");
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckOverlay(LF_TestContext ctx)
	{
		LF_HandbookConfig core = CoreConfig();
		Resource resource = BaseContainerTools.LoadContainer(OVERLAY);
		ctx.True(resource && resource.IsValid() && resource.GetResource(), "the overlay fixture loads");
		if (!core || !resource || !resource.IsValid() || !resource.GetResource())
			return;

		int changed = LF_Handbook.ApplyOverlay(core, resource.GetResource().ToBaseContainer(), OVERLAY);
		ctx.Equal(3, changed, "the overlay patches one page and adds two");

		array<LF_HandbookPage> pages = {};
		int skipped = LF_Handbook.Collect(core, pages);
		ctx.Equal(1, skipped, "the added page with no body is skipped");
		ctx.Equal(3, pages.Count(), "two core pages and the one good added page");
		if (pages.Count() != 3)
			return;

		ctx.EqualStr("test-extra", pages[0].m_sId, "the added page sorts by its order, before the core pages");
		ctx.EqualStr("find-the-rules", pages[1].m_sId, "the patched page moved behind a page it used to precede");
		ctx.EqualStr("how-to-read", pages[2].m_sId, "patch: how-to-read takes order 30");
		ctx.EqualStr("#Test-Handbook_PatchedTitle", pages[2].m_sTitleKey, "patch: the title key is the overlay's");
		ctx.Equal(2, pages[2].m_aBodyKeys.Count(), "patch: the body it did not set is kept");
		ctx.EqualStr("#Test-Handbook_ExtraBody", pages[0].m_aBodyKeys[0], "add: the new page has its body");
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckBroken(LF_TestContext ctx)
	{
		LF_HandbookConfig config = new LF_HandbookConfig();
		config.m_aPages = new array<ref LF_HandbookPage>();
		config.m_aPages.Insert(MakePage("good", "#t", "#b", 1));
		config.m_aPages.Insert(MakePage("no-title", "", "#b", 2));
		config.m_aPages.Insert(MakePage("no-body", "#t", "", 3));
		config.m_aPages.Insert(MakePage("", "#t", "#b", 4));
		config.m_aPages.Insert(MakePage("good-too", "#t", "#b", 5));
		array<LF_HandbookPage> pages = {};
		int skipped = LF_Handbook.Collect(config, pages);
		ctx.Equal(3, skipped, "a page with no title, no body or no id is skipped");
		ctx.Equal(2, pages.Count(), "the good pages still load");
		ctx.True(LF_Handbook.RefusePage(null) != "", "a null entry is refused");
		LF_HandbookPage blank = MakePage("blank-body", "#t", "#b", 6);
		blank.m_aBodyKeys.Insert("");
		ctx.True(LF_Handbook.RefusePage(blank) != "", "a blank body paragraph refuses the page");
	}
}
