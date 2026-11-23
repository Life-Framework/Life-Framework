// red-proof: 2026-11-23 (#4382), with the act skipped (RedProof() on, `cli test --no-build --red LF_Test_ResidentWorkLoop`)
//   no pose is started, so `a hammer worker plays the hammer pose`, `the pose was started once` and `a sweep worker
//   falls back to the idle loiter` fail. The RED-OK line is in the PR.
//! residents/work-loop (#4230, #4382): a resident with a work activity (carry, hammer, sweep) holds that job's body
//! pose. The rule with no world: a name is trimmed and lower-cased, an unknown one is not work, hammer has its own
//! anim set instance on the work graph and it loads, carry and sweep (no vanilla clip reads as either) have none
//! and play the plain idle loiter. With a character: Begin starts the hammer pose once, beginning again keeps it, a
//! sweep line swaps to the idle loiter, a check before the restart time leaves the pose alone, Stop ends it.
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
		}

		ctx.EqualStr(LF_ResidentWorkLoop.HAMMER_POSE, LF_ResidentWorkLoop.PoseOf("hammer"), "hammer has its own pose");
		ctx.EqualStr("", LF_ResidentWorkLoop.PoseOf("Carry"), "carry has no pose of its own and idles");
		ctx.EqualStr("", LF_ResidentWorkLoop.PoseOf("sweep"), "sweep has no pose of its own and idles");
		ctx.EqualStr("", LF_ResidentWorkLoop.PoseOf("moonwalk"), "an unknown name has no pose");

		ctx.True(Resource.Load(LF_ResidentWorkLoop.HAMMER_POSE).IsValid(), "the hammer pose instance loads");
		ctx.True(Resource.Load(LF_WorkAnimPlayer.PICK_GRAPH).IsValid(), "the work graph the poses run on loads");
		ctx.NotNull(LF_ResidentWorkLoop.PoseData(LF_ResidentWorkLoop.HAMMER_POSE), "a pose builds its loiter data");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunWorker(LF_TestContext ctx, IEntity worker)
	{
		ctx.False(LF_ResidentWorkLoop.IsRunning(worker), "nobody works before Begin");

		bool began;
		if (!RedProof())
			began = LF_ResidentWorkLoop.Begin(worker, "hammer");

		ctx.True(began, "a hammer worker begins");
		ctx.True(LF_ResidentWorkLoop.IsRunning(worker), "a worker that began is running");
		ctx.EqualStr(LF_ResidentWorkLoop.HAMMER_POSE, LF_ResidentWorkLoop.PlayedPose(worker), "a hammer worker plays the hammer pose");
		ctx.Equal(1, LF_ResidentWorkLoop.StartsOf(worker), "the pose was started once");

		// Beginning twice keeps one pose; a check before the restart time leaves it alone.
		ctx.True(LF_ResidentWorkLoop.Begin(worker, "hammer"), "beginning again keeps the loop");
		LF_ResidentWorkLoop.Check(worker, LF_ResidentWorkLoop.RESTART_MS - 1);
		ctx.Equal(1, LF_ResidentWorkLoop.StartsOf(worker), "the same work starts its pose once");
		ctx.Equal(1, LF_ResidentWorkLoop.Count(), "one worker is one loop");

		// A job with no pose of its own plays the idle loiter, not a hand signal.
		if (!RedProof())
			LF_ResidentWorkLoop.Begin(worker, "sweep");

		ctx.True(LF_ResidentWorkLoop.IsRunning(worker) && LF_ResidentWorkLoop.PlayedPose(worker).IsEmpty(), "a sweep worker falls back to the idle loiter");
		ctx.Equal(1, LF_ResidentWorkLoop.Count(), "a new job replaces the old one");

		LF_ResidentWorkLoop.Stop(worker);
		ctx.False(LF_ResidentWorkLoop.IsRunning(worker), "Stop ends the loop");
		ctx.Equal(0, LF_ResidentWorkLoop.StartsOf(worker), "a stopped worker has no pose");

		// A word that is not work never starts a loop.
		ctx.False(LF_ResidentWorkLoop.Begin(worker, "moonwalk"), "an unknown name does not begin");
		ctx.False(LF_ResidentWorkLoop.IsRunning(worker), "an unknown name leaves nobody working");
	}
}
