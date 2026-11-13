//------------------------------------------------------------------------------------------------
//! The fuel station's pump hum. Vanilla fuels a vehicle through SCR_FuelSupportStationComponent
//! (the pump nozzle prefabs); every fuelling step runs OnExecutedServer on the server. Each step
//! pulses the nearest LF_AmbientSwitchComponent in reach, which is an LF_AmbientEmitterComponent
//! prop placed at the pumps (Prefabs/Atmosphere/LF_FuelPumpHum.et); the pulse runs out a few
//! seconds after the last step, so the hum stops when the fuelling does.
class LF_AmbientFuelPump
{
	//! How far from the nozzle the hum prop may stand, in metres. The canopy is 10 m across.
	static const float REACH_M = 8; // lf-fixed: the canopy's extent, not a gameplay knob
	//! Seconds the hum lasts after a fuelling step. A step comes about once a second.
	static const float PULSE_S = 4; // lf-fixed: covers the gap between two fuelling steps

	protected static IEntity s_pFound;
	protected static vector s_vOrigin;
	protected static float s_fBest;

	//------------------------------------------------------------------------------------------------
	//! Server: a pump just fuelled something. Returns true when a hum prop took the pulse.
	static bool OnFuelled(IEntity pump)
	{
		if (!pump || !GetGame() || !GetGame().GetWorld())
			return false;

		s_pFound = null;
		s_vOrigin = pump.GetOrigin();
		s_fBest = REACH_M;
		GetGame().GetWorld().QueryEntitiesBySphere(s_vOrigin, REACH_M, Collect, null, EQueryEntitiesFlags.ALL);
		IEntity hum = s_pFound;
		s_pFound = null;
		// Most pumps in the world (a fuel truck, a station elsewhere) have no hum prop; that is not an error.
		if (!hum)
			return false;

		LF_AmbientSwitchComponent.PulseOn(hum, PULSE_S);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool Collect(IEntity entity)
	{
		if (!entity || !entity.FindComponent(LF_AmbientSwitchComponent))
			return true;

		float dist = vector.Distance(entity.GetOrigin(), s_vOrigin);
		if (dist < s_fBest)
		{
			s_fBest = dist;
			s_pFound = entity;
		}
		return true;
	}
}

//------------------------------------------------------------------------------------------------
modded class SCR_FuelSupportStationComponent
{
	//------------------------------------------------------------------------------------------------
	override void OnExecutedServer(notnull IEntity actionOwner, notnull IEntity actionUser, notnull SCR_BaseUseSupportStationAction action)
	{
		super.OnExecutedServer(actionOwner, actionUser, action);
		LF_AmbientFuelPump.OnFuelled(actionOwner);
	}
}
