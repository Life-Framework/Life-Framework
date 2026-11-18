// red-proof: 2026-11-18 `tools\cli test --no-build --red LF_Test_OrgWarSheet`: RED-OK org/war-sheet (hook skips the declared and accepted wars: "one war is running (expected 1, got 0)", "a war that just ended leaves the pair on cooldown (expected ..StateCooldown, got ..Members)"). Plain run green first time.
//! org/war-sheet (#2675, M7 gang war W7): the war sheet's row model. A viewer with the right sees Declare on
//! an open organisation and none on one at war or under cooldown; a viewer without the right sees the
//! same rows with no buttons; a declaration against the viewer shows Accept and the time left; an
//! active war shows its end time and no button. Times are plain unix seconds the test passes in.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_OrgWarSheet : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "org/war-sheet";
	}

	//------------------------------------------------------------------------------------------------
	protected LF_OrgWarSheetRow FindRow(notnull array<ref LF_OrgWarSheetRow> rows, string target)
	{
		foreach (LF_OrgWarSheetRow row : rows)
		{
			if (row.m_sTarget == target)
				return row;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_OrganizationManager.Reset();
		LF_OrgWars.ResetStatics();
		LF_EventBus.ResetStatics();

		LF_Organization us = LF_OrganizationManager.Create("war-sheet-us-boss", "Sheet Us");
		LF_Organization openOrg = LF_OrganizationManager.Create("war-sheet-open-boss", "Sheet Open");
		LF_Organization atWar = LF_OrganizationManager.Create("war-sheet-atwar-boss", "Sheet AtWar");
		LF_Organization pending = LF_OrganizationManager.Create("war-sheet-pending-boss", "Sheet Pending");
		LF_Organization few = LF_OrganizationManager.Create("war-sheet-few-boss", "Sheet Few");
		ctx.True(us && openOrg && atWar && pending && few, "five organizations are registered");
		if (!us || !openOrg || !atWar || !pending || !few)
			return;

		LF_OrgWarRules rules = new LF_OrgWarRules();
		rules.m_iFee = 5000;
		rules.m_iAnswerSeconds = 600;
		rules.m_iLengthSeconds = 3600;
		rules.m_iCooldownSeconds = 7200;
		rules.m_iMinMembers = 2;

		// Everyone but "few" has two members; the founders are the first member.
		LF_OrganizationManager.JoinDirect(us.GetId(), "war-sheet-us-member", LF_OrganizationConfig.RANK_MEMBER);
		LF_OrganizationManager.JoinDirect(openOrg.GetId(), "war-sheet-open-member", LF_OrganizationConfig.RANK_MEMBER);
		LF_OrganizationManager.JoinDirect(atWar.GetId(), "war-sheet-atwar-member", LF_OrganizationConfig.RANK_MEMBER);
		LF_OrganizationManager.JoinDirect(pending.GetId(), "war-sheet-pending-member", LF_OrganizationConfig.RANK_MEMBER);

		int t0 = 1800000000;
		LF_OrgWarRecord running;
		LF_OrgWarRecord declaredOnUs;
		if (!RedProof())
		{
			ctx.EqualStr("", LF_OrgWars.Declare(us.GetId(), atWar.GetId(), t0, rules, running), "we declare on AtWar");
			ctx.EqualStr("", LF_OrgWars.Accept(running.m_sWarId, t0 + 30, rules), "AtWar accepts");
			ctx.EqualStr("", LF_OrgWars.Declare(pending.GetId(), us.GetId(), t0 + 60, rules, declaredOnUs), "Pending declares on us");
		}

		// The founder holds DECLARE_WAR: 60 s after the declaration, 540 s of the window are left.
		int now = t0 + 120;
		LF_OrgWarSheetPage founderPage = LF_OrgWarSheet.BuildPageAt("war-sheet-us-boss", now, rules);
		ctx.True(founderPage.m_bHasOrg && founderPage.m_bCanDeclare, "the founder's page has an organisation and the right");
		ctx.Equal(5000, founderPage.m_iFee, "the page carries the fee");
		ctx.Equal(2, founderPage.m_iMinMembers, "the page carries the member minimum");

		array<ref LF_OrgWarSheetRow> orgRows = LF_OrgWarSheet.Rows(founderPage, LF_OrgWarSheet.SECTION_ORGS);
		ctx.Equal(4, orgRows.Count(), "the register shows every other organisation (not our own)");
		LF_OrgWarSheetRow openRow = FindRow(orgRows, openOrg.GetId());
		LF_OrgWarSheetRow atWarRow = FindRow(orgRows, atWar.GetId());
		LF_OrgWarSheetRow pendingRow = FindRow(orgRows, pending.GetId());
		LF_OrgWarSheetRow fewRow = FindRow(orgRows, few.GetId());
		ctx.True(openRow && atWarRow && pendingRow && fewRow, "all four register rows are there");
		if (!openRow || !atWarRow || !pendingRow || !fewRow)
			return;

		ctx.EqualStr("#LF-OrgWarSheet_Declare", openRow.m_sAction, "an open organisation carries Declare for the founder");
		ctx.EqualStr("#LF-OrgWarSheet_Members", openRow.m_sStatusKey, "an open row reads its member count");
		ctx.EqualStr("2", openRow.m_sStatusArg, "the member count is 2");
		ctx.EqualStr("", atWarRow.m_sAction, "an organisation at war has no Declare");
		ctx.EqualStr("#LF-OrgWarSheet_StateAtWar", atWarRow.m_sStatusKey, "and says it is at war");
		ctx.EqualStr("", pendingRow.m_sAction, "an organisation with a declaration open has no Declare");
		ctx.EqualStr("#LF-OrgWarSheet_StatePending", pendingRow.m_sStatusKey, "and says it is pending");
		ctx.EqualStr("", fewRow.m_sAction, "an organisation under the member minimum has no Declare");
		ctx.EqualStr("#LF-OrgWarSheet_StateTooFew", fewRow.m_sStatusKey, "and says it is short of members");
		ctx.EqualStr("2", fewRow.m_sStatusArg, "naming the member minimum");

		// The declaration against us: Accept, with the time left in the window (600 - 60 = 540).
		array<ref LF_OrgWarSheetRow> pendingRows = LF_OrgWarSheet.Rows(founderPage, LF_OrgWarSheet.SECTION_PENDING);
		ctx.Equal(1, pendingRows.Count(), "one declaration is made against us");
		if (pendingRows.Count() == 1)
		{
			ctx.EqualStr("#LF-OrgWarSheet_Accept", pendingRows[0].m_sAction, "a pending row carries Accept for the founder");
			ctx.EqualStr("Sheet Pending", pendingRows[0].m_sLabel, "it names the declaring organisation");
			ctx.Equal(540, pendingRows[0].m_iSeconds, "and the time left in the answer window");
			ctx.EqualStr(LF_OrgWarSheet.PREFIX_ACCEPT + declaredOnUs.m_sWarId, pendingRows[0].m_sName, "its button names the war");
		}

		// The war we are in: its end time, no button.
		array<ref LF_OrgWarSheetRow> activeRows = LF_OrgWarSheet.Rows(founderPage, LF_OrgWarSheet.SECTION_ACTIVE);
		ctx.Equal(1, activeRows.Count(), "one war is running");
		if (activeRows.Count() == 1)
		{
			ctx.EqualStr("", activeRows[0].m_sAction, "an active war has no button");
			ctx.EqualStr("Sheet AtWar", activeRows[0].m_sLabel, "it names the other organisation");
			ctx.Equal(3510, activeRows[0].m_iSeconds, "and the time until it ends (30 + 3600 - 120)");
		}

		// A plain member lacks DECLARE_WAR: the same rows, no buttons, and the intro says why.
		LF_OrgWarSheetPage memberPage = LF_OrgWarSheet.BuildPageAt("war-sheet-us-member", now, rules);
		ctx.True(memberPage.m_bHasOrg && !memberPage.m_bCanDeclare, "a plain member has no right");
		foreach (LF_OrgWarSheetRow memberRow : LF_OrgWarSheet.Rows(memberPage, LF_OrgWarSheet.SECTION_ORGS))
		{
			ctx.EqualStr("", memberRow.m_sAction, "no Declare for a member without the right: " + memberRow.m_sLabel);
		}

		foreach (LF_OrgWarSheetRow memberPending : LF_OrgWarSheet.Rows(memberPage, LF_OrgWarSheet.SECTION_PENDING))
		{
			ctx.EqualStr("", memberPending.m_sAction, "no Accept for a member without the right");
		}

		string fee;
		string members;
		ctx.EqualStr("#LF-OrgWarSheet_IntroNoRight", LF_OrgWarSheet.IntroKey(memberPage, fee, members), "the intro says the member lacks the right");
		ctx.EqualStr("#LF-OrgWarSheet_Intro", LF_OrgWarSheet.IntroKey(founderPage, fee, members), "the intro names fee and minimum for the founder");
		ctx.EqualStr("2", members, "the minimum is passed in words");

		// Someone with no organisation: the register, no buttons.
		LF_OrgWarSheetPage strangerPage = LF_OrgWarSheet.BuildPageAt("war-sheet-stranger", now, rules);
		ctx.True(!strangerPage.m_bHasOrg, "a stranger has no organisation");
		ctx.Equal(5, LF_OrgWarSheet.Rows(strangerPage, LF_OrgWarSheet.SECTION_ORGS).Count(), "a stranger sees the whole register");
		foreach (LF_OrgWarSheetRow strangerRow : LF_OrgWarSheet.Rows(strangerPage, LF_OrgWarSheet.SECTION_ORGS))
		{
			ctx.EqualStr("", strangerRow.m_sAction, "no Declare for a stranger: " + strangerRow.m_sLabel);
		}

		ctx.EqualStr("#LF-OrgWarSheet_IntroNoOrg", LF_OrgWarSheet.IntroKey(strangerPage, fee, members), "the intro tells a stranger to join one");

		// Once the war has run out, the declaration answered or lapsed and the cooldown runs: Cooldown, not Declare.
		int later = t0 + 3600 + 30 + 10;
		LF_OrgWarSheetPage afterPage = LF_OrgWarSheet.BuildPageAt("war-sheet-us-boss", later, rules);
		LF_OrgWarSheetRow restedRow = FindRow(LF_OrgWarSheet.Rows(afterPage, LF_OrgWarSheet.SECTION_ORGS), atWar.GetId());
		ctx.True(restedRow != null, "the finished war's organisation is still on the register");
		if (restedRow)
		{
			ctx.EqualStr("#LF-OrgWarSheet_StateCooldown", restedRow.m_sStatusKey, "a war that just ended leaves the pair on cooldown");
			ctx.EqualStr("", restedRow.m_sAction, "and offers no Declare");
		}

		ctx.Equal(0, LF_OrgWarSheet.Rows(afterPage, LF_OrgWarSheet.SECTION_ACTIVE).Count(), "no active war is listed once it ended");
	}
}
