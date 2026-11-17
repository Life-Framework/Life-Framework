// red-proof: 2026-11-17, before the fix there was no LF_GatherStock and the apple row had m_iMaxGathers 0: a spot gathered without end. With the fix reverted (Take always true) `--only LF_Test_GatherStock` fails "an empty spot refuses a gather".
// tier: LOGIC
//! gather/stock-rules (#2409): a spot holds a capped stock, an empty one refuses, and one gather
//! grows back per regrow interval, on the server's clock.
class LF_Test_GatherStock : LF_Test
{
	override string GetName()
	{
		return "gather/stock-rules";
	}

	override void Run(LF_TestContext ctx)
	{
		// 3 gathers, one back per 100 s.
		LF_GatherStock stock = LF_GatherStock.Create(3, 100000, 0);
		ctx.Equal(3, stock.Available(0), "a new spot starts full");
		ctx.True(stock.Take(0), "first gather");
		ctx.True(stock.Take(0), "second gather");
		ctx.True(stock.Take(0), "third gather");
		ctx.False(stock.Take(0), "an empty spot refuses a gather");
		ctx.Equal(0, stock.Available(50000), "nothing is back before the interval");
		ctx.False(stock.Take(99000), "still empty just before the interval");
		ctx.InRange(stock.MsUntilNext(60000), 39999, 40001, "the wait shown is what is left of the interval");

		ctx.True(stock.Take(100000), "one gather is back after one interval");
		ctx.False(stock.Take(100000), "only one came back");

		// Regrowth never exceeds the cap, and a long idle spot is full.
		ctx.Equal(3, stock.Available(10000000), "a long wait refills to the cap, not past it");
		ctx.Equal(0, stock.MsUntilNext(10000000), "a full spot has no wait");

		// A spot taken from while full starts its clock at that take, not at world start.
		LF_GatherStock late = LF_GatherStock.Create(2, 100000, 0);
		ctx.True(late.Take(500000), "first take, long after creation");
		ctx.True(late.Take(500000), "second take");
		ctx.False(late.Take(550000), "no free regrowth from the time the spot sat full");
		ctx.True(late.Take(600000), "one back 100 s after the first take");

		// Two intervals at once give two back.
		LF_GatherStock burst = LF_GatherStock.Create(5, 1000, 0);
		int i;
		for (i = 0; i < 5; i++)
			burst.Take(0);
		ctx.Equal(2, burst.Available(2500), "two intervals regrow two gathers");

		// No regrow time: a spent spot stays spent. No cap: nothing to take.
		LF_GatherStock never = LF_GatherStock.Create(1, 0, 0);
		ctx.True(never.Take(0), "one-gather spot gathers once");
		ctx.False(never.Take(99999999), "no regrow time means no regrowth");
		ctx.False(LF_GatherStock.Create(0, 1000, 0).Take(0), "a zero-cap spot is always empty");
	}
};
