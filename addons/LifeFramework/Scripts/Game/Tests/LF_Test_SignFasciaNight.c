// red-proof: `tools\cli test --no-build --red LF_Test_SignFasciaNight`: RedProof() skips the 22:00 look, so "the fascia lamp is lit at 22:00" fails.
//! A shop fascia is readable at night (night sweep 2026-11-25, card A): the prefab carries a night light
//! on its root, its lamp stands in front of the face (the face reads from the body's local +Z) and points
//! back at it, and it comes on at 22:00 and is off at 12:00. The lamp rides on the prefab, so no layer or
//! script names a coordinate.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_SignFasciaNight : LF_Test
{
	static const ResourceName FASCIA_PREFAB = "{4697D9464B596C24}Prefabs/Props/Signs/LF_Sign_Fascia.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "signs/fascia-night";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		float groundY = GetGame().GetWorld().GetSurfaceY(450, 100);
		IEntity entity = LF_Utils.SpawnEntityPrefab(FASCIA_PREFAB, Vector(450, groundY + 1, 100));
		ctx.NotNull(entity, "fascia spawned");
		if (!entity)
			return;

		LF_NightLightComponent night = LF_NightLightComponent.Cast(entity.FindComponent(LF_NightLightComponent));
		ctx.NotNull(night, "the fascia has a night light component");
		if (night)
		{
			vector offset = night.LightOffset();
			vector direction = night.LightDirection();
			ctx.True(offset[2] > 0.5, "the lamp stands in front of the face (local +Z)");
			ctx.True(offset[1] > 0.9, "the lamp hangs above the plate");
			ctx.True(direction[2] < -0.5, "the lamp points back at the face");
			ctx.True(direction[1] < 0, "and down onto it");

			if (!RedProof())
				night.Evaluate(22);

			ctx.True(night.IsLit(), "the fascia lamp is lit at 22:00");

			night.Evaluate(12);
			ctx.False(night.IsLit(), "the fascia lamp is off at 12:00");
		}

		SCR_EntityHelper.DeleteEntityAndChildren(entity);
	}
}
