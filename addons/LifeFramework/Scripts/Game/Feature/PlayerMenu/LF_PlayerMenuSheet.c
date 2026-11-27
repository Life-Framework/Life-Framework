// LF_PlayerMenuSheet.c: the personal file, the F2 key's sheet (gamepad: hold RB + Y). Client only.
// Built from UI kit parts (LF_UIScreen), so it takes the 1989 paper theme: no new screen .layout,
// no colours or fonts here (docs/style-guide.md "Screens"). A wide sheet (80% of the screen) with a
// header line (name, role badge, time on the island) and two tabs, each a page of the same file:
// PERSON in three columns (identity and licences; finances and status as ledger lines; every job
// skill with its level and XP bar, the picked one's record under the list) and PROGRESS in two (the
// first errands in chain order, then any other errand taken, each marked DONE, CURRENT or LATER; the
// picked errand's record and "Set waypoint" for the current errand). Everything printed comes from
// the server's answer (LF_PlayerMenuController, LF_PlayerMenuData); the sheet changes nothing. The
// footer's key-cap chips close it and open the welcome guide and the game settings. The vanilla
// hint card (the arrival note) is put away while the file is open and comes back when it closes.

class LF_PlayerMenuSheet
{
	static const string PART_TITLE = "PlayerMenuTitle";
	static const string PART_TABS = "PlayerMenuTabs";
	static const string PART_TAB_PERSON = "PlayerMenuPersonButton";
	static const string PART_TAB_PROGRESS = "PlayerMenuProgressButton";
	static const string PART_HEADER = "PlayerMenuHeader";
	static const string PART_IDENTITY = "PlayerMenuIdentity";
	static const string PART_COLUMNS = "PlayerMenuColumns";
	static const string PART_NAME = "PlayerMenuName";
	static const string PART_ROLE = "PlayerMenuRole";
	static const string PART_LICENCE = "PlayerMenuLicence_";
	static const string PART_CASH = "PlayerMenuCash";
	static const string PART_BANK = "PlayerMenuBank";
	static const string PART_HOUSES = "PlayerMenuHouses";
	static const string PART_GARAGE = "PlayerMenuGarage";
	static const string PART_WANTED = "PlayerMenuWanted";
	static const string PART_TAB_NEXT = "PlayerMenuNextButton";
	static const string PART_TAB_GANG = "PlayerMenuGangButton";
	static const string PART_TAB_FILE = "PlayerMenuFileButton";
	static const string PART_PORTRAIT = "PlayerMenuPortrait";
	static const string PART_JOB = "PlayerMenuJob";
	static const string PART_STANDING = "PlayerMenuStanding_";
	static const string PART_PLAYTIME = "PlayerMenuPlaytime";
	static const string PART_DAYS = "PlayerMenuDays";
	static const string PART_LICENCES = "PlayerMenuLicences";
	//! Up to this many licences are named in the row's value.
	static const int LICENCES_INLINE = 2;
	static const string PART_UNLOCK = "PlayerMenuDetailUnlock";
	static const string PART_PERKS = "PlayerMenuDetailPerks";
	static const string PART_RANK = "PlayerMenuDetailRank";
	//! Inner card columns, one per card.
	static const string CARD_ID = "PlayerMenuCardId";
	static const string CARD_MONEY = "PlayerMenuCardMoney";
	static const string CARD_STATUS = "PlayerMenuCardStatus";
	static const string CARD_SKILLS = "PlayerMenuCardSkills";
	static const string CARD_ERRANDS = "PlayerMenuCardErrands";
	static const string CARD_ERRAND = "PlayerMenuCardErrand";
	//! The character preview's window (reference pixels): the whole figure, standing.
	static const float PORTRAIT_W = 230;
	static const float PORTRAIT_H = 300;
	//! How often a hint that pops up while the file is open is put away again.
	static const int HINT_CHECK_MS = 400;	// lf-fixed: a UI poll that keeps a hint put away, not a gameplay tunable
	//! The sheet's share of the screen's width, and its narrowest (reference pixels).
	static const float SHEET_SHARE = 0.8;
	static const float SHEET_MIN_WIDTH = 1100;
	//! Every skill row shows up to this many; more scroll.
	static const int SKILL_ROWS_SHOWN = 10;
	//! Errand rows (group headings included) shown before the list scrolls.
	static const int QUEST_ROWS_SHOWN = 12;
	static const string PART_LIST = "PlayerMenuList";
	static const string PART_SUMMARY = "PlayerMenuSummary";
	static const string PART_DETAIL_NAME = "PlayerMenuDetailName";
	static const string PART_DETAIL_TEXT = "PlayerMenuDetailText";
	static const string PART_DETAIL_BAR = "PlayerMenuDetailBar";
	static const string PART_DETAIL_NUMBERS = "PlayerMenuDetailNumbers";
	static const string PART_WAYPOINT = "PlayerMenuWaypointButton";
	static const string PART_FOOTER = "PlayerMenuFooter";
	static const string PART_GUIDE = "PlayerMenuGuideButton";
	static const string PART_SETTINGS = "PlayerMenuSettingsButton";
	static const string PART_CLOSE = "PlayerMenuCloseButton";
	static const string ROW_SKILL = "PlayerMenuSkill_";
	static const string ROW_QUEST = "PlayerMenuQuest_";
	static const string ROW_GROUP = "PlayerMenuGroup_";

	static const int TAB_PERSON = 0;
	static const int TAB_PROGRESS = 1;
	//! "What to do next": shown only when LF_PlayerMenuNextTab has something to say.
	static const int TAB_NEXT = 2;
	//! "Gang": shown only to a player in a gang (LF_PlayerMenuGangTab.HasTabLocal).
	static const int TAB_GANG = 3;
	//! "My file": tickets, bookings and seized licences (LF_PlayerMenuFileTab). Shown to everyone.
	static const int TAB_FILE = 4;

	protected static ref LF_UIScreen s_Screen;
	//! The character drawn in the preview (a dummy in the preview world), cleared when the file closes.
	protected static ref LF_WantedPortrait s_Portrait;
	//! The HUD was showing when the file opened: it comes back when the file closes.
	protected static bool s_bHudHeld;
	protected static ref LF_PlayerMenuSheet s_Sheet;
	//! The last answer, so a reopen prints at once while the fresh ask is on its way.
	protected static ref LF_PlayerMenuData s_Data;
	protected static string s_sWire;
	protected static int s_iTab;
	protected static int s_iSkill = -1;
	protected static int s_iQuest = -1;
	//! The hint card was showing when the file opened: it is put back when the file closes.
	protected static bool s_bHintHeld;
	//! A tab change closes the old sheet; that close does not put the hint back.
	protected static bool s_bSwapping;

	protected LF_UIScreen m_Subscribed;

	//------------------------------------------------------------------------------------------------
	static LF_UIScreen Current()
	{
		return s_Screen;
	}

	//------------------------------------------------------------------------------------------------
	static LF_PlayerMenuData Data()
	{
		return s_Data;
	}

	//------------------------------------------------------------------------------------------------
	static int CurrentTab()
	{
		return s_iTab;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsOpen()
	{
		return s_Screen && s_Screen.IsOpen();
	}

	//------------------------------------------------------------------------------------------------
	//! The key: open the file, or put it away when it is open.
	static void Toggle()
	{
		if (IsOpen())
		{
			Hide();
			return;
		}

		string why = WhyNot();
		if (!why.IsEmpty())
		{
			LF_Debug.Info(LF_PlayerMenuServer.FEATURE, "file not opened: " + why);
			return;
		}

		Open(s_iTab);
	}

	//------------------------------------------------------------------------------------------------
	//! \return Why the key does nothing now, or empty when the file may open.
	protected static string WhyNot()
	{
		if (!GetGame() || !GetGame().GetPlayerController())
			return "no player controller";

		IEntity body = GetGame().GetPlayerController().GetControlledEntity();
		if (!body)
			return "no body";

		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(body);
		SCR_CharacterControllerComponent control;
		if (character)
			control = SCR_CharacterControllerComponent.Cast(character.GetCharacterController());

		if (control && control.GetLifeState() != ECharacterLifeState.ALIVE)
			return "the character is down";

		if (GetGame().GetMenuManager() && GetGame().GetMenuManager().IsAnyMenuOpen())
			return "a menu is open";

		if (LF_UIScreen.IsActionPromptSuppressed())
			return "another sheet is open";

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! Opens the file on a tab and asks the server for a fresh copy.
	static LF_UIScreen Open(int tab = 0)
	{
		Hide();
		s_iTab = TAB_PERSON;
		if (tab == TAB_PROGRESS || tab == TAB_NEXT || tab == TAB_GANG || tab == TAB_FILE)
			s_iTab = tab;

		s_Sheet = new LF_PlayerMenuSheet();
		LF_UIScreen screen = s_Sheet.Show();
		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (controller)
			controller.LF_AskPlayerMenu();
		else
			LF_Debug.Warn(LF_PlayerMenuServer.FEATURE, "file not asked: no player controller");

		if (s_iTab == TAB_NEXT)
			LF_ActivityPage.Ask();
		else if (s_iTab == TAB_GANG)
			AskGang();
		else if (s_iTab == TAB_FILE)
			AskFile();

		return screen;
	}

	//------------------------------------------------------------------------------------------------
	//! The server's answer. A changed file is printed again on the tab that is open.
	static void OnData(string wire)
	{
		LF_PlayerMenuData data = LF_PlayerMenuData.Decode(wire);
		if (!data)
		{
			LF_Debug.Error(LF_PlayerMenuServer.FEATURE, "file answer did not decode: " + wire.Length().ToString() + " chars");
			return;
		}

		bool changed = wire != s_sWire;
		s_sWire = wire;
		s_Data = data;
		LF_WelcomeGuideSheet.OnFileData();
		LF_Debug.Info(LF_PlayerMenuServer.FEATURE, string.Format("file received: %1 skills, %2 quests, %3 licences", data.m_aSkills.Count(), data.m_aQuests.Count(), data.m_aLicenceKeys.Count()));
		if (changed && IsOpen() && s_Sheet)
			GetGame().GetCallqueue().CallLater(s_Sheet.ShowLater, 0, false);
	}

	//------------------------------------------------------------------------------------------------
	static void Hide()
	{
		s_Sheet = null;
		if (s_Screen)
		{
			LF_UIScreen screen = s_Screen;
			s_Screen = null;
			screen.Close();
		}

		ReleaseHint();
	}

	//------------------------------------------------------------------------------------------------
	//! Puts the HUD and the vanilla hint card (the arrival note) away while the file is open.
	protected static void HoldHint()
	{
		SCR_HUDManagerComponent hud = SCR_HUDManagerComponent.GetHUDManager();
		if (!s_bHudHeld && hud && hud.IsVisible())
		{
			s_bHudHeld = true;
			hud.SetVisible(false);
		}

		// The notification toasts (the cards top right) wait hidden until the file closes.
		LF_NotificationHold.Hold(true);

		// A hint shown while the file is open (the arrival note comes back on its own) is put away too.
		if (GetGame().GetCallqueue())
		{
			GetGame().GetCallqueue().Remove(KeepHintDown);
			GetGame().GetCallqueue().CallLater(KeepHintDown, HINT_CHECK_MS, true);
		}

		KeepHintDown();
	}

	//------------------------------------------------------------------------------------------------
	protected static void KeepHintDown()
	{
		if (!IsOpen())
			return;

		SCR_HintManagerComponent hints = SCR_HintManagerComponent.GetInstance();
		if (!hints || !hints.IsShown())
			return;

		s_bHintHeld = true;
		hints.Hide();
		LF_Debug.Info(LF_PlayerMenuServer.FEATURE, "hint card put away while the file is open");
	}

	//------------------------------------------------------------------------------------------------
	//! Brings the HUD and the hint card (silently) back, and lets the preview figure go, when the file closes.
	protected static void ReleaseHint()
	{
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(KeepHintDown);

		LF_NotificationHold.Hold(false);

		if (s_Portrait)
			s_Portrait.Clear();

		s_Portrait = null;
		if (s_bHudHeld)
		{
			s_bHudHeld = false;
			SCR_HUDManagerComponent hud = SCR_HUDManagerComponent.GetHUDManager();
			if (hud)
				hud.SetVisible(true);
		}

		if (!s_bHintHeld)
			return;

		s_bHintHeld = false;
		SCR_HintManagerComponent.RepeatHint(true);
	}

	//------------------------------------------------------------------------------------------------
	//! The sheet went away by itself (Esc, death): the HUD and the hint card come back.
	protected void OnClosed()
	{
		if (s_bSwapping)
			return;

		ReleaseHint();
	}

	//------------------------------------------------------------------------------------------------
	//! Draws the local character, as dressed now, in the PERSON tab's preview window.
	protected static void DrawPortrait(notnull LF_UIScreen screen)
	{
		ItemPreviewWidget photo = screen.GetPortrait(PART_PORTRAIT);
		if (!photo)
			return;

		if (s_Portrait)
			s_Portrait.Clear();

		s_Portrait = new LF_WantedPortrait();
		string look = LF_WantedLook.Capture(LF_Utils.GetLocalCharacter());
		int dressed = s_Portrait.Show(photo, look);
		if (dressed < 0)
			LF_Debug.Warn(LF_PlayerMenuServer.FEATURE, "character preview not drawn: no look of the local character");
	}

	//------------------------------------------------------------------------------------------------
	protected LF_UIScreen Show()
	{
		if (s_Screen)
		{
			s_bSwapping = true;
			s_Screen.Close();
			s_bSwapping = false;
			s_Screen = null;
		}

		// A tab with nothing to show (the next-steps tab before its feature lands) falls back to PERSON.
		if (s_iTab == TAB_NEXT && !LF_PlayerMenuNextTab.HasContent(s_Data))
			s_iTab = TAB_PERSON;

		// The Gang tab shows only for a player in a gang; one who left it while the file was open falls back too.
		if (s_iTab == TAB_GANG && !LF_PlayerMenuGangTab.HasTabLocal())
			s_iTab = TAB_PERSON;

		LF_UISkin skin = LF_UISkin.Dashboard(SHEET_SHARE, SHEET_MIN_WIDTH);
		array<ref LF_UIPart> parts;
		if (s_iTab == TAB_PROGRESS)
			parts = BuildProgress(s_Data);
		else if (s_iTab == TAB_NEXT)
			parts = BuildNext(s_Data);
		else if (s_iTab == TAB_GANG)
			parts = BuildGang(s_Data);
		else if (s_iTab == TAB_FILE)
			parts = BuildFile(s_Data);
		else
			parts = BuildPerson(s_Data);

		LF_UIScreen screen = LF_UIScreen.Open(parts, skin);
		if (!screen)
		{
			LF_Debug.Error(LF_PlayerMenuServer.FEATURE, "file sheet failed to open");
			return null;
		}

		s_Screen = screen;
		if (m_Subscribed)
		{
			m_Subscribed.GetOnButton().Remove(OnButton);
			m_Subscribed.GetOnClosed().Remove(OnClosed);
		}

		m_Subscribed = screen;
		screen.GetOnButton().Insert(OnButton);
		screen.GetOnClosed().Insert(OnClosed);
		screen.CloseOnDeath();
		screen.CloseWhenUncontrolled();
		HoldHint();
		if (s_Data && s_iTab != TAB_NEXT && s_iTab != TAB_GANG && s_iTab != TAB_FILE)
			screen.SetListHeight(PART_LIST, ListHeight(s_Data, s_iTab));

		if (s_Data && s_iTab == TAB_PERSON)
			DrawPortrait(screen);

		if (s_iTab == TAB_NEXT)
			LF_ActivityPage.Fill(screen);
		else if (s_iTab != TAB_GANG && s_iTab != TAB_FILE)
			FillList(screen, s_Data);

		// Focus starts on the open tab's key, so a pad user sees where they are.
		string openTab = PART_TAB_PERSON;
		if (s_iTab == TAB_PROGRESS)
			openTab = PART_TAB_PROGRESS;
		else if (s_iTab == TAB_NEXT)
			openTab = PART_TAB_NEXT;
		else if (s_iTab == TAB_GANG)
			openTab = PART_TAB_GANG;
		else if (s_iTab == TAB_FILE)
			openTab = PART_TAB_FILE;

		screen.FocusPart(openTab);
		LF_Debug.Info(LF_PlayerMenuServer.FEATURE, "file open on tab " + s_iTab.ToString());
		return screen;
	}

	//------------------------------------------------------------------------------------------------
	protected void ShowLater()
	{
		if (s_Sheet == this && IsOpen())
			Show();
	}

	//------------------------------------------------------------------------------------------------
	//! The header bar (the big name, the role badge, the days and the time on the island) and the tabs.
	protected static array<ref LF_UIPart> Header(int tab, LF_PlayerMenuData data)
	{
		array<ref LF_UIPart> parts = {};
		if (data)
		{
			string subtitle = LF_Terms.Text("#LF-PlayerMenu_Subtitle", DaysOnIsland(data.m_iPlaytimeSeconds, DayMinutes()).ToString(), PlaytimeText(data.m_iPlaytimeSeconds));
			LF_UIPart header = LF_UIPart.CreatePair(PART_HEADER, NameText(data), subtitle, RoleText(data));
			parts.Insert(header.Compact().Tone(RoleTone(data)).WithIcon(RoleIcon(data)));
		}
		else
		{
			parts.Insert(LF_UIPart.Create(LF_EUIPartKind.TITLE, PART_TITLE, "#LF-PlayerMenu_Key"));
		}

		// The open tab is printed between dots. The next-steps tab shows only when it has something to say.
		array<string> names = {PART_TAB_PERSON, PART_TAB_PROGRESS};
		array<string> keys = {"#LF-PlayerMenu_TabPerson", "#LF-PlayerMenu_TabProgress"};
		array<int> tabs = {TAB_PERSON, TAB_PROGRESS};
		if (LF_PlayerMenuNextTab.HasContent(data))
		{
			names.Insert(PART_TAB_NEXT);
			keys.Insert("#LF-PlayerMenu_TabNext");
			tabs.Insert(TAB_NEXT);
		}

		// The Gang key shows only for a player in a gang, read from their own body so it is right before any answer.
		if (LF_PlayerMenuGangTab.HasTabLocal())
		{
			names.Insert(PART_TAB_GANG);
			keys.Insert("#LF-PlayerMenu_TabGang");
			tabs.Insert(TAB_GANG);
		}

		// "My file" is for everyone: it ends the row.
		names.Insert(PART_TAB_FILE);
		keys.Insert("#LF-PlayerMenu_TabFile");
		tabs.Insert(TAB_FILE);

		array<string> texts = {};
		foreach (int i, string key : keys)
		{
			string text = LF_Terms.Text(key);
			if (tabs[i] == tab)
				text = LF_Terms.Text("#LF-PlayerMenu_TabOpen", text);

			texts.Insert(text);
		}

		parts.Insert(LF_UIPart.CreateKeys(PART_TABS, names, texts));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.RULE, "PlayerMenuRuleTop", string.Empty));
		return parts;
	}

	//------------------------------------------------------------------------------------------------
	//! The activities answer arrived (LF_ActivityPage): print the tab again when it is the one open.
	static void OnActivities(bool changed)
	{
		if (changed && IsOpen() && s_Sheet && s_iTab == TAB_NEXT)
			GetGame().GetCallqueue().CallLater(s_Sheet.ShowLater, 0, false);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: asks the server for the crew facts (the Gang tab opened; the last answer prints meanwhile).
	protected static void AskGang()
	{
		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (controller)
			controller.LF_AskPlayerMenuGang();
		else
			LF_Debug.Warn(LF_PlayerMenuServer.FEATURE, "gang tab not asked: no player controller");
	}

	//------------------------------------------------------------------------------------------------
	//! The server's crew answer (LF_PlayerMenuGangTabWire). A changed answer prints the Gang tab again when it is open.
	static void OnGangData(string wire)
	{
		bool changed = LF_PlayerMenuGangView.Apply(wire);
		if (changed && IsOpen() && s_Sheet && s_iTab == TAB_GANG)
			GetGame().GetCallqueue().CallLater(s_Sheet.ShowLater, 0, false);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: asks the server for the player's file (the My file tab opened; the last answer prints meanwhile).
	protected static void AskFile()
	{
		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (controller)
			controller.LF_AskPlayerMenuFile();
		else
			LF_Debug.Warn(LF_PlayerMenuServer.FEATURE, "file tab not asked: no player controller");
	}

	//------------------------------------------------------------------------------------------------
	//! The server's file answer (LF_PlayerMenuFileTabWire). A changed answer prints the My file tab again when it is open.
	static void OnFileData(string wire)
	{
		bool changed = LF_PlayerMenuFileView.Apply(wire);
		if (changed && IsOpen() && s_Sheet && s_iTab == TAB_FILE)
			GetGame().GetCallqueue().CallLater(s_Sheet.ShowLater, 0, false);
	}

	//------------------------------------------------------------------------------------------------
	//! The My file page: LF_PlayerMenuFileTab fills it between the header and the footer.
	static array<ref LF_UIPart> BuildFile(LF_PlayerMenuData data)
	{
		array<ref LF_UIPart> parts = Header(TAB_FILE, data);
		LF_PlayerMenuFileTab.Build(parts, LF_PlayerMenuFileView.Data());
		Footer(parts);
		return parts;
	}

	//------------------------------------------------------------------------------------------------
	//! The Gang page: LF_PlayerMenuGangTab fills it between the header and the footer.
	static array<ref LF_UIPart> BuildGang(LF_PlayerMenuData data)
	{
		array<ref LF_UIPart> parts = Header(TAB_GANG, data);
		LF_PlayerMenuGangTab.Build(parts, LF_PlayerMenuGangView.Data());
		Footer(parts);
		return parts;
	}

	//------------------------------------------------------------------------------------------------
	//! The "What to do next" page: LF_PlayerMenuNextTab fills it between the header and the footer.
	static array<ref LF_UIPart> BuildNext(LF_PlayerMenuData data)
	{
		array<ref LF_UIPart> parts = Header(TAB_NEXT, data);
		LF_PlayerMenuNextTab.Build(parts, data);
		Footer(parts);
		return parts;
	}

	//------------------------------------------------------------------------------------------------
	//! The footer bar: each key with its key-cap chip (Esc closes, F3 is the welcome guide's key).
	protected static void Footer(notnull array<ref LF_UIPart> parts)
	{
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.RULE, "PlayerMenuRuleBottom", string.Empty));
		array<string> names = {PART_CLOSE, PART_GUIDE, PART_SETTINGS};
		array<string> texts = {"#LF-PlayerMenu_Close", "#LF-PlayerMenu_Guide", "#LF-PlayerMenu_Settings"};
		array<string> caps = {LF_Terms.Text("#LF-PlayerMenu_CapClose"), LF_Terms.Text("#LF-PlayerMenu_CapGuide"), string.Empty};
		parts.Insert(LF_UIPart.CreateKeys(PART_FOOTER, names, texts).WithCaps(caps));
	}

	//------------------------------------------------------------------------------------------------
	//! A part in column `index` of the file's columns.
	protected static LF_UIPart In(int index, LF_UIPart part)
	{
		return part.InColumn(PART_COLUMNS, index);
	}

	//------------------------------------------------------------------------------------------------
	//! A card in column `index`: its parts go in with Card(name, part).
	protected static LF_UIPart CardIn(int index, string name)
	{
		return In(index, LF_UIPart.CreateColumns(name, Weights(1)).Cards());
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_UIPart Card(string card, LF_UIPart part)
	{
		return part.InColumn(card, 0);
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_UIPart Heading(string card, string name, string key)
	{
		return Card(card, LF_UIPart.Create(LF_EUIPartKind.HEADING, name, key));
	}

	//------------------------------------------------------------------------------------------------
	//! How tall the open tab's list draws: every row up to the shown count, then it scrolls.
	static float ListHeight(notnull LF_PlayerMenuData data, int tab)
	{
		if (tab == TAB_PROGRESS)
		{
			int rows = data.m_aQuests.Count();
			int doneArrival;
			int doneOther;
			if (data.CountGroup(LF_EPlayerMenuQuestGroup.ARRIVAL, doneArrival) > 0)
				rows++;

			if (data.CountGroup(LF_EPlayerMenuQuestGroup.ERRANDS, doneOther) > 0)
				rows++;

			rows = LF_Utils.ClampInt(rows, 1, QUEST_ROWS_SHOWN);
			return rows * LF_UIScreen.COMPACT_ROW_PITCH + 8;
		}

		int skills = LF_Utils.ClampInt(data.m_aSkills.Count(), 1, SKILL_ROWS_SHOWN);
		return skills * (LF_UIScreen.COMPACT_ROW_PITCH + LF_UIScreen.COMPACT_BAR_PITCH) + 8;
	}

	//------------------------------------------------------------------------------------------------
	//! The PERSON page. Null data prints the waiting line in place of the figures.
	static array<ref LF_UIPart> BuildPerson(LF_PlayerMenuData data)
	{
		array<ref LF_UIPart> parts = Header(TAB_PERSON, data);
		if (!data)
		{
			parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, PART_IDENTITY, "#LF-PlayerMenu_Waiting"));
			Footer(parts);
			return parts;
		}

		parts.Insert(LF_UIPart.CreateColumns(PART_COLUMNS, Weights(1, 1, 1.3)));

		// 1: the character as they stand, their trade and how the island sees them.
		parts.Insert(CardIn(0, CARD_ID));
		parts.Insert(Heading(CARD_ID, "PlayerMenuHeadIdentity", "#LF-PlayerMenu_HeadIdentity"));
		parts.Insert(Card(CARD_ID, LF_UIPart.CreatePortrait(PART_PORTRAIT, string.Empty, string.Empty, new array<string>(), new array<string>()).Sized(PORTRAIT_W, PORTRAIT_H)));
		parts.Insert(Card(CARD_ID, LF_UIPart.CreatePair(PART_JOB, "#LF-PlayerMenu_Job", JobText()).WithIcon("job")));
		parts.Insert(Card(CARD_ID, LF_UIPart.CreatePair(PART_WANTED, "#LF-PlayerMenu_Wanted", WantedText(data.m_iWanted)).WithIcon("wanted")));
		array<string> labels = {};
		array<string> words = {};
		StandingRows(labels, words);
		foreach (int s, string label : labels)
		{
			parts.Insert(Card(CARD_ID, LF_UIPart.CreatePair(PART_STANDING + s.ToString(), label, words[s]).WithIcon("standing")));
		}

		// 2: the money, then the standing on paper: property, papers and time served on the island.
		parts.Insert(In(1, LF_UIPart.CreateColumns(CARD_MONEY, Weights(1)).Cards()));
		parts.Insert(Heading(CARD_MONEY, "PlayerMenuHeadMoney", "#LF-PlayerMenu_HeadMoney"));
		parts.Insert(Card(CARD_MONEY, LF_UIPart.CreatePair(PART_CASH, "#LF-PlayerMenu_Cash", LF_MoneyFormat.FormatMoney(data.m_iCash)).WithIcon("cash")));
		parts.Insert(Card(CARD_MONEY, LF_UIPart.CreatePair(PART_BANK, "#LF-PlayerMenu_Bank", LF_MoneyFormat.FormatMoney(data.m_iBank)).WithIcon("bank")));
		parts.Insert(In(1, LF_UIPart.CreateColumns(CARD_STATUS, Weights(1)).Cards()));
		parts.Insert(Heading(CARD_STATUS, "PlayerMenuHeadStatus", "#LF-PlayerMenu_HeadStatus"));
		parts.Insert(Card(CARD_STATUS, LF_UIPart.CreatePair(PART_HOUSES, "#LF-PlayerMenu_Houses", data.m_iHouses.ToString()).WithIcon("house")));
		string garage = LF_Terms.Text("#LF-PlayerMenu_GarageValue", data.m_iGarageUsed.ToString(), data.m_iGarageCap.ToString());
		parts.Insert(Card(CARD_STATUS, LF_UIPart.CreatePair(PART_GARAGE, "#LF-PlayerMenu_Garage", garage).WithIcon("garage")));
		parts.Insert(Card(CARD_STATUS, LF_UIPart.CreatePair(PART_DAYS, "#LF-PlayerMenu_Days", DaysOnIsland(data.m_iPlaytimeSeconds, DayMinutes()).ToString()).WithIcon("calendar")));
		parts.Insert(Card(CARD_STATUS, LF_UIPart.CreatePair(PART_PLAYTIME, "#LF-PlayerMenu_Playtime", PlaytimeText(data.m_iPlaytimeSeconds)).WithIcon("clock")));
		parts.Insert(Card(CARD_STATUS, LF_UIPart.CreatePair(PART_LICENCES, "#LF-PlayerMenu_HeadLicences", LicencesValue(data)).WithIcon("licence")));
		// Three or more do not fit the value: each gets its own row, named on the left (review B2).
		if (data.m_aLicenceKeys.Count() > LICENCES_INLINE)
		{
			foreach (int i, string licence : data.m_aLicenceKeys)
			{
				parts.Insert(Card(CARD_STATUS, LF_UIPart.CreatePair(PART_LICENCE + i.ToString(), LF_Terms.Text("#LF-PlayerMenu_LicenceItem", Printed(licence)), string.Empty)));
			}
		}

		// 3: every job skill with its level chip and XP bar; the picked one's record under the list.
		parts.Insert(CardIn(2, CARD_SKILLS));
		parts.Insert(Heading(CARD_SKILLS, "PlayerMenuHeadSkills", "#LF-PlayerMenu_HeadSkills"));
		parts.Insert(Card(CARD_SKILLS, LF_UIPart.CreateList(PART_LIST).Compact()));
		parts.Insert(Card(CARD_SKILLS, LF_UIPart.Create(LF_EUIPartKind.RULE, "PlayerMenuRuleDetail", string.Empty)));
		parts.Insert(Card(CARD_SKILLS, LF_UIPart.Create(LF_EUIPartKind.LINE, PART_DETAIL_NAME, "#LF-PlayerMenu_SkillHint")));
		parts.Insert(Card(CARD_SKILLS, LF_UIPart.Create(LF_EUIPartKind.LINE, PART_DETAIL_TEXT, string.Empty)));
		parts.Insert(Card(CARD_SKILLS, LF_UIPart.Create(LF_EUIPartKind.BAR, PART_DETAIL_BAR, string.Empty)));
		parts.Insert(Card(CARD_SKILLS, LF_UIPart.Create(LF_EUIPartKind.LINE, PART_DETAIL_NUMBERS, string.Empty)));
		parts.Insert(Card(CARD_SKILLS, LF_UIPart.Create(LF_EUIPartKind.LINE, PART_UNLOCK, string.Empty)));
		parts.Insert(Card(CARD_SKILLS, LF_UIPart.Create(LF_EUIPartKind.LINE, PART_PERKS, string.Empty)));
		parts.Insert(Card(CARD_SKILLS, LF_UIPart.Create(LF_EUIPartKind.LINE, PART_RANK, string.Empty)));
		Footer(parts);
		return parts;
	}

	//------------------------------------------------------------------------------------------------
	//! The PROGRESS page: the errand list on the left, the picked errand's record on the right.
	static array<ref LF_UIPart> BuildProgress(LF_PlayerMenuData data)
	{
		array<ref LF_UIPart> parts = Header(TAB_PROGRESS, data);
		if (!data)
		{
			parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, PART_SUMMARY, "#LF-PlayerMenu_Waiting"));
			Footer(parts);
			return parts;
		}

		parts.Insert(LF_UIPart.CreateColumns(PART_COLUMNS, Weights(1.1, 1)));
		parts.Insert(CardIn(0, CARD_ERRANDS));
		parts.Insert(Heading(CARD_ERRANDS, "PlayerMenuHeadErrands", "#LF-PlayerMenu_HeadErrands"));
		parts.Insert(Card(CARD_ERRANDS, LF_UIPart.CreatePair(PART_SUMMARY, "#LF-PlayerMenu_ErrandsDone", SummaryCount(data)).WithIcon("errand")));
		parts.Insert(Card(CARD_ERRANDS, LF_UIPart.CreateList(PART_LIST).Compact()));

		string hint = "#LF-PlayerMenu_QuestHint";
		if (data.m_aQuests.IsEmpty())
			hint = "#LF-PlayerMenu_NoQuests";

		parts.Insert(CardIn(1, CARD_ERRAND));
		parts.Insert(Heading(CARD_ERRAND, "PlayerMenuHeadErrand", "#LF-PlayerMenu_HeadErrand"));
		parts.Insert(Card(CARD_ERRAND, LF_UIPart.Create(LF_EUIPartKind.LINE, PART_DETAIL_NAME, hint)));
		parts.Insert(Card(CARD_ERRAND, LF_UIPart.Create(LF_EUIPartKind.RULE, "PlayerMenuRuleErrand", string.Empty)));
		parts.Insert(Card(CARD_ERRAND, LF_UIPart.Create(LF_EUIPartKind.LINE, PART_DETAIL_TEXT, string.Empty)));
		parts.Insert(Card(CARD_ERRAND, LF_UIPart.Create(LF_EUIPartKind.LINE, PART_DETAIL_NUMBERS, string.Empty)));
		if (CurrentArrival(data) >= 0)
			parts.Insert(Card(CARD_ERRAND, LF_UIPart.Create(LF_EUIPartKind.BUTTON, PART_WAYPOINT, "#LF-PlayerMenu_Waypoint")));

		Footer(parts);
		return parts;
	}

	//------------------------------------------------------------------------------------------------
	//! The list rows of the open tab: one per skill, or the quests under their group rows.
	protected static void FillList(notnull LF_UIScreen screen, LF_PlayerMenuData data)
	{
		if (!data)
			return;

		if (s_iTab == TAB_PERSON)
		{
			foreach (int i, LF_PlayerMenuSkill skill : data.m_aSkills)
			{
				float share = LF_PlayerMenuXp.Fraction(skill.m_iXp, skill.m_iFloor, skill.m_iNext);
				screen.AddListBarRow(PART_LIST, ROW_SKILL + i.ToString(), SkillName(skill.m_sJob), LF_Terms.Text("#LF-PlayerMenu_SkillLevel", skill.m_iLevel.ToString()), "#LF-PlayerMenu_Read", share);
			}

			if (s_iSkill >= 0 && s_iSkill < data.m_aSkills.Count())
				ShowSkill(screen, data, s_iSkill);

			return;
		}

		AddGroup(screen, data, LF_EPlayerMenuQuestGroup.ARRIVAL, "#LF-PlayerMenu_GroupArrival");
		AddGroup(screen, data, LF_EPlayerMenuQuestGroup.ERRANDS, "#LF-PlayerMenu_GroupErrands");
		// Nothing picked yet: the record shows the current errand, so the right column is never blank.
		int quest = s_iQuest;
		if (quest < 0 || quest >= data.m_aQuests.Count())
			quest = CurrentArrival(data);

		if (quest >= 0)
			ShowQuest(screen, data, quest);
	}

	//------------------------------------------------------------------------------------------------
	protected static void AddGroup(notnull LF_UIScreen screen, notnull LF_PlayerMenuData data, LF_EPlayerMenuQuestGroup group, string headingKey)
	{
		int done;
		int total = data.CountGroup(group, done);
		if (total == 0)
			return;

		screen.AddListRow(PART_LIST, ROW_GROUP + group.ToString(), headingKey, LF_Terms.Text("#LF-PlayerMenu_GroupCount", done.ToString(), total.ToString()), string.Empty);
		foreach (int i, LF_PlayerMenuQuest quest : data.m_aQuests)
		{
			if (quest.m_eGroup == group)
				screen.AddListRow(PART_LIST, ROW_QUEST + i.ToString(), QuestTitle(quest), StateKey(quest.m_eState), "#LF-PlayerMenu_Read");
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void OnButton(string name)
	{
		if (name == PART_CLOSE)
		{
			GetGame().GetCallqueue().CallLater(Hide, 0, false);
			return;
		}

		if (name == PART_TAB_PERSON || name == PART_TAB_PROGRESS || name == PART_TAB_NEXT || name == PART_TAB_GANG || name == PART_TAB_FILE)
		{
			int tab = TAB_PERSON;
			if (name == PART_TAB_PROGRESS)
				tab = TAB_PROGRESS;
			else if (name == PART_TAB_NEXT)
				tab = TAB_NEXT;
			else if (name == PART_TAB_GANG)
				tab = TAB_GANG;
			else if (name == PART_TAB_FILE)
				tab = TAB_FILE;

			if (tab == s_iTab)
				return;

			s_iTab = tab;
			if (tab == TAB_NEXT)
				LF_ActivityPage.Ask();
			else if (tab == TAB_GANG)
				AskGang();
			else if (tab == TAB_FILE)
				AskFile();

			// The press runs inside the old sheet; build the next one after it has finished.
			GetGame().GetCallqueue().CallLater(ShowLater, 0, false);
			return;
		}

		if (s_iTab == TAB_NEXT && LF_ActivityPage.OnButton(s_Screen, name))
			return;

		if (name.StartsWith(ROW_SKILL))
		{
			s_iSkill = name.Substring(ROW_SKILL.Length(), name.Length() - ROW_SKILL.Length()).ToInt();
			if (s_Data && s_Screen)
				ShowSkill(s_Screen, s_Data, s_iSkill);

			return;
		}

		if (name.StartsWith(ROW_QUEST))
		{
			s_iQuest = name.Substring(ROW_QUEST.Length(), name.Length() - ROW_QUEST.Length()).ToInt();
			if (s_Data && s_Screen)
				ShowQuest(s_Screen, s_Data, s_iQuest);

			return;
		}

		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (name == PART_WAYPOINT)
		{
			if (!controller)
			{
				LF_Debug.Warn(LF_PlayerMenuServer.FEATURE, "set waypoint refused: no player controller");
				return;
			}

			// The welcome guide's ask: the server marks the current arrival errand and toasts.
			controller.LF_AskWelcomeGuideWaypoint();
			GetGame().GetCallqueue().CallLater(Hide, 0, false);
			return;
		}

		if (name == PART_GUIDE)
		{
			GetGame().GetCallqueue().CallLater(OpenGuide, 0, false);
			return;
		}

		if (name == PART_SETTINGS)
		{
			GetGame().GetCallqueue().CallLater(OpenSettings, 0, false);
			return;
		}

		LF_Debug.Warn(LF_PlayerMenuServer.FEATURE, "file button " + name + " does nothing");
	}

	//------------------------------------------------------------------------------------------------
	protected static void OpenGuide()
	{
		Hide();
		LF_WelcomeGuideSheet.Open(0);
	}

	//------------------------------------------------------------------------------------------------
	protected static void OpenSettings()
	{
		Hide();
		if (!GetGame().GetMenuManager())
		{
			LF_Debug.Warn(LF_PlayerMenuServer.FEATURE, "settings not opened: no menu manager");
			return;
		}

		GetGame().GetMenuManager().OpenMenu(ChimeraMenuPreset.SettingsSuperMenu);
	}

	//------------------------------------------------------------------------------------------------
	//! Prints one skill under the list: its name, what it is, the bar, and the numbers.
	static void ShowSkill(notnull LF_UIScreen screen, notnull LF_PlayerMenuData data, int index)
	{
		if (index < 0 || index >= data.m_aSkills.Count())
			return;

		LF_PlayerMenuSkill skill = data.m_aSkills[index];
		string tradeName = SkillName(skill.m_sJob);
		screen.SetLine(PART_DETAIL_NAME, LF_PlayerMenuTrade.TitleLine(skill, tradeName));
		screen.SetLine(PART_DETAIL_TEXT, SkillDescription(skill.m_sJob));
		screen.SetBar(PART_DETAIL_BAR, LF_PlayerMenuXp.Fraction(skill.m_iXp, skill.m_iFloor, skill.m_iNext));
		screen.SetLine(PART_DETAIL_NUMBERS, SkillNumbers(skill));
		string unlock;
		if (skill.m_iNext >= 0)
		{
			unlock = LF_PlayerMenuTrade.UnlockLine(skill);
			if (unlock.IsEmpty())
				unlock = NextUnlock(skill.m_sJob, skill.m_iLevel + 1);
		}

		screen.SetLine(PART_UNLOCK, unlock);
		screen.SetLine(PART_PERKS, LF_PlayerMenuTrade.PerksLine(skill));
		screen.SetLine(PART_RANK, LF_PlayerMenuTrade.RankLine(skill, tradeName));
	}

	//------------------------------------------------------------------------------------------------
	static void ShowQuest(notnull LF_UIScreen screen, notnull LF_PlayerMenuData data, int index)
	{
		if (index < 0 || index >= data.m_aQuests.Count())
			return;

		LF_PlayerMenuQuest quest = data.m_aQuests[index];
		screen.SetLine(PART_DETAIL_NAME, QuestTitle(quest));
		string text = string.Empty;
		if (!quest.m_sDescKey.IsEmpty())
			text = LF_Terms.Text(quest.m_sDescKey);

		screen.SetLine(PART_DETAIL_TEXT, text);
		string numbers = LF_Terms.Text(StateKey(quest.m_eState));
		if (quest.m_iTarget > 0)
			numbers = LF_Terms.Text("#LF-PlayerMenu_QuestProgress", numbers, quest.m_iProgress.ToString(), quest.m_iTarget.ToString());

		screen.SetLine(PART_DETAIL_NUMBERS, numbers);
	}

	//------------------------------------------------------------------------------------------------
	//! \return The index of the CURRENT arrival errand, or -1.
	static int CurrentArrival(notnull LF_PlayerMenuData data)
	{
		foreach (int i, LF_PlayerMenuQuest quest : data.m_aQuests)
		{
			if (quest.m_eGroup == LF_EPlayerMenuQuestGroup.ARRIVAL && quest.m_eState == LF_EPlayerMenuQuestState.CURRENT)
				return i;
		}

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	//------------------------------------------------------------------------------------------------
	//! Column weights as an array (two or three columns; a negative weight is left out).
	protected static array<float> Weights(float a, float b = -1, float c = -1)
	{
		array<float> weights = {a};
		if (b > 0)
			weights.Insert(b);

		if (c > 0)
			weights.Insert(c);

		return weights;
	}

	//------------------------------------------------------------------------------------------------
	static string NameText(notnull LF_PlayerMenuData data)
	{
		if (data.m_sName.IsEmpty())
			return LF_Terms.Text("#LF-PlayerMenu_NoName");

		return data.m_sName;
	}

	//------------------------------------------------------------------------------------------------
	//! The role badge: Civilian, or the service and whether they are on duty.
	static string RoleText(notnull LF_PlayerMenuData data)
	{
		string role = LF_Terms.Text(RoleKey(data.m_eFaction));
		if (data.m_eFaction == LF_Faction.CIVILIAN)
			return role;

		string duty = "#LF-PlayerMenu_OffDuty";
		if (data.m_bOnDuty)
			duty = "#LF-PlayerMenu_OnDuty";

		return LF_Terms.Text("#LF-PlayerMenu_RoleDuty", role, LF_Terms.Text(duty));
	}

	//------------------------------------------------------------------------------------------------
	//! The badge's colour: police blue, medic red, a civilian's plain strip.
	static LF_EUIBadgeTone RoleTone(notnull LF_PlayerMenuData data)
	{
		if (data.m_eFaction == LF_Faction.POLICE)
			return LF_EUIBadgeTone.BLUE;

		if (data.m_eFaction == LF_Faction.MEDIC)
			return LF_EUIBadgeTone.RED;

		return LF_EUIBadgeTone.PLAIN;
	}

	//------------------------------------------------------------------------------------------------
	static string RoleIcon(notnull LF_PlayerMenuData data)
	{
		if (data.m_eFaction == LF_Faction.POLICE)
			return "police";

		if (data.m_eFaction == LF_Faction.MEDIC)
			return "medic";

		return "licence";
	}

	//------------------------------------------------------------------------------------------------
	//! The local character's current job, read from their own replicated job component.
	static string JobText()
	{
		SCR_ChimeraCharacter character = LF_Utils.GetLocalCharacter();
		LF_PlayerJobComponent job;
		if (character)
			job = LF_PlayerJobComponent.Cast(character.FindComponent(LF_PlayerJobComponent));

		if (!job || job.GetJobName().IsEmpty())
			return LF_Terms.Text("#LF-PlayerMenu_JobNone");

		return Printed(job.GetJobName());
	}

	//------------------------------------------------------------------------------------------------
	//! The island's word on the character, one row per faction, from the tiers the server synced to
	//! this client (the same words the ID card prints). Nothing yet: no rows.
	static void StandingRows(notnull array<string> labels, notnull array<string> words)
	{
		int town;
		int police;
		int underworld;
		if (!LF_ReputationSync.ClientTier(LF_Reputation.FACTION_TOWN, town))
			return;

		LF_ReputationSync.ClientTier(LF_Reputation.FACTION_POLICE, police);
		LF_ReputationSync.ClientTier(LF_Reputation.FACTION_UNDERWORLD, underworld);
		array<string> labelKeys = {};
		array<string> wordKeys = {};
		LF_ReputationWords.Pairs(LF_ReputationConfig.Get(), town, police, underworld, labelKeys, wordKeys);
		for (int i = 0; i < labelKeys.Count(); i++)
		{
			labels.Insert(Printed(labelKeys[i]));
			words.Insert(Printed(wordKeys[i]));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Real minutes one island day lasts on this server (the day cycle config), 24 h when unset.
	static int DayMinutes()
	{
		LF_DayCycleConfig config = LF_DayCycleConfig.Get();
		if (!config || config.m_iDayLengthMinutes < 1)
			return 1440;

		return config.m_iDayLengthMinutes;
	}

	//------------------------------------------------------------------------------------------------
	//! The island day the character is on: day 1 on arrival, one more for each island day played.
	static int DaysOnIsland(int playtimeSeconds, int dayMinutes)
	{
		if (dayMinutes < 1)
			dayMinutes = 1440;

		return 1 + LF_Utils.MaxInt(playtimeSeconds, 0) / (dayMinutes * 60);
	}

	//------------------------------------------------------------------------------------------------
	//! What the next level of `job` opens: the chains with a step gated at exactly that level.
	//! \return "Next level opens: Cider, Moonshine", or empty when nothing is gated there.
	static string NextUnlock(string job, int nextLevel)
	{
		array<LF_ChainDef> chains = {};
		LF_ChainCatalog.GetEnabled(chains);
		array<string> names = {};
		foreach (LF_ChainDef chain : chains)
		{
			if (!chain || !chain.m_aSteps)
				continue;

			foreach (LF_ChainStepDef step : chain.m_aSteps)
			{
				if (step.m_iMinLevel != nextLevel || step.m_sLevelJob != job)
					continue;

				string name = Printed(chain.m_sNameKey);
				if (names.Find(name) < 0)
					names.Insert(name);
			}
		}

		if (names.IsEmpty())
			return string.Empty;

		string list = string.Empty;
		foreach (string n : names)
		{
			if (!list.IsEmpty())
				list += ", ";

			list += n;
		}

		return LF_Terms.Text("#LF-PlayerMenu_SkillNext", list);
	}

	//------------------------------------------------------------------------------------------------
	static string SummaryCount(notnull LF_PlayerMenuData data)
	{
		int done;
		int total = data.CountGroup(LF_EPlayerMenuQuestGroup.ARRIVAL, done);
		int otherDone;
		int others = data.CountGroup(LF_EPlayerMenuQuestGroup.ERRANDS, otherDone);
		return LF_Terms.Text("#LF-PlayerMenu_GroupCount", (done + otherDone).ToString(), (total + others).ToString());
	}

	//------------------------------------------------------------------------------------------------
	static string RoleKey(LF_Faction faction)
	{
		if (faction == LF_Faction.POLICE)
			return "#LF-PlayerMenu_RolePolice";

		if (faction == LF_Faction.MEDIC)
			return "#LF-PlayerMenu_RoleMedic";

		return "#LF-PlayerMenu_RoleCivilian";
	}

	//------------------------------------------------------------------------------------------------
	static string PlaytimeText(int seconds)
	{
		int total = LF_Utils.MaxInt(seconds, 0);
		int hours = total / 3600;
		int minutes = (total % 3600) / 60;
		return LF_Terms.Text("#LF-PlayerMenu_PlaytimeValue", hours.ToString(), minutes.ToString());
	}

	//------------------------------------------------------------------------------------------------
	static string WantedText(int level)
	{
		if (level <= 0)
			return LF_Terms.Text("#LF-PlayerMenu_WantedNo");

		return LF_Terms.Text("#LF-PlayerMenu_WantedLevel", level.ToString());
	}

	//------------------------------------------------------------------------------------------------
	//! The licences row's value: "None", the names of one or two ("Field hand"), else how many.
	static string LicencesValue(notnull LF_PlayerMenuData data)
	{
		int count = data.m_aLicenceKeys.Count();
		if (count == 0)
			return LF_Terms.Text("#LF-PlayerMenu_LicencesNoneShort");

		if (count > LICENCES_INLINE)
			return LF_Terms.Text("#LF-PlayerMenu_LicencesHeld", count.ToString());

		string names = string.Empty;
		foreach (string key : data.m_aLicenceKeys)
		{
			if (!names.IsEmpty())
				names += ", ";

			names += Printed(key);
		}

		return names;
	}

	//------------------------------------------------------------------------------------------------
	//! A key's text, or the raw value when it is not a key.
	protected static string Printed(string key)
	{
		if (!key.StartsWith("#"))
			return key;

		return LF_Terms.Text(key);
	}

	//------------------------------------------------------------------------------------------------
	static string StateKey(LF_EPlayerMenuQuestState state)
	{
		if (state == LF_EPlayerMenuQuestState.DONE)
			return "#LF-PlayerMenu_StateDone";

		if (state == LF_EPlayerMenuQuestState.CURRENT)
			return "#LF-PlayerMenu_StateCurrent";

		return "#LF-PlayerMenu_StateLater";
	}

	//------------------------------------------------------------------------------------------------
	static string QuestTitle(notnull LF_PlayerMenuQuest quest)
	{
		if (quest.m_sTitleKey.IsEmpty())
			return quest.m_sId;

		return LF_Terms.Text(quest.m_sTitleKey);
	}

	//------------------------------------------------------------------------------------------------
	//! The string keys of a job track: its name and what it is. False for a job a pack added without rows.
	static bool SkillKeys(string job, out string nameKey, out string descKey)
	{
		if (job == "CIVILIAN")
		{
			nameKey = "#LF-PlayerMenu_Skill_CIVILIAN";
			descKey = "#LF-PlayerMenu_SkillDesc_CIVILIAN";
			return true;
		}

		if (job == "DELIVERY")
		{
			nameKey = "#LF-PlayerMenu_Skill_DELIVERY";
			descKey = "#LF-PlayerMenu_SkillDesc_DELIVERY";
			return true;
		}

		if (job == "FARMER")
		{
			nameKey = "#LF-PlayerMenu_Skill_FARMER";
			descKey = "#LF-PlayerMenu_SkillDesc_FARMER";
			return true;
		}

		if (job == "MINER")
		{
			nameKey = "#LF-PlayerMenu_Skill_MINER";
			descKey = "#LF-PlayerMenu_SkillDesc_MINER";
			return true;
		}

		if (job == "LUMBERJACK")
		{
			nameKey = "#LF-PlayerMenu_Skill_LUMBERJACK";
			descKey = "#LF-PlayerMenu_SkillDesc_LUMBERJACK";
			return true;
		}

		if (job == "FISHER")
		{
			nameKey = "#LF-PlayerMenu_Skill_FISHER";
			descKey = "#LF-PlayerMenu_SkillDesc_FISHER";
			return true;
		}

		if (job == "MEDIC")
		{
			nameKey = "#LF-PlayerMenu_Skill_MEDIC";
			descKey = "#LF-PlayerMenu_SkillDesc_MEDIC";
			return true;
		}

		if (job == "POLICE")
		{
			nameKey = "#LF-PlayerMenu_Skill_POLICE";
			descKey = "#LF-PlayerMenu_SkillDesc_POLICE";
			return true;
		}

		if (job == "SMELTING")
		{
			nameKey = "#LF-PlayerMenu_Skill_SMELTING";
			descKey = "#LF-PlayerMenu_SkillDesc_SMELTING";
			return true;
		}

		if (job == "SALVAGE")
		{
			nameKey = "#LF-PlayerMenu_Skill_SALVAGE";
			descKey = "#LF-PlayerMenu_SkillDesc_SALVAGE";
			return true;
		}

		nameKey = string.Empty;
		descKey = "#LF-PlayerMenu_SkillDescOther";
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! The job's printed name, or the id in plain case for a job without rows.
	static string SkillName(string job)
	{
		string nameKey;
		string descKey;
		if (!SkillKeys(job, nameKey, descKey))
			return PlainCase(job);

		return LF_Terms.Text(nameKey);
	}

	//------------------------------------------------------------------------------------------------
	static string SkillDescription(string job)
	{
		string nameKey;
		string descKey;
		SkillKeys(job, nameKey, descKey);
		return LF_Terms.Text(descKey);
	}

	//------------------------------------------------------------------------------------------------
	//! "FISHER" reads "Fisher", "FUEL_DELIVERY" reads "Fuel delivery".
	static string PlainCase(string id)
	{
		string lower = id;
		lower.ToLower();
		lower.Replace("_", " ");
		if (lower.IsEmpty())
			return lower;

		string first = lower.Get(0);
		first.ToUpper();
		return first + lower.Substring(1, lower.Length() - 1); // lf-bounded: a job id, a few characters
	}

	//------------------------------------------------------------------------------------------------
	static string SkillNumbers(notnull LF_PlayerMenuSkill skill)
	{
		if (skill.m_iNext < 0)
			return LF_Terms.Text("#LF-PlayerMenu_SkillTop", skill.m_iLevel.ToString(), skill.m_iXp.ToString());

		return LF_Terms.Text("#LF-PlayerMenu_SkillXp", skill.m_iLevel.ToString(), skill.m_iXp.ToString(), skill.m_iNext.ToString());
	}

	//------------------------------------------------------------------------------------------------
	void ~LF_PlayerMenuSheet()
	{
		if (m_Subscribed)
			m_Subscribed.GetOnButton().Remove(OnButton);
	}

	//------------------------------------------------------------------------------------------------
	//! World end: no sheet and no answer of the old world outlives it.
	static void ResetStatics()
	{
		Hide();
		s_Data = null;
		s_sWire = string.Empty;
		s_iTab = TAB_PERSON;
		s_iSkill = -1;
		s_iQuest = -1;
		s_bHintHeld = false;
		s_bHudHeld = false;
		s_bSwapping = false;
		s_Portrait = null;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_PlayerMenuSheet's statics (LF_StaticReset finds it by reflection).
class LF_PlayerMenuSheet_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_PlayerMenuSheet";
	}

	override void Apply()
	{
		LF_PlayerMenuSheet.ResetStatics();
	}
}
