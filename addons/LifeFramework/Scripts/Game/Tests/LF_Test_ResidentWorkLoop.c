// red-proof: 2026-11-23, before LF_ResidentWorkLoop existed the test did not compile; with the act skipped (RedProof()
//   on, `cli test --no-build --red LF_Test_ResidentWorkLoop`) no step is ever applied, so `a carry worker starts on
//   step 0`, `the loop moves on to the next step` and `the worker plays the step's gesture` fail. The run that went
//   red and then green is in the PR.
//! residents/work-loop (#4230): a resident with a work activity (carry, hammer, sweep) cycles through its steps on
//! the clock. The rule with no world: a name is trimmed and lower-cased, an unknown one is not work, every
//! activity has several steps, the step at a second wraps around the cycle and every step gesture is a vanilla
//! one. With a character: Begin starts step 0, a later second moves to the next step and plays its gesture, a
//! whole cycle later it is back on step 0, and Stop ends the loop.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_ResidentWorkLoop : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "residents/work-loop";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		RunRule(ctx);

		LF_ResidentWorkLoop.ResetStatics();
		IEntity worker = LF_TestCharacterFixture.Spawn(ctx);
		if (worker)
			RunWorker(ctx, worker);

		LF_ResidentWorkLoop.ResetStatics();
		LF_TestCharacterFixture.Delete(worker);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunRule(LF_TestContext ctx)
	{
		bool known;
		ctx.EqualStr("hammer", LF_ResidentWorkLoop.Normalise(" Hammer ", known), "a work name is trimmed and lower-cased");
		ctx.True(known, "hammer is a work activity");
		ctx.EqualStr("", LF_ResidentWorkLoop.Normalise("sit", known), "sit is not a work activity");
		ctx.False(known, "sit reads as not work");
		ctx.False(LF_ResidentWorkLoop.IsWork("moonwalk"), "an unknown name is not work");

		array<string> names = {LF_ResidentWorkLoop.CARRY, LF_ResidentWorkLoop.HAMMER, LF_ResidentWorkLoop.SWEEP};
		foreach (string name : names)
		{
			ctx.True(LF_ResidentWorkLoop.IsWork(name), name + " is a work activity");
			ctx.True(LF_ResidentWorkLoop.StepCount(name) >= 3, name + " has several steps");
			int total = 0;
			for (int i = 0; i <LF_ResidentWorkLoop.StepCount(name); i++)
			{
				ctx.True(LF_ResidentWorkLoop.StepSeconds(name, i) > 0, name + " step " + i + " lasts a while");
				total += LF_ResidentWorkLoop.StepSeconds(name, i);
			}

			ctx.Equal(total, LF_ResidentWorkLoop.CycleSeconds(name), name + " cycle is the sum of its steps");
			ctx.Equal(0, LF_ResidentWorkLoop.StepAt(name, 0), name + " starts on step 0");
			ctx.Equal(0, LF_ResidentWorkLoop.StepAt(name, total), name + " wraps to step 0 after one cycle");
			ctx.Equal(LF_ResidentWorkLoop.StepCount(name) - 1, LF_ResidentWorkLoop.StepAt(name, total - 1), name + " ends on its last step");
			ctx.Equal(1, LF_ResidentWorkLoop.StepAt(name, LF_ResidentWorkLoop.StepSeconds(name, 0)), name + " moves to step 1 when step 0 is over");
		}

		ctx.Equal(-1, LF_ResidentWorkLoop.StepAt("moonwalk", 5), "an unknown name has no step");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunWorker(LF_TestContext ctx, IEntity worker)
	{
		string name = LF_ResidentWorkLoop.CARRY;
		ctx.False(LF_ResidentWorkLoop.IsRunning(worker), "nobody works before Begin");

		bool began;
		if (!RedProof())
			began = LF_ResidentWorkLoop.Begin(worker, name);

		ctx.True(began, "a carry worker begins");
		ctx.True(LF_ResidentWorkLoop.IsRunning(worker), "a worker that began is running");
		ctx.Equal(0, LF_ResidentWorkLoop.StepOf(worker), "a carry worker starts on step 0");

		int first = LF_ResidentWorkLoop.StepSeconds(name, 0);
		if (!RedProof())
			LF_ResidentWorkLoop.TickAt(worker, first);

		ctx.Equal(1, LF_ResidentWorkLoop.StepOf(worker), "the loop moves on to the next step");
		ctx.Equal(LF_ResidentWorkLoop.GestureOf(name, 1), LF_ResidentWorkLoop.PlayedGesture(worker), "the worker plays the step's gesture");

		if (!RedProof())
			LF_ResidentWorkLoop.TickAt(worker, LF_ResidentWorkLoop.CycleSeconds(name) + 1);

		ctx.Equal(0, LF_ResidentWorkLoop.StepOf(worker), "a whole cycle later the worker is back on step 0");

		// Beginning twice keeps one loop.
		ctx.True(LF_ResidentWorkLoop.Begin(worker, name), "beginning again keeps the loop");
		ctx.Equal(1, LF_ResidentWorkLoop.Count(), "one worker is one loop");

		LF_ResidentWorkLoop.Stop(worker);
		ctx.False(LF_ResidentWorkLoop.IsRunning(worker), "Stop ends the loop");
		ctx.Equal(-1, LF_ResidentWorkLoop.StepOf(worker), "a stopped worker has no step");

		// A word that is not work never starts a loop.
		ctx.False(LF_ResidentWorkLoop.Begin(worker, "moonwalk"), "an unknown name does not begin");
		ctx.False(LF_ResidentWorkLoop.IsRunning(worker), "an unknown name leaves nobody working");
	}
}
