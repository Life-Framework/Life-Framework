// red-proof: 2026-11-19 `tools/cli test --no-build --red LF_Test_HeistStages` printed `RED-OK crime/heist-stages (fails with the hook on, 44 assertions)`; RedProof() skips the out-of-order guard, the clock, the crew death and the last-stage success. Green without the hook.
//! Heist stages (#3409): ordered stages each with a gate, one clock for the job, a final fail state.
//! Pure logic: times are world milliseconds passed in, nothing touches the world.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_HeistStages : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "crime/heist-stages";
	}

	//------------------------------------------------------------------------------------------------
	//! Three stages: open the van, the crew inside the yard, hold the yard for 30 s. 10 minute clock.
	protected LF_HeistStages Build()
	{
		LF_HeistStages heist = new LF_HeistStages();
		heist.AddStage(LF_EHeistGate.TARGET_OPENED, 0);
		heist.AddStage(LF_EHeistGate.CREW_IN_AREA, 0);
		heist.AddStage(LF_EHeistGate.TIME_HELD, 30000);
		return heist;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		// Order: a later stage's signal does nothing until the earlier stages are done.
		LF_HeistStages heist = Build();
		ctx.Equal(LF_EHeistState.IDLE, heist.GetState(), "a new heist is idle");
		ctx.False(heist.TargetOpened(0, 1000), "a signal before Start does nothing");
		heist.Start(1000, 600000);
		ctx.Equal(LF_EHeistState.RUNNING, heist.GetState(), "Start runs the heist");
		ctx.Equal(0, heist.GetStage(), "the first stage is current");

		if (!RedProof())
		{
			ctx.False(heist.CrewInArea(1, true, 2000), "stage 2 signalled first: nothing happens");
			ctx.False(heist.TargetOpened(2, 2000), "stage 3 signalled first: nothing happens");
			ctx.False(heist.TargetOpened(1, 2000), "a wrong gate kind for the stage does nothing");
		}
		ctx.Equal(0, heist.GetStage(), "out of order signals left the stage at 0");

		ctx.True(heist.TargetOpened(0, 3000), "stage 1 gate met advances");
		ctx.Equal(1, heist.GetStage(), "stage 2 is current");
		ctx.False(heist.TargetOpened(0, 3500), "a repeat of a finished stage does nothing");
		ctx.Equal(1, heist.GetStage(), "a repeat did not move the stage");

		// Crew gate: not in the area is not met.
		ctx.False(heist.CrewInArea(1, false, 4000), "crew outside the area: gate not met");
		ctx.Equal(1, heist.GetStage(), "still stage 2");
		ctx.True(heist.CrewInArea(1, true, 5000), "crew in the area advances");
		ctx.Equal(2, heist.GetStage(), "stage 3 is current");

		// Time gate: hold for 30 s; leaving the area resets the hold.
		ctx.False(heist.Hold(2, true, 6000), "hold begins, not yet long enough");
		ctx.False(heist.Hold(2, false, 20000), "holding broken");
		ctx.False(heist.Hold(2, true, 21000), "hold restarts from 21 s");
		ctx.False(heist.Tick(40000, 3), "19 s into the new hold: not done");
		ctx.Equal(LF_EHeistState.RUNNING, heist.GetState(), "still running");

		// Success after the last stage, once the hold is long enough.
		if (!RedProof())
			ctx.True(heist.Tick(51000, 3), "30 s held: the last stage completes");
		ctx.Equal(LF_EHeistState.SUCCEEDED, heist.GetState(), "success after the last stage");
		ctx.False(heist.IsFailed(), "a success is not a fail");

		// A success is final too: late ticks and a dead crew change nothing.
		heist.Tick(9000000, 0);
		ctx.Equal(LF_EHeistState.SUCCEEDED, heist.GetState(), "a finished heist ignores the clock");

		// The clock fails it.
		LF_HeistStages timed = Build();
		timed.Start(0, 60000);
		ctx.False(timed.Tick(59999, 3), "just inside the clock: running");
		ctx.Equal(LF_EHeistState.RUNNING, timed.GetState(), "running inside the clock");
		if (!RedProof())
			timed.Tick(60000, 3);
		ctx.Equal(LF_EHeistState.FAILED, timed.GetState(), "the clock running out fails the heist");
		ctx.EqualStr(LF_HeistStages.FAIL_CLOCK, timed.GetFailReason(), "the fail reason is the clock");

		// A fail is final: later gates do not revive it.
		ctx.False(timed.TargetOpened(0, 61000), "a failed heist takes no more signals");
		ctx.Equal(0, timed.GetStage(), "the stage stays where it failed");
		ctx.Equal(LF_EHeistState.FAILED, timed.GetState(), "a fail is final");
		ctx.False(timed.Start(70000, 600000), "a failed heist cannot be restarted");

		// A signal after the deadline fails instead of advancing.
		LF_HeistStages late = Build();
		late.Start(0, 60000);
		ctx.False(late.TargetOpened(0, 70000), "a gate met after the deadline does not advance");
		ctx.Equal(LF_EHeistState.FAILED, late.GetState(), "a late signal fails the heist");

		// The crew dying fails it, and the clock reason is not overwritten later.
		LF_HeistStages dead = Build();
		dead.Start(0, 600000);
		dead.TargetOpened(0, 1000);
		if (!RedProof())
			dead.Tick(2000, 0);
		ctx.Equal(LF_EHeistState.FAILED, dead.GetState(), "the crew dying fails the heist");
		ctx.EqualStr(LF_HeistStages.FAIL_CREW, dead.GetFailReason(), "the fail reason is the crew");
		dead.Tick(9000000, 2);
		ctx.EqualStr(LF_HeistStages.FAIL_CREW, dead.GetFailReason(), "the first fail reason stands");

		// A zero clock means no time limit; a heist with no stages never starts.
		LF_HeistStages open = Build();
		open.Start(0, 0);
		open.Tick(90000000, 2);
		ctx.Equal(LF_EHeistState.RUNNING, open.GetState(), "clock 0 means no limit");
		LF_HeistStages empty = new LF_HeistStages();
		ctx.False(empty.Start(0, 1000), "no stages: Start refuses");
		ctx.Equal(LF_EHeistState.IDLE, empty.GetState(), "still idle");
	}
}
