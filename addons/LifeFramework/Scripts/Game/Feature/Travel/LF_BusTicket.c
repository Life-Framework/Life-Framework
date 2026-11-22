// LF_BusTicket.c - the bus at a stop, as the player's own machine sees it: one vanilla action per other
// stop on the sign ("Bus to Harbour: $18, 20 s", nearest first), greyed with the reason when this
// machine already knows the server would refuse (a ticket waiting, the cooldown, the police close by
// while wanted, too little cash), and one "Cancel trip" action while a ticket waits. No window: the
// answers are a hint and the vanilla toasts. The fares shown are priced from the same config the
// server charges by; the server checks every rule again and its price is the one paid.

//------------------------------------------------------------------------------------------------
//! One destination as an action offers it.
class LF_BusOffer
{
	string m_sToId;
	string m_sLabel;
	int m_iFare;
	int m_iWait;
}

//------------------------------------------------------------------------------------------------
class LF_BusTicket
{
	//! Destination actions a stop prefab carries (LF_BusStopAction.m_iSlot 0 .. MAX_SLOTS - 1).
	static const int MAX_SLOTS = 12;	// lf-fixed: the prefab's action count; more stops than this are left off the list
	//! A stop's offers are worked out again at most this often while its actions are on screen.
	static const int OFFER_CACHE_MS = 1000;	// lf-fixed: the list is re-read every frame; stops never move

	protected static bool s_bWaiting;
	protected static bool s_bAsking;
	protected static float s_fDepartAt;
	protected static int s_iFare;
	//! World seconds of the last ask and arrival; the flags say whether there was one.
	protected static float s_fAskedAt;
	protected static bool s_bAskedOnce;
	protected static float s_fArrivedAt;
	protected static bool s_bArrivedOnce;

	protected static EntityID s_CachedStop;
	protected static int s_iCachedAt;
	protected static ref array<ref LF_BusOffer> s_aCached;

	//------------------------------------------------------------------------------------------------
	//! Pure: every other stop as an offer, nearest first. A stop with no LF_BusStopComponent, or one with
	//! this stop's id, is left out.
	static void Offers(notnull IEntity from, notnull array<IEntity> stops, LF_BusStopConfig config, notnull array<ref LF_BusOffer> into)
	{
		LF_BusStopComponent here = LF_BusStopComponent.Of(from);
		string fromId;
		if (here)
			fromId = here.GetStopId();

		vector origin = from.GetOrigin();
		array<float> distances = {};
		foreach (IEntity stop : stops)
		{
			if (!stop || stop == from)
				continue;

			LF_BusStopComponent component = LF_BusStopComponent.Of(stop);
			if (!component || component.GetStopId() == fromId)
				continue;

			float metres = vector.Distance(origin, stop.GetOrigin());
			LF_BusOffer offer = new LF_BusOffer();
			offer.m_sToId = component.GetStopId();
			offer.m_sLabel = component.GetLabel();
			offer.m_iFare = LF_BusStop.Fare(config, metres);
			offer.m_iWait = Math.Round(LF_BusStop.WaitSeconds(config, metres));

			int at = into.Count();
			for (int i = 0; i < distances.Count(); i++)
			{
				if (metres < distances[i])
				{
					at = i;
					break;
				}
			}

			into.InsertAt(offer, at);
			distances.InsertAt(metres, at);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: why a destination action is greyed, as far as this machine can tell; NONE lets it be pressed
	//! (the server decides the rest). Order: a ticket already waiting, the cooldowns, the police, the cash.
	static LF_EBusRefusal ActionRefusal(LF_BusStopConfig config, bool waiting, float askedAt, float arrivedAt, float now, int wanted, bool officerNear, int cash, int fare)
	{
		if (waiting)
			return LF_EBusRefusal.BUSY;

		if (LF_BusStop.IsTooSoon(config, askedAt, arrivedAt, now))
			return LF_EBusRefusal.TOO_SOON;

		if (wanted > 0 && officerNear)
			return LF_EBusRefusal.PURSUIT;

		if (fare > 0 && cash < fare)
			return LF_EBusRefusal.NO_CASH;

		return LF_EBusRefusal.NONE;
	}

	//------------------------------------------------------------------------------------------------
	//! The short reason under a greyed action.
	static string ReasonKey(LF_EBusRefusal refusal)
	{
		switch (refusal)
		{
			case LF_EBusRefusal.BUSY: return "#LF-BusStop_WhyBusy";
			case LF_EBusRefusal.TOO_SOON: return "#LF-BusStop_WhyTooSoon";
			case LF_EBusRefusal.PURSUIT: return "#LF-BusStop_WhyPursuit";
			case LF_EBusRefusal.NO_CASH: return "#LF-BusStop_WhyNoCash";
		}

		return LF_BusStop.RefusalKey(refusal);
	}

	//------------------------------------------------------------------------------------------------
	//! "Bus to Harbour: $18, 20 s".
	static string ActionName(notnull LF_BusOffer offer)
	{
		return LF_Terms.Text("#LF-BusStop_ActionTo", WidgetManager.Translate(offer.m_sLabel), offer.m_iFare.ToString(), offer.m_iWait.ToString());
	}

	//------------------------------------------------------------------------------------------------
	//! "Cancel trip (refund $18): bus in 14 s".
	static string CancelName(int fare, int secondsLeft)
	{
		return LF_Terms.Text("#LF-BusStop_CancelAction", fare.ToString(), LF_Utils.MaxInt(secondsLeft, 0).ToString());
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the offer an action slot of this stop stands for, or null when the stop has fewer.
	static LF_BusOffer OfferAt(IEntity stop, int slot)
	{
		if (!stop || slot < 0 || slot >= MAX_SLOTS)
			return null;

		int now = System.GetTickCount();
		if (!s_aCached || s_CachedStop != stop.GetID() || now - s_iCachedAt > OFFER_CACHE_MS)
		{
			s_aCached = {};
			array<IEntity> stops = {};
			LF_Places.FindAll(LF_BusStop.PLACE_KIND, stops);
			Offers(stop, stops, LF_BusStopConfig.Get(), s_aCached);
			s_CachedStop = stop.GetID();
			s_iCachedAt = now;
		}

		if (slot >= s_aCached.Count())
			return null;

		return s_aCached[slot];
	}

	//------------------------------------------------------------------------------------------------
	//! Client seconds (world time, the same clock the server's cooldowns use).
	static float Now()
	{
		return LF_CharacterSwitch.Now();
	}

	//------------------------------------------------------------------------------------------------
	static bool IsWaiting()
	{
		return s_bWaiting;
	}

	//------------------------------------------------------------------------------------------------
	//! Seconds until the waiting bus leaves, rounded up.
	static int SecondsLeft()
	{
		return Math.Ceil(s_fDepartAt - Now());
	}

	//------------------------------------------------------------------------------------------------
	static int WaitingFare()
	{
		return s_iFare;
	}

	//------------------------------------------------------------------------------------------------
	//! Client: why this user may not take this offer now, as far as this machine knows.
	static LF_EBusRefusal LocalRefusal(IEntity user, notnull LF_BusOffer offer)
	{
		LF_BusStopConfig config = LF_BusStopConfig.Get();
		int wanted = LF_AccountStateBits.GetWantedLevel(LF_AccountReplication.GetState(user));
		bool officerNear = false;
		if (wanted > 0)
			officerNear = IsOfficerInSight(user, config.m_fPursuitRadius);

		int cash = LF_MoneyUtils.GetCash(user);
		float askedAt = -1;
		if (s_bAsking)
			askedAt = Now();
		else if (s_bAskedOnce)
			askedAt = s_fAskedAt;

		float arrivedAt = -1;
		if (s_bArrivedOnce)
			arrivedAt = s_fArrivedAt;

		return ActionRefusal(config, s_bWaiting, askedAt, arrivedAt, Now(), wanted, officerNear, cash, offer.m_iFare);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: an on-duty officer this machine can see within the radius (the server's check is the
	//! real one; this only greys the action early).
	protected static bool IsOfficerInSight(IEntity user, float radius)
	{
		if (!user || radius <= 0 || !GetGame().GetPlayerManager())
			return false;

		array<int> playerIds = {};
		GetGame().GetPlayerManager().GetPlayers(playerIds);
		foreach (int playerId : playerIds)
		{
			IEntity body = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
			if (!body || body == user || vector.Distance(body.GetOrigin(), user.GetOrigin()) > radius)
				continue;

			if (LF_AccountStateBits.IsOnDutyPolice(LF_AccountReplication.GetState(body)))
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Client: buy the ticket for an offer at this stop.
	static void Ask(notnull IEntity stop, notnull LF_BusOffer offer)
	{
		LF_BusStopComponent from = LF_BusStopComponent.Of(stop);
		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!from || !controller)
		{
			LF_Debug.Error(LF_BusStop.FEATURE, "bus ticket not asked: no stop component or no player controller");
			return;
		}

		s_bAsking = true;
		s_bAskedOnce = true;
		s_fAskedAt = Now();
		LF_Debug.Info(LF_BusStop.FEATURE, string.Format("bus ticket asked: %1 to %2", from.GetStopId(), offer.m_sToId));
		controller.LF_AskBusTrip(from.GetStopId(), offer.m_sToId);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: take the waiting ticket back.
	static void AskCancel()
	{
		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!controller)
		{
			LF_Debug.Error(LF_BusStop.FEATURE, "bus cancel not asked: no player controller");
			return;
		}

		LF_Debug.Info(LF_BusStop.FEATURE, "bus cancel asked");
		controller.LF_AskBusCancel();
	}

	//------------------------------------------------------------------------------------------------
	//! Client, from the server's answer to a ticket.
	static void OnResult(int refusal, int fare, int waitSeconds)
	{
		s_bAsking = false;
		if (refusal == LF_EBusRefusal.NONE)
		{
			s_bWaiting = true;
			s_iFare = fare;
			s_fDepartAt = Now() + waitSeconds;
			LF_Debug.Info(LF_BusStop.FEATURE, string.Format("bus ticket bought: fare %1, wait %2 s", fare, waitSeconds));
			LF_Utils.NoteToast(LF_Terms.Text("#LF-BusStop_Boarded", fare.ToString(), waitSeconds.ToString()), WidgetManager.Translate("#LF-BusStop_Title"), waitSeconds);
			return;
		}

		LF_Debug.Info(LF_BusStop.FEATURE, string.Format("bus ticket refused here: %1", typename.EnumToString(LF_EBusRefusal, refusal)));
		LF_Utils.NoteToast(LF_Terms.Text(LF_BusStop.RefusalKey(refusal)), WidgetManager.Translate("#LF-BusStop_Title"), 5);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the wait is over (the server's notification carries the words).
	static void OnDone(bool arrived)
	{
		s_bWaiting = false;
		s_bAsking = false;
		if (arrived)
		{
			s_bArrivedOnce = true;
			s_fArrivedAt = Now();
		}

		SCR_HintManagerComponent hints = SCR_HintManagerComponent.GetInstance();
		if (hints)
			hints.Hide();

		LF_Debug.Info(LF_BusStop.FEATURE, string.Format("bus wait over here: arrived %1", arrived));
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_bWaiting = false;
		s_bAsking = false;
		s_fDepartAt = 0;
		s_iFare = 0;
		s_fAskedAt = 0;
		s_bAskedOnce = false;
		s_fArrivedAt = 0;
		s_bArrivedOnce = false;
		s_CachedStop = EntityID.INVALID;
		s_iCachedAt = 0;
		s_aCached = null;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_BusTicket's statics (LF_StaticReset finds it by reflection).
class LF_BusTicket_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_BusTicket";
	}

	override void Apply()
	{
		LF_BusTicket.ResetStatics();
	}
}
