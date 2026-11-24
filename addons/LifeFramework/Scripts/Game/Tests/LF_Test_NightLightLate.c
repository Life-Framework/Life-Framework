// red-proof: 2026-11-24, `--red LF_Test_NightLightLate` printed RED-OK (17 assertions): RedProof() skips the second look, so "the late light is switched on" fails.
//! A night light whose child LightEntity appears after the first look (#4831): the first look misses and
//! logs no error, the next look finds the light and switches it on. The same prefab with a child that
//! never comes builds its own light after RETRY_MAX looks and switches it on.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_NightLightLate : LF_Test
{
	protected static const ResourceName GLOW = "{E24B79E78F3A5685}Prefabs/Props/Lights/LF_WindowGlow.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "atmosphere/night-light-late";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		float groundY = GetGame().GetWorld().GetSurfaceY(450, 100);
		IEntity entity = LF_Utils.SpawnEntityPrefab(GLOW, Vector(450, groundY + 2, 100));
		ctx.NotNull(entity, "window glow spawned");
		if (!entity)
			return;

		LF_NightLightComponent night = LF_NightLightComponent.Cast(entity.FindComponent(LF_NightLightComponent));
		ctx.NotNull(night, "window glow has a night light component");
		if (!night)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(entity);
			return;
		}

		night.ForceRenders(true);

		// The light is not there yet: take any built child away, as a late stream would leave it.
		LightEntity built = night.FindLight();
		if (built)
			entity.RemoveChild(built);

		night.Evaluate(22);
		ctx.Equal(1, night.Misses(), "the first look misses once");
		ctx.False(night.GaveUp(), "one miss logs no error");

		// The child appears a frame late.
		LightEntity late = built;
		if (!late)
			late = LightEntity.CreateLight(LightType.POINT, 0, 3, Color.White, 1, entity.GetOrigin());

		ctx.NotNull(late, "a late light can be made");
		if (late)
		{
			entity.AddChild(late, -1);

			late.SetEnabled(false);

			if (!RedProof())
				night.Evaluate(22);

			ctx.True(late.IsEnabled(), "the late light is switched on at 22:00");
			ctx.False(night.GaveUp(), "a light found on the second look logs no error");
			ctx.Equal(0, night.Misses(), "a found light clears the misses");
			SCR_EntityHelper.DeleteEntityAndChildren(late);
		}

		SCR_EntityHelper.DeleteEntityAndChildren(entity);

		// A child that never comes: after the retries the component builds the light and switches it on.
		IEntity bare = LF_Utils.SpawnEntityPrefab(GLOW, Vector(452, groundY + 2, 100));
		ctx.NotNull(bare, "second window glow spawned");
		if (!bare)
			return;

		LF_NightLightComponent none = LF_NightLightComponent.Cast(bare.FindComponent(LF_NightLightComponent));
		if (none)
		{
			none.ForceRenders(true);
			LightEntity gone = none.FindLight();
			if (gone)
				bare.RemoveChild(gone);

			int i;
			for (i = 0; i < LF_NightLight.RETRY_MAX; i++)
				none.Evaluate(22);

			ctx.False(none.Built(), "no light is built inside the retries");
			ctx.False(none.GaveUp(), "no error inside the retries");
			none.Evaluate(22);
			ctx.True(none.Built(), "the light is built when the retries run out");
			ctx.False(none.GaveUp(), "a built light logs no error");
			LightEntity made = none.FindLight();
			ctx.NotNull(made, "the built light is a child of the glow");
			if (made)
				ctx.True(made.IsEnabled(), "the built light is on at 22:00");

			if (gone)
				SCR_EntityHelper.DeleteEntityAndChildren(gone);
		}

		SCR_EntityHelper.DeleteEntityAndChildren(bare);
	}
}
