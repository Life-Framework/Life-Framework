//------------------------------------------------------------------------------------------------
//! Stolen vehicles (#414). A car taken without its key carries the replicated stolen flag
//! (LF_VehicleLockComponent.IsStolen, set by the hotwire, #412). The flag alone tells the police
//! nothing: the owner has to report it. A report puts the plate on the wanted board and makes a
//! plate check read "stolen"; an officer who finds the car recovers it and it goes back to the
//! owner's garage (LF_VehicleOwnership.Store).
//!
//! Server state, keyed by the vehicle identifier. It is session state on purpose: the vehicle
//! entity does not survive a restart either (an owned car left on the street is impounded at load,
//! LF_VehicleOwnership.ImpoundStreetVehicles), so a stale "stolen" poster could never be matched
//! with a car. Nothing here is saved.
//!
//! LF_StolenVehicles.CheckPlate is the plate-check seam: stops and searches (#419) call it.
//------------------------------------------------------------------------------------------------
enum LF_EStolenReportCheck
{
	NONE,
	NO_VEHICLE,
	NOT_STOLEN,
	NOT_OWNER,
	ALREADY
}

//------------------------------------------------------------------------------------------------
enum LF_EPlateStatus
{
	//! The plate is on no registered vehicle.
	UNKNOWN,
	//! Registered, and nobody reported it stolen.
	CLEAR,
	//! The owner reported the vehicle stolen and it is not recovered.
	STOLEN
}

//------------------------------------------------------------------------------------------------
class LF_StolenReport : Managed
{
	string m_sVehicleId;
	string m_sPlate;
	//! Display-name key of the vehicle.
	string m_sNameKey;
	string m_sOwnerUid;
	//! Where the report was made (a sign title key or "grid" text).
	string m_sPlace;
	int m_iSequence;
	//! The vehicle prefab's GUID (LF_WantedLook.GuidOf), for the picture on the wanted board's face.
	string m_sPrefabGuid;
}

//------------------------------------------------------------------------------------------------
//! The pure rules.
class LF_StolenVehicleRules
{
	//------------------------------------------------------------------------------------------------
	static LF_EStolenReportCheck CheckReport(bool hasVehicleId, bool stolen, bool isOwner, bool alreadyReported)
	{
		if (!hasVehicleId)
			return LF_EStolenReportCheck.NO_VEHICLE;

		if (!isOwner)
			return LF_EStolenReportCheck.NOT_OWNER;

		if (!stolen)
			return LF_EStolenReportCheck.NOT_STOLEN;

		if (alreadyReported)
			return LF_EStolenReportCheck.ALREADY;

		return LF_EStolenReportCheck.NONE;
	}

	//------------------------------------------------------------------------------------------------
	static string RefusalKey(LF_EStolenReportCheck check)
	{
		switch (check)
		{
			case LF_EStolenReportCheck.NO_VEHICLE: return "#LF-Stolen_NoVehicle";
			case LF_EStolenReportCheck.NOT_STOLEN: return "#LF-Stolen_NotStolen";
			case LF_EStolenReportCheck.NOT_OWNER: return "#LF-Stolen_NotOwner";
			case LF_EStolenReportCheck.ALREADY: return "#LF-Stolen_Already";
		}

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! The plate status as the officer reads it.
	static string StatusKey(LF_EPlateStatus status)
	{
		switch (status)
		{
			case LF_EPlateStatus.STOLEN: return "#LF-Stolen_PlateStolen";
			case LF_EPlateStatus.CLEAR: return "#LF-Stolen_PlateClear";
		}

		return "#LF-Stolen_PlateUnknown";
	}
}

//------------------------------------------------------------------------------------------------
class LF_StolenVehicles
{
	static const string FEATURE = "StolenVehicle";
	static const string ENTRY_PREFIX = "V;";
	static const string ENTRY_SEPARATOR = ";";

	protected static ref map<string, ref LF_StolenReport> s_mReports;
	protected static int s_iSequence;

	//------------------------------------------------------------------------------------------------
	protected static map<string, ref LF_StolenReport> Reports()
	{
		if (!s_mReports)
			s_mReports = new map<string, ref LF_StolenReport>();

		return s_mReports;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_mReports = null;
		s_iSequence = 0;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsReported(string vehicleId)
	{
		if (vehicleId.IsEmpty() || !s_mReports)
			return false;

		return s_mReports.Contains(vehicleId);
	}

	//------------------------------------------------------------------------------------------------
	static int Count()
	{
		if (!s_mReports)
			return 0;

		return s_mReports.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! The plate painted on a vehicle, empty when it carries none.
	static string PlateOf(IEntity vehicle)
	{
		LF_LicensePlateManagerComponent plates = LF_Component<LF_LicensePlateManagerComponent>.Find(vehicle);
		if (!plates)
			return string.Empty;

		return plates.m_Registration;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: files a report. The seam tests drive; Report is the in-world path.
	//! \return The stored report, or null when the vehicle id is empty.
	static LF_StolenReport FileReport(string vehicleId, string plate, string nameKey, string ownerUid, string place)
	{
		if (vehicleId.IsEmpty())
		{
			LF_Debug.Error(FEATURE, "report refused: no vehicle id");
			return null;
		}

		LF_StolenReport report = new LF_StolenReport();
		s_iSequence++;
		report.m_sVehicleId = vehicleId;
		report.m_sPlate = plate;
		report.m_sNameKey = nameKey;
		report.m_sOwnerUid = ownerUid;
		report.m_sPlace = place;
		report.m_iSequence = s_iSequence;
		Reports().Set(vehicleId, report);
		LF_Debug.Info(FEATURE, string.Format("reported stolen: %1 plate='%2' owner=%3 place='%4'", vehicleId, plate, ownerUid, place));
		LF_EventBus.Publish(LF_OnVehicleReportedStolen.Create(ownerUid, vehicleId, plate));
		return report;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the owner reports their vehicle stolen. Refusals log and toast the reporter.
	static LF_EStolenReportCheck Report(IEntity vehicle, IEntity reporter)
	{
		if (!Replication.IsServer())
			return LF_EStolenReportCheck.NO_VEHICLE;

		LF_VehicleLockComponent vehicleLock;
		if (vehicle)
			vehicleLock = LF_Component<LF_VehicleLockComponent>.Find(vehicle);

		string vehicleId;
		string ownerUid;
		bool stolen;
		if (vehicleLock)
		{
			vehicleId = vehicleLock.GetVehicleIdentifier();
			ownerUid = LF_VehicleOwnership.GetOwner(vehicleId);
			stolen = vehicleLock.IsStolen();
		}

		string reporterUid = LF_Utils.GetPlayerUID(reporter);
		bool isOwner = !ownerUid.IsEmpty() && ownerUid == reporterUid;
		LF_EStolenReportCheck check = LF_StolenVehicleRules.CheckReport(!vehicleId.IsEmpty(), stolen, isOwner, IsReported(vehicleId));
		if (check != LF_EStolenReportCheck.NONE)
		{
			LF_Debug.Warn(FEATURE, string.Format("report refused for %1 on %2: %3", reporterUid, vehicleId, typename.EnumToString(LF_EStolenReportCheck, check)));
			LF_Utils.NotifyEntity(reporter, LF_StolenVehicleRules.RefusalKey(check), "#LF-Stolen_Title", 4.0);
			return check;
		}

		LF_StolenReport filed = FileReport(vehicleId, PlateOf(vehicle), LF_VehicleOwnership.GetNameKey(vehicleId), ownerUid, LF_WantedReports.DescribePlace(vehicle.GetOrigin()));
		if (filed && vehicle.GetPrefabData())
			filed.m_sPrefabGuid = LF_WantedLook.GuidOf(vehicle.GetPrefabData().GetPrefabName());
		LF_Utils.NotifyEntity(reporter, "#LF-Stolen_Reported", "#LF-Stolen_Title", 4.0);
		return check;
	}

	//------------------------------------------------------------------------------------------------
	//! The plate-check seam (a stop, an ID check, a search). Reads the server's reports only; it
	//! never reads the stolen flag, so a car the owner has not reported is not "stolen" to police.
	//! \param report Receives the report when the status is STOLEN, else null.
	static LF_EPlateStatus CheckPlate(string plate, out LF_StolenReport report)
	{
		report = null;
		if (plate.IsEmpty())
			return LF_EPlateStatus.UNKNOWN;

		if (s_mReports)
		{
			foreach (string vehicleId, LF_StolenReport candidate : s_mReports)
			{
				if (candidate && candidate.m_sPlate == plate)
				{
					report = candidate;
					return LF_EPlateStatus.STOLEN;
				}
			}
		}

		if (LF_LicensePlateRegistry.IsLive(plate))
			return LF_EPlateStatus.CLEAR;

		return LF_EPlateStatus.UNKNOWN;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: an on-duty officer checks the plate of a vehicle in front of them and reads the result.
	static LF_EPlateStatus CheckAsOfficer(IEntity vehicle, IEntity officer)
	{
		if (!Replication.IsServer() || !vehicle)
			return LF_EPlateStatus.UNKNOWN;

		if (!LF_PoliceUtils.IsOnDutyOfficer(LF_PoliceUtils.GetAccountForEntity(officer)))
		{
			LF_Debug.Warn(FEATURE, "plate check refused: not an on-duty officer");
			return LF_EPlateStatus.UNKNOWN;
		}

		string plate = PlateOf(vehicle);
		LF_StolenReport report;
		LF_EPlateStatus status = CheckPlate(plate, report);
		LF_Debug.Info(FEATURE, string.Format("plate check by %1: '%2' -> %3", LF_Utils.GetPlayerUID(officer), plate, typename.EnumToString(LF_EPlateStatus, status)));
		LF_Utils.NotifyEntity(officer, LF_StolenVehicleRules.StatusKey(status), "#LF-Stolen_CheckTitle", 5.0, plate);
		return status;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: an on-duty officer recovers a reported vehicle: it returns to the owner's garage.
	//! \return True when the vehicle went to the garage.
	static bool Recover(IEntity vehicle, IEntity officer)
	{
		if (!Replication.IsServer() || !vehicle)
			return false;

		if (!LF_PoliceUtils.IsOnDutyOfficer(LF_PoliceUtils.GetAccountForEntity(officer)))
		{
			LF_Debug.Warn(FEATURE, "recover refused: not an on-duty officer");
			return false;
		}

		LF_VehicleLockComponent vehicleLock = LF_Component<LF_VehicleLockComponent>.Find(vehicle);
		string vehicleId;
		if (vehicleLock)
			vehicleId = vehicleLock.GetVehicleIdentifier();

		LF_StolenReport report;
		if (!vehicleId.IsEmpty() && s_mReports)
			report = s_mReports.Get(vehicleId);

		if (!report)
		{
			LF_Debug.Warn(FEATURE, string.Format("recover refused on %1: no stolen report", vehicleId));
			LF_Utils.NotifyEntity(officer, "#LF-Stolen_RecoverNone", "#LF-Stolen_CheckTitle", 4.0);
			return false;
		}

		if (HasOccupant(vehicle))
		{
			LF_Debug.Warn(FEATURE, string.Format("recover refused on %1: someone is inside", vehicleId));
			LF_Utils.NotifyEntity(officer, "#LF-Stolen_RecoverOccupied", "#LF-Stolen_CheckTitle", 4.0);
			return false;
		}

		bool tracked = LF_VehicleOwnership.GetVehicle(vehicleId) == vehicle;
		if (!LF_VehicleOwnership.Store(vehicleId, report.m_sOwnerUid))
		{
			LF_Debug.Error(FEATURE, string.Format("recover failed on %1: the garage did not take it", vehicleId));
			return false;
		}

		// Store deletes the vehicle it tracks; one it does not track is deleted here.
		if (!tracked)
			SCR_EntityHelper.DeleteEntityAndChildren(vehicle);

		Reports().Remove(vehicleId);
		LF_Debug.Info(FEATURE, string.Format("recovered %1 plate='%2': back in the garage of %3", vehicleId, report.m_sPlate, report.m_sOwnerUid));
		LF_EventBus.Publish(LF_OnVehicleRecovered.Create(LF_Utils.GetPlayerUID(officer), report.m_sOwnerUid, vehicleId, report.m_sPlate));
		LF_Utils.NotifyEntity(officer, "#LF-Stolen_Recovered", "#LF-Stolen_CheckTitle", 5.0, report.m_sPlate);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The vehicle left the world for another reason (chopped): the report is moot.
	static void Clear(string vehicleId, string how)
	{
		if (!IsReported(vehicleId))
			return;

		s_mReports.Remove(vehicleId);
		LF_Debug.Info(FEATURE, string.Format("report on %1 closed: %2", vehicleId, how));
		// Not every close is an event (a test, a future cause); the wanted board's face rebuilds.
		LF_WantedPosters.MarkDirty("stolen report closed");
	}

	//------------------------------------------------------------------------------------------------
	static bool HasOccupant(IEntity vehicle)
	{
		SCR_BaseCompartmentManagerComponent compartments = SCR_BaseCompartmentManagerComponent.Cast(vehicle.FindComponent(SCR_BaseCompartmentManagerComponent));
		if (!compartments)
			return false;

		array<IEntity> occupants = {};
		compartments.GetOccupants(occupants);
		return !occupants.IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the open reports, newest first.
	static void CollectNewestFirst(notnull array<LF_StolenReport> ordered)
	{
		ordered.Clear();
		if (!s_mReports)
			return;

		foreach (string vehicleId, LF_StolenReport report : s_mReports)
		{
			if (!report)
				continue;

			int at = 0;
			while (at < ordered.Count() && ordered[at].m_iSequence > report.m_iSequence)
				at++;

			ordered.InsertAt(report, at);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the reports as wanted-board entries for an officer, newest first.
	static void BoardEntries(int maxLines, notnull array<string> outEntries)
	{
		array<LF_StolenReport> ordered = {};
		CollectNewestFirst(ordered);
		foreach (LF_StolenReport ordered_report : ordered)
		{
			if (maxLines > 0 && outEntries.Count() >= maxLines)
				break;

			// The place is last: a sign title may hold the separator.
			outEntries.Insert(string.Format("V;%1;%2;%3", ordered_report.m_sPlate, ordered_report.m_sNameKey, ordered_report.m_sPlace));
		}
	}

	//------------------------------------------------------------------------------------------------
	static bool IsBoardEntry(string entry)
	{
		return entry.StartsWith(ENTRY_PREFIX);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: one entry as a board line. A malformed entry reads as an empty string.
	static string BoardLine(string entry)
	{
		array<string> fields = {};
		entry.Split(ENTRY_SEPARATOR, fields, false);
		if (fields.Count() < 4)
		{
			LF_Debug.Error(FEATURE, string.Format("stolen board entry dropped: '%1'", entry));
			return string.Empty;
		}

		string place = fields[3];
		for (int i = 4; i < fields.Count(); i++)
		{
			place += ENTRY_SEPARATOR + fields[i];
		}

		return WidgetManager.Translate("#LF-Stolen_BoardLine", fields[1], LF_WantedBoard.ResolveKey(fields[2]), LF_WantedBoard.ResolvePlace(place));
	}
}

//------------------------------------------------------------------------------------------------
//! Owner: report the car in front of them stolen. Shown to a key holder of a vehicle flagged
//! stolen; the server checks the owner and the flag.
class LF_ReportStolenAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		if (!LF_Systems.IsEnabled("Crime"))
			return false;

		LF_VehicleLockComponent vehicleLock = LF_Component<LF_VehicleLockComponent>.Find(GetOwner());
		return vehicleLock && vehicleLock.IsStolen() && vehicleLock.UserHasValidKey(user);
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (LF_Systems.Refuses("Crime", "LF_ReportStolenAction"))
			return;

		if (!Replication.IsServer())
			return;

		LF_StolenVehicles.Report(pOwnerEntity, pUserEntity);
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "#LF-Stolen_ReportAction";
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Officer: check the plate of the vehicle in front of them.
class LF_CheckPlateAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		if (!LF_Systems.IsEnabled("Crime"))
			return false;

		return LF_AccountStateBits.IsOnDutyPolice(LF_AccountReplication.GetState(user));
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (LF_Systems.Refuses("Crime", "LF_CheckPlateAction"))
			return;

		if (!Replication.IsServer())
			return;

		LF_StolenVehicles.CheckAsOfficer(pOwnerEntity, pUserEntity);
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "#LF-Stolen_CheckAction";
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Officer: recover a reported vehicle to its owner's garage.
class LF_RecoverVehicleAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		if (!LF_Systems.IsEnabled("Crime"))
			return false;

		LF_VehicleLockComponent vehicleLock = LF_Component<LF_VehicleLockComponent>.Find(GetOwner());
		return vehicleLock && vehicleLock.IsStolen() && LF_AccountStateBits.IsOnDutyPolice(LF_AccountReplication.GetState(user));
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (LF_Systems.Refuses("Crime", "LF_RecoverVehicleAction"))
			return;

		if (!Replication.IsServer())
			return;

		LF_StolenVehicles.Recover(pOwnerEntity, pUserEntity);
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "#LF-Stolen_RecoverAction";
		return true;
	}
}
