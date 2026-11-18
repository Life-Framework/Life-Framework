//------------------------------------------------------------------------------------------------
//! Why a rent at a locker bank was or was not taken (#2659). Logged and shown to the renter.
enum LF_ELockerBankResult
{
	RENTED,
	ALREADY_RENTED,	//!< the character already has a locker at this site
	CANNOT_AFFORD,	//!< the rent is not in hand; nothing was taken
	OUT_OF_REACH,	//!< no locker bank within reach of the character
	BAD_CALLER,	//!< no character or no character key behind the request
	NOT_SERVER	//!< rents are a server decision
}

//------------------------------------------------------------------------------------------------
//! One rental in the "lockers" payload (#2659). Flat on purpose: saved as it is, row version first.
class LF_LockerRentalRow
{
	//! Row format, written first. 1 is this shape.
	int m_iVersion;
	string m_sSiteId;
	string m_sCharacterKey;
	//! UTC seconds; LF_LockerRules.NEVER (0) for a locker that never lapses.
	int m_iPaidUntil;

	static const int ROW_VERSION = 1;

	//------------------------------------------------------------------------------------------------
	static LF_LockerRentalRow Create(string siteId, string characterKey, int paidUntil)
	{
		LF_LockerRentalRow row = new LF_LockerRentalRow();
		row.m_iVersion = ROW_VERSION;
		row.m_sSiteId = siteId;
		row.m_sCharacterKey = characterKey;
		row.m_iPaidUntil = paidUntil;
		return row;
	}
}

//------------------------------------------------------------------------------------------------
//! Locker rentals (#2659, chapter #1038): who rents a locker at which site and until when. Server only.
//! The rules are LF_LockerRules (one locker per character per site, paid until); this class holds the
//! rentals, takes the cash and is the "lockers" payload's owner. A site is the stable id on a
//! LF_LockerBankComponent; the character is the character record key (LF_Utils.GetPlayerUID), always
//! derived on the server from the acting entity, never sent by a client.
class LF_LockerBank
{
	static const string FEATURE = "Lockers";

	//! siteId -> characterKey -> paid-until (UTC seconds; 0 never lapses).
	protected static ref map<string, ref map<string, int>> s_mRentals;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_mRentals = null;
	}

	//------------------------------------------------------------------------------------------------
	protected static map<string, ref map<string, int>> Rentals()
	{
		if (!s_mRentals)
			s_mRentals = new map<string, ref map<string, int>>();

		return s_mRentals;
	}

	//------------------------------------------------------------------------------------------------
	//! True when the character holds a locker at the site.
	static bool IsRenting(string siteId, string characterKey)
	{
		if (!s_mRentals)
			return false;

		map<string, int> site = s_mRentals.Get(siteId);
		return site && site.Contains(characterKey);
	}

	//------------------------------------------------------------------------------------------------
	//! The paid-until moment of a rental; LF_LockerRules.NEVER when there is none or it never lapses.
	static int PaidUntil(string siteId, string characterKey)
	{
		if (!s_mRentals)
			return LF_LockerRules.NEVER;

		map<string, int> site = s_mRentals.Get(siteId);
		if (!site)
			return LF_LockerRules.NEVER;

		int paidUntil;
		if (!site.Find(characterKey, paidUntil))
			return LF_LockerRules.NEVER;

		return paidUntil;
	}

	//------------------------------------------------------------------------------------------------
	//! Number of rentals held across every site.
	static int Count()
	{
		if (!s_mRentals)
			return 0;

		int total;
		foreach (string siteId, map<string, int> site : s_mRentals)
		{
			total += site.Count();
		}

		return total;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the acting character rents at the nearest locker bank within that bank's reach. The bank
	//! is found from the character's own position on the server; nothing the client says picks it.
	static LF_ELockerBankResult RentNearest(IEntity user)
	{
		if (!Replication.IsServer())
			return LF_ELockerBankResult.NOT_SERVER;

		if (!user)
		{
			LF_Debug.Error(FEATURE, "rent refused: no character behind the request");
			return LF_ELockerBankResult.BAD_CALLER;
		}

		LF_LockerBankComponent bank = LF_LockerBankComponent.FindNearest(user.GetOrigin());
		if (!bank)
		{
			LF_Debug.Info(FEATURE, string.Format("rent refused at %1: no locker bank within reach", user.GetOrigin()));
			return LF_ELockerBankResult.OUT_OF_REACH;
		}

		return RentAt(System.GetUnixTime(), LF_Utils.GetPlayerUID(user), user, user.GetOrigin(), bank.GetSiteId(), bank.GetOwner().GetOrigin(), bank.GetReach(), LF_LockerConfig.RentPrice(), LF_LockerConfig.RentPeriodDays());
	}

	//------------------------------------------------------------------------------------------------
	//! The seam tests drive with their own clock, positions and prices. Takes the rent from the
	//! character's cash only when every check passes; a short payment is handed back.
	static LF_ELockerBankResult RentAt(int now, string characterKey, IEntity user, vector userPos, string siteId, vector bankPos, float reach, int price, int rentPeriodDays)
	{
		if (!Replication.IsServer())
			return LF_ELockerBankResult.NOT_SERVER;

		if (!user || characterKey.IsEmpty() || siteId.IsEmpty())
		{
			LF_Debug.Error(FEATURE, string.Format("rent refused: missing character (%1) or site (%2)", characterKey, siteId));
			return LF_ELockerBankResult.BAD_CALLER;
		}

		if (vector.Distance(userPos, bankPos) > reach)
		{
			LF_Debug.Info(FEATURE, string.Format("%1 refused at %2: out of reach (%3 m)", characterKey, siteId, vector.Distance(userPos, bankPos)));
			return LF_ELockerBankResult.OUT_OF_REACH;
		}

		if (price < 0)
			price = 0;

		LF_ELockerRentResult check = LF_LockerRules.CheckRent(IsRenting(siteId, characterKey), LF_MoneyUtils.GetCash(user), price);
		if (check == LF_ELockerRentResult.ALREADY_RENTED)
		{
			LF_Debug.Info(FEATURE, string.Format("%1 refused at %2: already rents a locker here", characterKey, siteId));
			return LF_ELockerBankResult.ALREADY_RENTED;
		}

		if (check == LF_ELockerRentResult.CANNOT_AFFORD)
		{
			LF_Debug.Info(FEATURE, string.Format("%1 refused at %2: short of the rent %3", characterKey, siteId, price));
			return LF_ELockerBankResult.CANNOT_AFFORD;
		}

		if (price > 0)
		{
			int taken = LF_MoneyUtils.RemoveCash(user, price, "locker-rent");
			if (taken < price)
			{
				// The cash moved between the check and the take: hand back what was taken, rent nothing.
				if (taken > 0)
					LF_MoneyUtils.AddCash(user, taken, "locker-rent-refund", "", true);
				LF_Debug.Warn(FEATURE, string.Format("%1 refused at %2: took %3 of %4, handed back", characterKey, siteId, taken, price));
				return LF_ELockerBankResult.CANNOT_AFFORD;
			}
		}

		int paidUntil = LF_LockerRules.PaidUntil(LF_LockerRules.NEVER, now, rentPeriodDays);
		Set(siteId, characterKey, paidUntil);
		LF_Debug.Info(FEATURE, string.Format("%1 rented a locker at %2 for %3, paid until %4", characterKey, siteId, price, paidUntil));
		return LF_ELockerBankResult.RENTED;
	}

	//------------------------------------------------------------------------------------------------
	protected static void Set(string siteId, string characterKey, int paidUntil)
	{
		map<string, ref map<string, int>> rentals = Rentals();
		map<string, int> site = rentals.Get(siteId);
		if (!site)
		{
			site = new map<string, int>();
			rentals.Set(siteId, site);
		}

		site.Set(characterKey, paidUntil);
	}

	//------------------------------------------------------------------------------------------------
	//! The "lockers" payload rows: one per rental.
	static array<ref LF_LockerRentalRow> ExportAll()
	{
		array<ref LF_LockerRentalRow> rows = new array<ref LF_LockerRentalRow>();
		if (!s_mRentals)
			return rows;

		foreach (string siteId, map<string, int> site : s_mRentals)
		{
			foreach (string characterKey, int paidUntil : site)
			{
				rows.Insert(LF_LockerRentalRow.Create(siteId, characterKey, paidUntil));
			}
		}

		return rows;
	}

	//------------------------------------------------------------------------------------------------
	//! Replaces every rental with the loaded rows; applying the same rows twice gives the same table.
	//! A row with no site or character, or of an unknown row version, is dropped and logged.
	//! \return Rows kept.
	static int ApplyAll(array<ref LF_LockerRentalRow> rows)
	{
		s_mRentals = new map<string, ref map<string, int>>();
		if (!rows)
			return 0;

		int kept;
		foreach (LF_LockerRentalRow row : rows)
		{
			if (!row || row.m_sSiteId.IsEmpty() || row.m_sCharacterKey.IsEmpty())
			{
				LF_Debug.Warn(FEATURE, "dropped a locker rental row with no site or character");
				continue;
			}

			if (row.m_iVersion != LF_LockerRentalRow.ROW_VERSION)
			{
				LF_Debug.Warn(FEATURE, string.Format("dropped a locker rental row of unknown version %1", row.m_iVersion));
				continue;
			}

			Set(row.m_sSiteId, row.m_sCharacterKey, row.m_iPaidUntil);
			kept++;
		}

		return kept;
	}

	//------------------------------------------------------------------------------------------------
	//! Localization key the renter sees for a result.
	static string ResultKey(LF_ELockerBankResult result)
	{
		switch (result)
		{
			case LF_ELockerBankResult.RENTED:
				return "#LF-LockerBank_Rented";
			case LF_ELockerBankResult.ALREADY_RENTED:
				return "#LF-LockerBank_AlreadyRented";
			case LF_ELockerBankResult.CANNOT_AFFORD:
				return "#LF-LockerBank_CannotAfford";
			case LF_ELockerBankResult.OUT_OF_REACH:
				return "#LF-LockerBank_OutOfReach";
		}

		return "#LF-LockerBank_Refused";
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_LockerBank's statics (LF_StaticReset finds it by reflection).
class LF_LockerBank_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_LockerBank";
	}

	override void Apply()
	{
		LF_LockerBank.ResetStatics();
	}
}
