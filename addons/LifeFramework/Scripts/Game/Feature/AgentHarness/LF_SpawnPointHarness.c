// Harness verb `spawncheck`: every point a script puts a player body at (first-spawn and hospital beds,
// house spawns, the jail cell and release point, the role spawn points), measured against the floor
// under it: gap, over a prop mesh, room for a standing body (LF_SpawnPointAudit). One
// `[LFDebug:SpawnPoint]` line per point. `ok points=<n> worst=<metres>` when every point passes;
// `error` says how many fail and names the first three. Read-only. `bedcheck` stays the bed-only check.

//------------------------------------------------------------------------------------------------
modded class LF_AgentHarnessVerbs
{
	override bool Handle(LF_AgentCommand cmd)
	{
		if (cmd.Verb != "spawncheck")
			return super.Handle(cmd);

		array<ref LF_SpawnPointRow> rows = {};
		LF_SpawnPointAudit.Collect(rows);
		float worst;
		foreach (LF_SpawnPointRow row : rows)
		{
			LF_SpawnPointAudit.Log(row);
			worst = Math.Max(worst, Math.AbsFloat(row.m_fGap));
		}

		int failing;
		string text = LF_SpawnPointAudit.Failures(rows, failing);
		if (failing > 0)
		{
			LF_AgentHarness.Answer(cmd.Id, "error", string.Format("%1 of %2 points fail: %3", failing, rows.Count(), text));
			return true;
		}

		LF_AgentHarness.Answer(cmd.Id, "ok", string.Format("points=%1 worst=%2", rows.Count(), worst));
		return true;
	}
}
