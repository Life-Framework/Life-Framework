// red-proof: Observed 2026-11-08 with `tools\cli test --no-build --only LF_Test_Dispatch,LF_Test_DispatchAlarm`.
// LF_Dispatch.StartAlarm calling StopAlarmsNear(site, -1) (never replaces) failed dispatch/site-alarm with
// `[LFTEST]   - a newer alarm at the same site replaces the older one` and
// `- still one alarm at the site (expected 1, got 2)`. Restored; green.

//------------------------------------------------------------------------------------------------
//! dispatch/site-alarm (M1-20): both alarm prefabs spawn as replicated entities at the site,
//! the loud one is louder (its own level, a longer hear range), a newer alarm at the same site
//! replaces the older one, and stopping removes it.
// tier: WORLD
class LF_Test_DispatchAlarm : LF_Test
{
	override string GetName()
	{
		return "dispatch/site-alarm";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_Dispatch.ResetStatics();
		ctx.True(Replication.IsServer(), "the alarm test runs on the server");

		vector site = "0 0 0";
		IEntity till = GetGame().GetWorld().FindEntityByName(LF_RobAction.TILL_ENTITY_NAME);
		if (till)
			site = till.GetOrigin();
		else
			site = "50 0 50";

		IEntity normal = LF_Dispatch.StartAlarm(site, false);
		ctx.NotNull(normal, "the normal alarm spawns at the site");
		if (!normal)
			return;

		LF_SiteAlarmComponent normalAlarm = LF_SiteAlarmComponent.Cast(normal.FindComponent(LF_SiteAlarmComponent));
		ctx.NotNull(normalAlarm, "the normal alarm carries LF_SiteAlarmComponent");
		ctx.NotNull(normal.FindComponent(RplComponent), "the alarm is replicated, so nearby players get it");
		ctx.True(vector.Distance(normal.GetOrigin(), site) < 0.5, "the alarm stands at the site");
		float normalRange = 0;
		if (normalAlarm)
		{
			ctx.False(normalAlarm.IsLoud(), "the normal alarm is not the loud level");
			normalRange = normalAlarm.GetHearRange();
		}

		ctx.Equal(1, LF_Dispatch.ActiveAlarmCount(), "one alarm sounds");
		EntityID normalId = normal.GetID();

		IEntity loud = LF_Dispatch.StartAlarm(site, true);
		ctx.NotNull(loud, "the loud alarm spawns at the site");
		ctx.True(GetGame().GetWorld().FindEntityByID(normalId) == null, "a newer alarm at the same site replaces the older one");
		ctx.Equal(1, LF_Dispatch.ActiveAlarmCount(), "still one alarm at the site");
		if (!loud)
			return;

		LF_SiteAlarmComponent loudAlarm = LF_SiteAlarmComponent.Cast(loud.FindComponent(LF_SiteAlarmComponent));
		ctx.NotNull(loudAlarm, "the loud alarm carries LF_SiteAlarmComponent");
		if (loudAlarm)
		{
			ctx.True(loudAlarm.IsLoud(), "the loud prefab is the loud level");
			ctx.True(loudAlarm.GetHearRange() > normalRange, "the loud alarm carries further than the normal one");
		}

		LF_Dispatch.StopAlarm(loud.GetID());
		ctx.Equal(0, LF_Dispatch.ActiveAlarmCount(), "a stopped alarm is gone");
		LF_Dispatch.ResetStatics();
	}
}
