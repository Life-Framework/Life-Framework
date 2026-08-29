class EL_FactionSelectionMenu : ChimeraMenuBase
{
	protected PlayerController m_PlayerController;
	protected ButtonWidget m_wCivilianButton;
	protected ButtonWidget m_wPoliceButton;

	//------------------------------------------------------------------------------------------------
	void SetPlayerController(PlayerController playerController)
	{
		m_PlayerController = playerController;
	}

	//------------------------------------------------------------------------------------------------
	override void OnMenuOpen()
	{
		super.OnMenuOpen();

		Widget root = GetRootWidget();
		EL_Utils.EnsureMenuRootAttached(root);
		DumpRootState("OnMenuOpen");

		m_wCivilianButton = ButtonWidget.Cast(root.FindAnyWidget("CivilianButton"));
		m_wPoliceButton = ButtonWidget.Cast(root.FindAnyWidget("PoliceButton"));
		Print(string.Format("[EL_FactionSelectionMenu] open: root=%1 civilianBtn=%2 policeBtn=%3", root != null, m_wCivilianButton != null, m_wPoliceButton != null), LogLevel.NORMAL);
		GetGame().GetCallqueue().CallLater(DumpRootState, 1000, false, "plus1s");
	}

	//------------------------------------------------------------------------------------------------
	//! Diagnostics: is the menu root attached to the workspace and set visible? A detached or
	//! hidden root renders nothing while every lookup on it still works.
	protected void DumpRootState(string tag)
	{
		Widget root = GetRootWidget();
		if (!root)
		{
			Print("[EL_FactionSelectionMenu] " + tag + ": no root widget", LogLevel.NORMAL);
			return;
		}

		float sx = FrameSlot.GetSizeX(root);
		float sy = FrameSlot.GetSizeY(root);
		Print(string.Format("[EL_FactionSelectionMenu] %1: name=%2 parent=%3 visible=%4 size=%5x%6", tag, root.GetName(), root.GetParent() != null, root.IsVisibleInHierarchy(), sx, sy), LogLevel.NORMAL);

		// The MenuManager sizes menu roots after OnMenuOpen and can collapse a late-attached one;
		// re-assert the full-screen rect once on the follow-up tick if that happened.
if (tag == "plus1s" && (sx < 1 || sy < 1))
		{
			FrameSlot.SetPos(root, 0, 0);
			FrameSlot.SetSize(root, GetGame().GetWorkspace().GetWidth(), GetGame().GetWorkspace().GetHeight());
			Print(string.Format("[EL_FactionSelectionMenu] re-asserted size: %1x%2", FrameSlot.GetSizeX(root), FrameSlot.GetSizeY(root)), LogLevel.NORMAL);
		}
	}

	//------------------------------------------------------------------------------------------------
	override bool OnClick(Widget w, int x, int y, int button)
	{
		if (w == m_wCivilianButton)
		{
			Print("[EL_FactionSelectionMenu] Civilian clicked", LogLevel.NORMAL);
			OnFactionSelected(EL_Faction.CIVILIAN);
			return true;
		}

		if (w == m_wPoliceButton)
		{
			Print("[EL_FactionSelectionMenu] Police clicked", LogLevel.NORMAL);
			OnFactionSelected(EL_Faction.POLICE);
			return true;
		}

		return super.OnClick(w, x, y, button);
	}

	//------------------------------------------------------------------------------------------------
	void OnFactionSelected(EL_Faction faction)
	{
		if (!m_PlayerController)
			return;

		// Set faction in account
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

		// Proceed to character creation
		EL_CharacterCreationManager manager = EL_CharacterCreationManager.GetInstance();
		if (manager)
			manager.OnPlayerConnected(m_PlayerController.GetPlayerId());

		// Close menu
		GetGame().GetMenuManager().CloseMenu(this);
	}
};