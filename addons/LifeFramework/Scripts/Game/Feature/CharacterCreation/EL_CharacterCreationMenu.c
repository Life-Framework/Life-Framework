[BaseContainerProps(configRoot: true)]
class EL_CharacterCreationMenu : ChimeraMenuBase
{
	protected PlayerController m_PlayerController;

	protected Widget m_wRoot;
	protected EditBoxWidget m_wFirstName;
	protected EditBoxWidget m_wLastName;
	protected EditBoxWidget m_wAge;
	protected ButtonWidget m_wCreateButton;

	//------------------------------------------------------------------------------------------------
	override void OnMenuOpen()
	{
		super.OnMenuOpen();

		m_wRoot = GetRootWidget();
		EL_Utils.EnsureMenuRootAttached(m_wRoot);

		// The edit boxes are ButtonWidget wrappers around the actual EditBox widget (the WLib_EditBox
		// prefab names its inner edit box "EditBox"), so each lookup scopes through the wrapper name.
		m_wFirstName = EditBoxWidget.Cast(m_wRoot.FindAnyWidget("FirstNameEditBox").FindAnyWidget("EditBox"));
		m_wLastName = EditBoxWidget.Cast(m_wRoot.FindAnyWidget("LastNameEditBox").FindAnyWidget("EditBox"));
		m_wAge = EditBoxWidget.Cast(m_wRoot.FindAnyWidget("AgeEditBox").FindAnyWidget("EditBox"));
		m_wCreateButton = ButtonWidget.Cast(m_wRoot.FindAnyWidget("CreateButton"));
	}

	//------------------------------------------------------------------------------------------------
	override bool OnClick(Widget w, int x, int y, int button)
	{
		if (w == m_wCreateButton)
		{
			CreateCharacter();
			return true;
		}

		return super.OnClick(w, x, y, button);
	}

	//------------------------------------------------------------------------------------------------
	void SetPlayerController(PlayerController playerController)
	{
		m_PlayerController = playerController;
	}

	//------------------------------------------------------------------------------------------------
	protected void CreateCharacter()
	{
		if (!m_PlayerController)
			return;

		string firstName = m_wFirstName.GetText();
		string lastName = m_wLastName.GetText();
		string ageText = m_wAge.GetText();

		if (firstName.IsEmpty() || lastName.IsEmpty() || ageText.IsEmpty())
		{
			// Show error message
			return;
		}

		int age = ageText.ToInt();
		if (age < 18 || age > 80)
		{
			// Show error message
			return;
		}

		// Notify manager
		EL_CharacterCreationManager manager = EL_CharacterCreationManager.GetInstance();
		if (manager)
			manager.OnCharacterCreated(m_PlayerController, firstName, lastName, age);

		// Close menu
		GetGame().GetMenuManager().CloseMenu(this);
	}
};