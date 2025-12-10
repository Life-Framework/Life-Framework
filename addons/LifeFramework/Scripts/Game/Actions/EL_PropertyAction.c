class EL_PropertyAction : ScriptedUserAction
{
    // Reference to the property component
    protected EL_PropertyComponent m_PropertyComponent;

    // Override to check if action is available
    override bool CanBePerformedScript(IEntity user)
    {
        if (!m_PropertyComponent)
            return false;

        // Get player UID (assuming we have a way to get it)
        string playerUID = GetPlayerUID(user);
        if (playerUID == "")
            return false;

        // If not owned, can purchase
        if (m_PropertyComponent.GetOwnerUID() == "")
            return true;

        // If owned by player, can interact
        return m_PropertyComponent.IsOwner(playerUID);
    }

    // Override to get action name
    override bool GetActionNameScript(out string outName)
    {
        if (!m_PropertyComponent)
            return false;

        string playerUID = EL_Utils.GetPlayerUID(GetGame().GetPlayerController().GetControlledEntity());
        if (m_PropertyComponent.GetOwnerUID() == "")
        {
            outName = "#EL-Property_Buy";
            return true;
        }
        else if (m_PropertyComponent.IsOwner(playerUID))
        {
            outName = "#EL-Property_Manage";
            return true;
        }

        return false;
    }

    // Override to perform action
    override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
    {
        if (!m_PropertyComponent)
            return;

        string playerUID = GetPlayerUID(pOwnerEntity);
        if (m_PropertyComponent.GetOwnerUID() == "")
        {
            // Purchase
            EL_CharacterATMComponent atmComp = GetATMComponent(pOwnerEntity);
            if (atmComp && atmComp.GetBankAccount())
            {
                if (atmComp.GetBankAccount().Withdraw(m_PropertyComponent.GetPurchasePrice()))
                {
                    m_PropertyComponent.Purchase(playerUID);
                    Print("Property purchased successfully!");
                }
                else
                {
                    Print("Not enough money to purchase property!");
                }
            }
        }
        else if (m_PropertyComponent.IsOwner(playerUID))
        {
            // Open property menu
            OpenPropertyMenu(pOwnerEntity);
        }
    }

    // Helper to get player UID
    protected string GetPlayerUID(IEntity user)
    {
        if (!user)
            return "";
        return EL_Utils.GetPlayerUID(user);
    }

    // Helper to get ATM component
    protected EL_CharacterATMComponent GetATMComponent(IEntity user)
    {
        return EL_CharacterATMComponent.Cast(user.FindComponent(EL_CharacterATMComponent));
    }

    // Open property menu
    protected void OpenPropertyMenu(IEntity user)
    {
        EL_PropertyMenu menu = EL_PropertyMenu.Cast(GetGame().GetMenuManager().OpenMenu(ChimeraMenuPreset.PropertyMenu));
        if (menu)
        {
            menu.SetPropertyComponent(m_PropertyComponent);
        }
    }

    // Set property component
    void SetPropertyComponent(EL_PropertyComponent comp)
    {
        m_PropertyComponent = comp;
    }
}