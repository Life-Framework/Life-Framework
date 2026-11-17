// red-proof: 2026-11-17, bug fix. On the unfixed code (chess asks counted against COMMS) `--only LF_Test_ChessRateBucket` fails "chess is not refused by a full phone bucket". With LF_ERequestKind.CHESS, green.

// tier: LOGIC
//! #2405 (review 2026-11-17): chess asks have their own rate bucket. Three phone or board reads
//! (the COMMS burst) must not refuse a chess move, and a chess flood must not refuse the phone.
class LF_Test_ChessRateBucket : LF_Test
{
	protected static const int PLAYER = 990405;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "chess/rate-bucket";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		for (int i = 0; i < LF_RateLimit.COMMS_BURST; i++)
		{
			LF_RateLimit.Allow(PLAYER, LF_ERequestKind.COMMS, "test comms", false);
		}

		ctx.False(LF_RateLimit.Allow(PLAYER, LF_ERequestKind.COMMS, "test comms", false), "the phone bucket is full");
		ctx.True(LF_ChessTableComponent.AllowAsk(PLAYER, "test chess", false), "chess is not refused by a full phone bucket");

		int admitted = 1;
		for (int j = 0; j < LF_RateLimit.CHESS_BURST + 3; j++)
		{
			if (LF_ChessTableComponent.AllowAsk(PLAYER, "test chess", false))
				admitted++;
		}

		ctx.Equal(LF_RateLimit.CHESS_BURST, admitted, "chess admits its own burst, then refuses");
	}
}
