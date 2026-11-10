// red-proof: observed 2026-11-10 with `tools/cli test --no-build --only LF_Test_ScannerIllegal`: `found > 0` changed to `found > 1` in LF_ScannerLaw.SearchOffences failed it with `- carrying a scanner in a search adds the offence (expected 1, got 0)`. Restored; green.
//------------------------------------------------------------------------------------------------
//! comms/scanner-illegal (#253): a search that finds a scanner adds the offence (once, however many),
//! a search that finds none adds nothing, and legality is config: a legal scanner is no offence.
//! The offence reaches the hidden crime record like any other crime.
// tier: LOGIC
class LF_Test_ScannerIllegal : LF_Test
{
	protected static const string UID = "scanner-illegal-a";

	override string GetName()
	{
		return "comms/scanner-illegal";
	}

	override void Run(LF_TestContext ctx)
	{
		array<string> offences = {};
		LF_ScannerLaw.SearchOffences(1, true, offences);
		ctx.Equal(1, offences.Count(), "carrying a scanner in a search adds the offence");
		if (offences.Count() == 1)
			ctx.EqualStr(LF_WantedReports.OFFENCE_SCANNER, offences[0], "the offence is the scanner offence");

		LF_ScannerLaw.SearchOffences(3, true, offences);
		ctx.Equal(1, offences.Count(), "three scanners are still one offence");

		LF_ScannerLaw.SearchOffences(0, true, offences);
		ctx.Equal(0, offences.Count(), "a search that finds no scanner adds nothing");

		LF_ScannerLaw.SearchOffences(2, false, offences);
		ctx.Equal(0, offences.Count(), "a legal scanner is no offence");

		ctx.True(LF_ServiceCallsConfig.ScannerIllegal(), "the flagship config makes the scanner illegal");

		array<ref LF_CrimeRecordRow> before = LF_CrimeRecord.ExportAll();
		LF_CrimeRecord.ApplyAll(null);
		LF_CrimeRecord.Record(LF_OnCrimeCommitted.Create(UID, LF_WantedReports.OFFENCE_SCANNER));
		ctx.Equal(1, LF_CrimeRecord.Count(UID, LF_WantedReports.OFFENCE_SCANNER), "the offence lands on the crime record");
		LF_CrimeRecord.ApplyAll(before);
	}
}
