// red-proof: 2026-11-17, cli test --no-build --red LF_Test_MapAreasDraw: RED-OK (RedProof() skips the far-zoom Build, so the fill, outline and token checks fail). Green without it.

//------------------------------------------------------------------------------------------------
//! map/areas-draw (#2234, epic #2223, card M6): the canvas records for a fixture area at two zooms.
//! A circle safe zone and a lawless one become a fill, an outline and, at the close zoom only, a label;
//! a dashed outline is cut into dashes; an area off the canvas or too small draws nothing; the theme's
//! `map-*` tokens recolour it. Pure script: the builder takes the screen outline, no map, no widgets.
// tier: LOGIC
class LF_Test_MapAreasDraw : LF_Test
{
	protected static const float VIEW_W = 1280;
	protected static const float VIEW_H = 720;

	override string GetName()
	{
		return "map/areas-draw";
	}

	override void Run(LF_TestContext ctx)
	{
		RunZooms(ctx);
		RunHatchAndDash(ctx);
		RunCull(ctx);
		RunTokens(ctx);
	}

	//------------------------------------------------------------------------------------------------
	//! A circle area of `radius` metres at the world point (x, z), as the screen outline at `ppu` pixels per
	//! metre, the view centre at world (0, 0).
	protected LF_MapArea MakeArea(LF_MapAreaStyle style, float x, float z, float radius, string label)
	{
		LF_MapArea area = new LF_MapArea();
		area.m_sName = "fixture";
		area.m_eStyle = style;
		area.m_sLabel = label;
		LF_MapAreas.CircleOutline(Vector(x, 0, z), radius, LF_MapAreas.DEFAULT_SEGMENTS, area.m_aOutline);
		return area;
	}

	//------------------------------------------------------------------------------------------------
	protected void ToScreen(notnull LF_MapArea area, float ppu, notnull array<vector> screen)
	{
		screen.Clear();
		foreach (vector world : area.m_aOutline)
			screen.Insert(Vector(VIEW_W / 2 + world[0] * ppu, VIEW_H / 2 - world[2] * ppu, 0));
	}

	//------------------------------------------------------------------------------------------------
	protected int CountOf(notnull array<ref LF_MapAreaCmd> cmds, LF_MapAreaCmdKind kind)
	{
		int count;
		foreach (LF_MapAreaCmd cmd : cmds)
		{
			if (cmd.m_eKind == kind)
				count++;
		}

		return count;
	}

	//------------------------------------------------------------------------------------------------
	protected LF_MapAreaCmd FirstOf(notnull array<ref LF_MapAreaCmd> cmds, LF_MapAreaCmdKind kind)
	{
		foreach (LF_MapAreaCmd cmd : cmds)
		{
			if (cmd.m_eKind == kind)
				return cmd;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! The safe zone at the far and the close zoom.
	protected void RunZooms(LF_TestContext ctx)
	{
		LF_MapArea safe = MakeArea(LF_MapAreaStyle.SAFE, 0, 0, 40, string.Empty);
		array<vector> screen = {};

		// Far: 0.1 px per metre, a 4 px circle at a quarter of the span of a screen side. 40 m is 4 px,
		// so the far view uses 0.2 (8 px): visible, below the label zoom.
		ToScreen(safe, 0.2, screen);
		array<ref LF_MapAreaCmd> far = {};
		int added;
		if (!RedProof())
			added = LF_MapAreasDraw.Build(safe, screen, 0.2, VIEW_W, VIEW_H, string.Empty, "Safe zone", far);

		ctx.Equal(2, added, "far zoom: a fill and an outline");
		ctx.Equal(1, CountOf(far, LF_MapAreaCmdKind.FILL), "far zoom: one fill");
		ctx.Equal(1, CountOf(far, LF_MapAreaCmdKind.OUTLINE), "far zoom: a solid outline is one closed line");
		ctx.Equal(0, CountOf(far, LF_MapAreaCmdKind.LABEL), "far zoom: no label under the label zoom");

		LF_MapAreaCmd fill = FirstOf(far, LF_MapAreaCmdKind.FILL);
		ctx.NotNull(fill, "the fill exists");
		if (fill)
		{
			ctx.EqualStr("map-area-safe-fill", fill.m_sToken, "the fill uses the safe fill token");
			ctx.EqualStr("none", fill.m_sHatch, "a safe zone is a wash, no hatch");
			ctx.Equal(38, LF_MapAreasDraw.AlphaOf(fill.m_iColor), "the safe wash is 15 percent");
			ctx.Equal(LF_MapAreas.DEFAULT_SEGMENTS * 2, fill.m_aVertices.Count(), "the fill is the 32-gon, x and y each");
		}

		LF_MapAreaCmd line = FirstOf(far, LF_MapAreaCmdKind.OUTLINE);
		if (line)
		{
			ctx.True(line.m_bClosed, "a solid outline closes on itself");
			ctx.EqualFloat(2, line.m_fWidth, 0.001, "the safe line is 2 wide");
			ctx.EqualStr("map-area-safe-line", line.m_sToken, "the outline uses the safe line token");
			ctx.Equal(255, LF_MapAreasDraw.AlphaOf(line.m_iColor), "the outline is opaque");
		}

		// Close: 1 px per metre, the same area is 80 px across and carries a label at its middle.
		ToScreen(safe, 1.0, screen);
		array<ref LF_MapAreaCmd> near = {};
		LF_MapAreasDraw.Build(safe, screen, 1.0, VIEW_W, VIEW_H, string.Empty, "Safe zone", near);
		ctx.Equal(1, CountOf(near, LF_MapAreaCmdKind.LABEL), "close zoom: the label shows");
		LF_MapAreaCmd label = FirstOf(near, LF_MapAreaCmdKind.LABEL);
		if (label)
		{
			ctx.EqualStr("Safe zone", label.m_sText, "the label is the text given");
			ctx.True(label.m_aVertices[0] < VIEW_W / 2, "the label starts left of the centre so it is centred");
			ctx.EqualFloat(VIEW_H / 2 - LF_MapAreasDraw.LABEL_SIZE / 2, label.m_aVertices[1], 0.01, "the label sits at the centre height");
		}

		// An area with no label text shows none at any zoom.
		array<ref LF_MapAreaCmd> unlabelled = {};
		LF_MapAreasDraw.Build(safe, screen, 1.0, VIEW_W, VIEW_H, string.Empty, string.Empty, unlabelled);
		ctx.Equal(0, CountOf(unlabelled, LF_MapAreaCmdKind.LABEL), "no label text, no label");
	}

	//------------------------------------------------------------------------------------------------
	//! Lawless: a hatched fill, a heavy dash. The town line: no fill, a thin dash. Solid when a huge area
	//! would need too many dashes.
	protected void RunHatchAndDash(LF_TestContext ctx)
	{
		LF_MapArea lawless = MakeArea(LF_MapAreaStyle.LAWLESS, 0, 0, 60, string.Empty);
		array<vector> screen = {};
		ToScreen(lawless, 1.0, screen);
		array<ref LF_MapAreaCmd> cmds = {};
		LF_MapAreasDraw.Build(lawless, screen, 1.0, VIEW_W, VIEW_H, string.Empty, "No police", cmds);

		LF_MapAreaCmd fill = FirstOf(cmds, LF_MapAreaCmdKind.FILL);
		ctx.NotNull(fill, "lawless has a fill");
		if (fill)
		{
			ctx.EqualStr("diagonal", fill.m_sHatch, "lawless is a diagonal hatch");
			ctx.Equal(64, LF_MapAreasDraw.AlphaOf(fill.m_iColor), "the lawless hatch is 25 percent");
			ctx.False(LF_MapAreasDraw.HatchTexture(fill.m_sHatch).IsEmpty(), "the diagonal hatch has a core texture");
		}

		float perimeter = LF_MapAreasDraw.Perimeter(screen);
		int dashes = CountOf(cmds, LF_MapAreaCmdKind.OUTLINE);
		int expected = Math.Round(perimeter / (LF_MapAreasDraw.HEAVY_ON + LF_MapAreasDraw.HEAVY_OFF));
		ctx.InRange(dashes, expected - 1, expected + 1, "the heavy dash cuts the outline into about perimeter / 26 dashes");
		LF_MapAreaCmd dash = FirstOf(cmds, LF_MapAreaCmdKind.OUTLINE);
		if (dash)
		{
			ctx.False(dash.m_bClosed, "a dash is an open line");
			ctx.EqualFloat(4, dash.m_fWidth, 0.001, "the lawless line is heavy");
			ctx.EqualFloat(LF_MapAreasDraw.HEAVY_ON, PolylineLength(dash.m_aVertices), 0.5, "a dash is the heavy dash length");
		}

		// The same area further out has fewer dashes: the dash is in screen pixels, not metres.
		ToScreen(lawless, 0.5, screen);
		array<ref LF_MapAreaCmd> half = {};
		LF_MapAreasDraw.Build(lawless, screen, 0.5, VIEW_W, VIEW_H, string.Empty, "No police", half);
		ctx.True(CountOf(half, LF_MapAreaCmdKind.OUTLINE) < dashes, "at a lower zoom the outline has fewer dashes");

		// The town line: dashed, no fill.
		LF_MapArea town = MakeArea(LF_MapAreaStyle.TOWN, 0, 0, 100, string.Empty);
		ToScreen(town, 1.0, screen);
		array<ref LF_MapAreaCmd> townCmds = {};
		LF_MapAreasDraw.Build(town, screen, 1.0, VIEW_W, VIEW_H, string.Empty, string.Empty, townCmds);
		ctx.Equal(0, CountOf(townCmds, LF_MapAreaCmdKind.FILL), "the town line has no fill");
		ctx.True(CountOf(townCmds, LF_MapAreaCmdKind.OUTLINE) > 4, "the town line is dashed");

		// A gated area is a cross hatch with a solid line.
		LF_MapArea gated = MakeArea(LF_MapAreaStyle.GATED, 0, 0, 50, string.Empty);
		ToScreen(gated, 1.0, screen);
		array<ref LF_MapAreaCmd> gatedCmds = {};
		LF_MapAreasDraw.Build(gated, screen, 1.0, VIEW_W, VIEW_H, string.Empty, string.Empty, gatedCmds);
		LF_MapAreaCmd gatedFill = FirstOf(gatedCmds, LF_MapAreaCmdKind.FILL);
		if (gatedFill)
			ctx.EqualStr("cross", gatedFill.m_sHatch, "a gated area is a cross hatch");
		ctx.Equal(1, CountOf(gatedCmds, LF_MapAreaCmdKind.OUTLINE), "a gated outline is solid");

		// A huge dashed outline falls back to solid instead of thousands of commands.
		LF_MapArea huge = MakeArea(LF_MapAreaStyle.LAWLESS, 0, 0, 100000, string.Empty);
		ToScreen(huge, 1.0, screen);
		array<ref LF_MapAreaCmd> hugeCmds = {};
		LF_MapAreasDraw.Build(huge, screen, 1.0, VIEW_W, VIEW_H, string.Empty, string.Empty, hugeCmds);
		ctx.Equal(1, CountOf(hugeCmds, LF_MapAreaCmdKind.OUTLINE), "a huge outline is one solid line, not thousands of dashes");
	}

	//------------------------------------------------------------------------------------------------
	protected float PolylineLength(notnull array<float> vertices)
	{
		float total;
		for (int i = 0; i + 3 < vertices.Count(); i += 2)
			total += Math.Sqrt(Math.Pow(vertices[i + 2] - vertices[i], 2) + Math.Pow(vertices[i + 3] - vertices[i + 1], 2));

		return total;
	}

	//------------------------------------------------------------------------------------------------
	protected void RunCull(LF_TestContext ctx)
	{
		array<vector> screen = {};
		LF_MapArea area = MakeArea(LF_MapAreaStyle.SAFE, 5000, 0, 40, string.Empty);
		ToScreen(area, 1.0, screen);
		array<ref LF_MapAreaCmd> off = {};
		ctx.Equal(0, LF_MapAreasDraw.Build(area, screen, 1.0, VIEW_W, VIEW_H, string.Empty, "x", off), "an area off the canvas draws nothing");

		LF_MapArea speck = MakeArea(LF_MapAreaStyle.SAFE, 0, 0, 1, string.Empty);
		ToScreen(speck, 1.0, screen);
		array<ref LF_MapAreaCmd> small = {};
		ctx.Equal(0, LF_MapAreasDraw.Build(speck, screen, 1.0, VIEW_W, VIEW_H, string.Empty, "x", small), "an area a few pixels across draws nothing");

		array<vector> two = {Vector(1, 1, 0), Vector(5, 5, 0)};
		array<ref LF_MapAreaCmd> bad = {};
		ctx.Equal(0, LF_MapAreasDraw.Build(area, two, 1.0, VIEW_W, VIEW_H, string.Empty, "x", bad), "fewer than three screen points draws nothing");
	}

	//------------------------------------------------------------------------------------------------
	//! The theme's map tokens recolour an area; a name left out keeps the built-in.
	protected void RunTokens(LF_TestContext ctx)
	{
		string themed = "map-area-safe-fill=#102030; map-area-safe-line=#FF0000";
		ctx.EqualStr("#102030", LF_MapAreasDraw.HexOf("map-area-safe-fill", themed), "the theme names the safe fill");
		ctx.EqualStr("#A02820", LF_MapAreasDraw.HexOf("map-area-lawless-line", themed), "a token the theme leaves out keeps its built-in");
		ctx.EqualStr("#468250", LF_MapAreasDraw.HexOf("map-area-safe-fill", "map-area-safe-fill=nonsense"), "a malformed theme value falls back to the built-in");
		ctx.EqualStr(string.Empty, LF_MapAreasDraw.BuiltInHex("map-nothing"), "an unknown token has no colour");

		// 15 percent of #102030 is alpha 38, red 0x10.
		int color = LF_MapAreasDraw.ColorOf("map-area-safe-fill", 15, themed);
		ctx.Equal(38, LF_MapAreasDraw.AlphaOf(color), "the percent is the alpha byte");
		ctx.Equal(0x10, (color >> 16) & 0xFF, "the red byte is the theme's");
		ctx.Equal(0x20, (color >> 8) & 0xFF, "the green byte is the theme's");
		ctx.Equal(0x30, color & 0xFF, "the blue byte is the theme's");

		LF_MapArea safe = MakeArea(LF_MapAreaStyle.SAFE, 0, 0, 40, string.Empty);
		array<vector> screen = {};
		ToScreen(safe, 1.0, screen);
		array<ref LF_MapAreaCmd> cmds = {};
		LF_MapAreasDraw.Build(safe, screen, 1.0, VIEW_W, VIEW_H, themed, string.Empty, cmds);
		LF_MapAreaCmd line = FirstOf(cmds, LF_MapAreaCmdKind.OUTLINE);
		if (line)
			ctx.Equal(0xFF, (line.m_iColor >> 16) & 0xFF, "a themed line colour reaches the outline");

		// Every style's tokens resolve to a colour (core ships them all).
		array<LF_MapAreaStyle> styles = {LF_MapAreaStyle.SAFE, LF_MapAreaStyle.LAWLESS, LF_MapAreaStyle.TOWN, LF_MapAreaStyle.GATED, LF_MapAreaStyle.EVENT, LF_MapAreaStyle.CUSTOM};
		foreach (LF_MapAreaStyle style : styles)
		{
			LF_MapAreaLook look = LF_MapAreas.LookOf(style);
			ctx.False(LF_MapAreasDraw.BuiltInHex(look.m_sFillToken).IsEmpty(), "style " + typename.EnumToString(LF_MapAreaStyle, style) + " fill token has a colour");
			ctx.False(LF_MapAreasDraw.BuiltInHex(look.m_sLineToken).IsEmpty(), "style " + typename.EnumToString(LF_MapAreaStyle, style) + " line token has a colour");
		}
	}
}
