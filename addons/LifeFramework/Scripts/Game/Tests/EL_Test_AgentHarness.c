// red-proof: make IsMenuAction accept MoveForward, then "movement input is refused" fails.
// DecideApplyAction(false, false) asserted true fails "a non-owner client does not apply".
// tier: LOGIC
class EL_Test_AgentHarness : EL_Test
{
	override string GetName()
	{
		return "agent-harness/command-parse";
	}

	override void Run(EL_TestContext ctx)
	{
		EL_AgentCommand cmd;

		ctx.True(EL_AgentCommand.Parse("4 use EL_ShopAction Trader_01", cmd), "named use parses");
		ctx.Equal(4, cmd.Id, "use id");
		ctx.EqualStr("use", cmd.Verb, "use verb");
		ctx.EqualStr("EL_ShopAction", cmd.Arg1, "use class");
		ctx.EqualStr("Trader_01", cmd.Arg2, "use entity");

		ctx.True(EL_AgentCommand.Parse("9 interact confirm", cmd), "interact confirm parses");
		ctx.EqualStr("confirm", cmd.Arg1, "interact step");

		ctx.False(EL_AgentCommand.Parse("2 interact jump", cmd), "unknown interact step is rejected");
		ctx.EqualStr("bad_interact", cmd.Error, "bad interact error");

		ctx.True(EL_AgentCommand.Parse("5 input MenuSelect", cmd), "menu select parses");
		ctx.False(EL_AgentCommand.Parse("1 input MoveForward", cmd), "movement input is refused");
		ctx.EqualStr("action_not_allowed", cmd.Error, "movement refusal reason");

		ctx.True(EL_AgentCommand.Parse("8 capture world atm_pad 128 1 133", cmd), "world capture parses");
		ctx.EqualStr("world", cmd.Arg1, "capture mode");
		ctx.EqualStr("atm_pad", cmd.Arg2, "capture name");
		ctx.EqualFloat(128, cmd.PosX, 0.01, "capture x");
		ctx.EqualFloat(1, cmd.PosY, 0.01, "capture y");
		ctx.EqualFloat(133, cmd.PosZ, 0.01, "capture z");

		ctx.True(EL_AgentCommand.Parse("6 open DebugMenu show", cmd), "force-open parses");
		ctx.EqualStr("show", cmd.Arg2, "open mode");
		ctx.False(EL_AgentCommand.Parse("6 open DebugMenu poke", cmd), "unknown open mode is rejected");
		ctx.EqualStr("bad_open_mode", cmd.Error, "bad open mode error");

		ctx.True(EL_AgentCommand.Parse("10 stand ATM_Debug", cmd), "stand parses");
		ctx.EqualStr("ATM_Debug", cmd.Arg1, "stand entity");

		ctx.True(EL_AgentCommand.Parse("14 ui", cmd), "ui list parses");
		ctx.True(EL_AgentCommand.Parse("12 click CivilianButton", cmd), "click parses");
		ctx.EqualStr("CivilianButton", cmd.Arg1, "click widget");
		ctx.EqualStr("0", cmd.Arg2, "click default index");
		ctx.True(EL_AgentCommand.Parse("13 click BuyButton 1", cmd), "indexed click parses");
		ctx.EqualStr("1", cmd.Arg2, "click index");
		ctx.False(EL_AgentCommand.Parse("13 click BuyButton next", cmd), "click index must be digits");
		ctx.EqualStr("bad_index", cmd.Error, "bad click index error");

		ctx.False(EL_AgentCommand.Parse("", cmd), "empty line is rejected");

		ctx.True(EL_NetworkUtils.DecideApplyAction(true, false), "the owner applies the action");
		ctx.False(EL_NetworkUtils.DecideApplyAction(false, false), "a non-owner client does not apply");
		ctx.True(EL_NetworkUtils.DecideApplyAction(false, true), "the server applies when no peer owns the prop");
	}
}
