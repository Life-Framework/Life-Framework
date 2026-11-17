// red-proof: 2026-11-18 batched `--red` RED PROOF OK: with the hook on no visitor is forgotten, so "the list holds only the visitors on the pad" fails (expected 2, got 52). Green with the fix.
// tier: LOGIC
//! #2406: the jump pad remembers a visitor only while it stands on the pad (the list never outgrows the
//! people on it), and only a living body is thrown: a corpse or a downed player is not.
class LF_Test_JumpPadSeen : LF_Test
{
	override string GetName()
	{
		return "toys/jump-pad-seen";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_JumpPadSeen seen = new LF_JumpPadSeen();
		ctx.Equal(0, seen.TokenFor("a", false), "a stranger has no token without create");
		int first = seen.TokenFor("a", true);
		int second = seen.TokenFor("b", true);
		ctx.True(first > 0 && second > 0 && first != second, "each visitor gets its own token");
		ctx.Equal(first, seen.TokenFor("a", true), "a visitor on the pad keeps its token");
		ctx.Equal(2, seen.Count(), "two visitors on the pad");

		// They come and go: the list holds only who is on the pad now.
		for (int i = 0; i < 50; i++)
		{
			string key = "visitor" + i.ToString();
			seen.TokenFor(key, true);
			if (!RedProof())
				seen.Forget(key);
		}

		ctx.Equal(2, seen.Count(), "the list holds only the visitors on the pad");
		seen.Forget("a");
		ctx.Equal(1, seen.Count(), "a visitor who left is forgotten");
		ctx.Equal(0, seen.TokenFor("a", false), "and no longer known");
		int again = seen.TokenFor("a", true);
		ctx.True(again > 0 && again != first, "a returning visitor starts with a fresh token");
		seen.Forget("never-seen");
		ctx.Equal(2, seen.Count(), "forgetting a stranger changes nothing");

		// Dead and downed bodies are not thrown; a living one is.
		ctx.True(LF_JumpPad.CanLaunch(ECharacterLifeState.ALIVE), "a living body is thrown");
		ctx.False(LF_JumpPad.CanLaunch(ECharacterLifeState.DEAD), "a corpse is not thrown");
		ctx.False(LF_JumpPad.CanLaunch(ECharacterLifeState.INCAPACITATED), "a downed body is not thrown");
	}
}
