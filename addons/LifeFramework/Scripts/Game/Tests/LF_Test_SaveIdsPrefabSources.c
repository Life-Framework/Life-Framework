// red-proof: with `RedProof()` on the alias table is not asked, so no source is listed, nothing loads or is marked missing, and
// the "sources", "loads", "missing once" and "empty saved id" checks fail.

//! persist/save-ids-prefab-sources: the rarely taken prefab branches of LF_SaveIds. `PrefabSourcesOf` lists every older
//! prefab id whose alias chain ENDS at a target (a system that counts one kind of item, such as cash, counts these too
//! because an old save may still hold them), `PrefabLoads` says whether a prefab loads (never for an empty id), `NoteMissing`
//! marks a missing id once per session by GUID whatever the path, and `SpawnablePrefab` of an empty saved id takes the first
//! fallback that loads without calling anything missing.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_SaveIdsPrefabSources : LF_Test
{
	static const ResourceName TARGET = "{0E2EA11A50000A03}Prefabs/Test/Target.et";
	static const ResourceName MID = "{0E2EA11A50000A02}Prefabs/Test/Mid.et";
	static const ResourceName OLD = "{0E2EA11A50000A01}Prefabs/Test/Old.et";
	static const ResourceName SIDE = "{0E2EA11A50000A04}Prefabs/Test/Side.et";
	static const ResourceName UNRELATED_FROM = "{0E2EA11A50000A05}Prefabs/Test/Unrelated.et";
	static const ResourceName UNRELATED_TO = "{0E2EA11A50000A06}Prefabs/Test/UnrelatedNew.et";
	static const ResourceName APPLE = "{C9D661E5B0714711}Prefabs/Items/Food/Apple.et";
	static const ResourceName GONE = "{0E2EA11A500000FF}Prefabs/Items/Food/Gone.et";

	override string GetName()
	{
		return "persist/save-ids-prefab-sources";
	}

	//------------------------------------------------------------------------------------------------
	protected bool Lists(array<ResourceName> sources, ResourceName prefab)
	{
		foreach (ResourceName source : sources)
		{
			if (LF_Utils.SamePrefab(source, prefab))
				return true;
		}

		return false;
	}

	override void Run(LF_TestContext ctx)
	{
		int targetCount = -1;
		bool targetHasOld = false;
		bool targetHasMid = false;
		bool targetHasSide = false;
		bool targetHasItself = true;
		bool targetHasUnrelated = true;
		int midCount = -1;
		bool midHasOld = false;
		int oldCount = -1;
		int emptyTargetCount = -1;
		int strangerCount = -1;
		bool loadsApple = false;
		bool loadsGone = true;
		bool loadsEmpty = true;
		bool missingBefore = true;
		bool missingAfter = false;
		bool missingByGuid = false;
		bool missingOtherKind = true;
		bool missingOtherPrefab = true;
		ResourceName emptySaved = ResourceName.Empty;
		bool emptySavedMissing = true;
		string kinds = "-";
		if (!RedProof())
		{
			LF_SaveIds.ResetStatics();
			LF_SaveIds.Add(LF_SaveIds.KIND_PREFAB, OLD, MID);
			LF_SaveIds.Add(LF_SaveIds.KIND_PREFAB, MID, TARGET);
			LF_SaveIds.Add(LF_SaveIds.KIND_PREFAB, SIDE, TARGET);
			LF_SaveIds.Add(LF_SaveIds.KIND_PREFAB, UNRELATED_FROM, UNRELATED_TO);

			array<ResourceName> sources = {};
			LF_SaveIds.PrefabSourcesOf(TARGET, sources);
			targetCount = sources.Count();
			targetHasOld = Lists(sources, OLD);
			targetHasMid = Lists(sources, MID);
			targetHasSide = Lists(sources, SIDE);
			targetHasItself = Lists(sources, TARGET);
			targetHasUnrelated = Lists(sources, UNRELATED_FROM);

			array<ResourceName> midSources = {};
			LF_SaveIds.PrefabSourcesOf(MID, midSources);
			midCount = midSources.Count();
			midHasOld = Lists(midSources, OLD);

			array<ResourceName> oldSources = {};
			LF_SaveIds.PrefabSourcesOf(OLD, oldSources);
			oldCount = oldSources.Count();

			array<ResourceName> emptyTarget = {};
			LF_SaveIds.PrefabSourcesOf("", emptyTarget);
			emptyTargetCount = emptyTarget.Count();

			array<ResourceName> stranger = {};
			LF_SaveIds.PrefabSourcesOf("{0E2EA11A50000AFF}Prefabs/Test/Nobody.et", stranger);
			strangerCount = stranger.Count();

			loadsApple = LF_SaveIds.PrefabLoads(APPLE);
			loadsGone = LF_SaveIds.PrefabLoads(GONE);
			loadsEmpty = LF_SaveIds.PrefabLoads("");

			missingBefore = LF_SaveIds.WasMissing(LF_SaveIds.KIND_PREFAB, GONE);
			LF_SaveIds.NoteMissing(LF_SaveIds.KIND_PREFAB, GONE, "");
			LF_SaveIds.NoteMissing(LF_SaveIds.KIND_PREFAB, GONE, " (again)");
			missingAfter = LF_SaveIds.WasMissing(LF_SaveIds.KIND_PREFAB, GONE);
			missingByGuid = LF_SaveIds.WasMissing(LF_SaveIds.KIND_PREFAB, "{0e2ea11a500000ff}Prefabs/Moved/Elsewhere.et");
			missingOtherKind = LF_SaveIds.WasMissing(LF_SaveIds.KIND_JOB, GONE);
			missingOtherPrefab = LF_SaveIds.WasMissing(LF_SaveIds.KIND_PREFAB, APPLE);

			array<ResourceName> fallbacks = {GONE, APPLE};
			emptySaved = LF_SaveIds.SpawnablePrefab(ResourceName.Empty, fallbacks);
			emptySavedMissing = LF_SaveIds.WasMissing(LF_SaveIds.KIND_PREFAB, ResourceName.Empty);
			LF_SaveIds.ResetStatics();
		}

		ctx.Equal(3, targetCount, "three older ids lead to the target: the middle one, the one behind it, and the side branch");
		ctx.True(targetHasOld, "an id two links back is a source (the chain is followed)");
		ctx.True(targetHasMid, "an id one link back is a source");
		ctx.True(targetHasSide, "a second branch into the target is a source");
		ctx.False(targetHasItself, "the target is not its own source");
		ctx.False(targetHasUnrelated, "an alias leading elsewhere is not a source");
		ctx.Equal(0, midCount, "an id whose chain runs on past the middle one ends at the target, so the middle one has no sources");
		ctx.False(midHasOld, "a source is listed under the id its chain ends at, not under every id on the way");
		ctx.Equal(0, oldCount, "nothing leads to the oldest id");
		ctx.Equal(0, emptyTargetCount, "an empty target has no sources");
		ctx.Equal(0, strangerCount, "a prefab no alias leads to has no sources");

		ctx.True(loadsApple, "a real prefab loads");
		ctx.False(loadsGone, "a prefab that is not there does not load");
		ctx.False(loadsEmpty, "an empty id never loads");

		ctx.False(missingBefore, "nothing is missing before a load meets it");
		ctx.True(missingAfter, "a noted id is missing");
		ctx.True(missingByGuid, "a prefab is missing by GUID, whatever its path or case");
		ctx.False(missingOtherKind, "a missing prefab does not make a job of the same text missing");
		ctx.False(missingOtherPrefab, "another prefab is not missing");
		ctx.EqualStr(APPLE, emptySaved, "an empty saved id spawns from the first fallback that loads");
		ctx.False(emptySavedMissing, "an empty saved id is not reported as missing content");
	}
}
