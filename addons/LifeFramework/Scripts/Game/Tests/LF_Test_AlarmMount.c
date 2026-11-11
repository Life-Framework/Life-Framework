// red-proof: 2026-11-11, with the mount lookup in LF_Dispatch.AlarmSpot disabled (the old ground placement),
//   cli test --no-build --only LF_Test_AlarmMount:
//   [LFTEST]   - the vault alarm stands on the bell box (2.53851 m off)
//   [LFTEST]   - the van alarm stands on the roof (5.96046e-08 m up)
//   [LFTEST]   - the van alarm is on the van, not the road
// Restored; green.
// tier: WORLD
//! dispatch/alarm-mount: a robbed site near an alarm mount sounds its alarm there, a small beacon on the bell box
//! above the bank vault door and on the cash van roof. The alarm stood on the ground at the site point instead: a
//! 1.6 m beacon on a post by the vault door and on the road behind the van (#571 maintainer review).
class LF_Test_AlarmMount : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "dispatch/alarm-mount";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_Dispatch.ResetStatics();
		BaseWorld world = GetGame().GetWorld();
		IEntity vault = world.FindEntityByName(LF_TestAnnex.BANK_VAULT);
		IEntity bellBox = world.FindEntityByName(LF_TestAnnex.BANK_ALARM_BOX);
		IEntity vanBox = world.FindEntityByName(LF_TestAnnex.CASH_VAN_BOX);
		IEntity van = world.FindEntityByName(LF_TestAnnex.CASH_VAN);
		ctx.True(vault && bellBox && vanBox && van, "the vault, its bell box, the cash van and its box are placed");
		if (!vault || !bellBox || !vanBox || !van)
			return;

		IEntity alarm = LF_Dispatch.StartAlarm(vault.GetOrigin(), false);
		ctx.NotNull(alarm, "the vault alarm spawns");
		if (alarm)
		{
			vector top = bellBox.CoordToParent("0 0.167 0.065");
			ctx.True(vector.Distance(alarm.GetOrigin(), top) < 0.05, string.Format("the vault alarm stands on the bell box (%1 m off)", vector.Distance(alarm.GetOrigin(), top)));
			ctx.True(alarm.GetOrigin()[1] > bellBox.GetOrigin()[1], "the vault alarm is up on the bell box, above where the box is mounted");
			CheckBeacon(ctx, alarm, "vault");
		}

		alarm = LF_Dispatch.StartAlarm(vanBox.GetOrigin(), true);
		ctx.NotNull(alarm, "the cash van alarm spawns");
		if (alarm)
		{
			float roof = alarm.GetOrigin()[1] - van.GetOrigin()[1];
			ctx.True(roof > 1.7 && roof < 2.1, string.Format("the van alarm stands on the roof (%1 m up)", roof));
			ctx.True(vector.DistanceXZ(alarm.GetOrigin(), van.GetOrigin()) < 1.5, "the van alarm is on the van, not the road");
			CheckBeacon(ctx, alarm, "van");
		}

		LF_Dispatch.StopAllAlarms();
		LF_Dispatch.ResetStatics();
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckBeacon(LF_TestContext ctx, IEntity alarm, string where)
	{
		IEntity beacon = alarm.GetChildren();
		ctx.NotNull(beacon, string.Format("the %1 alarm carries its beacon", where));
		if (!beacon)
			return;

		vector mins;
		vector maxs;
		beacon.GetWorldBounds(mins, maxs);
		float tall = maxs[1] - mins[1];
		ctx.True(tall > 0.1 && tall < 0.3, string.Format("the %1 beacon is beacon-sized (%2 m tall, want 0.1 to 0.3)", where, tall));
		ctx.EqualFloat(alarm.GetOrigin()[1], mins[1], 0.05, string.Format("the %1 beacon sits at the alarm's foot", where));
	}
}
