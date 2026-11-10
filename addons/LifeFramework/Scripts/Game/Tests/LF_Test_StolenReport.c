// red-proof: 2026-11-10 `tools/cli test --no-build --only LF_Test_StolenReport` with the reported-plate expectation flipped to CLEAR failed `- the reported plate reads stolen (expected 1, got 2)`. Restored; green.
// tier: LOGIC
//! Stolen vehicles (#414): only the owner of a stolen car reports it; a report puts the plate on the
//! board and makes a plate check read stolen, and nothing else does; a clear or unknown plate is not
//! an arrest case. The report publishes on the bus.
class LF_StolenListener : LF_EventSubscriber
{
	int m_iCount;
	string m_sPlate;

	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnVehicleReportedStolen reported = LF_OnVehicleReportedStolen.Cast(busEvent);
		if (!reported)
			return false;

		m_iCount++;
		m_sPlate = reported.m_sPlate;
		return true;
	}
}

class LF_Test_StolenReport : LF_Test
{
	protected static const string PLATE = "ST AA 4141";
	protected static const string OTHER_PLATE = "ST BB 4242";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "stolen/report";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_StolenVehicles.ResetStatics();

		// Who may report.
		ctx.Equal(LF_EStolenReportCheck.NONE, LF_StolenVehicleRules.CheckReport(true, true, true, false), "the owner of a stolen car reports it");
		ctx.Equal(LF_EStolenReportCheck.NOT_OWNER, LF_StolenVehicleRules.CheckReport(true, true, false, false), "a stranger cannot report someone's car");
		ctx.Equal(LF_EStolenReportCheck.NOT_STOLEN, LF_StolenVehicleRules.CheckReport(true, false, true, false), "a car nobody stole is not reported");
		ctx.Equal(LF_EStolenReportCheck.ALREADY, LF_StolenVehicleRules.CheckReport(true, true, true, true), "a second report is refused");
		ctx.Equal(LF_EStolenReportCheck.NO_VEHICLE, LF_StolenVehicleRules.CheckReport(false, true, true, false), "no registered car, no report");
		ctx.True(LF_StolenVehicleRules.RefusalKey(LF_EStolenReportCheck.NOT_OWNER) != "", "every refusal has a line to print");

		// A plate before and after a report.
		LF_StolenReport found;
		LF_LicensePlateRegistry.Remember(PLATE, "stolen-test-car");
		LF_LicensePlateRegistry.Remember(OTHER_PLATE, "stolen-test-other");
		ctx.Equal(LF_EPlateStatus.CLEAR, LF_StolenVehicles.CheckPlate(PLATE, found), "a registered plate with no report is clear");
		ctx.Equal(LF_EPlateStatus.UNKNOWN, LF_StolenVehicles.CheckPlate("ZZ ZZ 0000", found), "a plate on no register is unknown");
		ctx.Equal(LF_EPlateStatus.UNKNOWN, LF_StolenVehicles.CheckPlate("", found), "an empty plate is unknown");

		LF_StolenListener listener = new LF_StolenListener();
		LF_EventBus.Subscribe(LF_OnVehicleReportedStolen, listener);
		LF_StolenReport report = LF_StolenVehicles.FileReport("stolen-test-car", PLATE, "#EL-Vehicle_S105", "stolen-test-owner", "#LF-Wanted_PlaceGrid 003-002");
		LF_EventBus.Unsubscribe(LF_OnVehicleReportedStolen, listener);
		ctx.True(report != null, "the report is stored");
		ctx.Equal(1, listener.m_iCount, "the report is published on the bus once");
		ctx.EqualStr(PLATE, listener.m_sPlate, "the event carries the plate");
		ctx.True(LF_StolenVehicles.IsReported("stolen-test-car"), "the car is reported");

		ctx.Equal(LF_EPlateStatus.STOLEN, LF_StolenVehicles.CheckPlate(PLATE, found), "the reported plate reads stolen");
		ctx.True(found != null && found.m_sOwnerUid == "stolen-test-owner", "the check returns the report");
		ctx.Equal(LF_EPlateStatus.CLEAR, LF_StolenVehicles.CheckPlate(OTHER_PLATE, found), "another car's plate stays clear");
		ctx.True(found == null, "a clear plate returns no report");

		// The board.
		array<string> entries = {};
		LF_StolenVehicles.BoardEntries(5, entries);
		ctx.Equal(1, entries.Count(), "the board carries the report");
		if (entries.Count() == 1)
		{
			ctx.True(LF_StolenVehicles.IsBoardEntry(entries[0]), "the entry is a stolen-car entry");
			ctx.True(entries[0].Contains(PLATE), "the entry carries the plate");
		}

		// Closing it (recovered, chopped) takes the plate off the board and the check.
		LF_StolenVehicles.Clear("stolen-test-car", "test");
		ctx.False(LF_StolenVehicles.IsReported("stolen-test-car"), "the report is closed");
		ctx.Equal(LF_EPlateStatus.CLEAR, LF_StolenVehicles.CheckPlate(PLATE, found), "a recovered plate is clear again");
		entries.Clear();
		LF_StolenVehicles.BoardEntries(5, entries);
		ctx.Equal(0, entries.Count(), "the board is empty again");

		LF_LicensePlateRegistry.Release(PLATE);
		LF_LicensePlateRegistry.Release(OTHER_PLATE);
		LF_StolenVehicles.ResetStatics();
	}
}
