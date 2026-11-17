// red-proof: 2026-11-18 `tools\cli test --no-build --red LF_Test_OrgWarSweep` with the sweep subscribers not installed: RED-OK org/war-sweep ("with reason forfeit (expected 'forfeit', got 'time')", "told once (expected 1, got 0)"); green with them (`--only`, 22 lines, failures=0).
// tier: LOGIC
//! org/war-sweep (#2669, M7 gang war W4): the war sweep lapses a declaration nobody answered, ends a war
//! past endsAt, and an organization that lost its last member forfeits its wars. Each closing, and the
//! declare and accept before it, is told to the island exactly once (the announce count is the seam).
//! Time is the unix second the test hands to `LF_OrgWarSweep.SweepAt`.
class LF_TestOrgWarSweepRecorder : LF_EventSubscriber
{
	int m_iEnded;
	string m_sLastReason;

	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnOrgWarEnded ended = LF_OnOrgWarEnded.Cast(busEvent);
		if (!ended)
			return false;

		m_iEnded++;
		m_sLastReason = ended.m_sReason;
		return true;
	}
}

//------------------------------------------------------------------------------------------------
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_OrgWarSweep : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "org/war-sweep";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_OrganizationManager.Reset();
		LF_OrgWars.ResetStatics();
		LF_OrgWarSweep.ResetStatics();
		LF_OrgWarAnnounce.ResetStatics();
		if (!RedProof())
			LF_OrgWarSweep.Install();

		LF_TestOrgWarSweepRecorder rec = new LF_TestOrgWarSweepRecorder();
		LF_EventBus.Subscribe(LF_OnOrgWarEnded, rec);

		LF_Organization a = LF_OrganizationManager.Create("war-sweep-founder-a", "War Sweep Alpha");
		LF_Organization b = LF_OrganizationManager.Create("war-sweep-founder-b", "War Sweep Bravo");
		LF_Organization c = LF_OrganizationManager.Create("war-sweep-founder-c", "War Sweep Charlie");
		LF_Organization d = LF_OrganizationManager.Create("war-sweep-founder-d", "War Sweep Delta");
		ctx.NotNull(a, "org A is registered");
		ctx.NotNull(b, "org B is registered");
		ctx.NotNull(c, "org C is registered");
		ctx.NotNull(d, "org D is registered");
		if (!a || !b || !c || !d)
			return;

		LF_OrgWarRules rules = new LF_OrgWarRules();
		rules.m_iAnswerSeconds = 600;
		rules.m_iLengthSeconds = 3600;
		rules.m_iCooldownSeconds = 7200;
		rules.m_iMinMembers = 1;

		int t0 = 1800000000;

		// 1. An unanswered declaration lapses when the sweep passes the window.
		LF_OrgWarRecord lapsing;
		ctx.EqualStr("", LF_OrgWars.Declare(a.GetId(), b.GetId(), t0, rules, lapsing), "A declares on B");
		ctx.NotNull(lapsing, "the declaration returns the record");
		if (!lapsing)
			return;

		ctx.Equal(1, LF_OrgWarAnnounce.GetAnnouncedCount(), "the declaration is announced once");
		ctx.Equal(0, LF_OrgWarSweep.SweepAt(t0 + 599), "a sweep inside the window closes nothing");
		ctx.Equal(LF_EOrgWarState.DECLARED, lapsing.m_eState, "inside the window it is still DECLARED");
		ctx.Equal(1, LF_OrgWarAnnounce.GetAnnouncedCount(), "a quiet sweep announces nothing");
		ctx.Equal(1, LF_OrgWarSweep.SweepAt(t0 + 601), "a sweep past the window closes the declaration");
		ctx.Equal(LF_EOrgWarState.LAPSED, lapsing.m_eState, "the declaration lapsed");
		ctx.Equal(2, LF_OrgWarAnnounce.GetAnnouncedCount(), "the lapse is announced once");
		ctx.EqualStr(LF_OrgWarAnnounce.KEY_LAPSED, LF_OrgWarAnnounce.GetLastKey(), "the lapse uses the lapsed text");
		ctx.EqualStr("War Sweep Alpha / War Sweep Bravo", LF_OrgWarAnnounce.GetLastArg(), "the toast names declarer and target");
		ctx.Equal(0, LF_OrgWarSweep.SweepAt(t0 + 700), "a second sweep finds nothing to close");
		ctx.Equal(2, LF_OrgWarAnnounce.GetAnnouncedCount(), "a lapse is never announced twice");

		// 2. An accepted war ends at endsAt.
		int t1 = t0 + 1000;
		LF_OrgWarRecord war;
		ctx.EqualStr("", LF_OrgWars.Declare(a.GetId(), b.GetId(), t1, rules, war), "A declares on B again");
		ctx.EqualStr("", LF_OrgWars.Accept(war.m_sWarId, t1 + 100, rules), "B accepts inside the window");
		ctx.Equal(4, LF_OrgWarAnnounce.GetAnnouncedCount(), "declare and accept are announced once each");
		ctx.Equal(0, LF_OrgWarSweep.SweepAt(war.m_iEndsAt - 1), "a sweep before endsAt closes nothing");
		ctx.True(LF_OrgWars.IsAtWar(a.GetId(), b.GetId()), "the war runs until endsAt");
		ctx.Equal(1, LF_OrgWarSweep.SweepAt(war.m_iEndsAt), "a sweep at endsAt closes the war");
		ctx.Equal(LF_EOrgWarState.ENDED, war.m_eState, "the war ended on time");
		ctx.EqualStr(LF_OrgWars.REASON_TIME, rec.m_sLastReason, "the end carries reason time");
		ctx.Equal(5, LF_OrgWarAnnounce.GetAnnouncedCount(), "the end is announced once");
		ctx.EqualStr(LF_OrgWarAnnounce.KEY_TIME, LF_OrgWarAnnounce.GetLastKey(), "the end uses the time-up text");
		ctx.Equal(0, LF_OrgWarSweep.SweepAt(war.m_iEndsAt + 60), "an ended war is not closed twice");
		ctx.Equal(5, LF_OrgWarAnnounce.GetAnnouncedCount(), "no further announce after the end");

		// 3. The declarer loses its last member while at war: the war ends as forfeit.
		int t2 = war.m_iEndsAt + rules.m_iCooldownSeconds + 10;
		LF_OrgWarRecord forfeit;
		ctx.EqualStr("", LF_OrgWars.Declare(c.GetId(), b.GetId(), t2, rules, forfeit), "C declares on B");
		ctx.EqualStr("", LF_OrgWars.Accept(forfeit.m_sWarId, t2 + 50, rules), "B accepts C's declaration");
		int before = LF_OrgWarAnnounce.GetAnnouncedCount();
		int endedBefore = rec.m_iEnded;
		ctx.True(LF_OrganizationManager.LeaveDirect("war-sweep-founder-c"), "C's only member leaves");
		ctx.Equal(LF_EOrgWarState.ENDED, forfeit.m_eState, "emptying the declarer ends its war");
		ctx.EqualStr(LF_OrgWars.REASON_FORFEIT, rec.m_sLastReason, "the end carries reason forfeit");
		ctx.Equal(endedBefore + 1, rec.m_iEnded, "one end event for the forfeit");
		ctx.Equal(before + 1, LF_OrgWarAnnounce.GetAnnouncedCount(), "the forfeit is announced once");
		ctx.EqualStr(LF_OrgWarAnnounce.KEY_FORFEIT, LF_OrgWarAnnounce.GetLastKey(), "the forfeit uses the forfeit text");
		ctx.True(!LF_OrgWars.IsAtWar(c.GetId(), b.GetId()), "the forfeited war is over");

		// 4. The target loses its last member while a declaration waits: it closes as forfeit too.
		LF_OrgWarRecord waiting;
		ctx.EqualStr("", LF_OrgWars.Declare(a.GetId(), d.GetId(), t2 + 100, rules, waiting), "A declares on D");
		before = LF_OrgWarAnnounce.GetAnnouncedCount();
		ctx.True(LF_OrganizationManager.LeaveDirect("war-sweep-founder-d"), "D's only member leaves");
		ctx.Equal(LF_EOrgWarState.LAPSED, waiting.m_eState, "a waiting declaration on an emptied org closes");
		ctx.EqualStr(LF_OrgWars.REASON_FORFEIT, rec.m_sLastReason, "with reason forfeit");
		ctx.Equal(before + 1, LF_OrgWarAnnounce.GetAnnouncedCount(), "told once");

		// 5. An org that is still staffed keeps its war when another one empties.
		LF_OrgWarRecord kept;
		ctx.EqualStr("", LF_OrgWars.Declare(a.GetId(), b.GetId(), t2 + 200, rules, kept), "A declares on B");
		ctx.EqualStr("", LF_OrgWars.Accept(kept.m_sWarId, t2 + 210, rules), "B accepts");
		ctx.Equal(0, LF_OrgWarSweep.ForfeitOf(c.GetId(), t2 + 220), "an emptied org with no open war forfeits nothing");
		ctx.True(LF_OrgWars.IsAtWar(a.GetId(), b.GetId()), "A and B stay at war");

		LF_OrgWarSweep.ResetStatics();
		LF_OrgWarAnnounce.ResetStatics();
		LF_EventBus.Unsubscribe(LF_OnOrgWarEnded, rec);
		LF_OrgWars.ResetStatics();
		LF_OrganizationManager.Reset();
	}
}
