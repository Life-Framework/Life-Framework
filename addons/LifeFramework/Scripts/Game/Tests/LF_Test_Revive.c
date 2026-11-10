// red-proof: observed 2026-11-09 with `tools\cli test --no-build --only LF_Test_Revive`: LF_Revive.Decide ignoring the range failed it with `- a medic out of range is refused (expected 5, got 0)`. Restored; green.

//------------------------------------------------------------------------------------------------
//! Records the LF_OnPlayerRevived events a test publishes.
class LF_TestReviveRecorder : LF_EventSubscriber
{
	string m_sLog;

	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnPlayerRevived revived = LF_OnPlayerRevived.Cast(busEvent);
		if (revived)
			m_sLog += revived.m_sPlayerUid + "<" + revived.m_sMedicUid + ";";

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! medic/revive (#259): an on-duty medic within range revives a downed patient (the downed state
//! and the bleed-out clock end, the event is published, the inventory is untouched); a civilian,
//! a medic out of range, a patient who is not down and a revive of yourself are refused and change
//! nothing. Test characters have no player behind them, so the duty flag is passed in.
// tier: WORLD
class LF_Test_Revive : LF_Test
{
	override string GetName()
	{
		return "medic/revive";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_MedicConfig config = LF_MedicConfig.Get();
		bool wasEnabled = config.m_bDownedEnabled;
		config.m_bDownedEnabled = true;
		LF_Downed.ResetStatics();
		LF_Downed.SetTestAllEligible(true);
		LF_Revive.ResetStatics();
		LF_EventBus.ResetStatics();
		LF_TestReviveRecorder recorder = new LF_TestReviveRecorder();
		LF_EventBus.Subscribe(LF_OnPlayerRevived, recorder);

		// The pure rule: each refusal, in order.
		float range = config.m_fReviveRange;
		ctx.Equal(LF_EReviveRefusal.NONE, LF_Revive.Decide(true, false, true, true, 1, range), "a medic beside a downed patient may revive");
		ctx.Equal(LF_EReviveRefusal.NO_PATIENT, LF_Revive.Decide(false, false, true, true, 1, range), "no patient is refused");
		ctx.Equal(LF_EReviveRefusal.SELF, LF_Revive.Decide(true, true, true, true, 0, range), "reviving yourself is refused");
		ctx.Equal(LF_EReviveRefusal.NOT_DOWNED, LF_Revive.Decide(true, false, false, true, 1, range), "a patient who is not down is refused");
		ctx.Equal(LF_EReviveRefusal.NOT_MEDIC, LF_Revive.Decide(true, false, true, false, 1, range), "a user who is not an on-duty medic is refused");
		ctx.Equal(LF_EReviveRefusal.TOO_FAR, LF_Revive.Decide(true, false, true, true, range + 1, range), "a medic out of range is refused");

		IEntity medic = LF_TestCharacterFixture.Spawn(ctx);
		IEntity patient = LF_TestCharacterFixture.Spawn(ctx);
		if (medic && patient)
			RunRevive(ctx, config, recorder, medic, patient);

		LF_TestCharacterFixture.Delete(medic);
		LF_TestCharacterFixture.Delete(patient);
		config.m_bDownedEnabled = wasEnabled;
		LF_Downed.SetTestAllEligible(false);
		LF_Downed.ResetStatics();
		LF_Revive.ResetStatics();
		LF_EventBus.ResetStatics();
	}

	//------------------------------------------------------------------------------------------------
	protected void RunRevive(LF_TestContext ctx, LF_MedicConfig config, LF_TestReviveRecorder recorder, IEntity medic, IEntity patient)
	{
		SCR_ChimeraCharacter patientCharacter = SCR_ChimeraCharacter.Cast(patient);
		SCR_CharacterDamageManagerComponent damage = SCR_CharacterDamageManagerComponent.Cast(patient.FindComponent(SCR_CharacterDamageManagerComponent));
		if (!patientCharacter || !damage)
		{
			ctx.Fail("the patient lacks SCR_ChimeraCharacter or the character damage manager");
			return;
		}

		patient.SetOrigin(medic.GetOrigin() + "1 0 0");

		// Not down yet: nothing to revive.
		ctx.Equal(LF_EReviveRefusal.NOT_DOWNED, LF_Revive.Apply(medic, patient, true), "a standing patient is refused");
		ctx.Equal(LF_EReviveRefusal.SELF, LF_Revive.Apply(medic, medic, true), "a medic cannot revive themselves");

		damage.Kill(Instigator.CreateInstigator(null));
		ctx.True(patientCharacter.LF_IsDowned(), "the patient is downed");
		float health = damage.GetDefaultHitZone().GetHealthScaled();
		ctx.True(patientCharacter.LF_GetBleedLeft() > 0, "the patient is bleeding out");

		// A civilian cannot, and nothing changes.
		ctx.Equal(LF_EReviveRefusal.NOT_MEDIC, LF_Revive.Apply(medic, patient, false), "a civilian is refused");
		ctx.True(patientCharacter.LF_IsDowned(), "the patient is still downed after a civilian tried");
		ctx.True(patientCharacter.LF_GetBleedLeft() > 0, "the bleed-out clock still runs");
		ctx.EqualStr("", recorder.m_sLog, "no event after a refusal");

		// Out of range.
		patient.SetOrigin(medic.GetOrigin() + "20 0 0");
		ctx.Equal(LF_EReviveRefusal.TOO_FAR, LF_Revive.Apply(medic, patient, true), "a medic out of range is refused");
		ctx.True(patientCharacter.LF_IsDowned(), "the patient is still downed after an out-of-range try");

		// On duty and beside the patient.
		patient.SetOrigin(medic.GetOrigin() + "1 0 0");
		ctx.Equal(LF_EReviveRefusal.NONE, LF_Revive.Apply(medic, patient, true), "an on-duty medic revives the patient");
		ctx.False(patientCharacter.LF_IsDowned(), "the downed flag is cleared");
		ctx.EqualFloat(0, patientCharacter.LF_GetBleedLeft(), 0.01, "the bleed-out clock is cancelled");
		ctx.EqualFloat(-1, patientCharacter.LF_GetBleedSecondsLeft(), 0.01, "the replicated clock reads not downed");
		ctx.False(damage.LF_WantsUnconscious(), "the patient is no longer held unconscious");
		ctx.True(damage.GetResilienceHitZone().GetHealthScaled() >= config.m_fReviveRecoveryScaled - 0.01, "resilience is lifted so the patient wakes at once");
		ctx.EqualFloat(health, damage.GetDefaultHitZone().GetHealthScaled(), 0.01, "the patient keeps the sliver of health, above zero");
		ctx.True(damage.GetDefaultHitZone().GetHealthScaled() > 0, "the patient is alive with health above zero");
		ctx.EqualStr("npc<npc;", recorder.m_sLog, "LF_OnPlayerRevived was published once");
		ctx.Equal(1, LF_Revive.GetRevivedCount(), "one revive was counted");

		// A second revive finds nobody down.
		ctx.Equal(LF_EReviveRefusal.NOT_DOWNED, LF_Revive.Apply(medic, patient, true), "the revived patient cannot be revived again");
		ctx.Equal(1, LF_Revive.GetRevivedCount(), "the refused revive is not counted");
	}
}
