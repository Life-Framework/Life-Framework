// red-proof: 2026-11-25, `--red LF_Test_AmbientWindow` (RedProof() skips the WindowEdge calls): failed "the generator closing at 23:00 logs one closed edge (expected '1', got '0')". The unfixed code had no window line at all.
//! The hour windows of the north outpost's beds (#4955): generator 05 to 23, fire 17 to 05 (wraps
//! midnight), radio 08 to 23, wind 21 to 05, and the window edge an emitter logs when its hour flips.
//! A `-noSound` session never starts a voice, so no `stop` line exists; the edge line is what the
//! outpost proof reads for "the generator bed ends at 23:00".
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_AmbientWindow : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "atmosphere/window";
	}

	//------------------------------------------------------------------------------------------------
	//! Sweeps `fromHour` to `toHour` in 0.25 h steps (past 24 wraps to 0) the way the mix ticks, and
	//! counts the edges `WindowEdge` reports with `want` ("open" or "closed"), not counting the first look.
	protected int CountEdges(float fromHour, float toHour, float windowFrom, float windowTo, string want)
	{
		int last = -1;
		int count = 0;
		for (float h = fromHour; h <= toHour; h += 0.25)
		{
			float hour = h;
			if (hour >= 24)
				hour -= 24;

			bool open = LF_AtmosphereMix.InHours(hour, windowFrom, windowTo);
			string edge = "";
			if (!RedProof())
				edge = LF_AtmosphereMix.WindowEdge(last, open);

			if (last != -1 && edge == want)
				count++;

			if (!edge.IsEmpty())
			{
				if (open)
					last = 1;
				else
					last = 0;
			}
		}

		return count;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		// The generator, 05 to 23.
		ctx.True(LF_AtmosphereMix.InHours(12, 5, 23), "the generator runs at noon");
		ctx.True(LF_AtmosphereMix.InHours(22.99, 5, 23), "the generator runs until 23:00");
		ctx.False(LF_AtmosphereMix.InHours(23, 5, 23), "the generator stops at 23:00");
		ctx.False(LF_AtmosphereMix.InHours(23.9, 5, 23), "the generator is off late at night");
		ctx.False(LF_AtmosphereMix.InHours(0, 5, 23), "the generator is off at midnight");
		ctx.False(LF_AtmosphereMix.InHours(4.99, 5, 23), "the generator is off before 05:00");
		ctx.True(LF_AtmosphereMix.InHours(5, 5, 23), "the generator starts at 05:00");

		// The fire, 17 to 05, wrapping midnight.
		ctx.False(LF_AtmosphereMix.InHours(12, 17, 5), "the fire is out at noon");
		ctx.True(LF_AtmosphereMix.InHours(17, 17, 5), "the fire starts at 17:00");
		ctx.True(LF_AtmosphereMix.InHours(23, 17, 5), "the fire burns at 23:00");
		ctx.True(LF_AtmosphereMix.InHours(0, 17, 5), "the fire burns at midnight");
		ctx.True(LF_AtmosphereMix.InHours(4.99, 17, 5), "the fire burns until 05:00");
		ctx.False(LF_AtmosphereMix.InHours(5, 17, 5), "the fire ends at 05:00");

		// The wind, 21 to 05, and the tent radio, 08 to 23.
		ctx.False(LF_AtmosphereMix.InHours(20.99, 21, 5), "the wind starts at 21:00");
		ctx.True(LF_AtmosphereMix.InHours(23, 21, 5), "the wind blows at 23:00");
		ctx.False(LF_AtmosphereMix.InHours(7.99, 8, 23), "the radio is off before 08:00");
		ctx.False(LF_AtmosphereMix.InHours(23, 8, 23), "the radio is off at 23:00");

		// The edge: first look, no change, flips.
		if (!RedProof())
		{
			ctx.EqualStr("open", LF_AtmosphereMix.WindowEdge(-1, true), "the first look at an open window reports open");
			ctx.EqualStr("closed", LF_AtmosphereMix.WindowEdge(-1, false), "the first look at a closed window reports closed");
			ctx.EqualStr("", LF_AtmosphereMix.WindowEdge(1, true), "an open window that stays open is quiet");
			ctx.EqualStr("", LF_AtmosphereMix.WindowEdge(0, false), "a closed window that stays closed is quiet");
			ctx.EqualStr("closed", LF_AtmosphereMix.WindowEdge(1, false), "an open window that closes reports closed");
			ctx.EqualStr("open", LF_AtmosphereMix.WindowEdge(0, true), "a closed window that opens reports open");
		}

		// The clock crossing 23:00: the generator reports closed once, the fire stays open and stays quiet.
		ctx.Equal(1, CountEdges(22, 24, 5, 23, "closed"), "the generator closing at 23:00 logs one closed edge");
		ctx.Equal(0, CountEdges(22, 24, 17, 5, "closed"), "the fire does not close across 23:00");
		// The clock crossing midnight and 05:00: the fire closes once at 05:00, the generator opens once.
		ctx.Equal(1, CountEdges(23, 29, 17, 5, "closed"), "the fire closing at 05:00 logs one closed edge across midnight");
		ctx.Equal(1, CountEdges(23, 29, 5, 23, "open"), "the generator opening at 05:00 logs one open edge");
	}
}
