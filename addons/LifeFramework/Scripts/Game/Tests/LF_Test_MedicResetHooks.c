// red-proof: 2026-11-17 `tools/cli test --no-build --only LF_Test_MedicResetHooks` on the unfixed code failed all three
// assertions ("LF_TreatmentBills registers its world-end reset hook", LF_ReviveAnim, LF_HospitalRespawn); green with the hooks.

// tier: LOGIC
//! The medic and hospital statics are dropped at world end (docs/reviews/2026-11-17-life.md #1).
//! LF_TreatmentBills, LF_ReviveAnim, LF_HospitalRespawn (and LF_NewLifeRule through it) had reset
//! functions that only tests called, so a Workbench play restart or a mission change kept the last
//! world's bills, pending wakes, wristbands and a stuck debt-sweep flag.
class LF_Test_MedicResetHooks : LF_Test
{
	override string GetName()
	{
		return "medic/reset-hooks";
	}

	override void Run(LF_TestContext ctx)
	{
		array<string> done = LF_StaticReset.Registered();
		array<string> expect = {"LF_TreatmentBills", "LF_ReviveAnim", "LF_HospitalRespawn"};
		foreach (string cls : expect)
		{
			ctx.True(done.Contains(cls), cls + " registers its world-end reset hook");
		}
	}
}
