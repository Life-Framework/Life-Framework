// red-proof: 2026-11-19 `tools\cli test --no-build --red LF_Test_QualificationRules`: RED PROOF OK (RedProof() skips every LF_QualificationRules.Check act, so the pass, each refusal reason and the unlock ids all fail). Green without the hook.
//! qualifications/rules: the course list is config and the rules refuse in words. The core table
//! loads with its four courses; a character who fits (faction, rank, purse, course not yet held)
//! passes and gets the unlock ids, the fee and the study time; each refusal (unknown course, already
//! held, wrong faction, rank too low, short purse) names its own localization key and the number its
//! sentence needs; the first failing rule wins; a course with no faction is open to everyone; an
//! unlock list is trimmed and has no empty entries; a missing table degrades to "unknown course".
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_QualificationRules : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "qualifications/rules";
	}

	//------------------------------------------------------------------------------------------------
	//! The core table as shipped, with no pack overlay (the core leg boots without a pack).
	protected LF_QualificationConfig LoadCore()
	{
		Resource container = BaseContainerTools.LoadContainer(LF_QualificationConfig.CONFIG_PATH);
		if (!container || !container.IsValid() || !container.GetResource())
			return null;

		LF_QualificationConfig config = LF_QualificationConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (config && !config.m_aCourses)
			config.m_aCourses = new array<ref LF_QualificationCourse>();

		return config;
	}

	//------------------------------------------------------------------------------------------------
	//! The act under test. A red-proof run skips it, so every assertion about its verdict fails.
	protected LF_QualificationVerdict Ask(LF_QualificationConfig config, string courseId, string faction, int rank, notnull array<string> held, int cash)
	{
		if (RedProof())
			return new LF_QualificationVerdict();

		return LF_QualificationRules.Check(config, courseId, faction, rank, held, cash);
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_QualificationConfig table = LoadCore();
		ctx.NotNull(table, "the core course table loads");
		if (!table)
			return;

		ctx.True(table.Find("detective_course") != null, "the core table has the detective course");
		ctx.True(table.Find("nonsense") == null, "an unknown id finds nothing");

		RunPass(ctx, table);
		RunRefusals(ctx, table);
		RunOrder(ctx, table);
		RunOpenCourse(ctx, table);
		RunUnlocks(ctx);
		RunMissingTable(ctx);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunPass(LF_TestContext ctx, LF_QualificationConfig table)
	{
		array<string> held = {};
		LF_QualificationVerdict v = Ask(table, "detective_course", "police", 1, held, 400);
		ctx.True(v.m_bOk, "a rank 1 officer with the exact fee passes the detective course");
		ctx.EqualStr("", v.m_sReason, "a pass has no refusal");
		ctx.Equal(1, v.m_aUnlocks.Count(), "the detective course unlocks one id");
		if (v.m_aUnlocks.Count() == 1)
			ctx.EqualStr("detective", v.m_aUnlocks[0], "the unlock is the string detective");

		ctx.Equal(400, v.m_iFee, "a pass names the fee to charge");
		ctx.Equal(240, v.m_iStudyMinutes, "a pass names the study time");

		LF_QualificationVerdict rich = Ask(table, "detective_course", "police", 5, held, 100000);
		ctx.True(rich.m_bOk, "a higher rank and a full purse also pass");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunRefusals(LF_TestContext ctx, LF_QualificationConfig table)
	{
		array<string> held = {};

		LF_QualificationVerdict unknown = Ask(table, "nonsense", "police", 5, held, 9999);
		ctx.False(unknown.m_bOk, "an unknown course is refused");
		ctx.EqualStr(LF_QualificationRules.REASON_UNKNOWN, unknown.m_sReason, "an unknown course says so");

		LF_QualificationVerdict faction = Ask(table, "detective_course", "medic", 5, held, 9999);
		ctx.False(faction.m_bOk, "the wrong faction is refused");
		ctx.EqualStr(LF_QualificationRules.REASON_FACTION, faction.m_sReason, "the wrong faction says so");

		LF_QualificationVerdict rank = Ask(table, "detective_course", "police", 0, held, 9999);
		ctx.False(rank.m_bOk, "too low a rank is refused");
		ctx.EqualStr(LF_QualificationRules.REASON_RANK, rank.m_sReason, "too low a rank says so");
		ctx.Equal(1, rank.m_iNumber, "the rank sentence carries the rank needed");

		LF_QualificationVerdict purse = Ask(table, "detective_course", "police", 1, held, 399);
		ctx.False(purse.m_bOk, "a short purse is refused");
		ctx.EqualStr(LF_QualificationRules.REASON_PURSE, purse.m_sReason, "a short purse says so");
		ctx.Equal(400, purse.m_iNumber, "the purse sentence carries the fee");

		array<string> has = {"detective_course"};
		LF_QualificationVerdict again = Ask(table, "detective_course", "police", 5, has, 9999);
		ctx.False(again.m_bOk, "a course already held is refused");
		ctx.EqualStr(LF_QualificationRules.REASON_HELD, again.m_sReason, "a held course says so");
		ctx.Equal(0, again.m_aUnlocks.Count(), "a refusal returns no unlocks");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunOrder(LF_TestContext ctx, LF_QualificationConfig table)
	{
		// Wrong faction, rank and purse all fail at once: faction is reported. Held beats them all.
		array<string> held = {};
		LF_QualificationVerdict all = Ask(table, "detective_course", "medic", 0, held, 0);
		ctx.EqualStr(LF_QualificationRules.REASON_FACTION, all.m_sReason, "faction is reported before rank and purse");

		LF_QualificationVerdict rankAndPurse = Ask(table, "detective_course", "police", 0, held, 0);
		ctx.EqualStr(LF_QualificationRules.REASON_RANK, rankAndPurse.m_sReason, "rank is reported before purse");

		array<string> has = {"detective_course"};
		LF_QualificationVerdict heldFirst = Ask(table, "detective_course", "medic", 0, has, 0);
		ctx.EqualStr(LF_QualificationRules.REASON_HELD, heldFirst.m_sReason, "a held course is reported before the rest");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunOpenCourse(LF_TestContext ctx, LF_QualificationConfig table)
	{
		array<string> held = {};
		LF_QualificationVerdict v = Ask(table, "first_aid_course", "civilian", 0, held, 50);
		ctx.True(v.m_bOk, "a course with no faction is open to a civilian at the entry rank");
		LF_QualificationVerdict medic = Ask(table, "first_aid_course", "medic", 0, held, 50);
		ctx.True(medic.m_bOk, "and to a medic");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunUnlocks(LF_TestContext ctx)
	{
		LF_QualificationConfig config = new LF_QualificationConfig();
		config.m_aCourses = new array<ref LF_QualificationCourse>();
		LF_QualificationCourse course = LF_QualificationCourse.Make("multi");
		course.m_sUnlocks = " detective , ,traffic,";
		config.m_aCourses.Insert(course);

		array<string> held = {};
		LF_QualificationVerdict v = Ask(config, "multi", "", 0, held, 0);
		ctx.True(v.m_bOk, "a free open course passes with an empty purse");
		ctx.Equal(2, v.m_aUnlocks.Count(), "the unlock list is trimmed and has no empty entries");
		if (v.m_aUnlocks.Count() == 2)
		{
			ctx.EqualStr("detective", v.m_aUnlocks[0], "first unlock");
			ctx.EqualStr("traffic", v.m_aUnlocks[1], "second unlock");
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void RunMissingTable(LF_TestContext ctx)
	{
		array<string> held = {};
		LF_QualificationVerdict v = Ask(null, "detective_course", "police", 5, held, 9999);
		ctx.False(v.m_bOk, "no table: nothing passes");
		ctx.EqualStr(LF_QualificationRules.REASON_UNKNOWN, v.m_sReason, "no table: the course is unknown, no throw");
	}
}
