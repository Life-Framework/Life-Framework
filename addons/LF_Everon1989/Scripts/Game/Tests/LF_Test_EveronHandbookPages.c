// red-proof: observed 2026-11-16 (#1776): before the overlay rows and strings, `tools\cli test --no-build --only LF_Test_EveronHandbookPages` failed `- the handbook lists the seven flagship pages (expected 7, got 2)` (passed=0 failed=1); with them it passes (passed=1 failed=0, pack leg worlds=LF_Everon1989_Test).
// tier: WORLD
//! #1776 in game: under the Everon 1989 header (the pack leg of `tools\cli test`), the handbook lists
//! the seven flagship pages in reading order, and every title and body key is in a string table, so
//! no `#LF-` key shows raw on the page. The core file ships two neutral pages; the pack overlay adds
//! the other five and patches the rules page into the law and the permit.
class LF_Test_EveronHandbookPages : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/handbook-pages";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_MissionHeader header = LF_MissionHeader.GetCurrent();
		ctx.NotNull(header, "the pack leg runs under an LF mission header");

		// Layer the overlay again so an earlier test's cached config cannot hide it.
		LF_Handbook.Reload();

		array<LF_HandbookPage> pages = {};
		int skipped = LF_Handbook.GetPages(pages);
		ctx.Equal(0, skipped, "no handbook page is skipped under the pack");

		array<string> expected = {"how-to-read", "parcel", "jobs", "police", "medic", "crime", "find-the-rules"};
		ctx.Equal(expected.Count(), pages.Count(), "the handbook lists the seven flagship pages");
		if (pages.Count() != expected.Count())
			return;

		foreach (int i, string id : expected)
		{
			LF_HandbookPage page = pages[i];
			ctx.EqualStr(id, page.m_sId, string.Format("page %1 is '%2'", i + 1, id));
			CheckKey(ctx, page.m_sId, page.m_sTitleKey);
			ctx.True(page.m_aBodyKeys.Count() >= 2 && page.m_aBodyKeys.Count() <= 5, string.Format("page '%1' has two to five paragraphs", id));
			foreach (string body : page.m_aBodyKeys)
			{
				CheckKey(ctx, page.m_sId, body);
			}
		}

		// The rules page keeps its core id but carries the pack's words.
		LF_HandbookPage rules = LF_Handbook.Find("find-the-rules");
		ctx.NotNull(rules, "the rules page is readable");
		if (rules)
			ctx.True(rules.m_sTitleKey != "#LF-Handbook_RulesTitle", "the pack patched the rules page title");

		LF_Handbook.Reload();
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckKey(LF_TestContext ctx, string pageId, string key)
	{
		ctx.True(key.StartsWith("#"), string.Format("page '%1': '%2' is a localization key", pageId, key));
		string text = WidgetManager.Translate(key);
		ctx.True(text != key && !text.IsEmpty(), string.Format("page '%1': key '%2' resolves to text", pageId, key));
		ctx.True(!text.Contains("#LF-"), string.Format("page '%1': key '%2' shows no raw key", pageId, key));
	}
}
