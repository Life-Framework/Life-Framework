// red-proof: observed 2026-11-15 with LF_MedicDutyDesk.et inheriting the police desk again: `tools/cli test --no-build --only LF_Test_DutyDeskActions` failed `- the MEDIC desk Annex_Hospital_Duty_Desk offers only its own job's actions (expected 0, got 2)` and `- no police application is offered at the hospital desk`. Prefab restored; green.
//------------------------------------------------------------------------------------------------
//! jobs/duty-desk-actions: every placed duty desk offers only its own job's actions. The hospital
//! desk once inherited the police desk prefab and showed "Apply to the police" and "Review
//! applications"; a job's own actions never appear at another job's desk.
// tier: WORLD
class LF_Test_DutyDeskActions : LF_Test
{
	override string GetName()
	{
		return "jobs/duty-desk-actions";
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: whether an action class belongs to another job. The police application actions are
	//! police only, the treatment bill is medic only; the clock, the fine and the generic payments
	//! are offered at any duty desk.
	protected static bool IsForeign(string job, BaseUserAction action)
	{
		if (LF_PoliceApplyAction.Cast(action) || LF_PoliceReviewAction.Cast(action))
			return job != LF_Workplace.JOB_POLICE;

		if (LF_TreatmentBillPayAction.Cast(action))
			return job != LF_Workplace.JOB_MEDIC;

		return false;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		// The pure offer rule.
		ctx.True(LF_Workplace.JobOffersPolice(LF_Workplace.JOB_POLICE, true), "a police desk offers the police actions");
		ctx.False(LF_Workplace.JobOffersPolice(LF_Workplace.JOB_MEDIC, true), "a medic desk does not offer the police actions");
		ctx.False(LF_Workplace.JobOffersPolice("BAKER", true), "no other job's desk offers the police actions");
		ctx.True(LF_Workplace.JobOffersPolice("", false), "a clock outside any workplace keeps the police actions");

		array<string> jobs = {LF_Workplace.JOB_POLICE, LF_Workplace.JOB_MEDIC};
		int checkedDesks = 0;
		foreach (string job : jobs)
		{
			array<IEntity> desks = {};
			LF_Workplace.FindAll(job, desks);
			foreach (IEntity desk : desks)
			{
				IEntity clock = LF_ActionSite.FindNear(desk.GetOrigin(), LF_Workplace.CLOCK_RANGE, "LF_DutyAction");
				ctx.NotNull(clock, string.Format("the %1 desk %2 has a punch clock", job, desk.GetName()));
				if (!clock)
					continue;

				BaseActionsManagerComponent manager = BaseActionsManagerComponent.Cast(clock.FindComponent(BaseActionsManagerComponent));
				ctx.NotNull(manager, string.Format("the %1 desk's clock has an actions manager", job));
				if (!manager)
					continue;

				array<BaseUserAction> actions = {};
				manager.GetActionsList(actions);
				int foreign = 0;
				foreach (BaseUserAction action : actions)
				{
					if (IsForeign(job, action))
					{
						foreign++;
						LF_Debug.Warn("Jobs", string.Format("%1 desk %2 carries a foreign action: %3", job, desk.GetName(), action.Type().ToString()));
					}
				}

				ctx.Equal(0, foreign, string.Format("the %1 desk %2 offers only its own job's actions", job, desk.GetName()));
				checkedDesks++;
			}
		}

		ctx.True(checkedDesks >= 1, "at least one duty desk is placed to check");

		// The hospital desk by name: no police application anywhere on its clock.
		IEntity hospital = GetGame().GetWorld().FindEntityByName(LF_TestAnnex.HOSPITAL_DUTY_DESK);
		ctx.NotNull(hospital, "the annex hospital duty desk is placed");
		if (hospital)
			ctx.True(LF_ActionSite.FindNear(hospital.GetOrigin(), LF_Workplace.CLOCK_RANGE, "LF_PoliceApplyAction") == null, "no police application is offered at the hospital desk");
	}
}
