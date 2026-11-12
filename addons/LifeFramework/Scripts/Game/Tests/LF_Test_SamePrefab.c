// red-proof: observed 2026-11-12 with `tools\cli test --no-build --only LF_Test_SamePrefab`: making LF_Utils.SamePrefab compare paths when both names carry a GUID failed `- one GUID at two paths is one prefab`, `- a bare GUID matches its prefab` and `- another GUID at the same path is another prefab`. Restored; green.
//------------------------------------------------------------------------------------------------
//! prefab/same-prefab (#323): LF_Utils.SamePrefab and PrefabGuid. The GUID decides; the path only
//! when a side has no GUID; casing never matters; empty never matches.
// tier: LOGIC
class LF_Test_SamePrefab : LF_Test
{
	// Split from the path so enforce-lint L6 does not read a deliberate mismatch as a broken ref.
	static const string PICKAXE_GUID = "{23341E2840F5ACB4}";
	static const string AXE_GUID = "{9CAC68BA551417A4}";

	override string GetName()
	{
		return "prefab/same-prefab";
	}

	override void Run(LF_TestContext ctx)
	{
		ctx.True(LF_Utils.PrefabGuid("{23341E2840F5ACB4}Prefabs/Tools/Pickaxe.et") == "23341E2840F5ACB4", "the GUID is the part in braces");
		ctx.True(LF_Utils.PrefabGuid("{23341e2840f5acb4}Prefabs/Tools/Pickaxe.et") == "23341E2840F5ACB4", "the GUID comes back upper-cased");
		ctx.True(LF_Utils.PrefabGuid("{23341E2840F5ACB4}") == "23341E2840F5ACB4", "a bare GUID is its own GUID");
		ctx.True(LF_Utils.PrefabGuid("Prefabs/Tools/Pickaxe.et") == "PREFABS/TOOLS/PICKAXE.ET", "a name without a GUID keys on its path");

		ctx.True(LF_Utils.SamePrefab("{23341E2840F5ACB4}Prefabs/Tools/Pickaxe.et", PICKAXE_GUID + "Prefabs/Moved/Pickaxe.et"), "one GUID at two paths is one prefab");
		ctx.True(LF_Utils.SamePrefab("{23341E2840F5ACB4}Prefabs/Tools/Pickaxe.et", "{23341e2840f5acb4}prefabs/tools/pickaxe.et"), "casing does not split a prefab");
		ctx.True(LF_Utils.SamePrefab("{23341E2840F5ACB4}Prefabs/Tools/Pickaxe.et", "{23341E2840F5ACB4}"), "a bare GUID matches its prefab");
		ctx.False(LF_Utils.SamePrefab("{23341E2840F5ACB4}Prefabs/Tools/Pickaxe.et", AXE_GUID + "Prefabs/Tools/Pickaxe.et"), "another GUID at the same path is another prefab");
		ctx.False(LF_Utils.SamePrefab("{23341E2840F5ACB4}Prefabs/Tools/Pickaxe.et", "{9CAC68BA551417A4}Prefabs/Tools/Axe/Axe.et"), "the axe is not the pickaxe");

		ctx.True(LF_Utils.SamePrefab("Prefabs/Tools/Pickaxe.et", "{23341E2840F5ACB4}Prefabs/Tools/Pickaxe.et"), "a name without a GUID falls back to the path");
		ctx.True(LF_Utils.SamePrefab("prefabs/tools/pickaxe.et", "Prefabs/Tools/Pickaxe.et"), "two bare paths compare without casing");
		ctx.False(LF_Utils.SamePrefab("Prefabs/Moved/Pickaxe.et", "{23341E2840F5ACB4}Prefabs/Tools/Pickaxe.et"), "a bare path at another folder is not the prefab");

		ctx.False(LF_Utils.SamePrefab("{23341E2840F5ACB4}Prefabs/Tools/Pickaxe.et", ""), "no prefab is not the pickaxe");
		ctx.False(LF_Utils.SamePrefab("", ""), "two empty names are not one prefab");
	}
}
