// red-proof: 2026-11-11, before the fix (StartAlarm spawned at the site point, beacon child 1.1 m up),
//   cli test --no-build --only LF_Test_AlarmRests:
//   [LFTEST]   - a site point 1 m up puts the alarm on the ground below it (expected 0, got 1)
//   [LFTEST]   - the beacon stands on the ground, not above it (expected 0, got 2.1)
// Fixed (LF_Dispatch.RestingSpot, beacon at the alarm origin); green with dispatch/site-alarm and LF_Test_Dispatch.
//------------------------------------------------------------------------------------------------
//! dispatch/alarm-rests: a site alarm stands on whatever is under the site, not in the air. The beacon was a
//! child 1.1 m above the site point, so at the cash van box it hung in mid-air over the van (#571 trailer review).
// tier: WORLD
class LF_Test_AlarmRests : LF_Test
{
	override string GetName()
	{
		return "dispatch/alarm-rests";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_Dispatch.ResetStatics();
		BaseWorld world = GetGame().GetWorld();
		float ground = world.GetSurfaceY(300, 200);
		vector site = Vector(300, ground + 1.0, 200);
		IEntity alarm = LF_Dispatch.StartAlarm(site, true);
		ctx.NotNull(alarm, "the alarm spawns over open ground");
		if (!alarm)
			return;

		ctx.EqualFloat(ground, alarm.GetOrigin()[1], 0.1, "a site point 1 m up puts the alarm on the ground below it");

		IEntity beacon = alarm.GetChildren();
		ctx.NotNull(beacon, "the alarm carries its beacon");
		if (beacon)
			ctx.EqualFloat(ground, beacon.GetOrigin()[1], 0.1, "the beacon stands on the ground, not above it");

		LF_Dispatch.StopAllAlarms();
		LF_Dispatch.ResetStatics();
	}
}
