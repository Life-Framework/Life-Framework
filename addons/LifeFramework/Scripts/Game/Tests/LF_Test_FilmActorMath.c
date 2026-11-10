// red-proof: 2026-11-10, Steer(0, 0.5) > 0 -> < 0, cli test --no-build --only LF_Test_FilmActorMath:
//   [LFTEST]   - a target to the +yaw side steers positive
// Restored; green.

// tier: LOGIC
class LF_Test_FilmActorMath : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "film/actor-math";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		ctx.True(LF_FilmMath.Steer(0, 0.5) > 0, "a target to the +yaw side steers positive");
		ctx.True(LF_FilmMath.Steer(0, -0.5) < 0, "a target to the -yaw side steers negative");
		ctx.True(LF_FilmMath.Steer(3.0, -3.0) > 0, "the short way round +-pi steers positive");
		ctx.True(LF_FilmMath.Steer(0, 3.0) == 1, "steering is clamped to 1");

		float throttle;
		float brake;
		LF_FilmMath.Pedals(0, 30, throttle, brake);
		ctx.True(throttle > 0.9 && brake == 0, "standing still below target: full throttle");
		LF_FilmMath.Pedals(40, 30, throttle, brake);
		ctx.True(throttle == 0 && brake > 0, "over target: brake");
		LF_FilmMath.Pedals(30, 0, throttle, brake);
		ctx.True(throttle == 0 && brake == 1, "target zero: full brake");

		LF_FilmStuck stuck = new LF_FilmStuck(2);
		ctx.False(stuck.Update(0.5, 10), "first sample counts as progress");
		ctx.False(stuck.Update(0.5, 8), "closing in is progress");
		ctx.False(stuck.Update(1.0, 8), "1 s idle is under the limit");
		ctx.True(stuck.Update(1.5, 8), "2.5 s without progress is stuck");
		stuck.Reset();
		ctx.False(stuck.Update(0.5, 8), "reset clears the idle time");
	}
}
