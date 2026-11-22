// red-proof: 2026-11-22, `--only LF_Test_ToolCondition` before the change: LF_ToolCondition did not exist, so the test could not compile. `--red LF_Test_ToolCondition` skips the wear step, so the condition never drops and the speed and text checks fail.
//! tool/condition (#4182, WK-W7): a tool loses condition per use at its tier's rate, the gather time
//! grows at low condition (a worn-out tool still works), and the action text changes at the thresholds.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_ToolCondition : LF_Test
{
	override string GetName()
	{
		return "tool/condition";
	}

	override void Run(LF_TestContext ctx)
	{
		// Wear per use follows the tier: worn fastest, fine slowest.
		float wornRate = LF_ToolCondition.WearPerUse(LF_ToolCondition.TIER_WORN);
		float standardRate = LF_ToolCondition.WearPerUse(LF_ToolCondition.TIER_STANDARD);
		float fineRate = LF_ToolCondition.WearPerUse(LF_ToolCondition.TIER_FINE);
		ctx.True(wornRate > standardRate, "a worn tool wears faster than a standard one");
		ctx.True(standardRate > fineRate, "a standard tool wears faster than a fine one");
		ctx.True(fineRate > 0, "even a fine tool wears");

		// A tool as sold is at full condition; each use drops it at its tier's rate.
		LF_ToolCondition standard = LF_ToolCondition.Create(LF_ToolCondition.TIER_STANDARD);
		ctx.EqualFloat(100.0, standard.m_fCondition, 0.001, "a tool as sold is at full condition");
		for (int i = 0; i < 10; i++)
		{
			if (!RedProof())
				standard.Use();
		}
		ctx.EqualFloat(100.0 - 10 * standardRate, standard.m_fCondition, 0.001, "ten uses cost ten times the tier's rate");

		LF_ToolCondition worn = LF_ToolCondition.Create(LF_ToolCondition.TIER_WORN);
		LF_ToolCondition fine = LF_ToolCondition.Create(LF_ToolCondition.TIER_FINE);
		if (!RedProof())
		{
			worn.Use();
			fine.Use();
		}
		ctx.True(worn.m_fCondition < fine.m_fCondition, "one use leaves a worn tool lower than a fine one");

		// Gather time grows as the condition falls, but never stops.
		float fullTime = LF_ToolCondition.Duration(3.0, 1.0, 100.0);
		float midTime = LF_ToolCondition.Duration(3.0, 1.0, 30.0);
		float shotTime = LF_ToolCondition.Duration(3.0, 1.0, 0);
		ctx.EqualFloat(3.0, fullTime, 0.001, "a good tool holds for the base time");
		ctx.True(midTime > fullTime, "a half-worn tool takes longer");
		ctx.True(shotTime > midTime, "a worn-out tool takes longest");
		ctx.True(shotTime < 100, "a worn-out tool still finishes the hold");
		ctx.EqualFloat(6.0, shotTime, 0.001, "worn out is half speed: a 3 s hold takes 6 s");
		ctx.EqualFloat(1.0, LF_ToolCondition.SpeedFactor(LF_ToolCondition.GOOD_FROM), 0.001, "the good threshold works at full speed");
		ctx.True(LF_ToolCondition.SpeedFactor(-5) > 0, "a bad condition value never stops the tool");

		// The condition floors at zero and the tool keeps working.
		ctx.EqualFloat(0, LF_ToolCondition.AfterUse(0.5, LF_ToolCondition.TIER_WORN), 0.001, "wear floors at zero");
		LF_ToolCondition spent = LF_ToolCondition.Create(LF_ToolCondition.TIER_WORN);
		spent.m_fCondition = 0;
		spent.Use();
		ctx.EqualFloat(0, spent.m_fCondition, 0.001, "a worn-out tool stays at zero");
		ctx.True(spent.Speed() > 0, "a worn-out tool still has a speed");

		// The action text changes at the thresholds, in words with no digit.
		ctx.EqualStr("#LF-ToolCondition_Good", LF_ToolCondition.StateKey(100), "full condition reads good");
		ctx.EqualStr("#LF-ToolCondition_Good", LF_ToolCondition.StateKey(LF_ToolCondition.GOOD_FROM), "the good threshold reads good");
		ctx.EqualStr("#LF-ToolCondition_Worn", LF_ToolCondition.StateKey(LF_ToolCondition.GOOD_FROM - 1), "just under good reads worn");
		ctx.EqualStr("#LF-ToolCondition_Worn", LF_ToolCondition.StateKey(LF_ToolCondition.WORN_FROM), "the worn threshold reads worn");
		ctx.EqualStr("#LF-ToolCondition_Poor", LF_ToolCondition.StateKey(LF_ToolCondition.WORN_FROM - 1), "just under worn reads poor");
		ctx.EqualStr("#LF-ToolCondition_Poor", LF_ToolCondition.StateKey(1), "a sliver of condition reads poor");
		ctx.EqualStr("#LF-ToolCondition_WornOut", LF_ToolCondition.StateKey(0), "zero reads worn out");
		ctx.True(standard.Key() != "#LF-ToolCondition_WornOut", "ten uses is nowhere near worn out");
		ctx.False(HasDigit(LF_ToolCondition.StateKey(10)), "the state key carries no digit");

		// Wearing a tool all the way down walks through every state in order.
		LF_ToolCondition walker = LF_ToolCondition.Create(LF_ToolCondition.TIER_WORN);
		int lastState = LF_ToolCondition.STATE_GOOD;
		bool backwards = false;
		int guard = 0;
		while (walker.m_fCondition > 0 && guard < 1000)
		{
			if (RedProof())
				break;
			walker.Use();
			int state = LF_ToolCondition.StateOf(walker.m_fCondition);
			if (state < lastState)
				backwards = true;
			lastState = state;
			guard++;
		}
		ctx.False(backwards, "wear never moves a tool back to a better state");
		ctx.Equal(LF_ToolCondition.STATE_WORN_OUT, lastState, "using a tool long enough wears it out");
		ctx.Equal(50, guard, "a worn tool is worn out after fifty uses");

		// Tier names are words too.
		ctx.EqualStr("#LF-ToolTier_Worn", LF_ToolCondition.TierKey(LF_ToolCondition.TIER_WORN), "worn tier key");
		ctx.EqualStr("#LF-ToolTier_Standard", LF_ToolCondition.TierKey(LF_ToolCondition.TIER_STANDARD), "standard tier key");
		ctx.EqualStr("#LF-ToolTier_Fine", LF_ToolCondition.TierKey(LF_ToolCondition.TIER_FINE), "fine tier key");
	}

	//------------------------------------------------------------------------------------------------
	protected bool HasDigit(string text)
	{
		for (int i = 0; i < text.Length(); i++)
		{
			int c = text.ToAscii(i);
			if (c >= 48 && c <= 57)
				return true;
		}
		return false;
	}
};
