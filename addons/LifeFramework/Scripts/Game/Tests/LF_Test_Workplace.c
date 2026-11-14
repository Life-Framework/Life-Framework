// red-proof: observed 2026-11-14 with `tools/cli test --no-build --only LF_Test_Workplace`: expecting LF_Faction.POLICE for the MEDIC job failed it with `- MEDIC clocks in the medic role (expected 1, got 2)`; an unparented clock (workplace looked up through GetParent only) failed `- the clock finds its workplace through the desk` and the old `medic/duty-desk` with `(expected 2, got 1)`. Restored; green.
//------------------------------------------------------------------------------------------------
//! jobs/workplace (#1111): the generic workplace prefab. The hospital desk is the LF_Workplace
//! prefab with the job id MEDIC set: its punch clock takes the duty role from the job id (no role
//! on the action), the desk registers as a "Workplace" place, and a job without a duty role has no
//! role at all, so its clock is not offered.
// tier: WORLD
class LF_Test_Workplace : LF_Test
{
	override string GetName()
	{
		return "jobs/workplace";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_Faction role;
		ctx.True(LF_Workplace.RoleFor(LF_Workplace.JOB_MEDIC, role), "MEDIC is a duty job");
		ctx.Equal(LF_Faction.MEDIC, role, "MEDIC clocks in the medic role");
		ctx.True(LF_Workplace.RoleFor(LF_Workplace.JOB_POLICE, role), "POLICE is a duty job");
		ctx.Equal(LF_Faction.POLICE, role, "POLICE clocks in the police role");
		ctx.False(LF_Workplace.RoleFor("BAKER", role), "a job with no duty role has no clock-in role");
		ctx.False(LF_Workplace.RoleFor("", role), "an empty job id has no role");

		IEntity desk = GetGame().GetWorld().FindEntityByName(LF_TestAnnex.HOSPITAL_DUTY_DESK);
		ctx.NotNull(desk, "the hospital duty desk is placed");
		if (!desk)
			return;

		LF_WorkplaceComponent workplace = LF_WorkplaceComponent.Cast(desk.FindComponent(LF_WorkplaceComponent));
		ctx.NotNull(workplace, "the desk carries the workplace component");
		if (!workplace)
			return;

		ctx.True(workplace.GetJob() == LF_Workplace.JOB_MEDIC, "the desk sets the job id MEDIC");
		ctx.False(workplace.HasNpcSlot(), "the hospital desk leaves the NPC slot empty");

		IEntity placed = LF_Places.Find(LF_Workplace.PLACE_KIND, workplace.GetPlaceId());
		ctx.True(placed == desk, "the workplace is registered as a place under its entity name");

		array<IEntity> medics = {};
		LF_Workplace.FindAll(LF_Workplace.JOB_MEDIC, medics);
		ctx.True(medics.Find(desk) != -1, "FindAll(MEDIC) lists the desk");

		IEntity clock = LF_ActionSite.FindNear(desk.GetOrigin(), 1.5, "LF_DutyAction");
		ctx.NotNull(clock, "the workplace punch clock carries the clock-in action");
		if (!clock)
			return;

		ctx.True(LF_Workplace.FindFor(clock) == workplace, "the clock finds its workplace beside the desk");
	}
}
