// LF_UIScreen.c — assembles a screen from the shared part layouts.
//
// MenuManager.OpenMenu leaves the root orphaned in Workbench play, so this
// creates widgets with WorkspaceWidget.CreateWidgets, wires
// SCR_ButtonTextComponent, and activates MenuContext every frame
// (MenuTextEditContext while an edit box is focused, MenuWithEditorContext so
// a pad can move focus). Close removes the widgets and that per-frame call.
// It does not go through MenuManager.
//
// Input: MenuUp/MenuDown move focus (and scroll a list to the focused row),
// MenuSelect presses the focused button, MenuBack closes. Focus lands on the
// first non-field entry at open. A feature hears its buttons through
// GetOnButton() (the part name) and learns the sheet went away through
// GetOnClosed() (Close button, MenuBack, death, distance).

void LF_UIButtonMethod(string name);
typedef func LF_UIButtonMethod;
typedef ScriptInvokerBase<LF_UIButtonMethod> LF_UIButtonInvoker;

void LF_UIClosedMethod();
typedef func LF_UIClosedMethod;
typedef ScriptInvokerBase<LF_UIClosedMethod> LF_UIClosedInvoker;

class LF_UIScreen
{
	//! Focus order keys: each top-level part owns a span, list rows sort inside their list's span.
	static const int ORDER_SPAN = 1000;

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
	//! Parallel to m_aFocus: ascending order keys, so list rows added later still sit in place.
	protected ref array<int> m_aFocusOrder = {};
	protected ref map<string, int> m_mListOrder = new map<string, int>();
	protected int m_iPartSeq;
	protected int m_iCurrentOrder;
	protected ref LF_UIButtonInvoker m_OnButton;
	protected ref LF_UIClosedInvoker m_OnClosed;

	//------------------------------------------------------------------------------------------------
	//! Fires with the part name when a BUTTON, a STATUS action, or a list row action is pressed.
	LF_UIButtonInvoker GetOnButton()
	{
		if (!m_OnButton)
			m_OnButton = new LF_UIButtonInvoker();

		return m_OnButton;
	}

	//------------------------------------------------------------------------------------------------
	//! Fires once when the sheet closes, for any reason.
	LF_UIClosedInvoker GetOnClosed()
	{
		if (!m_OnClosed)
			m_OnClosed = new LF_UIClosedInvoker();

		return m_OnClosed;
	}

	//------------------------------------------------------------------------------------------------
	bool IsOpen()
	{
		return m_wRoot != null;
	}

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

		FocusInitial(workspace);

		EL_Debug.Info("UIKit", "screen open");
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Rewrites a title or body line after the screen is open.
	bool SetLine(string name, string text)
	{
		LF_UIPart part = FindPart(name);
		if (!part)
		{
			EL_Debug.Error("UIKit", "no part named " + name);
			return false;
		}

		if (part.GetKind() != LF_EUIPartKind.LINE && part.GetKind() != LF_EUIPartKind.TITLE)
		{
			EL_Debug.Error("UIKit", name + " is not a line");
			return false;
		}

		part.SetText(text);
		Widget widget = WidgetNamed(name);
		TextWidget line = TextWidget.Cast(widget);
		if (!line)
		{
			EL_Debug.Error("UIKit", name + " has no text");
			return false;
		}

		line.SetText(text);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Rewrites a status row. An empty action hides its button.
	bool SetStatusRow(string name, string label, string status, string action)
	{
		LF_UIPart part = FindPart(name);
		if (!part || part.GetKind() != LF_EUIPartKind.STATUS)
		{
			EL_Debug.Error("UIKit", name + " is not a status row");
			return false;
		}

		part.SetText(label);
		part.SetStatus(status);
		part.SetAction(action);

		Widget row = WidgetNamed(name);
		if (!row)
			return false;

		TextWidget labelWidget = TextWidget.Cast(row.FindAnyWidget("RowLabel"));
		if (labelWidget)
			labelWidget.SetText(label);

		TextWidget statusWidget = TextWidget.Cast(row.FindAnyWidget("RowStatus"));
		if (statusWidget)
			statusWidget.SetText(status);

		Widget button = WidgetNamed(name + "_Action");
		if (!button)
			return false;

		bool show = !action.IsEmpty();
		button.SetVisible(show);
		if (!show)
			return true;

		SCR_ButtonTextComponent textButton = SCR_ButtonTextComponent.Cast(button.FindHandler(SCR_ButtonTextComponent));
		if (textButton)
			textButton.SetText(action);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Sets the stack a quantity part splits. Take is clamped; keep is the rest.
	bool SetQuantity(string name, int total, int requestedTake)
	{
		if (m_bApplyingQuantity)
			return false;

		LF_UIPart part = FindPart(name);
		if (!part || part.GetKind() != LF_EUIPartKind.QUANTITY)
		{
			EL_Debug.Error("UIKit", name + " is not a quantity");
			return false;
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
		if (!row)
			return false;

		return true;
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

		m_iCurrentOrder = m_iPartSeq * ORDER_SPAN;
		m_iPartSeq++;

		if (part.GetKind() == LF_EUIPartKind.LIST)
		{
			m_mListOrder.Set(part.GetName(), m_iCurrentOrder);
			if (!widget.FindAnyWidget("ListContent"))
				EL_Debug.Error("UIKit", "list " + part.GetName() + " is missing ListContent");

			return;
		}

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
			AddFocus(sliderRoot, m_iCurrentOrder);

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
		box.SetTextOutline(0);
		box.SetTextShadow(0);

		AddFocus(box, m_iCurrentOrder);
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
		AddFocus(widget, m_iCurrentOrder);
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
	protected void Forget(string name)
	{
		int index = m_aWidgetNames.Find(name);
		if (index < 0)
			return;

		m_aWidgetNames.Remove(index);
		m_aWidgets.Remove(index);
	}

	//------------------------------------------------------------------------------------------------
	protected void AddFocus(Widget widget, int order)
	{
		if (!widget)
			return;

		int at = LF_UIFocus.InsertIndex(m_aFocusOrder, order);
		m_aFocus.InsertAt(widget, at);
		m_aFocusOrder.InsertAt(order, at);
	}

	//------------------------------------------------------------------------------------------------
	protected void RemoveFocus(Widget widget)
	{
		int index = m_aFocus.Find(widget);
		if (index < 0)
			return;

		m_aFocus.Remove(index);
		m_aFocusOrder.Remove(index);
		if (m_iFocus >= m_aFocus.Count())
			m_iFocus = m_aFocus.Count() - 1;
	}

	//------------------------------------------------------------------------------------------------
	//! Focus the first non-field entry so MenuSelect works without pressing MenuDown first.
	protected void FocusInitial(WorkspaceWidget workspace)
	{
		array<bool> isField = {};
		foreach (Widget candidate : m_aFocus)
		{
			isField.Insert(EditBoxWidget.Cast(candidate) != null);
		}

		int index = LF_UIFocus.Initial(isField);
		if (index < 0)
			return;

		m_iFocus = index;
		if (workspace)
			workspace.SetFocusedWidget(m_aFocus.Get(index));
	}

	//------------------------------------------------------------------------------------------------
	//! Text typed into a FIELD part, or empty when there is no such field.
	string GetFieldText(string name)
	{
		Widget row = WidgetNamed(name);
		if (!row)
			return string.Empty;

		EditBoxWidget box = EditBoxWidget.Cast(row.FindAnyWidget("EditBox"));
		if (!box)
			return string.Empty;

		return box.GetText();
	}

	//------------------------------------------------------------------------------------------------
	bool SetFieldText(string name, string text)
	{
		Widget row = WidgetNamed(name);
		if (!row)
			return false;

		EditBoxWidget box = EditBoxWidget.Cast(row.FindAnyWidget("EditBox"));
		if (!box)
			return false;

		box.SetText(text);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Adds a status row to a LIST part. The row's action button reports rowName through GetOnButton.
	//! An empty action hides the button. Row names are unique across the screen.
	bool AddListRow(string listName, string rowName, string label, string status, string action)
	{
		LF_UIPart list = FindPart(listName);
		if (!list || list.GetKind() != LF_EUIPartKind.LIST)
		{
			EL_Debug.Error("UIKit", listName + " is not a list");
			return false;
		}

		if (FindPart(rowName))
		{
			EL_Debug.Error("UIKit", "skipped duplicate list row " + rowName);
			return false;
		}

		Widget listWidget = WidgetNamed(listName);
		Widget content;
		if (listWidget)
			content = listWidget.FindAnyWidget("ListContent");

		WorkspaceWidget workspace;
		if (GetGame())
			workspace = GetGame().GetWorkspace();

		if (!content || !workspace)
		{
			EL_Debug.Error("UIKit", "list " + listName + " cannot take rows");
			return false;
		}

		Widget row = workspace.CreateWidgets(LF_UITheme.STATUS_LAYOUT, content);
		if (!row)
		{
			EL_Debug.Error("UIKit", "failed to create list row " + rowName);
			return false;
		}

		LF_UIPart part = LF_UIPart.CreateStatus(rowName, label, status, action);
		part.SetList(listName);
		m_aParts.Insert(part);
		row.SetName(rowName);
		Remember(rowName, row);

		int listOrder = 0;
		m_mListOrder.Find(listName, listOrder);
		m_iCurrentOrder = listOrder + 1 + list.GetRows().Count();
		list.GetRows().Insert(rowName);

		AddStatus(row, part);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Removes every row of a LIST part (for a refresh).
	void ClearList(string listName)
	{
		LF_UIPart list = FindPart(listName);
		if (!list || list.GetKind() != LF_EUIPartKind.LIST)
		{
			EL_Debug.Error("UIKit", listName + " is not a list");
			return;
		}

		foreach (string rowName : list.GetRows())
		{
			Widget button = WidgetNamed(rowName + "_Action");
			RemoveFocus(button);
			Forget(rowName + "_Action");

			Widget row = WidgetNamed(rowName);
			if (row)
				row.RemoveFromHierarchy();

			Forget(rowName);
			LF_UIPart rowPart = FindPart(rowName);
			if (rowPart)
				m_aParts.RemoveItem(rowPart);
		}

		list.GetRows().Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! Scrolls the owning list so a focused row stays on screen.
	protected void ScrollIntoView(Widget focused)
	{
		string name = PartNameFrom(focused);
		LF_UIPart part = FindPart(name);
		if (!part || part.GetList().IsEmpty())
			return;

		LF_UIPart list = FindPart(part.GetList());
		Widget listWidget = WidgetNamed(part.GetList());
		if (!list || !listWidget)
			return;

		ScrollLayoutWidget scroll = ScrollLayoutWidget.Cast(listWidget.FindAnyWidget("ListScroll"));
		if (!scroll)
			return;

		int index = list.GetRows().Find(name);
		scroll.SetSliderPos(0, LF_UIFocus.ScrollFraction(index, list.GetRows().Count()));
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
		// Closed earlier this frame; the deferred DisarmInput removes this call.
		if (!m_bInputArmed || !m_wRoot)
			return;

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
		if (inputManager.GetActionTriggered("MenuBack"))
		{
			EL_Debug.Info("UIKit", "closed: back");
			Close();
			return;
		}

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
				ScrollIntoView(candidate);
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
		if (!m_OnButton)
		{
			EL_Debug.Warn("UIKit", "button " + name + " has no listener");
			return;
		}

		m_OnButton.Invoke(name);
	}

	//------------------------------------------------------------------------------------------------
	//! Close can run inside the repeating ActivateInputContext call (MenuBack, MenuSelect on the
	//! Close button, death or distance). Removing a call from inside its own callback is not safe
	//! (see EL_CharacterCreationManager.OnPlayerConnected), so the removal is deferred one tick and
	//! ActivateInputContext refuses to run once m_bInputArmed is false.
	void Close()
	{
		if (m_bInputArmed && GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().CallLater(DisarmInput, 0, false);

		m_bInputArmed = false;
		m_bCheckDeath = false;
		m_bCheckLost = false;
		m_bCheckDistance = false;

		if (!m_wRoot)
			return;

		m_wRoot.RemoveFromHierarchy();
		m_wRoot = null;
		EL_Debug.Info("UIKit", "screen closed");

		if (m_OnClosed)
			m_OnClosed.Invoke();
	}

	//------------------------------------------------------------------------------------------------
	protected void DisarmInput()
	{
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(ActivateInputContext);
	}
}
