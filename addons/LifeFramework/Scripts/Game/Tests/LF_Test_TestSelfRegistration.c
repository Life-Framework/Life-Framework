// red-proof: Observed 2026-11-17 with `tools\cli test --no-build --red LF_Test_TestSelfRegistration`: [LFTEST] RED-OK tests/self-registration (fails with the hook on), "LF_Test_TestSelfRegistration is collected exactly once (expected 1, got 0)" with the test dropped from the collected list as if its registration class were gone.

// tier: LOGIC
//! tests/self-registration: a test file that carries its own LF_TestRegistration is collected exactly
//! once, with its tier, at its class-name position beside the generated registry (#2615, #2610).
class LF_Test_TestSelfRegistration : LF_Test
{
	override string GetName()
	{
		return "tests/self-registration";
	}

	override void Run(LF_TestContext ctx)
	{
		// A fresh manager, so the running singleton's list is not rebuilt mid-run.
		LF_TestManager manager = new LF_TestManager();
		array<ref LF_Test> tests = manager.Collected();
		string self = ClassName();
		// Red proof: the list as if this file had no registration class.
		if (RedProof())
		{
			for (int r = tests.Count() - 1; r >= 0; r--)
			{
				if (tests[r] && tests[r].ClassName() == self)
					tests.RemoveOrdered(r);
			}
		}

		int count = 0;
		int index = -1;
		array<string> order = {};
		foreach (int i, LF_Test test : tests)
		{
			if (!test)
				continue;

			string name = test.ClassName();
			order.Insert(name);
			if (name != self)
				continue;

			count++;
			index = i;
		}

		ctx.Equal(1, count, string.Format("%1 is collected exactly once", self));
		bool logicTier = false;
		if (index >= 0)
			logicTier = tests[index].Tier() == LF_TestTier.LOGIC;
		ctx.True(logicTier, "the self-registered test carries its LOGIC tier");

		// Merged in by class name (LF_TestManager.MergeSelfRegistered): every test before it sorts before
		// it, and the one after it sorts after it, whatever order the scripts loaded in.
		int outOfPlace = -1;
		if (index >= 0)
		{
			outOfPlace = 0;
			for (int k = 0; k < index; k++)
			{
				if (!LF_TestManager.SortsBefore(order[k], self))
					outOfPlace++;
			}

			if (index + 1 < order.Count() && !LF_TestManager.SortsBefore(self, order[index + 1]))
				outOfPlace++;
		}

		ctx.Equal(0, outOfPlace, "the self-registered test sits at its class-name position");
		ctx.True(order.Count() > 1, "the generated registry is still collected beside the self-registered tests");
	}
}

//------------------------------------------------------------------------------------------------
//! Self-registration (LF_TestRegistration.c): this file is the whole of adding this test.
class LF_TestRegistration_TestSelfRegistration : LF_TestRegistration
{
	protected static bool s_bRegistered = LF_TestManager.Enqueue(new LF_TestRegistration_TestSelfRegistration());

	override LF_TestTier Tier()
	{
		return LF_TestTier.LOGIC;
	}

	override LF_Test Create()
	{
		return new LF_Test_TestSelfRegistration();
	}
}
