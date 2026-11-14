class Bad
{
	bool A(IEntity e, ResourceName saved)
	{
		return LF_Utils.GetPrefabName(e) == saved;
	}

	bool B(ResourceName savedPrefab, ResourceName livePrefab)
	{
		if (savedPrefab != livePrefab)
			return false;
		return true;
	}

	bool C(IEntity e)
	{
		return LF_Utils.GetPrefabName(e).Contains("Prefabs/Items/");
	}
}
