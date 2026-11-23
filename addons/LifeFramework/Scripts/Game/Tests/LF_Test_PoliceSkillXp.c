// red-proof: observed 2026-11-23 `tools/cli test --no-build --red LF_Test_PoliceSkillXp`: RED PROOF OK; with the subscriber off and no perk picks it fails the recovery XP, fast-cuff and long-radar checks (quick booking was retired 2026-11-23, #4409: the desk booking is instant, so no hold could use it). Green with `--only`.
//! police/skill-xp (#4424, PO-R8): a resolved call, a booking and a recovered stolen car each award the
//! configured POLICE XP; an unfounded close, no one found and an hour on duty award none; with each perk
//! picked, the cuff time, radar range and booking time read the perk value, and without it the base.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_PoliceSkillXp : LF_Test
{
	static const string OFFICER = "test-policeskill-officer";
	static const string OWNER = "test-policeskill-owner";
	static const string OTHER = "test-policeskill-other";

	override string GetName()
	{
		return "police/skill-xp";
	}

	//------------------------------------------------------------------------------------------------
	protected void ForgetAll()
	{
		LF_JobLevels.Forget(OFFICER);
		LF_JobLevels.Forget(OTHER);
		LF_TradePerks.Forget(OFFICER);
		LF_TradePerks.Forget(OTHER);
	}

	//------------------------------------------------------------------------------------------------
	protected int Row(LF_JobsConfig config, string action, LF_TestContext ctx)
	{
		LF_JobXpAwardDef award = config.FindAward(action);
		ctx.NotNull(award, "the shipped config awards " + action);
		if (!award)
			return 0;

		ctx.EqualStr(LF_PoliceSkill.JOB, award.m_sJob, action + " feeds POLICE");
		ctx.True(award.m_iXp > 0, action + " is worth XP");
		return award.m_iXp;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_ConfigService.Reset();
		LF_JobsConfig.Reload();
		LF_JobsConfig config = LF_JobsConfig.Get();
		ctx.NotNull(config, "the shipped jobs config loads");
		if (!config)
			return;

		int resolved = Row(config, LF_PoliceSkill.ACTION_CALL_RESOLVED, ctx);
		int booking = Row(config, LF_PoliceSkill.ACTION_BOOKING, ctx);
		int recovery = Row(config, LF_PoliceSkill.ACTION_RECOVERY, ctx);

		// XP is for outcomes only: the three rows are every POLICE award, none is a time or patrol row.
		int policeRows = 0;
		foreach (LF_JobXpAwardDef def : config.m_aXpAwards)
		{
			if (def.m_sJob == LF_PoliceSkill.JOB)
				policeRows++;
		}

		ctx.Equal(3, policeRows, "POLICE has exactly three award rows: call, booking, recovery (nothing for time on duty)");

		ForgetAll();
		if (RedProof())
			LF_PoliceSkill.ResetStatics();
		else
			LF_PoliceSkill.EnsureSubscribed();

		// A call closed as resolved pays; no one found and unfounded pay nothing.
		LF_PoliceSkill.OnCallClosed(OFFICER, LF_CallOutcomeKind.UNFOUNDED);
		LF_PoliceSkill.OnCallClosed(OFFICER, LF_CallOutcomeKind.NO_ONE_FOUND);
		LF_PoliceSkill.OnCallClosed(OFFICER, LF_CallOutcomeKind.NONE);
		ctx.Equal(0, LF_JobLevels.XpOf(OFFICER, LF_PoliceSkill.JOB), "an unfounded close, no one found and no outcome award no POLICE xp");
		LF_PoliceSkill.OnCallClosed(OFFICER, LF_CallOutcomeKind.RESOLVED);
		ctx.Equal(resolved, LF_JobLevels.XpOf(OFFICER, LF_PoliceSkill.JOB), "a resolved call awards its configured POLICE xp");

		// A booking pays.
		LF_PoliceSkill.OnBooked(OFFICER);
		ctx.Equal(resolved + booking, LF_JobLevels.XpOf(OFFICER, LF_PoliceSkill.JOB), "a booking awards its configured POLICE xp");

		// A recovered stolen car pays through the bus; an officer's own car pays nothing.
		LF_EventBus.Publish(LF_OnVehicleRecovered.Create(OFFICER, OWNER, "veh-1", "ABC123"));
		ctx.Equal(resolved + booking + recovery, LF_JobLevels.XpOf(OFFICER, LF_PoliceSkill.JOB), "a recovered stolen car awards its configured POLICE xp");
		LF_JobLevels.Forget(OFFICER);
		LF_EventBus.Publish(LF_OnVehicleRecovered.Create(OFFICER, OFFICER, "veh-2", "XYZ789"));
		ctx.Equal(0, LF_JobLevels.XpOf(OFFICER, LF_PoliceSkill.JOB), "recovering your own car awards nothing");

		// An hour on duty is no outcome: nothing in the skill reads the clock, so no call awards XP.
		ctx.Equal(0, LF_JobLevels.XpOf(OTHER, LF_PoliceSkill.JOB), "an officer who only stays on duty has no POLICE xp");

		// Perks: the base without a pick, the perk value with it.
		ctx.EqualFloat(1.8, LF_PoliceSkill.CuffSeconds(OFFICER, 1.8), 0.001, "no perk: the cuff hold is the base");
		ctx.EqualFloat(150, LF_PoliceSkill.RadarRange(OFFICER, 150), 0.001, "no perk: the radar range is the base");

		if (!RedProof())
		{
			LF_TradePerks.Pick(OFFICER, LF_PoliceSkill.JOB, 10, LF_PoliceSkill.PERK_FAST_CUFF);
			LF_TradePerks.Pick(OFFICER, LF_PoliceSkill.JOB, 20, LF_PoliceSkill.PERK_LONG_RADAR);
		}

		ctx.EqualFloat(1.8 * LF_PoliceSkill.CUFF_PERCENT / 100, LF_PoliceSkill.CuffSeconds(OFFICER, 1.8), 0.001, "fast-cuff perk: the cuff hold reads the perk value");
		ctx.EqualFloat(150 * LF_PoliceSkill.RADAR_PERCENT / 100, LF_PoliceSkill.RadarRange(OFFICER, 150), 0.001, "long-radar perk: the radar range reads the perk value");
		ctx.EqualFloat(1.8, LF_PoliceSkill.CuffSeconds(OTHER, 1.8), 0.001, "a perk is the officer's own: another officer cuffs at the base");
		ctx.True(LF_PoliceSkill.CuffSeconds(OFFICER, 1.8) < 1.8, "the fast-cuff perk is faster");
		ctx.True(LF_PoliceSkill.RadarRange(OFFICER, 150) > 150, "the long-radar perk reaches farther");

		ForgetAll();
		LF_PoliceSkill.EnsureSubscribed();
		LF_JobsConfig.Reload();
	}
}
