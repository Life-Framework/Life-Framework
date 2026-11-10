// red-proof: observed 2026-11-10 with `toolsi test --no-build --only LF_Test_NewLifeRule`: asserting the second look also reminds failed `- the reminder fires once`; restored, green.

// tier: LOGIC
//! medic/nlr: a real death gives a wristband on the wake; a return near the scene inside the
//! window reminds once and logs one incident row; outside the window nothing; a revive (no
//! recorded death) gives no wristband.
class LF_Test_NewLifeRule : LF_Test
{
	override string GetName()
	{
		return "medic/nlr";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_NewLifeRule.Reset();
		vector scene = Vector(100, 0, 100);
		float radius = 75;
		string uid = "test-nlr-1";

		ctx.True(LF_NewLifeRule.Wear(uid, 1000, 15) == null, "no recorded death (a revive) gives no wristband");

		LF_NewLifeRule.RecordDeath(uid, scene);
		LF_NewLifeWristband band = LF_NewLifeRule.Wear(uid, 1000, 15);
		ctx.NotNull(band, "a recorded death gives a wristband on the wake");
		if (!band)
			return;

		ctx.Equal(1900, band.m_iExpiresAt, "the wristband runs for the configured minutes");
		ctx.True(LF_NewLifeRule.Wear(uid, 1010, 15) == null, "one death gives one wristband");

		ctx.False(LF_NewLifeRule.Observe(band, Vector(110, 0, 100), 1005, radius), "waking inside the radius is not a return");
		ctx.False(LF_NewLifeRule.Observe(band, Vector(400, 0, 400), 1100, radius), "walking away is not a return");
		ctx.True(LF_NewLifeRule.Observe(band, Vector(130, 0, 100), 1200, radius), "coming back inside the radius and the window reminds");
		ctx.False(LF_NewLifeRule.Observe(band, Vector(130, 0, 100), 1210, radius), "the reminder fires once");

		// Outside the window.
		LF_NewLifeRule.RecordDeath(uid, scene);
		LF_NewLifeWristband late = LF_NewLifeRule.Wear(uid, 5000, 15);
		LF_NewLifeRule.Observe(late, Vector(400, 0, 400), 5100, radius);
		ctx.False(LF_NewLifeRule.Observe(late, Vector(100, 0, 100), 5900, radius), "a return after the window logs nothing");

		// The incident log records the published return.
		array<ref LF_IncidentRecord> before = LF_IncidentLog.ExportAll();
		LF_IncidentLog.ResetStatics();
		LF_IncidentLog.Install();
		LF_EventBus.Publish(LF_OnNewLifeReturn.Create(uid, Vector(130, 0, 100), scene, 200));
		array<ref LF_IncidentRecord> rows = LF_IncidentLog.Query(vector.Zero, -1, 0, -1, LF_IncidentConfig.KIND_NLR);
		ctx.Equal(1, rows.Count(), "one return writes one incident row");
		if (rows.Count() == 1)
			ctx.EqualStr(uid, rows[0].m_sSubjectUid, "the returner is the subject");

		LF_IncidentLog.ResetStatics();
		LF_IncidentLog.ApplyAll(before);
		LF_NewLifeRule.Reset();
	}
}
