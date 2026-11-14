//------------------------------------------------------------------------------------------------
//! Investigation part of the world reset (Core/LF_WorldReset.c): open scenes, casings and
//! markers, loose evidence bags and the evidence locker log.

modded class LF_WorldResetRegistry
{
	override void Collect(notnull array<ref LF_WorldResetStep> steps)
	{
		super.Collect(steps);
		steps.Insert(new LF_WorldReset_Evidence());
	}
}

//------------------------------------------------------------------------------------------------
class LF_WorldReset_Evidence : LF_WorldResetStep
{
	protected ref array<EntityID> m_aPlaced = {};
	protected bool m_bStarted;

	protected array<ResourceName> Prefabs()
	{
		array<ResourceName> prefabs = {LF_Evidence.BAG_PREFAB, LF_Evidence.CASING_PREFAB, LF_Evidence.MarkerPrefab()};
		return prefabs;
	}

	override void OnStart()
	{
		LF_WorldReset.RememberPlaced(Prefabs(), m_aPlaced);
		m_bStarted = true;
	}

	override string GetName()
	{
		return "evidence";
	}

	override bool IsCrimeState()
	{
		return true;
	}

	override int Run()
	{
		int count = LF_Evidence.ClearAll() + LF_EvidenceLocker.ClearAll();
		if (m_bStarted)
			count += LF_WorldReset.DeleteLoose(Prefabs(), m_aPlaced);

		return count;
	}
}
