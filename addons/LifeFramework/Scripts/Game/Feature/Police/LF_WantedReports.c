//------------------------------------------------------------------------------------------------
//! Police tuning. Loaded from Configs/Police/LF_Police.conf; a missing file falls back to the
//! defaults below and logs once.
[BaseContainerProps(configRoot: true)]
class LF_PoliceConfig
{
	static const ResourceName CONFIG_PATH = "{D6DB2FB6399D91AB}Configs/Police/LF_Police.conf";

	[Attribute("25", UIWidgets.Auto, "An on-duty officer this close (metres) when a crime is committed catches the suspect in the act: the report is identified at once")]
	float m_fCatchInActRadius;

	[Attribute("80", UIWidgets.Auto, "A crime report names the nearest sign within this many metres as the place; beyond it the place is a map grid square")]
	float m_fPlaceRadius;

	[Attribute("4", UIWidgets.Auto, "How close (metres) a player must stand to the wanted board for the server to answer a read")]
	float m_fBoardReadDistance;

	[Attribute("8", UIWidgets.Auto, "Most posters the wanted board shows, newest first")]
	int m_iBoardMaxPosters;

	protected static ref LF_PoliceConfig s_Config;
	protected static bool s_bLoadAttempted;

	//------------------------------------------------------------------------------------------------
	//! The loaded config, or the code defaults when the file does not resolve (logged once).
	static LF_PoliceConfig Get()
	{
		if (s_bLoadAttempted && s_Config)
			return s_Config;

		s_bLoadAttempted = true;
		Resource container = BaseContainerTools.LoadContainer(CONFIG_PATH);
		if (container && container.GetResource())
			s_Config = LF_PoliceConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));

		if (!s_Config)
		{
			EL_Debug.Error("Police", string.Format("police config does not resolve, using defaults: %1", CONFIG_PATH));
			s_Config = new LF_PoliceConfig();
			s_Config.m_fCatchInActRadius = 25;
			s_Config.m_fPlaceRadius = 80;
			s_Config.m_fBoardReadDistance = 4;
			s_Config.m_iBoardMaxPosters = 8;
		}

		return s_Config;
	}
}

//------------------------------------------------------------------------------------------------
//! One crime report: what a witness would tell the police. The description is a snapshot taken
//! at the crime (what the suspect wore, what they drove, where it happened), so a suspect who
//! changes clothes or cars is not described by their new look. The name stays off the poster
//! until the server marks the report identified (an ID check by an on-duty officer, or an
//! officer close enough to catch the crime in the act). The client never sends the flag.
//! Also the persisted record of the "wanted" payload (save version 9).
class LF_WantedReportRecord : Managed
{
	string m_sPlayerUid;
	//! Localization key of the offence (one of the OFFENCE_ constants).
	string m_sOffence;
	//! Clothing display-name keys joined by '|', outermost first.
	string m_sClothing;
	//! Vehicle display-name key, empty on foot.
	string m_sVehicle;
	//! Nearest sign title key, or "grid X-Z" text.
	string m_sPlace;
	bool m_bIdentified;
	//! Report order across the server's life; newest has the highest number.
	int m_iSequence;

	//------------------------------------------------------------------------------------------------
	LF_WantedReportRecord Copy()
	{
		LF_WantedReportRecord copy = new LF_WantedReportRecord();
		copy.m_sPlayerUid = m_sPlayerUid;
		copy.m_sOffence = m_sOffence;
		copy.m_sClothing = m_sClothing;
		copy.m_sVehicle = m_sVehicle;
		copy.m_sPlace = m_sPlace;
		copy.m_bIdentified = m_bIdentified;
		copy.m_iSequence = m_iSequence;
		return copy;
	}
}

//------------------------------------------------------------------------------------------------
//! Server store of crime reports, keyed by player uid. A crime calls Report with the suspect's
//! body; the wanted board (LF_WantedBoard) reads the store per read. Reports come down when the
//! suspect's wanted level returns to 0 (EL_PlayerAccount.SetWantedLevel).
class LF_WantedReports
{
	static const string FEATURE = "Wanted";
	static const string OFFENCE_ROBBERY = "#LF-Offence_Robbery";
	static const string OFFENCE_VEHICLE_THEFT = "#LF-Offence_VehicleTheft";
	static const string OFFENCE_WEAPON_THEFT = "#LF-Offence_WeaponTheft";
	static const string OFFENCE_BREAK_IN = "#LF-Offence_BreakIn";
	static const string OFFENCE_HOSPITAL = "#LF-Offence_HospitalRobbery";
	static const string PLACE_GRID_KEY = "#LF-Wanted_PlaceGrid";

	protected static ref map<string, ref array<ref LF_WantedReportRecord>> s_mReports;
	protected static int s_iSequence;

	//------------------------------------------------------------------------------------------------
	protected static map<string, ref array<ref LF_WantedReportRecord>> Reports()
	{
		if (!s_mReports)
			s_mReports = new map<string, ref array<ref LF_WantedReportRecord>>();

		return s_mReports;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_mReports = null;
		s_iSequence = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: records a crime. Describes the suspect's body as it is now; a missing body still
	//! records the offence with an empty description.
	//! \return The stored report, or null when the uid is empty.
	static LF_WantedReportRecord Report(string playerUid, IEntity suspect, string offence)
	{
		if (playerUid.IsEmpty())
		{
			EL_Debug.Error(FEATURE, "crime report refused: no player uid");
			return null;
		}

		string clothing;
		string vehicle;
		string place;
		if (suspect)
		{
			clothing = DescribeClothing(suspect);
			vehicle = DescribeVehicle(suspect);
			place = DescribePlace(suspect.GetOrigin());
		}

		bool caught = suspect && IsOfficerNear(suspect, LF_PoliceConfig.Get().m_fCatchInActRadius);
		LF_WantedReportRecord record = Add(playerUid, offence, clothing, vehicle, place, caught);
		EL_Debug.Info(FEATURE, string.Format("crime report #%1 %2 by %3: clothing='%4' vehicle='%5' place='%6' identified=%7", record.m_iSequence, offence, playerUid, clothing, vehicle, place, caught));
		return record;
	}

	//------------------------------------------------------------------------------------------------
	//! Stores a report with a ready description. The seam tests drive.
	static LF_WantedReportRecord Add(string playerUid, string offence, string clothing, string vehicle, string place, bool identified)
	{
		LF_WantedReportRecord record = new LF_WantedReportRecord();
		s_iSequence++;
		record.m_sPlayerUid = playerUid;
		record.m_sOffence = offence;
		record.m_sClothing = clothing;
		record.m_sVehicle = vehicle;
		record.m_sPlace = place;
		record.m_bIdentified = identified;
		record.m_iSequence = s_iSequence;

		array<ref LF_WantedReportRecord> list = Reports().Get(playerUid);
		if (!list)
		{
			list = new array<ref LF_WantedReportRecord>();
			Reports().Set(playerUid, list);
		}

		list.Insert(record);
		return record;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the suspect is identified (an ID check by an on-duty officer, a catch in the act).
	//! Every open report of theirs shows the name from now on.
	//! \return Number of reports newly identified.
	static int Identify(string playerUid, string how)
	{
		array<ref LF_WantedReportRecord> list = GetFor(playerUid);
		if (!list)
			return 0;

		int changed;
		foreach (LF_WantedReportRecord record : list)
		{
			if (record && !record.m_bIdentified)
			{
				record.m_bIdentified = true;
				changed++;
			}
		}

		if (changed > 0)
			EL_Debug.Info(FEATURE, string.Format("%1 identified by %2: %3 report(s) now name them", playerUid, how, changed));

		return changed;
	}

	//------------------------------------------------------------------------------------------------
	static array<ref LF_WantedReportRecord> GetFor(string playerUid)
	{
		if (!s_mReports || playerUid.IsEmpty())
			return null;

		return s_mReports.Get(playerUid);
	}

	//------------------------------------------------------------------------------------------------
	static int CountFor(string playerUid)
	{
		array<ref LF_WantedReportRecord> list = GetFor(playerUid);
		if (!list)
			return 0;

		return list.Count();
	}

	//------------------------------------------------------------------------------------------------
	static bool IsIdentified(string playerUid)
	{
		array<ref LF_WantedReportRecord> list = GetFor(playerUid);
		if (!list)
			return false;

		foreach (LF_WantedReportRecord record : list)
		{
			if (record && record.m_bIdentified)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! The wanted level went back to 0: the posters come down.
	static void ClearFor(string playerUid)
	{
		if (!s_mReports || playerUid.IsEmpty() || !s_mReports.Contains(playerUid))
			return;

		s_mReports.Remove(playerUid);
		EL_Debug.Info(FEATURE, string.Format("reports for %1 taken down: no longer wanted", playerUid));
	}

	//------------------------------------------------------------------------------------------------
	//! Every report, newest first.
	static void CollectNewestFirst(notnull array<LF_WantedReportRecord> outReports)
	{
		outReports.Clear();
		if (!s_mReports)
			return;

		foreach (string uid, array<ref LF_WantedReportRecord> list : s_mReports)
		{
			if (!list)
				continue;

			foreach (LF_WantedReportRecord record : list)
			{
				if (!record)
					continue;

				int at = 0;
				while (at < outReports.Count() && outReports[at].m_iSequence > record.m_iSequence)
					at++;

				outReports.InsertAt(record, at);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	static array<ref LF_WantedReportRecord> ExportAll()
	{
		array<ref LF_WantedReportRecord> records = new array<ref LF_WantedReportRecord>();
		array<LF_WantedReportRecord> ordered = {};
		CollectNewestFirst(ordered);
		foreach (LF_WantedReportRecord record : ordered)
		{
			records.Insert(record.Copy());
		}

		return records;
	}

	//------------------------------------------------------------------------------------------------
	//! Replaces the store (idempotent). A report for a player who is no longer wanted is dropped.
	static void ApplyAll(array<ref LF_WantedReportRecord> records)
	{
		s_mReports = new map<string, ref array<ref LF_WantedReportRecord>>();
		s_iSequence = 0;
		if (!records)
			return;

		EL_PlayerAccountManager accounts = EL_PlayerAccountManager.GetInstance();
		foreach (LF_WantedReportRecord record : records)
		{
			if (!record || record.m_sPlayerUid.IsEmpty())
			{
				EL_Debug.Warn(FEATURE, "load skipped a wanted report with no player id");
				continue;
			}

			if (accounts)
			{
				EL_PlayerAccount account = accounts.GetFromCache(record.m_sPlayerUid);
				if (account && account.GetWantedLevel() <= 0)
					continue;
			}

			array<ref LF_WantedReportRecord> list = Reports().Get(record.m_sPlayerUid);
			if (!list)
			{
				list = new array<ref LF_WantedReportRecord>();
				Reports().Set(record.m_sPlayerUid, list);
			}

			list.Insert(record.Copy());
			s_iSequence = Math.Max(s_iSequence, record.m_iSequence);
		}

		// Oldest first inside each list, the order Add keeps.
		foreach (string uid, array<ref LF_WantedReportRecord> list : s_mReports)
		{
			for (int i = 1; i < list.Count(); i++)
			{
				for (int j = i; j > 0 && list[j - 1].m_iSequence > list[j].m_iSequence; j--)
					list.SwapItems(j, j - 1);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Clothing display names, outermost layer first (jacket, then pants, then headgear and the
	//! rest), as localization keys joined by '|'.
	static string DescribeClothing(IEntity suspect)
	{
		array<string> names = {};
		array<Managed> storages = {};
		suspect.FindComponents(BaseInventoryStorageComponent, storages);
		foreach (Managed managed : storages)
		{
			BaseInventoryStorageComponent storage = BaseInventoryStorageComponent.Cast(managed);
			if (!storage)
				continue;

			for (int slot = 0; slot < storage.GetSlotsCount(); slot++)
			{
				IEntity item = storage.Get(slot);
				if (!item || !item.FindComponent(BaseLoadoutClothComponent))
					continue;

				UIInfo info = EL_UIInfoUtils.GetInfo(item);
				if (!info)
					continue;

				string name = info.GetName();
				if (!name.IsEmpty() && names.Find(name) < 0)
					names.Insert(name);
			}
		}

		string joined;
		foreach (int index, string name : names)
		{
			if (index > 0)
				joined += "|";

			joined += name;
		}

		return joined;
	}

	//------------------------------------------------------------------------------------------------
	//! The vehicle the suspect sits in, as its display-name key; empty on foot.
	static string DescribeVehicle(IEntity suspect)
	{
		IEntity vehicle = CompartmentAccessComponent.GetVehicleIn(suspect);
		if (!vehicle)
			return string.Empty;

		SCR_EditableVehicleComponent editable = SCR_EditableVehicleComponent.Cast(vehicle.FindComponent(SCR_EditableVehicleComponent));
		if (editable && editable.GetInfo())
			return editable.GetInfo().GetName();

		return "#LF-Wanted_VehicleUnknown";
	}

	//------------------------------------------------------------------------------------------------
	//! The nearest sign's title within the configured radius, else the 100 m grid square.
	static string DescribePlace(vector origin)
	{
		IEntity signEntity = EL_ActionSite.FindNear(origin, LF_PoliceConfig.Get().m_fPlaceRadius, "LF_SignReadAction");
		if (signEntity)
		{
			LF_SignComponent sign = LF_SignComponent.Cast(signEntity.FindComponent(LF_SignComponent));
			if (sign && sign.GetSign() && !sign.GetSign().m_sTitle.IsEmpty())
				return sign.GetSign().m_sTitle;
		}

		return GridPlace(origin);
	}

	//------------------------------------------------------------------------------------------------
	//! "grid X-Z" text: the 100 m square, three digits each, as a 1989 map reads.
	static string GridPlace(vector origin)
	{
		int x = Math.Floor(origin[0] / 100);
		int z = Math.Floor(origin[2] / 100);
		return string.Format("%1 %2-%3", PLACE_GRID_KEY, x.ToString(3), z.ToString(3));
	}

	//------------------------------------------------------------------------------------------------
	//! Server: an on-duty officer other than the suspect stands within radius.
	static bool IsOfficerNear(IEntity suspect, float radius)
	{
		if (!suspect || radius <= 0 || !GetGame() || !GetGame().GetPlayerManager())
			return false;

		EL_PlayerAccountManager accounts = EL_PlayerAccountManager.GetInstance();
		if (!accounts)
			return false;

		array<int> playerIds = {};
		GetGame().GetPlayerManager().GetPlayers(playerIds);
		foreach (int playerId : playerIds)
		{
			IEntity body = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
			if (!body || body == suspect)
				continue;

			if (vector.Distance(body.GetOrigin(), suspect.GetOrigin()) > radius)
				continue;

			if (EL_PoliceUtils.IsOnDutyOfficer(accounts.GetFromCache(playerId)))
				return true;
		}

		return false;
	}
}
