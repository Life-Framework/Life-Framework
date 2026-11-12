//------------------------------------------------------------------------------------------------
//! Call-back requests (#147), the stand-in for voicemail: script cannot record a voice
//! (docs/architecture/phone-calls-spike.md, "Voicemail verdict"). A caller whose call rang out, or
//! found nobody on duty, may leave a request at that telephone for a while (the offer), with an
//! optional short typed message. Officers read the requests at the station telephone and ring one
//! back. Requests expire with the dispatch board and are never saved: a restart clears them, as it
//! clears the board.
//!
//! This is the book alone: no world, no players, no clock of its own. LF_PhoneCallBacks feeds it;
//! LF_Test_PhoneCallBackBook drives it.
//------------------------------------------------------------------------------------------------
enum LF_EPhoneRequestRefusal
{
	OK,
	//! No offer for this player at this telephone (no missed call, the offer ran out, another phone).
	NO_OFFER,
	TOO_LONG,
	NOT_FOUND,
	//! Someone is already ringing that telephone back.
	CALLING
}

//------------------------------------------------------------------------------------------------
//! A caller may leave a request at this phone until m_fUntil.
class LF_PhoneCallBackOffer : Managed
{
	string m_sCallerUid;
	int m_iCallerId;
	string m_sPhoneKey;
	string m_sService;
	string m_sPlace;
	int m_iRangSeconds;
	float m_fUntil;
}

//------------------------------------------------------------------------------------------------
class LF_PhoneCallBackRequest : Managed
{
	int m_iId;
	string m_sService;
	string m_sCallerUid;
	string m_sPhoneKey;
	string m_sPlace;
	//! In-game clock when it was left, "HH:MM".
	string m_sTime;
	int m_iRangSeconds;
	string m_sMessage;
	float m_fLeft;
	float m_fExpires;
	//! The call ringing that phone back now; 0 when nobody is.
	int m_iCallId;
}

//------------------------------------------------------------------------------------------------
class LF_PhoneCallBackBook : Managed
{
	static const string FIELD_SEPARATOR = "|";

	protected ref array<ref LF_PhoneCallBackOffer> m_aOffers = {};
	protected ref array<ref LF_PhoneCallBackRequest> m_aRequests = {};
	protected int m_iNextId = 1;

	//------------------------------------------------------------------------------------------------
	//! Pure: a typed message made safe to store, list and log: trimmed, one line, and without the
	//! characters the pad and the UI treat as markup or separators. Length is checked separately.
	static string Clean(string text)
	{
		string clean = text;
		array<string> drop = {FIELD_SEPARATOR, "<", ">", "~", "\n", "\r", "\t"};
		foreach (string c : drop)
		{
			clean.Replace(c, " ");
		}

		clean.TrimInPlace();
		while (clean.Contains("  "))
		{
			clean.Replace("  ", " ");
		}
		return clean;
	}

	//------------------------------------------------------------------------------------------------
	array<ref LF_PhoneCallBackRequest> Requests()
	{
		return m_aRequests;
	}

	//------------------------------------------------------------------------------------------------
	//! A missed call offers its caller a request at that phone; a newer offer replaces theirs.
	LF_PhoneCallBackOffer Offer(float now, string callerUid, int callerId, string phoneKey, string service, string place, int rangSeconds, int offerSeconds)
	{
		DropOffer(callerUid);
		LF_PhoneCallBackOffer offer = new LF_PhoneCallBackOffer();
		offer.m_sCallerUid = callerUid;
		offer.m_iCallerId = callerId;
		offer.m_sPhoneKey = phoneKey;
		offer.m_sService = service;
		offer.m_sPlace = place;
		offer.m_iRangSeconds = rangSeconds;
		offer.m_fUntil = now + offerSeconds;
		m_aOffers.Insert(offer);
		return offer;
	}

	//------------------------------------------------------------------------------------------------
	//! The live offer of this caller at this phone, or null.
	LF_PhoneCallBackOffer OfferFor(float now, string callerUid, string phoneKey)
	{
		foreach (LF_PhoneCallBackOffer offer : m_aOffers)
		{
			if (offer.m_sCallerUid == callerUid && offer.m_sPhoneKey == phoneKey && now < offer.m_fUntil)
				return offer;
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Player id of whoever holds an offer at this phone (newest first); 0 for nobody. Purge drops
	//! the run-out ones.
	int OfferedAt(string phoneKey)
	{
		for (int i = m_aOffers.Count() - 1; i >= 0; i--)
		{
			if (m_aOffers[i].m_sPhoneKey == phoneKey)
				return m_aOffers[i].m_iCallerId;
		}
		return 0;
	}

	//------------------------------------------------------------------------------------------------
	void DropOffer(string callerUid)
	{
		for (int i = m_aOffers.Count() - 1; i >= 0; i--)
		{
			if (m_aOffers[i].m_sCallerUid == callerUid)
				m_aOffers.Remove(i);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The caller leaves a request on their offer. message is already Clean()ed; an empty one is
	//! fine (the pad shows where and when). One open request per caller: a new one replaces theirs.
	//! The oldest request goes when the book is full.
	LF_PhoneCallBackRequest Leave(float now, string callerUid, string phoneKey, string message, string clockText, int maxChars, int expirySeconds, int maxRequests, out LF_EPhoneRequestRefusal refusal)
	{
		LF_PhoneCallBackOffer offer = OfferFor(now, callerUid, phoneKey);
		if (!offer)
		{
			refusal = LF_EPhoneRequestRefusal.NO_OFFER;
			return null;
		}

		if (message.Length() > maxChars)
		{
			refusal = LF_EPhoneRequestRefusal.TOO_LONG;
			return null;
		}

		for (int i = m_aRequests.Count() - 1; i >= 0; i--)
		{
			if (m_aRequests[i].m_sCallerUid == callerUid && m_aRequests[i].m_iCallId == 0)
				m_aRequests.RemoveOrdered(i);
		}

		while (m_aRequests.Count() >= Math.Max(maxRequests, 1))
		{
			m_aRequests.RemoveOrdered(0);
		}

		LF_PhoneCallBackRequest request = new LF_PhoneCallBackRequest();
		request.m_iId = m_iNextId;
		m_iNextId++;
		request.m_sService = offer.m_sService;
		request.m_sCallerUid = callerUid;
		request.m_sPhoneKey = phoneKey;
		request.m_sPlace = offer.m_sPlace;
		request.m_sTime = clockText;
		request.m_iRangSeconds = offer.m_iRangSeconds;
		request.m_sMessage = message;
		request.m_fLeft = now;
		request.m_fExpires = now + expirySeconds;
		m_aRequests.Insert(request);
		DropOffer(callerUid);
		refusal = LF_EPhoneRequestRefusal.OK;
		return request;
	}

	//------------------------------------------------------------------------------------------------
	LF_PhoneCallBackRequest Find(int requestId)
	{
		foreach (LF_PhoneCallBackRequest request : m_aRequests)
		{
			if (request.m_iId == requestId)
				return request;
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! A request an officer of service may ring back now, or null with the refusal.
	LF_PhoneCallBackRequest Claimable(float now, int requestId, string service, out LF_EPhoneRequestRefusal refusal)
	{
		LF_PhoneCallBackRequest request = Find(requestId);
		if (!request || request.m_sService != service || now >= request.m_fExpires)
		{
			refusal = LF_EPhoneRequestRefusal.NOT_FOUND;
			return null;
		}

		if (request.m_iCallId != 0)
		{
			refusal = LF_EPhoneRequestRefusal.CALLING;
			return null;
		}

		refusal = LF_EPhoneRequestRefusal.OK;
		return request;
	}

	//------------------------------------------------------------------------------------------------
	//! The call ringing a request's phone back changed. Picked up: the request is done and leaves
	//! the pad. Over without a pick-up: the request is open again. \return The request, or null.
	LF_PhoneCallBackRequest OnCallBack(int callId, bool connected, bool over)
	{
		if (callId <= 0)
			return null;

		foreach (int i, LF_PhoneCallBackRequest request : m_aRequests)
		{
			if (request.m_iCallId != callId)
				continue;

			if (connected)
				m_aRequests.RemoveOrdered(i);
			else if (over)
				request.m_iCallId = 0;

			return request;
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Drops run-out offers and expired requests (never one being rung back). Fills what expired.
	//! \return True when an offer ran out (a line may need to drop its offer).
	bool Purge(float now, notnull array<ref LF_PhoneCallBackRequest> outExpired)
	{
		outExpired.Clear();
		bool offers;
		for (int i = m_aOffers.Count() - 1; i >= 0; i--)
		{
			if (now >= m_aOffers[i].m_fUntil)
			{
				m_aOffers.Remove(i);
				offers = true;
			}
		}

		for (int i = m_aRequests.Count() - 1; i >= 0; i--)
		{
			if (now >= m_aRequests[i].m_fExpires && m_aRequests[i].m_iCallId == 0)
			{
				outExpired.Insert(m_aRequests[i]);
				m_aRequests.RemoveOrdered(i);
			}
		}
		return offers;
	}

	//------------------------------------------------------------------------------------------------
	//! The pad of a service, oldest first, as "id|time|rang|calling|place|message" entries.
	int Entries(float now, string service, notnull array<string> outEntries)
	{
		outEntries.Clear();
		foreach (LF_PhoneCallBackRequest request : m_aRequests)
		{
			if (request.m_sService != service || now >= request.m_fExpires)
				continue;

			int calling;
			if (request.m_iCallId != 0)
				calling = 1;

			outEntries.Insert(string.Format("%1|%2|%3|%4|%5|%6", request.m_iId, request.m_sTime, request.m_iRangSeconds, calling, request.m_sPlace, request.m_sMessage));
		}
		return outEntries.Count();
	}
}
