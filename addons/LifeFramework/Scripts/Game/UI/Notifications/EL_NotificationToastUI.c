//------------------------------------------------------------------------------------------------
//! UI handler for notification toast
class EL_NotificationToastUI : ScriptedWidgetComponent
{
	protected Widget m_wRoot;
	protected ImageWidget m_wIcon;
	protected TextWidget m_wTitle;
	protected TextWidget m_wMessage;
	protected Widget m_wBackground;
	protected Widget m_wColorStrip;
	
	protected float m_fDisplayTime = 5.0;
	protected float m_fElapsedTime = 0;
	protected bool m_bFadingOut = false;
	
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
		
		Print(string.Format("[EL_NotificationToastUI] HandlerAttached - Background: %1, ColorStrip: %2", m_wBackground != null, m_wColorStrip != null), LogLevel.NORMAL);
		
		// Start invisible for fade-in animation
		if (m_wRoot)
			m_wRoot.SetOpacity(0);
			
		// Hide icon by default
		if (m_wIcon)
			m_wIcon.SetVisible(false);
	}
	
	//------------------------------------------------------------------------------------------------
	void Show(string title, string message, EL_ENotificationType type, float duration = 5.0)
	{
		Print(string.Format("[EL_NotificationToastUI] Show called: title='%1', message='%2'", title, message), LogLevel.NORMAL);
		
		m_fDisplayTime = duration;
		m_fElapsedTime = 0;
		m_bFadingOut = false;
		
		// Set text
		if (m_wTitle)
		{
			m_wTitle.SetText(title);
			m_wTitle.SetColor(Color.FromSRGBA(255, 255, 255, 255)); // White title
			Print(string.Format("[EL_NotificationToastUI] ✅ Title widget found and text set to: '%1'", title), LogLevel.NORMAL);
		}
		else
		{
			Print("[EL_NotificationToastUI] ❌ ERROR: Title widget is NULL!", LogLevel.ERROR);
		}
		
		if (m_wMessage)
		{
			m_wMessage.SetText(message);
			Print(string.Format("[EL_NotificationToastUI] ✅ Message widget found and text set to: '%1'", message), LogLevel.NORMAL);
		}
		else
		{
			Print("[EL_NotificationToastUI] ❌ ERROR: Message widget is NULL!", LogLevel.ERROR);
		}
		
		// Set color based on type
		Color typeColor = GetColorForType(type);
		
		// Background is always dark
		if (m_wBackground)
			m_wBackground.SetColor(Color.FromSRGBA(0, 0, 0, 230)); // ~90% opacity black
			
		// Color strip shows the type color
		if (m_wColorStrip)
			m_wColorStrip.SetColor(typeColor);
		
		// TODO: Set icon based on type when we have icon assets
		// For now, hide icon
		if (m_wIcon)
			m_wIcon.SetVisible(false);
		
		// Fade in animation
		AnimateWidget.Opacity(m_wRoot, 1.0, 0.3);
		
		// Start update loop
		GetGame().GetCallqueue().CallLater(Update, 16, true); // ~60fps
	}
	
	//------------------------------------------------------------------------------------------------
	protected void Update()
	{
		m_fElapsedTime += 0.016; // Approximately 16ms per frame
		
		// Check if time to fade out
		if (!m_bFadingOut && m_fElapsedTime >= m_fDisplayTime)
		{
			m_bFadingOut = true;
			AnimateWidget.Opacity(m_wRoot, 0, 0.3);
		}
		
		// Check if fully faded out and ready to destroy
		if (m_bFadingOut && m_fElapsedTime >= m_fDisplayTime + 0.3)
		{
			GetGame().GetCallqueue().Remove(Update);
			
			// Destroy widget
			if (m_wRoot)
			{
				m_wRoot.RemoveFromHierarchy();
			}
		}
	}
	
	//------------------------------------------------------------------------------------------------
	protected Color GetColorForType(EL_ENotificationType type)
	{
		switch (type)
		{
			case EL_ENotificationType.SUCCESS:
				return Color.FromSRGBA(46, 204, 113, 255); // Green
			case EL_ENotificationType.WARNING:
				return Color.FromSRGBA(230, 126, 34, 255); // Orange
			case EL_ENotificationType.ERROR:
			case EL_ENotificationType.ROBBERY_ALERT:
				return Color.FromSRGBA(231, 76, 60, 255); // Red
			case EL_ENotificationType.POLICE_ALERT:
				return Color.FromSRGBA(52, 152, 219, 255); // Blue
			case EL_ENotificationType.EMS_ALERT:
				return Color.FromSRGBA(192, 57, 43, 255); // Dark Red
			case EL_ENotificationType.SMS_RECEIVED:
				return Color.FromSRGBA(155, 89, 182, 255); // Purple
			default:
				return Color.FromSRGBA(149, 165, 166, 255); // Gray
		}
		
		return Color.FromSRGBA(149, 165, 166, 255);
	}
}
