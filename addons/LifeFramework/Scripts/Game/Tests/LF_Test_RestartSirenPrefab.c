// red-proof: 2026-11-15, cli test --no-build --red LF_Test_RestartSirenPrefab: RED-OK ops/siren-prefab (fails with the hook on, 4 assertions; the spawn is skipped), then --only passed=1 failed=0.
// tier: WORLD
//! ops/siren-prefab: the restart siren prefab is a placeable prop (#1345). Spawned, it registers
//! with LF_Places under the kind "restart-siren" and its mesh resolves (not an empty MeshObject).
class LF_Test_RestartSirenPrefab : LF_Test
{
	protected static const ResourceName PREFAB = "{947559670A40E3A5}Prefabs/Props/Ops/LF_RestartSiren.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "ops/siren-prefab";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		IEntity anchor = LF_Places.Find(LF_Bank.PLACE_KIND, LF_TestAnnex.BANK_VAULT);
		ctx.NotNull(anchor, "the annex vault is placed, to spawn the siren near");
		if (!anchor)
			return;

		array<IEntity> before = {};
		ctx.Equal(0, LF_Places.FindAll(LF_RestartSirenComponent.PLACE_KIND, before), "no restart siren is placed before the prefab spawns");

		IEntity siren = null;
		if (!RedProof())
			siren = LF_Utils.SpawnEntityPrefab(PREFAB, anchor.GetOrigin() + "0 -20 0", "0 0 0", false);

		ctx.NotNull(siren, "the restart siren prefab spawns");
		if (!siren)
			return;

		ctx.True(siren.FindComponent(LF_RestartSirenComponent) != null, "the prefab carries LF_RestartSirenComponent");
		ctx.True(LF_Places.Find(LF_RestartSirenComponent.PLACE_KIND, LF_RestartSirenComponent.DEFAULT_ID) == siren, "LF_Places finds the siren by kind");

		array<IEntity> found = {};
		ctx.Equal(1, LF_Places.FindAll(LF_RestartSirenComponent.PLACE_KIND, found), "exactly one restart siren is placed");
		ctx.True(siren.GetVObject() != null, "the siren mesh resolves");

		SCR_EntityHelper.DeleteEntityAndChildren(siren);
		ctx.True(LF_Places.Find(LF_RestartSirenComponent.PLACE_KIND, LF_RestartSirenComponent.DEFAULT_ID) == null, "a deleted siren is not found");
	}
}
