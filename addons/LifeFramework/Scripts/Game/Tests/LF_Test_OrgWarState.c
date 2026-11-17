// red-proof: 2026-11-17 `tools\cli test --no-build --only LF_Test_OrgWarState` before LF_OrgWars existed failed (scripts do not compile); green with it. `--red` run: RED-OK org/war-state ("a world end clears the war table (expected 0, got 3)").
// tier: LOGIC
//! org/war-state (#2666, M7 gang war W1): the war record moves declared -> accepted -> ended by the
//! rules; an unanswered declaration lapses; the pair cooldown and self-war refusals hold; the three
//! bus events fire once each. Times are plain unix seconds the test passes in.
class LF_TestOrgWarRecorder : LF_EventSubscriber
{
	int m_iDeclared;
	int m_iAccepted;
	int m_iEnded;
	string m_sLastReason;

	override bool Handle(notnull LF_Event busEvent)
	{
		if (LF_OnOrgWarDeclared.Cast(busEvent))
			m_iDeclared++;

		if (LF_OnOrgWarAccepted.Cast(busEvent))
			m_iAccepted++;

		LF_OnOrgWarEnded ended = LF_OnOrgWarEnded.Cast(busEvent);
		if (ended)
		{
			m_iEnded++;
			m_sLastReason = ended.m_sReason;
		}

		return true;
	}
}

//------------------------------------------------------------------------------------------------
class LF_Test_OrgWarState : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "org/war-state";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_OrganizationManager.Reset();
		LF_OrgWars.ResetStatics();
		LF_EventBus.ResetStatics();

		LF_TestOrgWarRecorder rec = new LF_TestOrgWarRecorder();
		LF_EventBus.Subscribe(LF_OnOrgWarDeclared, rec);
		LF_EventBus.Subscribe(LF_OnOrgWarAccepted, rec);
		LF_EventBus.Subscribe(LF_OnOrgWarEnded, rec);

		LF_Organization a = LF_OrganizationManager.Create("war-state-founder-a", "War State Alpha");
		LF_Organization b = LF_OrganizationManager.Create("war-state-founder-b", "War State Bravo");
		LF_Organization c = LF_OrganizationManager.Create("war-state-founder-c", "War State Charlie");
		ctx.NotNull(a, "org A is registered");
		ctx.NotNull(b, "org B is registered");
		ctx.NotNull(c, "org C is registered");
		if (!a || !b || !c)
			return;

		LF_OrgWarRules rules = new LF_OrgWarRules();
		rules.m_iFee = 500;
		rules.m_iAnswerSeconds = 600;
		rules.m_iLengthSeconds = 3600;
		rules.m_iCooldownSeconds = 7200;
		rules.m_iMinMembers = 1;

		int t0 = 1800000000;

		// Refusals that leave no record.
		LF_OrgWarRecord none;
		ctx.EqualStr(LF_OrgWars.REFUSE_SELF, LF_OrgWars.Declare(a.GetId(), a.GetId(), t0, rules, none), "an org cannot declare on itself");
		ctx.EqualStr(LF_OrgWars.REFUSE_UNKNOWN_ORG, LF_OrgWars.Declare(a.GetId(), "no-such-org", t0, rules, none), "an unknown target is refused");
		ctx.EqualStr(LF_OrgWars.REFUSE_UNKNOWN_ORG, LF_OrgWars.Declare("no-such-org", b.GetId(), t0, rules, none), "an unknown declarer is refused");
		ctx.Equal(0, LF_OrgWars.Count(), "refusals leave no record");
		ctx.Equal(0, rec.m_iDeclared, "refusals publish nothing");

		// Legal path: declared -> accepted -> ended at endsAt.
		LF_OrgWarRecord war;
		ctx.EqualStr("", LF_OrgWars.Declare(a.GetId(), b.GetId(), t0, rules, war), "A declares on B");
		ctx.NotNull(war, "the declaration returns the record");
		if (!war)
			return;

		ctx.Equal(LF_EOrgWarState.DECLARED, war.m_eState, "a fresh war is DECLARED");
		ctx.Equal(t0, war.m_iDeclaredAt, "declaredAt is the clock passed in");
		ctx.True(!LF_OrgWars.IsAtWar(a.GetId(), b.GetId()), "a declared war is not yet a war");
		ctx.EqualStr(LF_OrgWars.REFUSE_PENDING, LF_OrgWars.Declare(b.GetId(), a.GetId(), t0 + 1, rules, none), "the other side cannot cross-declare while one is pending");
		ctx.EqualStr(LF_OrgWars.REFUSE_PENDING, LF_OrgWars.Declare(a.GetId(), b.GetId(), t0 + 1, rules, none), "a second declaration on a pending pair is refused");

		ctx.EqualStr("", LF_OrgWars.Accept(war.m_sWarId, t0 + 100, rules), "B accepts inside the window");
		ctx.Equal(LF_EOrgWarState.ACCEPTED, war.m_eState, "the war is ACCEPTED");
		ctx.Equal(t0 + 100, war.m_iAcceptedAt, "acceptedAt is the accept clock");
		ctx.Equal(t0 + 100 + 3600, war.m_iEndsAt, "endsAt is accept time plus the war length");
		ctx.True(LF_OrgWars.IsAtWar(a.GetId(), b.GetId()), "A and B are at war");
		ctx.True(LF_OrgWars.IsAtWar(b.GetId(), a.GetId()), "at war is symmetric");
		ctx.True(!LF_OrgWars.IsAtWar(a.GetId(), c.GetId()), "A is not at war with C");
		ctx.Equal(1, LF_OrgWars.ActiveFor(a.GetId()).Count(), "A has one active war");
		ctx.Equal(1, LF_OrgWars.ActiveFor(b.GetId()).Count(), "B has one active war");
		ctx.Equal(0, LF_OrgWars.ActiveFor(c.GetId()).Count(), "C has none");
		ctx.EqualStr(LF_OrgWars.REFUSE_PENDING, LF_OrgWars.Declare(a.GetId(), b.GetId(), t0 + 200, rules, none), "an org at war with B cannot declare on B again");

		LF_OrgWars.Tick(t0 + 3000);
		ctx.True(LF_OrgWars.IsAtWar(a.GetId(), b.GetId()), "the war runs until endsAt");
		LF_OrgWars.Tick(war.m_iEndsAt);
		ctx.Equal(LF_EOrgWarState.ENDED, war.m_eState, "the war ends at endsAt");
		ctx.True(!LF_OrgWars.IsAtWar(a.GetId(), b.GetId()), "an ended war is over");
		ctx.Equal(war.m_iEndsAt + 7200, war.m_iCooldownUntil, "the pair cooldown runs from the end");
		ctx.Equal(0, LF_OrgWars.ActiveFor(a.GetId()).Count(), "A has no active war after the end");

		// Cooldown: the same pair, either side, is refused inside it and free after.
		ctx.EqualStr(LF_OrgWars.REFUSE_COOLDOWN, LF_OrgWars.Declare(a.GetId(), b.GetId(), war.m_iCooldownUntil - 1, rules, none), "the same pair is refused inside the cooldown");
		ctx.EqualStr(LF_OrgWars.REFUSE_COOLDOWN, LF_OrgWars.Declare(b.GetId(), a.GetId(), war.m_iCooldownUntil - 1, rules, none), "the cooldown holds for the other side too");
		LF_OrgWarRecord other;
		ctx.EqualStr("", LF_OrgWars.Declare(a.GetId(), c.GetId(), war.m_iEndsAt + 1, rules, other), "another pair is not under the cooldown");
		LF_OrgWars.End(other.m_sWarId, LF_OrgWars.REASON_FORFEIT, war.m_iEndsAt + 2);
		ctx.Equal(LF_EOrgWarState.LAPSED, other.m_eState, "ending a war that was never accepted lapses it");

		LF_OrgWarRecord again;
		ctx.EqualStr("", LF_OrgWars.Declare(a.GetId(), b.GetId(), war.m_iCooldownUntil, rules, again), "the pair can declare once the cooldown is over");

		// A lapsed declaration: unanswered past the window, IsAtWar stays false.
		LF_OrgWars.ResetStatics();
		LF_OrgWarRecord lapsing;
		ctx.EqualStr("", LF_OrgWars.Declare(a.GetId(), b.GetId(), t0, rules, lapsing), "A declares again on a clean table");
		LF_OrgWars.Tick(t0 + 599);
		ctx.Equal(LF_EOrgWarState.DECLARED, lapsing.m_eState, "inside the window it is still DECLARED");
		LF_OrgWars.Tick(t0 + 601);
		ctx.Equal(LF_EOrgWarState.LAPSED, lapsing.m_eState, "an unanswered declaration lapses");
		ctx.True(!LF_OrgWars.IsAtWar(a.GetId(), b.GetId()), "a lapsed declaration is no war");
		ctx.EqualStr(LF_OrgWars.REFUSE_NOT_PENDING, LF_OrgWars.Accept(lapsing.m_sWarId, t0 + 602, rules), "a lapsed declaration cannot be accepted");
		ctx.EqualStr(LF_OrgWars.REFUSE_UNKNOWN_WAR, LF_OrgWars.Accept("no-such-war", t0, rules), "an unknown war id is refused");

		// An accept after the window with no tick in between lapses it instead of starting a war.
		LF_OrgWarRecord late;
		ctx.EqualStr("", LF_OrgWars.Declare(a.GetId(), c.GetId(), t0 + 700, rules, late), "A declares on C");
		ctx.EqualStr(LF_OrgWars.REFUSE_EXPIRED, LF_OrgWars.Accept(late.m_sWarId, t0 + 700 + 601, rules), "an accept after the window is refused");
		ctx.Equal(LF_EOrgWarState.LAPSED, late.m_eState, "and the declaration is LAPSED");

		// Forfeit: an accepted war ended early by a reason.
		LF_OrgWarRecord forfeit;
		ctx.EqualStr("", LF_OrgWars.Declare(b.GetId(), c.GetId(), t0 + 5000, rules, forfeit), "B declares on C");
		ctx.EqualStr("", LF_OrgWars.Accept(forfeit.m_sWarId, t0 + 5010, rules), "C accepts");
		LF_OrgWars.End(forfeit.m_sWarId, LF_OrgWars.REASON_FORFEIT, t0 + 5100);
		ctx.Equal(LF_EOrgWarState.ENDED, forfeit.m_eState, "End closes an accepted war");
		ctx.Equal(t0 + 5100 + 7200, forfeit.m_iCooldownUntil, "a forfeit starts the cooldown at the forfeit");

		// Events, counted exactly. Declared: a-b, a-c, a-b again, a-b lapsing, a-c late, b-c = 6.
		// Ended: a-b (time), a-c (forfeit), a-b lapsing, a-c late, b-c (forfeit) = 5; the "again"
		// record was dropped by ResetStatics without an event.
		ctx.Equal(6, rec.m_iDeclared, "one declared event per accepted declaration");
		ctx.Equal(2, rec.m_iAccepted, "one accepted event per accepted war");
		ctx.Equal(5, rec.m_iEnded, "one ended event per war that closed (time, lapsed, forfeit)");
		ctx.EqualStr(LF_OrgWars.REASON_FORFEIT, rec.m_sLastReason, "the last end carried its reason");

		// World end clears the table.
		LF_StaticResetHook hook = new LF_OrgWars_StaticResetHook();
		if (!RedProof())
			hook.Apply();

		ctx.Equal(0, LF_OrgWars.Count(), "a world end clears the war table");
		ctx.True(LF_StaticReset.Registered().Contains("LF_OrgWars"), "a world end resets the war table");

		LF_EventBus.ResetStatics();
		LF_OrgWars.ResetStatics();
		LF_OrganizationManager.Reset();
	}
}
