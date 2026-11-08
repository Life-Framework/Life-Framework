// red-proof: observed 2026-11-07, tools\cli test --no-build --tier all, one assertion broken per
// test, then restored:
//   ui-kit/initial-focus  expected 0 for {field, button, button}:
//     "[ELTEST]   - focus skips a leading text field so MenuSelect works at once (expected 0, got 1)"
//   ui-kit/button-invoker  expected "Sell" after Notify("Buy"):
//     "[ELTEST]   - a pressed button reaches the feature by part name (expected 'Sell', got 'Buy')"
//   notifications/toast-timing  expected phase 0 at 5.1 s of 5:
//     "[ELTEST]   - a slip fades once its time is up (expected 0, got 1)"
//   notifications/toast-layout  expected a widget named ColorStripX:
//     "[ELTEST]   - toast has widget ColorStripX"
// SUMMARY of that run: tier=all passed=128 failed=4 total=132. The dedicated server has a
// workspace, so the layout test instantiated the toast and found the other widgets.

class LF_TestButtonListener : Managed
{
	string m_sHeard;
	int m_iHeardCount;
	int m_iClosedCount;

	void OnButton(string name)
	{
		m_sHeard = name;
		m_iHeardCount++;
	}

	void OnClosed()
	{
		m_iClosedCount++;
	}
}

// tier: LOGIC
class EL_Test_UIKitInitialFocus : EL_Test
{
	override string GetName()
	{
		return "ui-kit/initial-focus";
	}

	override void Run(EL_TestContext ctx)
	{
		array<bool> none = {};
		ctx.Equal(-1, LF_UIFocus.Initial(none), "an empty sheet focuses nothing");

		array<bool> fieldFirst = {true, false, false};
		ctx.Equal(1, LF_UIFocus.Initial(fieldFirst), "focus skips a leading text field so MenuSelect works at once");

		array<bool> buttonFirst = {false, true};
		ctx.Equal(0, LF_UIFocus.Initial(buttonFirst), "a leading button takes focus");

		array<bool> onlyFields = {true, true};
		ctx.Equal(0, LF_UIFocus.Initial(onlyFields), "a sheet of fields still focuses the first");

		ctx.EqualFloat(0, LF_UIFocus.ScrollFraction(0, 10), 0.001, "the first row scrolls to the top");
		ctx.EqualFloat(1, LF_UIFocus.ScrollFraction(9, 10), 0.001, "the last row scrolls to the bottom");
		ctx.EqualFloat(0.5, LF_UIFocus.ScrollFraction(2, 5), 0.001, "a middle row scrolls proportionally");
		ctx.EqualFloat(0, LF_UIFocus.ScrollFraction(0, 1), 0.001, "a single row never scrolls");

		array<int> keys = {0, 1000, 2000};
		ctx.Equal(2, LF_UIFocus.InsertIndex(keys, 1001), "a list row added late sits inside its list's span");
		ctx.Equal(3, LF_UIFocus.InsertIndex(keys, 2000), "an equal key goes after the existing entry");
		ctx.Equal(0, LF_UIFocus.InsertIndex(null, 5), "no keys inserts at the start");
	}
}

// tier: LOGIC
class EL_Test_UIKitButtonInvoker : EL_Test
{
	override string GetName()
	{
		return "ui-kit/button-invoker";
	}

	override void Run(EL_TestContext ctx)
	{
		LF_UIScreen screen = new LF_UIScreen();
		LF_TestButtonListener listener = new LF_TestButtonListener();
		screen.GetOnButton().Insert(listener.OnButton);
		screen.GetOnClosed().Insert(listener.OnClosed);

		screen.Notify("Buy");
		ctx.EqualStr("Buy", listener.m_sHeard, "a pressed button reaches the feature by part name");
		ctx.Equal(1, listener.m_iHeardCount, "one press is one callback");

		ctx.False(screen.IsOpen(), "a screen that never opened is not open");
		screen.Close();
		ctx.Equal(0, listener.m_iClosedCount, "closing a screen that never opened does not report a close");
	}
}

// tier: LOGIC
class EL_Test_NotificationToastTiming : EL_Test
{
	override string GetName()
	{
		return "notifications/toast-timing";
	}

	override void Run(EL_TestContext ctx)
	{
		ctx.Equal(0, EL_NotificationToastUI.Phase(0, 5), "a fresh slip shows");
		ctx.Equal(0, EL_NotificationToastUI.Phase(4.9, 5), "a slip shows until its time is up");
		ctx.Equal(1, EL_NotificationToastUI.Phase(5.1, 5), "a slip fades once its time is up");
		ctx.Equal(2, EL_NotificationToastUI.Phase(5 + EL_NotificationToastUI.FADE_SECONDS + 0.01, 5), "a slip is removed after the fade");
		// A 200 ms hitch must not stretch the slip: phase is a function of real elapsed time only.
		ctx.Equal(2, EL_NotificationToastUI.Phase(60, 5), "a long stall still removes the slip");
	}
}

// tier: WORLD
class EL_Test_NotificationToastLayout : EL_Test
{
	static const string TOAST_LAYOUT = "{4C38FD68D45325D2}UI/Layouts/Notifications/EL_NotificationToast.layout";

	override string GetName()
	{
		return "notifications/toast-layout";
	}

	override void Run(EL_TestContext ctx)
	{
		Resource res = Resource.Load(TOAST_LAYOUT);
		ctx.True(res != null && res.IsValid(), "the toast layout NotificationManager loads exists");
		if (!res || !res.IsValid())
			return;

		array<string> expected = {"Background", "ColorStrip", "Icon", "Title", "Message"};

		WorkspaceWidget workspace;
		if (GetGame())
			workspace = GetGame().GetWorkspace();

		// The dedicated server has a workspace (observed in the red run recorded above).
		ctx.NotNull(workspace, "a workspace exists to build the toast in");
		if (!workspace)
			return;

		Widget root = workspace.CreateWidgets(TOAST_LAYOUT);
		ctx.NotNull(root, "the toast layout instantiates");
		if (!root)
			return;

		foreach (string name : expected)
		{
			ctx.NotNull(root.FindAnyWidget(name), "toast has widget " + name);
		}

		ctx.NotNull(root.FindHandler(EL_NotificationToastUI), "toast root carries EL_NotificationToastUI");
		root.RemoveFromHierarchy();
	}
}
