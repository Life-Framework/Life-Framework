//------------------------------------------------------------------------------------------------
//! One dispatch: what the alarm company radios to the station. It carries the crime report's
//! description snapshot (clothing, vehicle, place) and never a name. Session-local: the board's
//! dispatch log is the last few calls since the server started; the poster (LF_WantedReports) is
//! the persisted record.
class LF_DispatchRecord : Managed
{
	string m_sOffence;
	string m_sClothing;
	string m_sVehicle;
	string m_sPlace;
	//! In-game clock at the call, "HH:MM".
	string m_sTime;
	//! On-duty officers when the crime started.
	int m_iOfficers;
	//! Nobody was on duty: the alarm at the site is the loud one and nobody is radioed.
	bool m_bLoud;
	//! Number of officers the radio call went to.
	int m_iRadioed;
	int m_iSequence;
}

//------------------------------------------------------------------------------------------------
//! The one entry point every crime reports through (M1-20, #105). On the server, Report:
//!   1. records the crime report with its description snapshot (LF_WantedReports),
//!   2. sounds the alarm at the site: the normal alarm when an officer is on duty, the loud,
//!      longer alarm with a flashing light when nobody is (decision "Major crimes are always
//!      allowed; no police means a louder alarm"),
//!   3. radios every on-duty officer: a radio chirp from their own body, then a short dispatch
//!      line with the place and the description (decision "Radios, payphones, and pagers"),
//!   4. appends the call to the dispatch log the wanted board shows to on-duty police.
//! The crime itself (wanted, cash, cooldown) stays with the caller. No crime refuses here.
class LF_Dispatch
{
	static const string FEATURE = "Dispatch";
	static const string FIELD_SEPARATOR = "|";
	static const string RADIO_KEY = "#LF-Dispatch_Radio";
	static const string RADIO_CALL_KEY = "#LF-Dispatch_RadioCall";

	protected static ref array<ref LF_DispatchRecord> s_aLog;
	protected static int s_iSequence;
	//! Alarms this server spawned and has not deleted yet, by alarm number (the call queue
	//! cannot carry an EntityID, so the timer carries the number).
	protected static ref map<int, EntityID> s_mAlarms;
	protected static int s_iAlarmNumber;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_aLog = null;
		s_iSequence = 0;
		s_mAlarms = null;
		s_iAlarmNumber = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: report a crime. site is where the alarm sounds (the till, the vault door, the
	//! cabinet); a zero vector sounds no alarm (a murder in the street, the logic-tier seam).
	//! \param officersOnDuty On-duty police when the crime started; negative counts them now.
	//! \return The dispatch record, or null when the crime report was refused (no player uid).
	static LF_DispatchRecord Report(string playerUid, IEntity suspect, string offence, vector site, int officersOnDuty = -1)
	{
		LF_WantedReportRecord report = LF_WantedReports.Report(playerUid, suspect, offence);
		if (!report)
		{
			LF_Debug.Error(FEATURE, string.Format("dispatch dropped: no crime report for %1 (%2)", playerUid, offence));
			return null;
		}

		int officers = officersOnDuty;
		if (officers < 0)
			officers = LF_AccountReplication.CountPoliceOnDuty();

		LF_DispatchRecord record = Record(offence, report.m_sClothing, report.m_sVehicle, report.m_sPlace, officers);
		record.m_sTime = ClockText();

		if (site == vector.Zero)
			LF_Debug.Info(FEATURE, string.Format("dispatch #%1: no alarm site, no alarm sounded", record.m_iSequence));
		else
			StartAlarm(site, record.m_bLoud);

		if (officers > 0)
			record.m_iRadioed = RadioOfficers(record);

		LF_Debug.Info(FEATURE, string.Format("dispatch #%1 %2 at '%3': officers=%4 alarm=%5 radioed=%6 clothing='%7' vehicle='%8'", record.m_iSequence, offence, record.m_sPlace, officers, AlarmLevel(record.m_bLoud), record.m_iRadioed, record.m_sClothing, record.m_sVehicle));
		return record;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a witness phoned it in (payphone, #247). Logs the call and radios every on-duty
	//! officer with the place; no crime report, no suspect description, no alarm.
	//! param summary What happened: a localization key or text.
	//! eturn The dispatch record, or null off the server.
	static LF_DispatchRecord RelayCall(string summary, string place, vector site, int officersOnDuty)
	{
		if (!Replication.IsServer())
			return null;

		LF_DispatchRecord record = Record(summary, string.Empty, string.Empty, place, officersOnDuty);
		record.m_sTime = ClockText();
		if (officersOnDuty > 0)
			record.m_iRadioed = RadioOfficers(record);

		LF_Debug.Info(FEATURE, string.Format("dispatch #%1 phoned in %2 at '%3': officers=%4 radioed=%5", record.m_iSequence, summary, record.m_sPlace, officersOnDuty, record.m_iRadioed));
		return record;
	}

	//------------------------------------------------------------------------------------------------
	//! Logs a call with a ready description and decides the alarm level. The seam tests drive.
	static LF_DispatchRecord Record(string offence, string clothing, string vehicle, string place, int officersOnDuty)
	{
		if (!s_aLog)
			s_aLog = {};

		LF_DispatchRecord record = new LF_DispatchRecord();
		s_iSequence++;
		record.m_iSequence = s_iSequence;
		record.m_sOffence = offence;
		record.m_sClothing = clothing;
		record.m_sVehicle = vehicle;
		record.m_sPlace = place;
		record.m_iOfficers = officersOnDuty;
		record.m_bLoud = IsLoud(officersOnDuty);
		s_aLog.Insert(record);

		// Keep the log short: the board shows the newest few, and nothing older is read.
		int keep = Math.Max(LF_PoliceConfig.DispatchLogLines(), 1);
		while (s_aLog.Count() > keep)
			s_aLog.RemoveOrdered(0);

		return record;
	}

	//------------------------------------------------------------------------------------------------
	//! Nobody on duty: the loud alarm.
	static bool IsLoud(int officersOnDuty)
	{
		return officersOnDuty <= 0;
	}

	//------------------------------------------------------------------------------------------------
	static string AlarmLevel(bool loud)
	{
		if (loud)
			return "loud";

		return "normal";
	}

	//------------------------------------------------------------------------------------------------
	//! Calls since the server started (not capped by the log length).
	static int CallCount()
	{
		return s_iSequence;
	}

	//------------------------------------------------------------------------------------------------
	//! The newest call, or null.
	static LF_DispatchRecord Last()
	{
		if (!s_aLog || s_aLog.IsEmpty())
			return null;

		return s_aLog[s_aLog.Count() - 1];
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the dispatch log as board entries, newest first, at most maxLines. Each entry is
	//! "time|offence|loud|officers|place", resolved into words on the reading client.
	static void BuildBoardEntries(int maxLines, notnull array<string> outEntries)
	{
		outEntries.Clear();
		if (!s_aLog)
			return;

		for (int i = s_aLog.Count() - 1; i >= 0; i--)
		{
			if (maxLines > 0 && outEntries.Count() >= maxLines)
				break;

			LF_DispatchRecord record = s_aLog[i];
			if (!record)
				continue;

			int loud = 0;
			if (record.m_bLoud)
				loud = 1;

			outEntries.Insert(string.Format("%1|%2|%3|%4|%5", record.m_sTime, record.m_sOffence, loud, record.m_iOfficers, record.m_sPlace));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Client: one board entry as a line. "21:14  Robbery at Fuel Station: 1 unit radioed" or
	//! "... : nobody on duty, loud alarm". A malformed entry reads as an empty string.
	static string BoardLine(string entry)
	{
		array<string> fields = {};
		entry.Split(FIELD_SEPARATOR, fields, false);
		if (fields.Count() < 5)
		{
			LF_Debug.Error(FEATURE, string.Format("dispatch board entry dropped: '%1'", entry));
			return string.Empty;
		}

		// A place is the last field; a sign title holding the separator stays whole.
		string place = fields[4];
		for (int i = 5; i < fields.Count(); i++)
		{
			place += FIELD_SEPARATOR + fields[i];
		}

		string outcome;
		if (fields[2] == "1")
			outcome = WidgetManager.Translate("#LF-Dispatch_BoardLoud");
		else
			outcome = WidgetManager.Translate("#LF-Dispatch_BoardRadioed", fields[3]);

		return WidgetManager.Translate("#LF-Dispatch_BoardLine", fields[0], LF_WantedBoard.ResolveKey(fields[1]), PlaceText(place), outcome);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the radio message an officer hears. "Dispatch, all units: Robbery at Fuel Station.
	//! Suspect wearing ...; on foot." The description is the snapshot; there is no name field.
	static string RadioLine(string offence, string clothing, string vehicle, string place)
	{
		// No clothing and no vehicle: a witness phoned it in, there is no suspect to describe.
		if (clothing.IsEmpty() && vehicle.IsEmpty())
			return WidgetManager.Translate(RADIO_CALL_KEY, LF_WantedBoard.ResolveKey(offence), PlaceText(place));

		string description = LF_WantedBoard.PosterDescription(clothing, vehicle, string.Empty);
		return WidgetManager.Translate(RADIO_KEY, LF_WantedBoard.ResolveKey(offence), PlaceText(place), description);
	}

	//------------------------------------------------------------------------------------------------
	protected static string PlaceText(string place)
	{
		if (place.IsEmpty())
			return WidgetManager.Translate("#LF-Dispatch_UnknownPlace");

		return LF_WantedBoard.ResolvePlace(place);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: spawn the alarm prefab for this level at the site and schedule its removal. An
	//! older alarm still sounding at the same site is replaced.
	//! \return The alarm entity, or null when the prefab does not spawn (logged).
	static IEntity StartAlarm(vector site, bool loud)
	{
		if (!Replication.IsServer())
			return null;

		ResourceName prefab = LF_PoliceConfig.AlarmPrefab(loud);
		if (prefab.IsEmpty())
		{
			LF_Debug.Error(FEATURE, string.Format("no %1 alarm prefab configured; the site stays quiet", AlarmLevel(loud)));
			return null;
		}

		StopAlarmsNear(site, 3);
		IEntity alarm = LF_Utils.SpawnEntityPrefab(prefab, site);
		if (!alarm)
		{
			LF_Debug.Error(FEATURE, string.Format("%1 alarm did not spawn: %2", AlarmLevel(loud), prefab));
			return null;
		}

		if (!s_mAlarms)
			s_mAlarms = new map<int, EntityID>();

		s_iAlarmNumber++;
		s_mAlarms.Set(s_iAlarmNumber, alarm.GetID());
		float seconds = LF_PoliceConfig.AlarmSeconds(loud);
		GetGame().GetCallqueue().CallLater(ExpireAlarm, Math.Max(seconds, 1) * 1000, false, s_iAlarmNumber);
		LF_Debug.Info(FEATURE, string.Format("%1 alarm sounding at %2 for %3 s", AlarmLevel(loud), site, seconds));
		return alarm;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the alarm time ran out. An alarm already stopped is fine.
	protected static void ExpireAlarm(int number)
	{
		if (!s_mAlarms || !s_mAlarms.Contains(number))
			return;

		StopAlarm(s_mAlarms.Get(number));
	}

	//------------------------------------------------------------------------------------------------
	//! Server: stop an alarm (its time ran out, or a newer alarm replaced it). Gone already is fine.
	static void StopAlarm(EntityID alarmId)
	{
		if (s_mAlarms)
		{
			int found = -1;
			foreach (int number, EntityID id : s_mAlarms)
			{
				if (id == alarmId)
					found = number;
			}

			if (found >= 0)
				s_mAlarms.Remove(found);
		}

		if (!GetGame() || !GetGame().GetWorld())
			return;

		IEntity alarm = GetGame().GetWorld().FindEntityByID(alarmId);
		if (!alarm)
			return;

		LF_Debug.Info(FEATURE, string.Format("alarm at %1 stopped", alarm.GetOrigin()));
		SCR_EntityHelper.DeleteEntityAndChildren(alarm);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: alarms this server is sounding now.
	static int ActiveAlarmCount()
	{
		if (!s_mAlarms || !GetGame() || !GetGame().GetWorld())
			return 0;

		int count;
		foreach (int number, EntityID id : s_mAlarms)
		{
			if (GetGame().GetWorld().FindEntityByID(id))
				count++;
		}

		return count;
	}

	//------------------------------------------------------------------------------------------------
	protected static void StopAlarmsNear(vector site, float radius)
	{
		if (!s_mAlarms || !GetGame() || !GetGame().GetWorld())
			return;

		array<EntityID> ids = {};
		foreach (int number, EntityID id : s_mAlarms)
		{
			ids.Insert(id);
		}

		foreach (EntityID id : ids)
		{
			IEntity alarm = GetGame().GetWorld().FindEntityByID(id);
			if (!alarm)
			{
				StopAlarm(id);
				continue;
			}

			if (vector.Distance(alarm.GetOrigin(), site) <= radius)
			{
				StopAlarm(id);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server: radio the call to every connected on-duty officer.
	//! \return Officers reached.
	protected static int RadioOfficers(LF_DispatchRecord record)
	{
		if (!GetGame() || !GetGame().GetPlayerManager())
			return 0;

		LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();
		if (!accounts)
		{
			LF_Debug.Error(FEATURE, "dispatch not radioed: no account manager");
			return 0;
		}

		int reached;
		array<int> playerIds = {};
		GetGame().GetPlayerManager().GetPlayers(playerIds);
		foreach (int playerId : playerIds)
		{
			if (!LF_PoliceUtils.IsOnDutyOfficer(accounts.GetAccount(LF_Utils.GetPlayerUID(playerId))))
				continue;

			SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
			if (!controller)
			{
				LF_Debug.Warn(FEATURE, string.Format("dispatch #%1 not radioed to player %2: no controller", record.m_iSequence, playerId));
				continue;
			}

			controller.LF_DeliverDispatch(record.m_sOffence, record.m_sClothing, record.m_sVehicle, record.m_sPlace);
			reached++;
		}

		return reached;
	}

	//------------------------------------------------------------------------------------------------
	//! The world's clock as "HH:MM"; the real clock when the world has none.
	static string ClockText()
	{
		int hours, minutes, seconds;
		ChimeraWorld world;
		if (GetGame())
			world = ChimeraWorld.CastFrom(GetGame().GetWorld());

		TimeAndWeatherManagerEntity clock;
		if (world)
			clock = world.GetTimeAndWeatherManager();

		if (clock)
			clock.GetHoursMinutesSeconds(hours, minutes, seconds);
		else
			System.GetHourMinuteSecond(hours, minutes, seconds);

		return string.Format("%1:%2", hours.ToString(2), minutes.ToString(2));
	}
}

//------------------------------------------------------------------------------------------------
//! The radio call on the officer's own controller: an Owner RPC reaches that client only.
modded class SCR_PlayerController
{
	//! Dispatches this client heard, and the newest as "offence@place", for the harness state.
	protected static int s_iLF_DispatchCount;
	protected static string s_sLF_LastDispatch;

	//------------------------------------------------------------------------------------------------
	static int LF_GetDispatchCount()
	{
		return s_iLF_DispatchCount;
	}

	//------------------------------------------------------------------------------------------------
	static string LF_GetLastDispatch()
	{
		return s_sLF_LastDispatch;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: radio a dispatch to this controller's player.
	void LF_DeliverDispatch(string offence, string clothing, string vehicle, string place)
	{
		if (GetGame().GetPlayerController() == this)
		{
			RpcDo_LF_Dispatch(offence, clothing, vehicle, place);
			return;
		}

		Rpc(RpcDo_LF_Dispatch, offence, clothing, vehicle, place);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the radio chirps from the officer's body first; the written line follows a beat later.
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_LF_Dispatch(string offence, string clothing, string vehicle, string place)
	{
		s_iLF_DispatchCount++;
		s_sLF_LastDispatch = offence + "@" + place;
		LF_Debug.Info(LF_Dispatch.FEATURE, string.Format("dispatch received #%1: %2 at '%3' clothing='%4' vehicle='%5'", s_iLF_DispatchCount, offence, place, clothing, vehicle));

		IEntity body = GetControlledEntity();
		ResourceName project = LF_PoliceConfig.RadioSoundProject();
		string soundEvent = LF_PoliceConfig.RadioSoundEvent();
		if (body && !project.IsEmpty() && !soundEvent.IsEmpty())
		{
			SCR_AudioSourceConfiguration config = new SCR_AudioSourceConfiguration();
			config.m_sSoundProject = project;
			config.m_sSoundEventName = soundEvent;
			config.m_vOffset = "0 1.2 0";
			SCR_SoundManagerModule.CreateAndPlayAudioSource(body, config);
		}
		else
		{
			LF_Debug.Warn(LF_Dispatch.FEATURE, "radio chirp skipped: no body or no radio sound configured");
		}

		GetGame().GetCallqueue().CallLater(LF_ShowDispatchLine, 700, false, offence, clothing, vehicle, place);
	}

	//------------------------------------------------------------------------------------------------
	protected void LF_ShowDispatchLine(string offence, string clothing, string vehicle, string place)
	{
		LF_NotificationManagerComponent manager = LF_NotificationManagerComponent.GetInstance();
		if (!manager)
		{
			LF_Debug.Error(LF_Dispatch.FEATURE, "dispatch line dropped: no LF_NotificationManagerComponent on this client");
			return;
		}

		string line = LF_Dispatch.RadioLine(offence, clothing, vehicle, place);
		// The harness reads the last toast by its key, as for every other toast.
		s_sLF_LastToast = LF_Dispatch.RADIO_KEY;
		LF_Debug.Info(LF_Dispatch.FEATURE, string.Format("dispatch line shown: %1", line));
		manager.ShowNotificationToast("#LF-Dispatch_Title", line, 8.0, LF_ENotificationType.POLICE_ALERT);
	}
}
