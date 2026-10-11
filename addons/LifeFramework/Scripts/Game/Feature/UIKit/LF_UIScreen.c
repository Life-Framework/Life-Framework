// LF_UIScreen.c — assembles a screen from the shared part layouts.
//
// MenuManager.OpenMenu leaves the root orphaned in Workbench play, so this
// creates widgets with WorkspaceWidget.CreateWidgets, wires
// SCR_ButtonTextComponent, and activates MenuContext every frame
// (MenuTextEditContext while an edit box is focused, MenuWithEditorContext so
// a pad can move focus). Close removes the widgets and that per-frame call.
// It does not go through MenuManager.
class LF_UIScreen
{
	protected Widget m_wRoot;
	protected bool m_bHasField;
	protected bool m_bInputArmed;
	protected bool m_bCheckDeath;
	protected bool m_bCheckLost;
	protected bool m_bCheckDistance;
	protected bool m_bApplyingQuantity;
	protected float m_fMaxDistance;
	protected EntityID m_Anchor;
	protected int m_iFocus;
	protected ref array<ref LF_UIPart> m_aParts = {};
	protected ref array<string> m_aWidgetNames = {};
	protected ref array<Widget> m_aWidgets = {};
	protected ref array<Widget> m_aFocus = {};

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
		m_Anchor = EntityID.INVALID;
		m_iFocus = -1;

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
	//! Rewrites a title or body line after the screen is open.
	void SetLine(string name, string text)
	{
		LF_UIPart part = FindPart(name);
		if (!part)
		{
			EL_Debug.Error("UIKit", "no part named " + name);
			return;
		}

		if (part.GetKind() != LF_EUIPartKind.LINE && part.GetKind() != LF_EUIPartKind.TITLE)
		{
			EL_Debug.Error("UIKit", name + " is not a line");
			return;
		}

		part.SetText(text);
		Widget widget = WidgetNamed(name);
		if (!widget)
			return;

		TextWidget line = TextWidget.Cast(widget);
		if (!line)
		{
			EL_Debug.Error("UIKit", name + " has no text");
			return;
		}

		line.SetText(text);
	}

	//------------------------------------------------------------------------------------------------
	//! Rewrites a status row. An empty action hides its button.
	void SetStatusRow(string name, string label, string status, string action)
	{
		LF_UIPart part = FindPart(name);
		if (!part || part.GetKind() != LF_EUIPartKind.STATUS)
		{
			EL_Debug.Error("UIKit", name + " is not a status row");
			return;
		}

		part.SetText(label);
		part.SetStatus(status);
		part.SetAction(action);

		Widget row = WidgetNamed(name);
		if (!row)
			return;

		TextWidget labelWidget = TextWidget.Cast(row.FindAnyWidget("RowLabel"));
		if (labelWidget)
			labelWidget.SetText(label);

		TextWidget statusWidget = TextWidget.Cast(row.FindAnyWidget("RowStatus"));
		if (statusWidget)
			statusWidget.SetText(status);

		Widget button = WidgetNamed(name + "_Action");
		if (!button)
			return;

		bool show = !action.IsEmpty();
		button.SetVisible(show);
		if (!show)
			return;

		SCR_ButtonTextComponent textButton = SCR_ButtonTextComponent.Cast(button.FindHandler(SCR_ButtonTextComponent));
		if (textButton)
			textButton.SetText(action);
	}

	//------------------------------------------------------------------------------------------------
	//! Sets the stack a quantity part splits. Take is clamped; keep is the rest.
	void SetQuantity(string name, int total, int requestedTake)
	{
		if (m_bApplyingQuantity)
			return;

		LF_UIPart part = FindPart(name);
		if (!part || part.GetKind() != LF_EUIPartKind.QUANTITY)
		{
			EL_Debug.Error("UIKit", name + " is not a quantity");
			return;
		}

		m_bApplyingQuantity = true;

		int keep;
		int take;
		bool split = LF_UIQuantity.Split(total, requestedTake, keep, take);
		part.SetTotal(total);
		part.SetTake(take);

		Widget row = WidgetNamed(name);
		if (row)
		{
			SetFace(row, "FaceKeep", "#LF-UIKit_Keep", keep);
			SetFace(row, "FaceTake", "#LF-UIKit_Take", take);

			Widget sliderRoot = row.FindAnyWidget("QuantitySlider");
			if (sliderRoot)
			{
				sliderRoot.SetEnabled(split);
				EL_ThinSliderWidget slider = EL_ThinSliderWidget.Cast(sliderRoot.FindHandler(EL_ThinSliderWidget));
				if (split && slider)
					slider.Configure(1, total - 1, 1, take);
				else if (!slider)
					EL_Debug.Error("UIKit", name + " slider is missing");
			}
		}

		if (!split)
			EL_Debug.Info("UIKit", name + " has nothing to split");

		m_bApplyingQuantity = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Slider callback. Drag and release both commit the same clamped take.
	void NotifyQuantity(string name, int take, bool finished)
	{
		if (m_bApplyingQuantity)
			return;

		LF_UIPart part = FindPart(name);
		if (!part)
		{
			EL_Debug.Error("UIKit", "no quantity named " + name);
			return;
		}

		// Drag and release both commit the clamped take.
		if (finished || take >= 0)
			SetQuantity(name, part.GetTotal(), take);
	}

	//------------------------------------------------------------------------------------------------
	void CloseOnDeath()
	{
		m_bCheckDeath = true;
	}

	//------------------------------------------------------------------------------------------------
	void CloseWhenUncontrolled()
	{
		m_bCheckLost = true;
	}

	//------------------------------------------------------------------------------------------------
	//! Closes once the local character is farther than meters from the anchor.
	void CloseBeyond(float meters, EntityID anchor)
	{
		if (meters <= 0)
		{
			EL_Debug.Error("UIKit", "close distance refused: range must be greater than 0");
			return;
		}

		if (anchor == EntityID.INVALID)
		{
			EL_Debug.Error("UIKit", "closed: anchor gone");
			Close();
			return;
		}

		m_bCheckDistance = true;
		m_fMaxDistance = meters;
		m_Anchor = anchor;
	}

	//------------------------------------------------------------------------------------------------
	protected void SetFace(Widget row, string widgetName, string key, int value)
	{
		TextWidget face = TextWidget.Cast(row.FindAnyWidget(widgetName));
		if (!face)
		{
			EL_Debug.Error("UIKit", "quantity is missing " + widgetName);
			return;
		}

		face.SetText(WidgetManager.Translate(key, value));
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
		if (FindPart(part.GetName()))
		{
			EL_Debug.Error("UIKit", "skipped duplicate part " + part.GetName());
			return;
		}

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

		m_aParts.Insert(part);
		widget.SetName(part.GetName());
		Remember(part.GetName(), widget);
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

		if (part.GetKind() == LF_EUIPartKind.STATUS)
		{
			AddStatus(widget, part);
			return;
		}

		if (part.GetKind() == LF_EUIPartKind.QUANTITY)
		{
			AddQuantity(widget, part);
			return;
		}

		if (part.GetKind() == LF_EUIPartKind.FIELD)
		{
			AddField(widget, part);
			return;
		}

		if (part.GetKind() == LF_EUIPartKind.BUTTON || part.GetKind() == LF_EUIPartKind.CLOSE)
		{
			WireButton(widget, part.GetName(), part.GetText(), part.GetKind() == LF_EUIPartKind.CLOSE);
			return;
		}

		EL_Debug.Error("UIKit", "skipped " + part.GetName() + ": unknown part");
	}

	//------------------------------------------------------------------------------------------------
	protected void AddStatus(Widget widget, LF_UIPart part)
	{
		PaintNamed(widget, "RowPlate", LF_UITheme.Row());
		ApplyInk(TextWidget.Cast(widget.FindAnyWidget("RowLabel")), LF_UITheme.FONT_BODY, part.GetText());
		ApplyInk(TextWidget.Cast(widget.FindAnyWidget("RowStatus")), LF_UITheme.FONT_BODY, part.GetStatus());

		Widget button = widget.FindAnyWidget("ActionButton");
		if (!button)
		{
			EL_Debug.Error("UIKit", "status row " + part.GetName() + " is missing its action");
			return;
		}

		string widgetName = part.GetName() + "_Action";
		button.SetName(widgetName);
		Remember(widgetName, button);
		WireButton(button, part.GetName(), part.GetAction(), false);

		if (part.GetAction().IsEmpty())
			button.SetVisible(false);
	}

	//------------------------------------------------------------------------------------------------
	protected void AddQuantity(Widget widget, LF_UIPart part)
	{
		ApplyInk(TextWidget.Cast(widget.FindAnyWidget("QuantityLabel")), LF_UITheme.FONT_BODY, part.GetText());
		ApplyInk(TextWidget.Cast(widget.FindAnyWidget("FaceKeep")), LF_UITheme.FONT_BODY, "");
		ApplyInk(TextWidget.Cast(widget.FindAnyWidget("FaceTake")), LF_UITheme.FONT_BODY, "");

		LF_UIQuantityRelay relay = LF_UIQuantityRelay.Cast(widget.FindHandler(LF_UIQuantityRelay));
		if (relay)
			relay.Bind(this, part.GetName());
		else
			EL_Debug.Error("UIKit", "quantity " + part.GetName() + " is missing its relay");

		Widget sliderRoot = widget.FindAnyWidget("QuantitySlider");
		if (sliderRoot)
			m_aFocus.Insert(sliderRoot);

		SetQuantity(part.GetName(), part.GetTotal(), part.GetTake());
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

		m_aFocus.Insert(box);
	}

	//------------------------------------------------------------------------------------------------
	protected void WireButton(Widget widget, string notifyName, string text, bool close)
	{
		SCR_ButtonTextComponent button = SCR_ButtonTextComponent.Cast(widget.FindHandler(SCR_ButtonTextComponent));
		if (!button)
		{
			EL_Debug.Error("UIKit", "button " + notifyName + " is missing SCR_ButtonTextComponent");
			return;
		}

		button.SetText(text);

		TextWidget label = button.GetTextWidget();
		if (label)
		{
			label.SetExactFontSize(LF_UITheme.FONT_BUTTON);
			label.SetFont(LF_UITheme.FONT);
		}

		LF_UIButtonRelay relay = LF_UIButtonRelay.Cast(widget.FindHandler(LF_UIButtonRelay));
		if (!relay)
		{
			EL_Debug.Error("UIKit", "button " + notifyName + " is missing its click relay");
			return;
		}

		relay.Bind(this, notifyName, close);
		m_aFocus.Insert(widget);
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
	protected void Remember(string name, Widget widget)
	{
		m_aWidgetNames.Insert(name);
		m_aWidgets.Insert(widget);
	}

	//------------------------------------------------------------------------------------------------
	protected Widget WidgetNamed(string name)
	{
		for (int i = 0; i < m_aWidgetNames.Count(); i++)
		{
			if (m_aWidgetNames.Get(i) == name)
				return m_aWidgets.Get(i);
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected LF_UIPart FindPart(string name)
	{
		foreach (LF_UIPart part : m_aParts)
		{
			if (part && part.GetName() == name)
				return part;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected bool EvaluateClose()
	{
		if (!m_bCheckDeath && !m_bCheckLost && !m_bCheckDistance)
			return false;

		if (!GetGame())
			return false;

		bool dead = false;
		bool hasCharacter = false;
		SCR_ChimeraCharacter character = EL_Utils.GetLocalCharacter();
		if (character)
		{
			hasCharacter = true;
			CharacterControllerComponent controller = CharacterControllerComponent.Cast(character.FindComponent(CharacterControllerComponent));
			if (controller && controller.GetLifeState() == ECharacterLifeState.DEAD)
				dead = true;
		}

		bool anchorMissing = false;
		float distance = 0;
		if (m_bCheckDistance)
		{
			if (!GetGame().GetWorld())
			{
				anchorMissing = true;
			}
			else
			{
				IEntity anchor = GetGame().GetWorld().FindEntityByID(m_Anchor);
				if (!anchor)
					anchorMissing = true;
				else if (character)
					distance = vector.Distance(character.GetOrigin(), anchor.GetOrigin());
			}
		}

		int reason = LF_UIClosePolicy.Reason(m_bCheckDeath, m_bCheckLost, m_bCheckDistance, dead, hasCharacter, anchorMissing, m_fMaxDistance, distance);
		if (reason == LF_UIClosePolicy.STAY)
			return false;

		EL_Debug.Info("UIKit", "closed: " + LF_UIClosePolicy.Name(reason));
		Close();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void ActivateInputContext()
	{
		if (EvaluateClose())
			return;

		if (!GetGame())
			return;

		InputManager inputManager = GetGame().GetInputManager();
		if (!inputManager)
			return;

		bool fieldFocused = false;
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (m_bHasField && workspace)
		{
			Widget focused = workspace.GetFocusedWidget();
			if (focused && EditBoxWidget.Cast(focused))
				fieldFocused = true;
		}

		if (fieldFocused)
			inputManager.ActivateContext("MenuTextEditContext");
		else
			inputManager.ActivateContext("MenuContext");

		// MenuUp and MenuDown are not in MenuContext. MenuWithEditorContext carries them.
		inputManager.ActivateContext("MenuWithEditorContext");
		MoveFocus(inputManager, workspace, fieldFocused);
	}

	//------------------------------------------------------------------------------------------------
	protected void MoveFocus(InputManager inputManager, WorkspaceWidget workspace, bool fieldFocused)
	{
		int direction = 0;
		if (inputManager.GetActionTriggered("MenuDown"))
			direction = 1;
		else if (inputManager.GetActionTriggered("MenuUp"))
			direction = -1;

		if (direction != 0)
			StepFocus(workspace, direction);

		if (fieldFocused)
			return;

		if (inputManager.GetActionTriggered("MenuLeft"))
			NudgeQuantity(workspace, -1);
		else if (inputManager.GetActionTriggered("MenuRight"))
			NudgeQuantity(workspace, 1);

		if (inputManager.GetActionTriggered("MenuSelect"))
			ActivateFocused(workspace);
	}

	//------------------------------------------------------------------------------------------------
	protected void StepFocus(WorkspaceWidget workspace, int direction)
	{
		int count = m_aFocus.Count();
		if (count <= 0 || !workspace)
			return;

		int current = IndexOfFocus(workspace.GetFocusedWidget());
		if (current < 0)
			current = m_iFocus;

		int next = LF_UIFocus.Step(count, current, direction);
		int guard = 0;
		while (guard < count)
		{
			Widget candidate = m_aFocus.Get(next);
			if (candidate && candidate.IsVisible())
			{
				m_iFocus = next;
				workspace.SetFocusedWidget(candidate);
				return;
			}

			next = LF_UIFocus.Step(count, next, direction);
			guard++;
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void NudgeQuantity(WorkspaceWidget workspace, int direction)
	{
		if (!workspace)
			return;

		int index = IndexOfFocus(workspace.GetFocusedWidget());
		if (index < 0)
			index = m_iFocus;

		if (index < 0 || index >= m_aFocus.Count())
			return;

		Widget focused = m_aFocus.Get(index);
		if (!focused)
			return;

		string name = PartNameFrom(focused);
		LF_UIPart part = FindPart(name);
		if (!part || part.GetKind() != LF_EUIPartKind.QUANTITY)
			return;

		SetQuantity(part.GetName(), part.GetTotal(), part.GetTake() + direction);
	}

	//------------------------------------------------------------------------------------------------
	protected string PartNameFrom(Widget widget)
	{
		Widget cursor = widget;
		while (cursor)
		{
			LF_UIPart part = FindPart(cursor.GetName());
			if (part)
				return part.GetName();

			cursor = cursor.GetParent();
		}

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	protected void ActivateFocused(WorkspaceWidget workspace)
	{
		if (!workspace)
			return;

		Widget focused = workspace.GetFocusedWidget();
		int index = IndexOfFocus(focused);
		if (index < 0)
			index = m_iFocus;

		if (index < 0 || index >= m_aFocus.Count())
			return;

		Widget widget = m_aFocus.Get(index);
		if (!widget || !widget.IsVisible())
			return;

		if (EditBoxWidget.Cast(widget))
			return;

		LF_UIButtonRelay relay = LF_UIButtonRelay.Cast(widget.FindHandler(LF_UIButtonRelay));
		if (!relay && widget.GetName() == "QuantitySlider")
			return;

		if (relay)
			relay.Activate();
	}

	//------------------------------------------------------------------------------------------------
	protected int IndexOfFocus(Widget widget)
	{
		if (!widget)
			return -1;

		for (int i = 0; i < m_aFocus.Count(); i++)
		{
			if (m_aFocus.Get(i) == widget)
				return i;
		}

		return -1;
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
		m_bCheckDeath = false;
		m_bCheckLost = false;
		m_bCheckDistance = false;

		if (!m_wRoot)
			return;

		m_wRoot.RemoveFromHierarchy();
		m_wRoot = null;
		EL_Debug.Info("UIKit", "screen closed");
	}
}
