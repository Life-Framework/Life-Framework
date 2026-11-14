// The vanilla entity catalog manager, minus its demand for a vanilla faction manager.
//
// Base-game systems (the resupply stations vanilla Everon places, arsenals) look up
// SCR_EntityCatalogManagerComponent on the game mode and log an error when it is missing.
// The vanilla component's Init() also raises a VME when the world has no SCR_FactionManager,
// and Life Framework has none on purpose: factions are LF_Factions, not the vanilla respawn
// faction system (docs/debug-log-triage.md). This subclass runs the same Init without that
// check. The faction-specific catalog getters already return null with no faction manager.
class LF_EntityCatalogManagerComponentClass : SCR_EntityCatalogManagerComponentClass
{
}

class LF_EntityCatalogManagerComponent : SCR_EntityCatalogManagerComponent
{
	//------------------------------------------------------------------------------------------------
	protected override void Init()
	{
		m_bInitDone = true;

		// Same as the base Init, without the SCR_FactionManager lookup and its Debug.Error2.
		InitCatalogs(m_aEntityCatalogs, m_mEntityCatalogs);
		m_aEntityCatalogs = null;

		if (s_OnEntityCatalogInitialized)
			s_OnEntityCatalogInitialized.Invoke();
	}
}
