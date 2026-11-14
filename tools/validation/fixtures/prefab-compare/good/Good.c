class Good
{
	bool A(IEntity e, ResourceName saved)
	{
		return LF_Utils.SamePrefab(LF_Utils.GetPrefabName(e), saved);
	}

	bool B(ResourceName prefab)
	{
		if (prefab == ResourceName.Empty || !prefab)
			return false;
		return true;
	}

	bool C(IEntity a, IEntity b)
	{
		// lf-path-ok: two live entities
		return LF_Utils.GetPrefabName(a) == LF_Utils.GetPrefabName(b);
	}
}
