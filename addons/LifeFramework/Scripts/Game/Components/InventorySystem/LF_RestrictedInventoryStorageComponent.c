class LF_RestrictedInventoryStorageComponentClass :SCR_UniversalInventoryStorageComponentClass
{
}

class LF_RestrictedInventoryStorageComponent : SCR_UniversalInventoryStorageComponent
{
    [Attribute("", UIWidgets.ResourceAssignArray, "Choose prefabs", "et", NULL, "Allowed Items Configuration")]
    protected ref array<ResourceName> m_TradablePrefabs;
    
    override bool CanStoreItem(IEntity item, int slotID)
    {   
        return m_TradablePrefabs.Contains(item.GetPrefabData().GetPrefabName());
    }
}
