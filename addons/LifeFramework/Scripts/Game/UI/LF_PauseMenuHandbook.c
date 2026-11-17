// LF_PauseMenuHandbook.c: the pause menu's Handbook button (#1773, refs #642). Client only.
// Adds one button under Field Manual that puts the pause menu away and opens the handbook
// (LF_HandbookScreen). The button is made the way the base game's rally mod adds its restart
// button (SCR_PauseMenuUI_modded.c): a PauseMenuButton prefab created into the button column, then
// slotted in by Z order. A pause menu without the column or the prefab logs one error and shows
// no button; the rest of the menu is untouched.

modded class PauseMenuUI
{
	static const string LF_HANDBOOK_BUTTON = "HandbookButton";
	static const string LF_BUTTON_LAYOUT = "{9ECCD201BCF07E95}UI/layouts/Menus/PauseMenu/PauseMenuButton.layout";
	//! The base game's field manual button: the handbook goes right under it.
	static const string LF_ANCHOR_BUTTON = "FieldManual";
	//! What pauseMenu.layout gives every button instance, which CreateWidgets does not inherit.
	static const float LF_BUTTON_HEIGHT = 54;	// lf-fixed: matches the base pause menu buttons

	protected SCR_ButtonTextComponent m_LFHandbookButton;

	//------------------------------------------------------------------------------------------------
	override protected void OnMenuOpen()
	{
		super.OnMenuOpen();
		LF_AddHandbookButton();
	}

	//------------------------------------------------------------------------------------------------
	protected void LF_AddHandbookButton()
	{
		SCR_ButtonTextComponent anchor = SCR_ButtonTextComponent.GetButtonText(LF_ANCHOR_BUTTON, m_wRoot);
		if (!anchor)
		{
			LF_Debug.Error(LF_HandbookScreen.FEATURE, "pause menu has no " + LF_ANCHOR_BUTTON + " button to sit under, no handbook button");
			return;
		}

		Widget anchorButton = anchor.GetRootWidget();
		if (!anchorButton)
			return;

		Widget parent = anchorButton.GetParent();
		if (!parent)
			return;

		array<ref Widget> existingButtons = {};
		SCR_WidgetHelper.GetAllChildren(parent, existingButtons);

		int insertPosition = existingButtons.Find(anchorButton) + 1;
		if (insertPosition <= 0)
		{
			LF_Debug.Error(LF_HandbookScreen.FEATURE, "pause menu button column does not hold " + LF_ANCHOR_BUTTON + ", no handbook button");
			return;
		}

		int order;
		foreach (Widget existingButton : existingButtons)
		{
			if (order == insertPosition)
				order++; // The new button's place

			existingButton.SetZOrder(order);
			order++;
		}

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
		{
			LF_Debug.Error(LF_HandbookScreen.FEATURE, "pause menu has no workspace, no handbook button");
			return;
		}

		Widget newButton = workspace.CreateWidgets(LF_BUTTON_LAYOUT, parent);
		if (!newButton)
		{
			LF_Debug.Error(LF_HandbookScreen.FEATURE, "pause menu button layout did not load, no handbook button: " + LF_BUTTON_LAYOUT);
			return;
		}

		newButton.SetName(LF_HANDBOOK_BUTTON);
		newButton.SetZOrder(insertPosition);
		SizeLayoutWidget newButtonSize = SizeLayoutWidget.Cast(newButton.GetChildren());
		if (newButtonSize)
		{
			newButtonSize.SetHeightOverride(LF_BUTTON_HEIGHT);
			LayoutSlot.SetPadding(newButtonSize, 7, 1, 0, 1);
		}

		m_LFHandbookButton = SCR_ButtonTextComponent.Cast(newButton.FindHandler(SCR_ButtonTextComponent));
		if (!m_LFHandbookButton)
		{
			LF_Debug.Error(LF_HandbookScreen.FEATURE, "pause menu handbook button has no button component");
			newButton.RemoveFromHierarchy();
			return;
		}

		m_LFHandbookButton.SetText("#LF-Handbook_PauseButton");
		m_LFHandbookButton.m_OnClicked.Insert(LF_OnHandbook);
	}

	//------------------------------------------------------------------------------------------------
	//! The pause menu goes first so the world runs again, then the handbook opens on the next tick:
	//! the handbook is a kit sheet, and a kit sheet and the pause menu would both answer one Back.
	protected void LF_OnHandbook()
	{
		Close();
		GetGame().GetCallqueue().CallLater(LF_HandbookScreen.OpenFromPause, 0, false);
	}
}
