// red-proof: commented out the bar replacement in LF_AgentCommand.UiEntry and ran
// `tools\cli test --no-build --only LF_Test_HarnessScreenText,...` (2026-11-08). Observed:
//   [LFTEST]   - a bar or a line break cannot split the result line (expected 'line:Status=Paid / thanks two lines', got 'line:Status=Paid | thanks two lines')
// Restored it and the test passed.

// tier: LOGIC
//! #108: the harness reads a UI kit sheet the way a player reads it (`ui text`: buttons with their
//! labels, fields with what is typed, printed lines), and `stand` turns the character to face the
//! entity so the interaction key aims at it.
class LF_Test_HarnessScreenText : LF_Test
{
	override string GetName()
	{
		return "agent-harness/screen-text";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_AgentCommand cmd;
		ctx.True(LF_AgentCommand.Parse("14 ui", cmd), "plain ui parses");
		ctx.EqualStr("", cmd.Arg1, "plain ui lists names only");
		ctx.True(LF_AgentCommand.Parse("15 ui text", cmd), "ui text parses");
		ctx.EqualStr("text", cmd.Arg1, "ui text mode");
		ctx.False(LF_AgentCommand.Parse("16 ui poke", cmd), "an unknown ui mode is rejected");
		ctx.EqualStr("bad_ui_mode", cmd.Error, "bad ui mode error");

		ctx.EqualStr("line:Balance=Balance: $400", LF_AgentCommand.UiEntry("line", "Balance", "Balance: $400"), "an entry is kind:name=text");
		ctx.EqualStr("line:Status=Paid / thanks two lines", LF_AgentCommand.UiEntry("line", "Status", "Paid | thanks\ntwo lines"), "a bar or a line break cannot split the result line");
		ctx.EqualStr("button:IdClose=", LF_AgentCommand.UiEntry("button", "IdClose", ""), "an unlabelled button keeps its name");

		// stand puts the body at +X of the entity; facing it is yaw -90 (CameraStand's convention).
		ctx.EqualFloat(-90, LF_AgentHarness.StandYaw(), 0.5, "stand faces the entity it stands beside");
	}
}
