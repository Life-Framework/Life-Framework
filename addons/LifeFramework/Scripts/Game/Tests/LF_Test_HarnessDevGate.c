// red-proof: made LF_AgentHarness.ServerStand skip the gate (`if (false && !gateOpen)`) and ran
// `tools\cli test --no-build --tier fast` (2026-11-08). Observed:
//   [LFTEST]   - stand is refused with dev_gate_closed (expected 'dev_gate_closed', got 'no_character')
//   [LFTEST] SUMMARY tier=fast passed=100 failed=1 total=101
// Restored the check and the suite went green (101/101).
// red-proof (LF_HARNESS, #108): made LF_DevGate.HarnessBuild return true without the define
// (2026-11-08). Observed:
//   [LFTEST]   - a build without LF_HARNESS keeps the hosted harness gate closed
// Restored it and the test passed.

// tier: LOGIC
//! M1-05: a multi-client session drives clients through the harness RPCs on a dedicated server.
//! Those RPCs must stay behind LF_DevGate. On the playtest mission a non-admin's stand is refused
//! before the server touches the character or the world, and the refusal is a named token the
//! harness reports, not a silent timeout.
class LF_Test_HarnessDevGate : LF_Test
{
	static const string PLAYTEST_MISSION = "{43902474F5601DAA}Missions/LF_HostedTest.conf"; // core fixture: the playtest headers are pack (#323)

	override string GetName()
	{
		return "harness/dev-gate-dedicated";
	}

	override void Run(LF_TestContext ctx)
	{
		MissionHeader header = MissionHeader.ReadMissionHeader(PLAYTEST_MISSION);
		ctx.True(header != null, "playtest mission header resolves");
		if (!header)
			return;

		// DebugWorld's game mode opts in; the playtest header closes the gate for a non-admin.
		bool devTools = LF_MissionHeader.ResolveDevToolsEnabled(true, header);
		bool gateOpen = LF_DevGate.IsAllowed(devTools, false);
		ctx.False(gateOpen, "the dev gate is closed for a non-admin on the playtest mission");

		vector destination;
		string refused = LF_AgentHarness.ServerStand(null, LF_TestAnnex.ORG_DESK, gateOpen, destination);
		ctx.EqualStr(LF_AgentHarness.REFUSED_DEV_GATE, refused, "stand is refused with dev_gate_closed");

		// With the gate open the same call goes past the gate (and stops at the missing character),
		// so the refusal above is the gate and not a side effect of the null character.
		string open = LF_AgentHarness.ServerStand(null, LF_TestAnnex.ORG_DESK, true, destination);
		ctx.EqualStr("no_character", open, "an open gate reaches the character check");

		// The harness opens its gate on the playtest mission only in a build compiled with
		// -scrDefine LF_HARNESS (session.mjs up --mission playtest). The suite, like tools\cli host
		// start, is built without it, so nothing but that define can open the hosted gate.
		ctx.False(LF_DevGate.HarnessBuild(), "a build without LF_HARNESS keeps the hosted harness gate closed");
	}
}
