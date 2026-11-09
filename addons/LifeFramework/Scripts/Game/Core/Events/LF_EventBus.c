//------------------------------------------------------------------------------------------------
//! Base of every bus event. An event is a plain typed record of something that already happened
//! on the server. Subscribers read it; they never change it and never answer a client with it.
class LF_Event : Managed
{
}

//------------------------------------------------------------------------------------------------
//! A bus subscriber. Subclass it and override Handle. Return false to report an error state;
//! the bus logs it and still calls the next subscriber.
class LF_EventSubscriber : Managed
{
	//------------------------------------------------------------------------------------------------
	//! Short name for the isolation log line.
	string GetName()
	{
		return Type().ToString();
	}

	//------------------------------------------------------------------------------------------------
	//! \return False when the subscriber could not do its work.
	bool Handle(notnull LF_Event busEvent)
	{
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Server event bus (docs/architecture/events.md). Features publish what they already log;
//! other features and addons subscribe instead of calling each other.
//!
//! Rules: server only (Publish from a client is refused and logged); an event is matched by its
//! exact class; subscribers run in the order they subscribed; each subscriber is isolated (a
//! null entry is skipped, a false return logs LFDebug.Error, the rest still run); the list is
//! copied before the calls, so subscribing or unsubscribing during a publish is safe and takes
//! effect from the next publish. A publish from inside a handler is allowed up to MAX_DEPTH.
class LF_EventBus
{
	static const string FEATURE = "Events";
	static const int MAX_DEPTH = 8;

	protected static ref map<string, ref array<ref LF_EventSubscriber>> s_mSubscribers;
	protected static int s_iDepth;
	protected static int s_iPublished;

	//------------------------------------------------------------------------------------------------
	protected static map<string, ref array<ref LF_EventSubscriber>> Table()
	{
		if (!s_mSubscribers)
			s_mSubscribers = new map<string, ref array<ref LF_EventSubscriber>>();

		return s_mSubscribers;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_mSubscribers = null;
		s_iDepth = 0;
		s_iPublished = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Events published since the server started. Tests read it.
	static int GetPublishedCount()
	{
		return s_iPublished;
	}

	//------------------------------------------------------------------------------------------------
	//! \param eventType The event class, e.g. LF_OnCrimeCommitted.
	//! \return False when the subscriber is null or already subscribed to this type.
	static bool Subscribe(typename eventType, LF_EventSubscriber subscriber)
	{
		if (!subscriber || !eventType)
		{
			LF_Debug.Error(FEATURE, "subscribe refused: null subscriber or event type");
			return false;
		}

		string key = eventType.ToString();
		array<ref LF_EventSubscriber> list = Table().Get(key);
		if (!list)
		{
			list = new array<ref LF_EventSubscriber>();
			Table().Set(key, list);
		}

		if (list.Find(subscriber) >= 0)
		{
			LF_Debug.Warn(FEATURE, string.Format("%1 is already subscribed to %2", subscriber.GetName(), key));
			return false;
		}

		list.Insert(subscriber);
		LF_Debug.Info(FEATURE, string.Format("%1 subscribed to %2 (%3 now)", subscriber.GetName(), key, list.Count()));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when the subscriber was subscribed and is now removed.
	static bool Unsubscribe(typename eventType, LF_EventSubscriber subscriber)
	{
		if (!subscriber || !eventType)
			return false;

		string key = eventType.ToString();
		array<ref LF_EventSubscriber> list = Table().Get(key);
		if (!list)
			return false;

		int index = list.Find(subscriber);
		if (index < 0)
			return false;

		list.Remove(index);
		LF_Debug.Info(FEATURE, string.Format("%1 unsubscribed from %2 (%3 left)", subscriber.GetName(), key, list.Count()));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	static int SubscriberCount(typename eventType)
	{
		array<ref LF_EventSubscriber> list = Table().Get(eventType.ToString());
		if (!list)
			return 0;

		return list.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: calls every subscriber of the event's class, in subscription order.
	//! \return How many subscribers handled it without an error.
	static int Publish(LF_Event busEvent)
	{
		if (!busEvent)
		{
			LF_Debug.Error(FEATURE, "publish refused: null event");
			return 0;
		}

		string key = busEvent.Type().ToString();
		if (!Replication.IsServer())
		{
			LF_Debug.Error(FEATURE, string.Format("publish of %1 refused: not the server", key));
			return 0;
		}

		if (s_iDepth >= MAX_DEPTH)
		{
			LF_Debug.Error(FEATURE, string.Format("publish of %1 refused: handlers nested %2 deep", key, s_iDepth));
			return 0;
		}

		s_iPublished++;
		array<ref LF_EventSubscriber> list = Table().Get(key);
		if (!list || list.Count() == 0)
		{
			LF_Debug.Info(FEATURE, string.Format("%1 published, no subscribers", key));
			return 0;
		}

		// Copy first: a handler may subscribe or unsubscribe.
		array<ref LF_EventSubscriber> snapshot = {};
		foreach (LF_EventSubscriber entry : list)
		{
			snapshot.Insert(entry);
		}

		LF_Debug.Info(FEATURE, string.Format("%1 published to %2 subscriber(s)", key, snapshot.Count()));
		int handled;
		s_iDepth++;
		foreach (LF_EventSubscriber subscriber : snapshot)
		{
			if (!subscriber)
			{
				LF_Debug.Error(FEATURE, string.Format("%1: a subscriber was deleted; skipped", key));
				continue;
			}

			if (subscriber.Handle(busEvent))
				handled++;
			else
				LF_Debug.Error(FEATURE, string.Format("%1: subscriber %2 reported an error; the rest still run", key, subscriber.GetName()));
		}

		s_iDepth--;
		return handled;
	}
}
