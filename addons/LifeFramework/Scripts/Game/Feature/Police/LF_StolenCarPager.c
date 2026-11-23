//------------------------------------------------------------------------------------------------
//! Stolen-car pager lines (#4212). When an owner reports a car stolen (LF_StolenVehicles.Report
//! publishes LF_OnVehicleReportedStolen), every on-duty officer's pager gets one line with the
//! plate and the car, and the report stays on the hotlist (LF_StolenVehicles, the wanted board and
//! the plate check) until an officer recovers the car. Recovery (LF_OnVehicleRecovered) takes the
//! line off the pagers again.
//!
//! The pager entry is the board format "time|summary|0|place" (LF_Pager.Entry). The summary of a
//! stolen-car line is "V;<plate>;<car name key>", so the pager card renders it as its own line and
//! a recovery removes it by plate. Server state is the pagers' replicated message string; nothing
//! here is saved, like the report itself.
//------------------------------------------------------------------------------------------------
class LF_StolenCarPager
{
	static const string FEATURE = "StolenCarPager";
	static const string SUMMARY_PREFIX = "V;";
	static const string SUMMARY_SEPARATOR = ";";
	static const string FIELD_SEPARATOR = "|";

	//------------------------------------------------------------------------------------------------
	//! Pure: the summary field of a stolen-car line.
	static string Summary(string plate, string nameKey)
	{
		return SUMMARY_PREFIX + plate + SUMMARY_SEPARATOR + nameKey;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: the summary prefix every line of this plate starts with.
	static string PlatePrefix(string plate)
	{
		return SUMMARY_PREFIX + plate + SUMMARY_SEPARATOR;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: is this stored pager message a stolen-car line?
	static bool IsEntry(string entry)
	{
		array<string> fields = {};
		entry.Split(FIELD_SEPARATOR, fields, false);
		return fields.Count() >= 4 && fields[1].StartsWith(SUMMARY_PREFIX);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: one stolen-car entry as a pager line. A malformed entry reads as an empty string.
	static string EntryLine(string entry)
	{
		array<string> fields = {};
		entry.Split(FIELD_SEPARATOR, fields, false);
		if (fields.Count() < 4)
		{
			LF_Debug.Error(FEATURE, string.Format("pager entry dropped: '%1'", entry));
			return string.Empty;
		}

		// The summary is "V;plate;name key"; a plate holds no separator, the key is the tail.
		array<string> summary = {};
		fields[1].Split(SUMMARY_SEPARATOR, summary, false);
		if (summary.Count() < 3)
		{
			LF_Debug.Error(FEATURE, string.Format("pager summary dropped: '%1'", fields[1]));
			return string.Empty;
		}

		string plate = summary[1];
		if (plate.IsEmpty())
			plate = WidgetManager.Translate("#LF-Pager_NoPlate");

		string place = fields[3];
		for (int i = 4; i < fields.Count(); i++)
		{
			place += FIELD_SEPARATOR + fields[i];
		}

		if (place.IsEmpty())
			place = WidgetManager.Translate("#LF-Dispatch_UnknownPlace");
		else
			place = LF_WantedBoard.ResolvePlace(place);

		return WidgetManager.Translate("#LF-Pager_StolenLine", fields[0], plate, LF_WantedBoard.ResolveKey(summary[2]), place);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: write the stolen-car line to each pager and beep it. The seam tests drive.
	//! \return Pagers written.
	static int Page(notnull LF_StolenReport report, notnull array<LF_PagerComponent> pagers, string time)
	{
		string entry = LF_Pager.Entry(time, Summary(report.m_sPlate, report.m_sNameKey), report.m_sPlace);
		int written;
		foreach (LF_PagerComponent pager : pagers)
		{
			if (!pager)
				continue;

			pager.Receive(entry, LF_ServiceCallsConfig.PagerMessages());
			written++;
		}

		LF_Debug.Info(FEATURE, string.Format("stolen car plate='%1' paged to %2 pager(s)", report.m_sPlate, written));
		return written;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: take the plate's line off each pager (the car is recovered). No beep.
	//! \return Lines removed.
	static int Clear(string plate, notnull array<LF_PagerComponent> pagers)
	{
		int removed;
		foreach (LF_PagerComponent pager : pagers)
		{
			if (!pager)
				continue;

			removed += pager.RemoveBySummaryPrefix(PlatePrefix(plate));
		}

		LF_Debug.Info(FEATURE, string.Format("recovered plate='%1': %2 pager line(s) removed from %3 pager(s)", plate, removed, pagers.Count()));
		return removed;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the pagers carried by every on-duty officer right now.
	static void OnDutyPagers(notnull array<LF_PagerComponent> outPagers)
	{
		LF_ServiceRoute route = LF_ServiceCalls.Check("police");
		if (!route || !route.m_bKnown)
			return;

		LF_Pager.FindForPlayers(route.m_aReceivers, outPagers);
	}
}

//------------------------------------------------------------------------------------------------
//! Bus subscriber: a report pages the on-duty officers, a recovery clears the line.
class LF_StolenCarPagerSubscriber : LF_EventSubscriber
{
	static ref LF_StolenCarPagerSubscriber s_Subscriber;

	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnVehicleReportedStolen reported = LF_OnVehicleReportedStolen.Cast(busEvent);
		if (reported)
		{
			OnReported(reported);
			return true;
		}

		LF_OnVehicleRecovered recovered = LF_OnVehicleRecovered.Cast(busEvent);
		if (recovered)
		{
			OnRecovered(recovered);
			return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected void OnReported(notnull LF_OnVehicleReportedStolen reported)
	{
		if (!LF_Systems.IsEnabled(LF_Pager.SYSTEM))
		{
			LF_Debug.Info(LF_StolenCarPager.FEATURE, "stolen car not paged: the Pager system is off");
			return;
		}

		// The report holds the car's name and where it was reported; the event carries only the plate.
		LF_StolenReport report;
		LF_StolenVehicles.CheckPlate(reported.m_sPlate, report);
		if (!report)
		{
			LF_Debug.Warn(LF_StolenCarPager.FEATURE, string.Format("stolen car plate='%1' not paged: no open report", reported.m_sPlate));
			return;
		}

		array<LF_PagerComponent> pagers = {};
		LF_StolenCarPager.OnDutyPagers(pagers);
		LF_StolenCarPager.Page(report, pagers, LF_Dispatch.ClockText());
	}

	//------------------------------------------------------------------------------------------------
	protected void OnRecovered(notnull LF_OnVehicleRecovered recovered)
	{
		array<LF_PagerComponent> pagers = {};
		LF_StolenCarPager.OnDutyPagers(pagers);
		LF_StolenCarPager.Clear(recovered.m_sPlate, pagers);
	}

	//------------------------------------------------------------------------------------------------
	static void Install()
	{
		if (!Replication.IsServer())
			return;

		if (s_Subscriber)
		{
			LF_EventBus.Unsubscribe(LF_OnVehicleReportedStolen, s_Subscriber);
			LF_EventBus.Unsubscribe(LF_OnVehicleRecovered, s_Subscriber);
		}

		s_Subscriber = new LF_StolenCarPagerSubscriber();
		LF_EventBus.Subscribe(LF_OnVehicleReportedStolen, s_Subscriber);
		LF_EventBus.Subscribe(LF_OnVehicleRecovered, s_Subscriber);
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_Subscriber = null;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_StolenCarPagerSubscriber's statics (LF_StaticReset finds it by reflection).
class LF_StolenCarPagerSubscriber_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_StolenCarPagerSubscriber";
	}

	override void Apply()
	{
		LF_StolenCarPagerSubscriber.ResetStatics();
	}
}
