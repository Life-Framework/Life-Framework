// red-proof: 2026-11-18 batched `tools\cli test --no-build --red LF_Test_OrgWarBoardText`: RED-OK org/war-board-text (hook skips the war transitions: "a declared, an accepted and an ended war give three notices (expected 3, got 0)"). The first plain run failed 2 asserts (Math.Max is float: 480 read back as 512 at unix-time size), fixed with an int helper.
// tier: LOGIC
//! org/war-board-text (#2677, M7 gang war): the war notice board lists a declared, an accepted and an
//! ended war as three notices in that order; an ended war drops after an hour; a notice crosses the
//! board answer and comes back as the same line key. Times are plain unix seconds the test passes in.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_OrgWarBoardText : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "org/war-board-text";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_OrganizationManager.Reset();
		LF_OrgWars.ResetStatics();
		LF_EventBus.ResetStatics();
		LF_OrgWarBoard.ResetStatics();
		LF_OrgWarBoard.Install();

		LF_Organization a = LF_OrganizationManager.Create("war-board-founder-a", "Board Alpha");
		LF_Organization b = LF_OrganizationManager.Create("war-board-founder-b", "Board Bravo");
		LF_Organization c = LF_OrganizationManager.Create("war-board-founder-c", "Board Charlie");
		LF_Organization d = LF_OrganizationManager.Create("war-board-founder-d", "Board Delta");
		LF_Organization e = LF_OrganizationManager.Create("war-board-founder-e", "Board Echo");
		LF_Organization f = LF_OrganizationManager.Create("war-board-founder-f", "Board Foxtrot");
		ctx.True(a && b && c && d && e && f, "six organizations are registered");
		if (!a || !b || !c || !d || !e || !f)
			return;

		LF_OrgWarRules rules = new LF_OrgWarRules();
		rules.m_iAnswerSeconds = 600;
		rules.m_iLengthSeconds = 3600;
		rules.m_iCooldownSeconds = 7200;
		rules.m_iMinMembers = 1;
		int t0 = 1800000000;

		// Three wars, made in the wrong order for the board: the ended one first, the declared one last.
		LF_OrgWarRecord ended;
		LF_OrgWarRecord running;
		LF_OrgWarRecord declared;
		if (!RedProof())
		{
			ctx.EqualStr("", LF_OrgWars.Declare(e.GetId(), f.GetId(), t0, rules, ended), "E declares on F");
			ctx.EqualStr("", LF_OrgWars.Accept(ended.m_sWarId, t0 + 10, rules), "F accepts");
			ctx.EqualStr("", LF_OrgWars.Declare(c.GetId(), d.GetId(), t0 + 100, rules, running), "C declares on D");
			ctx.EqualStr("", LF_OrgWars.Accept(running.m_sWarId, t0 + 200, rules), "D accepts");
			ctx.EqualStr("", LF_OrgWars.Declare(a.GetId(), b.GetId(), t0 + 3500, rules, declared), "A declares on B");
		}

		// E and F's war runs out at t0 + 3610 (the board closes it when it reads).
		int now = t0 + 3620;
		array<ref LF_OrgWarNotice> notices = {};
		LF_OrgWarBoard.Collect(now, notices);
		ctx.Equal(3, notices.Count(), "a declared, an accepted and an ended war give three notices");
		if (notices.Count() != 3)
			return;

		ctx.Equal(LF_EOrgWarNotice.DECLARED, notices[0].m_eKind, "the declared war comes first");
		ctx.Equal(LF_EOrgWarNotice.ACCEPTED, notices[1].m_eKind, "the war at its height comes second");
		ctx.Equal(LF_EOrgWarNotice.ENDED, notices[2].m_eKind, "the ended war comes last");
		ctx.EqualStr("Board Alpha", notices[0].m_sDeclarerName, "the declared notice names the declarer");
		ctx.EqualStr("Board Bravo", notices[0].m_sTargetName, "the declared notice names the target");
		ctx.Equal(t0 + 3500 + 600, notices[0].m_iUntil, "a declaration is due an answer at the window's end");
		ctx.Equal(t0 + 200 + 3600, notices[1].m_iUntil, "a running war names when it ends");
		ctx.EqualStr(LF_OrgWars.REASON_TIME, notices[2].m_sReason, "the ended notice says it ran out");
		ctx.Equal(t0 + 10 + 3600, notices[2].m_iAt, "the ended notice carries the deadline, not the read time");

		// The line keys follow the kind and the reason.
		ctx.EqualStr("#LF-OrgWarBoard_Declared", LF_OrgWarBoard.LineKey(LF_EOrgWarNotice.DECLARED, ""), "declared line key");
		ctx.EqualStr("#LF-OrgWarBoard_Accepted", LF_OrgWarBoard.LineKey(LF_EOrgWarNotice.ACCEPTED, ""), "accepted line key");
		ctx.EqualStr("#LF-OrgWarBoard_Ended", LF_OrgWarBoard.LineKey(LF_EOrgWarNotice.ENDED, LF_OrgWars.REASON_TIME), "ran-out line key");
		ctx.EqualStr("#LF-OrgWarBoard_Forfeit", LF_OrgWarBoard.LineKey(LF_EOrgWarNotice.ENDED, LF_OrgWars.REASON_FORFEIT), "forfeit line key");
		ctx.EqualStr("#LF-OrgWarBoard_Lapsed", LF_OrgWarBoard.LineKey(LF_EOrgWarNotice.ENDED, LF_OrgWars.REASON_LAPSED), "lapsed line key");

		// The answer crosses as flat arrays and reads back as the same three notices, times as spans.
		LF_BoardAnswer answer = LF_OrgWarBoard.Pack(notices, now);
		ctx.Equal(3 * LF_OrgWarBoard.TEXT_STRIDE, answer.m_aTexts.Count(), "three notices make three text rows");
		ctx.Equal(3 * LF_OrgWarBoard.NUMBER_STRIDE, answer.m_aNumbers.Count(), "three notices make three number rows");
		if (answer.m_aNumbers.Count() == 6)
		{
			ctx.Equal(LF_EOrgWarNotice.DECLARED, LF_OrgWarBoard.KindOf(answer.m_aNumbers[0]), "the answer's first row is the declared war");
			ctx.Equal(480, answer.m_aNumbers[1], "the declaration has 480 s left to answer");
			ctx.Equal(LF_EOrgWarNotice.ENDED, LF_OrgWarBoard.KindOf(answer.m_aNumbers[4]), "the answer's last row is the ended war");
			ctx.Equal(10, answer.m_aNumbers[5], "the ended war closed 10 s ago");
		}

		ctx.Equal(1, LF_OrgWarBoard.SpanMinutes(1), "a second left reads as one minute");
		ctx.Equal(1, LF_OrgWarBoard.SpanMinutes(60), "sixty seconds is one minute");
		ctx.Equal(2, LF_OrgWarBoard.SpanMinutes(61), "sixty-one seconds rounds up to two");

		// An ended notice stays up an hour from its end, then drops; an open one never ages out.
		array<ref LF_OrgWarNotice> hand = {};
		hand.Insert(LF_OrgWarNotice.Create(LF_EOrgWarNotice.ENDED, "hand-1", "X", "Y", LF_OrgWars.REASON_TIME, t0, 0));
		hand.Insert(LF_OrgWarNotice.Create(LF_EOrgWarNotice.DECLARED, "hand-2", "P", "Q", "", t0, t0 + 600));
		LF_OrgWarBoard.Arrange(hand, t0 + LF_OrgWarBoard.ENDED_KEEP_SECONDS - 1);
		ctx.Equal(2, hand.Count(), "an ended war is still up a second before its hour");
		LF_OrgWarBoard.Arrange(hand, t0 + LF_OrgWarBoard.ENDED_KEEP_SECONDS);
		ctx.Equal(1, hand.Count(), "an ended war drops after its hour");
		if (hand.Count() == 1)
			ctx.Equal(LF_EOrgWarNotice.DECLARED, hand[0].m_eKind, "the open notice stays when the ended one drops");

		// The same through the real path: well past the hour the first war's notice is gone, and the
		// two wars the clock closed since are up as ended.
		LF_OrgWarBoard.Collect(t0 + 3610 + LF_OrgWarBoard.ENDED_KEEP_SECONDS, notices);
		ctx.Equal(2, notices.Count(), "the first war's notice dropped; the two closed since are up");
		foreach (LF_OrgWarNotice late : notices)
		{
			ctx.Equal(LF_EOrgWarNotice.ENDED, late.m_eKind, "every notice left is an ended war");
		}

		LF_OrgWarBoard.ResetStatics();
		LF_OrgWars.ResetStatics();
		LF_EventBus.ResetStatics();
	}
}
