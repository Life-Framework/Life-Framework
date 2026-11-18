// red-proof: new test, no failing state to observe on old code (InBody did not exist). Batched --red run: RedProof() skips the
// flagging, so the body parts stay in Content and every structure assertion fails.
// RED-OK ui-kit/sheet-body (fails with the hook on, 13 assertions), 2026-11-18.

//! #3036: a kit sheet whose parts are flagged InBody keeps its title and action rows pinned and puts the
//! flagged run in a scrolling body, so a sheet at the screen cap scrolls the body instead of cutting
//! Leave and Close off. A sheet with no flagged part has no body and is built as before.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_UISheetBody : LF_Test
{
	override string GetName()
	{
		return "ui-kit/sheet-body";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		WorkspaceWidget workspace;
		if (GetGame())
			workspace = GetGame().GetWorkspace();

		ctx.NotNull(workspace, "a workspace exists to build the sheet in");
		if (!workspace)
			return;

		array<ref LF_UIPart> parts = {};
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.TITLE, "BodyTestTitle", "Ledger"));
		LF_UIPart first = LF_UIPart.Create(LF_EUIPartKind.LINE, "BodyTestFirst", "first");
		LF_UIPart second = LF_UIPart.Create(LF_EUIPartKind.LINE, "BodyTestSecond", "second");
		if (!RedProof())
		{
			first.InBody();
			second.InBody();
		}

		parts.Insert(first);
		parts.Insert(second);
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.CLOSE, "BodyTestClose", "Close"));

		LF_UIScreen screen = LF_UIScreen.Open(parts);
		ctx.NotNull(screen, "a sheet with a body assembles");
		if (!screen)
			return;

		Widget body = workspace.FindAnyWidget("Body");
		ctx.NotNull(body, "flagged parts make a scrolling body");
		if (body)
		{
			ctx.EqualStr("Content", body.GetParent().GetName(), "the body sits in the sheet's Content column");
			ctx.NotNull(body.FindAnyWidget("BodyContent"), "the body has a column for its parts");
		}

		ctx.EqualStr("BodyContent", ParentName(workspace, "BodyTestFirst"), "a flagged part sits in the body");
		ctx.EqualStr("BodyContent", ParentName(workspace, "BodyTestSecond"), "every flagged part sits in the body");
		ctx.EqualStr("Content", ParentName(workspace, "BodyTestTitle"), "the title stays pinned above the body");
		ctx.EqualStr("Content", ParentName(workspace, "BodyTestClose"), "Close stays pinned below the body");
		screen.Close();

		array<ref LF_UIPart> plain = {};
		plain.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, "BodyTestPlain", "plain"));
		LF_UIScreen plainScreen = LF_UIScreen.Open(plain);
		ctx.NotNull(plainScreen, "a sheet with no flagged part assembles");
		if (!plainScreen)
			return;

		ctx.True(workspace.FindAnyWidget("Body") == null, "a sheet with no flagged part has no body");
		ctx.EqualStr("Content", ParentName(workspace, "BodyTestPlain"), "an unflagged part stays in Content");
		plainScreen.Close();
	}

	//------------------------------------------------------------------------------------------------
	protected string ParentName(WorkspaceWidget workspace, string partName)
	{
		Widget widget = workspace.FindAnyWidget(partName);
		if (!widget || !widget.GetParent())
			return "";

		return widget.GetParent().GetName();
	}
}
