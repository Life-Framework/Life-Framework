// red-proof: with FaceLayoutScale returning the DPI scale for a game client,
// `tools\cli test --no-build --only EL_Test_SignFaceClient` (2026-11-08) failed
// signs/face-client with "a small game window keeps the layout font size (expected 96, got 32)".

// tier: LOGIC
class EL_Test_SignFaceClient : EL_Test
{
	override string GetName()
	{
		return "signs/face-client";
	}

	override void Run(EL_TestContext ctx)
	{
		// A 640x360 game client: DPIScale is 1/3, and the title must still fill the sheet.
		float scale = LF_SignComponent.FaceLayoutScale(1.0 / 3.0, false);
		ctx.EqualFloat(1, scale, 0.001, "a game client lays the title out at the layout size");

		vector pos;
		vector size;
		int font = LF_SignComponent.FaceTitleRect(scale, pos, size);
		ctx.Equal(96, font, "a small game window keeps the layout font size");
		ctx.EqualFloat(432, size[0], 0.01, "a small game window keeps the full title width");

		// A Workbench viewport still compensates (signs/face-fit).
		ctx.EqualFloat(0.5, LF_SignComponent.FaceLayoutScale(0.5, true), 0.001, "Workbench keeps the DPI scale");
	}
}
