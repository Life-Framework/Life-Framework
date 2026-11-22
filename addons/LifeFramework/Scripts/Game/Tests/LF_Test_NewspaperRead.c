// red-proof: the first run, before LF_Newspaper and the prefab existed, failed to compile (`tools\cli test --no-build --only LF_Test_NewspaperRead`).
// With the scaffold in place, RedProof() skips publishing the test edition and spawning the newspaper, so
// "the page prints the current edition's headline" and "the newspaper carries one Read action" fail.

[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_NewspaperRead : LF_Test
{
	static const ResourceName PREFAB = "{4D7E2B91A6C35F08}Prefabs/Items/Paper/LF_Newspaper.et";

	override string GetName()
	{
		return "newspaper/read";
	}

	override void Run(LF_TestContext ctx)
	{
		ExpectReadAction(ctx);
		ExpectPlaceholderPage(ctx);
		ExpectCurrentEditionOnPage(ctx);
	}

	//------------------------------------------------------------------------------------------------
	//! The item is a held paper with one Read action.
	protected void ExpectReadAction(LF_TestContext ctx)
	{
		Resource resource = Resource.Load(PREFAB);
		ctx.True(resource && resource.IsValid(), "the newspaper prefab loads under its GUID");
		if (!resource || !resource.IsValid())
			return;

		IEntity paper;
		if (!RedProof())
		{
			EntitySpawnParams params();
			params.TransformMode = ETransformMode.LOCAL;
			paper = GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), params);
		}

		ctx.NotNull(paper, "the newspaper spawns");
		if (!paper)
			return;

		int reads;
		ActionsManagerComponent actions = LF_Component<ActionsManagerComponent>.Find(paper);
		ctx.NotNull(actions, "the newspaper has an actions manager");
		if (actions)
		{
			array<BaseUserAction> list = {};
			actions.GetActionsList(list);
			foreach (BaseUserAction action : list)
			{
				if (LF_NewspaperReadAction.Cast(action))
					reads++;
			}
		}

		ctx.Equal(1, reads, "the newspaper carries one Read action");
		SCR_EntityHelper.DeleteEntityAndChildren(paper);
	}

	//------------------------------------------------------------------------------------------------
	//! With no edition published the page still reads: a masthead, a headline per story, a Close.
	protected void ExpectPlaceholderPage(LF_TestContext ctx)
	{
		LF_Newspaper.ResetStatics();
		LF_NewspaperIssue issue = LF_Newspaper.Current();
		ctx.NotNull(issue, "an unpublished week still has an edition to read");
		array<ref LF_UIPart> parts = LF_Newspaper.BuildParts(issue, null);
		ctx.True(LF_Newspaper.CountKind(parts, LF_EUIPartKind.TITLE) == 1, "the page has one masthead");
		ctx.True(LF_Newspaper.CountKind(parts, LF_EUIPartKind.CLOSE) == 1, "the page has a Close");
		ctx.True(LF_Newspaper.CountKind(parts, LF_EUIPartKind.HEADING) >= 1, "the page has at least one headline");
		ctx.True(!LF_Newspaper.PageText(parts).IsEmpty(), "the placeholder page prints words");
	}

	//------------------------------------------------------------------------------------------------
	//! The read page shows the current edition's text, and a newer edition replaces it.
	protected void ExpectCurrentEditionOnPage(LF_TestContext ctx)
	{
		LF_NewspaperIssue issue = new LF_NewspaperIssue();
		issue.m_sMasthead = "Test Courier";
		issue.m_sDateline = "Week 7";
		issue.AddStory("Bank vault robbed at night", "Two men fled with *$4,200* in cash.");
		issue.AddStory("New baker on Main Street", "The bakery opens Monday.");
		if (!RedProof())
			LF_Newspaper.SetCurrent(issue);

		array<ref LF_UIPart> parts = LF_Newspaper.BuildParts(LF_Newspaper.Current(), null);
		string page = LF_Newspaper.PageText(parts);
		ctx.True(page.Contains("Test Courier"), "the page prints the current edition's masthead");
		ctx.True(page.Contains("Week 7"), "the page prints the dateline");
		ctx.True(page.Contains("Bank vault robbed at night"), "the page prints the current edition's headline");
		ctx.True(page.Contains("Two men fled with"), "the page prints the story text");
		ctx.True(page.Contains("New baker on Main Street"), "the page prints every story");
		ctx.True(!page.Contains("*"), "the accent marks are not printed");
		ctx.Equal(2, LF_Newspaper.CountKind(parts, LF_EUIPartKind.HEADING), "one headline part per story");

		LF_NewspaperIssue next = new LF_NewspaperIssue();
		next.m_sMasthead = "Test Courier";
		next.AddStory("Harvest festival", "Next week.");
		LF_Newspaper.SetCurrent(next);
		string newer = LF_Newspaper.PageText(LF_Newspaper.BuildParts(LF_Newspaper.Current(), null));
		ctx.True(newer.Contains("Harvest festival") && !newer.Contains("Bank vault"), "a newer edition replaces the old page");

		LF_Newspaper.ResetStatics();
	}
}
