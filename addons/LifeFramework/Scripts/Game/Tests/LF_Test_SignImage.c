// red-proof: `tools\cli test --no-build --only LF_Test_SignImage` (2026-11-11) failed signs/image
// with "the image name defaults to full (expected 'full', got '')" until m_sImageName got its initializer.

// tier: LOGIC
class LF_Test_SignImage : LF_Test
{
	override string GetName()
	{
		return "signs/image";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_SignDef plain = LF_Test_SignCatalog.MakeSign("bank", "Bank", "Money is kept here.", "", "", "");
		array<ref LF_UIPart> plainParts = LF_SignCatalog.BuildParts(plain);
		foreach (LF_UIPart none : plainParts)
			ctx.True(none.GetKind() != LF_EUIPartKind.IMAGE, "a sign with no image has no picture part");

		LF_SignDef pictured = LF_Test_SignCatalog.MakeSign("hospital", "Hospital", "Medics heal you.", "", "", "");
		pictured.m_sImage = "{F02831E43418249A}Assets/UI/Illustrations/LF_IdStamp.imageset";
		array<ref LF_UIPart> parts = LF_SignCatalog.BuildParts(pictured);
		ctx.Equal(plainParts.Count() + 1, parts.Count(), "an image adds one part");
		if (parts.Count() < 3)
			return;

		LF_UIPart picture = parts.Get(2);
		ctx.Equal(LF_EUIPartKind.IMAGE, picture.GetKind(), "the picture sits under the title and its rule");
		ctx.EqualStr(pictured.m_sImage, picture.GetText(), "the part carries the imageset");
		ctx.EqualStr("full", picture.GetStatus(), "the image name defaults to full");
		ctx.True(LF_UIPart.LayoutFor(LF_EUIPartKind.IMAGE) != "", "the picture part has a layout");
	}
}
