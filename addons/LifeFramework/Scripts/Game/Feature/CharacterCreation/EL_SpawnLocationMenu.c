//! Second stage of the spawn menu: after picking a faction, the player picks a spawn
//! location. Same structure as EL_FactionSelectionMenu (plain widget tree on the
//! workspace, MenuContext re-activated per frame, SCR_ButtonTextComponent invokers).
//! Each faction has two location buttons in the layout; the irrelevant pair is hidden.
class EL_SpawnLocationMenu
{
	protected Widget m_wRoot;
	protected PlayerController m_PlayerController;
	protected EL_Faction m_eFaction;

	//------------------------------------------------------------------------------------------------
	//! \return The controller, or null when the layout failed to load.
	static EL_SpawnLocationMenu Open(PlayerController playerController, EL_Faction faction)
	{
		EL_SpawnLocationMenu menu = new EL_SpawnLocationMenu();
		if (!menu.OpenInternal(playerController, faction))
			return null;

		return menu;
	}

	//------------------------------------------------------------------------------------------------
	protected bool OpenInternal(PlayerController playerController, EL_Faction faction)
	{
		m_PlayerController = playerController;
		m_eFaction = faction;

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		m_wRoot = workspace.CreateWidgets("{41489243B750D910}UI/Layouts/SpawnLocationMenu.layout");
		if (!m_wRoot)
		{
			EL_Debug.Error("SpawnMenu", "failed to create the spawn location layout");
			return false;
		}

		bool civilian = faction == EL_Faction.CIVILIAN;
		Show("CivilianGroup", civilian);
		Show("PoliceGroup", !civilian);

		SCR_ButtonTextComponent town = SCR_ButtonTextComponent.GetButtonText("TownButton", m_wRoot);
		if (town)
			town.m_OnClicked.Insert(OnTownClicked);
		else
			EL_Debug.Error("SpawnMenu", "button not found in layout: TownButton");

		SCR_ButtonTextComponent mine = SCR_ButtonTextComponent.GetButtonText("MineButton", m_wRoot);
		if (mine)
			mine.m_OnClicked.Insert(OnMineClicked);
		else
			EL_Debug.Error("SpawnMenu", "button not found in layout: MineButton");

		SCR_ButtonTextComponent station = SCR_ButtonTextComponent.GetButtonText("StationButton", m_wRoot);
		if (station)
			station.m_OnClicked.Insert(OnStationClicked);
		else
			EL_Debug.Error("SpawnMenu", "button not found in layout: StationButton");

		SCR_ButtonTextComponent outpost = SCR_ButtonTextComponent.GetButtonText("OutpostButton", m_wRoot);
		if (outpost)
			outpost.m_OnClicked.Insert(OnOutpostClicked);
		else
			EL_Debug.Error("SpawnMenu", "button not found in layout: OutpostButton");

		GetGame().GetCallqueue().CallLater(ActivateMenuContext, 0, true);

		EL_Debug.Info("SpawnMenu", "open for faction " + typename.EnumToString(EL_Faction, faction));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void Show(string widgetName, bool visible)
	{
		Widget widget = m_wRoot.FindAnyWidget(widgetName);
		if (widget)
			widget.SetVisible(visible);
	}

	//------------------------------------------------------------------------------------------------
	protected void ActivateMenuContext()
	{
		InputManager inputManager = GetGame().GetInputManager();
		if (inputManager)
			inputManager.ActivateContext("MenuContext");
	}

	//------------------------------------------------------------------------------------------------
	protected void OnTownClicked()
	{
		Choose("town");
	}

	//------------------------------------------------------------------------------------------------
	protected void OnMineClicked()
	{
		Choose("mine");
	}

	//------------------------------------------------------------------------------------------------
	protected void OnStationClicked()
	{
		Choose("station");
	}

	//------------------------------------------------------------------------------------------------
	protected void OnOutpostClicked()
	{
		Choose("outpost");
	}

	//------------------------------------------------------------------------------------------------
	protected void Choose(string locationKey)
	{
		EL_Debug.Info("SpawnMenu", "location '" + locationKey + "' chosen for faction " + typename.EnumToString(EL_Faction, m_eFaction));
		Close();

		if (!m_PlayerController)
			return;

		EL_SpawnLogic.SetSelectedLocation(m_PlayerController.GetPlayerId(), locationKey);

		EL_CharacterCreationManager manager = EL_CharacterCreationManager.GetInstance();
		if (manager)
			manager.OnSpawnLocationChosen(m_PlayerController, m_eFaction);
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
