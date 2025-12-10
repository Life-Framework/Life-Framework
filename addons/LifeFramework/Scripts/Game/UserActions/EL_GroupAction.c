class EL_GroupAction : ScriptedUserAction
{
    // Override to check if can be performed
    override bool CanBePerformedScript(IEntity user)
    {
        // TODO: Check if player can perform group actions
        return true;
    }

    // Override to perform action
    override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
    {
        // Open group menu or create group
        EL_GroupMenu menu = EL_GroupMenu.Cast(GetGame().GetMenuManager().OpenMenu(ChimeraMenuPreset.LF_GroupMenu));
        if (menu)
        {
            // TODO: Set group component if exists
        }
    }

    // Override to get action name
    override bool GetActionNameScript(out string outName)
    {
        outName = "Manage Group";
        return true;
    }
};