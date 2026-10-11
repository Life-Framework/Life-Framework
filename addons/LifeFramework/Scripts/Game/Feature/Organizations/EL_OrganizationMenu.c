//------------------------------------------------------------------------------------------------
//! Organization desk. Empty state is the create row. After create the menu lists
//! the name, the fund balance, and the processing-slot upgrade.
//!
//! Deliberately a workspace widget. MenuManager.OpenMenu leaves the root
//! orphaned in a Workbench play session, so the tree exists and nothing renders.
//! Same path as EL_FactionSelectionMenu and EL_CharacterCreationMenu.
//------------------------------------------------------------------------------------------------
class EL_OrganizationMenu
{
	protected static ref EL_OrganizationMenu s_Instance;

	protected Widget m_wRoot;
	protected TextWidget m_wName;
	protected TextWidget m_wFund;
	protected TextWidget m_wUpgrade;
	protected EditBoxWidget m_wNameEdit;
	protected EditBoxWidget m_wAmount;
	protected ref map<string, float> m_mActionStampMs;
	protected bool m_bExtraStorage;
	protected bool m_bParkedVehicle;

	protected static const float ACTION_DEBOUNCE_MS = 200;

	//------------------------------------------------------------------------------------------------
	static string LayoutResource()
	{
		return "{977FCAE08556886B}UI/Layouts/OrganizationMenu.layout";
	}

	//------------------------------------------------------------------------------------------------
	//! The desk used to call MenuManager.OpenMenu. That path stays closed.
	static bool OpensThroughMenuManager()
	{
		return false;
	}

	//------------------------------------------------------------------------------------------------
	static EL_OrganizationMenu GetInstance()
	{
		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	//! Creates the layout on the workspace and wires the desk buttons.
	//! \return The controller, or null when the layout failed to load.
	static EL_OrganizationMenu Open()
	{
		if (s_Instance)
			return s_Instance;

		EL_OrganizationMenu menu = new EL_OrganizationMenu();
		if (!menu.OpenInternal())
			return null;

		s_Instance = menu;
		return menu;
	}

	//------------------------------------------------------------------------------------------------
	protected bool OpenInternal()
	{
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
		{
			EL_Debug.Error("Organizations", "organization menu failed to open: no workspace");
			return false;
		}

		m_wRoot = workspace.CreateWidgets(LayoutResource());
		if (!m_wRoot)
		{
			EL_Debug.Error("Organizations", "organization menu failed to open: CreateWidgets returned null");
			return false;
		}

		m_wName = TextWidget.Cast(m_wRoot.FindAnyWidget("NameText"));
		m_wFund = TextWidget.Cast(m_wRoot.FindAnyWidget("FundText"));
		m_wUpgrade = TextWidget.Cast(m_wRoot.FindAnyWidget("UpgradeText"));
		m_wNameEdit = FindEditBox("NameEdit");
		m_wAmount = FindEditBox("AmountEdit");

		if (!m_wName)
			EL_Debug.Error("Organizations", "organization menu missing NameText");
		if (!m_wFund)
			EL_Debug.Error("Organizations", "organization menu missing FundText");
		if (!m_wUpgrade)
			EL_Debug.Error("Organizations", "organization menu missing UpgradeText");

		BindCreate();
		BindDeposit();
		BindWithdraw();
		BindBuy();
		BindClose();

		ShowEmpty();

		GetGame().GetCallqueue().CallLater(ActivateInputContext, 0, true);

		SCR_ChimeraCharacter character = LocalCharacter();
		if (character)
			character.EL_AskOrganizationView();

		EL_Debug.Info("Organizations", "organization menu open");
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected EditBoxWidget FindEditBox(string wrapperName)
	{
		Widget wrapper = m_wRoot.FindAnyWidget(wrapperName);
		if (!wrapper)
		{
			EL_Debug.Error("Organizations", "organization menu missing " + wrapperName);
			return null;
		}

		EditBoxWidget edit = EditBoxWidget.Cast(wrapper.FindAnyWidget("EditBox"));
		if (!edit)
			EL_Debug.Error("Organizations", "organization menu missing " + wrapperName + " EditBox");

		return edit;
	}

	//------------------------------------------------------------------------------------------------
	protected void BindCreate()
	{
		SCR_ButtonTextComponent button = SCR_ButtonTextComponent.GetButtonText("CreateButton", m_wRoot);
		if (!button)
		{
			EL_Debug.Error("Organizations", "organization menu missing CreateButton");
			return;
		}

		button.m_OnClicked.Insert(OnCreate);
	}

	//------------------------------------------------------------------------------------------------
	protected void BindDeposit()
	{
		SCR_ButtonTextComponent button = SCR_ButtonTextComponent.GetButtonText("DepositButton", m_wRoot);
		if (!button)
		{
			EL_Debug.Error("Organizations", "organization menu missing DepositButton");
			return;
		}

		button.m_OnClicked.Insert(OnDeposit);
	}

	//------------------------------------------------------------------------------------------------
	protected void BindWithdraw()
	{
		SCR_ButtonTextComponent button = SCR_ButtonTextComponent.GetButtonText("WithdrawButton", m_wRoot);
		if (!button)
		{
			EL_Debug.Error("Organizations", "organization menu missing WithdrawButton");
			return;
		}

		button.m_OnClicked.Insert(OnWithdraw);
	}

	//------------------------------------------------------------------------------------------------
	protected void BindBuy()
	{
		SCR_ButtonTextComponent button = SCR_ButtonTextComponent.GetButtonText("BuyButton", m_wRoot);
		if (!button)
		{
			EL_Debug.Error("Organizations", "organization menu missing BuyButton");
			return;
		}

		button.m_OnClicked.Insert(OnBuy);
	}

	//------------------------------------------------------------------------------------------------
	protected void BindClose()
	{
		SCR_ButtonTextComponent button = SCR_ButtonTextComponent.GetButtonText("CloseButton", m_wRoot);
		if (!button)
		{
			EL_Debug.Error("Organizations", "organization menu missing CloseButton");
			return;
		}

		button.m_OnClicked.Insert(OnClose);
	}

	//------------------------------------------------------------------------------------------------
	protected void ActivateInputContext()
	{
		InputManager inputManager = GetGame().GetInputManager();
		if (!inputManager)
			return;

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		Widget focused = null;
		if (workspace)
			focused = workspace.GetFocusedWidget();

		if (focused && EditBoxWidget.Cast(focused))
			inputManager.ActivateContext("MenuTextEditContext");
		else
			inputManager.ActivateContext("MenuContext");

		if (inputManager.GetActionTriggered("MenuBack"))
			OnClose();
	}

	//------------------------------------------------------------------------------------------------
	void ApplyView(bool hasOrg, string name, int fund, bool processing, bool extraStorage, bool parkedVehicle)
	{
		m_bExtraStorage = extraStorage;
		m_bParkedVehicle = parkedVehicle;

		if (!hasOrg)
		{
			ShowEmpty();
			return;
		}

		if (m_wName)
			m_wName.SetText(WidgetManager.Translate("#EL-Org_Name") + ": " + name);
		if (m_wFund)
			m_wFund.SetText(WidgetManager.Translate("#EL-Org_Fund") + ": " + EL_MoneyFormat.FormatMoney(fund));
		if (m_wUpgrade)
		{
			if (processing)
				m_wUpgrade.SetText(WidgetManager.Translate("#EL-Org_UpgradeOwned"));
			else
				m_wUpgrade.SetText(WidgetManager.Translate("#EL-Org_UpgradeNone"));
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void ShowEmpty()
	{
		if (m_wName)
			m_wName.SetText(WidgetManager.Translate("#EL-Org_Empty"));
		if (m_wFund)
			m_wFund.SetText("");
		if (m_wUpgrade)
			m_wUpgrade.SetText("");
	}

	//------------------------------------------------------------------------------------------------
	protected void OnCreate()
	{
		if (!ActionReady("Create"))
			return;

		SCR_ChimeraCharacter character = LocalCharacter();
		if (!character)
			return;
		if (!m_wNameEdit)
			return;

		character.EL_AskOrganizationCreate(m_wNameEdit.GetText());
	}

	//------------------------------------------------------------------------------------------------
	protected void OnDeposit()
	{
		if (!ActionReady("Deposit"))
			return;

		SCR_ChimeraCharacter character = LocalCharacter();
		if (!character)
			return;

		character.EL_AskOrganizationDeposit(ReadAmount());
	}

	//------------------------------------------------------------------------------------------------
	protected void OnWithdraw()
	{
		if (!ActionReady("Withdraw"))
			return;

		SCR_ChimeraCharacter character = LocalCharacter();
		if (!character)
			return;

		character.EL_AskOrganizationWithdraw(ReadAmount());
	}

	//------------------------------------------------------------------------------------------------
	protected void OnBuy()
	{
		if (!ActionReady("Buy"))
			return;

		SCR_ChimeraCharacter character = LocalCharacter();
		if (!character)
			return;

		int upgrade = EL_EOrganizationUpgrade.PROCESSING_SLOT;
		character.EL_AskOrganizationBuy(upgrade);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnClose()
	{
		if (!ActionReady("MenuBack"))
			return;

		Close();
	}

	//------------------------------------------------------------------------------------------------
	protected int ReadAmount()
	{
		if (!m_wAmount)
			return 0;

		string amountText = m_wAmount.GetText();
		if (amountText.IsEmpty())
			return 0;

		return amountText.ToInt();
	}

	//------------------------------------------------------------------------------------------------
	protected SCR_ChimeraCharacter LocalCharacter()
	{
		PlayerController playerController = GetGame().GetPlayerController();
		if (!playerController)
			return null;

		return SCR_ChimeraCharacter.Cast(playerController.GetControlledEntity());
	}

	//------------------------------------------------------------------------------------------------
	protected bool ActionReady(string key)
	{
		if (!m_mActionStampMs)
			m_mActionStampMs = new map<string, float>();

		ChimeraWorld world = GetGame().GetWorld();
		if (!world)
			return true;

		float nowMs = world.GetWorldTime();
		float lastMs;
		if (m_mActionStampMs.Find(key, lastMs) && nowMs - lastMs < ACTION_DEBOUNCE_MS)
			return false;

		m_mActionStampMs.Set(key, nowMs);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	void Close()
	{
		GetGame().GetCallqueue().Remove(ActivateInputContext);

		if (m_wRoot)
		{
			m_wRoot.RemoveFromHierarchy();
			m_wRoot = null;
		}

		s_Instance = null;
		EL_Debug.Info("Organizations", "organization menu closed");
	}
}
