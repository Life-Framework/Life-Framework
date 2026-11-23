// red-proof: #4645 AutoOpenWanted: `--red` run, the agent-client assertion fails when the client looks like a player's (RED-OK line in the PR); earlier: observed 2026-11-23: the new CanAutoOpen(..., dialogueOpen) and CanOpenByKey assertions do not compile on the unfixed code (red); `tools/cli test --no-build --only LF_Test_WelcomeGuide,LF_Test_FirstHoursGuide` green after the fix (passed=2).
//! Welcome guide rules, no world: a new character's guide is pending once, a repeat mark is one mark,
//! an empty id is refused, "Don't show again" clears it for good, export and apply round-trip the set and
//! skip bad rows, the auto open waits for a standing body with nothing else on screen, and each
//! waypoint answer has its own toast.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_WelcomeGuide : LF_Test
{
	static const string ID = "test-welcome-guide-a";
	static const string OTHER = "test-welcome-guide-b";

	override string GetName()
	{
		return "quests/welcome-guide";
	}

	override void Run(LF_TestContext ctx)
	{
		array<ref LF_WelcomeGuideRow> saved = LF_WelcomeGuide.ExportAll();
		LF_WelcomeGuide.ApplyAll(null);

		ctx.False(LF_WelcomeGuide.MarkPending(""), "a character with no id is not marked");
		ctx.Equal(0, LF_WelcomeGuide.Count(), "a refused mark changes nothing");
		ctx.False(LF_WelcomeGuide.IsPending(ID), "a character never marked is not pending");

		// RedProof(): the first-spawn mark is skipped, so the pending assertions fail (`--red`).
		if (!RedProof())
			LF_WelcomeGuide.MarkPending(ID);

		ctx.True(LF_WelcomeGuide.IsPending(ID), "the first spawn makes the guide pending");
		LF_WelcomeGuide.MarkPending(ID);
		ctx.Equal(1, LF_WelcomeGuide.Count(), "marking twice is one mark");
		ctx.False(LF_WelcomeGuide.IsPending(OTHER), "another character is not pending");

		array<ref LF_WelcomeGuideRow> rows = LF_WelcomeGuide.ExportAll();
		rows.Insert(LF_WelcomeGuideRow.Create(""));
		rows.Insert(LF_WelcomeGuideRow.Create(ID));
		rows.Insert(LF_WelcomeGuideRow.Create(OTHER));
		ctx.Equal(2, LF_WelcomeGuide.ApplyAll(rows), "apply skips an empty id and a repeat");
		ctx.Equal(2, LF_WelcomeGuide.ApplyAll(rows), "applying twice ends in the same set");
		ctx.True(LF_WelcomeGuide.IsPending(ID) && LF_WelcomeGuide.IsPending(OTHER), "both characters are pending after the apply");

		ctx.True(LF_WelcomeGuide.Dismiss(ID), "Don't show again clears the mark");
		ctx.False(LF_WelcomeGuide.IsPending(ID), "a dismissed guide no longer opens by itself");
		ctx.False(LF_WelcomeGuide.Dismiss(ID), "a second dismiss finds nothing");
		ctx.True(LF_WelcomeGuide.IsPending(OTHER), "dismissing one character leaves the other");
		ctx.False(LF_WelcomeGuide.Dismiss(""), "an empty id dismisses nothing");

		ctx.True(LF_WelcomeGuide.CanAutoOpen(true, true, false, false, false, false), "a standing body with a clear screen gets the guide");
		ctx.False(LF_WelcomeGuide.CanAutoOpen(false, false, false, false, false, false), "no body, no guide");
		ctx.False(LF_WelcomeGuide.CanAutoOpen(true, false, false, false, false, false), "a dead body gets no guide");
		ctx.False(LF_WelcomeGuide.CanAutoOpen(true, true, true, false, false, false), "not while getting out of bed");
		ctx.False(LF_WelcomeGuide.CanAutoOpen(true, true, false, true, false, false), "not over another menu");
		ctx.False(LF_WelcomeGuide.CanAutoOpen(true, true, false, false, true, false), "not over another sheet");
		ctx.False(LF_WelcomeGuide.CanAutoOpen(true, true, false, false, false, true), "not over a conversation: the clerk scene keeps the confirm key");
		// A client an agent drives gets no auto open (#4645); RedProof(): the client looks like a real player's, so it would open.
		bool agentClient = true;
		if (RedProof())
			agentClient = false;

		ctx.False(LF_WelcomeGuide.AutoOpenWanted(agentClient, false), "an agent session client never gets the guide by itself: it covers every picture");
		ctx.True(LF_WelcomeGuide.AutoOpenWanted(agentClient, true), "an agent session client that opted in (the guide proof) gets it");
		ctx.True(LF_WelcomeGuide.AutoOpenWanted(false, false), "a real player's client always gets the guide on a first join");
		ctx.False(LF_WelcomeGuide.CanOpenByKey(true), "the guide key does nothing while a conversation is up");
		ctx.True(LF_WelcomeGuide.CanOpenByKey(false), "the guide key opens the sheet with no conversation");

		ctx.EqualStr("#LF-WelcomeGuide_WaypointSet", LF_WelcomeGuide.WaypointToastKey(LF_EWelcomeWaypoint.SET), "a set waypoint says where");
		ctx.EqualStr("#LF-WelcomeGuide_WaypointNoStep", LF_WelcomeGuide.WaypointToastKey(LF_EWelcomeWaypoint.NO_STEP), "a finished chain says so");
		ctx.EqualStr("#LF-WelcomeGuide_WaypointRefused", LF_WelcomeGuide.WaypointToastKey(LF_EWelcomeWaypoint.NO_BODY), "no body is a refusal");
		ctx.EqualStr("#LF-WelcomeGuide_WaypointRefused", LF_WelcomeGuide.WaypointToastKey(LF_EWelcomeWaypoint.REFUSED), "a rate limit is a refusal");

		LF_WelcomeGuide.ResetStatics();
		ctx.Equal(0, LF_WelcomeGuide.Count(), "the static reset clears the set");
		LF_WelcomeGuide.ApplyAll(saved);
	}
}
