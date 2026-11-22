// LF_NotificationHold.c: puts the notification toasts (the cards top right) away while a full-screen
// sheet is open, the personal file (F2) among them. A toast shown while held is made and kept, just
// hidden; on release every toast still on screen shows again and runs out its own time. Client only.
class LF_NotificationHold
{
	protected static bool s_bHeld;
	protected static ref array<Widget> s_aToasts;

	//------------------------------------------------------------------------------------------------
	//! A new toast's widget: hidden at once while the toasts are held.
	static void Track(Widget toast)
	{
		if (!toast)
			return;

		if (!s_aToasts)
			s_aToasts = {};

		Prune();
		s_aToasts.Insert(toast);
		if (s_bHeld)
			toast.SetVisible(false);
	}

	//------------------------------------------------------------------------------------------------
	//! Hides (true) or shows again (false) every toast on screen, and the ones made while held.
	static void Hold(bool held)
	{
		if (s_bHeld == held)
			return;

		s_bHeld = held;
		Prune();
		if (!s_aToasts)
			return;

		foreach (Widget toast : s_aToasts)
		{
			toast.SetVisible(!held);
		}

		LF_Debug.Info("Notify", string.Format("toasts held %1 (%2 on screen)", held, s_aToasts.Count()));
	}

	//------------------------------------------------------------------------------------------------
	static bool IsHeld()
	{
		return s_bHeld;
	}

	//------------------------------------------------------------------------------------------------
	//! Drops toasts that have gone (timed out and removed from the screen).
	protected static void Prune()
	{
		if (!s_aToasts)
			return;

		for (int i = s_aToasts.Count() - 1; i >= 0; i--)
		{
			Widget toast = s_aToasts[i];
			if (!toast || !toast.GetParent())
				s_aToasts.Remove(i);
		}
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_bHeld = false;
		s_aToasts = null;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_NotificationHold's statics (LF_StaticReset finds it by reflection).
class LF_NotificationHold_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_NotificationHold";
	}

	override void Apply()
	{
		LF_NotificationHold.ResetStatics();
	}
}
