// LF_PlayerMenuFileTab.c: the personal file's fifth tab, "My file" (LF_PlayerMenuSheet), read only: what
// the law has on you. Your open tickets with what each costs now, the bookings on your record and when
// the latest was, and any licence that was seized with the days left before you can get it back
// (the same count the licence desk quotes, LF_LicenceSeizure.DaysLeft). The tab shows for everyone.
// The facts come from the server in an answer of their own (LF_PlayerMenuFileTabWire) and are built for
// the asking player's own account only. Every source is read through its public accessor
// (LF_FineTickets, LF_ConvictionRecord, LF_LicenceBans); nothing is saved or changed here.
//
// The count and the latest in-game day come from LF_ConvictionRecord; each past booking at the desk,
// with its charges, fine, settlement, seized licences and booking officer, from LF_BookingRecords
// (newest first, at most LF_BookingRecords.KEEP).
//
// Wire (version 1): records split by ';', fields by '|', names cleaned like LF_PlayerMenuData.
//   V|1
//   T|number|offenceKey|amountOwed         one open ticket
//   B|count|daysAgo                        bookings on record; daysAgo is 0 for today, -1 unknown
//   L|licenceName|daysLeft                 one seized licence (LF_ELicenseType name)
//   R|daysAgo|codes|fine|settled|servedSeconds|licences|officer
//                                          one past booking, newest first; codes and licences joined
//                                          by LF_BookingRecordRow.LIST_SEP, settled an LF_EBookingSettled

//------------------------------------------------------------------------------------------------
class LF_PlayerMenuFileData
{
	static const int VERSION = 1;
	static const string RECORD = ";";
	static const string FIELD = "|";

	ref array<int> m_aTicketNumbers = {};
	ref array<string> m_aTicketOffences = {};
	ref array<int> m_aTicketAmounts = {};
	int m_iBookings;
	//! In-game days since the latest booking; 0 is today, -1 with no booking.
	int m_iLastBookingDaysAgo = -1;
	ref array<string> m_aLicences = {};
	ref array<int> m_aLicenceDays = {};
	//! Past bookings, newest first. m_iDay holds days ago here (0 today), not the in-game day.
	ref array<ref LF_BookingRecordRow> m_aRecords = {};

	//------------------------------------------------------------------------------------------------
	string Encode()
	{
		string wire = "V" + FIELD + VERSION.ToString();
		foreach (int i, int number : m_aTicketNumbers)
		{
			wire += RECORD + "T" + FIELD + number.ToString() + FIELD + LF_PlayerMenuData.Clean(m_aTicketOffences[i]) + FIELD + m_aTicketAmounts[i].ToString();
		}

		wire += RECORD + "B" + FIELD + m_iBookings.ToString() + FIELD + m_iLastBookingDaysAgo.ToString();
		foreach (int l, string licence : m_aLicences)
		{
			wire += RECORD + "L" + FIELD + LF_PlayerMenuData.Clean(licence) + FIELD + m_aLicenceDays[l].ToString();
		}

		foreach (LF_BookingRecordRow row : m_aRecords)
		{
			wire += RECORD + "R" + FIELD + row.m_iDay.ToString() + FIELD + LF_PlayerMenuData.Clean(row.m_sCodes) + FIELD + row.m_iFine.ToString();
			wire += FIELD + row.m_iSettled.ToString() + FIELD + row.m_iServedSeconds.ToString() + FIELD + LF_PlayerMenuData.Clean(row.m_sLicences) + FIELD + LF_PlayerMenuData.Clean(row.m_sOfficer);
		}

		return wire;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The data, or null for a wire of another version.
	static LF_PlayerMenuFileData Decode(string wire)
	{
		array<string> records = {};
		wire.Split(RECORD, records, false);
		if (records.IsEmpty() || records[0] != "V" + FIELD + VERSION.ToString())
			return null;

		LF_PlayerMenuFileData data = new LF_PlayerMenuFileData();
		foreach (string record : records)
		{
			array<string> f = {};
			record.Split(FIELD, f, false);
			if (f.IsEmpty())
				continue;

			if (f[0] == "T" && f.Count() >= 4)
			{
				data.m_aTicketNumbers.Insert(f[1].ToInt());
				data.m_aTicketOffences.Insert(f[2]);
				data.m_aTicketAmounts.Insert(f[3].ToInt());
			}
			else if (f[0] == "B" && f.Count() >= 3)
			{
				data.m_iBookings = f[1].ToInt();
				data.m_iLastBookingDaysAgo = f[2].ToInt();
			}
			else if (f[0] == "L" && f.Count() >= 3 && !f[1].IsEmpty())
			{
				data.m_aLicences.Insert(f[1]);
				data.m_aLicenceDays.Insert(f[2].ToInt());
			}
			else if (f[0] == "R" && f.Count() >= 8)
			{
				LF_BookingRecordRow row = new LF_BookingRecordRow();
				row.m_iDay = f[1].ToInt();
				row.m_sCodes = f[2];
				row.m_iFine = f[3].ToInt();
				row.m_iSettled = f[4].ToInt();
				row.m_iServedSeconds = f[5].ToInt();
				row.m_sLicences = f[6];
				row.m_sOfficer = f[7];
				data.m_aRecords.Insert(row);
			}
		}

		return data;
	}
}

//------------------------------------------------------------------------------------------------
class LF_PlayerMenuFileTab
{
	static const string FEATURE = "PlayerMenu";
	//! The most tickets the card lists; the rest are counted in the line below.
	static const int TICKETS_SHOWN = 8;
	//! The most charges one past booking lists on the wire.
	static const int CODES_SHOWN = 6;
	static const string PART_COLUMNS = "PlayerMenuFileColumns";
	static const string CARD_TICKETS = "PlayerMenuFileTickets";
	static const string CARD_BOOKINGS = "PlayerMenuFileBookings";
	static const string CARD_LICENCES = "PlayerMenuFileLicences";
	static const string PART_WAITING = "PlayerMenuFileWaiting";
	static const string ROW_TICKET = "PlayerMenuFileTicket_";
	static const string ROW_BOOKING = "PlayerMenuFileBooking_";
	static const string ROW_LICENCE = "PlayerMenuFileLicence_";
	static const string ROW_RECORD = "PlayerMenuFileRecord_";
	static const string ROW_RECORD_NOTE = "PlayerMenuFileRecordNote_";

	//------------------------------------------------------------------------------------------------
	//! Server: the file of one account, read through the features' public accessors. Never null.
	//! \param uid The account's player uid. \param now The in-game day (LF_ConvictionRecord.Today).
	static LF_PlayerMenuFileData Gather(string uid, int today)
	{
		LF_PlayerMenuFileData data = new LF_PlayerMenuFileData();
		if (uid.IsEmpty())
			return data;

		array<LF_FineTicketRecord> tickets = {};
		LF_FineTickets.CollectFor(uid, tickets, true);
		foreach (LF_FineTicketRecord ticket : tickets)
		{
			data.m_aTicketNumbers.Insert(ticket.m_iNumber);
			data.m_aTicketOffences.Insert(ticket.m_sOffence);
			data.m_aTicketAmounts.Insert(ticket.m_iAmount);
		}

		data.m_iBookings = LF_ConvictionRecord.Count(uid);
		if (data.m_iBookings > 0)
		{
			foreach (LF_ConvictionRow row : LF_ConvictionRecord.ExportAll())
			{
				if (row.m_sPlayerUid == uid)
					data.m_iLastBookingDaysAgo = LF_Utils.MaxInt(today - row.m_iDay, 0);
			}
		}

		array<ref LF_BookingRecordRow> records = {};
		LF_BookingRecords.CollectFor(uid, records);
		foreach (LF_BookingRecordRow record : records)
		{
			record.m_iDay = LF_Utils.MaxInt(today - record.m_iDay, 0);
			// A fine paid at the desk starts no sentence, so the conviction count never saw it: count it here.
			if (record.m_iSettled == LF_EBookingSettled.PAID)
				data.m_iBookings++;

			if (data.m_iLastBookingDaysAgo < 0 || record.m_iDay < data.m_iLastBookingDaysAgo)
				data.m_iLastBookingDaysAgo = record.m_iDay;

			// Bounds the answer: ten bookings of a sheet with every code would pass 15k characters.
			array<string> codes = {};
			record.Codes(codes);
			if (codes.Count() > CODES_SHOWN)
			{
				codes.Resize(CODES_SHOWN);
				record.m_sCodes = LF_BookingRecordRow.Join(codes);
			}

			data.m_aRecords.Insert(record);
		}

		array<LF_ELicenseType> seizable = {LF_ELicenseType.DRIVING_LICENCE, LF_ELicenseType.FIREARM_PERMIT};
		foreach (LF_ELicenseType licence : seizable)
		{
			int days = LF_LicenceSeizure.DaysLeft(uid, licence);
			if (days <= 0)
				continue;

			data.m_aLicences.Insert(typename.EnumToString(LF_ELicenseType, licence));
			data.m_aLicenceDays.Insert(days);
		}

		return data;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the file for the player at playerId, from the server's own state.
	static LF_PlayerMenuFileData ServerBuild(int playerId)
	{
		if (!GetGame() || !GetGame().GetPlayerManager())
			return new LF_PlayerMenuFileData();

		IEntity body = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		return Gather(LF_Utils.GetPlayerUid(body), LF_ConvictionRecord.Today());
	}

	//------------------------------------------------------------------------------------------------
	//! The `#` key of a seized licence's name.
	static string LicenceKey(string licence)
	{
		if (licence == typename.EnumToString(LF_ELicenseType, LF_ELicenseType.DRIVING_LICENCE))
			return "#LF-PlayerMenuFile_LicenceDriving";

		if (licence == typename.EnumToString(LF_ELicenseType, LF_ELicenseType.FIREARM_PERMIT))
			return "#LF-PlayerMenuFile_LicenceFirearm";

		return "#LF-PlayerMenuFile_LicenceOther";
	}

	//------------------------------------------------------------------------------------------------
	//! The latest booking in words: today, yesterday or N days ago.
	static string LastBookingText(int daysAgo)
	{
		if (daysAgo <= 0)
			return LF_Terms.Text("#LF-PlayerMenuFile_BookedToday");

		if (daysAgo == 1)
			return LF_Terms.Text("#LF-PlayerMenuFile_BookedYesterday");

		return LF_Terms.Text("#LF-PlayerMenuFile_BookedDaysAgo", daysAgo.ToString());
	}

	//------------------------------------------------------------------------------------------------
	//! A past booking's charges in words: each code's own text, joined by ", ".
	static string ChargesText(notnull LF_BookingRecordRow record)
	{
		array<string> codes = {};
		record.Codes(codes);
		string words;
		foreach (string code : codes)
		{
			if (!words.IsEmpty())
				words += ", ";

			words += LF_Terms.Text(code);
		}

		if (words.IsEmpty())
			return LF_Terms.Text("#LF-PlayerMenuFile_RecordNoCharge");

		return words;
	}

	//------------------------------------------------------------------------------------------------
	//! How a past booking was settled: "$X paid" or "N min served".
	static string SettledText(notnull LF_BookingRecordRow record)
	{
		if (record.m_iSettled == LF_EBookingSettled.PAID)
			return LF_Terms.Text("#LF-PlayerMenuFile_RecordPaid", LF_MoneyFormat.FormatMoney(record.m_iFine));

		return LF_Terms.Text("#LF-PlayerMenuFile_RecordServed", LF_BookingDesk.Minutes(record.m_iServedSeconds).ToString());
	}

	//------------------------------------------------------------------------------------------------
	//! The line under a past booking: the licences it took and who booked it; empty with neither.
	static string RecordNote(notnull LF_BookingRecordRow record)
	{
		string note;
		array<string> licences = {};
		record.Licences(licences);
		foreach (string licence : licences)
		{
			if (!note.IsEmpty())
				note += "; ";

			note += LF_Terms.Text("#LF-PlayerMenuFile_RecordSeized", LF_Terms.Text(LicenceKey(licence)));
		}

		if (!record.m_sOfficer.IsEmpty())
		{
			if (!note.IsEmpty())
				note += "; ";

			note += LF_Terms.Text("#LF-PlayerMenuFile_RecordBookedBy", record.m_sOfficer);
		}

		return note;
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_UIPart CardIn(int index, string name)
	{
		array<float> weights = {1};
		return LF_UIPart.CreateColumns(name, weights).Cards().InColumn(PART_COLUMNS, index);
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_UIPart Card(string card, LF_UIPart part)
	{
		return part.InColumn(card, 0);
	}

	//------------------------------------------------------------------------------------------------
	//! Appends the tab's parts between the file's header and footer. Null data prints the waiting line.
	static void Build(notnull array<ref LF_UIPart> parts, LF_PlayerMenuFileData data)
	{
		if (!data)
		{
			parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, PART_WAITING, "#LF-PlayerMenuFile_Waiting"));
			return;
		}

		array<float> shares = {1, 1, 1};
		parts.Insert(LF_UIPart.CreateColumns(PART_COLUMNS, shares));

		parts.Insert(CardIn(0, CARD_TICKETS));
		parts.Insert(Card(CARD_TICKETS, LF_UIPart.Create(LF_EUIPartKind.HEADING, "PlayerMenuFileHeadTickets", "#LF-PlayerMenuFile_HeadTickets")));
		if (data.m_aTicketNumbers.IsEmpty())
			parts.Insert(Card(CARD_TICKETS, LF_UIPart.Create(LF_EUIPartKind.LINE, ROW_TICKET + "none", "#LF-PlayerMenuFile_TicketsNone")));

		foreach (int t, int number : data.m_aTicketNumbers)
		{
			if (t >= TICKETS_SHOWN)
				break;

			string label = LF_Terms.Text("#LF-PlayerMenuFile_TicketLabel", number.ToString(), LF_Terms.Text(data.m_aTicketOffences[t]));
			parts.Insert(Card(CARD_TICKETS, LF_UIPart.CreatePair(ROW_TICKET + t.ToString(), label, LF_MoneyFormat.FormatMoney(data.m_aTicketAmounts[t])).WithIcon("money")));
		}

		parts.Insert(CardIn(1, CARD_BOOKINGS));
		parts.Insert(Card(CARD_BOOKINGS, LF_UIPart.Create(LF_EUIPartKind.HEADING, "PlayerMenuFileHeadBookings", "#LF-PlayerMenuFile_HeadBookings")));
		if (data.m_iBookings <= 0)
		{
			parts.Insert(Card(CARD_BOOKINGS, LF_UIPart.Create(LF_EUIPartKind.LINE, ROW_BOOKING + "none", "#LF-PlayerMenuFile_BookingsNone")));
		}
		else
		{
			parts.Insert(Card(CARD_BOOKINGS, LF_UIPart.CreatePair(ROW_BOOKING + "count", "#LF-PlayerMenuFile_BookingsOnRecord", data.m_iBookings.ToString())));
			parts.Insert(Card(CARD_BOOKINGS, LF_UIPart.CreatePair(ROW_BOOKING + "last", "#LF-PlayerMenuFile_BookingLatest", LastBookingText(data.m_iLastBookingDaysAgo))));
		}

		foreach (int r, LF_BookingRecordRow record : data.m_aRecords)
		{
			string when = LF_Terms.Text("#LF-PlayerMenuFile_RecordLabel", LastBookingText(record.m_iDay), ChargesText(record));
			parts.Insert(Card(CARD_BOOKINGS, LF_UIPart.CreatePair(ROW_RECORD + r.ToString(), when, SettledText(record))));
			string note = RecordNote(record);
			if (!note.IsEmpty())
				parts.Insert(Card(CARD_BOOKINGS, LF_UIPart.Create(LF_EUIPartKind.LINE, ROW_RECORD_NOTE + r.ToString(), note)));
		}

		parts.Insert(CardIn(2, CARD_LICENCES));
		parts.Insert(Card(CARD_LICENCES, LF_UIPart.Create(LF_EUIPartKind.HEADING, "PlayerMenuFileHeadLicences", "#LF-PlayerMenuFile_HeadLicences")));
		if (data.m_aLicences.IsEmpty())
			parts.Insert(Card(CARD_LICENCES, LF_UIPart.Create(LF_EUIPartKind.LINE, ROW_LICENCE + "none", "#LF-PlayerMenuFile_LicencesNone")));

		foreach (int l, string licence : data.m_aLicences)
		{
			string back = LF_Terms.Text("#LF-PlayerMenuFile_BackInDays", data.m_aLicenceDays[l].ToString());
			parts.Insert(Card(CARD_LICENCES, LF_UIPart.CreatePair(ROW_LICENCE + l.ToString(), LicenceKey(licence), back).WithIcon("job")));
		}
	}
}
