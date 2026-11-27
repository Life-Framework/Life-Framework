//------------------------------------------------------------------------------------------------
//! One open booking offer (#4420, police L5): what the charge sheet came to for one suspect and who
//! booked them. Server only; the suspect's client gets the numbers through RpcDo_LF_BookingOffer.
class LF_BookingOffer : Managed
{
	string m_sSuspectUid;
	string m_sOfficerUid;
	//! The booking officer's body, re-resolved and uid-checked on "take the time" (short-lived).
	EntityID m_OfficerId;
	//! The suspect's body when booked: a lapse clears its desk actions, and a new body (a reconnect)
	//! drops the offer so the suspect can be booked again.
	EntityID m_SuspectId;
	//! The fine the sheet asks for after the rank cap; 0 when jail only.
	int m_iFine;
	//! True when "Pay $X now" is offered.
	bool m_bPayable;
	//! The sheet's time before the jail's own cap.
	int m_iSheetSeconds;
	//! The booking option passed to LF_JailBooking.Book for "take the time".
	int m_iPercent;
	//! What "take the time" actually serves (the jail's length at m_iPercent).
	int m_iServeSeconds;
	//! The counted law codes, for the log and the later paper sheet.
	ref array<string> m_aCodes = {};
	//! System.GetTickCount() when the offer was made.
	int m_iMadeAtMs;

	//------------------------------------------------------------------------------------------------
	int ServeMinutes()
	{
		return LF_BookingDesk.Minutes(m_iServeSeconds);
	}
}

//------------------------------------------------------------------------------------------------
//! The booking desk (#4420, police L5). An on-duty officer at the desk uses "Book suspect" on the
//! cuffed suspect beside them. The server builds the charge sheet from the suspect's open crime
//! reports with `LF_ChargeSheetRules`, using the officer's rank and duty from the server's own account
//! (never from a client), and offers the suspect two scroll actions at the desk: "Pay $X now" and
//! "Take the time (N min)". One open offer per suspect; a second booking is refused in words.
class LF_BookingDesk
{
	static const string FEATURE = "Law";
	//! How far from the desk origin the booking officer may stand.
	static const float DESK_METERS = 3.0;	// lf-fixed: one desk's counter, not a tunable
	//! An unanswered offer lapses after this long, and the suspect can be booked again.
	static const int OFFER_MS = 300000;	// lf-fixed: five minutes to answer at the desk

	protected static ref map<string, ref LF_BookingOffer> s_mOffers;
	//! The uid the last offer went to (the test's "reaches the suspect only").
	protected static string s_sLastOfferedTo;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_mOffers = null;
		s_sLastOfferedTo = string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	protected static map<string, ref LF_BookingOffer> Offers()
	{
		if (!s_mOffers)
			s_mOffers = new map<string, ref LF_BookingOffer>();

		return s_mOffers;
	}

	//------------------------------------------------------------------------------------------------
	//! Whole minutes for an action label, rounded up so "1 min" never hides 30 s.
	static int Minutes(int seconds)
	{
		if (seconds <= 0)
			return 0;

		return (seconds + 59) / 60;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the suspect's open offer, or null (a lapsed offer is dropped here).
	static LF_BookingOffer OfferFor(string suspectUid)
	{
		if (suspectUid.IsEmpty() || !s_mOffers)
			return null;

		LF_BookingOffer offer = s_mOffers.Get(suspectUid);
		if (!offer)
			return null;

		string stale;
		if (System.GetTickCount() - offer.m_iMadeAtMs > OFFER_MS)
			stale = "lapsed unanswered";
		else if (LF_Jail.IsServing(suspectUid))
			stale = "the suspect is already serving";
		else if (!IsStillWanted(suspectUid))
			stale = "the suspect is no longer wanted";

		if (stale.IsEmpty())
			return offer;

		LF_Debug.Info(FEATURE, string.Format("booking offer for %1 dropped: %2", suspectUid, stale));
		Drop(offer);
		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool IsStillWanted(string suspectUid)
	{
		LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();
		if (!accounts)
			return false;

		LF_PlayerAccount account = accounts.GetAccount(suspectUid);
		return account && account.GetWantedLevel() > 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: forget an offer and clear the desk actions on the suspect's body, if it is still there.
	protected static void Drop(notnull LF_BookingOffer offer)
	{
		IEntity body;
		if (offer.m_SuspectId != EntityID.INVALID && GetGame().GetWorld())
			body = GetGame().GetWorld().FindEntityByID(offer.m_SuspectId);

		Close(offer.m_sSuspectUid, body);
	}

	//------------------------------------------------------------------------------------------------
	static string LastOfferedTo()
	{
		return s_sLastOfferedTo;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: drop a suspect's offer and clear their desk actions.
	static void Close(string suspectUid, IEntity suspect = null)
	{
		if (s_mOffers)
			s_mOffers.Remove(suspectUid);

		SCR_ChimeraCharacter body = SCR_ChimeraCharacter.Cast(suspect);
		if (body)
			body.LF_SendBookingOffer(0, 0, false, false);
	}

	//------------------------------------------------------------------------------------------------
	//! Server, from the desk action: book the suspect the request names. Every input is re-checked:
	//! the id must resolve to a cuffed character beside the officer, not the officer.
	//! \return "" when the offer was made, else the key the officer reads.
	static string RequestBook(IEntity officer, vector deskOrigin, RplId suspectId)
	{
		if (!Replication.IsServer() || !officer)
			return "#LF-BookingDesk_NoSuspect";

		string officerUid = LF_Utils.GetPlayerUid(officer);
		IEntity suspect = LF_NetworkUtils.FindEntityByRplId(suspectId);
		SCR_ChimeraCharacter body = SCR_ChimeraCharacter.Cast(suspect);
		string refusal;
		if (!body || suspect == officer)
			refusal = "#LF-BookingDesk_NoSuspect";
		else if (vector.Distance(suspect.GetOrigin(), officer.GetOrigin()) > LF_PoliceConfig.ArrestReach())
			refusal = "#LF-BookingDesk_NoSuspect";
		else if (!LF_Restraint.IsRestrained(suspect))
			refusal = "#LF-JailBook_CuffFirst";

		if (!refusal.IsEmpty())
		{
			LF_Debug.Warn(FEATURE, string.Format("booking refused: officer %1 asked for suspect rpl %2 (%3)", officerUid, suspectId, refusal));
			return refusal;
		}

		string suspectUid = LF_Utils.GetPlayerUid(suspect);
		return Offer(officerUid, officer.GetOrigin(), deskOrigin, suspectUid, officer, suspect);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: build the charge sheet and open the offer. The officer's duty and rank come from the
	//! server's account for `officerUid`. A signed charge sheet (#4426) passes the codes the officer ticked:
	//! only those of the suspect's open reports are charged, and every number is still the server's.
	//! \return "" when the offer was made, else the refusal key.
	static string Offer(string officerUid, vector officerOrigin, vector deskOrigin, string suspectUid, IEntity officer = null, IEntity suspect = null, array<string> tickedCodes = null)
	{
		if (!Replication.IsServer())
			return "#LF-BookingDesk_NoSuspect";

		LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();
		LF_PlayerAccount account;
		if (accounts && !officerUid.IsEmpty())
			account = accounts.GetAccount(officerUid);

		if (!LF_PoliceUtils.IsOnDutyOfficer(account))
		{
			LF_Debug.Warn(FEATURE, string.Format("booking refused: officer %1 is not on duty", officerUid));
			return "#LF-BookingDesk_NotOnDuty";
		}

		if (vector.Distance(officerOrigin, deskOrigin) > DESK_METERS)
		{
			LF_Debug.Warn(FEATURE, string.Format("booking refused: officer %1 is %2 m from the desk", officerUid, vector.Distance(officerOrigin, deskOrigin)));
			return "#LF-BookingDesk_NotHere";
		}

		if (suspectUid.IsEmpty() || suspectUid == officerUid)
		{
			LF_Debug.Warn(FEATURE, string.Format("booking refused: officer %1 named no suspect", officerUid));
			return "#LF-BookingDesk_NoSuspect";
		}

		// The desk serves the cell: "take the time" books through the jail, which needs the cell close.
		if (!LF_JailBooking.IsAtBooking(deskOrigin))
		{
			LF_Debug.Warn(FEATURE, string.Format("booking refused: officer %1, this desk is not at the jail cell", officerUid));
			return "#LF-JailBook_NotHere";
		}

		LF_BookingOffer open = OfferFor(suspectUid);
		if (open && suspect && open.m_SuspectId != suspect.GetID())
		{
			// The suspect reconnected: the old body's offer never reached this one.
			LF_Debug.Info(FEATURE, string.Format("booking offer for %1 dropped: a new body", suspectUid));
			Drop(open);
			open = null;
		}

		if (open)
		{
			LF_Debug.Warn(FEATURE, string.Format("booking refused: %1 already has an open offer", suspectUid));
			return "#LF-BookingDesk_OfferOpen";
		}

		array<string> offences = {};
		array<ref LF_WantedReportRecord> reports = LF_WantedReports.GetFor(suspectUid);
		if (reports)
		{
			foreach (LF_WantedReportRecord report : reports)
			{
				if (report && !report.m_sOffence.IsEmpty())
					offences.Insert(report.m_sOffence);
			}
		}

		if (tickedCodes)
		{
			array<string> kept = {};
			foreach (string offence : offences)
			{
				if (tickedCodes.Contains(offence) && !kept.Contains(offence))
					kept.Insert(offence);
			}

			offences = kept;
		}

		array<LF_CrimeEntry> codes = {};
		LF_LawCode.Resolve(offences, codes);
		array<string> repeats = {};	// per-code repeats today are not recorded yet; the sheet counts none
		LF_ChargeSheetResult sheet = LF_ChargeSheetRules.Evaluate(codes, repeats, account.GetPoliceRank(), true, false, officerUid);
		if (!sheet || !sheet.IsSigned())
		{
			LF_Debug.Warn(FEATURE, string.Format("booking refused: %1 has no open charge (%2 reports)", suspectUid, offences.Count()));
			return "#LF-BookingDesk_NoCharges";
		}

		LF_BookingOffer offer = new LF_BookingOffer();
		offer.m_sSuspectUid = suspectUid;
		offer.m_sOfficerUid = officerUid;
		if (officer)
			offer.m_OfficerId = officer.GetID();
		else
			offer.m_OfficerId = EntityID.INVALID;

		if (suspect)
			offer.m_SuspectId = suspect.GetID();
		else
			offer.m_SuspectId = EntityID.INVALID;

		offer.m_iFine = sheet.m_iFine;
		offer.m_bPayable = sheet.m_bFineOffered;
		offer.m_iSheetSeconds = sheet.m_iSentenceSeconds;
		foreach (LF_CrimeEntry counted : sheet.m_aCounted)
			offer.m_aCodes.Insert(counted.m_sCrimeId);

		// A jail-only code adds no fine, so the fine count leaves it out, but it is a conviction all the same: the one
		// of its stacking group with the longest time counts, and its licence ban (murder takes the permit) is booked.
		array<LF_CrimeEntry> bySentence = {};
		LF_LawCodeRules.Counted(codes, false, bySentence);
		foreach (LF_CrimeEntry served : bySentence)
		{
			if (served.m_bJailOnly && !offer.m_aCodes.Contains(served.m_sCrimeId))
				offer.m_aCodes.Insert(served.m_sCrimeId);
		}

		PlaceTime(offer);
		offer.m_iMadeAtMs = System.GetTickCount();
		Offers().Set(suspectUid, offer);
		s_sLastOfferedTo = suspectUid;
		LF_Debug.Info(FEATURE, string.Format("booking offer: %1 by %2 codes=%3 fine=$%4 payable=%5 sheet=%6 s serve=%7 s", suspectUid, officerUid, offer.m_aCodes.Count(), offer.m_iFine, offer.m_bPayable, offer.m_iSheetSeconds, offer.m_iServeSeconds));
		Deliver(offer, officer, suspect);
		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! The jail's own length for this suspect (#4207 caps it) and the share of it the sheet asks for.
	protected static void PlaceTime(notnull LF_BookingOffer offer)
	{
		int wanted = 0;
		LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();
		if (accounts)
		{
			LF_PlayerAccount suspectAccount = accounts.GetAccount(offer.m_sSuspectUid);
			if (suspectAccount)
				wanted = suspectAccount.GetWantedLevel();
		}

		// A fine-only sheet has no time of its own: refusing the fine costs cell time by the fine (#4427).
		int sheetSeconds = offer.m_iSheetSeconds;
		if (sheetSeconds <= 0)
			sheetSeconds = LF_ChargePayment.RefusedFineSeconds(offer.m_iFine);

		// The same full length LF_Jail.Arrest serves: the wanted time or the longest reported code.
		int full = LF_CrimeCatalog.SentenceFor(offer.m_sSuspectUid, LF_Jail.SentenceSeconds(wanted));
		if (full <= 0)
		{
			offer.m_iPercent = LF_JailBooking.FULL_PERCENT;
			offer.m_iServeSeconds = LF_Utils.MinInt(sheetSeconds, LF_ChargeSheetRules.MAX_SENTENCE_SECONDS);
			return;
		}

		int percent = (sheetSeconds * LF_JailBooking.FULL_PERCENT + full - 1) / full;
		offer.m_iPercent = LF_Utils.ClampInt(percent, 1, LF_JailBooking.FULL_PERCENT);
		offer.m_iServeSeconds = LF_JailBooking.Scaled(full, offer.m_iPercent);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the offer goes to the suspect's machine only (an owner RPC on their body) and a toast to
	//! each side; the officer is told the outcome, never handed the actions.
	protected static void Deliver(notnull LF_BookingOffer offer, IEntity officer, IEntity suspect)
	{
		SCR_ChimeraCharacter body = SCR_ChimeraCharacter.Cast(suspect);
		if (body)
			body.LF_SendBookingOffer(offer.m_iFine, offer.m_iServeSeconds, offer.m_bPayable, true);

		string terms = string.Format("%1 min", offer.ServeMinutes());
		if (offer.m_bPayable)
			terms = string.Format("%1 or %2 min", LF_MoneyFormat.FormatMoney(offer.m_iFine), offer.ServeMinutes());

		int suspectId = LF_PlayerInteraction.PlayerIdOf(suspect);
		if (suspectId > 0)
			LF_NotificationManagerComponent.NotifyPlayer(suspectId, "#LF-BookingDesk_Title", "#LF-BookingDesk_OfferSuspect", 10.0, LF_ENotificationType.POLICE_ALERT, terms);

		int officerId = LF_PlayerInteraction.PlayerIdOf(officer);
		if (officerId > 0)
			LF_NotificationManagerComponent.NotifyPlayer(officerId, "#LF-BookingDesk_Title", "#LF-BookingDesk_Offered", 6.0, LF_ENotificationType.SUCCESS, terms);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the suspect chose "Take the time". The booking officer books them through the jail's
	//! own path (LF_JailBooking.Book), which re-checks the cell, the cuffs and the officer.
	//! \return "" when the sentence started, else the key the suspect reads.
	static string TakeTime(IEntity suspect)
	{
		string suspectUid = LF_Utils.GetPlayerUid(suspect);
		LF_BookingOffer offer = OfferFor(suspectUid);
		if (!offer)
		{
			LF_Debug.Warn(FEATURE, string.Format("take the time refused: %1 has no open offer", suspectUid));
			return "#LF-BookingDesk_NoOffer";
		}

		IEntity officer = GetGame().GetWorld().FindEntityByID(offer.m_OfficerId);
		if (!officer || LF_Utils.GetPlayerUid(officer) != offer.m_sOfficerUid)
		{
			LF_Debug.Warn(FEATURE, string.Format("take the time refused: booking officer %1 of %2 is gone", offer.m_sOfficerUid, suspectUid));
			Close(suspectUid, suspect);
			return "#LF-BookingDesk_OfficerGone";
		}

		if (!LF_JailBooking.Book(officer, suspect, offer.m_iPercent))
		{
			LF_Debug.Warn(FEATURE, string.Format("take the time: the jail refused %1, offer stays open", suspectUid));
			return "#LF-JailBook_Failed";
		}

		LF_Debug.Info(FEATURE, string.Format("take the time: %1 booked by %2 percent=%3", suspectUid, offer.m_sOfficerUid, offer.m_iPercent));
		Close(suspectUid, suspect);
		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the suspect chose "Pay $X now". The fine is the server's offer, never a client number.
	//! \return "" when paid, else the key the suspect reads.
	static string PayNow(IEntity suspect)
	{
		string suspectUid = LF_Utils.GetPlayerUid(suspect);
		LF_BookingOffer offer = OfferFor(suspectUid);
		if (!offer || !offer.m_bPayable)
		{
			LF_Debug.Warn(FEATURE, string.Format("pay now refused: %1 has no payable offer", suspectUid));
			return "#LF-BookingDesk_NoOffer";
		}

		// Paid from cash then bank to the town, or short and to the cell (LF_ChargePayment, #4427).
		string refusal = LF_ChargePayment.Settle(offer, suspect);
		if (!refusal.IsEmpty())
			return refusal;

		Close(suspectUid, suspect);
		return string.Empty;
	}
}

//------------------------------------------------------------------------------------------------
//! Client: the local player's open offer, as the server sent it (labels only; the server decides).
class LF_BookingDeskClient
{
	protected static bool s_bOpen;
	protected static int s_iFine;
	protected static int s_iSeconds;
	protected static bool s_bPayable;
	protected static int s_iOpenedAtMs;

	//------------------------------------------------------------------------------------------------
	static void Set(int fine, int seconds, bool payable, bool open)
	{
		s_bOpen = open;
		s_iFine = fine;
		s_iSeconds = seconds;
		s_bPayable = payable;
		s_iOpenedAtMs = System.GetTickCount();
	}

	//------------------------------------------------------------------------------------------------
	static bool IsOpen()
	{
		// The server lapses an unanswered offer; the labels go with it even if no close arrives.
		return s_bOpen && System.GetTickCount() - s_iOpenedAtMs <= LF_BookingDesk.OFFER_MS;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsPayable()
	{
		return IsOpen() && s_bPayable;
	}

	//------------------------------------------------------------------------------------------------
	static int Fine()
	{
		return s_iFine;
	}

	//------------------------------------------------------------------------------------------------
	static int Seconds()
	{
		return s_iSeconds;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		Set(0, 0, false, false);
	}
}

//------------------------------------------------------------------------------------------------
//! The offer travels to the suspect's own machine only: an owner RPC on their body.
modded class SCR_ChimeraCharacter
{
	//------------------------------------------------------------------------------------------------
	//! Server only.
	void LF_SendBookingOffer(int fine, int seconds, bool payable, bool open)
	{
		if (!Replication.IsServer())
			return;

		Rpc(RpcDo_LF_BookingOffer, fine, seconds, payable, open);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_LF_BookingOffer(int fine, int seconds, bool payable, bool open)
	{
		LF_BookingDeskClient.Set(fine, seconds, payable, open);
	}
}

//------------------------------------------------------------------------------------------------
class LF_BookingDesk_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_BookingDesk";
	}

	override void Apply()
	{
		LF_BookingDesk.ResetStatics();
		LF_BookingDeskClient.ResetStatics();
	}
}
