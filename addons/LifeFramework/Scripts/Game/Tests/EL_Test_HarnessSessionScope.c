// red-proof: made EL_AgentCommand.SessionAccepts return true and ran
// `tools\cli test --no-build --only EL_Test_HarnessSessionScope,...` (2026-11-08). Observed:
//   [ELTEST]   - a client refuses another session's command
//   [ELTEST]   - a session client refuses an unaddressed command
//   [ELTEST]   - Workbench refuses a command addressed to a session
// Restored the comparison and the test passed.

// tier: LOGIC
//! A harness command from one worktree reached another worktree's session client (the session
//! profiles share one dir, and a new session started between two runs). Each `session.mjs up`
//! now names its clients with -lfSession <id>, every command carries s:<id>, and a client runs
//! only commands addressed to its own session.
class EL_Test_HarnessSessionScope : EL_Test
{
	override string GetName()
	{
		return "agent-harness/session-scope";
	}

	override void Run(EL_TestContext ctx)
	{
		EL_AgentCommand cmd;
		ctx.True(EL_AgentCommand.Parse("7 s:a1b2c3 click SignButton", cmd), "an addressed command parses");
		ctx.EqualStr("a1b2c3", cmd.Session, "the session token is read");
		ctx.EqualStr("click", cmd.Verb, "the verb follows the session token");
		ctx.EqualStr("SignButton", cmd.Arg1, "arguments shift past the session token");

		ctx.True(EL_AgentCommand.Parse("8 look", cmd), "a command without a session parses");
		ctx.EqualStr("", cmd.Session, "no token is no session");

		ctx.False(EL_AgentCommand.Parse("9 s:a1b2c3", cmd), "a session token with no verb is rejected");
		ctx.False(EL_AgentCommand.Parse("9 s:a-b look", cmd), "a session token outside letters and digits is rejected");
		ctx.EqualStr("bad_session", cmd.Error, "bad session error");

		ctx.True(EL_AgentCommand.SessionAccepts("a1b2c3", "a1b2c3"), "a client runs its own session's command");
		ctx.False(EL_AgentCommand.SessionAccepts("a1b2c3", "ffff00"), "a client refuses another session's command");
		ctx.False(EL_AgentCommand.SessionAccepts("a1b2c3", ""), "a session client refuses an unaddressed command");
		ctx.False(EL_AgentCommand.SessionAccepts("", "a1b2c3"), "Workbench refuses a command addressed to a session");
		ctx.True(EL_AgentCommand.SessionAccepts("", ""), "Workbench runs unaddressed commands");
	}
}
