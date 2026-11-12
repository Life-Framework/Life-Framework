//------------------------------------------------------------------------------------------------
//! The telephone exchange (#147, docs/architecture/phone-calls-spike.md): the call table and its
//! rules, with no world, no players and no clock of its own. Phones are named by their key
//! (LF_Payphone.PhoneKey), players by their server player id. LF_PhoneCalls feeds it from the
//! world; LF_Test_PhoneExchange drives it directly. Server only, session-local: a restart drops
//! every call, as it drops the dispatch board.
//------------------------------------------------------------------------------------------------
enum LF_EPhoneCallState
{
	RINGING,
	CONNECTED,
	ENDED,
	MISSED
}

//! What a phone line shows to the people standing at it. Replicated on the line entity.
enum LF_EPhoneLineState
{
	IDLE,
	//! A call rings at this phone; someone may pick up.
	RINGING,
	//! The receiver is off the hook: this phone placed a call that is ringing out, or it is talking.
	BUSY
}

//! Why a call is over. NONE while it is live.
enum LF_EPhoneEnd
{
	NONE,
	HUNG_UP,
	WALKED_AWAY,
	//! The talker went down or died.
	DOWN,
	DISCONNECTED,
	TOO_LONG,
	PHONE_GONE,
	//! Rang out: nobody picked up in time.
	NO_ANSWER,
	//! Nobody was on duty for the service when the call was placed.
	NOBODY_ON_DUTY
}

enum LF_EPhoneRefusal
{
	OK,
	NOT_SERVER,
	BAD_CALLER,
	//! The player is already on a live call.
	ON_A_CALL,
	//! This phone is already in use.
	PHONE_BUSY,
	//! Every phone the call would ring is in use.
	LINE_BUSY,
	//! No phone in the world answers for the service.
	NO_LINE,
	NOT_RINGING,
	NOT_ON_DUTY,
	TOO_FAR,
	NOT_YOUR_CALL,
	//! LF_Payphone.UseAt refused (cooldown, coin, unknown service).
	PAYPHONE,
	//! Nobody on duty: the call was placed and missed at once.
	NOBODY_ON_DUTY,
	//! The shared comms rate limit (LF_RateLimit) refused it; the limiter told the player.
	TOO_FAST
}

//------------------------------------------------------------------------------------------------
//! One call. Phones are keys and players are server ids; nothing here leaves the server.
class LF_PhoneCall : Managed
{
	int m_iId;
	LF_EPhoneCallState m_eState;
	LF_EPhoneEnd m_eEnd;
	string m_sService;
	//! The phone the call was dialled from.
	string m_sFromPhone;
	//! The phones it rings while RINGING.
	ref array<string> m_aRingPhones = {};
	//! The phone that picked up.
	string m_sToPhone;
	int m_iCallerId;
	string m_sCallerUid;
	int m_iAnswererId;
	string m_sAnswererUid;
	//! Place of the caller's phone, for the event and the logs.
	string m_sPlace;
	bool m_bCallBack;
	float m_fStarted;
	float m_fConnected = -1;
	float m_fEnded = -1;

	//------------------------------------------------------------------------------------------------
	bool IsLive()
	{
		return m_eState == LF_EPhoneCallState.RINGING || m_eState == LF_EPhoneCallState.CONNECTED;
	}

	//------------------------------------------------------------------------------------------------
	//! Seconds from dialling to the pick-up, or to the end when nobody picked up.
	int RangSeconds()
	{
		float until = m_fConnected;
		if (until < 0)
			until = m_fEnded;
		if (until < 0)
			return 0;

		return Math.Round(until - m_fStarted);
	}

	//------------------------------------------------------------------------------------------------
	//! Seconds talked; 0 for a call nobody picked up or one still talking.
	int TalkSeconds()
	{
		if (m_fConnected < 0 || m_fEnded < 0)
			return 0;

		return Math.Round(m_fEnded - m_fConnected);
	}

	//------------------------------------------------------------------------------------------------
	//! Does this phone take part in the live call (dialled from, ringing, or picked up)?
	bool Uses(string phoneKey)
	{
		if (!IsLive() || phoneKey.IsEmpty())
			return false;

		if (m_sFromPhone == phoneKey || m_sToPhone == phoneKey)
			return true;

		return m_eState == LF_EPhoneCallState.RINGING && m_aRingPhones.Find(phoneKey) >= 0;
	}

	//------------------------------------------------------------------------------------------------
	//! The phone this player talks at, or empty when they are not a talker of the call.
	string PhoneOf(int playerId)
	{
		if (playerId <= 0)
			return string.Empty;

		if (playerId == m_iCallerId)
			return m_sFromPhone;

		if (playerId == m_iAnswererId)
			return m_sToPhone;

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	string Describe()
	{
		return string.Format("call %1 %2 %3", m_iId, typename.EnumToString(LF_EPhoneCallState, m_eState), m_sService);
	}
}

//------------------------------------------------------------------------------------------------
class LF_PhoneExchange : Managed
{
	//! Finished calls kept for the logs and the tests, oldest dropped first.
	static const int KEEP_FINISHED = 32; // lf-fixed: memory bound on a session table, not a gameplay knob

	protected ref array<ref LF_PhoneCall> m_aCalls = {};
	protected int m_iNextId = 1;

	//------------------------------------------------------------------------------------------------
	array<ref LF_PhoneCall> Calls()
	{
		return m_aCalls;
	}

	//------------------------------------------------------------------------------------------------
	LF_PhoneCall Find(int callId)
	{
		foreach (LF_PhoneCall call : m_aCalls)
		{
			if (call.m_iId == callId)
				return call;
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! The live call this player talks on, or null.
	LF_PhoneCall CallOfPlayer(int playerId)
	{
		if (playerId <= 0)
			return null;

		foreach (LF_PhoneCall call : m_aCalls)
		{
			if (call.IsLive() && (call.m_iCallerId == playerId || call.m_iAnswererId == playerId))
				return call;
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! The live call that uses this phone, or null.
	LF_PhoneCall CallAtPhone(string phoneKey)
	{
		foreach (LF_PhoneCall call : m_aCalls)
		{
			if (call.Uses(phoneKey))
				return call;
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! The call ringing at this phone, or null.
	LF_PhoneCall RingingAt(string phoneKey)
	{
		LF_PhoneCall call = CallAtPhone(phoneKey);
		if (call && call.m_eState == LF_EPhoneCallState.RINGING && call.m_aRingPhones.Find(phoneKey) >= 0)
			return call;

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! What the line at this phone shows, and who talks there (0 for nobody).
	LF_EPhoneLineState LineState(string phoneKey, out int talkerId)
	{
		talkerId = 0;
		LF_PhoneCall call = CallAtPhone(phoneKey);
		if (!call)
			return LF_EPhoneLineState.IDLE;

		if (call.m_sFromPhone == phoneKey)
		{
			talkerId = call.m_iCallerId;
			return LF_EPhoneLineState.BUSY;
		}

		if (call.m_sToPhone == phoneKey)
		{
			talkerId = call.m_iAnswererId;
			return LF_EPhoneLineState.BUSY;
		}

		return LF_EPhoneLineState.RINGING;
	}

	//------------------------------------------------------------------------------------------------
	//! Can this player dial from this phone to these phones? Fills the phones that would ring.
	LF_EPhoneRefusal CheckDial(int callerId, string fromPhone, notnull array<string> ringPhones, notnull array<string> outIdle)
	{
		outIdle.Clear();
		if (callerId <= 0 || fromPhone.IsEmpty())
			return LF_EPhoneRefusal.BAD_CALLER;

		if (CallOfPlayer(callerId))
			return LF_EPhoneRefusal.ON_A_CALL;

		if (CallAtPhone(fromPhone))
			return LF_EPhoneRefusal.PHONE_BUSY;

		foreach (string key : ringPhones)
		{
			if (key != fromPhone && !key.IsEmpty() && !CallAtPhone(key) && outIdle.Find(key) < 0)
				outIdle.Insert(key);
		}

		if (ringPhones.IsEmpty())
			return LF_EPhoneRefusal.NO_LINE;

		if (outIdle.IsEmpty())
			return LF_EPhoneRefusal.LINE_BUSY;

		return LF_EPhoneRefusal.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! A new call rings every idle phone in ringPhones. \return The call, or null with the refusal.
	LF_PhoneCall Dial(float now, int callerId, string callerUid, string fromPhone, string service, notnull array<string> ringPhones, bool callBack, out LF_EPhoneRefusal refusal)
	{
		array<string> idle = {};
		refusal = CheckDial(callerId, fromPhone, ringPhones, idle);
		if (refusal != LF_EPhoneRefusal.OK)
			return null;

		LF_PhoneCall call = NewCall(now, callerId, callerUid, fromPhone, service);
		call.m_aRingPhones.Copy(idle);
		call.m_bCallBack = callBack;
		return call;
	}

	//------------------------------------------------------------------------------------------------
	//! A call that never rang: nobody was on duty. It is MISSED from the start, for the log and the
	//! bus; it holds no phone.
	LF_PhoneCall Missed(float now, int callerId, string callerUid, string fromPhone, string service)
	{
		LF_PhoneCall call = NewCall(now, callerId, callerUid, fromPhone, service);
		Finish(call, now, LF_EPhoneEnd.NOBODY_ON_DUTY);
		return call;
	}

	//------------------------------------------------------------------------------------------------
	//! Someone picks up the ringing phone. mayAnswer is the caller's verdict on who they are (an
	//! on-duty officer at a dispatch phone; anyone at a ringing payphone).
	LF_PhoneCall Answer(float now, int playerId, string playerUid, string phoneKey, bool mayAnswer, out LF_EPhoneRefusal refusal)
	{
		LF_PhoneCall call = RingingAt(phoneKey);
		if (!call)
		{
			refusal = LF_EPhoneRefusal.NOT_RINGING;
			return null;
		}

		if (playerId <= 0)
		{
			refusal = LF_EPhoneRefusal.BAD_CALLER;
			return null;
		}

		if (CallOfPlayer(playerId))
		{
			refusal = LF_EPhoneRefusal.ON_A_CALL;
			return null;
		}

		if (!mayAnswer)
		{
			refusal = LF_EPhoneRefusal.NOT_ON_DUTY;
			return null;
		}

		refusal = LF_EPhoneRefusal.OK;
		call.m_eState = LF_EPhoneCallState.CONNECTED;
		call.m_sToPhone = phoneKey;
		call.m_iAnswererId = playerId;
		call.m_sAnswererUid = playerUid;
		call.m_fConnected = now;
		call.m_aRingPhones.Clear();
		return call;
	}

	//------------------------------------------------------------------------------------------------
	//! A talker puts the receiver down at their phone. \return The call that ended, or null.
	LF_PhoneCall HangUp(float now, int playerId, string phoneKey, out LF_EPhoneRefusal refusal)
	{
		LF_PhoneCall call = CallOfPlayer(playerId);
		if (!call || call.PhoneOf(playerId) != phoneKey)
		{
			refusal = LF_EPhoneRefusal.NOT_YOUR_CALL;
			return null;
		}

		refusal = LF_EPhoneRefusal.OK;
		Finish(call, now, LF_EPhoneEnd.HUNG_UP);
		return call;
	}

	//------------------------------------------------------------------------------------------------
	//! Ends a live call. A ringing call that rang out is MISSED; every other end is ENDED.
	//! \return False when the call was already over.
	bool Finish(notnull LF_PhoneCall call, float now, LF_EPhoneEnd why)
	{
		if (call.m_eEnd != LF_EPhoneEnd.NONE)
			return false;

		bool unanswered = call.m_eState == LF_EPhoneCallState.RINGING;
		if (unanswered && (why == LF_EPhoneEnd.NO_ANSWER || why == LF_EPhoneEnd.NOBODY_ON_DUTY))
			call.m_eState = LF_EPhoneCallState.MISSED;
		else
			call.m_eState = LF_EPhoneCallState.ENDED;

		call.m_eEnd = why;
		call.m_fEnded = now;
		call.m_aRingPhones.Clear();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The clock's part: rang out and talked too long. Fills the calls that changed.
	void Expire(float now, int ringSeconds, int maxCallSeconds, notnull array<LF_PhoneCall> outChanged)
	{
		outChanged.Clear();
		foreach (LF_PhoneCall call : m_aCalls)
		{
			if (call.m_eState == LF_EPhoneCallState.RINGING && now - call.m_fStarted >= ringSeconds)
				outChanged.Insert(call);
			else if (call.m_eState == LF_EPhoneCallState.CONNECTED && now - call.m_fConnected >= maxCallSeconds)
				outChanged.Insert(call);
		}

		foreach (LF_PhoneCall call : outChanged)
		{
			if (call.m_eState == LF_EPhoneCallState.RINGING)
				Finish(call, now, LF_EPhoneEnd.NO_ANSWER);
			else
				Finish(call, now, LF_EPhoneEnd.TOO_LONG);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Every phone key a live call uses, for the line sync.
	void LivePhones(notnull array<string> outKeys)
	{
		outKeys.Clear();
		foreach (LF_PhoneCall call : m_aCalls)
		{
			if (!call.IsLive())
				continue;

			if (!call.m_sFromPhone.IsEmpty() && outKeys.Find(call.m_sFromPhone) < 0)
				outKeys.Insert(call.m_sFromPhone);
			if (!call.m_sToPhone.IsEmpty() && outKeys.Find(call.m_sToPhone) < 0)
				outKeys.Insert(call.m_sToPhone);
			foreach (string key : call.m_aRingPhones)
			{
				if (outKeys.Find(key) < 0)
					outKeys.Insert(key);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	protected LF_PhoneCall NewCall(float now, int callerId, string callerUid, string fromPhone, string service)
	{
		// Pruned here, never inside Finish: a caller may still hold a call it just finished.
		Prune();
		LF_PhoneCall call = new LF_PhoneCall();
		call.m_iId = m_iNextId;
		m_iNextId++;
		call.m_eState = LF_EPhoneCallState.RINGING;
		call.m_sService = service;
		call.m_sFromPhone = fromPhone;
		call.m_iCallerId = callerId;
		call.m_sCallerUid = callerUid;
		call.m_fStarted = now;
		m_aCalls.Insert(call);
		return call;
	}

	//------------------------------------------------------------------------------------------------
	//! Drops the oldest finished calls above KEEP_FINISHED. Live calls are never dropped.
	protected void Prune()
	{
		int finished;
		foreach (LF_PhoneCall call : m_aCalls)
		{
			if (!call.IsLive())
				finished++;
		}

		int i;
		while (i < m_aCalls.Count() && finished > KEEP_FINISHED)
		{
			if (m_aCalls[i].IsLive())
			{
				i++;
				continue;
			}

			m_aCalls.RemoveOrdered(i);
			finished--;
		}
	}
}
