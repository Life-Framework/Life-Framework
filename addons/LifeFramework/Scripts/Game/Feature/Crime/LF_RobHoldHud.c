//------------------------------------------------------------------------------------------------
//! The robber's panel during a held robbery (#936): "Keep the clerk at gunpoint: 0:20", a bar that
//! fills as the hold runs, the distance to the till against the radius (red and "Too far!" past
//! three quarters of it), the rules in one line, and at the end why it ended. Client only; the
//! server judges (LF_RobHold), this only shows. The clock is the client's own from the moment the
//! hold arrived, so it can trail the server by the network delay.
//------------------------------------------------------------------------------------------------
class LF_RobHoldHud
{
	static const ResourceName LAYOUT = "{0AC2C8700B2F87E6}UI/Layouts/HUD/LF_RobHold.layout";
	static const int TICK_MS = 100; // lf-fixed: UI refresh
	static const int RESULT_SHOW_MS = 5000; // lf-fixed: how long the outcome stays on screen
	static const float BAR_WIDTH = 500; // lf-fixed: the track's width in LF_RobHold.layout
	static const float BAR_HEIGHT = 14; // lf-fixed: the track's height in LF_RobHold.layout

	protected static Widget s_wRoot;
	protected static vector s_vTill;
	protected static float s_fRadius;
	protected static int s_iHoldMs;
	protected static int s_iStartMs;
	protected static bool s_bRunning;
	protected static int s_iHideAtMs;
	protected static int s_iEnd = -1;
	protected static float s_fDistance;
	protected static bool s_bEdge;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		Remove();
		s_iEnd = -1;
	}

	//------------------------------------------------------------------------------------------------
	//! The hold began: show the panel and start the clock.
	static void Begin(vector till, float radius, int holdMs)
	{
		Remove();
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (workspace)
			s_wRoot = workspace.CreateWidgets(LAYOUT);

		if (!s_wRoot)
		{
			LF_Debug.Error(LF_RobHold.FEATURE, "the robbery hold panel did not load; the hold runs without it");
			return;
		}

		s_vTill = till;
		s_fRadius = radius;
		s_iHoldMs = holdMs;
		s_iStartMs = System.GetTickCount();
		s_bRunning = true;
		s_iEnd = -1;
		GetGame().GetCallqueue().CallLater(Tick, TICK_MS, true);
		Tick();
		LF_Debug.Info(LF_RobHold.FEATURE, string.Format("robbery hold panel shown: %1 s within %2 m", holdMs / 1000, radius));
	}

	//------------------------------------------------------------------------------------------------
	//! The server ended the hold (an LF_ERobHoldEnd): say why, then fade after a few seconds.
	static void End(int end, int paid)
	{
		s_iEnd = end;
		s_bRunning = false;
		s_iHideAtMs = System.GetTickCount() + RESULT_SHOW_MS;
		LF_Debug.Info(LF_RobHold.FEATURE, string.Format("robbery hold ended on screen: %1, paid %2", typename.EnumToString(LF_ERobHoldEnd, end), paid));
		if (!s_wRoot)
			return;

		bool won = end == LF_ERobHoldEnd.PAID;
		SetText("Timer", LF_RobHoldRules.EndKey(end), ColorOf(won));
		SetText("Range", "", Color.FromRGBA(255, 255, 255, 255));
		if (won)
			SetBar(1, Color.FromRGBA(110, 190, 90, 255));
		else
			SetBar(LF_RobHoldRules.Progress(System.GetTickCount() - s_iStartMs, s_iHoldMs), Color.FromRGBA(190, 50, 40, 255));
	}

	//------------------------------------------------------------------------------------------------
	protected static void Tick()
	{
		if (!s_wRoot)
		{
			Remove();
			return;
		}

		int now = System.GetTickCount();
		if (!s_bRunning)
		{
			if (now >= s_iHideAtMs)
				Remove();

			return;
		}

		int elapsed = now - s_iStartMs;
		SetText("Timer", WidgetManager.Translate("#LF-RobHold_Timer", LF_RobHoldRules.Clock(s_iHoldMs - elapsed)), Color.FromRGBA(242, 237, 224, 255));
		SetBar(LF_RobHoldRules.Progress(elapsed, s_iHoldMs), Color.FromRGBA(219, 168, 56, 255));

		IEntity me = LF_Utils.GetLocalCharacter();
		if (!me)
			return;

		s_fDistance = vector.Distance(me.GetOrigin(), s_vTill);
		s_bEdge = LF_RobHoldRules.NearEdge(s_fDistance, s_fRadius);
		string radius = FormatMetres(s_fRadius);
		string distance = FormatMetres(s_fDistance);
		if (s_bEdge)
			SetText("Range", WidgetManager.Translate("#LF-RobHold_Edge", radius, distance), Color.FromRGBA(235, 70, 55, 255));
		else
			SetText("Range", WidgetManager.Translate("#LF-RobHold_Range", radius, distance), Color.FromRGBA(209, 204, 189, 255));
	}

	//------------------------------------------------------------------------------------------------
	protected static string FormatMetres(float metres)
	{
		return (Math.Round(metres * 10) / 10).ToString(-1, 1);
	}

	//------------------------------------------------------------------------------------------------
	protected static Color ColorOf(bool won)
	{
		if (won)
			return Color.FromRGBA(140, 215, 110, 255);

		return Color.FromRGBA(235, 70, 55, 255);
	}

	//------------------------------------------------------------------------------------------------
	protected static void SetText(string name, string text, Color color)
	{
		TextWidget widget = TextWidget.Cast(s_wRoot.FindAnyWidget(name));
		if (!widget)
			return;

		widget.SetText(text);
		widget.SetColor(color);
	}

	//------------------------------------------------------------------------------------------------
	protected static void SetBar(float share, Color color)
	{
		Widget fill = s_wRoot.FindAnyWidget("Fill");
		if (!fill)
			return;

		FrameSlot.SetSize(fill, BAR_WIDTH * share, BAR_HEIGHT);
		fill.SetColor(color);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Remove()
	{
		if (GetGame())
			GetGame().GetCallqueue().Remove(Tick);

		if (s_wRoot)
			s_wRoot.RemoveFromHierarchy();

		s_wRoot = null;
		s_bRunning = false;
	}

	//------------------------------------------------------------------------------------------------
	//! For the harness: what the panel shows now.
	static string Describe()
	{
		if (!s_wRoot)
			return string.Format("hidden end=%1", s_iEnd);

		string state = "result";
		if (s_bRunning)
			state = "running";

		int elapsed = System.GetTickCount() - s_iStartMs;
		return string.Format("%1 left=%2 progress=%3 dist=%4 radius=%5 edge=%6 end=%7", state, LF_RobHoldRules.Clock(s_iHoldMs - elapsed), LF_RobHoldRules.Progress(elapsed, s_iHoldMs).ToString(-1, 2), s_fDistance.ToString(-1, 1), s_fRadius, s_bEdge, s_iEnd);
	}
}
