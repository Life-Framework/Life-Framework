//------------------------------------------------------------------------------------------------
//! One sheet an officer has open at the desk (#4426): which suspect, which desk, and the codes the
//! suspect has open reports for. Server only. The officer's rank and duty are never kept here: they
//! are read from the server's account at open and again at sign.
class LF_ChargeSheetPending : Managed
{
	string m_sOfficerUid;
	string m_sSuspectUid;
	EntityID m_SuspectId;
	vector m_vDesk;
	//! The code ids the suspect has open reports for, resolved in the catalog. A signature may only tick these.
	ref array<string> m_aOpen = {};
	int m_iMadeAtMs;
}

//------------------------------------------------------------------------------------------------
//! The server's half of the charge sheet (#4426). `Open` checks the officer and suspect, keeps the
//! open codes and sends them (with the officer's rank cap) to the officer's screen. `Sign` takes the
//! ticked ids back, keeps only those the suspect really has open, and makes the booking offer through
//! `LF_BookingDesk.Offer`, which recomputes every number with `LF_ChargeSheetRules` from the server's own
//! account. A total, a rank or a duty flag the client claims is never read.
class LF_ChargeSheetDesk
{
	static const string FEATURE = "Law";
	//! An unsigned sheet lapses after this long.
	static const int SHEET_MS = 300000;	// lf-fixed: five minutes at the desk, as the offer
	//! The most ids one signature may name (a client could send anything).
	static const int MAX_TICKED = 24;	// lf-fixed: mirrors the paper's row limit

	protected static ref map<string, ref LF_ChargeSheetPending> s_mPending;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_mPending = null;
	}

	//------------------------------------------------------------------------------------------------
	protected static map<string, ref LF_ChargeSheetPending> Sheets()
	{
		if (!s_mPending)
			s_mPending = new map<string, ref LF_ChargeSheetPending>();

		return s_mPending;
	}

	//------------------------------------------------------------------------------------------------
	//! The officer's open sheet, or null (a lapsed one is dropped here).
	static LF_ChargeSheetPending PendingFor(string officerUid)
	{
		if (officerUid.IsEmpty() || !s_mPending)
			return null;

		LF_ChargeSheetPending pending = s_mPending.Get(officerUid);
		if (!pending)
			return null;

		if (System.GetTickCount() - pending.m_iMadeAtMs > SHEET_MS)
		{
			LF_Debug.Info(FEATURE, string.Format("charge sheet of %1 lapsed unsigned", officerUid));
			s_mPending.Remove(officerUid);
			return null;
		}

		return pending;
	}

	//------------------------------------------------------------------------------------------------
	//! The open code ids a suspect can be charged with, in report order, each once.
	static void OpenCodeIds(string suspectUid, notnull array<string> outIds)
	{
		outIds.Clear();
		array<string> offences = {};
		array<ref LF_WantedReportRecord> reports = LF_WantedReports.GetFor(suspectUid);
		if (reports)
		{
			foreach (LF_WantedReportRecord report : reports)
			{
				if (report && !report.m_sOffence.IsEmpty() && !offences.Contains(report.m_sOffence))
					offences.Insert(report.m_sOffence);
			}
		}

		array<LF_CrimeEntry> codes = {};
		LF_LawCode.Resolve(offences, codes);
		foreach (LF_CrimeEntry code : codes)
		{
			outIds.Insert(code.m_sCrimeId);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server: keep a sheet for an officer and suspect. The duty and rank come from the server's account.
	//! \param[out] capOut The officer's rank cap, to show on the paper.
	//! \return "" when the sheet was made, else the key the officer reads.
	static string Prepare(string officerUid, vector deskOrigin, string suspectUid, IEntity suspect, out int capOut, out string idsOut)
	{
		LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();
		LF_PlayerAccount account;
		if (accounts && !officerUid.IsEmpty())
			account = accounts.GetAccount(officerUid);

		if (!LF_PoliceUtils.IsOnDutyOfficer(account))
		{
			LF_Debug.Warn(FEATURE, string.Format("charge sheet refused: officer %1 is not on duty", officerUid));
			return "#LF-BookingDesk_NotOnDuty";
		}

		if (suspectUid.IsEmpty() || suspectUid == officerUid)
		{
			LF_Debug.Warn(FEATURE, string.Format("charge sheet refused: officer %1 named no suspect", officerUid));
			return "#LF-BookingDesk_NoSuspect";
		}

		array<string> ids = {};
		OpenCodeIds(suspectUid, ids);
		if (ids.IsEmpty())
		{
			LF_Debug.Warn(FEATURE, string.Format("charge sheet refused: %1 has no open charge", suspectUid));
			return "#LF-BookingDesk_NoCharges";
		}

		LF_ChargeSheetPending pending = new LF_ChargeSheetPending();
		pending.m_sOfficerUid = officerUid;
		pending.m_sSuspectUid = suspectUid;
		if (suspect)
			pending.m_SuspectId = suspect.GetID();
		else
			pending.m_SuspectId = EntityID.INVALID;

		pending.m_vDesk = deskOrigin;
		pending.m_aOpen.Copy(ids);
		pending.m_iMadeAtMs = System.GetTickCount();
		Sheets().Set(officerUid, pending);
		capOut = LF_PoliceRanks.FineCap(account.GetPoliceRank());
		idsOut = string.Empty;
		foreach (string id : ids)
		{
			if (!idsOut.IsEmpty())
				idsOut += ",";

			idsOut += id;
		}

		LF_Debug.Info(FEATURE, string.Format("charge sheet opened: officer %1, suspect %2, %3 open code(s), cap $%4", officerUid, suspectUid, ids.Count(), capOut));
		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! Server, from the desk action: open the sheet for the cuffed suspect beside the officer.
	//! \return "" when the sheet went to the officer's screen, else the key the officer reads.
	static string Open(IEntity officer, vector deskOrigin, RplId suspectId)
	{
		if (!Replication.IsServer() || !officer)
			return "#LF-BookingDesk_NoSuspect";

		string officerUid = LF_Utils.GetPlayerUid(officer);
		IEntity suspect = LF_NetworkUtils.FindEntityByRplId(suspectId);
		string refusal;
		if (!SCR_ChimeraCharacter.Cast(suspect) || suspect == officer)
			refusal = "#LF-BookingDesk_NoSuspect";
		else if (vector.Distance(suspect.GetOrigin(), officer.GetOrigin()) > LF_PoliceConfig.ArrestReach())
			refusal = "#LF-BookingDesk_NoSuspect";
		else if (!LF_Restraint.IsRestrained(suspect))
			refusal = "#LF-JailBook_CuffFirst";

		if (!refusal.IsEmpty())
		{
			LF_Debug.Warn(FEATURE, string.Format("charge sheet refused: officer %1 asked for suspect rpl %2 (%3)", officerUid, suspectId, refusal));
			return refusal;
		}

		int cap;
		string ids;
		refusal = Prepare(officerUid, deskOrigin, LF_Utils.GetPlayerUid(suspect), suspect, cap, ids);
		if (!refusal.IsEmpty())
			return refusal;

		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(LF_PlayerInteraction.PlayerIdOf(officer)));
		if (!controller)
		{
			LF_Debug.Warn(FEATURE, string.Format("charge sheet: officer %1 has no controller to show the paper", officerUid));
			Sheets().Remove(officerUid);
			return "#LF-BookingDesk_NoSuspect";
		}

		controller.LF_SendChargeSheet(ids, cap);
		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! The ids a signature may charge: the client's list, each id once, only those the sheet holds open.
	static void KeepTicked(notnull LF_ChargeSheetPending pending, string csv, notnull array<string> outTicked)
	{
		outTicked.Clear();
		array<string> sent = {};
		csv.Split(",", sent, true);
		foreach (string id : sent)
		{
			if (outTicked.Count() >= MAX_TICKED)
				break;

			if (!pending.m_aOpen.Contains(id))
			{
				LF_Debug.Warn(FEATURE, string.Format("charge sheet: officer %1 signed '%2', which is not open against the suspect, dropped", pending.m_sOfficerUid, id));
				continue;
			}

			if (!outTicked.Contains(id))
				outTicked.Insert(id);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the officer signed. The codes are checked against the open sheet and the offer is made by
	//! the booking desk, which recomputes the fine and time with the server's account for `officerUid`.
	//! `claimedFine` is only logged when it differs.
	//! \return "" when the offer was made, else the key the officer reads.
	static string Sign(string officerUid, vector officerOrigin, string csv, int claimedFine, IEntity officer, IEntity suspect)
	{
		LF_ChargeSheetPending pending = PendingFor(officerUid);
		if (!pending)
		{
			LF_Debug.Warn(FEATURE, string.Format("charge sheet sign refused: officer %1 has no open sheet", officerUid));
			return "#LF-ChargeSheet_NoSheet";
		}

		array<string> ticked = {};
		KeepTicked(pending, csv, ticked);
		if (ticked.IsEmpty())
		{
			LF_Debug.Warn(FEATURE, string.Format("charge sheet sign refused: officer %1 ticked no open charge", officerUid));
			return "#LF-ChargeSheet_NoneTicked";
		}

		string refusal = LF_BookingDesk.Offer(officerUid, officerOrigin, pending.m_vDesk, pending.m_sSuspectUid, officer, suspect, ticked);
		if (!refusal.IsEmpty())
			return refusal;

		Sheets().Remove(officerUid);
		LF_BookingOffer offer = LF_BookingDesk.OfferFor(pending.m_sSuspectUid);
		if (offer && offer.m_iFine != claimedFine)
			LF_Debug.Warn(FEATURE, string.Format("charge sheet: officer %1's paper showed $%2, the server's sheet is $%3; the server's number stands", officerUid, claimedFine, offer.m_iFine));

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! Server, from the signing RPC: resolve the officer's own body and the suspect again, then sign.
	static void SignFromOfficer(IEntity officer, string csv, int claimedFine)
	{
		if (!Replication.IsServer() || !officer)
			return;

		string officerUid = LF_Utils.GetPlayerUid(officer);
		string refusal;
		IEntity suspect;
		LF_ChargeSheetPending pending = PendingFor(officerUid);
		if (pending && pending.m_SuspectId != EntityID.INVALID && GetGame().GetWorld())
			suspect = GetGame().GetWorld().FindEntityByID(pending.m_SuspectId);

		if (pending && (!suspect || LF_Utils.GetPlayerUid(suspect) != pending.m_sSuspectUid || vector.Distance(suspect.GetOrigin(), officer.GetOrigin()) > LF_PoliceConfig.ArrestReach() || !LF_Restraint.IsRestrained(suspect)))
		{
			LF_Debug.Warn(FEATURE, string.Format("charge sheet sign refused: the suspect of %1 is gone, away or free", officerUid));
			Sheets().Remove(officerUid);
			refusal = "#LF-BookingDesk_NoSuspect";
		}
		else
		{
			refusal = Sign(officerUid, officer.GetOrigin(), csv, claimedFine, officer, suspect);
		}

		int officerId = LF_PlayerInteraction.PlayerIdOf(officer);
		if (!refusal.IsEmpty() && officerId > 0)
			LF_NotificationManagerComponent.NotifyPlayer(officerId, "#LF-BookingDesk_Title", refusal, 5.0, LF_ENotificationType.WARNING);
	}
}

//------------------------------------------------------------------------------------------------
//! The paper travels to the officer's own machine (owner RPC) and the signature back (server RPC).
modded class SCR_PlayerController
{
	//------------------------------------------------------------------------------------------------
	//! Server only: show the officer the sheet for the suspect at the desk.
	void LF_SendChargeSheet(string ids, int fineCap)
	{
		if (!Replication.IsServer())
			return;

		Rpc(RpcDo_LF_ChargeSheet, ids, fineCap);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_LF_ChargeSheet(string ids, int fineCap)
	{
		LF_ChargeSheetScreen.OpenFromServer(ids, fineCap);
	}

	//------------------------------------------------------------------------------------------------
	//! Client -> server: the officer signed. The payload is the ticked ids and the total the paper showed;
	//! the server reads only the ids.
	void LF_AskSignChargeSheet(string ids, int shownFine)
	{
		if (Replication.IsServer())
			RpcAsk_LF_SignChargeSheet(ids, shownFine);
		else
			Rpc(RpcAsk_LF_SignChargeSheet, ids, shownFine);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_SignChargeSheet(string ids, int shownFine)
	{
		IEntity officer = GetControlledEntity();
		if (!officer)
		{
			LF_Debug.Warn(LF_ChargeSheetDesk.FEATURE, "charge sheet signature dropped: the controller has no body");
			return;
		}

		LF_ChargeSheetDesk.SignFromOfficer(officer, ids, shownFine);
	}
}

//------------------------------------------------------------------------------------------------
class LF_ChargeSheetDesk_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_ChargeSheetDesk";
	}

	override void Apply()
	{
		LF_ChargeSheetDesk.ResetStatics();
	}
}
