// red-proof: observed 2026-11-08 with `tools\cli test --no-build --tier all`: adding
// `SetEventMask(owner, EntityEvent.FRAME);` back to EL_PlayerJobComponent.OnPostInit printed
// [ELTEST]   - the job component runs no per-frame paycheck clock (expected 0, got 8); restored, green.

// tier: WORLD
//! A spawned character is the only way to hold an EL_PlayerJobComponent, so this is WORLD.
//! The old paycheck ran on the component's frame event; a component with no frame event and
//! no cash after spawn cannot mint over time.
class EL_Test_NoPassivePaycheck : EL_Test
{
	override string GetName()
	{
		return "jobs/no-passive-paycheck";
	}

	override void Run(EL_TestContext ctx)
	{
		IEntity character = EL_ExploitGuardsWorldFixture.SpawnCharacter(ctx, "85 0 85", false);
		if (!character)
			return;

		EL_PlayerJobComponent jobs = EL_Component<EL_PlayerJobComponent>.Find(character);
		ctx.True(jobs != null, "test character carries EL_PlayerJobComponent");
		if (ctx.FailureCount() > 0)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(character);
			return;
		}

		ctx.True(jobs.GetJob() == EL_EJobType.UNEMPLOYED, "a fresh character is unemployed");
		ctx.Equal(0, jobs.GetEventMask() & EntityEvent.FRAME, "the job component runs no per-frame paycheck clock");
		ctx.Equal(0, EL_MoneyUtils.GetCash(character), "an unemployed character with no completed run holds no cash");

		SCR_EntityHelper.DeleteEntityAndChildren(character);
	}
}
