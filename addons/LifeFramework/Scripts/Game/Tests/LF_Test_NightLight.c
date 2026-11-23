// red-proof: 2026-11-19, `--red LF_Test_NightLight` printed RED-OK (20 assertions): RedProof() skips Evaluate, so "window glow is lit at 22:00" and "lamp head is lit at 22:00" fail.
//! The night light (LF_NightLightComponent, Feature/Atmosphere): the pure on/off rule at fixed hours,
//! then the real window glow and lamp head prefabs spawned and driven at 22:00 and 12:00.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_NightLight : LF_Test
{
	protected static const ResourceName GLOW = "{E24B79E78F3A5685}Prefabs/Props/Lights/LF_WindowGlow.et";
	protected static const ResourceName LAMP_HEAD = "{E8943F7063E1262D}Prefabs/Props/Lights/LF_LampHeadNight.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "atmosphere/night-light";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		// The rule: 18:00 to 06:00 wraps midnight.
		ctx.True(LF_NightLight.IsOn(18, 18, 6), "18:00 is on (the window opens)");
		ctx.True(LF_NightLight.IsOn(22, 18, 6), "22:00 is on");
		ctx.True(LF_NightLight.IsOn(3, 18, 6), "03:00 is on (after midnight)");
		ctx.False(LF_NightLight.IsOn(12, 18, 6), "12:00 is off");
		ctx.False(LF_NightLight.IsOn(6, 18, 6), "06:00 is off (the window closes)");
		ctx.False(LF_NightLight.IsOn(17.9, 18, 6), "17:54 is still off");
		// A window inside one day.
		ctx.True(LF_NightLight.IsOn(20, 19, 23), "20:00 is inside 19 to 23");
		ctx.False(LF_NightLight.IsOn(23.5, 19, 23), "23:30 is outside 19 to 23");

		float groundY = GetGame().GetWorld().GetSurfaceY(450, 100);
		CheckPrefab(ctx, GLOW, "window glow", Vector(450, groundY + 2, 100), 4);
		CheckPrefab(ctx, LAMP_HEAD, "lamp head", Vector(366, groundY + 5, 100), 16);
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckPrefab(LF_TestContext ctx, ResourceName prefab, string label, vector pos, float maxRadius)
	{
		IEntity entity = LF_Utils.SpawnEntityPrefab(prefab, pos);
		ctx.NotNull(entity, label + " spawned");
		if (!entity)
			return;

		LF_NightLightComponent night = LF_NightLightComponent.Cast(entity.FindComponent(LF_NightLightComponent));
		ctx.NotNull(night, label + " has a night light component");
		ctx.True(entity.FindComponent(RplComponent) == null, label + " does not replicate");
		if (night)
		{
			// The test server is headless and builds no LightEntity child (a render-only entity), so the
			// asserts below are on the component's state; the light itself is checked when a machine
			// that renders has one (the manual step in the PR).
			LightEntity light = night.FindLight();
			if (light)
			{
				ctx.False(light.IsCastShadow(), label + " casts no shadow");
				ctx.True(light.GetRadius() <= maxRadius, label + " stays inside its radius budget");
				light.SetEnabled(false);
			}

			if (!RedProof())
				night.Evaluate(22);

			ctx.True(night.IsLit(), label + " is lit at 22:00");
			if (light)
				ctx.True(light.IsEnabled(), label + " light is enabled at 22:00");

			if (!RedProof())
				night.Evaluate(12);

			ctx.False(night.IsLit(), label + " is dark at 12:00");
			if (light)
				ctx.False(light.IsEnabled(), label + " light is disabled at 12:00");
		}

		SCR_EntityHelper.DeleteEntityAndChildren(entity);
	}
}
