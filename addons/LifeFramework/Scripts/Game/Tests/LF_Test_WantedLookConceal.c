// red-proof: observed 2026-11-17 with `tools\cli test --no-build --only LF_Test_WantedLookConceal` on the
// unfixed Police scripts: "test-e2e: FAILED (scripts do not compile)" (LF_WantedReports.ApplyConcealment and
// LF_WantedLook.Witnessed did not exist). With the fix: passed=1 failed=0.
// tier: WORLD
//! What a suspect's clothes hide from a witness (#2438): the green raincoat hides the build and a
//! face-concealing catalogue item hides the face; the description says so, a hidden face leaves no
//! wanted portrait, a hidden build keeps it, and a plain outfit is described as before. The photo ID's
//! LF_WantedLook.Capture stays the full face.
class LF_Test_WantedLookConceal : LF_Test
{
	protected static const ResourceName BALACLAVA = "{67662815EFF734DD}Prefabs/Characters/HeadGear/Hat_FlatCap_01/Hat_FlatCap_01_darkgrey.et";
	protected static const string LOOK = "9B5BB216CC7FF18E|0=9F546CCA2582D16F";

	override string GetName()
	{
		return "police/wanted-look-conceal";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_ItemCatalogueConfig config = LF_Test_ItemCatalogue.MakeConfig();
		LF_CatalogueItem mask = LF_CatalogueItem.Make("balaclava", BALACLAVA, "clothing", 20, 5, "legal");
		mask.m_bConcealsFace = true;
		config.m_aItems.Insert(mask);

		// The text and the portrait, from a worn list.
		array<ResourceName> coat = {LF_Test_ItemCatalogue.RAINCOAT};
		bool face;
		bool build;
		LF_ItemCatalogue.Concealment(config, coat, face, build);
		ctx.EqualStr("a green jacket|" + LF_WantedReports.CLOTHING_BUILD_HIDDEN, LF_WantedReports.DescribeConcealed("a green jacket", face, build), "a raincoat reports the build hidden after the clothing seen");
		ctx.EqualStr(LOOK, LF_WantedLook.Witnessed(LOOK, face), "a hidden build keeps the portrait");

		array<ResourceName> masked = {BALACLAVA};
		LF_ItemCatalogue.Concealment(config, masked, face, build);
		ctx.EqualStr(LF_WantedReports.CLOTHING_FACE_HIDDEN, LF_WantedReports.DescribeConcealed("", face, build), "a face-concealing item reports the face hidden, with no clothing text before it");
		ctx.EqualStr("", LF_WantedLook.Witnessed(LOOK, face), "a hidden face drops the portrait");

		array<ResourceName> both = {LF_Test_ItemCatalogue.RAINCOAT, BALACLAVA};
		LF_ItemCatalogue.Concealment(config, both, face, build);
		ctx.EqualStr("a green jacket|" + LF_WantedReports.CLOTHING_FACE_HIDDEN + "|" + LF_WantedReports.CLOTHING_BUILD_HIDDEN, LF_WantedReports.DescribeConcealed("a green jacket", face, build), "both hide both");

		array<ResourceName> plain = {LF_Test_ItemCatalogue.PISTOL};
		LF_ItemCatalogue.Concealment(config, plain, face, build);
		ctx.EqualStr("a green jacket", LF_WantedReports.DescribeConcealed("a green jacket", face, build), "a plain outfit is described as before");
		ctx.EqualStr(LOOK, LF_WantedLook.Witnessed(LOOK, face), "a plain outfit keeps the portrait");

		// The real path: a body wearing a catalogue item that hides the build and one that hides the face.
		IEntity suspect = LF_TestCharacterFixture.Spawn(ctx);
		if (!suspect)
			return;

		string fullLook = LF_WantedLook.Capture(suspect);
		LF_CatalogueItem jacket = LF_CatalogueItem.Make("test_jacket", LF_TestCharacterFixture.JACKET_PREFAB, "clothing", 10, 1, "legal");
		jacket.m_bConcealsBuild = true;
		LF_ItemCatalogueConfig wornConfig = LF_Test_ItemCatalogue.MakeConfig();
		wornConfig.m_aItems.Insert(jacket);

		string clothing = "a green jacket";
		string look;
		LF_WantedReports.ApplyConcealment(suspect, wornConfig, clothing, look);
		ctx.EqualStr("a green jacket|" + LF_WantedReports.CLOTHING_BUILD_HIDDEN, clothing, "the jacket the suspect wears hides the build");
		ctx.EqualStr(fullLook, look, "the build alone keeps the portrait");

		jacket.m_bConcealsBuild = false;
		jacket.m_bConcealsFace = true;
		clothing = "a green jacket";
		LF_WantedReports.ApplyConcealment(suspect, wornConfig, clothing, look);
		ctx.EqualStr("a green jacket|" + LF_WantedReports.CLOTHING_FACE_HIDDEN, clothing, "a worn face-concealing item hides the face");
		ctx.EqualStr("", look, "no portrait for a covered face");
		ctx.True(fullLook.StartsWith("9B5BB216CC7FF18E"), "the ID photo's Capture still has the whole look");

		LF_TestCharacterFixture.Delete(suspect);
	}
}
