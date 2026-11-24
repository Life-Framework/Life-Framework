//------------------------------------------------------------------------------------------------
//! How one booking at the desk was settled.
enum LF_EBookingSettled
{
	//! The suspect paid the sheet's fine at the desk and walked free.
	PAID = 1,
	//! The suspect went to the cell: "Take the time", or short of the fine.
	TIME_SERVED = 2
}

//------------------------------------------------------------------------------------------------
//! One settled booking on an account's record (#4711 follow-up). Plain values only: the account's player
//! uid, the booking officer's printed name (never an id), the sheet's charge codes and the licences
//! the sheet took, each list joined by LIST_SEP.
class LF_BookingRecordRow : Managed
{
	static const int ROW_VERSION = 1;
	static const string LIST_SEP = ",";

	int m_iVersion;
	string m_sPlayerUid;
	//! In-game day of the booking (LF_ConvictionRecord.Today).
	int m_iDay;
	//! The signed sheet's law codes (crime ids, localization keys), joined by LIST_SEP.
	string m_sCodes;
	//! The sheet's fine after the rank cap; 0 for a jail-only sheet.
	int m_iFine;
	//! LF_EBookingSettled.
	int m_iSettled;
	//! Cell time the booking served; 0 when paid.
	int m_iServedSeconds;
	//! LF_ELicenseType names the sheet seized, joined by LIST_SEP.
	string m_sLicences;
	//! The booking officer's character name when the booking settled; may be empty.
	string m_sOfficer;

	//------------------------------------------------------------------------------------------------
	static LF_BookingRecordRow Create(string uid, int day, array<string> codes, int fine, LF_EBookingSettled settled, int servedSeconds, array<string> licences, string officer)
	{
		LF_BookingRecordRow row = new LF_BookingRecordRow();
		row.m_iVersion = ROW_VERSION;
		row.m_sPlayerUid = uid;
		row.m_iDay = day;
		row.m_sCodes = Join(codes);
		row.m_iFine = fine;
		row.m_iSettled = settled;
		row.m_iServedSeconds = servedSeconds;
		row.m_sLicences = Join(licences);
		row.m_sOfficer = officer;
		return row;
	}

	//------------------------------------------------------------------------------------------------
	LF_BookingRecordRow Copy()
	{
		LF_BookingRecordRow copy = new LF_BookingRecordRow();
		copy.m_iVersion = m_iVersion;
		copy.m_sPlayerUid = m_sPlayerUid;
		copy.m_iDay = m_iDay;
		copy.m_sCodes = m_sCodes;
		copy.m_iFine = m_iFine;
		copy.m_iSettled = m_iSettled;
		copy.m_iServedSeconds = m_iServedSeconds;
		copy.m_sLicences = m_sLicences;
		copy.m_sOfficer = m_sOfficer;
		return copy;
	}

	//------------------------------------------------------------------------------------------------
	void Codes(notnull array<string> outCodes)
	{
		Split(m_sCodes, outCodes);
	}

	//------------------------------------------------------------------------------------------------
	void Licences(notnull array<string> outLicences)
	{
		Split(m_sLicences, outLicences);
	}

	//------------------------------------------------------------------------------------------------
	static string Join(array<string> values)
	{
		string joined;
		if (!values)
			return joined;

		foreach (string value : values)
		{
			string clean = value;
			clean.Replace(LIST_SEP, " ");
			if (clean.IsEmpty())
				continue;

			if (!joined.IsEmpty())
				joined += LIST_SEP;

			joined += clean;
		}

		return joined;
	}

	//------------------------------------------------------------------------------------------------
	static void Split(string joined, notnull array<string> outValues)
	{
		outValues.Clear();
		if (joined.IsEmpty())
			return;

		joined.Split(LIST_SEP, outValues, true);
	}
}

//------------------------------------------------------------------------------------------------
//! Past bookings per account (#4711 follow-up to the booking desk #4699): what the signed sheet charged,
//! the fine, how the suspect settled it, the licences it took and who booked them. The server writes
//! one row when a desk offer settles (LF_BookingDesk.PayNow / TakeTime below) and keeps the newest
//! KEEP per account. Server only, never replicated: My file reads the asking player's own rows
//! (LF_PlayerMenuFileTab.Gather). Persisted as the "booking_records" save section.
class LF_BookingRecords
{
	static const string FEATURE = "Law";
	//! Rows kept per account; the oldest goes when a new booking passes it.
	static const int KEEP = 10;	// lf-fixed: My file's card lists at most this many

	//! Oldest first, all accounts.
	protected static ref array<ref LF_BookingRecordRow> s_aRows;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_aRows = null;
	}

	//------------------------------------------------------------------------------------------------
	protected static array<ref LF_BookingRecordRow> Rows()
	{
		if (!s_aRows)
			s_aRows = new array<ref LF_BookingRecordRow>();

		return s_aRows;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a booking settled. Writes one row for the offer's suspect and trims their record to KEEP.
	//! \param fine The sheet's fine as offered (PayNow zeroes the offer's fine once it is paid).
	//! \return The row written, or null for a client or an offer with no suspect.
	static LF_BookingRecordRow Note(LF_BookingOffer offer, LF_EBookingSettled settled, int fine)
	{
		if (!offer || !Replication.IsServer())
			return null;

		if (offer.m_sSuspectUid.IsEmpty())
		{
			LF_Debug.Error(FEATURE, "booking record skipped: the offer names no suspect");
			return null;
		}

		int served = 0;
		if (settled == LF_EBookingSettled.TIME_SERVED)
			served = offer.m_iServeSeconds;

		array<string> licences = {};
		SeizedBy(offer.m_aCodes, licences);
		string officer = LF_PlayerInteraction.DisplayName(offer.m_sOfficerUid);
		LF_BookingRecordRow row = LF_BookingRecordRow.Create(offer.m_sSuspectUid, LF_ConvictionRecord.Today(), offer.m_aCodes, fine, settled, served, licences, officer);
		Rows().Insert(row);
		int dropped = Trim(offer.m_sSuspectUid);
		LF_Debug.Info(FEATURE, string.Format("booking record: %1 day=%2 codes=%3 fine=$%4 settled=%5 served=%6 s licences=%7 officer='%8' dropped=%9", row.m_sPlayerUid, row.m_iDay, row.m_sCodes, fine, typename.EnumToString(LF_EBookingSettled, settled), served, row.m_sLicences, officer, dropped));
		return row;
	}

	//------------------------------------------------------------------------------------------------
	//! The licences a sheet with these codes takes, by the same rule LF_LicenceSeizure.Book applies;
	//! none while the License system is off (the sheet then seizes nothing).
	static void SeizedBy(notnull array<string> codes, notnull array<string> outLicences)
	{
		outLicences.Clear();
		if (!LF_Systems.IsEnabled(LF_LicenseGate.SYSTEM))
			return;

		array<ref LF_LicenceBan> bans = {};
		LF_LicenceSeizureRules.BansFor(codes, bans);
		foreach (LF_LicenceBan ban : bans)
		{
			string name = typename.EnumToString(LF_ELicenseType, ban.m_eLicence);
			if (!outLicences.Contains(name))
				outLicences.Insert(name);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Drops the oldest rows of uid past KEEP. \return How many went.
	protected static int Trim(string uid)
	{
		int count = CountFor(uid);
		int dropped = 0;
		for (int i = 0; i < Rows().Count() && count > KEEP; )
		{
			if (Rows()[i].m_sPlayerUid == uid)
			{
				// Ordered: Remove() moves the last row into the gap and the newest-first order breaks.
				Rows().RemoveOrdered(i);
				count--;
				dropped++;
				continue;
			}

			i++;
		}

		return dropped;
	}

	//------------------------------------------------------------------------------------------------
	static int CountFor(string uid)
	{
		int count = 0;
		if (!s_aRows || uid.IsEmpty())
			return 0;

		foreach (LF_BookingRecordRow row : s_aRows)
		{
			if (row.m_sPlayerUid == uid)
				count++;
		}

		return count;
	}

	//------------------------------------------------------------------------------------------------
	//! Copies of one account's rows, newest first. Another account's rows are never added.
	static int CollectFor(string uid, notnull array<ref LF_BookingRecordRow> outRows)
	{
		outRows.Clear();
		if (!s_aRows || uid.IsEmpty())
			return 0;

		for (int i = s_aRows.Count() - 1; i >= 0; i--)
		{
			if (s_aRows[i].m_sPlayerUid == uid)
				outRows.Insert(s_aRows[i].Copy());
		}

		return outRows.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Copies of every row, oldest first (the save section).
	static array<ref LF_BookingRecordRow> ExportAll()
	{
		array<ref LF_BookingRecordRow> rows = new array<ref LF_BookingRecordRow>();
		if (!s_aRows)
			return rows;

		foreach (LF_BookingRecordRow row : s_aRows)
			rows.Insert(row.Copy());

		return rows;
	}

	//------------------------------------------------------------------------------------------------
	//! Replaces the store with saved rows (a second load ends the same). A row with no uid or an unknown
	//! settlement is skipped; each account keeps its newest KEEP. Null clears. \return Rows kept.
	static int ApplyAll(array<ref LF_BookingRecordRow> rows)
	{
		s_aRows = new array<ref LF_BookingRecordRow>();
		if (!rows)
			return 0;

		int skipped = 0;
		foreach (LF_BookingRecordRow row : rows)
		{
			if (!row || row.m_sPlayerUid.IsEmpty() || (row.m_iSettled != LF_EBookingSettled.PAID && row.m_iSettled != LF_EBookingSettled.TIME_SERVED))
			{
				skipped++;
				continue;
			}

			LF_BookingRecordRow copy = row.Copy();
			copy.m_iVersion = LF_BookingRecordRow.ROW_VERSION;
			s_aRows.Insert(copy);
			Trim(copy.m_sPlayerUid);
		}

		if (skipped > 0)
			LF_Debug.Warn(FEATURE, string.Format("booking records: skipped %1 saved row(s) with no account or settlement", skipped));

		return s_aRows.Count();
	}
}

//------------------------------------------------------------------------------------------------
//! Both ways out of the desk write the booking's record. The fine is read before the settlement,
//! which zeroes it on a paid offer; a payable offer still payable afterwards went to the cell short.
modded class LF_BookingDesk
{
	//------------------------------------------------------------------------------------------------
	override static string TakeTime(IEntity suspect)
	{
		LF_BookingOffer offer = OfferFor(LF_Utils.GetPlayerUid(suspect));
		int fine;
		if (offer)
			fine = offer.m_iFine;

		string refusal = super.TakeTime(suspect);
		if (refusal.IsEmpty())
			LF_BookingRecords.Note(offer, LF_EBookingSettled.TIME_SERVED, fine);

		return refusal;
	}

	//------------------------------------------------------------------------------------------------
	override static string PayNow(IEntity suspect)
	{
		LF_BookingOffer offer = OfferFor(LF_Utils.GetPlayerUid(suspect));
		int fine;
		if (offer)
			fine = offer.m_iFine;

		string refusal = super.PayNow(suspect);
		if (refusal.IsEmpty() && offer)
			LF_BookingRecords.Note(offer, SettledBy(offer), fine);

		return refusal;
	}

	//------------------------------------------------------------------------------------------------
	//! After a successful PayNow: paid when the offer is no longer payable (LF_ChargePayment spent it),
	//! else the suspect was short and went to the cell.
	static LF_EBookingSettled SettledBy(notnull LF_BookingOffer offer)
	{
		if (offer.m_bPayable)
			return LF_EBookingSettled.TIME_SERVED;

		return LF_EBookingSettled.PAID;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_BookingRecords' statics (LF_StaticReset finds it by reflection).
class LF_BookingRecords_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_BookingRecords";
	}

	override void Apply()
	{
		LF_BookingRecords.ResetStatics();
	}
}
