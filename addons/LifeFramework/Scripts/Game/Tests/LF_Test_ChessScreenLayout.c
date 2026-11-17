// red-proof: new test. `tools\cli test --no-build --red LF_Test_ChessScreenLayout` skips LF_ChessScreen.FitBoard()
// behind the RedProof() hook: the kit's pause-menu button keeps its 40 pixel label padding and its
// 64 pixel arrow glyph, so every key's label row and glyph assertion fails (#2099, #2056).

// tier: WORLD
//! #2099: the chess board's 64 keys each carry their own label, centred. A row is eight keys of
//! equal width (one eighth each), every key's label widget sits inside that key's own button, and the
//! kit button's left padding and arrow glyph, which pushed a label one key to the right, are gone.
class LF_Test_ChessScreenLayout : LF_Test
{
	override string GetName()
	{
		return "chess/screen-layout";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		WorkspaceWidget workspace;
		if (GetGame())
			workspace = GetGame().GetWorkspace();

		ctx.NotNull(workspace, "a workspace exists to build the board in");
		if (!workspace)
			return;

		LF_UISkin skin = LF_ChessScreen.BoardSkin();
		ctx.True(skin.m_fHeight > 0, "the board sheet has a height of its own");
		ctx.True(skin.m_fWidth >= 8 * 64, "the sheet is wide enough for eight keys");

		LF_UIScreen screen = LF_UIScreen.Open(LF_ChessScreen.BuildParts(false), skin);
		ctx.NotNull(screen, "the chess board assembles");
		if (!screen)
			return;

		if (!RedProof())
			ctx.Equal(64, LF_ChessScreen.FitBoard(), "all 64 keys are fitted");

		for (int rank = 1; rank <= 8; rank++)
			CheckRow(ctx, workspace, rank);

		screen.Close();
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckRow(LF_TestContext ctx, WorkspaceWidget workspace, int rank)
	{
		string rowName = "ChessRank" + rank.ToString();
		Widget row = workspace.FindAnyWidget(rowName);
		ctx.NotNull(row, rowName + " is on the board");
		if (!row)
			return;

		int keys = 0;
		for (Widget key = row.GetChildren(); key; key = key.GetSibling())
		{
			CheckKey(ctx, key, rowName);
			keys++;
		}

		ctx.Equal(8, keys, rowName + " holds eight keys");
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckKey(LF_TestContext ctx, Widget key, string rowName)
	{
		string what = rowName + " key " + key.GetName();

		// One eighth of the row each: every key fills, with the same weight.
		ctx.True(LayoutSlot.GetSizeMode(key) == LayoutSizeMode.Fill, what + " fills its share of the row");
		ctx.EqualFloat(1, LayoutSlot.GetFillWeight(key), 0.001, what + " has the same weight as its neighbours");

		// The label belongs to this key's own button, not a neighbour's.
		Widget text = key.FindAnyWidget("Text");
		ctx.NotNull(text, what + " has a label widget");
		if (text)
		{
			Widget owner = text;
			while (owner && owner.GetParent() != key.GetParent())
				owner = owner.GetParent();
			ctx.True(owner == key, what + " owns its label");
		}

		// No leftover padding or arrow glyph in front of the label.
		Widget layout = key.FindAnyWidget("HorizontalLayout0");
		ctx.NotNull(layout, what + " has a label row");
		if (layout)
		{
			float left;
			float top;
			float right;
			float bottom;
			AlignableSlot.GetPadding(layout, left, top, right, bottom);
			ctx.EqualFloat(0, left + right, 0.001, what + " has no padding beside its label");
			ctx.True(AlignableSlot.GetHorizontalAlign(layout) == LayoutHorizontalAlign.Center, what + " centres its label");
		}

		Widget glyph = key.FindAnyWidget("Scale0");
		if (glyph)
			ctx.False(glyph.IsVisible(), what + " has no arrow glyph before its label");
	}
}
