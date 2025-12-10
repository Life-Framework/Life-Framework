class EL_GroupMenu : ChimeraMenuBase
{
    protected EL_GroupComponent m_GroupComponent;
    protected Widget m_RootWidget;
    protected ButtonWidget m_CreateButton;
    protected ButtonWidget m_InviteButton;
    protected ButtonWidget m_KickButton;
    protected ButtonWidget m_LeaveButton;
    protected ButtonWidget m_DepositButton;
    protected ButtonWidget m_WithdrawButton;
    protected EditBoxWidget m_AmountInput;
    protected TextWidget m_BankText;
    protected TextWidget m_GroupNameText;
    protected TextWidget m_MembersText;
    protected EditBoxWidget m_GroupNameInput;
    protected ButtonWidget m_DisbandButton;
    protected ButtonWidget m_CloseButton;

    // Get layout
    string GetLayoutFile()
    {
        return "{5A60AE2B8D435CE9}UI/Layouts/GroupMenu.layout";
    }

    // Override to initialize
    override void OnMenuInit()
    {
        m_RootWidget = GetRootWidget();

        // Setup widgets
        m_GroupNameText = TextWidget.Cast(m_RootWidget.FindWidget("GroupNameText"));
        m_MembersText = TextWidget.Cast(m_RootWidget.FindWidget("MembersText"));
        m_GroupNameInput = EditBoxWidget.Cast(m_RootWidget.FindWidget("GroupNameInput"));
        m_BankText = TextWidget.Cast(m_RootWidget.FindWidget("BankText"));
        m_AmountInput = EditBoxWidget.Cast(m_RootWidget.FindWidget("AmountInput"));
        m_CreateButton = ButtonWidget.Cast(m_RootWidget.FindWidget("CreateButton"));
        m_InviteButton = ButtonWidget.Cast(m_RootWidget.FindWidget("InviteButton"));
        m_KickButton = ButtonWidget.Cast(m_RootWidget.FindWidget("KickButton"));
        m_LeaveButton = ButtonWidget.Cast(m_RootWidget.FindWidget("LeaveButton"));
        m_DisbandButton = ButtonWidget.Cast(m_RootWidget.FindWidget("DisbandButton"));
        m_DepositButton = ButtonWidget.Cast(m_RootWidget.FindWidget("DepositButton"));
        m_WithdrawButton = ButtonWidget.Cast(m_RootWidget.FindWidget("WithdrawButton"));
        m_CloseButton = ButtonWidget.Cast(m_RootWidget.FindWidget("CloseButton"));

        // Button events handled in OnClick override
        // (avoids relying on engine private m_OnClicked member)

        UpdateUI();
    }

    override bool OnClick(Widget w, int x, int y, int button)
    {
        if (w == m_CreateButton)
        {
            OnCreateButtonClicked(ButtonWidget.Cast(w));
            return true;
        }

        if (w == m_InviteButton)
        {
            OnInviteButtonClicked(ButtonWidget.Cast(w));
            return true;
        }

        if (w == m_KickButton)
        {
            OnKickButtonClicked(ButtonWidget.Cast(w));
            return true;
        }

        if (w == m_LeaveButton)
        {
            OnLeaveButtonClicked(ButtonWidget.Cast(w));
            return true;
        }

        if (w == m_DepositButton)
        {
            OnDepositButtonClicked(ButtonWidget.Cast(w));
            return true;
        }

        if (w == m_WithdrawButton)
        {
            OnWithdrawButtonClicked(ButtonWidget.Cast(w));
            return true;
        }

        return false;
    }

    // Update UI based on group state
    void UpdateUI()
    {
        if (m_GroupComponent)
        {
            // Show group info
            if (m_GroupNameText)
                m_GroupNameText.SetText("Group: " + m_GroupComponent.GetGroupName());
            if (m_BankText)
                m_BankText.SetText("Group Bank: $" + m_GroupComponent.GetBank().ToString());
            // TODO: Show members
        }
        else
        {
            // Show create group UI
            if (m_GroupNameText)
                m_GroupNameText.SetText("Create Group");
            if (m_BankText)
                m_BankText.SetText("Group Bank: $0");
        }
    }

    // Create group
    void OnCreateButtonClicked(ButtonWidget button)
    {
        if (m_GroupNameInput)
        {
            string name = m_GroupNameInput.GetText();
            if (name != "")
            {
                // TODO: Create group
                Print("Creating group: " + name);
            }
        }
    }

    // Invite player
    void OnInviteButtonClicked(ButtonWidget button)
    {
        EL_GroupInviteDialog dialog = EL_GroupInviteDialog.Cast(GetGame().GetMenuManager().OpenDialog(ChimeraMenuPreset.LF_GroupInviteDialog));
        if (dialog)
        {
            dialog.SetGroupComponent(m_GroupComponent);
        }
    }

    // Kick member
    void OnKickButtonClicked(ButtonWidget button)
    {
        // TODO: Kick selected member
        Print("Kick member");
    }

    // Leave group
    void OnLeaveButtonClicked(ButtonWidget button)
    {
        // TODO: Leave group
        Print("Leave group");
    }

    // Disband group
    void OnDisbandButtonClicked(ButtonWidget button)
    {
        // TODO: Disband group
        Print("Disband group");
    }

    // Deposit button clicked
    void OnDepositButtonClicked(ButtonWidget button)
    {
        if (m_AmountInput && m_GroupComponent)
        {
            string amountStr = m_AmountInput.GetText();
            int amount = amountStr.ToInt();
            if (amount > 0)
            {
                // TODO: Get player UID and check money
                string playerUID = "player1"; // Placeholder
                if (m_GroupComponent.DepositMoney(amount, playerUID))
                {
                    UpdateUI();
                    Print("Deposited $" + amount.ToString());
                }
            }
        }
    }

    // Withdraw button clicked
    void OnWithdrawButtonClicked(ButtonWidget button)
    {
        if (m_AmountInput && m_GroupComponent)
        {
            string amountStr = m_AmountInput.GetText();
            int amount = amountStr.ToInt();
            if (amount > 0)
            {
                // TODO: Get player UID
                string playerUID = "player1"; // Placeholder
                if (m_GroupComponent.WithdrawMoney(amount, playerUID))
                {
                    UpdateUI();
                    Print("Withdrew $" + amount.ToString());
                }
            }
        }
    }

    // Set group component
    void SetGroupComponent(EL_GroupComponent comp)
    {
        m_GroupComponent = comp;
        UpdateUI();
    }
};