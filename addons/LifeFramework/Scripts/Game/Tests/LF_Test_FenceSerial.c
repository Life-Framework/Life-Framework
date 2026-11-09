// red-proof: Observed 2026-11-09 with `tools/cli test --no-build --only LF_Test_FenceSerial`: making LF_FenceSerials.Identify return 0 failed `investigation/serial` with `- a logged tool identifies its buyer's report (expected 1, got 0)`. Restored; green.
// tier: LOGIC
//! Fence tool serials (#272): each sale books a distinct serial to its buyer, and a tool checked at
//! the evidence locker identifies its buyer's open wanted report and nobody else's. An unknown
//! serial identifies nothing.
class LF_Test_FenceSerial : LF_Test
{
	protected static const string BUYER = "serial-buyer";
	protected static const string OTHER = "serial-other";
	protected static const string DRILL = "{4FF102B4CE9B0ED3}Prefabs/Items/Fence/LF_Drill.et";

	override string GetName()
	{
		return "investigation/serial";
	}

	override void Run(LF_TestContext ctx)
	{
		array<ref LF_FenceSerialRow> before = LF_FenceSerials.ExportAll();
		LF_FenceSerials.ApplyAll(null);

		string first = LF_FenceSerials.Issue(BUYER, DRILL);
		string second = LF_FenceSerials.Issue(OTHER, DRILL);
		ctx.True(!first.IsEmpty() && !second.IsEmpty(), "each sale gets a serial");
		ctx.True(first != second, "serials are distinct");
		ctx.True(LF_FenceSerials.Issue("", DRILL).IsEmpty(), "a sale with no buyer gets no serial");

		LF_FenceSerialRow row = LF_FenceSerials.Find(first);
		ctx.NotNull(row, "the book finds the serial");
		if (row)
			ctx.EqualStr(BUYER, row.m_sBuyerUid, "the serial leads back to its buyer");

		LF_WantedReports.Add(BUYER, LF_WantedReports.OFFENCE_ROBBERY, "", "", "", false);
		LF_WantedReports.Add(OTHER, LF_WantedReports.OFFENCE_ROBBERY, "", "", "", false);
		ctx.True(!LF_WantedReports.IsIdentified(BUYER), "the buyer's report starts unidentified");

		ctx.Equal(0, LF_FenceSerials.Identify("LF-9999", "test"), "an unknown serial identifies nothing");
		ctx.True(!LF_WantedReports.IsIdentified(BUYER), "an unknown serial leaves the report alone");

		ctx.Equal(1, LF_FenceSerials.Identify(first, "test"), "a logged tool identifies its buyer's report");
		ctx.True(LF_WantedReports.IsIdentified(BUYER), "the buyer's report is identified");
		ctx.True(!LF_WantedReports.IsIdentified(OTHER), "another buyer's report is not");

		LF_WantedReports.ClearFor(BUYER);
		LF_WantedReports.ClearFor(OTHER);
		LF_FenceSerials.ApplyAll(before);
	}
}
