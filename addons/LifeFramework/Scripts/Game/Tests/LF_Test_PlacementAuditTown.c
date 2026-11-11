// tier: WORLD
// red-proof: 2026-11-11, LF_PlacementAudit.RunBatch with its `measured++` commented out: `tools\cli audit placement` failed "the town audit measured every listed entity (expected 732, got 0)" and "the town audit found the layer entities in the world". Restored, green.
//! The town walk behind `tools\cli audit placement` (agent-box/place-audit.mjs). That command writes
//! $profile:place-audit/in.txt (every entity the DebugWorld layers place) and runs this test alone;
//! the test measures each one with LF_PlacementAudit and writes out.txt, which the command ranks.
//! In a normal suite run there is no in.txt: the test checks the walk on the evidence locker instead,
//! which stood half inside the police station wall until this audit found it (2026-11-11).
class LF_Test_PlacementAuditTown : LF_Test
{
	static const string IN_PATH = "$profile:place-audit/in.txt";
	static const string OUT_PATH = "$profile:place-audit/out.txt";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "agent-harness/placement-audit-town";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		if (!FileIO.FileExists(IN_PATH))
		{
			IEntity locker = GetGame().GetWorld().FindEntityByName("Roadmap_Police_EvidenceLocker");
			ctx.NotNull(locker, "Roadmap_Police_EvidenceLocker is in the town");
			if (!locker)
				return;

			IEntity found = LF_PlacementAudit.FindPlaced("Roadmap_Police_EvidenceLocker", "", locker.GetOrigin());
			ctx.True(found == locker, "the audit finds a layer entity by name and position");
			LF_PlacementMeasure m = LF_PlacementAudit.Measure(locker, true);
			string flags = LF_PlacementAudit.Flags(m);
			LF_Debug.Info("PlaceAudit", "Roadmap_Police_EvidenceLocker " + LF_PlacementAudit.Format(m));
			ctx.True(!m.m_bPoint, "the locker is measured by its mesh bounds");
			ctx.True(flags.IsEmpty(), "the locker stands on the ground, upright (flags=" + flags + ")");
			ctx.True(m.m_sClips == "none", "the locker clips nothing (clip=" + m.m_sClips + ")");
			ctx.True(m.m_fWalk >= 0 && m.m_fWalk < 99, "the locker has standing room");
			return;
		}

		int expected = CountEntityLines();
		int missing;
		int measured = LF_PlacementAudit.RunBatch(IN_PATH, OUT_PATH, missing);
		LF_Debug.Info("PlaceAudit", string.Format("town audit measured %1 entities (%2 not found) into %3", measured, missing, OUT_PATH));
		ctx.Equal(expected, measured, "the town audit measured every listed entity");
		ctx.True(measured > 0 && missing < measured, "the town audit found the layer entities in the world");
	}

	//------------------------------------------------------------------------------------------------
	protected static int CountEntityLines()
	{
		FileHandle input = FileIO.OpenFile(IN_PATH, FileMode.READ);
		if (!input)
			return 0;

		int count;
		string line;
		while (input.ReadLine(line) >= 0)
		{
			line.TrimInPlace();
			if (!line.IsEmpty() && !line.StartsWith("town "))
				count++;
		}

		input.Close();
		return count;
	}
}
