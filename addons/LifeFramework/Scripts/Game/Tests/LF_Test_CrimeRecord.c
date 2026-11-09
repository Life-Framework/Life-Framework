// red-proof: Observed 2026-11-09 with `tools/cli test --no-build --only LF_Test_CrimeRecord`: counting `+= 2` in LF_CrimeRecord.Record failed `crime/record` with `- one robbery counts once (expected 1, got 2)`. Restored; green.
// tier: LOGIC
//! The hidden crime record (#270): a robbery counts once for its account and offence, accounts
//! and offences are separate rows, an unpaid ticket is not a crime pulled off, and a bad event
//! is refused.
class LF_Test_CrimeRecord : LF_Test
{
	protected static const string UID_A = "crime-rec-a";
	protected static const string UID_B = "crime-rec-b";

	override string GetName()
	{
		return "crime/record";
	}

	override void Run(LF_TestContext ctx)
	{
		array<ref LF_CrimeRecordRow> before = LF_CrimeRecord.ExportAll();
		LF_CrimeRecord.ApplyAll(null);

		ctx.Equal(0, LF_CrimeRecord.Total(UID_A), "a fresh account has no record");
		ctx.True(LF_CrimeRecord.Record(LF_OnCrimeCommitted.Create(UID_A, LF_WantedReports.OFFENCE_ROBBERY)), "a robbery is recorded");
		ctx.Equal(1, LF_CrimeRecord.Count(UID_A, LF_WantedReports.OFFENCE_ROBBERY), "one robbery counts once");
		ctx.Equal(1, LF_CrimeRecord.Total(UID_A), "the total is one");

		LF_CrimeRecord.Record(LF_OnCrimeCommitted.Create(UID_A, LF_WantedReports.OFFENCE_ROBBERY));
		LF_CrimeRecord.Record(LF_OnCrimeCommitted.Create(UID_A, LF_WantedReports.OFFENCE_VEHICLE_THEFT));
		ctx.Equal(2, LF_CrimeRecord.Count(UID_A, LF_WantedReports.OFFENCE_ROBBERY), "a second robbery adds one");
		ctx.Equal(1, LF_CrimeRecord.Count(UID_A, LF_WantedReports.OFFENCE_VEHICLE_THEFT), "a theft is its own kind");
		ctx.Equal(3, LF_CrimeRecord.Total(UID_A), "the total sums every kind");
		ctx.Equal(0, LF_CrimeRecord.Total(UID_B), "another account is untouched");

		ctx.True(LF_CrimeRecord.Record(LF_OnCrimeCommitted.Create(UID_B, LF_FineTickets.OFFENCE_UNPAID)), "an unpaid ticket is accepted");
		ctx.Equal(0, LF_CrimeRecord.Total(UID_B), "an unpaid ticket is not a completed crime");

		ctx.True(!LF_CrimeRecord.Record(LF_OnCrimeCommitted.Create("", LF_WantedReports.OFFENCE_ROBBERY)), "an event with no account is refused");
		ctx.True(!LF_CrimeRecord.Record(LF_OnCrimeCommitted.Create(UID_B, "")), "an event with no offence is refused");
		ctx.Equal(3, LF_CrimeRecord.Total(UID_A) + LF_CrimeRecord.Total(UID_B), "refused events add nothing");

		LF_CrimeRecord.ApplyAll(before);
	}
}
