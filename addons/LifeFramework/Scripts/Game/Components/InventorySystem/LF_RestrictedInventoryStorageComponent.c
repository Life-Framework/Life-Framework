class LF_RestrictedInventoryStorageComponentClass :SCR_UniversalInventoryStorageComponentClass
{
}

class LF_RestrictedInventoryStorageComponent : SCR_UniversalInventoryStorageComponent
{
    [Attribute("", UIWidgets.ResourceAssignArray, "Choose prefabs", "et", NULL, "Allowed Items Configuration")]
    protected ref array<ResourceName> m_TradablePrefabs;
    
    override bool CanStoreItem(IEntity item, int slotID)
    {   
        if (!m_TradablePrefabs || !item || !item.GetPrefabData())
            return false;

        ResourceName prefab = item.GetPrefabData().GetPrefabName();
        foreach (ResourceName tradable : m_TradablePrefabs)
        {
            if (LF_Utils.SamePrefab(tradable, prefab))
                return true;
        }

        return false;
    }
}
