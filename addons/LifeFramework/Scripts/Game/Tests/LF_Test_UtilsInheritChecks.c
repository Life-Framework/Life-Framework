// red-proof: with `RedProof()` on the helpers are not asked, so every answer keeps its wrong default and the type and prefab
// checks fail.

//! core/utils-inherit-checks: the two "is this one of those" helpers (LF_Utils). `IsAnyInherited` says whether a class is, or
//! derives from, any class on a list (itself counts, a base never matches its child, an empty list or a missing type is no);
//! `IsInstanceAnyInherited` asks it of a live object (a null object is no). `PrefabInherits` is false for an empty name or
//! ancestor, true for the same prefab (the GUID decides, so a prefab moved to another folder is still itself), and false for a
//! different GUID that is not the same prefab's path.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_UtilsInheritChecks : LF_Test
{
	override string GetName()
	{
		return "core/utils-inherit-checks";
	}

	override void Run(LF_TestContext ctx)
	{
		bool childOfBase = false;
		bool itself = false;
		bool baseOfChild = true;
		bool emptyList = true;
		bool noType = true;
		bool onePastMiss = false;
		bool liveInstance = false;
		bool nullInstance = true;
		bool liveMiss = true;
		bool emptyName = true;
		bool emptyAncestor = true;
		bool samePrefab = false;
		bool movedPrefab = false;
		bool samePath = false;
		if (!RedProof())
		{
			array<typename> bases = {LF_Test};
			childOfBase = LF_Utils.IsAnyInherited(LF_Test_UtilsInheritChecks, bases);
			array<typename> itselfList = {LF_Test_UtilsInheritChecks};
			itself = LF_Utils.IsAnyInherited(LF_Test_UtilsInheritChecks, itselfList);
			array<typename> childList = {LF_Test_UtilsInheritChecks};
			baseOfChild = LF_Utils.IsAnyInherited(LF_Test, childList);
			array<typename> empty = {};
			emptyList = LF_Utils.IsAnyInherited(LF_Test_UtilsInheritChecks, empty);
			typename missing;
			noType = LF_Utils.IsAnyInherited(missing, bases);
			array<typename> mixed = {LF_TestContext, LF_Test};
			onePastMiss = LF_Utils.IsAnyInherited(LF_Test_UtilsInheritChecks, mixed);
			liveInstance = LF_Utils.IsInstanceAnyInherited(this, bases);
			LF_Test_UtilsInheritChecks nobody;
			nullInstance = LF_Utils.IsInstanceAnyInherited(nobody, bases);
			array<typename> unrelated = {LF_TestContext};
			liveMiss = LF_Utils.IsInstanceAnyInherited(this, unrelated);

			emptyName = LF_Utils.PrefabInherits("", "{AAAAAAAAAAAAAAAA}Prefabs/Test/Same.et");
			emptyAncestor = LF_Utils.PrefabInherits("{AAAAAAAAAAAAAAAA}Prefabs/Test/Same.et", "");
			samePrefab = LF_Utils.PrefabInherits("{AAAAAAAAAAAAAAAA}Prefabs/Test/Same.et", "{AAAAAAAAAAAAAAAA}Prefabs/Test/Same.et");
			movedPrefab = LF_Utils.PrefabInherits("{AAAAAAAAAAAAAAAA}Prefabs/Test/Moved/Same.et", "{AAAAAAAAAAAAAAAA}Prefabs/Test/Same.et");
			samePath = LF_Utils.PrefabInherits("Prefabs/Test/Same.et", "prefabs/test/same.et");
		}

		ctx.True(childOfBase, "a class derived from a listed base is on the list");
		ctx.True(itself, "a class is on a list that names it");
		ctx.False(baseOfChild, "a base class is not a child of its own child");
		ctx.False(emptyList, "an empty list matches nothing");
		ctx.False(noType, "no type matches nothing");
		ctx.True(onePastMiss, "a match later in the list still counts");
		ctx.True(liveInstance, "a live object is checked by its own class");
		ctx.False(nullInstance, "a missing object is never on a list");
		ctx.False(liveMiss, "a live object of an unrelated class is not on the list");
		ctx.False(emptyName, "an empty prefab name inherits nothing");
		ctx.False(emptyAncestor, "an empty ancestor is never matched");
		ctx.True(samePrefab, "a prefab is its own ancestor");
		ctx.True(movedPrefab, "a prefab moved to another folder is still the same prefab (the GUID decides)");
		ctx.True(samePath, "with no GUID on either side the path decides, ignoring case");
	}
}
