// red-proof: Observed 2026-11-09 with `toolscli test --no-build --only LF_Test_AimIntent`: asserting three rows instead of two failed `9 s of sustained aim writes two rows, one per window (expected 3, got 2)`; restored.

// tier: LOGIC
//! incident/aim-throttle: sustained aim at one player logs one intent row per window; a gap or a
//! glance restarts the streak; the cone and range are pure checks.
class LF_Test_AimIntent : LF_Test
{
	override string GetName()
	{
		return "incident/aim-throttle";
	}

	override void Run(LF_TestContext ctx)
	{
		// The pure cone and range check: aiming down +Z at a target 10 m ahead.
		vector from = "0 1.5 0";
		ctx.True(LF_AimIntent.IsAimedAt(from, "0 0 1", "0 1.5 10", 15, 6), "dead ahead in range is aimed at");
		ctx.True(!LF_AimIntent.IsAimedAt(from, "0 0 1", "0 1.5 20", 15, 6), "out of range is not aimed at");
		ctx.True(!LF_AimIntent.IsAimedAt(from, "0 0 1", "5 1.5 10", 15, 6), "26 degrees off is not aimed at");
		ctx.True(!LF_AimIntent.IsAimedAt(from, "0 0 1", "0 1.5 -10", 15, 6), "behind is not aimed at");

		LF_AimIntent.ResetStatics();

		// Sustained aim: 1 s ticks, 3 s window, one row at the window and one per further window.
		int fired = 0;
		for (int t = 0; t <= 8; t++)
		{
			if (LF_AimIntent.Offer("1>2", t, 3, 2.5))
				fired++;
		}
		ctx.Equal(2, fired, "9 s of sustained aim writes two rows, one per window");

		// A gap restarts the streak.
		LF_AimIntent.ResetStatics();
		fired = 0;
		if (LF_AimIntent.Offer("1>2", 0, 3, 2.5))
			fired++;
		if (LF_AimIntent.Offer("1>2", 1, 3, 2.5))
			fired++;
		if (LF_AimIntent.Offer("1>2", 10, 3, 2.5))
			fired++;
		if (LF_AimIntent.Offer("1>2", 11, 3, 2.5))
			fired++;
		ctx.Equal(0, fired, "aim that stopped and resumed has not held the window");

		// Pairs are independent.
		LF_AimIntent.ResetStatics();
		LF_AimIntent.Offer("1>2", 0, 3, 2.5);
		LF_AimIntent.Offer("1>2", 2, 3, 2.5);
		ctx.True(!LF_AimIntent.Offer("1>3", 3, 3, 2.5), "a different target starts its own streak");
		ctx.True(LF_AimIntent.Offer("1>2", 3, 3, 2.5), "the first pair completes its window");

		// The log records the published intent.
		array<ref LF_IncidentRecord> before = LF_IncidentLog.ExportAll();
		LF_IncidentLog.ResetStatics();
		LF_IncidentLog.Install();
		LF_EventBus.Publish(LF_OnAimedAt.Create("actor-1", "target-1", Vector(115, 0, 250)));
		array<ref LF_IncidentRecord> rows = LF_IncidentLog.Query(vector.Zero, -1, 0, -1, LF_IncidentConfig.KIND_AIM);
		ctx.Equal(1, rows.Count(), "one aim intent logs one row");
		if (rows.Count() == 1)
		{
			ctx.EqualStr("actor-1", rows[0].m_sSubjectUid, "the aimer is the subject");
			ctx.EqualStr("target-1", rows[0].m_sOtherUid, "the aimed-at player is the other party");
		}

		LF_IncidentLog.ResetStatics();
		LF_IncidentLog.ApplyAll(before);
	}
}
