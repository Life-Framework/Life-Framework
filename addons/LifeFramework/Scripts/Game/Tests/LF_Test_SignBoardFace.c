// red-proof: with BoardTitleFit returning BOARD_FONT_MAX before wrapping, `tools\cli test --no-build
// --only LF_Test_SignBoardFace` (2026-11-08) failed signs/board-face with
// "STATION across the box sets the font (expected 148, got 180)".

// tier: LOGIC
//! The painted board face: a title is broken at spaces and set as big as the
//! board's title box allows, so it reads from the road.
class LF_Test_SignBoardFace : LF_Test
{
	override string GetName()
	{
		return "signs/board-face";
	}

	override void Run(LF_TestContext ctx)
	{
		string lines;
		vector pos;
		vector size;

		int font = LF_SignComponent.BoardTitleFit("POLICE STATION", lines, pos, size);
		ctx.EqualStr("POLICE\nSTATION", lines, "a two-word title takes a line per word");
		ctx.Equal(148, font, "STATION across the box sets the font");

		// The box is centred on the shown part of the sheet (512 x 724 units) before it turns.
		ctx.EqualFloat(256, pos[0] + size[0] * 0.5, 0.01, "the title box is centred across the shown sheet");
		ctx.EqualFloat(362, pos[1] + size[1] * 0.5, 0.01, "the title box is centred along the shown sheet");
		ctx.True(size[1] <= LF_SignComponent.BOARD_SHOWN_WIDTH, "turned, the box height fits across the sheet");
		ctx.True(size[0] <= LF_SignComponent.BOARD_SHOWN_HEIGHT, "turned, the box width fits along the sheet");

		font = LF_SignComponent.BoardTitleFit("ATM", lines, pos, size);
		ctx.EqualStr("ATM", lines, "a one-word title stays on one line");
		ctx.Equal(LF_SignComponent.BOARD_FONT_MAX, font, "a short title takes the largest font");

		font = LF_SignComponent.BoardTitleFit("WELCOME TO EVERON", lines, pos, size);
		ctx.EqualStr("WELCOME\nTO\nEVERON", lines, "three lines beat two smaller ones");
		ctx.Equal(121, font, "three lines fit the box height at 121");
		ctx.True(3 * font * LF_SignComponent.BOARD_LINE_HEIGHT <= size[1], "the three lines fit the box height");

		font = LF_SignComponent.BoardTitleFit("ORGANIZATION OFFICE", lines, pos, size);
		ctx.EqualStr("ORGANIZATION\nOFFICE", lines, "the long word gets its own line");
		ctx.Equal(86, font, "a twelve-letter word fits at 86");

		font = LF_SignComponent.BoardTitleFit("", lines, pos, size);
		ctx.EqualStr("", lines, "an empty title prints nothing");
	}
}
