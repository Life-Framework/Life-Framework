// red-proof: expect the first part name to be "TitleX" instead of "Title"
// (ui-kit/parts), expect a split of 5 taking 2 to keep 9 (ui-kit/quantity),
// or expect a dead character with the death rule armed to stay (ui-kit/close).
// This change did not boot a server.

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

		ctx.Equal(9, parts.Count(), "sample lists every part");
		if (parts.Count() != 9)
			return;

		Expect(ctx, parts.Get(0), "Title", LF_EUIPartKind.TITLE, "#LF-UIKit_SampleTitle", 0);
		Expect(ctx, parts.Get(1), "Line", LF_EUIPartKind.LINE, "#LF-UIKit_SampleLine", 1);
		Expect(ctx, parts.Get(2), "Balance", LF_EUIPartKind.LINE, "#LF-UIKit_SampleBalance", 2);
		Expect(ctx, parts.Get(3), "Field", LF_EUIPartKind.FIELD, "#LF-UIKit_SampleField", 3);
		Expect(ctx, parts.Get(4), "Row", LF_EUIPartKind.ROW, "#LF-UIKit_SampleRow", 4);
		Expect(ctx, parts.Get(5), "Status", LF_EUIPartKind.STATUS, "#LF-UIKit_SampleStatusLabel", 5);
		Expect(ctx, parts.Get(6), "Quantity", LF_EUIPartKind.QUANTITY, "#LF-UIKit_SampleQuantity", 6);
		Expect(ctx, parts.Get(7), "Button", LF_EUIPartKind.BUTTON, "#LF-UIKit_SampleButton", 7);
		Expect(ctx, parts.Get(8), "Close", LF_EUIPartKind.CLOSE, "#LF-UIKit_SampleClose", 8);

		LF_UIPart status = parts.Get(5);
		ctx.EqualStr("#LF-UIKit_SampleStatus", status.GetStatus(), "status row carries its status");
		ctx.EqualStr("#LF-UIKit_SampleAction", status.GetAction(), "status row carries its one action");

		LF_UIPart quantity = parts.Get(6);
		ctx.Equal(5, quantity.GetTotal(), "quantity sample starts at 5");
		ctx.Equal(2, quantity.GetTake(), "quantity sample starts by taking 2");

		LF_UIPart balance = parts.Get(2);
		balance.SetText("400");
		ctx.EqualStr("400", balance.GetText(), "a line can change after it is built");

		string buttonLayout = LF_UIPart.LayoutFor(LF_EUIPartKind.BUTTON);
		string closeLayout = LF_UIPart.LayoutFor(LF_EUIPartKind.CLOSE);
		ctx.EqualStr(buttonLayout, closeLayout, "close uses the pause button part");
		ctx.False(buttonLayout.IsEmpty(), "button layout is set");

		string rowLayout = LF_UIPart.LayoutFor(LF_EUIPartKind.ROW);
		string statusLayout = LF_UIPart.LayoutFor(LF_EUIPartKind.STATUS);
		string quantityLayout = LF_UIPart.LayoutFor(LF_EUIPartKind.QUANTITY);
		ctx.False(rowLayout == statusLayout, "a status row is its own part");
		ctx.False(rowLayout == quantityLayout, "a quantity is its own part");
		ctx.False(statusLayout == quantityLayout, "status and quantity are different parts");
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

// tier: LOGIC
class EL_Test_UIKitQuantity : EL_Test
{
	override string GetName()
	{
		return "ui-kit/quantity";
	}

	override void Run(EL_TestContext ctx)
	{
		int keep = 0;
		int take = 0;

		ctx.True(LF_UIQuantity.Split(5, 2, keep, take), "a stack of 5 can split");
		ctx.Equal(3, keep, "taking 2 from 5 keeps 3");
		ctx.Equal(2, take, "taking 2 from 5 takes 2");

		ctx.True(LF_UIQuantity.Split(5, 0, keep, take), "a request below 1 still splits");
		ctx.Equal(1, take, "take clamps up to 1");
		ctx.Equal(4, keep, "clamped take of 1 keeps 4");

		ctx.True(LF_UIQuantity.Split(5, 9, keep, take), "a request above the stack still splits");
		ctx.Equal(4, take, "take clamps to total - 1");
		ctx.Equal(1, keep, "the last one stays");

		ctx.False(LF_UIQuantity.Split(1, 1, keep, take), "a stack of one cannot split");
		ctx.Equal(1, keep, "the single item stays");
		ctx.Equal(0, take, "nothing is taken from a stack of one");

		ctx.False(LF_UIQuantity.Split(0, 1, keep, take), "an empty stack cannot split");
		ctx.Equal(0, keep, "an empty stack keeps nothing");
		ctx.Equal(0, take, "an empty stack takes nothing");

		ctx.False(LF_UIQuantity.Split(-3, 1, keep, take), "a negative stack cannot split");
		ctx.Equal(0, keep, "a negative stack keeps nothing");
		ctx.Equal(0, take, "a negative stack takes nothing");
	}
}

// tier: LOGIC
class EL_Test_UIKitClose : EL_Test
{
	override string GetName()
	{
		return "ui-kit/close";
	}

	override void Run(EL_TestContext ctx)
	{
		ctx.Equal(LF_UIClosePolicy.STAY, LF_UIClosePolicy.Reason(false, false, false, true, false, true, 3, 10), "an unarmed sheet stays");
		ctx.Equal(LF_UIClosePolicy.DEATH, LF_UIClosePolicy.Reason(true, false, false, true, true, false, 0, 0), "death closes when that rule is armed");
		ctx.Equal(LF_UIClosePolicy.STAY, LF_UIClosePolicy.Reason(true, false, false, false, true, false, 0, 0), "a living character does not trip the death rule");
		ctx.Equal(LF_UIClosePolicy.LOST, LF_UIClosePolicy.Reason(false, true, false, false, false, false, 0, 0), "losing the character closes when that rule is armed");
		ctx.Equal(LF_UIClosePolicy.STAY, LF_UIClosePolicy.Reason(false, true, false, false, true, false, 0, 0), "a controlled character does not trip the lost rule");
		ctx.Equal(LF_UIClosePolicy.LOST, LF_UIClosePolicy.Reason(false, false, true, false, false, false, 3, 0), "distance cannot be measured without a character");
		ctx.Equal(LF_UIClosePolicy.ANCHOR, LF_UIClosePolicy.Reason(false, false, true, false, true, true, 3, 0), "a missing anchor closes the sheet");
		ctx.Equal(LF_UIClosePolicy.DISTANCE, LF_UIClosePolicy.Reason(false, false, true, false, true, false, 3, 4), "past the range closes the sheet");
		ctx.Equal(LF_UIClosePolicy.STAY, LF_UIClosePolicy.Reason(false, false, true, false, true, false, 3, 3), "standing on the range keeps the sheet");
		ctx.EqualStr("out of range", LF_UIClosePolicy.Name(LF_UIClosePolicy.DISTANCE), "distance has a log name");
	}
}

// tier: LOGIC
class EL_Test_UIKitFocus : EL_Test
{
	override string GetName()
	{
		return "ui-kit/focus";
	}

	override void Run(EL_TestContext ctx)
	{
		ctx.Equal(-1, LF_UIFocus.Step(0, 0, 1), "an empty sheet has nowhere to focus");
		ctx.Equal(0, LF_UIFocus.Step(4, -1, 1), "MenuDown from nowhere lands on the first control");
		ctx.Equal(3, LF_UIFocus.Step(4, -1, -1), "MenuUp from nowhere lands on the last control");
		ctx.Equal(3, LF_UIFocus.Step(4, 0, -1), "MenuUp on the first control wraps");
		ctx.Equal(0, LF_UIFocus.Step(4, 3, 1), "MenuDown on the last control wraps");
		ctx.Equal(2, LF_UIFocus.Step(4, 1, 1), "MenuDown steps forward");
		ctx.Equal(1, LF_UIFocus.Step(4, 2, -1), "MenuUp steps back");
	}
}
