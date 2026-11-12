//------------------------------------------------------------------------------------------------
//! Comms events on the server bus (docs/architecture/events.md). Fields are plain values: uids and
//! ints, never IEntity or EntityID. Published on the server only, after the state change happened.
//! A new event of this feature is appended to this file, not to a central list.

//! A player called a service (police, medic, taxi, ...) and at least one receiver is on duty for it.
//! Dispatch (#250) and any delivery channel subscribe; the caller does not know who listens.
//! m_aReceiverIds are server-side player ids, never sent to clients.
class LF_OnServiceCall : LF_Event
{
	string m_sCallerUid;
	string m_sService;
	//! Place key or grid text the call came from (a payphone's place).
	string m_sPlace;
	//! What happened, a preset key or free text.
	string m_sSummary;
	//! Where the call is delivered: comma list of pager, board, radio.
	string m_sChannels;
	//! World position of the caller's phone; zero when unknown.
	vector m_vPosition;
	ref array<int> m_aReceiverIds = {};
	//! Suspect description snapshot when the call is a crime report (LF_Dispatch.Report); empty for a witness call.
	string m_sClothing;
	string m_sVehicle;
	//! On-duty officers when the call was placed, when that differs from the receiver count; -1 means use the receivers.
	int m_iOnDuty = -1;

	static LF_OnServiceCall Create(string callerUid, string service, string place, string summary, string channels, vector position, array<int> receivers)
	{
		LF_OnServiceCall e = new LF_OnServiceCall();
		e.m_sCallerUid = callerUid;
		e.m_sService = service;
		e.m_sPlace = place;
		e.m_sSummary = summary;
		e.m_sChannels = channels;
		e.m_vPosition = position;
		if (receivers)
			e.m_aReceiverIds.Copy(receivers);
		return e;
	}
}

//! A responder took a service call (#251). Published by LF_ServiceTake.ServerTake after the server
//! validated that the taker is on duty for the service and the call had room.
class LF_OnServiceCallTaken : LF_Event
{
	string m_sCallerUid;
	string m_sService;
	string m_sTakerUid;
	string m_sPlace;
	string m_sSummary;
	//! Takers so far, this one included.
	int m_iTakers;

	static LF_OnServiceCallTaken Create(string callerUid, string service, string takerUid, string place, string summary, int takers)
	{
		LF_OnServiceCallTaken e = new LF_OnServiceCallTaken();
		e.m_sCallerUid = callerUid;
		e.m_sService = service;
		e.m_sTakerUid = takerUid;
		e.m_sPlace = place;
		e.m_sSummary = summary;
		e.m_iTakers = takers;
		return e;
	}
}

//! A police scanner picked up a service call (#253). Published by LF_Scanner for each scanner
//! carrier who is not a receiver of the call, so a criminal listening in leaves a trace on the bus.
class LF_OnScannerHeard : LF_Event
{
	string m_sListenerUid;
	string m_sService;
	string m_sSummary;
	string m_sPlace;

	static LF_OnScannerHeard Create(string listenerUid, string service, string summary, string place)
	{
		LF_OnScannerHeard e = new LF_OnScannerHeard();
		e.m_sListenerUid = listenerUid;
		e.m_sService = service;
		e.m_sSummary = summary;
		e.m_sPlace = place;
		return e;
	}
}

//! A telephone call changed state (#147): it started ringing, was picked up, ended, or rang out.
//! Published by LF_PhoneCalls on the server once per transition, after the call table changed.
//! m_eState is LF_EPhoneCallState and m_eEnd is LF_EPhoneEnd (NONE until the call is over).
//! Player ids stay on the server; subscribers key on the uids.
class LF_OnPhoneCall : LF_Event
{
	int m_iCallId;
	int m_eState;
	int m_eEnd;
	string m_sService;
	string m_sCallerUid;
	//! Empty until someone picks up.
	string m_sAnswererUid;
	//! Place of the phone the call came from (LF_WantedReports.DescribePlace).
	string m_sPlace;
	//! The call rings a payphone back from the station (a call-back) rather than the station.
	bool m_bCallBack;
	int m_iRangSeconds;
	int m_iTalkSeconds;

	static LF_OnPhoneCall Create(notnull LF_PhoneCall call)
	{
		LF_OnPhoneCall e = new LF_OnPhoneCall();
		e.m_iCallId = call.m_iId;
		e.m_eState = call.m_eState;
		e.m_eEnd = call.m_eEnd;
		e.m_sService = call.m_sService;
		e.m_sCallerUid = call.m_sCallerUid;
		e.m_sAnswererUid = call.m_sAnswererUid;
		e.m_sPlace = call.m_sPlace;
		e.m_bCallBack = call.m_bCallBack;
		e.m_iRangSeconds = call.RangSeconds();
		e.m_iTalkSeconds = call.TalkSeconds();
		return e;
	}
}

//! A call-back request changed (#147): m_sKind is "left" (a caller left it at a payphone), "rung"
//! (an officer rings that payphone back), "answered" (someone picked up the call-back, it leaves
//! the pad) or "expired" (it ran out with the dispatch board, unanswered). Published by
//! LF_PhoneCallBacks on the server. m_sMessage is the caller's typed text, already cleaned and
//! length-checked; it is logged like chat.
class LF_OnPhoneCallBack : LF_Event
{
	string m_sKind;
	int m_iRequestId;
	string m_sService;
	string m_sCallerUid;
	//! Empty unless an officer rang it back.
	string m_sOfficerUid;
	string m_sPlace;
	string m_sMessage;

	static LF_OnPhoneCallBack Create(string kind, int requestId, string service, string callerUid, string officerUid, string place, string message)
	{
		LF_OnPhoneCallBack e = new LF_OnPhoneCallBack();
		e.m_sKind = kind;
		e.m_iRequestId = requestId;
		e.m_sService = service;
		e.m_sCallerUid = callerUid;
		e.m_sOfficerUid = officerUid;
		e.m_sPlace = place;
		e.m_sMessage = message;
		return e;
	}
}
