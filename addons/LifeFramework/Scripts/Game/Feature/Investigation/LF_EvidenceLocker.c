//------------------------------------------------------------------------------------------------
//! One logged bag in the evidence locker (#269): the case it belongs to and what was in it. Plain
//! values, persisted as is in the "evidencelocker" payload (save version 18). The piece itself is
//! not kept (items do not survive a restart); the record of it is, and it is read later from the
//! locker.
class LF_EvidenceLockerRow
{
	int m_iNumber;
	//! The crime report the bag belongs to (LF_WantedReportRecord.m_iSequence).
	int m_iReport;
	string m_sOffence;
	string m_sPlace;
	string m_sKind;
	//! Minutes the piece lay at the scene before it was bagged.
	int m_iMinutes;
	//! Prefab path of the bagged piece, empty when the bag held nothing.
	string m_sPrefab;
	//! Serial read off the piece, empty when it had none.
	string m_sSerial;
	string m_sOfficerUid;
	//! True when the piece named a buyer and identified their wanted report.
	bool m_bIdentified;

	LF_EvidenceLockerRow Copy()
	{
		LF_EvidenceLockerRow copy = new LF_EvidenceLockerRow();
		copy.m_iNumber = m_iNumber;
		copy.m_iReport = m_iReport;
		copy.m_sOffence = m_sOffence;
		copy.m_sPlace = m_sPlace;
		copy.m_sKind = m_sKind;
		copy.m_iMinutes = m_iMinutes;
		copy.m_sPrefab = m_sPrefab;
		copy.m_sSerial = m_sSerial;
		copy.m_sOfficerUid = m_sOfficerUid;
		copy.m_bIdentified = m_bIdentified;
		return copy;
	}
}

//------------------------------------------------------------------------------------------------
//! The evidence locker's case file (#269). An on-duty officer logs the evidence bags they carry
//! (LF_LogEvidenceAction). Each bag is opened, every piece in it goes through
//! LF_FenceSerials.IdentifyItem (a serial that names its buyer identifies that account's open
//! wanted reports), a row records it against the crime report, and the bag is consumed. Server
//! only; persisted as the "evidencelocker" payload. Forced entry is LF_Breach: a breach of the
//! locker is already a reported break-in, and a raid on an open locker destroys the case file.
class LF_EvidenceLocker
{
	static const string FEATURE = "EvidenceLocker";
	static const int MAX_ROWS = 500;
	//! Kind of a row logged by a back room raid; the only kind kept without a crime report.
	static const string KIND_BACKROOM = "backroom";

	protected static ref array<ref LF_EvidenceLockerRow> s_aRows;
	protected static int s_iNumber;

	//------------------------------------------------------------------------------------------------
	protected static array<ref LF_EvidenceLockerRow> Rows()
	{
		if (!s_aRows)
			s_aRows = new array<ref LF_EvidenceLockerRow>();

		return s_aRows;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_aRows = null;
		s_iNumber = 0;
	}

	//------------------------------------------------------------------------------------------------
	static int Count()
	{
		return Rows().Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Logged bags that belong to one crime report: the link from the wanted board's report to
	//! its evidence.
	static int CountFor(int report)
	{
		int count;
		foreach (LF_EvidenceLockerRow row : Rows())
		{
			if (row.m_iReport == report)
				count++;
		}

		return count;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The logged row for a case, newest first at index 0 of a copy list. Null when none.
	static LF_EvidenceLockerRow Find(int number)
	{
		foreach (LF_EvidenceLockerRow row : Rows())
		{
			if (row.m_iNumber == number)
				return row;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: logs one evidence bag. The bag must be stamped (a report on it). Every piece inside
	//! is serial-checked, then the bag and its contents are consumed.
	//! \return The row, or null when the entity is not a stamped evidence bag.
	static LF_EvidenceLockerRow LogBag(IEntity bag, string officerUid)
	{
		if (!Replication.IsServer() || !bag)
			return null;

		LF_EvidenceBagComponent bagComponent = LF_EvidenceBagComponent.Cast(bag.FindComponent(LF_EvidenceBagComponent));
		if (!bagComponent || !bagComponent.IsStamped())
		{
			LF_Debug.Warn(FEATURE, "log refused: not a stamped evidence bag");
			return null;
		}

		LF_EvidenceLockerRow row = new LF_EvidenceLockerRow();
		s_iNumber++;
		row.m_iNumber = s_iNumber;
		row.m_iReport = bagComponent.GetReport();
		row.m_sOffence = bagComponent.GetOffence();
		row.m_sPlace = bagComponent.GetPlace();
		row.m_sKind = bagComponent.GetKind();
		row.m_iMinutes = bagComponent.GetMinutes();
		row.m_sOfficerUid = officerUid;

		array<IEntity> contents = {};
		bagComponent.GetContents(contents);
		foreach (IEntity piece : contents)
		{
			if (!piece)
				continue;

			if (row.m_sPrefab.IsEmpty() && piece.GetPrefabData())
				row.m_sPrefab = piece.GetPrefabData().GetPrefabName();

			LF_SerialComponent serial = LF_SerialComponent.Cast(piece.FindComponent(LF_SerialComponent));
			if (serial && !serial.GetSerial().IsEmpty())
				row.m_sSerial = serial.GetSerial();

			if (LF_FenceSerials.IdentifyItem(piece, "evidence locker") > 0)
				row.m_bIdentified = true;
		}

		array<ref LF_EvidenceLockerRow> rows = Rows();
		rows.Insert(row);
		while (rows.Count() > MAX_ROWS)
		{
			rows.RemoveOrdered(0);
		}

		LF_Debug.Info(FEATURE, string.Format("bag #%1 logged for report #%2 (%3, %4): serial '%5' identified=%6", row.m_iNumber, row.m_iReport, row.m_sKind, row.m_sPlace, row.m_sSerial, row.m_bIdentified));
		SCR_EntityHelper.DeleteEntityAndChildren(bag);
		return row;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: logs one stock line seized in a back room raid (#600). It has no crime report; the
	//! row carries kind KIND_BACKROOM and the room's name as its place.
	static LF_EvidenceLockerRow LogSeizure(string prefab, string place, string officerUid)
	{
		if (!Replication.IsServer())
			return null;

		LF_EvidenceLockerRow row = new LF_EvidenceLockerRow();
		s_iNumber++;
		row.m_iNumber = s_iNumber;
		row.m_sKind = KIND_BACKROOM;
		row.m_sOffence = "contraband";
		row.m_sPlace = place;
		row.m_sPrefab = prefab;
		row.m_sOfficerUid = officerUid;

		array<ref LF_EvidenceLockerRow> rows = Rows();
		rows.Insert(row);
		while (rows.Count() > MAX_ROWS)
		{
			rows.RemoveOrdered(0);
		}

		LF_Debug.Info(FEATURE, string.Format("back room seizure #%1 logged: %2 from '%3'", row.m_iNumber, prefab, place));
		return row;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: logs every stamped evidence bag the officer carries.
	//! \return Bags logged.
	static int LogCarried(IEntity officer)
	{
		if (!Replication.IsServer() || !officer)
			return 0;

		SCR_InventoryStorageManagerComponent inventory = SCR_InventoryStorageManagerComponent.Cast(officer.FindComponent(SCR_InventoryStorageManagerComponent));
		if (!inventory)
			return 0;

		array<IEntity> items = {};
		inventory.GetItems(items);
		string uid = LF_Utils.GetPlayerUID(officer);
		int logged;
		foreach (IEntity item : items)
		{
			if (item && item.FindComponent(LF_EvidenceBagComponent) && LogBag(item, uid))
				logged++;
		}

		return logged;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a raid on an opened locker burns the case file. The break-in itself was reported by
	//! LF_Breach when the charge was armed.
	//! \return Rows destroyed.
	static int Raid(string raiderUid)
	{
		int destroyed = Rows().Count();
		Rows().Clear();
		LF_Debug.Warn(FEATURE, string.Format("locker raided by %1: %2 logged bag(s) destroyed", raiderUid, destroyed));
		return destroyed;
	}

	//------------------------------------------------------------------------------------------------
	//! One line per logged bag, newest first, for the locker's read card.
	static void DescribeAll(notnull array<string> outLines)
	{
		array<ref LF_EvidenceLockerRow> rows = Rows();
		for (int i = rows.Count() - 1; i >= 0; i--)
		{
			LF_EvidenceLockerRow row = rows[i];
			string line = string.Format("#%1 %2 %3", row.m_iReport, row.m_sKind, row.m_sPlace);
			if (!row.m_sSerial.IsEmpty())
				line = line + " " + row.m_sSerial;

			if (row.m_bIdentified)
				line = line + " *";

			outLines.Insert(line);
		}
	}

	//------------------------------------------------------------------------------------------------
	static array<ref LF_EvidenceLockerRow> ExportAll()
	{
		array<ref LF_EvidenceLockerRow> records = new array<ref LF_EvidenceLockerRow>();
		foreach (LF_EvidenceLockerRow row : Rows())
		{
			records.Insert(row.Copy());
		}

		return records;
	}

	//------------------------------------------------------------------------------------------------
	//! Replaces the case file (idempotent). Rows with no report are skipped.
	static void ApplyAll(array<ref LF_EvidenceLockerRow> records)
	{
		s_aRows = new array<ref LF_EvidenceLockerRow>();
		s_iNumber = 0;
		if (!records)
			return;

		foreach (LF_EvidenceLockerRow record : records)
		{
			if (!record || (record.m_iReport <= 0 && record.m_sKind != KIND_BACKROOM))
			{
				LF_Debug.Warn(FEATURE, "load skipped a locker row with no report");
				continue;
			}

			s_aRows.Insert(record.Copy());
			if (record.m_iNumber > s_iNumber)
				s_iNumber = record.m_iNumber;
		}
	}
}

//------------------------------------------------------------------------------------------------
//! Shared offer rules for the locker actions.
class LF_EvidenceLockerAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	protected LF_BreachTargetComponent Target()
	{
		return LF_BreachTargetComponent.Cast(GetOwner().FindComponent(LF_BreachTargetComponent));
	}
}

//------------------------------------------------------------------------------------------------
//! An on-duty officer logs the evidence bags they carry. The locker accepts nothing else.
class LF_LogEvidenceAction : LF_EvidenceLockerAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		return LF_AccountStateBits.IsOnDutyPolice(LF_AccountReplication.GetState(user));
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;

		if (!LF_PoliceUtils.IsOnDutyOfficer(LF_PoliceUtils.GetAccountForEntity(pUserEntity)))
		{
			LF_Debug.Warn(LF_EvidenceLocker.FEATURE, "log refused: not an on-duty officer");
			return;
		}

		int logged = LF_EvidenceLocker.LogCarried(pUserEntity);
		int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(pUserEntity);
		if (logged > 0)
			LF_NotificationManagerComponent.NotifyPlayer(playerId, "#LF-Locker_Title", "#LF-Locker_Logged", 5.0, LF_ENotificationType.INFO, logged.ToString());
		else
			LF_NotificationManagerComponent.NotifyPlayer(playerId, "#LF-Locker_Title", "#LF-Locker_NoBags", 5.0, LF_ENotificationType.INFO);
	}
}

//------------------------------------------------------------------------------------------------
//! Read the case file: police read it; the server answers with a toast (the file never reaches a
//! client that is not police).
class LF_ReadLockerAction : LF_EvidenceLockerAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		return LF_AccountStateBits.IsOnDutyPolice(LF_AccountReplication.GetState(user));
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;

		if (!LF_PoliceUtils.IsOnDutyOfficer(LF_PoliceUtils.GetAccountForEntity(pUserEntity)))
			return;

		array<string> lines = {};
		LF_EvidenceLocker.DescribeAll(lines);
		string text = "";
		int shown;
		foreach (string line : lines)
		{
			if (shown >= 5)
				break;

			if (shown > 0)
				text = text + " | ";

			text = text + line;
			shown++;
		}

		int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(pUserEntity);
		if (shown == 0)
			LF_NotificationManagerComponent.NotifyPlayer(playerId, "#LF-Locker_Title", "#LF-Locker_Empty", 6.0, LF_ENotificationType.INFO);
		else
			LF_NotificationManagerComponent.NotifyPlayer(playerId, "#LF-Locker_Title", "#LF-Locker_Cases", 10.0, LF_ENotificationType.INFO, text);
	}
}

//------------------------------------------------------------------------------------------------
//! Once the locker has been forced open (LF_Breach), anyone but on-duty police can burn the case
//! file inside it.
class LF_RaidLockerAction : LF_EvidenceLockerAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		LF_BreachTargetComponent target = Target();
		if (!target || !target.IsReplicatedOpen())
			return false;

		return !LF_AccountStateBits.IsOnDutyPolice(LF_AccountReplication.GetState(user));
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;

		LF_BreachTargetComponent target = Target();
		if (!target || !LF_Breach.IsOpen(target.GetTargetId()))
		{
			LF_Debug.Warn(LF_EvidenceLocker.FEATURE, "raid refused: the locker is not forced open");
			return;
		}

		if (LF_PoliceUtils.IsOnDutyOfficer(LF_PoliceUtils.GetAccountForEntity(pUserEntity)))
			return;

		int destroyed = LF_EvidenceLocker.Raid(LF_Utils.GetPlayerUID(pUserEntity));
		int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(pUserEntity);
		LF_NotificationManagerComponent.NotifyPlayer(playerId, "#LF-Locker_Title", "#LF-Locker_Raided", 6.0, LF_ENotificationType.INFO, destroyed.ToString());
	}
}
