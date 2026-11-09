// red-proof: Observed 2026-11-09 with `tools/cli test --no-build --only`: with the admin check disabled in LF_IncidentGm.Answer a non-administrator got rows (`a non-administrator gets no rows`); restored; green.

// tier: LOGIC
//! incident/gm-query: the Game Master feed refuses a non-administrator with nothing, answers an
//! administrator with rows near the camera inside the window only, and caps the rows.
class LF_Test_IncidentGmQuery : LF_Test
{
	override string GetName()
	{
		return "incident/gm-query";
	}

	override void Run(LF_TestContext ctx)
	{
		array<ref LF_IncidentRecord> before = LF_IncidentLog.ExportAll();
		LF_IncidentLog.ResetStatics();

		LF_IncidentLog.Append(LF_IncidentConfig.KIND_DEATH, 100, "old-victim", "old-killer", Vector(100, 0, 100), 0, "rifle", true);
		LF_IncidentLog.Append(LF_IncidentConfig.KIND_DEATH, 4900, "victim|x", "killer", Vector(110, 0, 100), 0, "rifle", true);
		LF_IncidentLog.Append(LF_IncidentConfig.KIND_DEATH, 4950, "far-victim", "far-killer", Vector(9000, 0, 9000), 0, "rifle", false);

		array<string> refused = LF_IncidentGm.Answer(false, 7, Vector(100, 0, 100), 5000);
		ctx.Equal(0, refused.Count(), "a non-administrator gets no rows");

		array<string> rows = LF_IncidentGm.Answer(true, 7, Vector(100, 0, 100), 5000);
		ctx.Equal(1, rows.Count(), "an administrator gets the near incident inside the window, not the old or the far one");
		if (rows.Count() == 1)
		{
			array<string> parts = {};
			rows[0].Split("|", parts, false);
			ctx.Equal(11, parts.Count(), "a row has eleven fields; a pipe in a name did not split it");
			ctx.EqualStr("death", parts[1], "the row carries the kind");
			ctx.EqualStr("100", parts[2], "the row carries the age in seconds");
			ctx.EqualStr("110", parts[3], "the row carries x");
		}

		LF_IncidentLog.ResetStatics();
		for (int i = 0; i < LF_IncidentGm.MAX_ROWS + 5; i++)
			LF_IncidentLog.Append(LF_IncidentConfig.KIND_DEATH, 4000 + i, "v", "k", Vector(100, 0, 100), 0, "rifle", false);

		ctx.Equal(LF_IncidentGm.MAX_ROWS, LF_IncidentGm.Answer(true, 7, Vector(100, 0, 100), 5000).Count(), "rows are capped");

		LF_IncidentLog.ResetStatics();
		LF_IncidentLog.ApplyAll(before);
	}
}
