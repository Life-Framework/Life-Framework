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
	//! What respawning costs, from the respawn config the client already has (the pack's bill when it
	//! sets one). No config: no bill sentence, the other lines stay.
	static string CostText()
	{
		int bill = 0;
		LF_RespawnConfig config = LF_RespawnConfig.Get();
		if (config)
			bill = config.m_iBillAmount;

		return BuildCostText(bill);
	}

	//------------------------------------------------------------------------------------------------
	//! "You wake at the hospital. The bill is $150. What you carried stays with your body." A bill
	//! of 0 or less drops the bill sentence.
	static string BuildCostText(int bill)
	{
		string text = WidgetManager.Translate("#LF-Death_CostWake");
		if (bill > 0)
			text = text + " " + LF_Terms.Text("#LF-Death_CostBill", bill.ToString());

		// The currency symbol is a term ({currency:sym}) a pack can rename.
		return LF_Terms.Expand(text + " " + WidgetManager.Translate("#LF-Death_CostKeeps"));
	}

	//------------------------------------------------------------------------------------------------
	protected bool OpenInternal(PlayerController playerController)
	{
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return false;

		m_PlayerController = playerController;

		m_wRoot = workspace.CreateWidgets("{1CBC0B1329A8762C}UI/Layouts/DeathScreen.layout");
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

		// The price before the press: the bill and what the body keeps.
		RichTextWidget cost = RichTextWidget.Cast(m_wRoot.FindAnyWidget("CostText"));
		if (cost)
			cost.SetText(CostText());
		else
			LF_Debug.Error("Death", "death screen loaded but the CostText is missing");

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
	//! Called by LF_DeathScreen_StaticResetHook when a world ends: takes down a screen nobody answered.
	static void ResetStatics()
	{
		if (s_Instance)
			s_Instance.Close();

		s_Instance = null;
	}

	//------------------------------------------------------------------------------------------------
	void Close()
	{
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(ActivateMenuContext);

		if (m_wRoot)
		{
			m_wRoot.RemoveFromHierarchy();
			m_wRoot = null;
		}

		s_Instance = null;
	}
}
//------------------------------------------------------------------------------------------------
//! World-end reset of LF_DeathScreen's statics (LF_StaticReset finds it by reflection).
//! Only the Respawn button closed the screen, so a death followed by a disconnect or a server
//! restart kept the stale instance: the next death returned it and showed no screen.
class LF_DeathScreen_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_DeathScreen";
	}

	override void Apply()
	{
		LF_DeathScreen.ResetStatics();
	}
}
