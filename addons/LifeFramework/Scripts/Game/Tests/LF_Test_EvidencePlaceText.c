// red-proof: 2026-11-27 (pre-RC review part 4): on the unfixed code `--only LF_Test_EvidencePlaceText` failed "the locker card never shows the stored place join: #0 backroom Fuel Station^Saint Philippe"; with LF_WantedBoard.ResolvePlace in DescribeAll and LF_EvidenceBagComponent.ReadLine, green.
//! Evidence cards read the place the way every board does (pre-RC review part 4). #5156 stores a crime place
//! inside a town as "<site>^<town>" (LF_WantedReports.PLACE_JOIN); the locker's read card printed the stored
//! string as is, so an officer read "Fuel Station^Saint Philippe" (or the raw keys) instead of
//! "Fuel Station, Saint Philippe".
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_EvidencePlaceText : LF_Test
{
	override string GetName()
	{
		return "investigation/evidence-place-text";
	}

	override void Run(LF_TestContext ctx)
	{
		string place = "Fuel Station" + LF_WantedReports.PLACE_JOIN + "Saint Philippe";

		array<ref LF_EvidenceLockerRow> before = LF_EvidenceLocker.ExportAll();
		LF_EvidenceLocker.ApplyAll(null);
		LF_EvidenceLocker.LogSeizure("{AAAAAAAAAAAAAAAA}Prefabs/Test/Seized.et", place, "place-text-officer");
		array<string> lines = {};
		LF_EvidenceLocker.DescribeAll(lines);
		LF_EvidenceLocker.ApplyAll(before);
		LF_EvidenceLockerFill.ResetStatics();

		ctx.Equal(1, lines.Count(), "the locker card has the one logged row");
		if (lines.IsEmpty())
			return;

		ctx.False(lines[0].Contains(LF_WantedReports.PLACE_JOIN), "the locker card never shows the stored place join: " + lines[0]);
		ctx.True(lines[0].Contains("Fuel Station") && lines[0].Contains("Saint Philippe"), "the locker card names the site and the town: " + lines[0]);

		// The bag's own read line goes the same way.
		string bagLine = LF_EvidenceBagComponent.ReadLine(7, "Shoplifting", place, 4);
		ctx.False(bagLine.Contains(LF_WantedReports.PLACE_JOIN), "the bag read line never shows the stored place join: " + bagLine);
		ctx.True(bagLine.Contains("Fuel Station") && bagLine.Contains("Saint Philippe"), "the bag read line names the site and the town: " + bagLine);
	}
}
