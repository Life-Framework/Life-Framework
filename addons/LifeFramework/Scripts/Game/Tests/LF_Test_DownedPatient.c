// red-proof: 2026-11-14, `tools\cli test --no-build --only LF_Test_DownedPatient` with `idle >= delay` changed to `idle > delay` in LF_DownedPatient.Decide: `a treated patient is put back when the delay is up (expected 2, got 0)`; restored, green.
// tier: LOGIC
//! The practice patient's service rule, with no world: what each tick does with a patient, and
//! that the playtest gate keeps it inert anywhere else.
class LF_Test_DownedPatient : LF_Test
{
	override string GetName()
	{
		return "medic/downed-patient";
	}

	override void Run(LF_TestContext ctx)
	{
		float delay = 45;
		// (playtest, present, dead, downed, wasDowned, idle, delay)
		ctx.Equal(LF_EPatientStep.NONE, LF_DownedPatient.Decide(false, true, false, false, false, 0, delay), "outside a playtest mission nothing happens");
		ctx.Equal(LF_EPatientStep.NONE, LF_DownedPatient.Decide(false, false, false, false, true, 99, delay), "outside a playtest mission a lost patient is not replaced");
		ctx.Equal(LF_EPatientStep.ARM, LF_DownedPatient.Decide(true, true, false, false, false, 0, delay), "a standing patient that was never downed is downed");
		ctx.Equal(LF_EPatientStep.NONE, LF_DownedPatient.Decide(true, true, false, true, true, 0, delay), "a downed patient waits for a medic");
		ctx.Equal(LF_EPatientStep.NONE, LF_DownedPatient.Decide(true, true, false, false, true, delay - 1, delay), "a treated patient is left alone for the delay");
		ctx.Equal(LF_EPatientStep.RESET, LF_DownedPatient.Decide(true, true, false, false, true, delay, delay), "a treated patient is put back when the delay is up");
		ctx.Equal(LF_EPatientStep.REPLACE, LF_DownedPatient.Decide(true, true, true, false, true, 0, delay), "a dead patient is replaced");
		ctx.Equal(LF_EPatientStep.REPLACE, LF_DownedPatient.Decide(true, false, false, false, false, 0, delay), "a vanished patient is replaced");
		ctx.False(LF_DownedPatient.IsPatient(null), "no entity is no patient");

		LF_DownedPatient.ResetStatics();
		ctx.Equal(0, LF_DownedPatient.GetSlotCount(), "the static reset forgets every patient");
	}
}
