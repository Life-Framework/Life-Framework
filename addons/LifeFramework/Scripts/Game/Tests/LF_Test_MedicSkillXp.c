// red-proof: observed 2026-11-23 `tools/cli test --no-build --red LF_Test_MedicSkillXp`: RED PROOF OK; with both subscribers off and no perk picks it fails the revive, delivery and first-aid XP checks and the three perk checks. Green with `--only`.
//! medic/skill-xp (#4433, PO-S5): a revive, a delivered patient and a first aid on a bleeding player each
//! award the configured MEDIC XP once (a repeat inside the cooldown pays nothing); a self-revive, a
//! self-delivery, a missing medic, a bandage that bought no time, a medic off duty and an hour on duty
//! award none; with each perk picked, the revive hold, the dressing time and the carried bleed rate read
//! the perk value, and without it the base. No row here touches pay.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_MedicSkillXp : LF_Test
{
	static const string MEDIC = "test-medicskill-medic";
	static const string PATIENT = "test-medicskill-patient";
	static const string OTHER = "test-medicskill-other";

	override string GetName()
	{
		return "medic/skill-xp";
	}

	//------------------------------------------------------------------------------------------------
	protected void ForgetAll()
	{
		LF_JobLevels.Forget(MEDIC);
		LF_JobLevels.Forget(OTHER);
		LF_JobLevels.Forget(PATIENT);
		LF_TradePerks.Forget(MEDIC);
		LF_TradePerks.Forget(OTHER);
	}

	//------------------------------------------------------------------------------------------------
	protected int Row(LF_JobsConfig config, string action, LF_TestContext ctx)
	{
		LF_JobXpAwardDef award = config.FindAward(action);
		ctx.NotNull(award, "the shipped config awards " + action);
		if (!award)
			return 0;

		ctx.EqualStr(LF_MedicSkill.JOB, award.m_sJob, action + " feeds MEDIC");
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

		int revive = Row(config, LF_JobLevelSources.ACTION_MEDIC_REVIVE, ctx);
		int delivery = Row(config, LF_JobLevelSources.ACTION_MEDIC_DELIVERY, ctx);
		int firstAid = Row(config, LF_MedicSkill.ACTION_FIRST_AID, ctx);

		// XP is for outcomes only: the three rows are every MEDIC award, none is a duty or time row.
		int medicRows = 0;
		foreach (LF_JobXpAwardDef def : config.m_aXpAwards)
		{
			if (def.m_sJob == LF_MedicSkill.JOB)
				medicRows++;
		}

		ctx.Equal(3, medicRows, "MEDIC has exactly three award rows: revive, delivery, first aid (nothing for time on duty)");

		ForgetAll();
		if (RedProof())
		{
			LF_MedicSkill.ResetStatics();
			LF_JobLevelSources.ResetStatics();
		}
		else
		{
			LF_MedicSkill.EnsureSubscribed();
			LF_JobLevelSources.EnsureSubscribed();
		}

		vector here = "0 0 0";

		// A revive pays once; the same revive again inside the cooldown pays nothing.
		LF_EventBus.Publish(LF_OnPlayerRevived.Create(PATIENT, MEDIC, here));
		ctx.Equal(revive, LF_JobLevels.XpOf(MEDIC, LF_MedicSkill.JOB), "a revive awards its configured MEDIC xp");
		LF_EventBus.Publish(LF_OnPlayerRevived.Create(PATIENT, MEDIC, here));
		ctx.Equal(revive, LF_JobLevels.XpOf(MEDIC, LF_MedicSkill.JOB), "a repeat revive inside the cooldown awards nothing more");

		// A delivered patient pays once.
		LF_EventBus.Publish(LF_OnPatientDelivered.Create(PATIENT, MEDIC, 7, here));
		ctx.Equal(revive + delivery, LF_JobLevels.XpOf(MEDIC, LF_MedicSkill.JOB), "a delivered patient awards its configured MEDIC xp");

		// First aid pays for time actually bought, once; a bandage that added nothing pays nothing.
		LF_MedicSkill.OnFirstAid(MEDIC, PATIENT, 0, true);
		ctx.Equal(revive + delivery, LF_JobLevels.XpOf(MEDIC, LF_MedicSkill.JOB), "a first aid that added no time awards nothing");
		LF_MedicSkill.OnFirstAid(MEDIC, PATIENT, 30, false);
		ctx.Equal(revive + delivery, LF_JobLevels.XpOf(MEDIC, LF_MedicSkill.JOB), "first aid by a medic off duty awards nothing");
		LF_MedicSkill.OnFirstAid(MEDIC, PATIENT, 30, true);
		ctx.Equal(revive + delivery + firstAid, LF_JobLevels.XpOf(MEDIC, LF_MedicSkill.JOB), "first aid on a bleeding player awards its configured MEDIC xp");
		LF_MedicSkill.OnFirstAid(MEDIC, PATIENT, 30, true);
		ctx.Equal(revive + delivery + firstAid, LF_JobLevels.XpOf(MEDIC, LF_MedicSkill.JOB), "a second dressing inside the cooldown awards nothing more");

		// Self-award holes: reviving, delivering or bandaging yourself, and an NPC medic, earn nothing.
		LF_JobLevels.Forget(OTHER);
		LF_EventBus.Publish(LF_OnPlayerRevived.Create(OTHER, OTHER, here));
		LF_EventBus.Publish(LF_OnPatientDelivered.Create(OTHER, OTHER, 0, here));
		LF_MedicSkill.OnFirstAid(OTHER, OTHER, 30, true);
		LF_EventBus.Publish(LF_OnPlayerRevived.Create(PATIENT, string.Empty, here));
		ctx.Equal(0, LF_JobLevels.XpOf(OTHER, LF_MedicSkill.JOB), "a self-revive, a self-delivery and a self-bandage award no MEDIC xp");
		ctx.False(LF_MedicSkill.MayAward(string.Empty, PATIENT), "a missing medic earns nothing");
		ctx.False(LF_MedicSkill.MayAward(MEDIC, MEDIC), "a medic never earns on themselves");
		ctx.True(LF_MedicSkill.MayAward(MEDIC, PATIENT), "a medic earns on another player");

		// An hour on duty is no outcome: nothing in the skill reads the clock.
		ctx.Equal(0, LF_JobLevels.XpOf(PATIENT, LF_MedicSkill.JOB), "a character who only stays on duty has no MEDIC xp");

		// Perks: the base without a pick.
		ctx.EqualFloat(2.0, LF_MedicSkill.ReviveSeconds(MEDIC, 2.0), 0.001, "no perk: the revive hold is the base");
		ctx.EqualFloat(1.0, LF_MedicSkill.ReviveSpeed(MEDIC), 0.001, "no perk: the revive bar runs at normal speed");
		ctx.EqualFloat(30, LF_MedicSkill.AidSeconds(MEDIC, 30), 0.001, "no perk: a dressing buys the base seconds");
		ctx.EqualFloat(1.0, LF_MedicSkill.BleedScale(MEDIC, true), 0.001, "no perk: a carried patient bleeds at the normal rate");

		if (!RedProof())
		{
			LF_TradePerks.Pick(MEDIC, LF_MedicSkill.JOB, 10, LF_MedicSkill.PERK_QUICK_REVIVE);
			LF_TradePerks.Pick(MEDIC, LF_MedicSkill.JOB, 20, LF_MedicSkill.PERK_QUICK_AID);
			LF_TradePerks.Pick(OTHER, LF_MedicSkill.JOB, 10, LF_MedicSkill.PERK_STEADY_CARRY);
		}

		ctx.EqualFloat(2.0 * LF_MedicSkill.REVIVE_PERCENT / 100, LF_MedicSkill.ReviveSeconds(MEDIC, 2.0), 0.001, "quick-revive perk: the revive hold reads the perk value");
		ctx.EqualFloat(100.0 / LF_MedicSkill.REVIVE_PERCENT, LF_MedicSkill.ReviveSpeed(MEDIC), 0.001, "quick-revive perk: the revive bar runs faster");
		ctx.EqualFloat(30 * LF_MedicSkill.AID_PERCENT / 100, LF_MedicSkill.AidSeconds(MEDIC, 30), 0.001, "quick-aid perk: a dressing reads the perk value");
		ctx.EqualFloat(30, LF_MedicSkill.AidSeconds(OTHER, 30), 0.001, "a perk is the medic's own: another medic's dressing is the base");
		ctx.EqualFloat(1.0, LF_MedicSkill.BleedScale(MEDIC, true), 0.001, "a medic without the steady-carry perk carries at the normal rate");
		ctx.EqualFloat(LF_MedicSkill.CARRY_BLEED_PERCENT / 100.0, LF_MedicSkill.BleedScale(OTHER, true), 0.001, "steady-carry perk: a carried patient reads the perk bleed rate");
		ctx.EqualFloat(1.0, LF_MedicSkill.BleedScale(OTHER, false), 0.001, "steady-carry perk: a patient not carried bleeds at the normal rate");
		ctx.True(LF_MedicSkill.ReviveSeconds(MEDIC, 2.0) < 2.0, "the quick-revive perk is faster");
		ctx.True(LF_MedicSkill.AidSeconds(MEDIC, 30) > 30, "the quick-aid perk buys more time");
		ctx.True(LF_MedicSkill.BleedScale(OTHER, true) < 1.0, "the steady-carry perk holds the bleed-out longer");

		ForgetAll();
		LF_MedicSkill.EnsureSubscribed();
		LF_JobLevelSources.EnsureSubscribed();
		LF_JobsConfig.Reload();
	}
}
