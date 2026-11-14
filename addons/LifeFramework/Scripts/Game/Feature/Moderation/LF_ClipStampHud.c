//------------------------------------------------------------------------------------------------
//! The clip stamp on screen (#1173): a small paper tag in the top left, built from UI kit parts
//! (the frame sheet and one line, painted from LF_UITheme), no layout of its own. Client only.
class LF_ClipStampHud
{
	static const int TICK_MS = 250; // lf-fixed: UI refresh, the text changes once a second
	static const float TAG_WIDTH = 380; // lf-fixed: tag size in pixels at the 1920x1080 reference
	static const float TAG_HEIGHT = 34; // lf-fixed: tag size in pixels at the 1920x1080 reference
	static const float TAG_MARGIN = 16; // lf-fixed: gap to the screen corner
	static const float OPACITY = 0.82; // lf-fixed: unobtrusive over footage

	protected static Widget s_wRoot;
	protected static TextWidget s_wText;
	protected static bool s_bTicking;
	protected static int s_iLastSecond = -1;
	protected static int s_iLastSyncMs;

	//------------------------------------------------------------------------------------------------
	static void Remove()
	{
		if (s_bTicking && GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(Tick);

		s_bTicking = false;
		if (s_wRoot)
			s_wRoot.RemoveFromHierarchy();

		s_wRoot = null;
		s_wText = null;
		s_iLastSecond = -1;
	}

	//------------------------------------------------------------------------------------------------
	//! Show or hide to match LF_ClipStamp.IsShown().
	static void Refresh()
	{
		if (!LF_ClipStamp.IsShown() || !LF_ClipStamp.IsSynced())
		{
			Remove();
			return;
		}

		if (s_wRoot)
			return;

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return;

		s_wRoot = workspace.CreateWidgets(LF_UITheme.FRAME_LAYOUT);
		if (!s_wRoot)
		{
			LF_Debug.Error(LF_ClipStamp.FEATURE, "the clip stamp frame part did not load; the stamp stays hidden");
			return;
		}

		Widget blur = s_wRoot.FindAnyWidget("BackgroundBlur");
		if (blur)
			blur.SetVisible(false);

		Widget rule = s_wRoot.FindAnyWidget("Rule");
		if (rule)
			rule.SetVisible(false);

		Widget content = s_wRoot.FindAnyWidget("Content");
		if (content)
			content.SetVisible(false);

		Widget sheet = s_wRoot.FindAnyWidget("Sheet");
		if (!sheet)
		{
			LF_Debug.Error(LF_ClipStamp.FEATURE, "the clip stamp frame has no Sheet widget");
			Remove();
			return;
		}

		FrameSlot.SetAnchorMin(sheet, 0, 0);
		FrameSlot.SetAnchorMax(sheet, 0, 0);
		FrameSlot.SetPos(sheet, TAG_MARGIN, TAG_MARGIN);
		FrameSlot.SetSize(sheet, TAG_WIDTH, TAG_HEIGHT);
		Paint("Metal", LF_UITheme.Metal());
		Paint("Paper", LF_UITheme.Paper());

		s_wText = TextWidget.Cast(workspace.CreateWidgets(LF_UITheme.LINE_LAYOUT, sheet));
		if (!s_wText)
		{
			LF_Debug.Error(LF_ClipStamp.FEATURE, "the clip stamp line part did not load; the stamp stays hidden");
			Remove();
			return;
		}

		FrameSlot.SetAnchorMin(s_wText, 0, 0);
		FrameSlot.SetAnchorMax(s_wText, 1, 1);
		FrameSlot.SetOffsets(s_wText, 14, 7, 14, 4);
		s_wText.SetExactFontSize(LF_UITheme.CaptionSize());
		s_wText.SetColor(LF_UITheme.Ink());
		s_wRoot.SetOpacity(OPACITY);
		s_wRoot.SetZOrder(100);
		s_iLastSyncMs = System.GetTickCount();
		s_bTicking = true;
		GetGame().GetCallqueue().CallLater(Tick, TICK_MS, true);
		Tick();
	}

	//------------------------------------------------------------------------------------------------
	protected static void Paint(string name, Color color)
	{
		Widget widget = s_wRoot.FindAnyWidget(name);
		if (widget)
			widget.SetColor(color);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Tick()
	{
		if (!s_wText || !LF_ClipStamp.IsShown())
		{
			Remove();
			return;
		}

		int now = System.GetTickCount();
		if (now - s_iLastSyncMs >= LF_ClipStamp.RESYNC_MS)
		{
			s_iLastSyncMs = now;
			SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
			if (controller)
				controller.LF_AskClipStamp();
		}

		string text = LF_ClipStamp.Text(now);
		if (text != s_wText.GetText())
			s_wText.SetText(text);
	}
}
