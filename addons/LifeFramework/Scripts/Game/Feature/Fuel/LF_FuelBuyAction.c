//------------------------------------------------------------------------------------------------
//! "Fill the tank" on a pump that sells from its station's stock (#3328). The server does the whole sale in
//! `LF_FuelStationComponent.SellNearby`: it finds the vehicle at the pump, works out the litres, the price and
//! the cash, and tells the player how it went. The action carries no litres and no price.
class LF_FuelBuyAction : ScriptedUserAction
{
	protected LF_FuelStationComponent m_Station;

	//------------------------------------------------------------------------------------------------
	override void Init(IEntity pOwnerEntity, GenericComponent pManagerComponent)
	{
		m_Station = LF_FuelStationComponent.Cast(pOwnerEntity.FindComponent(LF_FuelStationComponent));
	}

	//------------------------------------------------------------------------------------------------
	//! Shown only while the station sells from stock; with fuel stock off the pump is a vanilla pump.
	override bool CanBeShownScript(IEntity user)
	{
		return m_Station != null && m_Station.SellsFromStock();
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		return m_Station != null && m_Station.SellsFromStock();
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "#LF-Fuel_Buy";
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;

		if (!m_Station)
		{
			LF_Debug.Error(LF_FuelStationComponent.FEATURE, "fuel buy action on an entity with no LF_FuelStationComponent");
			return;
		}

		m_Station.SellNearby(pUserEntity);
	}
}
