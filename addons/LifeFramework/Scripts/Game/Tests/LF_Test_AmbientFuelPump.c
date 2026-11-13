// tier: WORLD
// red-proof: 2026-11-13, LF_AmbientFuelPump.PULSE_S 0: `--only LF_Test_AmbientFuelPump` failed "the hum is on while fuelling". Restored, green.
//! The fuel station's pump hum (#897): a fuelling step on the server pulses the hum prop standing
//! at the pumps and turns it on; a pump with no hum prop in reach is left alone (a fuel truck, a
//! station elsewhere).
class LF_Test_AmbientFuelPump : LF_Test
{
	protected static const ResourceName HUM = "{D88D0B05DF83772F}Prefabs/Atmosphere/LF_FuelPumpHum.et";
	protected static const ResourceName NOZZLE = "{A8568D2A6C46B864}Prefabs/Atmosphere/LF_AmbientEmitter.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "atmosphere/fuelpump";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		float groundY = GetGame().GetWorld().GetSurfaceY(340, 100);
		IEntity hum = LF_Utils.SpawnEntityPrefab(HUM, Vector(340, groundY, 100));
		IEntity near = LF_Utils.SpawnEntityPrefab(NOZZLE, Vector(344, groundY, 100));
		IEntity far = LF_Utils.SpawnEntityPrefab(NOZZLE, Vector(380, groundY, 100));
		ctx.NotNull(hum, "the hum prop spawned");
		ctx.NotNull(near, "a pump spawned near it");
		ctx.NotNull(far, "a pump spawned far from it");
		if (!hum || !near || !far)
			return;

		LF_AmbientSwitchComponent humSwitch = LF_AmbientSwitchComponent.Cast(hum.FindComponent(LF_AmbientSwitchComponent));
		ctx.NotNull(humSwitch, "the hum prop has a switch");
		ctx.NotNull(hum.FindComponent(LF_AmbientEmitterComponent), "the hum prop has an emitter");
		if (humSwitch)
		{
			ctx.False(humSwitch.IsOn(), "the pumps are quiet until something is fuelled");
			ctx.False(LF_AmbientFuelPump.OnFuelled(far), "a pump out of reach of the hum prop pulses nothing");
			ctx.False(humSwitch.IsOn(), "and the hum stays off");
			ctx.True(LF_AmbientFuelPump.OnFuelled(near), "a fuelling step at the pump pulses the hum");
			ctx.True(humSwitch.IsOn(), "the hum is on while fuelling");
			humSwitch.SetOn(false);
		}

		SCR_EntityHelper.DeleteEntityAndChildren(hum);
		SCR_EntityHelper.DeleteEntityAndChildren(near);
		SCR_EntityHelper.DeleteEntityAndChildren(far);
	}
}
