// red-proof: Observed 2026-11-16 with `toolsi test --no-build --red LF_Test_DialogueFocusStep`: RED-OK, with the hook forgetting the held key Down held 4 ticks fails `Down held several ticks moves focus one row (expected 1, got 0)`; green without it.
// tier: LOGIC
//! #1715: MenuDown then MenuUp in the clerk dialogue returns the focus to the first reply. A key
//! held across several ticks (the harness holds one 300 ms) steps once, not once per tick.
class LF_Test_DialogueFocusStep : LF_Test
{
	override string GetName()
	{
		return "dialogue/focus-step";
	}

	//------------------------------------------------------------------------------------------------
	//! One key held for `ticks` ticks over `rows` replies, the way LF_DialogueScene.Tick reads it.
	protected int Hold(int focus, int rows, bool down, int ticks)
	{
		bool held = false;
		for (int i = 0; i < ticks; i++)
		{
			int step = LF_DialogueScene.ReadStep(down, !down, held);
			if (!RedProof())
				held = true;

			focus = (focus + step + rows) % rows;
		}

		return focus;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		int focus = Hold(0, 2, true, 4);
		ctx.Equal(1, focus, "Down held several ticks moves focus one row");
		focus = Hold(focus, 2, false, 4);
		ctx.Equal(0, focus, "Up afterwards returns focus to the first reply");
		ctx.Equal(0, LF_DialogueScene.ReadStep(false, false, false), "no key, no step");
		ctx.Equal(1, LF_DialogueScene.ReadStep(true, false, false), "a fresh Down steps +1");
		ctx.Equal(-1, LF_DialogueScene.ReadStep(false, true, false), "a fresh Up steps -1");
	}
}
