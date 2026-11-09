//------------------------------------------------------------------------------------------------
//! Client-side death screen. Shown when the server tells the local player they are ready to
//! respawn (SCR_RespawnComponent.GetOnRespawnReadyInvoker_O, which LF_SpawnLogic triggers on
//! death). A single Respawn button re-runs the account-aware spawn path; the body stays where
//! it fell with everything it carried.
//!
//! Deliberately a workspace widget (not a MenuManager menu) for the same reason as
//! LF_UIScreen: in a Workbench play session OpenMenu leaves the menu root
//! orphaned. While open, the vanilla MenuContext is re-activated every frame so a cursor
//! appears and the button can be clicked.
class LF_DeathScreen
{
	protected static ref LF_DeathScreen s_Instance;

	protected Widget m_wRoot;
	protected PlayerController m_PlayerController;
	protected ref LF_UIMenuInput m_Input = new LF_UIMenuInput();

	//------------------------------------------------------------------------------------------------
	static LF_DeathScreen GetInstance()
	{
		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	//! Creates the death screen on the workspace and wires the respawn button.
	//! \return The controller, or null when the layout failed to load.
	static LF_DeathScreen Open(PlayerController playerController)
	{
		if (s_Instance)
			return s_Instance;

		LF_DeathScreen screen = new LF_DeathScreen();
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
			LF_Debug.Error("Death", "failed to create the death screen layout");
			return false;
		}

		SCR_ButtonTextComponent respawnButton = SCR_ButtonTextComponent.GetButtonText("RespawnButton", m_wRoot);
		if (respawnButton)
			respawnButton.m_OnClicked.Insert(OnRespawnClicked);
		else
			LF_Debug.Error("Death", "death screen loaded but the RespawnButton is missing");

		// Pad: Respawn has focus at once, so MenuSelect respawns.
		array<Widget> focus = {};
		focus.Insert(m_wRoot.FindAnyWidget("RespawnButton"));
		m_Input.SetFocusOrder(focus);

		// 0ms delay = every frame; removed again in Close().
		GetGame().GetCallqueue().CallLater(ActivateMenuContext, 0, true);

		LF_Debug.Info("Death", "death screen open");
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

		if (pressed.GetName() == "RespawnButton")
			OnRespawnClicked();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnRespawnClicked()
	{
		LF_Debug.Info("Death", "respawn requested from the death screen");
		Close();

		SCR_RespawnComponent respawn = SCR_RespawnComponent.SGetLocalRespawnComponent();
		if (respawn)
			respawn.LF_AskRespawn();
		else
			LF_Debug.Error("Death", "no local SCR_RespawnComponent to request a respawn");
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