// red-proof: observed 2026-11-14 with SamePrefab's GUID branch swapped for a path compare in LF_Utils: `- a live charge matches its GUID under a wrong path text` failed; restored, it passed.
//------------------------------------------------------------------------------------------------
//! prefab/guid-compare (#323): a prefab string saved or configured with the right GUID and a
//! deliberately wrong path still matches the live entity, and the same path text under another
//! GUID does not. Guards the compares in LF_Breach, LF_BankRobKit and the asset-state harness.
// tier: WORLD
class LF_Test_GuidCompare : LF_Test
{
	override string GetName()
	{
		return "prefab/guid-compare";
	}

	override void Run(LF_TestContext ctx)
	{
		ResourceName wrongPath = "{" + LF_Utils.PrefabGuid(LF_Breach.FENCE_CHARGE_PREFAB) + "}Prefabs/Nowhere/Wrong/NotTheCharge.et";
		ResourceName otherGuid = "{" + LF_Utils.PrefabGuid(LF_Breach.CHARGE_PREFAB) + "}Prefabs/Items/Fence/LF_Charge.et";

		IEntity charge = LF_Utils.SpawnEntityPrefab(LF_Breach.FENCE_CHARGE_PREFAB, vector.Zero);
		ctx.NotNull(charge, "the fence charge spawns");
		if (!charge)
			return;

		string live = LF_Utils.GetPrefabName(charge);
		ctx.True(wrongPath != live, "the wrong path text differs from the live name");
		ctx.True(LF_Utils.SamePrefab(live, wrongPath), "a live charge matches its GUID under a wrong path text");
		ctx.True(!LF_Utils.SamePrefab(live, otherGuid), "the same path text under another GUID does not match");
		ctx.True(LF_Utils.SamePrefab(wrongPath, LF_Breach.FENCE_CHARGE_PREFAB), "a configured name with a wrong path matches the constant");

		SCR_EntityHelper.DeleteEntityAndChildren(charge);
	}
}
