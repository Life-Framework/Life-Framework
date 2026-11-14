// red-proof: Observed 2026-11-14 with `tools/cli test --no-build --only LF_Test_VaultDoorSwing`: dropping the upper wrap in LF_BreachTargetComponent.SwingTurn (`turn > 1800`) failed `crime/vault-door-swing` with `- and back from a wrapped open yaw is 100 degrees, not 260 (expected -100, got 260)` and `- an open door asked to open again does not turn (expected 0, got 360)`. Restored; green.
// tier: LOGIC
//! The vault door turns over LF_BreachTargetComponent.SWING_SECONDS instead of snapping (robbery video,
//! #740): the ease starts and ends at rest and the turn always takes the short way round, so a yaw the
//! engine wrapped into another turn never spins the door a full circle.
class LF_Test_VaultDoorSwing : LF_Test
{
	override string GetName()
	{
		return "crime/vault-door-swing";
	}

	override void Run(LF_TestContext ctx)
	{
		ctx.EqualFloat(0, LF_BreachTargetComponent.SwingEase(0), 0.0001, "the swing starts at the closed yaw");
		ctx.EqualFloat(1, LF_BreachTargetComponent.SwingEase(1), 0.0001, "the swing ends on the open yaw");
		ctx.EqualFloat(0.5, LF_BreachTargetComponent.SwingEase(0.5), 0.0001, "half the time is half the turn");
		ctx.EqualFloat(0, LF_BreachTargetComponent.SwingEase(-0.3), 0.0001, "before the start the door has not moved");
		ctx.EqualFloat(1, LF_BreachTargetComponent.SwingEase(1.7), 0.0001, "after the end the door stays open");
		ctx.True(LF_BreachTargetComponent.SwingEase(0.1) < 0.1, "the heavy door starts slowly");
		ctx.True(LF_BreachTargetComponent.SwingEase(0.9) > 0.9, "and settles slowly");

		float last = 0;
		bool rising = true;
		for (int i = 1; i <= 20; i++)
		{
			float share = LF_BreachTargetComponent.SwingEase(i / 20.0);
			if (share < last)
				rising = false;
			last = share;
		}
		ctx.True(rising, "the door never swings back on its way open");

		ctx.EqualFloat(100, LF_BreachTargetComponent.SwingTurn(20, 120), 0.0001, "open: a plain 100 degree turn");
		ctx.EqualFloat(-100, LF_BreachTargetComponent.SwingTurn(120, 20), 0.0001, "repair: the same turn back");
		ctx.EqualFloat(100, LF_BreachTargetComponent.SwingTurn(120, -140), 0.0001, "a target past 180 wrapped by the engine is still 100 degrees on");
		ctx.EqualFloat(-100, LF_BreachTargetComponent.SwingTurn(-140, 120), 0.0001, "and back from a wrapped open yaw is 100 degrees, not 260");
		ctx.EqualFloat(0, LF_BreachTargetComponent.SwingTurn(-140, 220), 0.0001, "an open door asked to open again does not turn");
	}
}
