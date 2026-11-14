// red-proof: `tools\cli test --no-build --only LF_Test_SignsOverlay` (2026-11-14): with the m_sImage patch
// line in LF_SignList.PatchFromContainer replaced by `if (false)`, got `FAIL signs/overlay` "patch: the
// hospital sign gets the overlay's picture (expected '{F02831E43418249A}...LF_IdStamp.imageset', got '')". Restored; green.

// tier: LOGIC
//! A pack overlay of LF_SignConfig patches and adds signs by id (#323): the 1989 pack gives the core
//! hospital sign its picture this way, so the core LF_Signs.conf names no pack resource.
class LF_Test_SignsOverlay : LF_Test
{
	static const ResourceName OVERLAY = "{6BF6404AB2FD3B8D}Configs/Tests/LF_Test_SignsOverlay.conf";
	static const ResourceName STAMP = "{F02831E43418249A}Assets/UI/Illustrations/LF_IdStamp.imageset";

	override string GetName()
	{
		return "signs/overlay";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_SignConfig config = new LF_SignConfig();
		config.m_aSigns = new array<ref LF_SignDef>();
		LF_SignDef hospital = LF_Test_SignCatalog.MakeSign("hospital", "Hospital", "Medics heal you.", "", "", "");
		config.m_aSigns.Insert(hospital);
		config.m_aSigns.Insert(LF_Test_SignCatalog.MakeSign("bank", "Bank", "Money is kept here.", "", "", ""));

		Resource resource = BaseContainerTools.LoadContainer(OVERLAY);
		ctx.True(resource && resource.IsValid() && resource.GetResource(), "the overlay fixture loads");
		if (!resource || !resource.IsValid() || !resource.GetResource())
			return;

		int changed = config.ApplyContainer(resource.GetResource().ToBaseContainer(), OVERLAY);
		ctx.Equal(2, changed, "the overlay patches one sign and adds one");
		LF_SignDef patched = LF_SignCatalog.FindIn(config, "hospital");
		ctx.EqualStr(STAMP, patched.m_sImage, "patch: the hospital sign gets the overlay's picture");
		ctx.EqualStr("Hospital", patched.m_sTitle, "and keeps the core title it did not set");
		ctx.EqualStr("", LF_SignCatalog.FindIn(config, "bank").m_sImage, "a sign the overlay does not name is untouched");
		LF_SignDef added = LF_SignCatalog.FindIn(config, "test_overlay_sign");
		ctx.True(added != null && added.m_sTitle == "Overlay sign", "add: a new id joins the catalogue");
	}
}
