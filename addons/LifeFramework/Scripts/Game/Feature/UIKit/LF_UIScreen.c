// LF_UIScreen.c — assembles a screen from the shared part layouts.
//
// MenuManager.OpenMenu leaves the root orphaned in Workbench play, so this
// creates widgets with WorkspaceWidget.CreateWidgets, wires
// SCR_ButtonTextComponent, and activates MenuContext every frame
// (MenuTextEditContext while an edit box is focused). Close removes the
// widgets and that per-frame call. It does not go through MenuManager.
class LF_UIScreen
{
	protected Widget m_wRoot;
	protected bool m_bHasField;
	protected bool m_bInputArmed;

	//------------------------------------------------------------------------------------------------
	//! Builds the frame, then each part, and starts the input driver.
	//! \return The screen, or null when the frame failed to load.
	static LF_UIScreen Open(array<ref LF_UIPart> parts)
	{
		LF_UIScreen screen = new LF_UIScreen();
		if (!screen.OpenInternal(parts))
			return null;

		return screen;
	}

	//------------------------------------------------------------------------------------------------
	protected bool OpenInternal(array<ref LF_UIPart> parts)
	{
		if (!parts || parts.Count() == 0)
		{
			EL_Debug.Error("UIKit", "refused to open: no parts");
			return false;
		}

		if (!GetGame())
		{
			EL_Debug.Error("UIKit", "failed to open: no game");
			return false;
		}

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
		{
			EL_Debug.Error("UIKit", "failed to create the frame: no workspace");
			return false;
		}

		m_wRoot = workspace.CreateWidgets(LF_UITheme.FRAME_LAYOUT);
		if (!m_wRoot)
		{
			EL_Debug.Error("UIKit", "failed to create the frame layout");
			return false;
		}

		PaintFrame();

		Widget content = m_wRoot.FindAnyWidget("Content");
		if (!content)
		{
			EL_Debug.Error("UIKit", "frame loaded but Content is missing");
			m_wRoot.RemoveFromHierarchy();
			m_wRoot = null;
			return false;
		}

		foreach (LF_UIPart part : parts)
		{
			if (!part)
			{
				EL_Debug.Error("UIKit", "skipped a null part");
				continue;
			}

			AddPart(workspace, content, part);
		}

		if (GetGame().GetCallqueue())
		{
			GetGame().GetCallqueue().CallLater(ActivateInputContext, 0, true);
			m_bInputArmed = true;
		}
		else
		{
			EL_Debug.Error("UIKit", "screen is open but the input driver failed: no callqueue");
		}

		EL_Debug.Info("UIKit", "screen open");
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void PaintFrame()
	{
		Paint("Metal", LF_UITheme.Metal());
		Paint("Paper", LF_UITheme.Paper());
		Paint("Rule", LF_UITheme.Rule());
		Paint("BackgroundBlur", LF_UITheme.Blur());
	}

	//------------------------------------------------------------------------------------------------
	protected void Paint(string name, Color color)
	{
		if (!m_wRoot)
			return;

		Widget widget = m_wRoot.FindAnyWidget(name);
		if (!widget)
		{
			EL_Debug.Error("UIKit", "frame is missing " + name);
			return;
		}

		widget.SetIsColorInherited(false);
		widget.SetColor(color);
	}

	//------------------------------------------------------------------------------------------------
	protected void AddPart(WorkspaceWidget workspace, Widget content, LF_UIPart part)
	{
		string layout = LF_UIPart.LayoutFor(part.GetKind());
		if (layout.IsEmpty())
		{
			EL_Debug.Error("UIKit", "skipped " + part.GetName() + ": no layout for that part");
			return;
		}

		Widget widget = workspace.CreateWidgets(layout, content);
		if (!widget)
		{
			EL_Debug.Error("UIKit", "failed to create part " + part.GetName());
			return;
		}

		widget.SetName(part.GetName());
		AlignableSlot.SetPadding(widget, LF_UITheme.PAD_LEFT, LF_UITheme.PAD_TOP, LF_UITheme.PAD_RIGHT, LF_UITheme.PAD_BOTTOM);

		if (part.GetKind() == LF_EUIPartKind.TITLE)
		{
			ApplyInk(TextWidget.Cast(widget), LF_UITheme.FONT_TITLE, part.GetText());
			return;
		}

		if (part.GetKind() == LF_EUIPartKind.LINE)
		{
			TextWidget line = TextWidget.Cast(widget);
			ApplyInk(line, LF_UITheme.FONT_BODY, part.GetText());
			if (line)
				line.SetTextWrapping(true);
			return;
		}

		if (part.GetKind() == LF_EUIPartKind.ROW)
		{
			PaintNamed(widget, "RowPlate", LF_UITheme.Row());
			ApplyInk(TextWidget.Cast(widget.FindAnyWidget("RowLabel")), LF_UITheme.FONT_BODY, part.GetText());
			return;
		}

		if (part.GetKind() == LF_EUIPartKind.FIELD)
		{
			AddField(widget, part);
			return;
		}

		if (part.GetKind() == LF_EUIPartKind.BUTTON || part.GetKind() == LF_EUIPartKind.CLOSE)
		{
			AddButton(widget, part);
			return;
		}

		EL_Debug.Error("UIKit", "skipped " + part.GetName() + ": unknown part");
	}

	//------------------------------------------------------------------------------------------------
	protected void AddField(Widget widget, LF_UIPart part)
	{
		m_bHasField = true;

		EditBoxWidget box = EditBoxWidget.Cast(widget.FindAnyWidget("EditBox"));
		if (!box)
		{
			EL_Debug.Error("UIKit", "field " + part.GetName() + " is missing the edit box");
			return;
		}

		box.SetIsColorInherited(false);
		box.SetColor(LF_UITheme.Ink());
		box.SetPlaceholderText(part.GetText());
		box.SetText("");

		TextWidget asText = TextWidget.Cast(box);
		if (asText)
		{
			asText.SetExactFontSize(LF_UITheme.FONT_FIELD);
			asText.SetFont(LF_UITheme.FONT);
			asText.SetOutline(0);
			asText.SetShadow(0);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void AddButton(Widget widget, LF_UIPart part)
	{
		SCR_ButtonTextComponent button = SCR_ButtonTextComponent.Cast(widget.FindHandler(SCR_ButtonTextComponent));
		if (!button)
		{
			EL_Debug.Error("UIKit", "button " + part.GetName() + " is missing SCR_ButtonTextComponent");
			return;
		}

		button.SetText(part.GetText());

		TextWidget label = button.GetTextWidget();
		if (label)
		{
			label.SetExactFontSize(LF_UITheme.FONT_BUTTON);
			label.SetFont(LF_UITheme.FONT);
		}

		bool close = part.GetKind() == LF_EUIPartKind.CLOSE;
		LF_UIButtonRelay relay = LF_UIButtonRelay.Cast(widget.FindHandler(LF_UIButtonRelay));
		if (!relay)
		{
			EL_Debug.Error("UIKit", "button " + part.GetName() + " is missing its click relay");
			return;
		}

		relay.Bind(this, part.GetName(), close);
	}

	//------------------------------------------------------------------------------------------------
	protected void ApplyInk(TextWidget text, int fontSize, string value)
	{
		if (!text)
		{
			EL_Debug.Error("UIKit", "text widget missing for a titled part");
			return;
		}

		text.SetIsColorInherited(false);
		text.SetColor(LF_UITheme.Ink());
		text.SetExactFontSize(fontSize);
		text.SetFont(LF_UITheme.FONT);
		text.SetOutline(0);
		text.SetShadow(0);
		text.SetText(value);
	}

	//------------------------------------------------------------------------------------------------
	protected void PaintNamed(Widget root, string name, Color color)
	{
		if (!root)
			return;

		Widget widget = root.FindAnyWidget(name);
		if (!widget)
		{
			EL_Debug.Error("UIKit", "part is missing " + name);
			return;
		}

		widget.SetIsColorInherited(false);
		widget.SetColor(color);
	}

	//------------------------------------------------------------------------------------------------
	protected void ActivateInputContext()
	{
		if (!GetGame())
			return;

		InputManager inputManager = GetGame().GetInputManager();
		if (!inputManager)
			return;

		if (m_bHasField)
		{
			WorkspaceWidget workspace = GetGame().GetWorkspace();
			if (workspace)
			{
				Widget focused = workspace.GetFocusedWidget();
				if (focused && EditBoxWidget.Cast(focused))
				{
					inputManager.ActivateContext("MenuTextEditContext");
					return;
				}
			}
		}

		inputManager.ActivateContext("MenuContext");
	}

	//------------------------------------------------------------------------------------------------
	//! A button that is not Close. Logged so the click is not a silent no-op.
	void Notify(string name)
	{
		EL_Debug.Info("UIKit", "button " + name);
	}

	//------------------------------------------------------------------------------------------------
	void Close()
	{
		if (m_bInputArmed && GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(ActivateInputContext);

		m_bInputArmed = false;

		if (!m_wRoot)
			return;

		m_wRoot.RemoveFromHierarchy();
		m_wRoot = null;
		EL_Debug.Info("UIKit", "screen closed");
	}
}
