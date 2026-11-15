// red-proof: observed 2026-11-16 with the batched red run (--red LF_Test_WorkbenchPlay): `RED-OK workbench/play-decisions`, failing `- Workbench with the plugin's request: fast play`.
// tier: LOGIC
//! Workbench play decisions (LF_WorkbenchPlay): fast play only inside Workbench with a fresh "fast"
//! request; a missing persistence system is expected only in Workbench play with no mission header;
//! the stand-in mission header is the first one for the open world that lists overlays.
class LF_Test_WorkbenchPlay : LF_Test
{
	override string GetName()
	{
		return "workbench/play-decisions";
	}

	override void Run(LF_TestContext ctx)
	{
		bool fast = LF_WorkbenchPlay.FastPlayDecision(true, "fast");
		if (RedProof())
			fast = false;

		ctx.True(fast, "Workbench with the plugin's request: fast play");
		ctx.True(LF_WorkbenchPlay.FastPlayDecision(true, "fast\n"), "a trailing newline in the request file still counts");
		ctx.True(!LF_WorkbenchPlay.FastPlayDecision(false, "fast"), "not Workbench (dedicated server, published build): never fast play, even with the file");
		ctx.True(!LF_WorkbenchPlay.FastPlayDecision(true, "used"), "a request already used: the full onboarding");
		ctx.True(!LF_WorkbenchPlay.FastPlayDecision(true, ""), "no request (plain Play): the full onboarding");
		ctx.True(!LF_WorkbenchPlay.IsWorkbench(), "the test server is not a Workbench build");
		ctx.True(!LF_WorkbenchPlay.IsFastPlay(), "so on the test server fast play is off");

		ctx.True(LF_WorkbenchPlay.PersistenceMissingExpected(true, false), "Workbench play with no mission header: no persistence system is expected");
		ctx.True(!LF_WorkbenchPlay.PersistenceMissingExpected(true, true), "Workbench with a mission header: still an error");
		ctx.True(!LF_WorkbenchPlay.PersistenceMissingExpected(false, false), "a server with no mission header: still an error");

		// Made-up worlds (no real resource): a header writes "{GUID}path", the game "$addon:path".
		string guid = "{" + "AAAA000000000001}";
		array<string> worlds = {};
		worlds.Insert(guid + "Worlds/TestDebug/TestDebug.ent");
		worlds.Insert(guid + "Worlds/TestTown/TestTown.ent");
		worlds.Insert(guid + "Worlds/TestTown/TestTown.ent");
		array<int> overlays = {3, 2, 4};
		ctx.Equal(2, LF_WorkbenchPlay.PickFallbackHeader(worlds, overlays, "$LF_TestPack:worlds/TestTown/TestTown.ent"), "the open world's header with the most overlays (the trimmed one before it is passed over)");
		ctx.Equal(0, LF_WorkbenchPlay.PickFallbackHeader(worlds, overlays, "Worlds/TestDebug/TestDebug.ent"), "the other world's header");
		ctx.Equal(-1, LF_WorkbenchPlay.PickFallbackHeader(worlds, overlays, "Worlds/Other/Other.ent"), "no header for the open world: none");
		array<int> noOverlays = {0, 0, 0};
		ctx.Equal(-1, LF_WorkbenchPlay.PickFallbackHeader(worlds, noOverlays, "Worlds/TestDebug/TestDebug.ent"), "a header with no overlays is no stand-in");
		ctx.Equal(-1, LF_WorkbenchPlay.PickFallbackHeader(worlds, overlays, ""), "no open world: none");
	}
}
