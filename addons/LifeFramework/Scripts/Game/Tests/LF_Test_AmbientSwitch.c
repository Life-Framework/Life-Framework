// tier: WORLD
// red-proof: 2026-11-13, Sawmill.et with m_bStartsOn 1: `--only LF_Test_AmbientSwitch` failed "a sawmill is quiet until a log is cut". Restored, green.
//! The replicated on/off behind an ambient sound (LF_AmbientSwitchComponent, Feature/Atmosphere) on
//! the server: the store radio starts on and a player's switch flips it; the sawmill starts off and
//! a pulse (what LF_ProcessAction sends after a log is cut) turns it on. The emitter beside it must
//! name a sound project, or the prefab is silent.
class LF_Test_AmbientSwitch : LF_Test
{
	protected static const ResourceName RADIO = "{6E621E4B599F06FD}Prefabs/Atmosphere/LF_Radio_Store.et";
	protected static const ResourceName SAWMILL = "{61418A72BA63D059}Prefabs/Processor/Sawmill.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "atmosphere/switch";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		float groundY = GetGame().GetWorld().GetSurfaceY(320, 100);
		IEntity radio = LF_Utils.SpawnEntityPrefab(RADIO, Vector(320, groundY, 100));
		ctx.NotNull(radio, "the radio spawned");
		if (!radio)
			return;

		LF_AmbientSwitchComponent radioSwitch = LF_AmbientSwitchComponent.Cast(radio.FindComponent(LF_AmbientSwitchComponent));
		ctx.NotNull(radioSwitch, "the radio has a switch");
		ctx.NotNull(radio.FindComponent(LF_AmbientEmitterComponent), "the radio has an emitter");
		ctx.NotNull(radio.FindComponent(RplComponent), "the radio replicates");
		if (radioSwitch)
		{
			ctx.True(radioSwitch.IsOn(), "a radio plays when the world starts");
			ctx.True(radioSwitch.SetOn(false), "switching it off changes it");
			ctx.False(radioSwitch.IsOn(), "it is off");
			ctx.False(radioSwitch.SetOn(false), "switching off twice changes nothing");
			ctx.True(radioSwitch.SetOn(true), "switching it on changes it back");
		}

		IEntity sawmill = LF_Utils.SpawnEntityPrefab(SAWMILL, Vector(326, groundY, 100));
		ctx.NotNull(sawmill, "the sawmill spawned");
		if (sawmill)
		{
			LF_AmbientSwitchComponent saw = LF_AmbientSwitchComponent.Cast(sawmill.FindComponent(LF_AmbientSwitchComponent));
			ctx.NotNull(saw, "the sawmill has a switch");
			if (saw)
			{
				ctx.False(saw.IsOn(), "a sawmill is quiet until a log is cut");
				LF_AmbientSwitchComponent.PulseOn(sawmill, 0);
				ctx.False(saw.IsOn(), "a pulse of no seconds does nothing");
				LF_AmbientSwitchComponent.PulseOn(sawmill, 30);
				ctx.True(saw.IsOn(), "a pulse turns it on");
				saw.SetOn(false);
			}

			SCR_EntityHelper.DeleteEntityAndChildren(sawmill);
		}

		// An entity with no switch is not an error for the caller.
		LF_AmbientSwitchComponent.PulseOn(null, 5);
		SCR_EntityHelper.DeleteEntityAndChildren(radio);
	}
}
