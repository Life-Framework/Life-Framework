// red-proof: Observed before LF_RouteRules.c existed: `tools\cli test --no-build --only LF_Test_RouteRules` failed to compile ("Unknown type 'LF_RouteRules'"), so the test could not pass. Then green on the feature.
//! Route job rules (#3324): a route is an ordered list of stops. LF_RouteRules says which stop is next,
//! refuses a stop out of order or twice, counts the stops done, pays stops x rate + the round bonus at
//! the end, and pays half the rate per done stop when the route is abandoned. A route of zero stops is
//! a bad config.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_RouteRules : LF_Test
{
	override string GetName()
	{
		return "jobs/route-rules";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_RouteRules route = LF_RouteRules.Create(4, 50, 100);
		ctx.NotNull(route, "a 4-stop route with a rate and a bonus is a good config");
		if (!route)
			return;

		ctx.Equal(4, route.StopCount(), "the route has 4 stops");
		ctx.Equal(0, route.StopsDone(), "nothing is done at the start");
		ctx.Equal(0, route.NextStop(), "the first stop is next");
		ctx.False(route.IsComplete(), "a fresh route is not complete");

		// Order is enforced.
		ctx.False(route.CompleteStop(2), "stop 2 before stop 0 is refused");
		ctx.False(route.CompleteStop(1), "stop 1 before stop 0 is refused");
		ctx.False(route.CompleteStop(-1), "a negative stop is refused");
		ctx.False(route.CompleteStop(4), "a stop past the end is refused");
		ctx.Equal(0, route.StopsDone(), "refused stops count for nothing");
		ctx.Equal(0, route.NextStop(), "the first stop is still next after refusals");

		ctx.True(route.CompleteStop(0), "stop 0 in order is accepted");
		ctx.Equal(1, route.StopsDone(), "one stop done");
		ctx.Equal(1, route.NextStop(), "stop 1 is next");

		// A repeat is refused.
		ctx.False(route.CompleteStop(0), "stop 0 twice is refused");
		ctx.Equal(1, route.StopsDone(), "a repeat does not count");

		// Abandon pays half the rate for the stops done: 1 x 50 / 2.
		ctx.Equal(25, route.AbandonPay(), "abandoning after one stop pays half of one stop");
		ctx.Equal(0, route.PayAtEnd(), "a route not finished pays no end pay");

		ctx.True(route.CompleteStop(1), "stop 1 accepted");
		ctx.True(route.CompleteStop(2), "stop 2 accepted");
		ctx.Equal(75, route.AbandonPay(), "abandoning after three stops pays 3 x 50 / 2");
		ctx.False(route.IsComplete(), "three of four is not complete");
		ctx.Equal(0, route.PayAtEnd(), "three of four pays no end pay");

		ctx.True(route.CompleteStop(3), "the last stop is accepted");
		ctx.True(route.IsComplete(), "all four done is complete");
		ctx.Equal(-1, route.NextStop(), "no stop is next once complete");
		ctx.Equal(300, route.PayAtEnd(), "end pay is 4 x 50 + 100");
		ctx.False(route.CompleteStop(3), "a stop after the end is refused");
		ctx.Equal(4, route.StopsDone(), "a late stop does not count");

		// Odd rates round down in whole cash, and a nothing-done abandon pays nothing.
		LF_RouteRules odd = LF_RouteRules.Create(3, 51, 0);
		ctx.NotNull(odd, "a route with no bonus is a good config");
		if (odd)
		{
			ctx.Equal(0, odd.AbandonPay(), "abandoning before any stop pays nothing");
			odd.CompleteStop(0);
			ctx.Equal(25, odd.AbandonPay(), "one stop at rate 51 pays 25");
			odd.CompleteStop(1);
			ctx.Equal(51, odd.AbandonPay(), "two stops at rate 51 pay 51");
			odd.CompleteStop(2);
			ctx.Equal(153, odd.PayAtEnd(), "no bonus: 3 x 51");
		}

		// One stop is a route.
		LF_RouteRules single = LF_RouteRules.Create(1, 80, 20);
		ctx.NotNull(single, "a one-stop route is a good config");
		if (single)
		{
			ctx.True(single.CompleteStop(0), "the only stop is accepted");
			ctx.Equal(100, single.PayAtEnd(), "1 x 80 + 20");
		}

		// Bad configs.
		ctx.True(LF_RouteRules.Create(0, 50, 100) == null, "zero stops is a bad config");
		ctx.True(LF_RouteRules.Create(-3, 50, 100) == null, "negative stops is a bad config");
		ctx.True(LF_RouteRules.Create(4, -1, 100) == null, "a negative rate is a bad config");
		ctx.True(LF_RouteRules.Create(4, 50, -1) == null, "a negative bonus is a bad config");
	}
}
