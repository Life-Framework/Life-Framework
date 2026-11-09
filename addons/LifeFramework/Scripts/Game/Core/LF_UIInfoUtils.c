class LF_UIInfoUtils
{
	protected static ref map<ResourceName, ref UIInfo> s_mCache = new map<ResourceName, ref UIInfo>();

	//------------------------------------------------------------------------------------------------
	static UIInfo GetInfo(ResourceName prefab)
	{
		UIInfo resultInfo = s_mCache.Get(prefab);

		if (!resultInfo)
		{
			// Hold the resource: a temporary is released before the source is read
			// ("Usage of released BaseResourceObject" on every shop catalog build).
			Resource resource = Resource.Load(prefab);
			IEntitySource entitySource = SCR_BaseContainerTools.FindEntitySource(resource);
			if (entitySource)
			{
			    for(int nComponent, componentCount = entitySource.GetComponentCount(); nComponent < componentCount; nComponent++)
			    {
			        IEntityComponentSource componentSource = entitySource.GetComponent(nComponent);
			        if(componentSource.GetClassName().ToType().IsInherited(InventoryItemComponent))
			        {
			            BaseContainer attributesContainer = componentSource.GetObject("Attributes");
			            if (attributesContainer)
			            {
			                BaseContainer itemDisplayNameContainer = attributesContainer.GetObject("ItemDisplayName");
			                if (itemDisplayNameContainer)
			                {
			                    resultInfo = UIInfo.Cast(BaseContainerTools.CreateInstanceFromContainer(itemDisplayNameContainer));
			                    break;
			                }
			            }
			        }
			    }
			}
			
			s_mCache.Set(prefab, resultInfo);
		}

		return resultInfo;
	}

	//------------------------------------------------------------------------------------------------
	static UIInfo GetInfo(IEntity entity)
	{
		if (!entity) return null;

		InventoryItemComponent item = LF_Component<InventoryItemComponent>.Find(entity);
		if (!item) return null;

		UIInfo resultInfo = item.GetAttributes().GetUIInfo();
		string prefab = LF_Utils.GetPrefabName(entity);
		if (prefab) s_mCache.Set(prefab, resultInfo);

		return resultInfo;
	}
}
