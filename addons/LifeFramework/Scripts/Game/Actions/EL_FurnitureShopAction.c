class EL_FurnitureShopAction : ScriptedUserAction
{
    // Reference to the shop component
    protected EL_FurnitureShopComponent m_ShopComponent;

    // Override to check if action is available
    override bool CanBePerformedScript(IEntity user)
    {
        return m_ShopComponent != null;
    }

    // Override to get action name
    override bool GetActionNameScript(out string outName)
    {
        outName = "#EL-FurnitureShop_Open";
        return true;
    }

    // Override to perform action
    override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
    {
        if (!m_ShopComponent)
            return;

        // Open furniture shop menu
        EL_FurnitureShopMenu menu = EL_FurnitureShopMenu.Cast(GetGame().GetMenuManager().OpenMenu(ChimeraMenuPreset.FurnitureShopMenu));
        if (menu)
        {
            menu.SetShopComponent(m_ShopComponent);
        }
    }

    // Set shop component
    void SetShopComponent(EL_FurnitureShopComponent comp)
    {
        m_ShopComponent = comp;
    }
}