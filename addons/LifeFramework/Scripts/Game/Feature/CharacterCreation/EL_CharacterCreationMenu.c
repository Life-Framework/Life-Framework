//! Name/age dialog for first-time players. Same structure as EL_FactionSelectionMenu: a plain
//! workspace widget created with CreateWidgets (the MenuManager is unusable for menus in a
//! Workbench play session), with a per-frame input context driver - MenuTextEditContext while one
//! of the edit boxes is focused so keystrokes reach it, MenuContext otherwise so the cursor and
//! menu actions stay alive. The engine's own focus-driven MenuWidgetActiveContext (priority 990)
//! outranks both while the player types.
class EL_CharacterCreationMenu
{
	protected Widget m_wRoot;
	protected PlayerController m_PlayerController;
	protected EditBoxWidget m_wFirstName;
	protected EditBoxWidget m_wLastName;
	protected EditBoxWidget m_wAge;
	protected TextWidget m_wError;

	//------------------------------------------------------------------------------------------------
	//! Creates the layout on the workspace, wires the create button and starts the input driver.
	//! \return The controller, or null when the layout failed to load.
	static EL_CharacterCreationMenu Open(PlayerController playerController)
	{
		EL_CharacterCreationMenu menu = new EL_CharacterCreationMenu();
		if (!menu.OpenInternal(playerController))
			return null;

		return menu;
	}

	//------------------------------------------------------------------------------------------------
	protected bool OpenInternal(PlayerController playerController)
	{
		m_PlayerController = playerController;

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		m_wRoot = workspace.CreateWidgets("{D9C065A9AD23B91B}UI/Layouts/EL_CharacterCreationMenu.layout");
		if (!m_wRoot)
		{
			EL_Debug.Error("CharacterCreation", "failed to create the character creation layout");
			return false;
		}

		// The edit boxes are ButtonWidget wrappers around the actual EditBox widget (the WLib_EditBox
		// prefab names its inner edit box "EditBox"), so each lookup scopes through the wrapper name.
		m_wFirstName = EditBoxWidget.Cast(m_wRoot.FindAnyWidget("FirstNameEditBox").FindAnyWidget("EditBox"));
		m_wLastName = EditBoxWidget.Cast(m_wRoot.FindAnyWidget("LastNameEditBox").FindAnyWidget("EditBox"));
		m_wAge = EditBoxWidget.Cast(m_wRoot.FindAnyWidget("AgeEditBox").FindAnyWidget("EditBox"));
		m_wError = TextWidget.Cast(m_wRoot.FindAnyWidget("ErrorText"));

		if (!m_wFirstName || !m_wLastName || !m_wAge)
		{
			EL_Debug.Error("CharacterCreation", "edit box widgets not found in the creation layout");
			return false;
		}

		SCR_ButtonTextComponent createButton = SCR_ButtonTextComponent.GetButtonText("CreateButton", m_wRoot);
		if (createButton)
			createButton.m_OnClicked.Insert(OnCreateClicked);

		// 0ms delay = every frame; removed again in Close().
		GetGame().GetCallqueue().CallLater(ActivateInputContext, 0, true);

		EL_Debug.Info("CharacterCreation", string.Format("creation menu open: editboxes=%1%2%3 create=%4", m_wFirstName != null, m_wLastName != null, m_wAge != null, createButton != null));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void ActivateInputContext()
	{
		InputManager inputManager = GetGame().GetInputManager();
		if (!inputManager)
			return;

		Widget focused = GetGame().GetWorkspace().GetFocusedWidget();
		if (focused && EditBoxWidget.Cast(focused))
			inputManager.ActivateContext("MenuTextEditContext");
		else
			inputManager.ActivateContext("MenuContext");
	}

	//------------------------------------------------------------------------------------------------
	protected void OnCreateClicked()
	{
		if (!m_wFirstName || !m_wLastName || !m_wAge)
			return;

		string firstName = m_wFirstName.GetText();
		string lastName = m_wLastName.GetText();
		string ageText = m_wAge.GetText();

		if (firstName.IsEmpty() || lastName.IsEmpty() || ageText.IsEmpty())
		{
			ShowError("#EL-CC_Error_Fields");
			return;
		}

		int age = ageText.ToInt();
		if (age < 18 || age > 80)
		{
			ShowError("#EL-CC_Error_Age");
			return;
		}

		EL_Debug.Info("CharacterCreation", "character created: " + firstName + " " + lastName + ", age " + age);
		Close();

		EL_CharacterCreationManager manager = EL_CharacterCreationManager.GetInstance();
		if (manager)
			manager.OnCharacterCreated(m_PlayerController, firstName, lastName, age);
	}

	//------------------------------------------------------------------------------------------------
	//! Shows a validation message. TextWidget.SetText translates #-prefixed stringtable keys.
	protected void ShowError(string key)
	{
		EL_Debug.Info("CharacterCreation", "validation error: " + key);
		if (m_wError)
			m_wError.SetText(key);
	}

	//------------------------------------------------------------------------------------------------
	void Close()
	{
		GetGame().GetCallqueue().Remove(ActivateInputContext);

		if (!m_wRoot)
			return;

		m_wRoot.RemoveFromHierarchy();
		m_wRoot = null;
	}
};