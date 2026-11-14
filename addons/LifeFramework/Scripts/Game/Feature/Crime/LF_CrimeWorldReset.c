//------------------------------------------------------------------------------------------------
//! Crime part of the world reset (Core/LF_WorldReset.c): the rob cooldowns, the vaults and the
//! loot a robbery left lying about.

modded class LF_WorldResetRegistry
{
	override void Collect(notnull array<ref LF_WorldResetStep> steps)
	{
		super.Collect(steps);
		steps.Insert(new LF_WorldReset_Crime());
		steps.Insert(new LF_WorldReset_Loot());
	}
}

//------------------------------------------------------------------------------------------------
//! Wanted levels, till and vault cooldowns and the search cooldowns, all restocked.
class LF_WorldReset_Crime : LF_WorldResetStep
{
	override string GetName()
	{
		return "crime";
	}

	override bool IsCrimeState()
	{
		return true;
	}

	override int Run()
	{
		int count = 0;
		LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();
		if (accounts)
			count += accounts.ClearAllWanted();

		count += LF_RobAction.ResetCooldowns();
		count += LF_Hospital.ResetTillCooldowns();
		count += LF_Bank.RestockAll();
		count += LF_ChainSearch.ClearSearchCooldowns();
		return count;
	}
}

//------------------------------------------------------------------------------------------------
//! Money bags, drills, keycards and charges lying loose in the world. Carried ones stay.
class LF_WorldReset_Loot : LF_WorldResetStep
{
	protected ref array<EntityID> m_aPlaced = {};
	protected bool m_bStarted;

	protected array<ResourceName> Prefabs()
	{
		array<ResourceName> prefabs = {LF_Bank.BagPrefab(), LF_Bank.DRILL_PREFAB, LF_Bank.KEYCARD_PREFAB, LF_Breach.FENCE_CHARGE_PREFAB, LF_Breach.CHARGE_PREFAB};
		return prefabs;
	}

	override string GetName()
	{
		return "loot";
	}

	//! The bags and charges the town was built with stay; only what play spilled is removed.
	override void OnStart()
	{
		LF_WorldReset.RememberPlaced(Prefabs(), m_aPlaced);
		m_bStarted = true;
	}

	override bool IsCrimeState()
	{
		return true;
	}

	override int Run()
	{
		// Without the start snapshot the placed ones cannot be told from spilled ones: remove nothing.
		if (!m_bStarted)
			return 0;

		return LF_WorldReset.DeleteLoose(Prefabs(), m_aPlaced);
	}
}
