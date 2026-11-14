//------------------------------------------------------------------------------------------------
//! One row of the case board's store, persisted as is in the "caseboard" payload (save version 31).
//! A report's own row (m_iNumber 0) holds the time of the crime; an evidence card's row holds the
//! locker row it stands for, what the piece says and whether it is pinned. Plain values only.
class LF_CaseBoardRow
{
	//! The crime report (LF_WantedReportRecord.m_iSequence).
	int m_iReport;
	//! The evidence locker row of a card (LF_EvidenceLockerRow.m_iNumber); 0 on a report's own row.
	int m_iNumber;
	//! A card: the evidence kind (LF_Evidence.KIND_*). A report's row: empty.
	string m_sKind;
	//! A card: what the piece says (LF_ScenePieces). A report's row: the world clock at the crime, "HH:MM".
	string m_sNote;
	//! Pin order, the newest pin highest; 0 when the card is not on the board.
	int m_iPinned;

	//------------------------------------------------------------------------------------------------
	LF_CaseBoardRow Copy()
	{
		LF_CaseBoardRow copy = new LF_CaseBoardRow();
		copy.m_iReport = m_iReport;
		copy.m_iNumber = m_iNumber;
		copy.m_sKind = m_sKind;
		copy.m_sNote = m_sNote;
		copy.m_iPinned = m_iPinned;
		return copy;
	}
}

//------------------------------------------------------------------------------------------------
//! The police station's case board (#148): the open cases, each with the crime, the place, the time,
//! the evidence cards officers pinned, the suspect's portrait once identified, and a red string from
//! every card to the suspect. Server only; the board on each officer's client draws it
//! (LF_CaseBoardComponent, LF_CaseBoardFace).
//!
//! It adds no crime model of its own. A case is a crime report (LF_WantedReports) and closes with
//! it (a cleared wanted level takes the report down). A card is an evidence locker row
//! (LF_EvidenceLocker): a bag logged at the locker, or pinned at the board, which logs it first. The
//! portrait is the wanted board's look (LF_WantedLook). The board keeps only what those do not: the
//! time of each crime, each card's note and which cards are pinned.
//!
//! What the board shows is police detail (the place, the evidence), so it is not drawn for everyone
//! who streams the prop in. The list goes to on-duty officers only, by an owner RPC on each
//! officer's player controller: when it changes (events, coalesced) and when an officer's client
//! asks (its face is built near the board, or the officer comes on duty). Everyone else sees the
//! plain cork. docs/decisions.md 2026-11-13 "#148".
class LF_CaseBoard
{
	static const string FEATURE = "CaseBoard";
	static const string KIND_CASE = "C";
	static const string KIND_CARD = "E";
	static const string FIELD = ";";
	//! Cases on the face: three columns.
	static const int MAX_CASES = 3;	// lf-fixed: the face layout has three columns
	//! Cards under one case: two rows of two.
	static const int CARDS_PER_CASE = 4;	// lf-fixed: the face layout has four card places a column
	//! Rows kept; past it the oldest unpinned row goes first.
	static const int MAX_ROWS = 500;
	//! Wait after the first change so a burst (a crime, its wanted level, its scene) makes one send.
	static const int COALESCE_MS = 1500;	// lf-fixed: replication batching, not gameplay
	static const int CASE_FIELDS = 8;	// lf-fixed: the entry format
	static const int CARD_FIELDS = 4;	// lf-fixed: the entry format

	protected static ref array<ref LF_CaseBoardRow> s_aRows;
	protected static int s_iPinOrder;
	protected static ref array<string> s_aEntries;
	protected static string s_sSignature;
	protected static bool s_bScheduled;
	protected static int s_iSends;
	protected static ref LF_CaseBoardSubscriber s_Subscriber;

	//------------------------------------------------------------------------------------------------
	protected static array<ref LF_CaseBoardRow> Rows()
	{
		if (!s_aRows)
			s_aRows = new array<ref LF_CaseBoardRow>();

		return s_aRows;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		if (s_bScheduled && GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(Flush);

		s_aRows = null;
		s_iPinOrder = 0;
		s_aEntries = null;
		s_sSignature = string.Empty;
		s_bScheduled = false;
		s_iSends = 0;
		s_Subscriber = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: listens for the events that change a case (a crime, a wanted level, an identification).
	//! Called when the game mode starts.
	static bool Install()
	{
		if (!Replication.IsServer())
			return false;

		if (s_Subscriber)
			return true;

		s_Subscriber = new LF_CaseBoardSubscriber();
		bool ok = LF_EventBus.Subscribe(LF_OnCrimeCommitted, s_Subscriber);
		ok = LF_EventBus.Subscribe(LF_OnWantedChanged, s_Subscriber) && ok;
		ok = LF_EventBus.Subscribe(LF_OnSuspectIdentified, s_Subscriber) && ok;
		return ok;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a crime report was filed. Keeps the time of the crime for its case.
	static void OnReport(int report)
	{
		if (!Replication.IsServer() || report <= 0)
			return;

		LF_CaseBoardRow row = new LF_CaseBoardRow();
		row.m_iReport = report;
		row.m_sNote = LF_Dispatch.ClockText();
		Keep(row);
		MarkDirty("report");
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a bag was logged at the evidence locker (LF_EvidenceLocker.LogBag). Its card waits,
	//! unpinned, until an officer pins it. A back room seizure has no report and no card.
	static LF_CaseBoardRow OnLogged(LF_EvidenceLockerRow logged, string note)
	{
		if (!Replication.IsServer() || !logged || logged.m_iReport <= 0)
			return null;

		LF_CaseBoardRow row = new LF_CaseBoardRow();
		row.m_iReport = logged.m_iReport;
		row.m_iNumber = logged.m_iNumber;
		row.m_sKind = logged.m_sKind;
		row.m_sNote = note;
		Keep(row);
		LF_Debug.Info(FEATURE, string.Format("card for locker row #%1 (report #%2, %3) filed, not pinned", row.m_iNumber, row.m_iReport, row.m_sKind));
		return row;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the locker's case file was burned (LF_EvidenceLocker.Raid). Its cards go with it.
	static void OnCaseFileBurned()
	{
		array<ref LF_CaseBoardRow> rows = Rows();
		int removed;
		for (int i = rows.Count() - 1; i >= 0; i--)
		{
			if (rows[i].m_iNumber > 0)
			{
				rows.RemoveOrdered(i);
				removed++;
			}
		}

		LF_Debug.Warn(FEATURE, string.Format("case file burned: %1 card(s) gone from the board", removed));
		MarkDirty("burned");
	}

	//------------------------------------------------------------------------------------------------
	protected static void Keep(notnull LF_CaseBoardRow row)
	{
		array<ref LF_CaseBoardRow> rows = Rows();
		rows.Insert(row);
		if (rows.Count() <= MAX_ROWS)
			return;

		for (int i = 0; i < rows.Count(); i++)
		{
			if (rows[i].m_iPinned == 0 && rows[i] != row)
			{
				rows.RemoveOrdered(i);
				return;
			}
		}

		rows.RemoveOrdered(0);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: an on-duty officer pins evidence at the board. Every stamped evidence bag they carry
	//! is logged at the locker first (the case file is the record) and its card pinned; with no bag,
	//! the next logged card of an open case goes up, newest case first. A case holds CARDS_PER_CASE.
	//! \return Cards pinned.
	static int Pin(IEntity officer)
	{
		if (!Replication.IsServer())
			return 0;

		int pinned;
		SCR_InventoryStorageManagerComponent inventory;
		if (officer)
			inventory = SCR_InventoryStorageManagerComponent.Cast(officer.FindComponent(SCR_InventoryStorageManagerComponent));

		if (inventory)
		{
			array<IEntity> items = {};
			inventory.GetItems(items);
			string uid = LF_Utils.GetPlayerUID(officer);
			foreach (IEntity item : items)
			{
				if (!item || !item.FindComponent(LF_EvidenceBagComponent))
					continue;

				LF_EvidenceLockerRow logged = LF_EvidenceLocker.LogBag(item, uid);
				if (logged && PinCard(FindCard(logged.m_iNumber)))
					pinned++;
			}
		}

		if (pinned == 0)
		{
			array<LF_WantedReportRecord> reports = {};
			LF_WantedReports.CollectNewestFirst(reports);
			foreach (LF_WantedReportRecord report : reports)
			{
				if (PinCard(NextUnpinned(report.m_iSequence)))
				{
					pinned++;
					break;
				}
			}
		}

		if (pinned > 0)
			MarkDirty("pin");
		else
			LF_Debug.Warn(FEATURE, "pin refused: no bag carried and no logged card of an open case left to pin");

		return pinned;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: takes down the card pinned last.
	//! \return The card taken down, or null when nothing is pinned.
	static LF_CaseBoardRow Unpin()
	{
		if (!Replication.IsServer())
			return null;

		LF_CaseBoardRow last;
		foreach (LF_CaseBoardRow row : Rows())
		{
			if (row.m_iPinned > 0 && (!last || row.m_iPinned > last.m_iPinned))
				last = row;
		}

		if (!last)
		{
			LF_Debug.Warn(FEATURE, "unpin refused: nothing is pinned");
			return null;
		}

		last.m_iPinned = 0;
		LF_Debug.Info(FEATURE, string.Format("card for locker row #%1 (report #%2) taken down", last.m_iNumber, last.m_iReport));
		MarkDirty("unpin");
		return last;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool PinCard(LF_CaseBoardRow card)
	{
		if (!card || card.m_iPinned > 0)
			return false;

		if (PinnedCount(card.m_iReport) >= CARDS_PER_CASE)
		{
			LF_Debug.Warn(FEATURE, string.Format("card for locker row #%1 stays in the locker: case #%2 already holds %3 cards", card.m_iNumber, card.m_iReport, CARDS_PER_CASE));
			return false;
		}

		s_iPinOrder++;
		card.m_iPinned = s_iPinOrder;
		LF_Debug.Info(FEATURE, string.Format("card for locker row #%1 (report #%2, %3) pinned", card.m_iNumber, card.m_iReport, card.m_sKind));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_CaseBoardRow FindCard(int number)
	{
		foreach (LF_CaseBoardRow row : Rows())
		{
			if (row.m_iNumber == number && number > 0)
				return row;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_CaseBoardRow NextUnpinned(int report)
	{
		foreach (LF_CaseBoardRow row : Rows())
		{
			if (row.m_iReport == report && row.m_iNumber > 0 && row.m_iPinned == 0)
				return row;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Cards pinned on one case.
	static int PinnedCount(int report)
	{
		int count;
		foreach (LF_CaseBoardRow row : Rows())
		{
			if (row.m_iReport == report && row.m_iPinned > 0)
				count++;
		}

		return count;
	}

	//------------------------------------------------------------------------------------------------
	//! The time of the crime ("HH:MM"); empty when the board never saw the report filed.
	static string TimeOf(int report)
	{
		foreach (LF_CaseBoardRow row : Rows())
		{
			if (row.m_iReport == report && row.m_iNumber == 0)
				return row.m_sNote;
		}

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the face's list. The cases with pinned cards come first, the one pinned to last at the
	//! top; open cases with no card fill the rest, newest first. Each case entry is followed by its
	//! cards, in the order they were pinned.
	//! \return Cases listed.
	static int BuildEntries(notnull array<string> outEntries)
	{
		outEntries.Clear();
		array<LF_WantedReportRecord> reports = {};
		LF_WantedReports.CollectNewestFirst(reports);

		// Cases by their newest pin, then the rest in report order (newest first).
		array<LF_WantedReportRecord> ordered = {};
		array<int> keys = {};
		foreach (LF_WantedReportRecord report : reports)
		{
			int newest = NewestPin(report.m_iSequence);
			int at = 0;
			while (at < ordered.Count() && keys[at] >= newest)
				at++;

			ordered.InsertAt(report, at);
			keys.InsertAt(newest, at);
		}

		LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();
		int cases;
		foreach (LF_WantedReportRecord open : ordered)
		{
			if (cases >= MAX_CASES)
				break;

			string name;
			if (open.m_bIdentified && accounts)
			{
				LF_PlayerCharacter suspect = accounts.FindCharacterByKey(open.m_sPlayerUid);
				if (suspect)
					name = suspect.GetFullName();
			}

			string appearance;
			if (open.m_bIdentified)
				appearance = open.m_sAppearance;

			outEntries.Insert(EncodeCase(open.m_iSequence, open.m_sOffence, open.m_sPlace, TimeOf(open.m_iSequence), name, appearance, open.m_bIdentified));
			cases++;

			array<LF_CaseBoardRow> cards = {};
			PinnedCards(open.m_iSequence, cards);
			foreach (LF_CaseBoardRow card : cards)
				outEntries.Insert(EncodeCard(card.m_iReport, card.m_sKind, card.m_sNote, card.m_iNumber));
		}

		return cases;
	}

	//------------------------------------------------------------------------------------------------
	protected static int NewestPin(int report)
	{
		int newest;
		foreach (LF_CaseBoardRow row : Rows())
		{
			if (row.m_iReport == report && row.m_iPinned > newest)
				newest = row.m_iPinned;
		}

		return newest;
	}

	//------------------------------------------------------------------------------------------------
	//! The pinned cards of one case, oldest pin first.
	static void PinnedCards(int report, notnull array<LF_CaseBoardRow> outCards)
	{
		outCards.Clear();
		foreach (LF_CaseBoardRow row : Rows())
		{
			if (row.m_iReport != report || row.m_iPinned == 0)
				continue;

			int at = 0;
			while (at < outCards.Count() && outCards[at].m_iPinned < row.m_iPinned)
				at++;

			outCards.InsertAt(row, at);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! A field never carries the separator: a typed name could otherwise shift the fields.
	static string Clean(string text)
	{
		string clean = text;
		clean.Replace(FIELD, ",");
		clean.Replace("\n", " ");
		return clean;
	}

	//------------------------------------------------------------------------------------------------
	//! "C;report;offence key;place;time;name;look;identified". The name and the look only once identified.
	static string EncodeCase(int report, string offence, string place, string time, string name, string appearance, bool identified)
	{
		string flag = "0";
		if (identified)
			flag = "1";

		return KIND_CASE + FIELD + report.ToString() + FIELD + Clean(offence) + FIELD + Clean(place) + FIELD + Clean(time) + FIELD + Clean(name) + FIELD + Clean(appearance) + FIELD + flag;
	}

	//------------------------------------------------------------------------------------------------
	//! "E;report;kind;note". The number of the locker row is in the log only.
	static string EncodeCard(int report, string kind, string note, int number)
	{
		return KIND_CARD + FIELD + report.ToString() + FIELD + Clean(kind) + FIELD + Clean(note);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the list back into cases with their cards. Malformed entries are dropped and logged.
	//! \return Cases decoded.
	static int Decode(notnull array<string> entries, notnull array<ref LF_CaseBoardCase> outCases)
	{
		outCases.Clear();
		LF_CaseBoardCase current;
		foreach (string entry : entries)
		{
			array<string> fields = {};
			entry.Split(FIELD, fields, false);
			if (fields.Count() >= CASE_FIELDS && fields[0] == KIND_CASE)
			{
				if (outCases.Count() >= MAX_CASES)
				{
					current = null;
					continue;
				}

				current = new LF_CaseBoardCase();
				current.m_iReport = fields[1].ToInt();
				current.m_sOffence = fields[2];
				current.m_sPlace = fields[3];
				current.m_sTime = fields[4];
				current.m_sName = fields[5];
				current.m_sAppearance = fields[6];
				current.m_bIdentified = fields[7] == "1";
				outCases.Insert(current);
				continue;
			}

			if (fields.Count() >= CARD_FIELDS && fields[0] == KIND_CARD && current && fields[1].ToInt() == current.m_iReport)
			{
				if (current.m_aKinds.Count() < CARDS_PER_CASE)
				{
					current.m_aKinds.Insert(fields[2]);
					current.m_aNotes.Insert(fields[3]);
				}

				continue;
			}

			LF_Debug.Error(FEATURE, "case board entry dropped, malformed or out of order: " + entry);
		}

		return outCases.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: something a case shows changed. The list is rebuilt once, COALESCE_MS later.
	static void MarkDirty(string why)
	{
		if (!Replication.IsServer() || s_bScheduled)
			return;

		if (!GetGame() || !GetGame().GetCallqueue())
		{
			Flush();
			return;
		}

		s_bScheduled = true;
		GetGame().GetCallqueue().CallLater(Flush, COALESCE_MS, false);
		LF_Debug.Log(FEATURE, "rebuild queued: " + why);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: rebuild; when it differs from what was sent, every on-duty officer gets it.
	//! \return True when the list changed.
	static bool Flush()
	{
		s_bScheduled = false;
		array<string> entries = {};
		BuildEntries(entries);
		string signature = Signature(entries);
		if (s_aEntries && signature == s_sSignature)
			return false;

		s_sSignature = signature;
		s_aEntries = entries;
		s_iSends++;
		int officers = SendToOfficers();
		LF_Debug.Info(FEATURE, string.Format("list changed: %1 entr(ies), %2 chars, sent to %3 on-duty officer(s)", entries.Count(), signature.Length(), officers));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	static string Signature(notnull array<string> entries)
	{
		string signature;
		foreach (string entry : entries)
			signature += entry + "\n";

		return signature;
	}

	//------------------------------------------------------------------------------------------------
	//! What was sent last (null before the first build). Tests read it.
	static array<string> Current()
	{
		return s_aEntries;
	}

	//------------------------------------------------------------------------------------------------
	//! Sends since the server started. Tests read it.
	static int SendCount()
	{
		return s_iSends;
	}

	//------------------------------------------------------------------------------------------------
	//! A rebuild is waiting for its coalescing delay. Tests read it.
	static bool IsQueued()
	{
		return s_bScheduled;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: who may see the board: an on-duty officer. The same check guards every send.
	static bool MayRead(IEntity body)
	{
		return body && LF_PoliceUtils.IsOnDutyOfficer(LF_PoliceUtils.GetAccountForEntity(body));
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the current list to every on-duty officer. \return Officers sent to.
	static int SendToOfficers()
	{
		if (!GetGame() || !GetGame().GetPlayerManager())
			return 0;

		PlayerManager players = GetGame().GetPlayerManager();
		array<int> ids = {};
		players.GetPlayers(ids);
		int sent;
		foreach (int id : ids)
		{
			SCR_PlayerController controller = SCR_PlayerController.Cast(players.GetPlayerController(id));
			if (controller && MayRead(players.GetPlayerControlledEntity(id)))
			{
				controller.LF_SendCaseBoard(CurrentOrBuild());
				sent++;
			}
		}

		return sent;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: one client asked (its face was built, or its officer came on duty). Only an on-duty
	//! officer gets the list; anyone else is refused and logged.
	//! \return True when the list was sent.
	static bool SendTo(SCR_PlayerController controller)
	{
		if (!Replication.IsServer() || !controller)
			return false;

		IEntity body = controller.GetControlledEntity();
		if (!MayRead(body))
		{
			LF_Debug.Warn(FEATURE, string.Format("list refused to player %1: not an on-duty officer", controller.GetPlayerId()));
			return false;
		}

		controller.LF_SendCaseBoard(CurrentOrBuild());
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static array<string> CurrentOrBuild()
	{
		if (!s_aEntries)
		{
			array<string> entries = {};
			BuildEntries(entries);
			s_aEntries = entries;
			s_sSignature = Signature(entries);
		}

		return s_aEntries;
	}

	//------------------------------------------------------------------------------------------------
	//! The rows to save: the open cases' own, cards included. A closed case's rows are dropped.
	static array<ref LF_CaseBoardRow> ExportAll()
	{
		array<LF_WantedReportRecord> reports = {};
		LF_WantedReports.CollectNewestFirst(reports);
		array<int> open = {};
		foreach (LF_WantedReportRecord report : reports)
			open.Insert(report.m_iSequence);

		array<ref LF_CaseBoardRow> records = new array<ref LF_CaseBoardRow>();
		foreach (LF_CaseBoardRow row : Rows())
		{
			if (open.Contains(row.m_iReport))
				records.Insert(row.Copy());
		}

		return records;
	}

	//------------------------------------------------------------------------------------------------
	//! Replaces the store (idempotent: a second load gives the same board). Rows with no report are
	//! skipped. The pin order resumes after the highest loaded pin.
	static void ApplyAll(array<ref LF_CaseBoardRow> records)
	{
		s_aRows = new array<ref LF_CaseBoardRow>();
		s_iPinOrder = 0;
		if (records)
		{
			foreach (LF_CaseBoardRow record : records)
			{
				if (!record || record.m_iReport <= 0)
				{
					LF_Debug.Warn(FEATURE, "load skipped a case board row with no report");
					continue;
				}

				s_aRows.Insert(record.Copy());
				if (record.m_iPinned > s_iPinOrder)
					s_iPinOrder = record.m_iPinned;
			}
		}

		MarkDirty("load");
	}
}

//------------------------------------------------------------------------------------------------
//! One case decoded on the client: the report, and its cards' kinds and notes in pin order.
class LF_CaseBoardCase : Managed
{
	int m_iReport;
	string m_sOffence;
	string m_sPlace;
	string m_sTime;
	//! Empty until the suspect is identified.
	string m_sName;
	//! The suspect's look (LF_WantedLook); empty until identified.
	string m_sAppearance;
	bool m_bIdentified;
	ref array<string> m_aKinds = {};
	ref array<string> m_aNotes = {};
}

//------------------------------------------------------------------------------------------------
//! Rebuilds the case board on the events that change a case.
class LF_CaseBoardSubscriber : LF_EventSubscriber
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "CaseBoard";
	}

	//------------------------------------------------------------------------------------------------
	override bool Handle(notnull LF_Event busEvent)
	{
		LF_CaseBoard.MarkDirty(busEvent.Type().ToString());
		return true;
	}
}
