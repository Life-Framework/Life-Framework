// red-proof: Observed 2026-11-17 with `tools\cli test --no-build --red LF_Test_TestSelfRegistration`: [LFTEST] RED-OK tests/self-registration (fails with the hook on, 12 assertions), "LF_Test_TestSelfRegistration is collected exactly once (expected 1, got 0)" with the test dropped from the collected list as if its [LF_TestInfo] attribute were gone.

//! tests/self-registration: a test class that carries its own [LF_TestInfo] attribute is collected exactly
//! once, with its tier, at its class-name position beside the generated registry; a subclass without an
//! attribute of its own is not collected (#2757, #2610).
[LF_TestInfo(LF_TestTier.LOGIC)]
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
		// Red proof: the list as if this class had no attribute.
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
		int probeCount = 0;
		array<string> order = {};
		foreach (int i, LF_Test test : tests)
		{
			if (!test)
				continue;

			string name = test.ClassName();
			order.Insert(name);
			if (name == "LF_TestSelfRegistrationProbe")
				probeCount++;

			if (name != self)
				continue;

			count++;
			index = i;
		}

		ctx.Equal(1, count, string.Format("%1 is collected exactly once", self));
		bool logicTier = false;
		bool notPack = false;
		if (index >= 0)
		{
			logicTier = tests[index].Tier() == LF_TestTier.LOGIC;
			notPack = !tests[index].IsPackTest();
		}
		ctx.True(logicTier, "the attribute-registered test carries its LOGIC tier");
		ctx.True(notPack, "a test without pack: true is not a pack test");

		// Merged in by class name (LF_TestManager.MergeDiscovered): every test before it sorts before
		// it, and the one after it sorts after it, whatever order reflection returned the types in.
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

		ctx.Equal(0, outOfPlace, "the attribute-registered test sits at its class-name position");
		ctx.True(order.Count() > 1, "the generated registry is still collected beside the attribute-registered tests");

		// Attributes are not inherited: the probe subclass is an LF_Test but has no [LF_TestInfo] of its own.
		ctx.Equal(0, probeCount, "a subclass without its own [LF_TestInfo] is not collected");
		typename probeType = LF_TestSelfRegistrationProbe;
		typename selfType = LF_Test_TestSelfRegistration;
		ctx.True(LF_TestManager.FindInfo(probeType) == null, "the probe subclass has no LF_TestInfo");
		ctx.True(LF_TestManager.FindInfo(selfType) != null, "this class has its LF_TestInfo");
	}
}

//------------------------------------------------------------------------------------------------
//! An LF_Test descendant with no attribute of its own: the manager must skip it.
class LF_TestSelfRegistrationProbe : LF_Test_TestSelfRegistration
{
}
