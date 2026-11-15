// LF_UIBack.c — the one Back path of every LF screen ("pressing back does not close dialogs").
//
// The vanilla menu-back action is MenuBack (Escape, Backspace, gamepad0:b in MenuContext, see the
// base inputCommon.conf). A screen hears it two ways and both end in LF_UIBack.Fresh(), so one
// press closes it once: an InputManager action listener on MenuBack (what vanilla menus use) and
// the per-frame poll in LF_UIScreen / LF_DialogueScene, kept as the backstop. Script methods
// cannot take func arguments, so each screen class calls AddActionListener itself with ACTION.
// A script never reads MenuBack on its own outside the kit: validate-ui-back-path fails one that does.
class LF_UIBack
{
	static const string ACTION = "MenuBack";
	//! Two reports of one physical press (listener and poll) land inside this window.
	static const int DEBOUNCE_MS = 150; // lf-fixed: input debounce, the same window as LF_UIMenuInput

	//! Backspace alone (keyboard:KC_BACK, chimeraInputCommon.conf). Backspace is also one of MenuBack's
	//! sources, so a press with a text field focused reports both and must delete text, not close.
	static const string BACKSPACE = "LF_UIBackspace";

	protected static int s_iLastMs = -100000;

	//------------------------------------------------------------------------------------------------
	//! True when this Back report is a Backspace in a focused text field: the field keeps it.
	static bool IsTyping(bool fieldFocused)
	{
		if (!fieldFocused || !GetGame())
			return false;

		InputManager input = GetGame().GetInputManager();
		return input && input.GetActionTriggered(BACKSPACE);
	}

	//------------------------------------------------------------------------------------------------
	//! True once per press: a second report inside DEBOUNCE_MS is refused.
	static bool Fresh()
	{
		int now = System.GetTickCount();
		if (now - s_iLastMs < DEBOUNCE_MS)
			return false;

		s_iLastMs = now;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Forgets the last press (a test, or a harness that presses twice on purpose).
	static void Reset()
	{
		s_iLastMs = -100000;
	}
}
