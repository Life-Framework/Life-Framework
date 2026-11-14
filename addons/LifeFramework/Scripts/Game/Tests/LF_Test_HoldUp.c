// red-proof: Observed 2026-11-09 with `tools\cli test --no-build --only LF_Test_HoldUp`: asserting two incident rows instead of one failed `one hold-up logs one incident (expected 2, got 1)`; restored.

// tier: WORLD (needs DebugWorld's placed cell, hideout door or plaza zone; LOGIC runs on any map, #1047)
//! incident/holdup: one hold-up publishes one intent that the incident log records (actor, target,
//! position, zone); out of range, no weapon, and the cooldown are refused and record nothing.
class LF_Test_HoldUp : LF_Test
{
	override string GetName()
	{
		return "incident/holdup";
	}

	override void Run(LF_TestContext ctx)
	{
		// The pure rule.
		ctx.Equal(LF_EHoldUp.OK, LF_HoldUp.Decide(1.5, 2, true, 100, -1, 30), "armed, in range, first time is allowed");
		ctx.Equal(LF_EHoldUp.OUT_OF_RANGE, LF_HoldUp.Decide(2.5, 2, true, 100, -1, 30), "out of range is refused");
		ctx.Equal(LF_EHoldUp.NO_WEAPON, LF_HoldUp.Decide(1, 2, false, 100, -1, 30), "no weapon is refused");
		ctx.Equal(LF_EHoldUp.COOLDOWN, LF_HoldUp.Decide(1, 2, true, 110, 100, 30), "inside the cooldown is refused");
		ctx.Equal(LF_EHoldUp.OK, LF_HoldUp.Decide(1, 2, true, 130, 100, 30), "after the cooldown is allowed");

		array<ref LF_IncidentRecord> before = LF_IncidentLog.ExportAll();
		LF_IncidentLog.ResetStatics();
		LF_IncidentLog.Install();

		// One hold-up, one intent row with actor, target and where.
		ctx.Equal(LF_EHoldUp.OK, LF_HoldUp.Attempt("actor-1", "target-1", Vector(115, 0, 250), 1.2, true, 1000), "a hold-up goes through");
		ctx.Equal(1, LF_IncidentLog.Count(), "one hold-up logs one incident");
		array<ref LF_IncidentRecord> rows = LF_IncidentLog.Query(vector.Zero, -1, 0, -1, LF_IncidentConfig.KIND_HOLDUP);
		ctx.Equal(1, rows.Count(), "the row is a holdup");
		if (rows.Count() == 1)
		{
			ctx.EqualStr("actor-1", rows[0].m_sSubjectUid, "the actor is the subject");
			ctx.EqualStr("target-1", rows[0].m_sOtherUid, "the target is the other party");
			ctx.EqualStr("safe", rows[0].m_sZone, "the plaza hold-up is stamped safe");
		}

		// Refusals record nothing.
		ctx.Equal(LF_EHoldUp.OUT_OF_RANGE, LF_HoldUp.Attempt("actor-2", "target-1", Vector(0, 0, 0), 9, true, 1000), "out of range is refused");
		ctx.Equal(LF_EHoldUp.NO_WEAPON, LF_HoldUp.Attempt("actor-2", "target-1", Vector(0, 0, 0), 1, false, 1000), "no weapon is refused");
		ctx.Equal(LF_EHoldUp.COOLDOWN, LF_HoldUp.Attempt("actor-1", "target-2", Vector(115, 0, 250), 1, true, 1005), "the same actor inside the cooldown is refused");
		ctx.Equal(1, LF_IncidentLog.Count(), "refused hold-ups left the log at one row");

		LF_IncidentLog.ResetStatics();
		LF_IncidentLog.ApplyAll(before);
	}
}
