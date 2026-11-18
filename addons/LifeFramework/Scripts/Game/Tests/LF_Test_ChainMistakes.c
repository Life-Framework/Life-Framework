// red-proof: observed 2026-11-18 with `--red LF_Test_ChainMistakes`: RED PROOF OK, every strike assertion fails with the hook on.
// red-proof (#2881 review): `--only LF_Test_ChainMistakes` on the old Roll failed the ruin-rate check (385 of 2000 batches ruined, 19 %); green on the new one.

//! Cook mistakes (#2854): a seeded chance adds hidden strikes, the limit ruins the batch.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_ChainMistakes : LF_Test
{
	override string GetName()
	{
		return "chains/mistakes";
	}

	//------------------------------------------------------------------------------------------------
	protected string StrikeSeq(int seed, int chance, int max, int ticks)
	{
		LF_ChainStrikes s = new LF_ChainStrikes();
		string seq = "";
		for (int i = 0; i < ticks; i++)
		{
			LF_EChainMistake r = LF_EChainMistake.NONE;
			if (!RedProof())
				r = LF_ChainMistakes.Tick(s, chance, max, seed);
			seq += r.ToString() + ",";
			if (r == LF_EChainMistake.RUINED)
				LF_ChainMistakes.Reset(s);
		}
		return seq;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		ctx.EqualStr(StrikeSeq(42, 50, 100, 40), StrikeSeq(42, 50, 100, 40), "a fixed seed repeats the strike sequence");
		ctx.True(StrikeSeq(42, 50, 100, 40) != StrikeSeq(43, 50, 100, 40), "another seed gives another sequence");

		LF_ChainStrikes s = new LF_ChainStrikes();
		int i;
		int struck = 0;
		for (i = 0; i < 200; i++)
		{
			if (LF_ChainMistakes.Tick(s, 0, 5, 7) != LF_EChainMistake.NONE)
				struck++;
		}
		ctx.Equal(0, struck, "chance 0 never strikes");
		ctx.Equal(0, s.m_iStrikes, "chance 0 leaves no strikes");

		LF_ChainMistakes.Reset(s);
		if (!RedProof())
		{
			for (i = 0; i < 4; i++)
				LF_ChainMistakes.Tick(s, 100, 5, 7);
		}
		ctx.Equal(4, s.m_iStrikes, "chance 100 strikes every work tick");
		ctx.Equal(1, LF_ChainMistakes.CueLevel(s, 10), "4 of 10 gives off-colour smoke");

		LF_EChainMistake last = LF_EChainMistake.NONE;
		LF_ChainMistakes.Reset(s);
		int ticksToRuin = 0;
		for (i = 0; i < 10; i++)
		{
			if (RedProof())
				break;
			last = LF_ChainMistakes.Tick(s, 100, 3, 7);
			ticksToRuin++;
			if (last == LF_EChainMistake.RUINED)
				break;
		}
		ctx.True(last == LF_EChainMistake.RUINED, "the batch is ruined when strikes reach the limit");
		ctx.Equal(3, ticksToRuin, "ruined on exactly the third strike");
		ctx.Equal(3, s.m_iStrikes, "the ruin tick counted its strike");

		LF_ChainMistakes.Reset(s);
		ctx.Equal(0, s.m_iStrikes, "a completed or ruined batch resets strikes");
		ctx.Equal(0, LF_ChainMistakes.CueLevel(s, 3), "reset is clean");

		LF_ChainStrikes never = new LF_ChainStrikes();
		int ruined = 0;
		for (i = 0; i < 100; i++)
		{
			if (LF_ChainMistakes.Tick(never, 100, 0, 7) != LF_EChainMistake.NONE)
				ruined++;
		}
		ctx.Equal(0, ruined, "maxMistakes 0 can never fail");

		LF_ChainStrikes cue = new LF_ChainStrikes();
		ctx.Equal(0, LF_ChainMistakes.CueLevel(cue, 4), "no strikes is level 0");
		cue.m_iStrikes = 1;
		ctx.Equal(1, LF_ChainMistakes.CueLevel(cue, 4), "1 of 4 is level 1");
		cue.m_iStrikes = 3;
		ctx.Equal(2, LF_ChainMistakes.CueLevel(cue, 4), "3 of 4 is about to blow");

		int hits = 0;
		for (i = 0; i < 1000; i++)
		{
			if (LF_ChainMistakes.Roll(99, i) < 30)
				hits++;
		}
		ctx.True(hits > 200 && hits < 400, "a 30 percent chance lands near 30 percent of 1000 rolls");

		// Ticks of one batch are independent (#2881): over 2000 seeds a 16-tick batch at 6 % and 3
		// mistakes is ruined about 7 % of the time (binomial). The old mix gave 22 %.
		int ruinedBatches = 0;
		for (int seed = 0; seed < 2000; seed++)
		{
			LF_ChainStrikes batch = new LF_ChainStrikes();
			for (int tick = 0; tick < 16; tick++)
			{
				if (LF_ChainMistakes.Tick(batch, 6, 3, seed) == LF_EChainMistake.RUINED)
				{
					ruinedBatches++;
					break;
				}
			}
		}
		ctx.True(ruinedBatches > 80 && ruinedBatches < 220, string.Format("a 6 percent, 3-mistake, 16-tick batch is ruined near 7 percent of the time (got %1 of 2000)", ruinedBatches));
	}
}
