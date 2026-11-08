//! Faction picker for first-time players. Deliberately NOT a MenuManager menu: in a Workbench play
//! session OpenMenu leaves the menu root orphaned (or detached/zero-sized), so the layout is
//! created straight on the workspace and the buttons are wired through SCR_ButtonTextComponent
//! invokers - the same structure Overthrow's OVT_UIContext uses for every screen.
//!
//! Input: a plain widget tree activates no input context, so no mouse cursor would appear and the
//! buttons could not be clicked. While open, the vanilla MenuContext (Priority 50, Flags 0x4 -
//! cursor showing, Menu* actions for gamepad) is re-activated every frame, exactly like
//! OVT_UIContext.EOnFrame does for its contexts.
class EL_FactionSelectionMenu
{
	protected Widget m_wRoot;
	protected PlayerController m_PlayerController;
	protected ref LF_UIMenuInput m_Input = new LF_UIMenuInput();

	//------------------------------------------------------------------------------------------------
	//! Creates the layout on the workspace and wires the two faction buttons.
	//! \return The controller, or null when the layout failed to load.
	static EL_FactionSelectionMenu Open(PlayerController playerController)
	{
		EL_FactionSelectionMenu menu = new EL_FactionSelectionMenu();
		if (!menu.OpenInternal(playerController))
			return null;

		return menu;
	}

	//------------------------------------------------------------------------------------------------
	protected bool OpenInternal(PlayerController playerController)
	{
		m_PlayerController = playerController;

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		m_wRoot = workspace.CreateWidgets("{41489243B750D7EC}UI/Layouts/FactionSelectionMenu.layout");
		if (!m_wRoot)
		{
			EL_Debug.Error("FactionMenu", "failed to create the faction selection layout");
			return false;
		}

		SCR_ButtonTextComponent civilian = SCR_ButtonTextComponent.GetButtonText("CivilianButton", m_wRoot);
		if (civilian)
			civilian.m_OnClicked.Insert(OnCivilianClicked);

		SCR_ButtonTextComponent police = SCR_ButtonTextComponent.GetButtonText("PoliceButton", m_wRoot);
		if (police)
			police.m_OnClicked.Insert(OnPoliceClicked);

		// Pad: focus starts on the first faction; MenuUp/MenuDown move, MenuSelect picks.
		array<Widget> focus = {};
		focus.Insert(m_wRoot.FindAnyWidget("CivilianButton"));
		focus.Insert(m_wRoot.FindAnyWidget("PoliceButton"));
		m_Input.SetFocusOrder(focus);

		// 0ms delay = every frame; removed again in Close().
		GetGame().GetCallqueue().CallLater(ActivateMenuContext, 0, true);

		EL_Debug.Info("FactionMenu", string.Format("open: root=%1 civilian=%2 police=%3", m_wRoot != null, civilian != null, police != null));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void ActivateMenuContext()
	{
		// Tick activates MenuContext (+ MenuWithEditorContext for MenuUp/MenuDown) and moves focus.
		// MenuBack is ignored: this choice is mandatory.
		if (m_Input.Tick() != LF_EUIMenuInput.SELECT)
			return;

		// Pressing can Close, which removes this repeating call; never do that from inside it.
		GetGame().GetCallqueue().CallLater(PressWidget, 0, false, m_Input.GetSelected());
	}

	//------------------------------------------------------------------------------------------------
	protected void PressWidget(Widget pressed)
	{
		// Already closed by a click on the same press.
		if (!m_wRoot || !pressed)
			return;

		if (pressed.GetName() == "CivilianButton")
			OnCivilianClicked();
		else if (pressed.GetName() == "PoliceButton")
			OnPoliceClicked();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnCivilianClicked()
	{
		Select(EL_Faction.CIVILIAN);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnPoliceClicked()
	{
		// The whitelist lives on the server; it decides in ServerSelectFaction and re-opens this
		// menu with a notification when the player is not cleared.
		Select(EL_Faction.POLICE);
	}

	//------------------------------------------------------------------------------------------------
	protected void Select(EL_Faction faction)
	{
		EL_Debug.Info("FactionMenu", typename.EnumToString(EL_Faction, faction) + " selected");
		Close();

		// The server validates the pick and answers by opening the spawn location menu.
		SCR_PlayerController controller = SCR_PlayerController.Cast(m_PlayerController);
		if (!controller)
		{
			EL_Debug.Error("FactionMenu", "faction pick not sent: no SCR_PlayerController");
			return;
		}

		controller.EL_AskSelectFaction(faction);
	}

	//------------------------------------------------------------------------------------------------
	void Close()
	{
		GetGame().GetCallqueue().Remove(ActivateMenuContext);

		if (!m_wRoot)
			return;

		m_wRoot.RemoveFromHierarchy();
		m_wRoot = null;
	}
};