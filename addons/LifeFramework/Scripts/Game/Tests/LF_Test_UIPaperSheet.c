// red-proof: observed 2026-11-08, tools\cli test --no-build --only LF_Test_UIPaperSheet, on the
// frame and skins as #162 left them (Paper a PanelWidget, paper alpha 242, blur inside the sheet),
// passed=0 failed=1:
//   "[LFTEST]   - the paper sheet is a solid image, not a styled panel"
//   "[LFTEST]   - the blur covers the screen, not the inside of the sheet"
//   "[LFTEST]   - paper paper is opaque (expected 1, got 0.94902)"
//   "[LFTEST]   - the price list keeps the kit's dimmed backdrop"

//------------------------------------------------------------------------------------------------
//! The kit sheet is solid: Metal, Paper, Rule and the row plates are untextured ImageWidgets
//! (a PanelWidget draws the engine's default-style panel image, which reads as see-through),
//! every skin's paper is opaque, and the blur dims the whole world behind the sheet instead of
//! sitting inside it.
// tier: WORLD
class LF_Test_UIPaperSheet : LF_Test
{
	override string GetName()
	{
		return "ui-kit/paper-sheet";
	}

	override void Run(LF_TestContext ctx)
	{
		WorkspaceWidget workspace;
		if (GetGame())
			workspace = GetGame().GetWorkspace();

		ctx.NotNull(workspace, "a workspace exists to build the frame in");
		if (!workspace)
			return;

		Widget root = workspace.CreateWidgets(LF_UITheme.FRAME_LAYOUT);
		ctx.NotNull(root, "the kit frame instantiates");
		if (!root)
			return;

		ctx.NotNull(ImageWidget.Cast(root.FindAnyWidget("Metal")), "the metal rim is a solid image, not a styled panel");
		ctx.NotNull(ImageWidget.Cast(root.FindAnyWidget("Paper")), "the paper sheet is a solid image, not a styled panel");
		ctx.NotNull(ImageWidget.Cast(root.FindAnyWidget("Rule")), "the rule is a solid image, not a styled panel");
		ctx.NotNull(root.FindAnyWidget("Sheet"), "the frame has a Sheet to size");
		ctx.NotNull(root.FindAnyWidget("Content"), "the frame has a Content column");

		Widget blur = root.FindAnyWidget("BackgroundBlur");
		ctx.NotNull(blur, "the frame has a background blur");
		if (blur)
		{
			ctx.True(blur.GetParent() == root, "the blur covers the screen, not the inside of the sheet");
			ctx.True(root.GetChildren() == blur, "the blur draws first, behind the sheet");
		}

		root.RemoveFromHierarchy();

		CheckPlate(ctx, workspace, LF_UITheme.ROW_LAYOUT, "list row");
		CheckPlate(ctx, workspace, LF_UITheme.STATUS_LAYOUT, "status row");

		CheckSkin(ctx, LF_UISkin.Paper());
		CheckSkin(ctx, LF_UISkin.PriceList());
		CheckSkin(ctx, LF_UISkin.Terminal());

		ctx.True(LF_UISkin.PriceList().m_bBlur, "the price list keeps the kit's dimmed backdrop");

		// A blank is named by an ink caption above it; the vanilla placeholder is light grey.
		array<ref LF_UIPart> parts = {};
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.FIELD, "PaperTestName", "Given name"));
		LF_UIScreen screen = LF_UIScreen.Open(parts);
		ctx.NotNull(screen, "a sheet with one blank assembles");
		if (!screen)
			return;

		TextWidget caption = TextWidget.Cast(workspace.FindAnyWidget("PaperTestNameCaption"));
		ctx.NotNull(caption, "the blank has a printed caption");
		if (caption)
			ctx.EqualStr("Given name", caption.GetText(), "the caption names the blank");

		screen.Close();
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckPlate(LF_TestContext ctx, WorkspaceWidget workspace, string layout, string what)
	{
		Widget row = workspace.CreateWidgets(layout);
		ctx.NotNull(row, "the " + what + " instantiates");
		if (!row)
			return;

		ctx.NotNull(ImageWidget.Cast(row.FindAnyWidget("RowPlate")), "the " + what + " plate is a solid image");
		row.RemoveFromHierarchy();
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckSkin(LF_TestContext ctx, LF_UISkin skin)
	{
		ctx.EqualFloat(1, skin.m_Paper.A(), 0.001, skin.m_sName + " paper is opaque");
		ctx.EqualFloat(1, skin.m_Metal.A(), 0.001, skin.m_sName + " rim is opaque");
		ctx.EqualFloat(1, skin.m_Row.A(), 0.001, skin.m_sName + " row plate is opaque");

		// WCAG contrast of ink on the sheet; 7:1 is the AAA line for body text.
		float paper = skin.m_Paper.GetLuminance();
		float ink = skin.m_Ink.GetLuminance();
		float contrast = (Math.Max(paper, ink) + 0.05) / (Math.Min(paper, ink) + 0.05);
		ctx.True(contrast >= 7, skin.m_sName + " ink reads at 7:1 or better on its sheet (" + contrast.ToString() + ")");

		// Button labels sit on the same sheet; vanilla white vanishes on paper.
		float button = skin.m_ButtonText.GetLuminance();
		float buttonContrast = (Math.Max(paper, button) + 0.05) / (Math.Min(paper, button) + 0.05);
		ctx.True(skin.m_bRecolorButtons, skin.m_sName + " paints its button labels");
		ctx.True(buttonContrast >= 4.5, skin.m_sName + " button labels read at 4.5:1 or better (" + buttonContrast.ToString() + ")");
	}
}
