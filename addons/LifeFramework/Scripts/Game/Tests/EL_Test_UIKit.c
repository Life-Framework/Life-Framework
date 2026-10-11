// red-proof: expect the first part name to be "TitleX" instead of "Title".
// The assertion "sample part 0 is Title" fails. This change did not boot a server.

// tier: LOGIC
class EL_Test_UIKitParts : EL_Test
{
	override string GetName()
	{
		return "ui-kit/parts";
	}

	override void Run(EL_TestContext ctx)
	{
		ref array<ref LF_UIPart> parts = LF_UISampleScreen.GetParts();
		ctx.NotNull(parts, "sample part list exists");
		if (!parts)
			return;

		ctx.Equal(6, parts.Count(), "sample lists every part");
		if (parts.Count() != 6)
			return;

		Expect(ctx, parts.Get(0), "Title", LF_EUIPartKind.TITLE, "#LF-UIKit_SampleTitle", 0);
		Expect(ctx, parts.Get(1), "Line", LF_EUIPartKind.LINE, "#LF-UIKit_SampleLine", 1);
		Expect(ctx, parts.Get(2), "Field", LF_EUIPartKind.FIELD, "#LF-UIKit_SampleField", 2);
		Expect(ctx, parts.Get(3), "Row", LF_EUIPartKind.ROW, "#LF-UIKit_SampleRow", 3);
		Expect(ctx, parts.Get(4), "Button", LF_EUIPartKind.BUTTON, "#LF-UIKit_SampleButton", 4);
		Expect(ctx, parts.Get(5), "Close", LF_EUIPartKind.CLOSE, "#LF-UIKit_SampleClose", 5);

		string buttonLayout = LF_UIPart.LayoutFor(LF_EUIPartKind.BUTTON);
		string closeLayout = LF_UIPart.LayoutFor(LF_EUIPartKind.CLOSE);
		ctx.EqualStr(buttonLayout, closeLayout, "close uses the pause button part");
		ctx.False(buttonLayout.IsEmpty(), "button layout is set");

		string titleLayout = LF_UIPart.LayoutFor(LF_EUIPartKind.TITLE);
		string lineLayout = LF_UIPart.LayoutFor(LF_EUIPartKind.LINE);
		string fieldLayout = LF_UIPart.LayoutFor(LF_EUIPartKind.FIELD);
		string rowLayout = LF_UIPart.LayoutFor(LF_EUIPartKind.ROW);
		ctx.False(titleLayout == lineLayout, "title and line are different parts");
		ctx.False(titleLayout == fieldLayout, "title and field are different parts");
		ctx.False(titleLayout == rowLayout, "title and row are different parts");
		ctx.False(titleLayout == buttonLayout, "title and button are different parts");
		ctx.False(lineLayout == fieldLayout, "line and field are different parts");
		ctx.False(lineLayout == rowLayout, "line and row are different parts");
		ctx.False(fieldLayout == rowLayout, "field and row are different parts");
	}

	protected void Expect(EL_TestContext ctx, LF_UIPart part, string name, LF_EUIPartKind kind, string text, int index)
	{
		ctx.NotNull(part, string.Format("sample part %1 exists", index));
		if (!part)
			return;

		ctx.EqualStr(name, part.GetName(), string.Format("sample part %1 is %2", index, name));
		ctx.Equal(kind, part.GetKind(), string.Format("sample part %1 has its kind", index));
		ctx.EqualStr(text, part.GetText(), string.Format("sample part %1 uses its key", index));
	}
}
