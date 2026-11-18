//------------------------------------------------------------------------------------------------
//! Deletes the loose (unparented) entities of one prefab within a radius: what a test leaves lying that
//! no character owns (#2792). Narrow on purpose; a suite-wide sweep of every fixture character crashed
//! the engine, so each test ends what it spawned and this only reaches a prefab it names.
class LF_TestLooseSweep
{
	protected ref array<IEntity> m_aFound = {};
	protected ResourceName m_Prefab;
	protected bool m_bUnnamed;

	//------------------------------------------------------------------------------------------------
	//! \return How many entities were deleted.
	int Delete(vector at, float radius, ResourceName prefab)
	{
		m_aFound.Clear();
		m_Prefab = prefab;
		GetGame().GetWorld().QueryEntitiesBySphere(at, radius, Visit);
		int count = m_aFound.Count();
		foreach (IEntity entity : m_aFound)
			SCR_EntityHelper.DeleteEntityAndChildren(entity);

		m_aFound.Clear();
		return count;
	}

	//------------------------------------------------------------------------------------------------
	//! Deletes a spawned destructible building (the payphone booth) together with the unnamed part it leaves
	//! behind: spawning one creates a loose GenericEntity about 1.1 m above its origin (0.46 m tall, no prefab) that
	//! deleting the building does not remove, so every test that spawned a payphone left a prop hanging there
	//! (#2792).
	static void DeleteBuilding(IEntity building)
	{
		if (!building)
			return;

		LF_TestLooseSweep sweep = new LF_TestLooseSweep();
		sweep.m_bUnnamed = true;
		sweep.Delete(building.GetOrigin() + "0 1.1 0", 1.5, "");
		SCR_EntityHelper.DeleteEntityAndChildren(building);
	}

	//------------------------------------------------------------------------------------------------
	protected bool Visit(IEntity entity)
	{
		if (entity.GetParent())
			return true;

		if (m_bUnnamed)
		{
			if (entity.ClassName() == "GenericEntity" && LF_TestDebrisProbe.NameOf(entity).IsEmpty())
				m_aFound.Insert(entity);

			return true;
		}

		if (LF_Utils.GetPrefabName(entity) == m_Prefab)
			m_aFound.Insert(entity);

		return true;
	}
}
