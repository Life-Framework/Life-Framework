// red-proof: observed 2026-11-10 with `toolsi test --no-build --only LF_Test_ReviveAnim`: the watch/begin rule ignoring refusals failed it with `- a patient no longer downed stops it (expected 0, got 1)`; restored; green.

//------------------------------------------------------------------------------------------------
//! medic/revive-anim (#512): the server starts the medic CPR only when the revive rule allows it,
//! runs one animation per medic, and stops it when the hold is cancelled, the medic walks out of
//! reach, the patient stops being downed, or the hold overruns. Broadcasts are silenced; the test
//! counts the starts and stops the server decided.
// tier: WORLD
class LF_Test_ReviveAnim : LF_Test
{
	override string GetName()
	{
		return "medic/revive-anim";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_MedicConfig config = LF_MedicConfig.Get();
		bool wasEnabled = config.m_bDownedEnabled;
		config.m_bDownedEnabled = true;
		LF_Downed.ResetStatics();
		LF_Downed.SetTestAllEligible(true);
		LF_ReviveAnim.ResetStatics();
		LF_ReviveAnim.SetSilent(true);

		// The pure step: a refusal or an overrun stops it, a healthy hold goes on.
		ctx.Equal(LF_EReviveAnimStep.KEEP, LF_ReviveAnim.Step(LF_EReviveRefusal.NONE, 1), "a healthy hold keeps animating");
		ctx.Equal(LF_EReviveAnimStep.STOP_REFUSED, LF_ReviveAnim.Step(LF_EReviveRefusal.TOO_FAR, 1), "a medic out of reach stops it");
		ctx.Equal(LF_EReviveAnimStep.STOP_REFUSED, LF_ReviveAnim.Step(LF_EReviveRefusal.NOT_DOWNED, 1), "a patient no longer downed stops it");
		ctx.Equal(LF_EReviveAnimStep.STOP_TIMEOUT, LF_ReviveAnim.Step(LF_EReviveRefusal.NONE, LF_ReviveAnim.MAX_SECONDS + 1), "an overrun stops it");

		IEntity medic = LF_TestCharacterFixture.Spawn(ctx);
		IEntity patient = LF_TestCharacterFixture.Spawn(ctx);
		if (medic && patient)
			RunAnim(ctx, medic, patient);

		LF_TestCharacterFixture.Delete(medic);
		LF_TestCharacterFixture.Delete(patient);
		config.m_bDownedEnabled = wasEnabled;
		LF_Downed.SetTestAllEligible(false);
		LF_Downed.ResetStatics();
		LF_ReviveAnim.ResetStatics();
	}

	//------------------------------------------------------------------------------------------------
	protected void RunAnim(LF_TestContext ctx, IEntity medic, IEntity patient)
	{
		SCR_CharacterDamageManagerComponent damage = SCR_CharacterDamageManagerComponent.Cast(patient.FindComponent(SCR_CharacterDamageManagerComponent));
		if (!damage)
		{
			ctx.Fail("the patient lacks the character damage manager");
			return;
		}

		patient.SetOrigin(medic.GetOrigin() + "1 0 0");

		// Nobody is down yet, so there is nothing to animate.
		ctx.False(LF_ReviveAnim.Begin(medic, patient, true), "no animation for a patient who is not down");
		ctx.Equal(0, LF_ReviveAnim.GetStartCount(), "nothing started");

		damage.Kill(Instigator.CreateInstigator(null));
		ctx.False(LF_ReviveAnim.Begin(medic, patient, false), "no animation for a civilian");
		ctx.Equal(0, LF_ReviveAnim.GetActiveCount(), "a civilian started nothing");

		// An on-duty medic beside a downed patient: one animation, however often it is asked for.
		ctx.True(LF_ReviveAnim.Begin(medic, patient, true), "the on-duty medic starts the animation");
		ctx.True(LF_ReviveAnim.Begin(medic, patient, true), "asking again keeps the one animation");
		ctx.Equal(1, LF_ReviveAnim.GetStartCount(), "one start");
		ctx.Equal(1, LF_ReviveAnim.GetActiveCount(), "one running animation");

		// A watch pass with everything still true changes nothing.
		LF_ReviveAnim.Watch();
		ctx.Equal(1, LF_ReviveAnim.GetActiveCount(), "a healthy pass keeps it running");

		// Cancelled.
		LF_ReviveAnim.End(medic, "test cancel");
		ctx.Equal(0, LF_ReviveAnim.GetActiveCount(), "a cancel stops it");
		ctx.Equal(1, LF_ReviveAnim.GetStopCount(), "one stop");
		LF_ReviveAnim.End(medic, "test cancel again");
		ctx.Equal(1, LF_ReviveAnim.GetStopCount(), "a second cancel is not a second stop");

		// The medic walks away mid-hold.
		ctx.True(LF_ReviveAnim.Begin(medic, patient, true), "started again");
		patient.SetOrigin(medic.GetOrigin() + "20 0 0");
		LF_ReviveAnim.Watch();
		ctx.Equal(0, LF_ReviveAnim.GetActiveCount(), "walking out of reach stops it");
		ctx.Equal(2, LF_ReviveAnim.GetStopCount(), "two stops");
		ctx.False(LF_ReviveAnim.Begin(medic, patient, true), "out of reach it cannot start");

		// The patient is revived by someone else mid-hold.
		patient.SetOrigin(medic.GetOrigin() + "1 0 0");
		ctx.True(LF_ReviveAnim.Begin(medic, patient, true), "started a third time");
		LF_Downed.Clear(patient);
		LF_ReviveAnim.Watch();
		ctx.Equal(0, LF_ReviveAnim.GetActiveCount(), "a patient no longer downed stops it");
		ctx.Equal(3, LF_ReviveAnim.GetStopCount(), "three stops");
	}
}
