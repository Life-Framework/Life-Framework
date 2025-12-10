class EL_GarageAction : ScriptedUserAction
{
    // Reference to the garage component
    protected EL_GarageComponent m_GarageComponent;

    // Override to check if action is available
    override bool CanBePerformedScript(IEntity user)
    {
        if (!m_GarageComponent)
            return false;

        // Get player UID
        string playerUID = GetPlayerUID(user);
        if (playerUID == "")
            return false;

        // Can access if owner
        return m_GarageComponent.IsOwner(playerUID);
    }

    // Override to get action name
    override bool GetActionNameScript(out string outName)
    {
        if (!m_GarageComponent)
            return false;

        outName = "#EL-Garage_Open";
        return true;
    }

    // Override to perform action
    override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
    {
        if (!m_GarageComponent)
            return;

        string playerUID = GetPlayerUID(pUserEntity);
        if (m_GarageComponent.IsOwner(playerUID))
        {
            // Open garage menu
            OpenGarageMenu(pOwnerEntity);
        }
    }

    // Helper to get player UID
    protected string GetPlayerUID(IEntity user)
    {
        if (!user)
            return "";

        // Use shared utility to resolve persistent character UID
        return EL_Utils.GetPlayerUID(user);
    }

    // Open garage menu
    protected void OpenGarageMenu(IEntity user)
    {
        EL_GarageMenu menu = EL_GarageMenu.Cast(GetGame().GetMenuManager().OpenMenu(ChimeraMenuPreset.EL_Garage));
        if (menu)
        {
            menu.SetGarageComponent(m_GarageComponent);
        }
    }

    // Set garage component
    void SetGarageComponent(EL_GarageComponent comp)
    {
        m_GarageComponent = comp;
    }
};