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
