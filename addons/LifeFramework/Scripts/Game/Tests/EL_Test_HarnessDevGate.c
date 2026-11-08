// red-proof: made EL_AgentHarness.ServerStand skip the gate (`if (false && !gateOpen)`) and ran
// `tools\cli test --no-build --tier fast` (2026-11-08). Observed:
//   [ELTEST]   - stand is refused with dev_gate_closed (expected 'dev_gate_closed', got 'no_character')
//   [ELTEST] SUMMARY tier=fast passed=100 failed=1 total=101
// Restored the check and the suite went green (101/101).

// tier: LOGIC
//! M1-05: a multi-client session drives clients through the harness RPCs on a dedicated server.
//! Those RPCs must stay behind EL_DevGate. On the playtest mission a non-admin's stand is refused
//! before the server touches the character or the world, and the refusal is a named token the
//! harness reports, not a silent timeout.
class EL_Test_HarnessDevGate : EL_Test
{
	static const string PLAYTEST_MISSION = "{1054CC8646357708}Missions/LF_PlaytestTown.conf";

	override string GetName()
	{
		return "harness/dev-gate-dedicated";
	}

	override void Run(EL_TestContext ctx)
	{
		MissionHeader header = MissionHeader.ReadMissionHeader(PLAYTEST_MISSION);
		ctx.True(header != null, "playtest mission header resolves");
		if (!header)
			return;

		// DebugWorld's game mode opts in; the playtest header closes the gate for a non-admin.
		bool devTools = LF_MissionHeader.ResolveDevToolsEnabled(true, header);
		bool gateOpen = EL_DevGate.IsAllowed(devTools, false);
		ctx.False(gateOpen, "the dev gate is closed for a non-admin on the playtest mission");

		vector destination;
		string refused = EL_AgentHarness.ServerStand(null, "Roadmap_Fuel_Till", gateOpen, destination);
		ctx.EqualStr(EL_AgentHarness.REFUSED_DEV_GATE, refused, "stand is refused with dev_gate_closed");

		// With the gate open the same call goes past the gate (and stops at the missing character),
		// so the refusal above is the gate and not a side effect of the null character.
		string open = EL_AgentHarness.ServerStand(null, "Roadmap_Fuel_Till", true, destination);
		ctx.EqualStr("no_character", open, "an open gate reaches the character check");
	}
}
