//------------------------------------------------------------------------------------------------
class LF_FuelDepotComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! The fuel depot's load point (#3330, framework #788): a pump or a filling stand beside which a tanker
//! parks. Put it on a prop beside `LF_FuelDepotLoadAction`. While the fuel config is on, the action sells the
//! hauler a tanker load at the wholesale price (`LF_FuelHaul.Buy`). With the config off (the core default)
//! the prop does nothing. The depot never runs dry: its price is the limit, not its stock.
class LF_FuelDepotComponent : ScriptComponent
{
	//------------------------------------------------------------------------------------------------
	//! True when the depot loads tankers: the fuel config is loaded and on.
	bool LoadsTankers()
	{
		LF_FuelConfig config = LF_FuelConfig.Get();
		return config && config.m_bEnabled;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: loads the tanker standing beside this depot for `hauler` and tells them how it went.
	LF_FuelHaulResult LoadNearby(IEntity hauler)
	{
		if (!Replication.IsServer())
			return null;

		LF_FuelHaulResult result = LF_FuelHaul.Buy(LF_FuelHaul.FindTankerNear(GetOwner()), hauler);
		LF_FuelHaul.Tell(hauler, result, "#LF-FuelHaul_Loaded");
		return result;
	}
}

//------------------------------------------------------------------------------------------------
//! "Load the tanker" at the depot. The server does the whole sale; the action carries no litres or price.
class LF_FuelDepotLoadAction : ScriptedUserAction
{
	protected LF_FuelDepotComponent m_Depot;

	//------------------------------------------------------------------------------------------------
	override void Init(IEntity pOwnerEntity, GenericComponent pManagerComponent)
	{
		m_Depot = LF_FuelDepotComponent.Cast(pOwnerEntity.FindComponent(LF_FuelDepotComponent));
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		return m_Depot != null && m_Depot.LoadsTankers();
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		return m_Depot != null && m_Depot.LoadsTankers();
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "#LF-FuelHaul_Load";
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;

		if (!m_Depot)
		{
			LF_Debug.Error(LF_FuelHaul.FEATURE, "fuel load action on an entity with no LF_FuelDepotComponent");
			return;
		}

		m_Depot.LoadNearby(pUserEntity);
	}
}
