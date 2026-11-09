//------------------------------------------------------------------------------------------------
//! One notification slip. Lives on the root of
//! {4C38FD68D45325D2}UI/Layouts/Notifications/EL_NotificationToast.layout.
//! A paper slip in the UI kit palette; the left strip carries the notice type in stamp ink.
//! Timing reads the real clock (System.GetTickCount), so a slow frame does not stretch the slip.
class LF_NotificationToastUI : ScriptedWidgetComponent
{
	static const string ROOT_NAME = "NotificationToast";
	static const float FADE_SECONDS = 0.3;
	static const int TICK_MS = 50;
	//! Vertical step between stacked slips, in pixels (slip height + gap).
	static const float STACK_STEP = 128;
	static const float STACK_TOP = 140;

	protected Widget m_wRoot;
	protected ImageWidget m_wIcon;
	protected TextWidget m_wTitle;
	protected TextWidget m_wMessage;
	protected Widget m_wBackground;
	protected Widget m_wColorStrip;

	protected float m_fDisplayTime = 5.0;
	protected int m_iShownAtMs;
	protected bool m_bFadingOut;
	protected bool m_bTicking;

	//------------------------------------------------------------------------------------------------
	override void HandlerAttached(Widget w)
	{
		super.HandlerAttached(w);

		m_wRoot = w;
		m_wBackground = m_wRoot.FindAnyWidget("Background");
		m_wColorStrip = m_wRoot.FindAnyWidget("ColorStrip");
		m_wIcon = ImageWidget.Cast(m_wRoot.FindAnyWidget("Icon"));
		m_wTitle = TextWidget.Cast(m_wRoot.FindAnyWidget("Title"));
		m_wMessage = TextWidget.Cast(m_wRoot.FindAnyWidget("Message"));

		if (!m_wTitle || !m_wMessage)
			LF_Debug.Error("Notifications", "toast layout is missing Title or Message");

		if (m_wMessage)
			m_wMessage.SetTextWrapping(true);

		// No icon assets yet; the strip carries the type.
		if (m_wIcon)
			m_wIcon.SetVisible(false);

		// Start invisible for the fade-in.
		m_wRoot.SetOpacity(0);
		StackBelowSiblings();
	}

	//------------------------------------------------------------------------------------------------
	override void HandlerDeattached(Widget w)
	{
		StopTicking();
		super.HandlerDeattached(w);
	}

	//------------------------------------------------------------------------------------------------
	//! Slips that arrive while others are still up stack downwards instead of overlapping.
	protected void StackBelowSiblings()
	{
		Widget parent = m_wRoot.GetParent();
		if (!parent)
			return;

		int earlier = 0;
		Widget sibling = parent.GetChildren();
		while (sibling)
		{
			if (sibling != m_wRoot && sibling.GetName() == ROOT_NAME)
				earlier++;

			sibling = sibling.GetSibling();
		}

		// The slip is authored with a frame slot (top right); move it down one step per earlier slip.
		if (earlier > 0)
			FrameSlot.SetPos(m_wRoot, FrameSlot.GetPosX(m_wRoot), STACK_TOP + earlier * STACK_STEP);
	}

	//------------------------------------------------------------------------------------------------
	void Show(string title, string message, LF_ENotificationType type, float duration = 5.0)
	{
		m_fDisplayTime = duration;
		m_iShownAtMs = System.GetTickCount();
		m_bFadingOut = false;

		if (m_wTitle)
			m_wTitle.SetText(title);

		if (m_wMessage)
			m_wMessage.SetText(message);

		if (m_wColorStrip)
		{
			m_wColorStrip.SetIsColorInherited(false);
			m_wColorStrip.SetColor(GetColorForType(type));
		}

		// The fade is driven from Tick on the real clock. AnimateWidget left the slip at
		// opacity 0 on a dedicated game client, so no toast was ever seen there (#157).
		m_wRoot.SetOpacity(OpacityAt(0, m_fDisplayTime));
		StartTicking();
	}

	//------------------------------------------------------------------------------------------------
	//! Seconds since Show, from the real clock.
	float GetElapsedSeconds()
	{
		return (System.GetTickCount() - m_iShownAtMs) / 1000.0;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure timing rule shared with the test: 0 = showing, 1 = fading, 2 = done.
	static int Phase(float elapsedSeconds, float displaySeconds)
	{
		if (elapsedSeconds < displaySeconds)
			return 0;

		if (elapsedSeconds < displaySeconds + FADE_SECONDS)
			return 1;

		return 2;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure fade rule: 0 -> 1 over FADE_SECONDS, held, then 1 -> 0 over FADE_SECONDS. Never fully
	//! transparent at the moment it is shown, so a slip is visible from its first frame.
	static float OpacityAt(float elapsedSeconds, float displaySeconds)
	{
		if (elapsedSeconds < FADE_SECONDS)
			return Math.Clamp(0.25 + 0.75 * elapsedSeconds / FADE_SECONDS, 0, 1);

		if (elapsedSeconds < displaySeconds)
			return 1;

		return Math.Clamp(1 - (elapsedSeconds - displaySeconds) / FADE_SECONDS, 0, 1);
	}

	//------------------------------------------------------------------------------------------------
	protected void StartTicking()
	{
		if (m_bTicking || !GetGame() || !GetGame().GetCallqueue())
			return;

		m_bTicking = true;
		GetGame().GetCallqueue().CallLater(Tick, TICK_MS, false);
	}

	//------------------------------------------------------------------------------------------------
	protected void StopTicking()
	{
		m_bTicking = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Self-rescheduling (non-repeating): removing a repeating call from inside its own callback
	//! is not safe, so the tick simply does not reschedule when it is done.
	protected void Tick()
	{
		if (!m_bTicking || !m_wRoot)
			return;

		float elapsed = GetElapsedSeconds();
		int phase = Phase(elapsed, m_fDisplayTime);
		if (phase == 1)
			m_bFadingOut = true;

		m_wRoot.SetOpacity(OpacityAt(elapsed, m_fDisplayTime));

		if (phase == 2)
		{
			m_bTicking = false;
			m_wRoot.RemoveFromHierarchy();
			return;
		}

		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().CallLater(Tick, TICK_MS, false);
	}

	//------------------------------------------------------------------------------------------------
	//! Stamp-ink colours: muted, printed, not neon.
	protected Color GetColorForType(LF_ENotificationType type)
	{
		switch (type)
		{
			case LF_ENotificationType.SUCCESS:
				return Color.FromSRGBA(74, 110, 64, 255); // ledger green
			case LF_ENotificationType.WARNING:
				return Color.FromSRGBA(176, 118, 42, 255); // ochre
			case LF_ENotificationType.ERROR:
			case LF_ENotificationType.ROBBERY_ALERT:
				return Color.FromSRGBA(150, 40, 32, 255); // stamp red
			case LF_ENotificationType.POLICE_ALERT:
				return Color.FromSRGBA(40, 64, 112, 255); // carbon blue
			case LF_ENotificationType.EMS_ALERT:
				return Color.FromSRGBA(120, 30, 30, 255); // oxblood
			case LF_ENotificationType.SMS_RECEIVED:
				return Color.FromSRGBA(92, 70, 110, 255); // violet copy ink
		}

		return Color.FromSRGBA(122, 102, 64, 255); // brass, as LF_UITheme.Rule
	}
}
