// red-proof: observed 2026-11-17 with `tools/cli test --no-build --only LF_Test_ReviveMedicState` on the unfixed rule: `- a dead medic is refused (expected 7, got 0)`, `- a downed medic is refused (expected 7, got 3)`, `- two starts broadcast once (expected 1, got 0)`; fixed; green.

//------------------------------------------------------------------------------------------------
//! medic/revive-medic-state (#2624): a downed or dead medic is refused with MEDIC_DOWN and the patient
//! stays down; the CPR start is throttled per medic (a start, a cancel and a start again inside the
//! cooldown broadcasts once, the first refusal of the window is logged once).
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_ReviveMedicState : LF_Test
{
	override string GetName()
	{
		return "medic/revive-medic-state";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_MedicConfig config = LF_MedicConfig.Get();
		bool wasEnabled = config.m_bDownedEnabled;
		config.m_bDownedEnabled = true;
		LF_Downed.ResetStatics();
		LF_ReviveAnim.ResetStatics();
		LF_ReviveAnim.SetSilent(true);

		IEntity medic = LF_TestCharacterFixture.Spawn(ctx);
		IEntity deadMedic = LF_TestCharacterFixture.Spawn(ctx);
		IEntity patient = LF_TestCharacterFixture.Spawn(ctx);
		if (medic && deadMedic && patient)
			RunStates(ctx, medic, deadMedic, patient);

		LF_TestCharacterFixture.Delete(medic);
		LF_TestCharacterFixture.Delete(deadMedic);
		LF_TestCharacterFixture.Delete(patient);
		config.m_bDownedEnabled = wasEnabled;
		LF_Downed.SetTestAllEligible(false);
		LF_Downed.ResetStatics();
		LF_ReviveAnim.ResetStatics();
	}

	//------------------------------------------------------------------------------------------------
	protected void Kill(IEntity character)
	{
		SCR_CharacterDamageManagerComponent damage = SCR_CharacterDamageManagerComponent.Cast(character.FindComponent(SCR_CharacterDamageManagerComponent));
		if (damage)
			damage.Kill(Instigator.CreateInstigator(null));
	}

	//------------------------------------------------------------------------------------------------
	protected void RunStates(LF_TestContext ctx, IEntity medic, IEntity deadMedic, IEntity patient)
	{
		medic.SetOrigin(patient.GetOrigin() + "1 0 0");
		deadMedic.SetOrigin(patient.GetOrigin() + "-1 0 0");

		// Not eligible for the downed state: a lethal hit kills for real.
		Kill(deadMedic);
		LF_Downed.SetTestAllEligible(true);
		Kill(patient);
		ctx.True(LF_Downed.IsDowned(patient), "the patient is downed");
		ctx.Equal(LF_EReviveRefusal.NONE, LF_Revive.Check(medic, patient, true), "a healthy on-duty medic may revive");

		// A dead medic.
		ctx.Equal(LF_EReviveRefusal.MEDIC_DOWN, LF_Revive.Check(deadMedic, patient, true), "a dead medic is refused");
		ctx.Equal(LF_EReviveRefusal.MEDIC_DOWN, LF_Revive.Apply(deadMedic, patient, true), "a dead medic cannot revive");
		ctx.True(LF_Downed.IsDowned(patient), "the patient is still down after a dead medic tried");
		ctx.False(LF_ReviveAnim.Begin(deadMedic, patient, true), "no CPR for a dead medic");

		// A downed medic.
		Kill(medic);
		ctx.True(LF_Downed.IsDowned(medic), "the medic is downed");
		ctx.Equal(LF_EReviveRefusal.MEDIC_DOWN, LF_Revive.Check(medic, patient, true), "a downed medic is refused");
		ctx.Equal(LF_EReviveRefusal.MEDIC_DOWN, LF_Revive.Apply(medic, patient, true), "a downed medic cannot revive");
		ctx.True(LF_Downed.IsDowned(patient), "the patient is still down after a downed medic tried");
		ctx.False(LF_ReviveAnim.Begin(medic, patient, true), "no CPR for a downed medic");
		ctx.Equal(0, LF_ReviveAnim.GetStartCount(), "nothing started for either");

		// The CPR start throttle needs a healthy medic: bring the first one back.
		ctx.True(LF_Downed.Clear(medic), "the downed medic is cleared");
		ctx.Equal(LF_EReviveRefusal.NONE, LF_Revive.Check(medic, patient, true), "a cleared medic may revive again");
		RunThrottle(ctx, medic, patient);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunThrottle(LF_TestContext ctx, IEntity medic, IEntity patient)
	{
		ctx.True(LF_ReviveAnim.Begin(medic, patient, true), "the first start runs");
		ctx.True(LF_ReviveAnim.Begin(medic, patient, true), "asking again keeps the one animation");
		ctx.Equal(1, LF_ReviveAnim.GetStartCount(), "two starts broadcast once");

		// Cancel and start again at once: the start is refused, the refusal logged once per window.
		LF_ReviveAnim.End(medic, "test cancel");
		ctx.False(LF_ReviveAnim.Begin(medic, patient, true), "a start straight after a cancel is throttled");
		ctx.False(LF_ReviveAnim.Begin(medic, patient, true), "a second throttled start");
		ctx.Equal(1, LF_ReviveAnim.GetStartCount(), "still one start");
		ctx.Equal(0, LF_ReviveAnim.GetActiveCount(), "no animation running");
		ctx.Equal(2, LF_ReviveAnim.GetThrottledCount(), "two starts throttled");
		ctx.Equal(1, LF_ReviveAnim.GetThrottleLogCount(), "one log line for the window");

		// Cooldown off: the same medic starts at once.
		LF_ReviveAnim.SetCooldown(0);
		ctx.True(LF_ReviveAnim.Begin(medic, patient, true), "no cooldown, no throttle");
		ctx.Equal(2, LF_ReviveAnim.GetStartCount(), "a second start");
		LF_ReviveAnim.End(medic, "test cancel");
	}
}
