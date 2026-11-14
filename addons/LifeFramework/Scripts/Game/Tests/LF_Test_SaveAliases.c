// red-proof: persist/save-aliases, observed 2026-11-14 with `tools\cli test --no-build --only LF_Test_SaveAliases`:
// stopping LF_SaveIds.Resolve after the first hop failed it with
// `- an old prefab id follows the chain to the apple (expected '{C9D661E5B0714711}Prefabs/Items/Food/Apple.et',
// got '{0E2EA11A500000C2}Prefabs/Items/Food/Apple_Mid.et')` and `- an alias loop is refused and the id loads
// unchanged (expected 'loop_a', got 'loop_b')`. Restored; green.
// red-proof: persist/save-aliases (spawnable body), observed 2026-11-14: skipping the fallback loop in
// LF_SaveIds.SpawnablePrefab failed it with `- a missing body spawns from the first default that loads
// (expected '{C9D661E5B0714711}Prefabs/Items/Food/Apple.et', got '')`. Restored; green.

//------------------------------------------------------------------------------------------------
//! The pack source of persist/save-aliases: the alias fixture for the alias table, plus a shops
//! overlay in the same list (a mission header carries every section's overlays in one list).
class LF_Test_SaveAliasSourceStub : LF_ConfigPackSource
{
	static const ResourceName FIXTURE = "{9E8A2122916530CF}Configs/Tests/LF_Test_SaveAliases.conf";

	override void GetOverlays(ResourceName coreConf, notnull array<ResourceName> overlays)
	{
		overlays.Insert(LF_Test_ConfigPackSourceStub.OVERLAY);
		overlays.Insert(FIXTURE);
	}

	override string GetName()
	{
		return "test-save-aliases";
	}
};

//------------------------------------------------------------------------------------------------
//! The alias table (#965): a pack file read through the mission-header seam, chains followed, a
//! prefab matched by GUID whatever its path, a loop and a bad row refused, a missing prefab kept.
// tier: WORLD
class LF_Test_SaveAliases : LF_Test
{
	static const ResourceName APPLE = "{C9D661E5B0714711}Prefabs/Items/Food/Apple.et";
	static const ResourceName OLD_APPLE = "{0E2EA11A500000C1}Prefabs/Items/Food/Apple_Old.et";
	static const ResourceName GONE = "{0E2EA11A500000FF}Prefabs/Items/Food/Gone.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "persist/save-aliases";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_SaveIds.ResetStatics();
		LF_ConfigService.SetPackSource(new LF_Test_SaveAliasSourceStub());

		ctx.Equal(5, LF_SaveIds.Count(), "the pack file's five good rows load; the unknown kind is refused and the shops overlay is not read as aliases");

		ctx.EqualStr(APPLE, LF_SaveIds.Resolve(LF_SaveIds.KIND_PREFAB, OLD_APPLE), "an old prefab id follows the chain to the apple");
		ctx.EqualStr(APPLE, LF_SaveIds.Resolve(LF_SaveIds.KIND_PREFAB, "{0e2ea11a500000c1}Prefabs/Moved/Elsewhere.et"), "a prefab alias matches the GUID, whatever the path and case");
		ctx.EqualStr(APPLE, LF_SaveIds.ResolvePrefab(OLD_APPLE), "ResolvePrefab takes the alias when its target loads");
		ctx.EqualStr(APPLE, LF_SaveIds.ResolvePrefab(APPLE), "a current id resolves to itself (a second load changes nothing)");
		ctx.EqualStr("DELIVERY", LF_SaveIds.Resolve(LF_SaveIds.KIND_JOB, "COURIER"), "a renamed job id loads as the new one");
		ctx.EqualStr("COURIER", LF_SaveIds.Resolve(LF_SaveIds.KIND_QUEST, "COURIER"), "an alias applies to its own kind only");
		ctx.EqualStr("loop_a", LF_SaveIds.Resolve(LF_SaveIds.KIND_PLACE, "loop_a"), "an alias loop is refused and the id loads unchanged");
		ctx.EqualStr("red", LF_SaveIds.Resolve("colour", "red"), "the row with an unknown kind was not added");

		ctx.False(LF_SaveIds.WasMissing(LF_SaveIds.KIND_PREFAB, GONE), "nothing is missing before the load meets it");
		ctx.EqualStr(GONE, LF_SaveIds.ResolvePrefab(GONE), "a prefab that does not load keeps its saved id");
		ctx.True(LF_SaveIds.WasMissing(LF_SaveIds.KIND_PREFAB, GONE), "the missing prefab is logged");

		ctx.True(LF_SaveIds.Add(LF_SaveIds.KIND_PREFAB, GONE, APPLE), "an alias added later (content back under a new id)");
		ctx.EqualStr(APPLE, LF_SaveIds.ResolvePrefab(GONE), "the missing id resolves once its alias exists");
		ctx.False(LF_SaveIds.Add(LF_SaveIds.KIND_PREFAB, APPLE, "{C9D661E5B0714711}" + "Prefabs/Other/Apple.et"), "an alias from an id to itself (same GUID) is refused");

		// A character body whose content is gone still spawns, from a default body; the id is kept.
		array<ResourceName> bodies = {GONE, APPLE};
		ctx.EqualStr(APPLE, LF_SaveIds.SpawnablePrefab(APPLE, bodies), "a body that loads spawns as itself");
		ctx.EqualStr(APPLE, LF_SaveIds.SpawnablePrefab("{0E2EA11A500000FE}Prefabs/Characters/Gone.et", bodies), "a missing body spawns from the first default that loads");
		ctx.True(LF_SaveIds.WasMissing(LF_SaveIds.KIND_PREFAB, "{0E2EA11A500000FE}Prefabs/Characters/Gone.et"), "the missing body is logged");
		ctx.EqualStr("", LF_SaveIds.SpawnablePrefab("{0E2EA11A500000FE}Prefabs/Characters/Gone.et", null), "no default that loads: nothing to spawn");

		LF_ConfigService.SetPackSource(null);
		LF_SaveIds.ResetStatics();
		ctx.Equal(0, LF_SaveIds.Count(), "the shipped core table is empty");
		LF_SaveIds.ResetStatics();
	}
}
