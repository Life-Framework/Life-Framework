// red-proof: expected 64 for the 714 px window font; `tools\cli test --no-build --tier fast`
// (2026-11-08) failed signs/face-fit with "the font shrinks with the sheet
// (expected 64, got 63)".

// tier: LOGIC
class EL_Test_SignFaceFit : EL_Test
{
	override string GetName()
	{
		return "signs/face-fit";
	}

	override void Run(EL_TestContext ctx)
	{
		vector pos;
		vector size;

		// 1080p: the layout's own numbers.
		int font = LF_SignComponent.FaceTitleRect(1, pos, size);
		ctx.Equal(96, font, "1080p keeps the layout font size");
		ctx.EqualFloat(40, pos[0], 0.01, "1080p keeps the left margin");
		ctx.EqualFloat(432, size[0], 0.01, "1080p title spans the sheet less both margins");
		ctx.EqualFloat(644, size[1], 0.01, "1080p title spans the sheet height less both margins");

		// A 714 px tall Workbench viewport: the shown part of the target is
		// 512 * 0.661 = 338 units wide. The whole title box must sit inside it.
		float scale = 714.0 / 1080.0;
		font = LF_SignComponent.FaceTitleRect(scale, pos, size);
		ctx.True(pos[0] + size[0] <= 512 * scale + 0.5, "a small window keeps the title inside the shown sheet width");
		ctx.True(pos[1] + size[1] <= 724 * scale + 0.5, "a small window keeps the title inside the shown sheet height");
		ctx.Equal(63, font, "the font shrinks with the sheet");

		// Garbage scale falls back to 1080p instead of a zero-size title.
		font = LF_SignComponent.FaceTitleRect(0, pos, size);
		ctx.Equal(96, font, "a zero DPI scale falls back to the layout size");
	}
}
