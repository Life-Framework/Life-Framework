// red-proof: Observed 2026-11-08 with `tools\cli test --no-build --tier all`: Lathe.et
// listed as {9FB8E1E473BF6504}Prefabs/Crafting/LatheMissing.et failed it with
// `[ELTEST] FAIL crafting/assets`, `- crafting prefab loads: {9FB8E1E473BF6504}Prefabs/Crafting/LatheMissing.et`.
// A wrong GUID with the right path, or a wrong path with the right GUID, still loads (observed
// green both ways): the engine resolves either half, so this test catches a deleted prefab, not a
// typo in one half. Restored; green. The README and file-existence checks live in
// tools/validation/validate-crafting-assets.mjs, which runs in `tools\cli validate`.
// tier: WORLD
class EL_Test_CraftingAssets : EL_Test
{
	override string GetName()
	{
		return "crafting/assets";
	}

	override void Run(EL_TestContext ctx)
	{
		// The engine resolves every ported crafting prefab by its {GUID}path.
		array<ResourceName> prefabs = {
			"{9FB8E1E473BF6503}Prefabs/Crafting/Lathe.et",
			"{9A539E857A434978}Prefabs/Crafting/SewingMachine.et",
			"{675FA53A080F67E0}Prefabs/Crafting/Boiler.et",
			"{0FB935C50541B694}Prefabs/Crafting/Fireplace.et",
			"{B2CB0DCEE184B35A}Prefabs/Crafting/Vice.et",
			"{6985A7CDF8ABF19F}Prefabs/Crafting/Charcoal.et",
			"{1B5FAD55637B9BC2}Prefabs/Crafting/Sulfur.et",
			"{9CB52FAF71DA568F}Prefabs/Crafting/Gunpowder.et",
			"{F0F6577FD74790D2}Prefabs/Crafting/Linen.et"
		};

		foreach (ResourceName prefab : prefabs)
		{
			Resource res = Resource.Load(prefab);
			ctx.True(res && res.IsValid(), "crafting prefab loads: " + prefab);
		}
	}
}
