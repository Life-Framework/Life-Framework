[EntityEditorProps(category: "EveronLife/Core", description: "Core gamemode")]
class EL_GameModeRoleplayClass: SCR_BaseGameModeClass
{
}

class EL_GameModeRoleplay: SCR_BaseGameMode
{
	protected ref EL_CharacterCreationManager m_CharacterCreationManager;
	protected ref EL_ATMManager m_ATMManager;
	protected ref EL_JobManager m_JobManager;
	protected ref EL_PropertyManager m_PropertyManager;
	protected ref EL_GroupManager m_GroupManager;
	protected ref EL_GarageManager m_GarageManager;

	//------------------------------------------------------------------------------------------------
	// Initialization moved to OnPlayerConnected (lazy init) to avoid constructor overloads
	//------------------------------------------------------------------------------------------------
	void ~EL_GameModeRoleplay()
	{
		EL_PlayerAccountManager.Reset();
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
		if (!m_PropertyManager)
			m_PropertyManager = new EL_PropertyManager();
		if (!m_GroupManager)
			m_GroupManager = new EL_GroupManager();
		if (!m_GarageManager)
			m_GarageManager = new EL_GarageManager();

		m_CharacterCreationManager.OnPlayerConnected(playerId);
	}
}
