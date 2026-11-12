// red-proof: set the expected capture hour to 18.6 and ran `toolsi test --no-build --only LF_Test_HarnessProbe` (2026-11-10). Observed: [LFTEST]   - capture hour (expected 18.6, got 18.5). Restored it and the test passed.
// tier: LOGIC
//! tools\cli shots area: `probe` measures the world (ground, trace, cam, ent) and a world capture
//! takes an optional clock hour that means a clean shot. Both ride the harness line parser.
class LF_Test_HarnessProbe : LF_Test
{
	override string GetName()
	{
		return "agent-harness/probe";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_AgentCommand cmd;
		ctx.True(LF_AgentCommand.Parse("21 probe ground 262.5 255", cmd), "probe ground parses");
		ctx.EqualStr("ground", cmd.Arg1, "probe mode");
		ctx.EqualFloat(262.5, cmd.PosX, 0.001, "probe ground x");
		ctx.EqualFloat(255, cmd.PosZ, 0.001, "probe ground z");

		ctx.True(LF_AgentCommand.Parse("22 probe trace 1 2 3 4 5 6", cmd), "probe trace parses");
		ctx.EqualFloat(2, cmd.PosY, 0.001, "trace start y");
		ctx.EqualFloat(6, cmd.CamZ, 0.001, "trace end z");

		ctx.True(LF_AgentCommand.Parse("23 probe ent Annex_Hospital_Bed_1 270.5 0.2 261.5", cmd), "probe ent parses");
		ctx.EqualStr(LF_TestAnnex.HOSPITAL_BED_1, cmd.Arg2, "probe ent name");
		ctx.EqualFloat(270.5, cmd.PosX, 0.001, "probe ent x");

		ctx.False(LF_AgentCommand.Parse("24 probe ent Bad-Name 1 2 3", cmd), "a bad entity token is rejected");
		ctx.False(LF_AgentCommand.Parse("25 probe trace 1 2 3", cmd), "a short trace is rejected");
		ctx.False(LF_AgentCommand.Parse("26 probe dig 1 2 3", cmd), "an unknown probe mode is rejected");

		ctx.True(LF_AgentCommand.Parse("27 capture world shot_a 10 0 20 -5 12 8 18.5", cmd), "a capture with an hour parses");
		ctx.EqualFloat(18.5, cmd.Hour, 0.001, "capture hour");
		ctx.True(LF_AgentCommand.Parse("29 time 18.5", cmd), "time parses");
		ctx.EqualFloat(18.5, cmd.PosX, 0.001, "time hour");
		ctx.False(LF_AgentCommand.Parse("30 time 25", cmd), "an hour past 24 is rejected");
		ctx.True(LF_AgentCommand.Parse("28 capture world shot_b 10 0 20 -5 12 8", cmd), "a capture without an hour parses");
		ctx.True(cmd.Hour < 0, "no hour leaves the clock alone");
	}
}
