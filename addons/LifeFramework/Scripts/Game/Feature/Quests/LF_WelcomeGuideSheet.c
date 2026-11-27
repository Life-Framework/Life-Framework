// LF_WelcomeGuideSheet.c: the welcome guide, a short paged sheet for a new player. Client only.
// Built from UI kit parts (LF_UIScreen), so it takes the 1989 theme from LF_UITheme: no new .layout,
// no colours or fonts here (docs/style-guide.md "Screens"). Five pages: first steps (with "Set
// waypoint", which asks the server to put the arrival errand on the map), money, the map, police and
// medics, more help (the manual link and the reopen key). Every text is a string key; the pack's table
// overrides the core rows with place names. It opens by itself when the server says the guide is
// pending (LF_WelcomeGuideController: at the first spawn and once at each join), only once the body is up and no other
// sheet or menu is open; the LF_WelcomeGuide key opens and closes it at any time.

class LF_WelcomeGuideSheet
{
	static const string PART_TITLE = "WelcomeGuideTitle";
	static const string PART_PAGE = "WelcomeGuidePage";
	static const string PART_HEADING = "WelcomeGuideHeading";
	static const string PART_BODY = "WelcomeGuideBody";
	static const string PART_NAV = "WelcomeGuideNav";
	static const string PART_PREV = "WelcomeGuidePrevButton";
	static const string PART_NEXT = "WelcomeGuideNextButton";
	static const string PART_WAYPOINT = "WelcomeGuideWaypointButton";
	static const string PART_DISMISS = "WelcomeGuideDismissButton";
	static const string PART_CLOSE = "WelcomeGuideCloseButton";
	static const float SHEET_WIDTH = 640;
	//! The first look after the server's "show": the bed wake runs first (LF_FirstSpawnBeds).
	static const int AUTO_DELAY_MS = 4000; // lf-fixed: UI pacing after the bed wake, not a server tunable
	static const int AUTO_RETRY_MS = 2000; // lf-fixed: UI poll step
	//! About two minutes of waiting for the body to stand and other sheets to close; then the next join.
	static const int AUTO_MAX_TRIES = 60; // lf-fixed: UI give-up; the next join tries again
	static const float TOAST_SECONDS = 6; // lf-fixed: toast length like the other sheets

	//! The paragraph that names the starting balance (%1 is filled from the config, A4).
	static const string KEY_MONEY = "#LF-WelcomeGuide_Body2a";

	//! Pages; PageKeys holds each one's heading and paragraph keys.
	static const int PAGES = 5;

	protected static ref LF_UIScreen s_Screen;
	protected static ref LF_WelcomeGuideSheet s_Sheet;
	protected static int s_iPage;
	//! Whether the open page was built with the arrival errands done.
	protected static bool s_bShownDone;
	protected static bool s_bAutoWaiting;
	protected static int s_iAutoTries;
	//! A session client (agent) that asked for the auto open, for the welcome-guide proof only.
	protected static bool s_bHarnessAutoOpen;

	protected LF_UIScreen m_Subscribed;

	//------------------------------------------------------------------------------------------------
	static int PageCount()
	{
		return PAGES;
	}

	//------------------------------------------------------------------------------------------------
	//! One page's string keys: the heading first, then its paragraphs. Past the last page: the last.
	//! param arrivalDone True once every arrival errand is turned in: page 1 then says what to do next
	//! instead of sending the player to the clerk.
	static array<string> PageKeys(int page, bool arrivalDone = false)
	{
		array<string> keys = {};
		if (page == 0 && arrivalDone)
		{
			keys.Insert("#LF-WelcomeGuide_DoneHead1");
			keys.Insert("#LF-WelcomeGuide_DoneBody1a");
			keys.Insert("#LF-WelcomeGuide_DoneBody1b");
		}
		else if (page == 0)
		{
			keys.Insert("#LF-WelcomeGuide_Head1");
			keys.Insert("#LF-WelcomeGuide_Body1a");
			keys.Insert("#LF-WelcomeGuide_Body1b");
		}
		else if (page == 1)
		{
			keys.Insert("#LF-WelcomeGuide_Head2");
			keys.Insert("#LF-WelcomeGuide_Body2a");
			keys.Insert("#LF-WelcomeGuide_Body2b");
		}
		else if (page == 2)
		{
			keys.Insert("#LF-WelcomeGuide_Head3");
			keys.Insert("#LF-WelcomeGuide_Body3a");
			keys.Insert("#LF-WelcomeGuide_Body3b");
		}
		else if (page == 3)
		{
			keys.Insert("#LF-WelcomeGuide_Head4");
			keys.Insert("#LF-WelcomeGuide_Body4a");
			keys.Insert("#LF-WelcomeGuide_Body4b");
		}
		else
		{
			keys.Insert("#LF-WelcomeGuide_Head5");
			keys.Insert("#LF-WelcomeGuide_Body5a");
			keys.Insert("#LF-WelcomeGuide_Body5b");
		}

		return keys;
	}

	//------------------------------------------------------------------------------------------------
	//! The opening bank a new character gets, read from the live config: the game mode's figure, which a
	//! pack's LF_CharacterConfig overlay replaces (Everon: $2,000). The guide never states a number of its own.
	static int StartingBank()
	{
		LF_SpawnLogic logic = LF_SpawnLogic.GetInstance();
		if (logic)
			return logic.GetStartingBank();

		return LF_Utils.MaxInt(0, LF_CharacterConfig.StartingBank(0));
	}

	//------------------------------------------------------------------------------------------------
	//! One paragraph's text: the money paragraph (page 2) carries the starting bank as %1, formatted like the HUD ($2,000).
	static string BodyText(string key)
	{
		if (key == KEY_MONEY)
			return LF_Terms.Text(key, LF_MoneyFormat.FormatMoney(StartingBank()));

		return LF_Terms.Text(key);
	}

	//------------------------------------------------------------------------------------------------
	//! The parts of one page. Page 0 carries "Set waypoint"; the nav row drops Previous on the first
	//! page and Next on the last.
	static array<ref LF_UIPart> BuildParts(int page, bool arrivalDone = false)
	{
		array<ref LF_UIPart> parts = {};
		if (page < 0 || page >= PageCount())
		{
			LF_Debug.Warn(LF_WelcomeGuide.FEATURE, "no guide page " + page);
			return parts;
		}

		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.TITLE, PART_TITLE, "#LF-WelcomeGuide_Title"));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, PART_PAGE, LF_Terms.Text("#LF-WelcomeGuide_Page", (page + 1).ToString(), PageCount().ToString())));
		array<string> keys = PageKeys(page, arrivalDone);
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.HEADING, PART_HEADING, LF_Terms.Text(keys[0])));
		for (int i = 1; i < keys.Count(); i++)
		{
			parts.Insert(LF_UIPart.Create(LF_EUIPartKind.RICH, PART_BODY + i.ToString(), BodyText(keys[i])).InBody());
		}

		// No errand is left to mark once the chain is done, so the button goes with the clerk text.
		if (page == 0 && !arrivalDone)
			parts.Insert(LF_UIPart.Create(LF_EUIPartKind.BUTTON, PART_WAYPOINT, "#LF-WelcomeGuide_Waypoint"));

		array<string> names = {};
		array<string> texts = {};
		if (page > 0)
		{
			names.Insert(PART_PREV);
			texts.Insert("#LF-WelcomeGuide_Prev");
		}

		if (page < PageCount() - 1)
		{
			names.Insert(PART_NEXT);
			texts.Insert("#LF-WelcomeGuide_Next");
		}

		parts.Insert(LF_UIPart.CreateKeys(PART_NAV, names, texts));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.BUTTON, PART_DISMISS, "#LF-WelcomeGuide_Dismiss"));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.CLOSE, PART_CLOSE, "#LF-WelcomeGuide_Close"));
		return parts;
	}

	//------------------------------------------------------------------------------------------------
	//! Opens the guide on a page (the key, the harness, the auto open).
	static LF_UIScreen Open(int page = 0)
	{
		Hide();
		if (page < 0 || page >= PageCount())
			page = 0;

		s_iPage = page;
		s_Sheet = new LF_WelcomeGuideSheet();
		// The personal file's copy is the client's only word on the arrival errands; ask for a fresh one
		// (OnFileData reprints page 1 if the answer changes it).
		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (controller)
			controller.LF_AskPlayerMenu();

		return s_Sheet.Show();
	}

	//------------------------------------------------------------------------------------------------
	//! True when the last personal-file answer says every arrival errand is turned in.
	static bool ArrivalDone()
	{
		LF_PlayerMenuData data = LF_PlayerMenuSheet.Data();
		return data && data.ArrivalFinished();
	}

	//------------------------------------------------------------------------------------------------
	//! The personal file's answer came in: reprint page 1 when it changes what page 1 says.
	static void OnFileData()
	{
		if (!s_Screen || !s_Screen.IsOpen() || !s_Sheet || s_iPage != 0)
			return;

		if (ArrivalDone() != s_bShownDone)
			GetGame().GetCallqueue().CallLater(s_Sheet.ShowLater, 0, false);
	}

	//------------------------------------------------------------------------------------------------
	//! The key: open on the first page, or put the guide away when it is open.
	static void Toggle()
	{
		if (s_Screen && s_Screen.IsOpen())
		{
			Hide();
			return;
		}

		if (!LF_WelcomeGuide.CanOpenByKey(LF_DialogueScene.IsOpen()))
		{
			LF_Debug.Info(LF_WelcomeGuide.FEATURE, "guide key ignored: a conversation is up");
			return;
		}

		Open(0);
	}

	//------------------------------------------------------------------------------------------------
	protected LF_UIScreen Show()
	{
		if (s_Screen)
		{
			s_Screen.Close();
			s_Screen = null;
		}

		LF_UISkin skin = LF_UISkin.Paper();
		skin.m_fWidth = SHEET_WIDTH;
		s_bShownDone = ArrivalDone();
		LF_UIScreen screen = LF_UIScreen.Open(BuildParts(s_iPage, s_bShownDone), skin);
		if (!screen)
		{
			LF_Debug.Error(LF_WelcomeGuide.FEATURE, "welcome guide sheet failed to open");
			return null;
		}

		screen.GetOnButton().Insert(OnButton);
		m_Subscribed = screen;
		screen.CloseOnDeath();
		screen.CloseWhenUncontrolled();
		s_Screen = screen;
		LF_Debug.Info(LF_WelcomeGuide.FEATURE, string.Format("guide page %1 of %2 open", s_iPage + 1, PageCount()));
		return screen;
	}

	//------------------------------------------------------------------------------------------------
	protected void OnButton(string name)
	{
		if (name == PART_NEXT || name == PART_PREV)
		{
			if (name == PART_NEXT)
				s_iPage = LF_Utils.ClampInt(s_iPage + 1, 0, PageCount() - 1);
			else
				s_iPage = LF_Utils.ClampInt(s_iPage - 1, 0, PageCount() - 1);

			// The press runs inside the old sheet; build the next one after it has finished.
			GetGame().GetCallqueue().CallLater(ShowLater, 0, false);
			return;
		}

		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (name == PART_WAYPOINT)
		{
			if (!controller)
			{
				LF_Debug.Warn(LF_WelcomeGuide.FEATURE, "set waypoint refused: no player controller");
				return;
			}

			controller.LF_AskWelcomeGuideWaypoint();
			GetGame().GetCallqueue().CallLater(Hide, 0, false);
			return;
		}

		if (name == PART_DISMISS)
		{
			if (controller)
				controller.LF_AskWelcomeGuideDismiss();
			else
				LF_Debug.Warn(LF_WelcomeGuide.FEATURE, "dismiss not sent: no player controller");

			Toast("#LF-WelcomeGuide_Dismissed", string.Empty);
			GetGame().GetCallqueue().CallLater(Hide, 0, false);
			return;
		}

		LF_Debug.Warn(LF_WelcomeGuide.FEATURE, "guide button " + name + " does nothing");
	}

	//------------------------------------------------------------------------------------------------
	protected void ShowLater()
	{
		if (s_Sheet == this)
			Show();
	}

	//------------------------------------------------------------------------------------------------
	//! The server's answer to "Set waypoint".
	static void OnWaypointAnswer(int result, string labelKey)
	{
		string label = string.Empty;
		if (!labelKey.IsEmpty())
			label = LF_Terms.Text(labelKey);

		LF_Debug.Info(LF_WelcomeGuide.FEATURE, string.Format("waypoint answer %1 (%2)", typename.EnumToString(LF_EWelcomeWaypoint, result), labelKey));
		Toast(LF_WelcomeGuide.WaypointToastKey(result), label);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Toast(string bodyKey, string arg)
	{
		LF_NotificationManagerComponent manager = LF_NotificationManagerComponent.GetInstance();
		if (!manager)
		{
			LF_Debug.Warn(LF_WelcomeGuide.FEATURE, "no toast for " + bodyKey + ": no notification manager");
			return;
		}

		manager.ShowNotificationToast("#LF-WelcomeGuide_Title", bodyKey, TOAST_SECONDS, LF_ENotificationType.INFO, arg);
	}

	//------------------------------------------------------------------------------------------------
	//! The server says the guide is pending: open it when the moment is right. A second "show" while a
	//! try is waiting (the join answer beside the first-spawn push) is the same one; a later one (a second
	//! new character on this connection) schedules again.
	static void AutoOpen()
	{
		if (!LF_WelcomeGuide.AutoOpenWanted(LF_AgentHarness.IsSessionClient(), s_bHarnessAutoOpen))
		{
			LF_Debug.Info(LF_WelcomeGuide.FEATURE, "guide not opened by itself: this client is driven by an agent session (`guide autoopen on` opts in)");
			return;
		}

		if (s_bAutoWaiting)
			return;

		s_bAutoWaiting = true;
		s_iAutoTries = 0;
		GetGame().GetCallqueue().CallLater(TryAutoOpen, AUTO_DELAY_MS, false);
	}

	//------------------------------------------------------------------------------------------------
	protected static void TryAutoOpen()
	{
		if (s_Screen && s_Screen.IsOpen())
		{
			s_bAutoWaiting = false;
			return;
		}

		IEntity body;
		PlayerController player = GetGame().GetPlayerController();
		if (player)
			body = player.GetControlledEntity();

		bool alive;
		bool loitering;
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(body);
		if (character)
		{
			SCR_CharacterControllerComponent controller = SCR_CharacterControllerComponent.Cast(character.GetCharacterController());
			if (controller)
			{
				alive = controller.GetLifeState() == ECharacterLifeState.ALIVE;
				loitering = controller.IsLoitering();
			}
		}

		bool menuOpen = GetGame().GetMenuManager() && GetGame().GetMenuManager().IsAnyMenuOpen();
		bool sheetOpen = LF_UIScreen.IsActionPromptSuppressed();
		bool dialogueOpen = LF_DialogueScene.IsOpen();
		if (LF_WelcomeGuide.CanAutoOpen(body != null, alive, loitering, menuOpen, sheetOpen, dialogueOpen))
		{
			s_bAutoWaiting = false;
			LF_Debug.Info(LF_WelcomeGuide.FEATURE, "guide opens by itself for a new character");
			Open(0);
			return;
		}

		s_iAutoTries++;
		if (s_iAutoTries >= AUTO_MAX_TRIES)
		{
			s_bAutoWaiting = false;
			LF_Debug.Warn(LF_WelcomeGuide.FEATURE, string.Format("guide did not open by itself (body=%1 alive=%2 loiter=%3 menu=%4 sheet=%5 dialogue=%6); it tries again at the next join", body != null, alive, loitering, menuOpen, sheetOpen, dialogueOpen));
			return;
		}

		GetGame().GetCallqueue().CallLater(TryAutoOpen, AUTO_RETRY_MS, false);
	}

	//------------------------------------------------------------------------------------------------
	//! Harness: let the auto open run on this session client (`guide autoopen on`) or put it back off.
	static void SetHarnessAutoOpen(bool on)
	{
		s_bHarnessAutoOpen = on;
	}

	//------------------------------------------------------------------------------------------------
	static LF_UIScreen Current()
	{
		return s_Screen;
	}

	//------------------------------------------------------------------------------------------------
	static int CurrentPage()
	{
		return s_iPage;
	}

	//------------------------------------------------------------------------------------------------
	static void Hide()
	{
		s_Sheet = null;
		if (!s_Screen)
			return;

		s_Screen.Close();
		s_Screen = null;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(TryAutoOpen);

		Hide();
		s_iPage = 0;
		s_bShownDone = false;
		s_bAutoWaiting = false;
		s_iAutoTries = 0;
		s_bHarnessAutoOpen = false;
	}

	//------------------------------------------------------------------------------------------------
	void ~LF_WelcomeGuideSheet()
	{
		if (m_Subscribed)
			m_Subscribed.GetOnButton().Remove(OnButton);
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_WelcomeGuideSheet's statics (LF_StaticReset finds it by reflection).
class LF_WelcomeGuideSheet_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_WelcomeGuideSheet";
	}

	override void Apply()
	{
		LF_WelcomeGuideSheet.ResetStatics();
	}
}
