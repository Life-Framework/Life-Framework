// red-proof: Observed 2026-11-13 with `tools/cli test --no-build --only LF_Test_ScenePieces`: dropping the golden angle in LF_ScenePieces.Scatter (`index * 0`) failed `- casing 1 does not land on another`. Restored; green.
// tier: LOGIC
//! Scene pieces (#148), with no world: which crimes leave a footprint card, that a burst's casings
//! land apart on a ring around the shooter, and how a statement's note is written.
class LF_Test_ScenePieces : LF_Test
{
	override string GetName()
	{
		return "investigation/scene-pieces";
	}

	override void Run(LF_TestContext ctx)
	{
		ctx.True(LF_EvidenceConfig.LeavesFootprint(LF_WantedReports.OFFENCE_ROBBERY), "a robbery leaves a footprint card");
		ctx.True(LF_EvidenceConfig.LeavesFootprint(LF_WantedReports.OFFENCE_BREAK_IN), "a break-in leaves a footprint card");
		ctx.False(LF_EvidenceConfig.LeavesFootprint(LF_WantedReports.OFFENCE_VEHICLE_THEFT), "a car theft does not");

		vector shooter = "100 0 100";
		array<vector> spots = {};
		for (int i = 0; i < 6; i++)
		{
			vector spot = LF_ScenePieces.Scatter(shooter, i);
			float away = vector.DistanceXZ(shooter, spot);
			ctx.True(away >= LF_ScenePieces.SCATTER_MIN - 0.01 && away <= LF_ScenePieces.SCATTER_MIN + 2 * LF_ScenePieces.SCATTER_STEP + 0.01, string.Format("casing %1 lands on the ring around the shooter (%2 m)", i, away));
			foreach (vector other : spots)
				ctx.True(vector.DistanceXZ(other, spot) > 0.2, string.Format("casing %1 does not land on another", i));

			spots.Insert(spot);
		}

		ctx.EqualStr("#AR-Jacket|#AR-Pants~#AR-Car~AB", LF_ScenePieces.NoteStatement("#AR-Jacket|#AR-Pants", "#AR-Car", "AB"), "a statement note keeps its three fields");
	}
}
