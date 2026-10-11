//------------------------------------------------------------------------------------------------
//! Organization desk. Empty state is the create row. After create the menu lists
//! the name, the fund balance, and the processing-slot upgrade.
//------------------------------------------------------------------------------------------------
class EL_OrganizationMenu : ChimeraMenuBase
{
	protected Widget m_wRoot;
	protected TextWidget m_wName;
	protected TextWidget m_wFund;
	protected TextWidget m_wUpgrade;
	protected EditBoxWidget m_wNameEdit;
	protected EditBoxWidget m_wAmount;
	protected ButtonWidget m_wCreate;
	protected ButtonWidget m_wDeposit;
	protected ButtonWidget m_wWithdraw;
	protected ButtonWidget m_wBuy;
	protected ButtonWidget m_wClose;
	protected ref map<string, float> m_mActionStampMs;
	protected bool m_bExtraStorage;
	protected bool m_bParkedVehicle;

	protected static const float ACTION_DEBOUNCE_MS = 200;

	//------------------------------------------------------------------------------------------------
	override void OnMenuOpen()
	{
		super.OnMenuOpen();

		m_wRoot = GetRootWidget();
		m_wName = TextWidget.Cast(m_wRoot.FindAnyWidget("NameText"));
		m_wFund = TextWidget.Cast(m_wRoot.FindAnyWidget("FundText"));
		m_wUpgrade = TextWidget.Cast(m_wRoot.FindAnyWidget("UpgradeText"));
		m_wNameEdit = EditBoxWidget.Cast(m_wRoot.FindAnyWidget("NameEdit"));
		m_wAmount = EditBoxWidget.Cast(m_wRoot.FindAnyWidget("AmountEdit"));
		m_wCreate = ButtonWidget.Cast(m_wRoot.FindAnyWidget("CreateButton"));
		m_wDeposit = ButtonWidget.Cast(m_wRoot.FindAnyWidget("DepositButton"));
		m_wWithdraw = ButtonWidget.Cast(m_wRoot.FindAnyWidget("WithdrawButton"));
		m_wBuy = ButtonWidget.Cast(m_wRoot.FindAnyWidget("BuyButton"));
		m_wClose = ButtonWidget.Cast(m_wRoot.FindAnyWidget("CloseButton"));

		ShowEmpty();

		SCR_ChimeraCharacter character = LocalCharacter();
		if (character)
			character.EL_AskOrganizationView();
	}

	//------------------------------------------------------------------------------------------------
	override void OnMenuUpdate(float tDelta)
	{
		super.OnMenuUpdate(tDelta);

		InputManager input = GetGame().GetInputManager();
		if (!input)
			return;

		input.ActivateContext("MenuContext");
		input.ActivateContext("MenuWithEditorContext");

		if (input.GetActionTriggered("MenuBack"))
			OnClose();
	}

	//------------------------------------------------------------------------------------------------
	override bool OnClick(Widget w, int x, int y, int button)
	{
		if (w == m_wCreate)
		{
			OnCreate();
			return true;
		}
		if (w == m_wDeposit)
		{
			OnDeposit();
			return true;
		}
		if (w == m_wWithdraw)
		{
			OnWithdraw();
			return true;
		}
		if (w == m_wBuy)
		{
			OnBuy();
			return true;
		}
		if (w == m_wClose)
		{
			OnClose();
			return true;
		}

		return super.OnClick(w, x, y, button);
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
}
