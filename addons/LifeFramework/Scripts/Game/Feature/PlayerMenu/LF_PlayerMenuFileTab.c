// LF_PlayerMenuFileTab.c: the personal file's fifth tab, "My file" (LF_PlayerMenuSheet), read only: what
// the law has on you. Your open tickets with what each costs now, the bookings on your record and when
// the latest was, and any licence that was seized with the days left before you can get it back
// (the same count the licence desk quotes, LF_LicenceSeizure.DaysLeft). The tab shows for everyone.
// The facts come from the server in an answer of their own (LF_PlayerMenuFileTabWire) and are built for
// the asking player's own account only. Every source is read through its public accessor
// (LF_FineTickets, LF_ConvictionRecord, LF_LicenceBans); nothing is saved or changed here.
//
// What a booking record holds today is a count and the latest in-game day (LF_ConvictionRecord); the
// charges of one booking and how it was settled (paid or time) are not stored per booking yet.
//
// Wire (version 1): records split by ';', fields by '|', names cleaned like LF_PlayerMenuData.
//   V|1
//   T|number|offenceKey|amountOwed         one open ticket
//   B|count|daysAgo                        bookings on record; daysAgo is 0 for today, -1 unknown
//   L|licenceName|daysLeft                 one seized licence (LF_ELicenseType name)

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
	static const string PART_COLUMNS = "PlayerMenuFileColumns";
	static const string CARD_TICKETS = "PlayerMenuFileTickets";
	static const string CARD_BOOKINGS = "PlayerMenuFileBookings";
	static const string CARD_LICENCES = "PlayerMenuFileLicences";
	static const string PART_WAITING = "PlayerMenuFileWaiting";
	static const string ROW_TICKET = "PlayerMenuFileTicket_";
	static const string ROW_BOOKING = "PlayerMenuFileBooking_";
	static const string ROW_LICENCE = "PlayerMenuFileLicence_";

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
