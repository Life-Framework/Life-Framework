// red-proof: 2026-11-17, cli test --no-build --red LF_Test_MapShares: RED-OK map/shares (RedProof() resolves every
//   audience case as EVERYONE and skips the membership refresh, so the PLAYER, ROLE, ORG and unknown
//   "no" cases and the on-duty, off-duty and org-leave steps fail). Green without it (56 assertions).
//! One case of the table, as the engine's class reader fills it: the keys are the member names.
class LF_Test_MapShareCaseInput
{
	string op;
	string audience;
	string id;
	string uid;
	ref array<string> roles;
	string org;
	float expires;
	float now;
	ref array<string> delivered;
	ref array<string> wanted;
	int radius;
	int style;
};

//------------------------------------------------------------------------------------------------
class LF_Test_MapShareCase
{
	string name;
	ref LF_Test_MapShareCaseInput input;
	string expect;
};

//------------------------------------------------------------------------------------------------
//! Records what LF_MapShares sends, as "add:<player>:<key>", "remove:<player>:<key>", "clear:<player>".
class LF_Test_MapShareSink : LF_MapShareSink
{
	ref array<string> m_aSent = {};

	override void Add(int playerId, LF_MapShareRow row)
	{
		m_aSent.Insert(string.Format("add:%1:%2", playerId, row.m_sKey));
	}

	override void Remove(int playerId, string key)
	{
		m_aSent.Insert(string.Format("remove:%1:%2", playerId, key));
	}

	override void Clear(int playerId)
	{
		m_aSent.Insert(string.Format("clear:%1", playerId));
	}

	bool Got(string line)
	{
		return m_aSent.Find(line) != -1;
	}

	//! Lines sent to one player (prefix match on ":<player>:" or "clear:<player>").
	int CountFor(int playerId)
	{
		int count;
		foreach (string line : m_aSent)
		{
			if (line.Contains(string.Format(":%1:", playerId)) || line == string.Format("clear:%1", playerId))
				count++;
		}
		return count;
	}
}

//------------------------------------------------------------------------------------------------
//! Fixture players: who is on and what they belong to, changed by the test.
class LF_Test_MapShareFacts : LF_MapShareFacts
{
	ref map<int, ref LF_MapShareMember> m_mMembers = new map<int, ref LF_MapShareMember>();
	ref array<int> m_aOrder = {};

	void Set(int playerId, string uid, string role, string org)
	{
		array<string> roles = {};
		if (!role.IsEmpty())
			roles.Insert(role);

		if (m_aOrder.Find(playerId) == -1)
			m_aOrder.Insert(playerId);

		m_mMembers.Set(playerId, LF_MapShareMember.Make(playerId, uid, roles, org));
	}

	override void Players(notnull array<int> outIds)
	{
		outIds.Copy(m_aOrder);
	}

	override LF_MapShareMember Of(int playerId)
	{
		return m_mMembers.Get(playerId);
	}
}

//------------------------------------------------------------------------------------------------
// tier: LOGIC
//! map/shares: the shared-mark core (#2237, everon-map.md section 3, card M9). The case table
//! Configs/Map/Rules/shares.cases.json (audience resolution for player, role, org and everyone;
//! expiry; the membership diff; whether a row's area is drawable) runs here through LF_MapShareRules and in
//! tools/validation/map-shares-selftest.mjs through the node twin tools/map/rules/shares.mjs.
//! Then four fixture players through LF_MapShares with a recording sink: each row reaches its audience
//! only, going on and off duty and leaving an org add and remove rows, a spawn clears and resends the
//! full set, an expired row leaves everyone, and moving a row's audience takes it from the old one.
class LF_Test_MapShares : LF_Test
{
	protected static const int OFFICER = 1;
	protected static const int MEDIC = 2;
	protected static const int MEMBER = 3;
	protected static const int CIVILIAN = 4;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "map/shares";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		RunCases(ctx);

		LF_MapShares.ResetStatics();
		RunTable(ctx);
		LF_MapShares.ResetStatics();
		LF_MapShares.EnsureStarted();
	}

	//------------------------------------------------------------------------------------------------
	protected void RunCases(LF_TestContext ctx)
	{
		SCR_JsonLoadContext json = new SCR_JsonLoadContext(false);
		bool parsed = json.ImportFromString(LF_MapShareCasesData.Json());
		ctx.True(parsed, "the shared case table is valid JSON");
		if (!parsed)
			return;

		array<ref LF_Test_MapShareCase> cases = {};
		ctx.True(json.ReadValue("cases", cases), "the table's cases read");
		ctx.True(cases.Count() >= 20, "the table holds the full case set");
		foreach (LF_Test_MapShareCase tableCase : cases)
		{
			if (!tableCase || !tableCase.input)
			{
				ctx.Fail("a case has no input");
				continue;
			}

			ctx.EqualStr(tableCase.expect, Evaluate(tableCase.input), "case: " + tableCase.name);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The Enforce side of tools/map/rules/shares.mjs evaluate().
	protected string Evaluate(LF_Test_MapShareCaseInput input)
	{
		if (input.op == "audience")
		{
			LF_EMapShareAudience audience = LF_MapShareRules.ParseAudience(input.audience);
			if (RedProof())
				audience = LF_EMapShareAudience.EVERYONE;

			LF_MapShareMember member = LF_MapShareMember.Make(0, input.uid, input.roles, input.org);
			if (LF_MapShareRules.InAudience(audience, input.id, member))
				return "yes";

			return "no";
		}

		if (input.op == "expired")
		{
			if (LF_MapShareRules.Expired(input.expires, input.now))
				return "yes";

			return "no";
		}

		if (input.op == "area")
		{
			if (LF_MapShareRules.AreaValid(input.radius, input.style))
				return "yes";

			return "no";
		}

		if (input.op == "diff")
		{
			array<string> adds = {};
			array<string> removes = {};
			LF_MapShareRules.Diff(input.delivered, input.wanted, adds, removes);
			return LF_MapShareRules.DiffText(adds, removes);
		}

		return "unknown op " + input.op;
	}

	//------------------------------------------------------------------------------------------------
	protected void RunTable(LF_TestContext ctx)
	{
		LF_Test_MapShareSink sink = new LF_Test_MapShareSink();
		LF_Test_MapShareFacts facts = new LF_Test_MapShareFacts();
		facts.Set(OFFICER, "char-officer", LF_MapShares.ROLE_POLICE, "");
		facts.Set(MEDIC, "char-medic", LF_MapShares.ROLE_MEDIC, "");
		facts.Set(MEMBER, "char-member", "", "crew");
		facts.Set(CIVILIAN, "char-civilian", "", "");
		LF_MapShares.SetSink(sink);
		LF_MapShares.SetFacts(facts);

		// Each row reaches its audience only.
		ctx.True(LF_MapShares.Put(LF_MapShareRow.Make("dispatch:1", LF_EMapShareAudience.ROLE, LF_MapShares.ROLE_POLICE, 100, 200, 0, 0, "call", 0)), "a police dispatch row is put");
		ctx.True(sink.Got("add:1:dispatch:1"), "the on-duty officer gets the police row");
		ctx.False(sink.Got("add:2:dispatch:1"), "the on-duty medic never gets the police row (D-M10)");
		ctx.False(sink.Got("add:3:dispatch:1") || sink.Got("add:4:dispatch:1"), "civilians never get the police row");

		LF_MapShares.Put(LF_MapShareRow.Make("event:fair", LF_EMapShareAudience.EVERYONE, "", 10, 20, 0, 0, "", 50));
		ctx.True(sink.Got("add:1:event:fair") && sink.Got("add:2:event:fair") && sink.Got("add:3:event:fair") && sink.Got("add:4:event:fair"), "an everyone row reaches all four players");

		LF_MapShares.Put(LF_MapShareRow.Make("org:crew:1", LF_EMapShareAudience.ORG, "crew", 30, 40, 0, 0, "stash", 0));
		ctx.True(sink.Got("add:3:org:crew:1"), "the org member gets the org row");
		ctx.Equal(1, CountKey(sink, "add:", "org:crew:1"), "nobody outside the org gets the org row");

		LF_MapShares.Put(LF_MapShareRow.Make("taxi:1", LF_EMapShareAudience.PLAYER, "char-civilian", 50, 60, 0, 0, "", 0));
		ctx.True(sink.Got("add:4:taxi:1"), "the player row reaches its player");
		ctx.Equal(1, CountKey(sink, "add:", "taxi:1"), "the player row reaches nobody else");

		ctx.False(LF_MapShares.Put(LF_MapShareRow.Make("bad:1", LF_EMapShareAudience.NONE, "", 0, 0, 0, 0, "", 0)), "a row with no audience is refused");
		ctx.False(LF_MapShares.Put(LF_MapShareRow.Make("", LF_EMapShareAudience.EVERYONE, "", 0, 0, 0, 0, "", 0)), "a row with no key is refused");
		ctx.Equal(4, LF_MapShares.Count(), "four rows are held");

		// Membership changes add and remove.
		sink.m_aSent.Clear();
		facts.Set(CIVILIAN, "char-civilian", LF_MapShares.ROLE_POLICE, "");
		facts.Set(OFFICER, "char-officer", "", "");
		facts.Set(MEMBER, "char-member", "", "");
		if (!RedProof())
			LF_MapShares.RefreshAll();

		ctx.True(sink.Got("add:4:dispatch:1"), "going on duty sends the police row");
		ctx.True(sink.Got("remove:1:dispatch:1"), "going off duty removes the police row");
		ctx.True(sink.Got("remove:3:org:crew:1"), "leaving the org removes the org row");
		ctx.Equal(0, sink.CountFor(MEDIC), "the medic, whose membership did not change, gets nothing");
		ctx.False(LF_MapShares.IsDelivered(OFFICER, "dispatch:1"), "the off-duty officer no longer holds the row");

		// A spawn clears and resends the full set.
		sink.m_aSent.Clear();
		LF_MapShares.OnSpawned(CIVILIAN);
		ctx.EqualStr("clear:4", FirstOr(sink, ""), "a spawn clears the client first");
		ctx.True(sink.Got("add:4:dispatch:1") && sink.Got("add:4:event:fair") && sink.Got("add:4:taxi:1"), "a spawn resends every row of the player's audiences");
		ctx.Equal(4, sink.m_aSent.Count(), "a spawn sends the clear and three rows, nothing more");
		ctx.Equal(0, sink.CountFor(MEDIC), "another player's spawn sends the medic nothing");

		// Moving a row's audience takes it from the old one.
		sink.m_aSent.Clear();
		LF_MapShares.Put(LF_MapShareRow.Make("dispatch:1", LF_EMapShareAudience.ROLE, LF_MapShares.ROLE_MEDIC, 100, 200, 0, 0, "", 0));
		ctx.True(sink.Got("remove:4:dispatch:1"), "a row moved to medics leaves the police officer");
		ctx.True(sink.Got("add:2:dispatch:1"), "a row moved to medics reaches the medic");

		// Expiry removes from everyone.
		sink.m_aSent.Clear();
		ctx.Equal(0, LF_MapShares.Expire(49.5), "nothing expires before its time");
		ctx.Equal(1, LF_MapShares.Expire(50), "the timed row expires at its time");
		ctx.False(LF_MapShares.Has("event:fair"), "the expired row is gone from the table");
		ctx.Equal(4, CountKey(sink, "remove:", "event:fair"), "the expired row leaves all four players");

		// An owner removing a row.
		sink.m_aSent.Clear();
		ctx.True(LF_MapShares.Remove("taxi:1"), "the taxi row is removed");
		ctx.True(sink.Got("remove:4:taxi:1") && sink.m_aSent.Count() == 1, "only its player is told");
		ctx.False(LF_MapShares.Remove("taxi:1"), "removing it again is a no-op");

		// A player who left is forgotten.
		facts.m_aOrder.RemoveItem(MEDIC);
		LF_MapShares.RefreshAll();
		ctx.Equal(0, LF_MapShares.DeliveredCount(MEDIC), "a player who left holds nothing");
	}

	//------------------------------------------------------------------------------------------------
	protected int CountKey(LF_Test_MapShareSink sink, string verb, string key)
	{
		int count;
		foreach (string line : sink.m_aSent)
		{
			if (line.StartsWith(verb) && line.EndsWith(":" + key))
				count++;
		}
		return count;
	}

	//------------------------------------------------------------------------------------------------
	protected string FirstOr(LF_Test_MapShareSink sink, string fallback)
	{
		if (sink.m_aSent.IsEmpty())
			return fallback;

		return sink.m_aSent[0];
	}
}
