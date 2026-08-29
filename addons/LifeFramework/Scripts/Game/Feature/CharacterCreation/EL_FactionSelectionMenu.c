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

		// 0ms delay = every frame; removed again in Close().
		GetGame().GetCallqueue().CallLater(ActivateMenuContext, 0, true);

		EL_Debug.Info("FactionMenu", string.Format("open: root=%1 civilian=%2 police=%3", m_wRoot != null, civilian != null, police != null));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void ActivateMenuContext()
	{
		InputManager inputManager = GetGame().GetInputManager();
		if (inputManager)
			inputManager.ActivateContext("MenuContext");
	}

	//------------------------------------------------------------------------------------------------
	protected void OnCivilianClicked()
	{
		Select(EL_Faction.CIVILIAN);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnPoliceClicked()
	{
		Select(EL_Faction.POLICE);
	}

	//------------------------------------------------------------------------------------------------
	protected void Select(EL_Faction faction)
	{
		EL_Debug.Info("FactionMenu", typename.EnumToString(EL_Faction, faction) + " selected");
		Close();

		if (!m_PlayerController)
			return;

		EL_PlayerAccountManager accountManager = EL_PlayerAccountManager.GetInstance();
		if (!accountManager)
			return;

		string playerUid = EL_Utils.GetPlayerUID(m_PlayerController.GetPlayerId());
		if (playerUid.IsEmpty())
			return;

		EL_PlayerAccount account = accountManager.GetAccount(playerUid);
		if (account)
		{
			account.SetFaction(faction);
			accountManager.SaveAndReleaseAccount(account);
		}

		// Advance the flow: the re-entrant pass sees the chosen faction and spawns the character.
		EL_CharacterCreationManager manager = EL_CharacterCreationManager.GetInstance();
		if (manager)
			manager.OnPlayerConnected(m_PlayerController.GetPlayerId());
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