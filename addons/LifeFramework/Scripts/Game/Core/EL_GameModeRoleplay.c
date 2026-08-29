[EntityEditorProps(category: "EveronLife/Core", description: "Core gamemode")]
class EL_GameModeRoleplayClass: SCR_BaseGameModeClass
{
}

class EL_GameModeRoleplay: SCR_BaseGameMode
{
	protected ref EL_CharacterCreationManager m_CharacterCreationManager;
	protected ref EL_ATMManager m_ATMManager;
	protected ref EL_JobManager m_JobManager;

	//------------------------------------------------------------------------------------------------
	// Initialization moved to OnPlayerConnected (lazy init) to avoid constructor overloads
	//------------------------------------------------------------------------------------------------
	void ~EL_GameModeRoleplay()
	{
		EL_PlayerAccountManager.Reset();
	}

	//------------------------------------------------------------------------------------------------
	override void OnPlayerSpawned(int playerId, IEntity controlledEntity)
	{
		super.OnPlayerSpawned(playerId, controlledEntity);

		EL_WhitelistRespawnHandlerComponent whitelist = EL_WhitelistRespawnHandlerComponent.Cast(FindComponent(EL_WhitelistRespawnHandlerComponent));
		if (whitelist)
			whitelist.OnPlayerSpawned(playerId, controlledEntity);
	}

	//------------------------------------------------------------------------------------------------
	override void OnPlayerConnected(int playerId)
	{
		super.OnPlayerConnected(playerId);

		// Lazy initialization of managers (create on first use)
		if (!m_CharacterCreationManager)
			m_CharacterCreationManager = new EL_CharacterCreationManager();
		if (!m_ATMManager)
			m_ATMManager = new EL_ATMManager();
		if (!m_JobManager)
			m_JobManager = new EL_JobManager();

		m_CharacterCreationManager.OnPlayerConnected(playerId);
	}
}
