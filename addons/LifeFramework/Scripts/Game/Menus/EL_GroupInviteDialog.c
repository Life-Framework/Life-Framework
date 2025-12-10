class EL_GroupInviteDialog : ChimeraMenuBase
{
    protected Widget m_RootWidget;
    protected SCR_ListBoxComponent m_PlayerList;
    protected ButtonWidget m_InviteButton;
    protected ButtonWidget m_CancelButton;
    protected EL_GroupComponent m_GroupComponent;
    protected ref array<string> m_PlayerNames = new array<string>();
    protected ref array<string> m_PlayerUIDs = new array<string>();

    // Get layout
    string GetLayoutFile()
    {
        return "{5A60AE2B8D435CE9}UI/Layouts/GroupInviteDialog.layout";
    }

    // Override to initialize
    override void OnMenuInit()
    {
        m_RootWidget = GetRootWidget();

        Widget wPlayerList = m_RootWidget.FindWidget("PlayerList");
        if (wPlayerList)
            m_PlayerList = SCR_ListBoxComponent.Cast(wPlayerList.FindHandler(SCR_ListBoxComponent));
        m_InviteButton = ButtonWidget.Cast(m_RootWidget.FindWidget("InviteButton"));
        m_CancelButton = ButtonWidget.Cast(m_RootWidget.FindWidget("CancelButton"));

        // Button clicks routed via OnClick override

        PopulatePlayerList();
    }

    // Populate list with nearby players not in group
    void PopulatePlayerList()
    {
        if (!m_PlayerList)
            return;

        m_PlayerList.Clear();
        m_PlayerNames.Clear();
        m_PlayerUIDs.Clear();

        // Get all players
        array<IEntity> players = {};
        PlayerManager pm = GetGame().GetPlayerManager();
        if (pm)
        {
            for (int i = 0; i < pm.GetPlayerCount(); i++)
            {
                IEntity player = pm.GetPlayerControlledEntity(i);
                if (player)
                    players.Insert(player);
            }
        }

        // Filter nearby players not in group
        IEntity localPlayer = SCR_PlayerController.GetLocalControlledEntity();
        if (!localPlayer)
            return;

        vector localPos = localPlayer.GetOrigin();

        foreach (IEntity player : players)
        {
            if (player == localPlayer)
                continue;

            if (vector.Distance(localPos, player.GetOrigin()) > 50) // 50m range
                continue;

            // Check if already in group
            string uid = GetPlayerUID(player);
            if (m_GroupComponent && m_GroupComponent.IsMember(uid))
                continue;

            string name = GetPlayerName(player);
            m_PlayerList.AddItem(name);
            m_PlayerNames.Insert(name);
            m_PlayerUIDs.Insert(uid);
        }
    }

    // Get player UID
    string GetPlayerUID(IEntity player)
    {
        if (!player)
            return "";

        return EL_Utils.GetPlayerUID(player);
    }

    // Get player name
    string GetPlayerName(IEntity player)
    {
        return EL_Utils.GetCharacterName(player);
    }

    // Invite button clicked
    void OnInviteButtonClicked(ButtonWidget button)
    {
        int selected = -1;
        if (m_PlayerList)
            selected = m_PlayerList.GetSelectedItem();

        if (selected >= 0 && selected < m_PlayerUIDs.Count())
        {
            string playerName = m_PlayerNames.Get(selected);
            string playerUID = m_PlayerUIDs.Get(selected);
            // TODO: Send invite to selected player using UID
            Print("Inviting: " + playerName + " (" + playerUID + ")");
            Close();
        }
    }

    // Cancel button clicked
    void OnCancelButtonClicked(ButtonWidget button)
    {
        Close();
    }

    override bool OnClick(Widget w, int x, int y, int button)
    {
        if (w == m_InviteButton)
        {
            OnInviteButtonClicked(ButtonWidget.Cast(w));
            return true;
        }

        if (w == m_CancelButton)
        {
            OnCancelButtonClicked(ButtonWidget.Cast(w));
            return true;
        }

        return false;
    }

    // Set group component
    void SetGroupComponent(EL_GroupComponent comp)
    {
        m_GroupComponent = comp;
    }
};