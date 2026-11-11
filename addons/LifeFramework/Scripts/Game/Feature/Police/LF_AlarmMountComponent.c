//------------------------------------------------------------------------------------------------
//! Where a site alarm sounds when the crime site is near this entity: the bell box on the bank wall above the
//! vault door, the roof of the cash van. LF_Dispatch.StartAlarm stands the alarm (its beacon, light and siren) on
//! the nearest mount within reach of the site; a site with no mount keeps the old placement, the first surface
//! under the site. The alarm used to stand on the ground at the site point, a beacon on a post by the vault door
//! and on the road behind the van (#571 maintainer review).
//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "LifeFramework/Feature/Police", description: "Mounts the site alarm of a nearby crime site here (a bell box, a van roof).")]
class LF_AlarmMountComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class LF_AlarmMountComponent : ScriptComponent
{
	//! The furthest any mount reaches; a larger m_fReach is capped here.
	static const float SEARCH_METRES = 12;

	[Attribute("0 0 0", UIWidgets.Coords, desc: "The foot of the alarm beacon in this entity's local space: the top of the bell box, a point on the roof.")]
	protected vector m_vOffset;

	[Attribute("6", UIWidgets.Auto, desc: "A crime site this close, in metres, sounds its alarm on this mount.")]
	protected float m_fReach;

	protected static vector s_vSite;
	protected static LF_AlarmMountComponent s_pBest;
	protected static float s_fBestDist;

	//------------------------------------------------------------------------------------------------
	//! The alarm's foot in the world.
	vector GetAlarmPosition()
	{
		return GetOwner().CoordToParent(m_vOffset);
	}

	//------------------------------------------------------------------------------------------------
	//! The alarm faces the way the mount does (a wall box faces out of the wall).
	float GetAlarmYaw()
	{
		return GetOwner().GetYawPitchRoll()[0];
	}

	//------------------------------------------------------------------------------------------------
	float GetReach()
	{
		return m_fReach;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The nearest mount whose reach covers the site, or null.
	static LF_AlarmMountComponent FindFor(vector site)
	{
		if (!GetGame() || !GetGame().GetWorld())
			return null;

		s_vSite = site;
		s_pBest = null;
		s_fBestDist = SEARCH_METRES;
		GetGame().GetWorld().QueryEntitiesBySphere(site, SEARCH_METRES, CollectMount, null, EQueryEntitiesFlags.ALL);
		LF_AlarmMountComponent found = s_pBest;
		s_pBest = null;
		return found;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool CollectMount(IEntity entity)
	{
		if (!entity)
			return true;

		LF_AlarmMountComponent mount = LF_AlarmMountComponent.Cast(entity.FindComponent(LF_AlarmMountComponent));
		if (!mount)
			return true;

		float dist = vector.Distance(entity.GetOrigin(), s_vSite);
		if (dist > mount.GetReach() || dist >= s_fBestDist)
			return true;

		s_pBest = mount;
		s_fBestDist = dist;
		return true;
	}
}
