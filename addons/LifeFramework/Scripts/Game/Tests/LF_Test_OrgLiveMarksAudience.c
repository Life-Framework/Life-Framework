// red-proof: 2026-11-22, cli test --no-build --red LF_Test_OrgLiveMarksAudience: RED PROOF OK with the first Publish skipped ("a member who barely moved is not re-sent (expected 0, got 4)", no add to either member).
//! gangs/live-marks-audience (#4216, card C10, N25): a live-marks pass puts one mark per organization
//! member at their position, and LF_MapShares sends it to that organization's members only: the
//! outsider's map gets nothing. Standing still sends nothing again; a 50 m move re-sends; a downed
//! member's mark goes dim; leaving the organization or going offline takes the mark away. The pass runs
//! every 5 s from the world's start.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_OrgLiveMarksAudience : LF_Test
{
	protected static const string BOSS = "livemarks-boss";
	protected static const string MATE = "livemarks-mate";
	protected static const string OUTSIDER = "livemarks-outsider";
	protected static const int P_BOSS = 1;
	protected static const int P_MATE = 2;
	protected static const int P_OUTSIDER = 3;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "gangs/live-marks-audience";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		ctx.True(LF_OrgLiveMarks.IsRunning(), "the live-marks pass runs from the world's start");
		ctx.Equal(5000, LF_OrgLiveMarks.INTERVAL_MS, "the pass runs every 5 s (N25)");

		array<ref LF_OrganizationRecord> orgsBefore = LF_OrganizationManager.ExportAll();
		array<ref LF_OrganizationRankRow> ranksBefore = LF_OrganizationManager.ExportRanks();
		array<ref LF_OrgMapMarkRow> marksBefore = LF_OrgMapMarks.ExportAll();
		LF_OrgMapMarks.ApplyAll(null);
		LF_OrgLiveMarks.ResetStatics();
		LF_MapShares.ResetStatics();
		LF_OrganizationManager.Reset();

		LF_Test_OrgLiveMarksSink sink = new LF_Test_OrgLiveMarksSink();
		LF_Test_MapShareFacts facts = new LF_Test_MapShareFacts();
		LF_MapShares.SetSink(sink);
		LF_MapShares.SetFacts(facts);

		LF_Organization org = LF_OrganizationManager.Create(BOSS, "Live Marks Crew");
		ctx.NotNull(org, "the organization is founded");
		if (!org)
		{
			Restore(orgsBefore, ranksBefore, marksBefore);
			return;
		}

		string orgId = org.GetId();
		ctx.True(LF_OrganizationManager.JoinDirect(orgId, MATE, LF_OrganizationConfig.RANK_MEMBER), "the mate joins");
		facts.Set(P_BOSS, BOSS, "", orgId);
		facts.Set(P_MATE, MATE, "", orgId);
		facts.Set(P_OUTSIDER, OUTSIDER, "", "");

		string bossKey = LF_OrgLiveMarks.KeyOf(orgId, BOSS);
		string mateKey = LF_OrgLiveMarks.KeyOf(orgId, MATE);

		// First pass: both members are marked; the outsider is neither marked nor sent anything.
		array<ref LF_OrgLiveFix> fixes = {};
		fixes.Insert(LF_OrgLiveFix.Make(BOSS, 1200, 3400, false, "Boss Man"));
		fixes.Insert(LF_OrgLiveFix.Make(MATE, 1300, 3500, false, "Mate"));
		fixes.Insert(LF_OrgLiveFix.Make(OUTSIDER, 1400, 3600, false, "Nosy"));
		if (!RedProof())
			LF_OrgLiveMarks.Publish(fixes);

		ctx.Equal(2, LF_OrgLiveMarks.Count(), "one live mark per member, none for the outsider");
		ctx.True(sink.Got(string.Format("add:%1:%2:1200:3400:%3", P_MATE, bossKey, LF_OrgLiveMarks.COLOUR_LIVE)), "the mate's map gets the boss at their position");
		ctx.True(sink.Got(string.Format("add:%1:%2:1300:3500:%3", P_BOSS, mateKey, LF_OrgLiveMarks.COLOUR_LIVE)), "the boss's map gets the mate at their position");
		ctx.Equal(0, sink.CountFor(P_OUTSIDER), "the outsider's map gets no live mark");

		// Standing still sends nothing; a 50 m move re-sends to the members only.
		int sentBefore = sink.m_aSent.Count();
		fixes[0] = LF_OrgLiveFix.Make(BOSS, 1202, 3401, false, "Boss Man");
		LF_OrgLiveMarks.Publish(fixes);
		ctx.Equal(sentBefore, sink.m_aSent.Count(), "a member who barely moved is not re-sent");
		fixes[0] = LF_OrgLiveFix.Make(BOSS, 1250, 3400, false, "Boss Man");
		LF_OrgLiveMarks.Publish(fixes);
		ctx.True(sink.Got(string.Format("add:%1:%2:1250:3400:%3", P_MATE, bossKey, LF_OrgLiveMarks.COLOUR_LIVE)), "a 50 m move reaches the mate");
		ctx.Equal(0, sink.CountFor(P_OUTSIDER), "the move does not reach the outsider");

		// Downed: the mark stays and goes dim.
		fixes[1] = LF_OrgLiveFix.Make(MATE, 1300, 3500, true, "Mate");
		LF_OrgLiveMarks.Publish(fixes);
		ctx.True(sink.Got(string.Format("add:%1:%2:1300:3500:%3", P_BOSS, mateKey, LF_OrgLiveMarks.COLOUR_DOWN)), "a downed mate is drawn dim");

		// Leaving: the leaver's mark goes from the boss's map, and the leaver loses the boss's.
		ctx.True(LF_OrganizationManager.LeaveDirect(MATE), "the mate leaves");
		facts.Set(P_MATE, MATE, "", "");
		LF_OrgLiveMarks.Publish(fixes);
		LF_MapShares.RefreshAll();
		ctx.True(sink.Got(string.Format("remove:%1:%2", P_BOSS, mateKey)), "the leaver's mark leaves the boss's map");
		ctx.True(sink.Got(string.Format("remove:%1:%2", P_MATE, bossKey)), "the leaver no longer sees the boss");
		ctx.False(LF_MapShares.Has(mateKey), "the leaver's live mark is gone");

		// Offline: no fix, no mark.
		array<ref LF_OrgLiveFix> none = {};
		LF_OrgLiveMarks.Publish(none);
		ctx.False(LF_MapShares.Has(bossKey), "an offline member's mark is gone");
		ctx.Equal(0, LF_OrgLiveMarks.Count(), "no live marks remain");

		Restore(orgsBefore, ranksBefore, marksBefore);
	}

	//------------------------------------------------------------------------------------------------
	protected void Restore(array<ref LF_OrganizationRecord> orgsBefore, array<ref LF_OrganizationRankRow> ranksBefore, array<ref LF_OrgMapMarkRow> marksBefore)
	{
		LF_OrganizationManager.Reset();
		LF_OrganizationManager.ApplyAll(orgsBefore);
		LF_OrganizationManager.ApplyRanks(ranksBefore);
		LF_MapShares.ResetStatics();
		LF_OrgLiveMarks.ResetStatics();
		LF_MapShares.EnsureStarted();
		LF_OrgLiveMarks.EnsureStarted();
		LF_OrgMapMarks.ResetStatics();
		LF_OrgMapMarks.ApplyAll(marksBefore);
	}
}

//------------------------------------------------------------------------------------------------
//! Records each send with its position and colour: add:<player>:<key>:<x>:<z>:<colour>.
class LF_Test_OrgLiveMarksSink : LF_Test_MapShareSink
{
	override void Add(int playerId, LF_MapShareRow row)
	{
		m_aSent.Insert(string.Format("add:%1:%2:%3:%4:%5", playerId, row.m_sKey, row.m_iX, row.m_iZ, row.m_iColour));
	}
}
