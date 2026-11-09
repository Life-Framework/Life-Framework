// red-proof: with ClientTitleRect always returning font 96, `tools\cli test --no-build
// --only LF_Test_SignFaceClient` (2026-11-08) failed signs/face-client with
// "the longest word sets the font (expected 66, got 96)".

// tier: LOGIC
class LF_Test_SignFaceClient : LF_Test
{
	override string GetName()
	{
		return "signs/face-client";
	}

	override void Run(LF_TestContext ctx)
	{
		// A game client shows about 328 of the target's 512 units across the
		// paper (seen at 640x360 and 1920x1080): the box must sit inside that.
		vector pos;
		vector size;
		int font = LF_SignComponent.ClientTitleRect("ATM", pos, size);
		ctx.True(pos[0] + size[0] <= 328, "the client title box ends inside the shown part of the sheet");
		ctx.Equal(96, font, "a short word keeps the layout font");

		// "WELCOME" at 96 measured 406 units, wider than the paper: the font shrinks.
		font = LF_SignComponent.ClientTitleRect("WELCOME TO EVERON", pos, size);
		ctx.Equal(66, font, "the longest word sets the font");
		ctx.True(406.0 * font / 96.0 <= size[0], "WELCOME fits the title box at that font");

		font = LF_SignComponent.ClientTitleRect("ORGANIZATION OFFICE", pos, size);
		ctx.Equal(38, font, "a twelve-letter word fits at 38");
		ctx.Equal(96, LF_SignComponent.ClientTitleRect("", pos, size), "an empty title keeps the layout font");
	}
}
