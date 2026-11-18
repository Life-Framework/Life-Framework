// red-proof: 2026-11-18, cli test --no-build --red (batched): RED-OK on the old COMMS bucket, "a mark sync is not refused by a full phone bucket" and "admit their own burst (expected 4, got 1)". Green with LF_ERequestKind.MAP.
//------------------------------------------------------------------------------------------------
//! map/marks-rate-bucket (review 2026-11-17, docs/reviews/2026-11-17-map.md): the personal-mark sync and
//! save have their own rate bucket. On the shared COMMS bucket three phone or board asks in five seconds
//! dropped a respawn's mark restore or the last save; the client's next upload then overwrote the stored
//! marks with its own, maybe empty, map. Same shape as chess/rate-bucket (#2405).
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_MapMarksRateBucket : LF_Test
{
	protected static const int PLAYER = 992223;

	override string GetName()
	{
		return "map/marks-rate-bucket";
	}

	//------------------------------------------------------------------------------------------------
	protected bool Ask()
	{
		if (RedProof())
			return LF_RateLimit.Allow(PLAYER, LF_ERequestKind.COMMS, "test map marks", false); // the old bucket

		return LF_MapMarks.AllowAsk(PLAYER, "test map marks", false);
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		for (int i = 0; i < LF_RateLimit.COMMS_BURST; i++)
		{
			LF_RateLimit.Allow(PLAYER, LF_ERequestKind.COMMS, "test comms", false);
		}

		ctx.False(LF_RateLimit.Allow(PLAYER, LF_ERequestKind.COMMS, "test comms", false), "the phone bucket is full");
		ctx.True(Ask(), "a mark sync is not refused by a full phone bucket");

		int admitted = 1;
		for (int j = 0; j < LF_RateLimit.MAP_BURST + 3; j++)
		{
			if (Ask())
				admitted++;
		}

		ctx.Equal(LF_RateLimit.MAP_BURST, admitted, "map marks admit their own burst, then refuse");
	}
}
