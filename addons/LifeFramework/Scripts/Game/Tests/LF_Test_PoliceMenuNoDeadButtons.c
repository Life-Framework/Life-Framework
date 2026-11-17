// red-proof: observed 2026-11-17, tools\cli test --no-build --only LF_Test_PoliceMenuNoDeadButtons, on the
// menu with its Search and Arrest buttons (focus ring of three), passed=0 failed=1:
//   "[LFTEST]   - the layout has no blank Search button"
//   "[LFTEST]   - the layout has no blank Arrest button"
//   "[LFTEST]   - the focus ring holds only Close (expected 1, got 3)"

//------------------------------------------------------------------------------------------------
//! The police duty menu shows only what works (#2509): its layout has no Search or Arrest
//! button, and every widget in its focus ring is a button that has a click handler.
// tier: WORLD
class LF_Test_PoliceMenuNoDeadButtons : LF_Test
{
	override string GetName()
	{
		return "police/menu-no-dead-buttons";
	}

	override void Run(LF_TestContext ctx)
	{
		WorkspaceWidget workspace;
		if (GetGame())
			workspace = GetGame().GetWorkspace();

		ctx.NotNull(workspace, "a workspace exists to build the menu in");
		if (!workspace)
			return;

		Widget root = workspace.CreateWidgets(LF_PoliceMenu.LAYOUT);
		ctx.NotNull(root, "the police menu layout instantiates");
		if (!root)
			return;

		ctx.True(root.FindAnyWidget("SearchButton") == null, "the layout has no blank Search button");
		ctx.True(root.FindAnyWidget("ArrestButton") == null, "the layout has no blank Arrest button");
		ctx.NotNull(root.FindAnyWidget("CloseButton"), "the layout keeps its Close button");

		array<Widget> focus = {};
		LF_PoliceMenu.BuildFocusOrder(root, focus);
		int live = 0;
		foreach (Widget entry : focus)
		{
			if (!entry)
				continue;

			live++;
			ctx.NotNull(LF_UIMenuInput.ClickInvoker(entry), "every focus-ring entry is a button with a click handler");
		}

		ctx.Equal(1, live, "the focus ring holds only Close");
		root.RemoveFromHierarchy();
	}
};
