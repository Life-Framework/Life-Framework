// red-proof: observed 2026-11-08 with `tools\cli test --no-build --tier all`: adding
// `SetEventMask(owner, EntityEvent.FRAME);` back to LF_PlayerJobComponent.OnPostInit printed
// [LFTEST]   - the job component runs no per-frame paycheck clock (expected 0, got 8); restored, green.

// tier: WORLD
//! A spawned character is the only way to hold an LF_PlayerJobComponent, so this is WORLD.
//! The old paycheck ran on the component's frame event; a component with no frame event and
//! no cash after spawn cannot mint over time.
class LF_Test_NoPassivePaycheck : LF_Test
{
	override string GetName()
	{
		return "jobs/no-passive-paycheck";
	}

	override void Run(LF_TestContext ctx)
	{
		IEntity character = LF_ExploitGuardsWorldFixture.SpawnCharacter(ctx, "85 0 85", false);
		if (!character)
			return;

		LF_PlayerJobComponent jobs = LF_Component<LF_PlayerJobComponent>.Find(character);
		ctx.True(jobs != null, "test character carries LF_PlayerJobComponent");
		if (ctx.FailureCount() > 0)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(character);
			return;
		}

		ctx.True(jobs.GetJob() == LF_EJobType.UNEMPLOYED, "a fresh character is unemployed");
		ctx.Equal(0, jobs.GetEventMask() & EntityEvent.FRAME, "the job component runs no per-frame paycheck clock");
		ctx.Equal(0, LF_MoneyUtils.GetCash(character), "an unemployed character with no completed run holds no cash");

		SCR_EntityHelper.DeleteEntityAndChildren(character);
	}
}
