// red-proof: 2026-11-12 made LF_RateLimiter.Take admit one past the burst (`m_iCount <= burst`) and ran
// `tools\cli test --no-build --only LF_Test_RateLimit`. Observed:
//   [LFTEST] FAIL ratelimit/window
//   [LFTEST]   - fourth request in the window refused and reported (expected 1, got 0)
//   [LFTEST]   - burst of one: the second request is refused (expected 1, got 0)
// Restored `m_iCount < burst`; green.

// tier: LOGIC
//! Request rate limit (#693b): a fixed window admits a burst, the next request is refused (first
//! refusal reported, the rest counted), a new window admits again and reports the count, buckets
//! are per player and per kind, a clock wrap opens a new window, and every kind has a sane limit.
class LF_Test_RateLimit : LF_Test
{
	override string GetName()
	{
		return "ratelimit/window";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_RateLimiter limiter = new LF_RateLimiter();
		int refused;

		// Burst of 3 per 1000 ms for player 1, purchases.
		ctx.Equal(LF_ERateVerdict.ALLOW, limiter.Take(1, LF_ERequestKind.PURCHASE, 100, 1000, 3, refused), "first request admitted");
		ctx.Equal(LF_ERateVerdict.ALLOW, limiter.Take(1, LF_ERequestKind.PURCHASE, 200, 1000, 3, refused), "second request admitted");
		ctx.Equal(LF_ERateVerdict.ALLOW, limiter.Take(1, LF_ERequestKind.PURCHASE, 300, 1000, 3, refused), "third request (the burst) admitted");
		ctx.Equal(LF_ERateVerdict.REJECT, limiter.Take(1, LF_ERequestKind.PURCHASE, 400, 1000, 3, refused), "fourth request in the window refused and reported");
		ctx.Equal(LF_ERateVerdict.REJECT_REPEAT, limiter.Take(1, LF_ERequestKind.PURCHASE, 500, 1000, 3, refused), "fifth request refused quietly");
		ctx.Equal(LF_ERateVerdict.REJECT_REPEAT, limiter.Take(1, LF_ERequestKind.PURCHASE, 1099, 1000, 3, refused), "still refused one ms before the window ends");
		ctx.Equal(0, refused, "no count is reported inside the window");

		// Other players and other kinds have their own buckets.
		ctx.Equal(LF_ERateVerdict.ALLOW, limiter.Take(2, LF_ERequestKind.PURCHASE, 500, 1000, 3, refused), "another player is not limited by player 1");
		ctx.Equal(LF_ERateVerdict.ALLOW, limiter.Take(1, LF_ERequestKind.TRAVEL, 500, 1000, 3, refused), "another kind is not limited by purchases");

		// The window rolls over at start + windowMs.
		ctx.Equal(LF_ERateVerdict.ALLOW, limiter.Take(1, LF_ERequestKind.PURCHASE, 1100, 1000, 3, refused), "a new window admits again");
		ctx.Equal(2, refused, "the new window reports the two quiet refusals of the old one");
		ctx.Equal(LF_ERateVerdict.ALLOW, limiter.Take(1, LF_ERequestKind.PURCHASE, 1101, 1000, 3, refused), "the new window has its burst back");
		ctx.Equal(0, refused, "the count is reported once");

		// A clock that goes backwards (tick wrap) opens a new window instead of locking the player out.
		LF_RateLimiter wrap = new LF_RateLimiter();
		wrap.Take(7, LF_ERequestKind.SPAWN, 5000, 1000, 1, refused);
		ctx.Equal(LF_ERateVerdict.REJECT, wrap.Take(7, LF_ERequestKind.SPAWN, 5001, 1000, 1, refused), "burst of one: the second request is refused");
		ctx.Equal(LF_ERateVerdict.ALLOW, wrap.Take(7, LF_ERequestKind.SPAWN, 10, 1000, 1, refused), "a wrapped clock opens a new window");

		// Idle buckets are pruned.
		wrap.Take(8, LF_ERequestKind.SPAWN, 20, 1000, 1, refused);
		wrap.Prune(100000, LF_RateLimiter.PRUNE_IDLE_MS);
		ctx.Equal(0, wrap.Count(), "a prune drops buckets idle past the idle span");

		// Every kind has a positive window and burst, and the idle span outlasts every window.
		array<LF_ERequestKind> kinds = {LF_ERequestKind.PURCHASE, LF_ERequestKind.TRAVEL, LF_ERequestKind.COMMS, LF_ERequestKind.SPAWN, LF_ERequestKind.INVENTORY};
		foreach (LF_ERequestKind kind : kinds)
		{
			string name = typename.EnumToString(LF_ERequestKind, kind);
			ctx.True(LF_RateLimit.WindowMs(kind) > 0, name + " has a window");
			ctx.True(LF_RateLimit.Burst(kind) >= 1, name + " admits at least one request");
			ctx.True(LF_RateLimiter.PRUNE_IDLE_MS > LF_RateLimit.WindowMs(kind), name + " window is shorter than the prune idle span");
		}
	}
}
