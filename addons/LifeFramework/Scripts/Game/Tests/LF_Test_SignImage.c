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
		ctx.Equal(3, plainParts.Count(), "a sign with no image has title, What, Close");

		LF_SignDef pictured = LF_Test_SignCatalog.MakeSign("hospital", "Hospital", "Medics heal you.", "", "", "");
		pictured.m_sImage = "{443758FF56C25F58}Assets/UI/Illustrations/medic_flow.imageset";
		array<ref LF_UIPart> parts = LF_SignCatalog.BuildParts(pictured);
		ctx.Equal(4, parts.Count(), "an image adds one part");
		if (parts.Count() != 4)
			return;

		LF_UIPart picture = parts.Get(1);
		ctx.Equal(LF_EUIPartKind.IMAGE, picture.GetKind(), "the picture sits right under the title");
		ctx.EqualStr(pictured.m_sImage, picture.GetText(), "the part carries the imageset");
		ctx.EqualStr("full", picture.GetStatus(), "the image name defaults to full");
		ctx.True(LF_UIPart.LayoutFor(LF_EUIPartKind.IMAGE) != "", "the picture part has a layout");
	}
}
