// red-proof: 2026-11-23, cli test --no-build --red LF_Test_OrgLiveMarkWiring: RED-OK gangs/live-mark-wiring (fails with the hook on, 29 assertions): no grey re-send, the boss in custody kept 3 live marks and got the move, the leaver's mark stayed (3 marks, no removes).
//! gangs/live-mark-wiring (#4364, split from #4222, decision N25): the live member-mark rules
//! (LF_OrgLiveMarkRules) are wired in. A downed member's mark is drawn in the placed-marker list's grey
//! entry (an index the list really holds, not one past its end); a viewer in custody stops receiving
//! their crew's live marks, while a crewmate still does; a member who leaves loses their mark on
//! every map at once, without waiting for the next 5 s pass.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_OrgLiveMarkWiring : LF_Test
{
	protected static const string BOSS = "livewire-boss";
	protected static const string MATE = "livewire-mate";
	protected static const string THIRD = "livewire-third";
	protected static const int P_BOSS = 1;
	protected static const int P_MATE = 2;
	protected static const int P_THIRD = 3;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "gangs/live-mark-wiring";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		// 1. The grey entry exists where COLOUR_DOWN points.
		SCR_MapMarkerManagerComponent manager = SCR_MapMarkerManagerComponent.GetInstance();
		ctx.NotNull(manager, "the game mode carries the marker manager");
		if (!manager || !manager.GetMarkerConfig())
			return;

		SCR_MapMarkerEntryPlaced placed = SCR_MapMarkerEntryPlaced.Cast(manager.GetMarkerConfig().GetMarkerEntryConfigByType(SCR_EMapMarkerType.PLACED_CUSTOM));
		ctx.NotNull(placed, "the marker config has a placed marker entry");
		if (!placed || !placed.GetColorEntries())
			return;

		int colourCount = placed.GetColorEntries().Count();
		ctx.True(LF_OrgLiveMarks.COLOUR_DOWN < colourCount, string.Format("the down colour %1 is inside the %2 placed colours", LF_OrgLiveMarks.COLOUR_DOWN, colourCount));
		if (LF_OrgLiveMarks.COLOUR_DOWN < colourCount)
			ctx.EqualStr("grey", placed.GetColorEntries()[LF_OrgLiveMarks.COLOUR_DOWN].GetName(), "the down colour is the grey entry");

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

		LF_Organization org = LF_OrganizationManager.Create(BOSS, "Live Wire Crew");
		ctx.NotNull(org, "the organization is founded");
		if (!org)
		{
			Restore(orgsBefore, ranksBefore, marksBefore);
			return;
		}

		string orgId = org.GetId();
		ctx.True(LF_OrganizationManager.JoinDirect(orgId, MATE, LF_OrganizationConfig.RANK_MEMBER), "the mate joins");
		ctx.True(LF_OrganizationManager.JoinDirect(orgId, THIRD, LF_OrganizationConfig.RANK_MEMBER), "the third joins");
		facts.Set(P_BOSS, BOSS, "", orgId);
		facts.Set(P_MATE, MATE, "", orgId);
		facts.Set(P_THIRD, THIRD, "", orgId);

		string mateKey = LF_OrgLiveMarks.KeyOf(orgId, MATE);
		string thirdKey = LF_OrgLiveMarks.KeyOf(orgId, THIRD);

		array<ref LF_OrgLiveFix> fixes = {};
		fixes.Insert(LF_OrgLiveFix.Make(BOSS, 1200, 3400, false, "Boss"));
		fixes.Insert(LF_OrgLiveFix.Make(MATE, 1300, 3500, false, "Mate"));
		fixes.Insert(LF_OrgLiveFix.Make(THIRD, 1400, 3600, false, "Third"));
		LF_OrgLiveMarks.Publish(fixes);
		ctx.True(LF_MapShares.IsDelivered(P_BOSS, mateKey), "the boss holds the mate's live mark");

		// Downed: the mate's mark is re-sent in grey.
		fixes[1] = LF_OrgLiveFix.Make(MATE, 1300, 3500, true, "Mate");
		if (!RedProof())
			LF_OrgLiveMarks.Publish(fixes);

		ctx.True(sink.Got(string.Format("add:%1:%2:1300:3500:%3", P_BOSS, mateKey, LF_OrgLiveMarks.COLOUR_DOWN)), "a downed mate reaches the boss in grey");

		// Custody: the boss is jailed and loses every live mark; the third still has the mate's.
		LF_MapShareMember boss = facts.Of(P_BOSS);
		if (!RedProof())
			boss.m_bInCustody = true;

		LF_MapShares.RefreshAll();
		ctx.True(sink.Got(string.Format("remove:%1:%2", P_BOSS, mateKey)), "a boss in custody loses the mate's live mark");
		ctx.Equal(0, LF_MapShares.DeliveredCount(P_BOSS), "a boss in custody holds no live mark");
		ctx.True(LF_MapShares.IsDelivered(P_THIRD, mateKey), "a crewmate not in custody keeps the mate's live mark");
		fixes[2] = LF_OrgLiveFix.Make(THIRD, 1500, 3600, false, "Third");
		LF_OrgLiveMarks.Publish(fixes);
		ctx.False(sink.Got(string.Format("add:%1:%2:1500:3600:%3", P_BOSS, thirdKey, LF_OrgLiveMarks.COLOUR_LIVE)), "a move is not sent to a boss in custody");
		ctx.True(sink.Got(string.Format("add:%1:%2:1500:3600:%3", P_MATE, thirdKey, LF_OrgLiveMarks.COLOUR_LIVE)), "the move still reaches the mate");
		boss.m_bInCustody = false;
		LF_MapShares.RefreshAll();
		ctx.True(LF_MapShares.IsDelivered(P_BOSS, mateKey), "released, the boss sees the crew again");

		// Leaving: the third's mark goes from every map at once, with no pass in between.
		bool left = false;
		if (!RedProof())
			left = LF_OrganizationManager.LeaveDirect(THIRD);

		ctx.True(left, "the third leaves");
		ctx.False(LF_MapShares.Has(thirdKey), "the leaver's live mark is gone at once");
		ctx.True(sink.Got(string.Format("remove:%1:%2", P_BOSS, thirdKey)), "the leaver leaves the boss's map at once");
		ctx.True(sink.Got(string.Format("remove:%1:%2", P_MATE, thirdKey)), "the leaver leaves the mate's map at once");
		ctx.Equal(2, LF_OrgLiveMarks.Count(), "the two who stayed keep their marks");

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
