//------------------------------------------------------------------------------------------------
//! Client-side death screen. Shown when the server tells the local player they are ready to
//! respawn (SCR_RespawnComponent.GetOnRespawnReadyInvoker_O, which EL_SpawnLogic triggers on
//! death). A single Respawn button re-runs the account-aware spawn path; the body stays where
//! it fell with everything it carried.
//!
//! Deliberately a workspace widget (not a MenuManager menu) for the same reason as
//! EL_FactionSelectionMenu: in a Workbench play session OpenMenu leaves the menu root
//! orphaned. While open, the vanilla MenuContext is re-activated every frame so a cursor
//! appears and the button can be clicked.
class EL_DeathScreen
{
	protected static ref EL_DeathScreen s_Instance;

	protected Widget m_wRoot;
	protected PlayerController m_PlayerController;

	//------------------------------------------------------------------------------------------------
	static EL_DeathScreen GetInstance()
	{
		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	//! Creates the death screen on the workspace and wires the respawn button.
	//! \return The controller, or null when the layout failed to load.
	static EL_DeathScreen Open(PlayerController playerController)
	{
		if (s_Instance)
			return s_Instance;

		EL_DeathScreen screen = new EL_DeathScreen();
		if (!screen.OpenInternal(playerController))
			return null;

		s_Instance = screen;
		return screen;
	}

	//------------------------------------------------------------------------------------------------
	protected bool OpenInternal(PlayerController playerController)
	{
		if (!GetGame().GetWorkspace())
			return false;

		m_PlayerController = playerController;

		m_wRoot = GetGame().GetWorkspace().CreateWidgets("{1CBC0B1329A8762C}UI/Layouts/DeathScreen.layout");
		if (!m_wRoot)
		{
			EL_Debug.Error("Death", "failed to create the death screen layout");
			return false;
		}

		SCR_ButtonTextComponent respawnButton = SCR_ButtonTextComponent.GetButtonText("RespawnButton", m_wRoot);
		if (respawnButton)
			respawnButton.m_OnClicked.Insert(OnRespawnClicked);
		else
			EL_Debug.Error("Death", "death screen loaded but the RespawnButton is missing");

		// 0ms delay = every frame; removed again in Close().
		GetGame().GetCallqueue().CallLater(ActivateMenuContext, 0, true);

		EL_Debug.Info("Death", "death screen open");
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
	protected void OnRespawnClicked()
	{
		EL_Debug.Info("Death", "respawn requested from the death screen");
		Close();

		SCR_RespawnComponent respawn = SCR_RespawnComponent.SGetLocalRespawnComponent();
		if (respawn)
			respawn.EL_AskRespawn();
		else
			EL_Debug.Error("Death", "no local SCR_RespawnComponent to request a respawn");
	}

	//------------------------------------------------------------------------------------------------
	void Close()
	{
		GetGame().GetCallqueue().Remove(ActivateMenuContext);

		if (m_wRoot)
		{
			m_wRoot.RemoveFromHierarchy();
			m_wRoot = null;
		}

		s_Instance = null;
	}
}