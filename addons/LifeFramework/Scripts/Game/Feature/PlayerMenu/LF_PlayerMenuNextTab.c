// LF_PlayerMenuNextTab.c: the personal file's third tab, "What to do next" (LF_PlayerMenuSheet). Filled by the
// activities feature (Feature/Activities, LF_ActivityPage): the list of activities the player can start, asked
// from the server when the tab opens, with START / GUIDE ME. The tab always shows: before the answer arrives
// the page prints a waiting line, and with nothing to start it says so.

class LF_PlayerMenuNextTab
{
	//------------------------------------------------------------------------------------------------
	//! \return True: the tab shows for everyone (the activity list has its own waiting and empty lines).
	static bool HasContent(LF_PlayerMenuData data)
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Appends the tab's parts between the file's header and footer.
	static void Build(notnull array<ref LF_UIPart> parts, LF_PlayerMenuData data)
	{
		LF_ActivityPage.AddParts(parts);
	}
}
