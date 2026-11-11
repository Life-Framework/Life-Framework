//------------------------------------------------------------------------------------------------
//! One crime report: what a witness would tell the police. The description is a snapshot taken
//! at the crime (what the suspect wore, what they drove, where it happened), so a suspect who
//! changes clothes or cars is not described by their new look. The name stays off the poster
//! until the server marks the report identified (an ID check by an on-duty officer, or an
//! officer close enough to catch the crime in the act). The client never sends the flag.
//! Also the persisted record of the "wanted" payload (save version 10).
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
//! suspect's wanted level returns to 0 (LF_PlayerAccount.SetWantedLevel).
class LF_WantedReports
{
	static const string FEATURE = "Wanted";
	static const string OFFENCE_ROBBERY = "#LF-Offence_Robbery";
	static const string OFFENCE_VEHICLE_THEFT = "#LF-Offence_VehicleTheft";
	static const string OFFENCE_WEAPON_THEFT = "#LF-Offence_WeaponTheft";
	static const string OFFENCE_BREAK_IN = "#LF-Offence_BreakIn";
	static const string OFFENCE_HOSPITAL = "#LF-Offence_HospitalRobbery";
	static const string OFFENCE_MURDER = "#LF-Offence_Murder";
	static const string OFFENCE_SCANNER = "#LF-Offence_Scanner";
	static const string OFFENCE_CASH_VAN = "#LF-Offence_CashVan";
	static const string OFFENCE_ARMORY = "#LF-Offence_ArmoryHeist";
	static const string OFFENCE_LOCKPICK = "#LF-Offence_Lockpick";
	static const string OFFENCE_CHOP = "#LF-Offence_ChopShop";
	static const string OFFENCE_JAILBREAK = "#LF-Offence_JailBreak";
	static const string OFFENCE_CHAIN_GOODS = "#LF-Offence_ChainGoods";
	static const string OFFENCE_SHOPLIFTING = "#LF-Offence_Shoplifting";
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
			LF_Debug.Error(FEATURE, "crime report refused: no player uid");
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

		LF_EventBus.Publish(LF_OnCrimeCommitted.Create(playerUid, offence));

		bool caught = suspect && IsOfficerNear(suspect, LF_PoliceConfig.CatchInActRadius());
		LF_WantedReportRecord record = Add(playerUid, offence, clothing, vehicle, place, caught);
		LF_Debug.Info(FEATURE, string.Format("crime report #%1 %2 by %3: clothing='%4' vehicle='%5' place='%6' identified=%7", record.m_iSequence, offence, playerUid, clothing, vehicle, place, caught));
		if (suspect)
		{
			LF_Evidence.OpenScene(record.m_iSequence, playerUid, offence, place, suspect.GetOrigin(), System.GetTickCount());
			LF_Witness.RecordCrime(record.m_iSequence, playerUid, offence, suspect, clothing, vehicle, place);
		}

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
			LF_Debug.Info(FEATURE, string.Format("%1 identified by %2: %3 report(s) now name them", playerUid, how, changed));

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
		LF_Debug.Info(FEATURE, string.Format("reports for %1 taken down: no longer wanted", playerUid));
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
	//! Takes down the reports of every loaded character whose wanted level is 0. The report names a
	//! character by its record key (#331), so this reads that character's standing, not the one its
	//! account plays. The save calls it after the last payload: the "characters" payload (save
	//! version 24) restores the wanted level of every character but the active one, after "wanted".
	//! A key with no loaded character keeps its reports.
	//! \return Characters whose reports were taken down.
	static int PruneCleared()
	{
		LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();
		if (!s_mReports || !accounts)
			return 0;

		array<string> cleared = {};
		foreach (string uid, array<ref LF_WantedReportRecord> list : s_mReports)
		{
			LF_CharacterStanding standing = accounts.FindStanding(uid);
			if (standing && standing.m_iWantedLevel <= 0)
				cleared.Insert(uid);
		}

		foreach (string uid : cleared)
		{
			s_mReports.Remove(uid);
		}

		if (!cleared.IsEmpty())
			LF_Debug.Info(FEATURE, string.Format("load took down the reports of %1 character(s) no longer wanted", cleared.Count()));

		return cleared.Count();
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
	//! Replaces the store (idempotent). Reports of characters no longer wanted are dropped afterwards
	//! by PruneCleared, once the save has restored every character's wanted level.
	static void ApplyAll(array<ref LF_WantedReportRecord> records)
	{
		s_mReports = new map<string, ref array<ref LF_WantedReportRecord>>();
		s_iSequence = 0;
		if (!records)
			return;

		foreach (LF_WantedReportRecord record : records)
		{
			if (!record || record.m_sPlayerUid.IsEmpty())
			{
				LF_Debug.Warn(FEATURE, "load skipped a wanted report with no player id");
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

				UIInfo info = LF_UIInfoUtils.GetInfo(item);
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
	//! The nearest place sign's title within the configured radius, else the 100 m grid square.
	//! Police notices (the wanted board, the roster sign) name no place: a robbery beside the
	//! station is not "seen at Wanted".
	static string DescribePlace(vector origin)
	{
		s_vPlaceOrigin = origin;
		s_fPlaceBest = LF_PoliceConfig.PlaceRadius();
		s_sPlaceTitle = string.Empty;
		if (GetGame() && GetGame().GetWorld())
			GetGame().GetWorld().QueryEntitiesBySphere(origin, s_fPlaceBest, CollectPlaceSign, null, EQueryEntitiesFlags.ALL);

		string title = s_sPlaceTitle;
		s_sPlaceTitle = string.Empty;
		if (!title.IsEmpty())
			return title;

		return GridPlace(origin);
	}

	protected static vector s_vPlaceOrigin;
	protected static float s_fPlaceBest;
	protected static string s_sPlaceTitle;

	//------------------------------------------------------------------------------------------------
	protected static bool CollectPlaceSign(IEntity entity)
	{
		if (!entity)
			return true;

		LF_SignComponent sign = LF_SignComponent.Cast(entity.FindComponent(LF_SignComponent));
		if (!sign || sign.GetSignId().StartsWith("police_"))
			return true;

		float distance = vector.Distance(entity.GetOrigin(), s_vPlaceOrigin);
		if (distance > s_fPlaceBest || !sign.GetSign() || sign.GetSign().m_sTitle.IsEmpty())
			return true;

		s_fPlaceBest = distance;
		s_sPlaceTitle = sign.GetSign().m_sTitle;
		return true;
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

		LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();
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

			if (LF_PoliceUtils.IsOnDutyOfficer(accounts.GetFromCache(playerId)))
				return true;
		}

		return false;
	}
}
