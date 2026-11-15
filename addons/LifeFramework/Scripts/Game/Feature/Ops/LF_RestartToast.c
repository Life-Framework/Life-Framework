//------------------------------------------------------------------------------------------------
//! Where the restart toast goes. The default sends one toast to every connected player through the
//! notifications feature; a test swaps in a subclass that records the call instead.
class LF_RestartToastSender : Managed
{
	//------------------------------------------------------------------------------------------------
	//! Server: one toast to every player. messageArg fills %1 of the message.
	void Send(string title, string message, string messageArg)
	{
		LF_NotificationManagerComponent.NotifyAll(title, message, LF_RestartToast.DURATION, LF_ENotificationType.WARNING, messageArg);
	}
}

//------------------------------------------------------------------------------------------------
//! Restart warning toast (Ops O6, #1265). Subscribes to LF_OnRestartWarning on the server bus and
//! shows every player "Server restart in N minutes". The text is a localization key; the minutes
//! ride as the message argument. A repeat of the same lead inside the same clock minute sends
//! nothing, so a ticker that re-fires a step does not stack toasts. It never waits for the ticker.
class LF_RestartToast : LF_EventSubscriber
{
	static const string FEATURE = "Ops";
	static const string TITLE_KEY = "#LF-Ops_RestartTitle";
	static const string MESSAGE_KEY = "#LF-Ops_RestartToast";
	static const float DURATION = 10.0;

	protected static ref LF_RestartToast s_Subscriber;
	protected static ref LF_RestartToastSender s_Sender;
	protected static int s_iLastLead = -1;
	protected static int s_iLastMinute = -1;
	//! Test clock in whole minutes; -1 reads the real one.
	protected static int s_iClockMinute = -1;

	//------------------------------------------------------------------------------------------------
	//! Server: subscribes once. The restart warner calls it when it starts.
	static bool Subscribe()
	{
		if (!Replication.IsServer())
			return false;

		if (s_Subscriber && LF_EventBus.SubscriberCount(LF_OnRestartWarning) > 0)
			return true;

		s_Subscriber = new LF_RestartToast();
		return LF_EventBus.Subscribe(LF_OnRestartWarning, s_Subscriber);
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_Subscriber = null;
		s_Sender = null;
		s_iLastLead = -1;
		s_iLastMinute = -1;
		s_iClockMinute = -1;
	}

	//------------------------------------------------------------------------------------------------
	//! Tests: replaces the default all-players sender.
	static void SetSender(LF_RestartToastSender sender)
	{
		s_Sender = sender;
	}

	//------------------------------------------------------------------------------------------------
	//! Tests: pins the clock to a whole minute; -1 restores the real clock.
	static void SetClockMinute(int minute)
	{
		s_iClockMinute = minute;
	}

	//------------------------------------------------------------------------------------------------
	protected static int NowMinute()
	{
		if (s_iClockMinute >= 0)
			return s_iClockMinute;

		return System.GetUnixTime() / 60;
	}

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "RestartToast";
	}

	//------------------------------------------------------------------------------------------------
	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnRestartWarning warning = LF_OnRestartWarning.Cast(busEvent);
		if (!warning)
			return true;

		int minute = NowMinute();
		if (warning.m_iMinutesLeft == s_iLastLead && minute == s_iLastMinute)
		{
			LF_Debug.Info(FEATURE, string.Format("restart toast: %1 minute(s) already announced this minute, skipped", warning.m_iMinutesLeft));
			return true;
		}

		s_iLastLead = warning.m_iMinutesLeft;
		s_iLastMinute = minute;

		LF_RestartToastSender sender = s_Sender;
		if (!sender)
		{
			sender = new LF_RestartToastSender();
			s_Sender = sender;
		}

		sender.Send(TITLE_KEY, MESSAGE_KEY, warning.m_iMinutesLeft.ToString());
		LF_Debug.Info(FEATURE, string.Format("restart toast: told every player the server restarts in %1 minute(s) (%2 s)", warning.m_iMinutesLeft, warning.m_iSecondsToRestart));
		return true;
	}
}
