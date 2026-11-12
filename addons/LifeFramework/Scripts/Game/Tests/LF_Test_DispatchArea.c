// red-proof: Observed 2026-11-10 with `tools\cli test --no-build --only LF_Test_DispatchArea`: expecting 3 test-call actions on the console failed it with `- the console offers a police and a medic test call (expected 3, got 2)`. Restored; green.
//------------------------------------------------------------------------------------------------
//! debugworld/dispatch-area: the dispatch area stands in the playtest town (a console with a
//! test police call and a test medic call, a police board and a medic board that can take a call)
//! and the dedicated test till offers the robbery action.
// tier: WORLD
class LF_Test_DispatchArea : LF_Test
{
	override string GetName()
	{
		return "debugworld/dispatch-area";
	}

	protected int CountOf(IEntity entity, typename actionType)
	{
		int count = 0;
		BaseActionsManagerComponent manager = BaseActionsManagerComponent.Cast(entity.FindComponent(BaseActionsManagerComponent));
		if (!manager)
			return 0;

		array<BaseUserAction> actions = {};
		manager.GetActionsList(actions);
		foreach (BaseUserAction action : actions)
		{
			if (action.Type().IsInherited(actionType))
				count++;
		}
		return count;
	}

	override void Run(LF_TestContext ctx)
	{
		IEntity console = GetGame().GetWorld().FindEntityByName(LF_TestAnnex.DISPATCH_CONSOLE);
		ctx.NotNull(console, "Annex_Dispatch_Console is placed");
		IEntity radio = null;
		if (console)
			radio = LF_ActionSite.FindNear(console.GetOrigin(), 1.5, "LF_DebugDispatchAction");
		ctx.NotNull(radio, "the console's radio carries the test-call action");
		if (radio)
			ctx.Equal(2, CountOf(radio, LF_DebugDispatchAction), "the console offers a police and a medic test call");

		IEntity police = GetGame().GetWorld().FindEntityByName(LF_TestAnnex.DISPATCH_POLICE_BOARD);
		ctx.NotNull(police, "Annex_Dispatch_PoliceBoard is placed");
		if (police)
			ctx.Equal(1, CountOf(police, LF_ServiceTakeAction), "the police board takes calls");

		IEntity medic = GetGame().GetWorld().FindEntityByName(LF_TestAnnex.DISPATCH_MEDIC_BOARD);
		ctx.NotNull(medic, "Annex_Dispatch_MedicBoard is placed");
		if (medic)
			ctx.Equal(1, CountOf(medic, LF_ServiceTakeAction), "the medic board takes calls");

		IEntity till = GetGame().GetWorld().FindEntityByName(LF_TestAnnex.TEST_TILL);
		ctx.NotNull(till, "Annex_Test_Till is placed");
		if (till)
			ctx.Equal(1, CountOf(till, LF_RobAction), "the test till offers the robbery action");
	}
}
