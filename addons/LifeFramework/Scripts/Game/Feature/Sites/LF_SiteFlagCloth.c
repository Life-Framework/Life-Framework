//------------------------------------------------------------------------------------------------
//! LF_SiteFlagCloth.c: the cloth on a contested site's pole (#4451, gangs C12). The pole prefab
//! (LF_SiteFlag.et) hangs a plain white cloth; while a gang holds the site, each rendering machine
//! hides it and stands the gang's flag (`LF_OrgFlags`) in its place. The flag comes from the prefab
//! slot gang_flag_NN through `LF_OrgFlagWall.SamplePrefab` (core names no pack file). A sample is a
//! whole pole with its cloth; it is spawned locally on the site pole, its own pole hidden and
//! without physics, so only its cloth shows. A pack-less boot, or a flag with no slot row, keeps the
//! plain cloth and logs why. Local decoration only: the flag number is the replicated state
//! (LF_SiteFlagComponent.m_iFlag); a dedicated server never builds a cloth.
class LF_SiteFlagCloth
{
	static const string FEATURE = "Sites";

	protected IEntity m_Sample;
	protected int m_iShown = -1;

	//------------------------------------------------------------------------------------------------
	//! The flag prefab a pole flies for a flag number; empty for NONE (the plain cloth) or a flag with
	//! no usable slot row.
	static ResourceName PrefabFor(int flag)
	{
		if (!LF_OrgFlags.IsValid(flag))
			return ResourceName.Empty;

		return LF_OrgFlagWall.SamplePrefab(flag);
	}

	//------------------------------------------------------------------------------------------------
	//! True for a cloth: an entity with a flag animation.
	static bool IsCloth(IEntity entity)
	{
		return entity && entity.FindComponent(FlagAnimationComponent);
	}

	//------------------------------------------------------------------------------------------------
	//! The pole's own plain cloth child, or null.
	static IEntity PlainCloth(IEntity pole)
	{
		if (!pole)
			return null;

		IEntity child = pole.GetChildren();
		while (child)
		{
			if (IsCloth(child))
				return child;

			child = child.GetSibling();
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! The pole's painted plate: its first child that is not a cloth (or a spawned sample).
	static IEntity Plate(IEntity pole)
	{
		if (!pole)
			return null;

		IEntity child = pole.GetChildren();
		while (child)
		{
			if (!IsCloth(child) && !IsCloth(child.GetChildren()))
				return child;

			child = child.GetSibling();
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! The flag this machine shows, -1 before the first Show.
	int GetShown()
	{
		return m_iShown;
	}

	//------------------------------------------------------------------------------------------------
	//! Show `flag` on `pole`: the gang's cloth for a flag, the plain one for NONE. A repeat does nothing.
	void Show(IEntity pole, int flag)
	{
		if (!pole || pole.IsDeleted() || flag == m_iShown)
			return;

		Clear();
		m_iShown = flag;
		IEntity plain = PlainCloth(pole);
		ResourceName prefab = PrefabFor(flag);
		if (prefab.IsEmpty())
		{
			if (flag != LF_OrgFlags.NONE)
				LF_Debug.Warn(FEATURE, string.Format("site pole flag %1 has no usable gang_flag slot; it flies the plain cloth", flag));

			ShowPlain(plain, true);
			return;
		}

		Resource resource = Resource.Load(prefab);
		if (!resource || !resource.IsValid())
		{
			LF_Debug.Error(FEATURE, string.Format("site pole flag %1 prefab %2 failed to load; it flies the plain cloth", flag, prefab));
			ShowPlain(plain, true);
			return;
		}

		EntitySpawnParams params();
		params.TransformMode = ETransformMode.LOCAL;
		params.Parent = pole;
		m_Sample = GetGame().SpawnEntityPrefabLocal(resource, pole.GetWorld(), params);
		if (!m_Sample)
		{
			LF_Debug.Error(FEATURE, string.Format("site pole flag %1 failed to spawn; it flies the plain cloth", flag));
			ShowPlain(plain, true);
			return;
		}

		// The site pole already stands here: only the sample's cloth child shows.
		m_Sample.ClearFlags(EntityFlags.VISIBLE, false);
		Physics physics = m_Sample.GetPhysics();
		if (physics)
			physics.Destroy();

		ShowPlain(plain, false);
	}

	//------------------------------------------------------------------------------------------------
	protected void ShowPlain(IEntity plain, bool visible)
	{
		if (!plain)
			return;

		if (visible)
			plain.SetFlags(EntityFlags.VISIBLE, false);
		else
			plain.ClearFlags(EntityFlags.VISIBLE, false);
	}

	//------------------------------------------------------------------------------------------------
	//! Remove the spawned gang cloth (the pole is going, or the flag changes).
	void Clear()
	{
		if (m_Sample && !m_Sample.IsDeleted())
			SCR_EntityHelper.DeleteEntityAndChildren(m_Sample);

		m_Sample = null;
		m_iShown = -1;
	}
}
