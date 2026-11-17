// red-proof: 2026-11-17, cli test --no-build --red LF_Test_MapKnown,LF_Test_MapKnownPersist: RED PROOF OK (RedProof() zeroes the case distances and ask counts and moves the sixth ask past the window, so the range and rate cases fail). Green without it.
//! One case of the discovery table, as the engine's class reader fills it.
class LF_Test_MapKnownCaseInput
{
	string action;
	string known;
	bool live;
	float distance;
	float radius;
	int recent;
};

//------------------------------------------------------------------------------------------------
class LF_Test_MapKnownCase
{
	string name;
	ref LF_Test_MapKnownCaseInput input;
	string expect;
};

//------------------------------------------------------------------------------------------------
// tier: LOGIC
//! map/known: discovery (#2241, everon-map.md section 4). The case table
//! Configs/Map/Rules/discovery.cases.json runs through LF_MapKnownRules.Decide here and through the
//! node twin tools/map/rules/discovery.mjs in validate, so the two cannot drift (the engine reads the
//! generated copy LF_MapKnownCasesData). Then the store: a visit out of range is refused, the sixth
//! ask in a minute is refused, hear then visit, hear never lowers visited, and the client's local set.
class LF_Test_MapKnown : LF_Test
{
	protected static const string CHARACTER = "mapknown-test-character";
	protected static const int PLAYER = 990241;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "map/known";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		RunCases(ctx);

		array<ref LF_MapKnownRow> before = LF_MapKnown.ExportAll();
		LF_MapKnown.ApplyAll(null);
		RunStore(ctx);
		RunLocal(ctx);
		LF_MapKnown.ApplyAll(before);
		LF_MapKnown.SetLocalAll(null, null);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunCases(LF_TestContext ctx)
	{
		SCR_JsonLoadContext json = new SCR_JsonLoadContext(false);
		bool parsed = json.ImportFromString(LF_MapKnownCasesData.Json());
		ctx.True(parsed, "the shared discovery table is valid JSON");
		if (!parsed)
			return;

		array<ref LF_Test_MapKnownCase> cases = {};
		ctx.True(json.ReadValue("cases", cases), "the table's cases read");
		ctx.True(cases.Count() >= 16, "the table holds the full case set");
		foreach (LF_Test_MapKnownCase tableCase : cases)
		{
			if (!tableCase || !tableCase.input)
			{
				ctx.Fail("a case has no input");
				continue;
			}

			float distance = tableCase.input.distance;
			if (RedProof())
				distance = 0;

			int recent = tableCase.input.recent;
			if (RedProof())
				recent = 0;

			LF_EMapKnownOutcome outcome = LF_MapKnownRules.Decide(tableCase.input.action == "visit", LF_MapPoiRules.ParseKnown(tableCase.input.known), tableCase.input.live, distance, tableCase.input.radius, recent);
			ctx.EqualStr(tableCase.expect, LF_MapKnownRules.OutcomeName(outcome), "case: " + tableCase.name);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void RunStore(LF_TestContext ctx)
	{
		int now = 1000000;
		float far = 31;
		if (RedProof())
			far = 5;

		// Out of range: refused, nothing recorded.
		LF_EMapKnownOutcome outcome = LF_MapKnown.VisitAsk(PLAYER, CHARACTER, "mk-shop", true, far, 0, now);
		ctx.True(outcome == LF_EMapKnownOutcome.REFUSED_RANGE, "a visit 31 m from a 20 m place is refused");
		ctx.True(LF_MapKnown.KnownOf(CHARACTER, "mk-shop") == LF_EMapPoiKnown.NONE, "the refused visit records nothing");

		// Hear, then visit.
		ctx.True(LF_MapKnown.HearCharacter(CHARACTER, "mk-shop") == LF_EMapKnownOutcome.HEARD, "hearing of the shop records heard of");
		ctx.True(LF_MapKnown.KnownOf(CHARACTER, "mk-shop") == LF_EMapPoiKnown.HEARD, "the shop is heard of");
		outcome = LF_MapKnown.VisitAsk(PLAYER, CHARACTER, "mk-shop", true, 4, 0, now + 1);
		ctx.True(outcome == LF_EMapKnownOutcome.VISITED, "standing at the heard-of shop makes it visited");
		ctx.True(LF_MapKnown.KnownOf(CHARACTER, "mk-shop") == LF_EMapPoiKnown.VISITED, "the shop is visited");
		ctx.True(LF_MapKnown.HearCharacter(CHARACTER, "mk-shop") == LF_EMapKnownOutcome.KEPT, "hearing again does not lower visited");
		ctx.True(LF_MapKnown.KnownOf(CHARACTER, "mk-shop") == LF_EMapPoiKnown.VISITED, "the shop stays visited");

		// Rate: two asks so far; three more pass, the sixth in the window is refused, a minute later it passes.
		for (int i = 0; i < 3; i++)
		{
			LF_MapKnown.VisitAsk(PLAYER, CHARACTER, "mk-rate-" + i, true, 2, 0, now + 2 + i);
		}

		ctx.Equal(4, LF_MapKnown.CountOf(CHARACTER), "five asks in a minute: the four in range are recorded");
		int later = now + 10;
		if (RedProof())
			later = now + LF_MapKnownRules.WINDOW_MS + 10;

		outcome = LF_MapKnown.VisitAsk(PLAYER, CHARACTER, "mk-rate-x", true, 2, 0, later);
		ctx.True(outcome == LF_EMapKnownOutcome.REFUSED_RATE, "the sixth ask in a minute is refused");
		ctx.True(LF_MapKnown.KnownOf(CHARACTER, "mk-rate-x") == LF_EMapPoiKnown.NONE, "the refused ask records nothing");
		outcome = LF_MapKnown.VisitAsk(PLAYER, CHARACTER, "mk-rate-x", true, 2, 0, now + LF_MapKnownRules.WINDOW_MS + 20);
		ctx.True(outcome == LF_EMapKnownOutcome.VISITED, "a minute later the ask is allowed again");

		// A dark place cannot be visited, but can be heard of.
		outcome = LF_MapKnown.VisitAsk(PLAYER + 1, CHARACTER, "mk-dark", false, 0, 0, now);
		ctx.True(outcome == LF_EMapKnownOutcome.REFUSED_DARK, "a place that is not live cannot be visited");
		ctx.True(LF_MapKnown.HearCharacter(CHARACTER, "mk-dark") == LF_EMapKnownOutcome.HEARD, "a place that is not live can be heard of");

		// Knowledge is per character (D-M12).
		ctx.True(LF_MapKnown.KnownOf("mapknown-test-other", "mk-shop") == LF_EMapPoiKnown.NONE, "another character knows nothing of it");
		ctx.True(LF_MapKnown.VisitAsk(PLAYER, "", "mk-shop", true, 0, 0, now) == LF_EMapKnownOutcome.REFUSED_BAD, "an ask with no character key is refused");

		// The join sync's export.
		array<string> ids = {};
		array<int> states = {};
		ctx.Equal(LF_MapKnown.CountOf(CHARACTER), LF_MapKnown.ExportCharacter(CHARACTER, ids, states), "the join sync exports the whole set");
		int shopAt = ids.Find("mk-shop");
		ctx.True(shopAt != -1 && states[shopAt] == 2, "the join sync carries the shop as visited");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunLocal(LF_TestContext ctx)
	{
		array<string> syncIds = {"mk-a", "mk-b"};
		array<int> syncStates = {1, 2};
		LF_MapKnown.SetLocalAll(syncIds, syncStates);
		ctx.True(LF_MapKnown.LocalKnownOf("mk-a") == LF_EMapPoiKnown.HEARD, "the sync sets heard of on the client");
		ctx.True(LF_MapKnown.LocalKnownOf("mk-b") == LF_EMapPoiKnown.VISITED, "the sync sets visited on the client");
		LF_MapKnown.SetLocal("mk-b", 1);
		ctx.True(LF_MapKnown.LocalKnownOf("mk-b") == LF_EMapPoiKnown.VISITED, "a late heard-of never lowers visited on the client");
		LF_MapKnown.SetLocal("mk-a", 2);
		ctx.True(LF_MapKnown.LocalKnownOf("mk-a") == LF_EMapPoiKnown.VISITED, "a visit upgrades heard of on the client");

		LF_MapPoiAudience audience = new LF_MapPoiAudience();
		LF_MapKnown.FillAudience(audience);
		ctx.True(audience.KnownOf("mk-a") == LF_EMapPoiKnown.VISITED, "the map's audience gets the local set");
		ctx.True(audience.KnownOf("mk-c") == LF_EMapPoiKnown.NONE, "a place not in the set stays unknown");
	}
}
