//------------------------------------------------------------------------------------------------
//! The signed charge sheet takes the licences (#4428, police L7). When the suspect settles the booking
//! desk's offer, by paying ("Pay $X now") or by going to the cell ("Take the time", or short of the
//! fine), the sheet's counted codes book the bans through LF_LicenceSeizure.Book: the rule's licences
//! (driving, firearm, any a pack rule names) are taken and banned for the rule's days, and the suspect
//! is told which and for how long.
//!
//! The cell path also starts a sentence, and LF_Jail publishes LF_OnArrested for it, which books bans
//! from the open crime reports (LF_LicenceSeizureSubscriber). While a desk settlement runs, that arrest
//! is held back for the suspect, so the sheet's codes are the conviction and nothing is booked twice.
//! An arrest with no desk (LF_Jail.Arrest from elsewhere) keeps the report path. Server only; nothing
//! here persists (the bans ride the "licencebans" section).
class LF_LicenceSeizureSheet
{
	static const string FEATURE = "License";

	//! Suspects whose desk settlement is running: their LF_OnArrested books no bans of its own.
	protected static ref array<string> s_aHeld;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_aHeld = null;
	}

	//------------------------------------------------------------------------------------------------
	protected static array<string> Held()
	{
		if (!s_aHeld)
			s_aHeld = {};

		return s_aHeld;
	}

	//------------------------------------------------------------------------------------------------
	//! A desk settlement for uid starts: its arrest event seizes nothing.
	static void Hold(string uid)
	{
		if (!uid.IsEmpty() && !Held().Contains(uid))
			Held().Insert(uid);
	}

	//------------------------------------------------------------------------------------------------
	static void Release(string uid)
	{
		Held().RemoveItem(uid);
	}

	//------------------------------------------------------------------------------------------------
	static bool IsHeld(string uid)
	{
		return s_aHeld && s_aHeld.Contains(uid);
	}

	//------------------------------------------------------------------------------------------------
	//! The offer was settled (paid, or the cell): books its sheet codes' bans on `body`, once per offer.
	//! \return How many licences were seized; 0 for a sheet with no mapped code or a second call.
	static int Signed(LF_BookingOffer offer, IEntity body)
	{
		if (!offer || !Replication.IsServer())
			return 0;

		if (offer.m_bLicencesSeized)
		{
			LF_Debug.Warn(FEATURE, string.Format("charge sheet of %1 already seized its licences", offer.m_sSuspectUid));
			return 0;
		}

		offer.m_bLicencesSeized = true;
		if (LF_Systems.Refuses(LF_LicenseGate.SYSTEM, "charge sheet licence seizure"))
			return 0;

		int seized = LF_LicenceSeizure.Book(offer.m_sSuspectUid, offer.m_aCodes, body);
		LF_Debug.Info(FEATURE, string.Format("charge sheet of %1 by %2 (%3 code(s)) seized %4 licence(s)", offer.m_sSuspectUid, offer.m_sOfficerUid, offer.m_aCodes.Count(), seized));
		return seized;
	}
}

//------------------------------------------------------------------------------------------------
//! Whether the signed sheet already booked this offer's licence bans.
modded class LF_BookingOffer
{
	bool m_bLicencesSeized;
}

//------------------------------------------------------------------------------------------------
//! Both ways out of the desk book the sheet's bans; the cell's own arrest event is held back meanwhile.
modded class LF_BookingDesk
{
	//------------------------------------------------------------------------------------------------
	override static string TakeTime(IEntity suspect)
	{
		string uid = LF_Utils.GetPlayerUid(suspect);
		LF_BookingOffer offer = OfferFor(uid);
		LF_LicenceSeizureSheet.Hold(uid);
		string refusal = super.TakeTime(suspect);
		LF_LicenceSeizureSheet.Release(uid);
		if (refusal.IsEmpty())
			LF_LicenceSeizureSheet.Signed(offer, suspect);

		return refusal;
	}

	//------------------------------------------------------------------------------------------------
	override static string PayNow(IEntity suspect)
	{
		string uid = LF_Utils.GetPlayerUid(suspect);
		LF_BookingOffer offer = OfferFor(uid);
		LF_LicenceSeizureSheet.Hold(uid);
		string refusal = super.PayNow(suspect);
		LF_LicenceSeizureSheet.Release(uid);
		if (refusal.IsEmpty())
			LF_LicenceSeizureSheet.Signed(offer, suspect);

		return refusal;
	}
}

//------------------------------------------------------------------------------------------------
//! A sentence that comes from the desk is booked by the sheet, not by the open reports.
modded class LF_LicenceSeizure
{
	//------------------------------------------------------------------------------------------------
	override static int OnArrest(string uid)
	{
		if (LF_LicenceSeizureSheet.IsHeld(uid))
		{
			LF_Debug.Info(FEATURE, string.Format("arrest of %1 comes from the booking desk: the charge sheet books its licences", uid));
			return 0;
		}

		return super.OnArrest(uid);
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_LicenceSeizureSheet's statics (LF_StaticReset finds it by reflection).
class LF_LicenceSeizureSheet_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_LicenceSeizureSheet";
	}

	override void Apply()
	{
		LF_LicenceSeizureSheet.ResetStatics();
	}
}
