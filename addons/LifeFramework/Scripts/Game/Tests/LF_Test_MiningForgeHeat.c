// red-proof: with `RedProof()` on the recipe is never set and no heat is started, so the nugget never turns into a bar, the
// recipe is never ready and every duration reads 0: the delay, recipe, start, wait and finish checks all fail.

//! mining/forge-heat: the timed ore forge (LF_MiningForge). The iron recipe is 1 nugget for 1 bar after 5 seconds; a heat
//! starts only with a nugget held and none already running; the nugget stays until the delay is up, then it becomes the
//! bar; a negative step never rewinds the clock; a heat that loses its nugget finishes with no bar (the nugget is never
//! made from nothing); a zero delay finishes inside Start.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_MiningForgeHeat : LF_Test
{
	override string GetName()
	{
		return "mining/forge-heat";
	}

	override void Run(LF_TestContext ctx)
	{
		int delayFive = -1;
		int delayZero = -1;
		int delayNegative = -1;
		bool blankReady = true;
		bool nullReady = true;
		bool recipeReady = false;
		int recipeDuration = -1;
		bool startWithoutNugget = true;
		bool startNull = true;
		bool started = false;
		bool heatingAfterStart = false;
		int nuggetsWhileHeating = -1;
		int barsWhileHeating = -1;
		bool startTwice = true;
		bool earlyStep = true;
		bool negativeStep = true;
		bool heatingMidway = false;
		bool lastStep = false;
		int nuggetsAfter = -1;
		int barsAfter = -1;
		bool heatingAfter = true;
		int elapsedAfter = -1;
		bool idleAdvance = true;
		bool zeroDelayStart = false;
		int zeroDelayBars = -1;
		bool starvedStep = true;
		int starvedBars = -1;
		bool starvedHeating = true;

		if (!RedProof())
		{
			delayFive = LF_MiningForge.DelayMs(5);
			delayZero = LF_MiningForge.DelayMs(0);
			delayNegative = LF_MiningForge.DelayMs(-3);

			LF_ForgeHeat blank = new LF_ForgeHeat();
			blankReady = LF_MiningForge.RecipeReady(blank);
			nullReady = LF_MiningForge.RecipeReady(null);
			LF_MiningForge.UseIronRecipe(null);

			LF_ForgeHeat heat = new LF_ForgeHeat();
			LF_MiningForge.UseIronRecipe(heat);
			recipeReady = LF_MiningForge.RecipeReady(heat);
			recipeDuration = heat.m_iDurationMs;

			startWithoutNugget = LF_MiningForge.Start(heat);
			startNull = LF_MiningForge.Start(null);
			heat.m_iNuggets = 1;
			started = LF_MiningForge.Start(heat);
			heatingAfterStart = heat.m_bHeating;
			nuggetsWhileHeating = heat.m_iNuggets;
			barsWhileHeating = heat.m_iBars;
			startTwice = LF_MiningForge.Start(heat);
			earlyStep = LF_MiningForge.Advance(heat, 4999);
			negativeStep = LF_MiningForge.Advance(heat, -1000);
			heatingMidway = heat.m_bHeating;
			lastStep = LF_MiningForge.Advance(heat, 1);
			nuggetsAfter = heat.m_iNuggets;
			barsAfter = heat.m_iBars;
			heatingAfter = heat.m_bHeating;
			elapsedAfter = heat.m_iElapsedMs;
			idleAdvance = LF_MiningForge.Advance(heat, 10000);

			LF_ForgeHeat instant = new LF_ForgeHeat();
			LF_MiningForge.UseIronRecipe(instant);
			instant.m_iDurationMs = 0;
			instant.m_iNuggets = 1;
			zeroDelayStart = LF_MiningForge.Start(instant);
			zeroDelayBars = instant.m_iBars;

			LF_ForgeHeat starved = new LF_ForgeHeat();
			LF_MiningForge.UseIronRecipe(starved);
			starved.m_iNuggets = 1;
			LF_MiningForge.Start(starved);
			starved.m_iNuggets = 0;
			starvedStep = LF_MiningForge.Advance(starved, 6000);
			starvedBars = starved.m_iBars;
			starvedHeating = starved.m_bHeating;
		}

		ctx.Equal(5000, delayFive, "five seconds wait 5000 ms");
		ctx.Equal(0, delayZero, "zero seconds wait no time");
		ctx.Equal(0, delayNegative, "a negative wait is no wait");
		ctx.False(blankReady, "a heat with no recipe is not ready");
		ctx.False(nullReady, "no heat is not ready");
		ctx.True(recipeReady, "the iron recipe is ready");
		ctx.Equal(5000, recipeDuration, "the iron heat lasts five seconds");
		ctx.False(startWithoutNugget, "no nugget, no heat");
		ctx.False(startNull, "no heat object is refused, not a crash");
		ctx.True(started, "a held nugget starts the heat");
		ctx.True(heatingAfterStart, "the heat is running");
		ctx.Equal(1, nuggetsWhileHeating, "the nugget stays in the forge while it heats");
		ctx.Equal(0, barsWhileHeating, "no bar before the delay is up");
		ctx.False(startTwice, "a second start on a running heat is refused");
		ctx.False(earlyStep, "one millisecond short of the delay makes no bar");
		ctx.False(negativeStep, "a negative step does nothing");
		ctx.True(heatingMidway, "the heat is still running just before the end");
		ctx.True(lastStep, "the last millisecond finishes the heat");
		ctx.Equal(0, nuggetsAfter, "the nugget is used up");
		ctx.Equal(1, barsAfter, "one bar comes out");
		ctx.False(heatingAfter, "a finished heat is no longer running");
		ctx.Equal(0, elapsedAfter, "a finished heat resets its clock");
		ctx.False(idleAdvance, "stepping a stopped heat makes nothing");
		ctx.True(zeroDelayStart, "a zero delay finishes inside Start");
		ctx.Equal(1, zeroDelayBars, "and the bar is there");
		ctx.False(starvedStep, "a heat that lost its nugget finishes with no bar");
		ctx.Equal(0, starvedBars, "no bar is made from nothing");
		ctx.False(starvedHeating, "and the heat is over");
	}
}
