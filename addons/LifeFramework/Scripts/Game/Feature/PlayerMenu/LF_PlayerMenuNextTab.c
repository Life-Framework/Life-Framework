// LF_PlayerMenuNextTab.c: the personal file's third tab, "What to do next" (LF_PlayerMenuSheet). The slot
// is wired (tab key, page, focus, the harness's `playermenu open next`) but empty: the tab key shows only
// when HasContent says there is something to say, so until the next-steps feature fills this file the
// file has two tabs. The feature that fills it edits only this file: HasContent reads the answer it
// needs, Build appends kit parts between the file's header and its footer (columns and cards as the
// other tabs build them: LF_UIPart.CreateColumns(...).Cards(), parts InColumn(card, 0)).

class LF_PlayerMenuNextTab
{
	//------------------------------------------------------------------------------------------------
	//! \return True when the tab has something to show for this answer. Empty slot: never.
	static bool HasContent(LF_PlayerMenuData data)
	{
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Appends the tab's parts. Called only when HasContent is true.
	static void Build(notnull array<ref LF_UIPart> parts, LF_PlayerMenuData data)
	{
	}
}
