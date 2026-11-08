// LF_UIMenuInput.c — keyboard and pad input for the menus that still open through MenuManager
// (ATM, Shop, Quest, Police) and the plain widget pickers (faction, spawn location, death,
// organization). Same rules as LF_UIScreen, packaged for a menu that owns its own layout.
//
// Input contexts must be activated every frame or only raw mouse clicks work. MenuUp/MenuDown
// live in MenuWithEditorContext, not MenuContext, so both are activated (see EL_DebugMenu).
// One physical press can reach a handler twice (a button's own invoker and this poll), so every
// handler passes Ready(key), a time debounce. A Close that runs twice takes the menu underneath
// with it.

enum LF_EUIMenuInput
{
	NONE,
	BACK,
	SELECT
}

class LF_UIMenuInput
{
	static const float DEBOUNCE_MS = 150;

	protected ref map<string, float> m_mStampMs = new map<string, float>();
	protected ref array<Widget> m_aFocus = {};
	protected int m_iFocus = -1;
	protected Widget m_wSelected;

	//------------------------------------------------------------------------------------------------
	//! The focus ring, top to bottom. Null entries are skipped. Focus lands on the first entry that
	//! is not a text field, so MenuSelect works at once.
	void SetFocusOrder(array<Widget> widgets)
	{
		m_aFocus.Clear();
		m_iFocus = -1;
		if (!widgets)
			return;

		array<bool> isField = {};
		foreach (Widget widget : widgets)
		{
			if (!widget)
				continue;

			m_aFocus.Insert(widget);
			isField.Insert(EditBoxWidget.Cast(widget) != null);
		}

		// A rebuilt ring (a list refresh while typing a search) keeps the widget that has focus.
		WorkspaceWidget workspace = GetWorkspace();
		if (workspace)
		{
			int kept = m_aFocus.Find(RingEntry(workspace.GetFocusedWidget()));
			if (kept >= 0)
			{
				m_iFocus = kept;
				return;
			}
		}

		int index = LF_UIFocus.Initial(isField);
		if (index < 0)
			return;

		m_iFocus = index;
		if (workspace)
			workspace.SetFocusedWidget(m_aFocus.Get(index));
	}

	//------------------------------------------------------------------------------------------------
	//! Call every frame while the menu is open. Returns BACK on MenuBack, SELECT on MenuSelect over a
	//! focused non-field widget (GetSelected names it), NONE otherwise. MenuUp/MenuDown move focus.
	LF_EUIMenuInput Tick()
	{
		m_wSelected = null;
		if (!GetGame())
			return LF_EUIMenuInput.NONE;

		InputManager input = GetGame().GetInputManager();
		if (!input)
			return LF_EUIMenuInput.NONE;

		WorkspaceWidget workspace = GetWorkspace();
		Widget focused;
		if (workspace)
			focused = workspace.GetFocusedWidget();

		bool fieldFocused = false;
		if (focused && EditBoxWidget.Cast(focused))
			fieldFocused = true;

		if (fieldFocused)
			input.ActivateContext("MenuTextEditContext");
		else
			input.ActivateContext("MenuContext");

		input.ActivateContext("MenuWithEditorContext");

		if (input.GetActionTriggered("MenuBack"))
			return LF_EUIMenuInput.BACK;

		int direction = 0;
		if (input.GetActionTriggered("MenuDown"))
			direction = 1;
		else if (input.GetActionTriggered("MenuUp"))
			direction = -1;

		if (direction != 0)
			Step(workspace, focused, direction);

		if (fieldFocused || !focused)
			return LF_EUIMenuInput.NONE;

		if (!input.GetActionTriggered("MenuSelect"))
			return LF_EUIMenuInput.NONE;

		m_wSelected = focused;
		return LF_EUIMenuInput.SELECT;
	}

	//------------------------------------------------------------------------------------------------
	//! The widget MenuSelect pressed during the last Tick, or null.
	Widget GetSelected()
	{
		return m_wSelected;
	}

	//------------------------------------------------------------------------------------------------
	//! True once per press: a second route reporting the same press within DEBOUNCE_MS is refused.
	bool Ready(string key)
	{
		float nowMs = System.GetTickCount();
		float lastMs;
		if (m_mStampMs.Find(key, lastMs) && nowMs - lastMs < DEBOUNCE_MS)
			return false;

		m_mStampMs.Set(key, nowMs);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The widget, or the first widget at or above it that is in the focus ring. Buttons report
	//! clicks from inner widgets; this maps them back to the ring entry.
	Widget RingEntry(Widget widget)
	{
		Widget cursor = widget;
		while (cursor)
		{
			if (m_aFocus.Find(cursor) >= 0)
				return cursor;

			cursor = cursor.GetParent();
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected void Step(WorkspaceWidget workspace, Widget focused, int direction)
	{
		int count = m_aFocus.Count();
		if (count <= 0 || !workspace)
			return;

		int current = m_aFocus.Find(RingEntry(focused));
		if (current < 0)
			current = m_iFocus;

		int next = LF_UIFocus.Step(count, current, direction);
		for (int guard = 0; guard < count; guard++)
		{
			Widget candidate = m_aFocus.Get(next);
			if (candidate && candidate.IsVisible() && candidate.IsEnabled())
			{
				m_iFocus = next;
				workspace.SetFocusedWidget(candidate);
				return;
			}

			next = LF_UIFocus.Step(count, next, direction);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected WorkspaceWidget GetWorkspace()
	{
		if (!GetGame())
			return null;

		return GetGame().GetWorkspace();
	}

	//------------------------------------------------------------------------------------------------
	//! The invoker a vanilla button fires when pressed: m_OnActivated for the dialog input buttons
	//! (DialogButtonCancel and friends), m_OnClicked for the other SCR_ButtonBaseComponent buttons.
	//! Null when the widget is not a button. Script methods cannot take func arguments, so the
	//! caller inserts its own handler.
	static ScriptInvoker ClickInvoker(Widget button)
	{
		if (!button)
			return null;

		SCR_InputButtonComponent inputButton = SCR_InputButtonComponent.Cast(button.FindHandler(SCR_InputButtonComponent));
		if (inputButton)
			return inputButton.m_OnActivated;

		SCR_ButtonBaseComponent component = SCR_ButtonBaseComponent.Cast(button.FindHandler(SCR_ButtonBaseComponent));
		if (component)
			return component.m_OnClicked;

		return null;
	}
}
