[BaseContainerProps(configRoot: true)]
class EL_SurvivalHUD : ChimeraMenuBase
{
	protected PlayerController m_PlayerController;
	protected EL_CharacterSurvivalComponent m_SurvivalComponent;

	protected ProgressBarWidget m_wHungerBar;
	protected ProgressBarWidget m_wThirstBar;
	protected ProgressBarWidget m_wHealthBar;

	//------------------------------------------------------------------------------------------------
	override void OnMenuOpen()
	{
		m_wHungerBar = ProgressBarWidget.Cast(GetRootWidget().FindWidget("HungerBar"));
		m_wThirstBar = ProgressBarWidget.Cast(GetRootWidget().FindWidget("ThirstBar"));
		m_wHealthBar = ProgressBarWidget.Cast(GetRootWidget().FindWidget("HealthBar"));
	}

	//------------------------------------------------------------------------------------------------
	void SetPlayerController(PlayerController playerController)
	{
		m_PlayerController = playerController;
		IEntity entity = playerController.GetControlledEntity();
		if (entity)
		{
			m_SurvivalComponent = EL_CharacterSurvivalComponent.Cast(entity.FindComponent(EL_CharacterSurvivalComponent));
		}
	}

	//------------------------------------------------------------------------------------------------
	override void OnMenuUpdate(float tDelta)
	{
		if (m_SurvivalComponent)
		{
			EL_SurvivalStats stats = m_SurvivalComponent.GetSurvivalStats();
			if (stats)
			{
				m_wHungerBar.SetCurrent(stats.GetHunger());
				m_wThirstBar.SetCurrent(stats.GetThirst());
				m_wHealthBar.SetCurrent(stats.GetHealth());
			}
		}
	}
};