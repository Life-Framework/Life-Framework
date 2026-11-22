// LF_PlayerMenuSheet.c: the personal file, the F2 key's sheet (gamepad: hold RB + Y). Client only.
// Built from UI kit parts (LF_UIScreen), so it takes the 1989 paper theme: no new screen .layout,
// no colours or fonts here (docs/style-guide.md "Screens"). Two tabs, each a page of the same file:
// PERSON (name and role, money, houses, garage, playtime, wanted, licences, the job skills with an XP
// bar for the one picked) and PROGRESS (the first errands in chain order, then any other errand
// taken, each marked DONE, CURRENT or LATER, with "Set waypoint" for the current errand). Everything
// printed comes from the server's answer (LF_PlayerMenuController, LF_PlayerMenuData); the sheet
// changes nothing. The welcome guide and the game settings open from its footer.

class LF_PlayerMenuSheet
{
	static const string PART_TITLE = "PlayerMenuTitle";
	static const string PART_TABS = "PlayerMenuTabs";
	static const string PART_TAB_PERSON = "PlayerMenuPersonButton";
	static const string PART_TAB_PROGRESS = "PlayerMenuProgressButton";
	static const string PART_IDENTITY = "PlayerMenuIdentity";
	static const string PART_MONEY = "PlayerMenuMoney";
	static const string PART_STATUS = "PlayerMenuStatus";
	//! The list's height: three rows show, the rest scroll, so the picked row's record stays on screen.
	static const float LIST_HEIGHT = 240;
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
	static const float SHEET_WIDTH = 680;

	static const int TAB_PERSON = 0;
	static const int TAB_PROGRESS = 1;

	protected static ref LF_UIScreen s_Screen;
	protected static ref LF_PlayerMenuSheet s_Sheet;
	//! The last answer, so a reopen prints at once while the fresh ask is on its way.
	protected static ref LF_PlayerMenuData s_Data;
	protected static string s_sWire;
	protected static int s_iTab;
	protected static int s_iSkill = -1;
	protected static int s_iQuest = -1;

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
		if (tab == TAB_PROGRESS)
			s_iTab = TAB_PROGRESS;

		s_Sheet = new LF_PlayerMenuSheet();
		LF_UIScreen screen = s_Sheet.Show();
		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (controller)
			controller.LF_AskPlayerMenu();
		else
			LF_Debug.Warn(LF_PlayerMenuServer.FEATURE, "file not asked: no player controller");

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
		LF_Debug.Info(LF_PlayerMenuServer.FEATURE, string.Format("file received: %1 skills, %2 quests, %3 licences", data.m_aSkills.Count(), data.m_aQuests.Count(), data.m_aLicenceKeys.Count()));
		if (changed && IsOpen() && s_Sheet)
			GetGame().GetCallqueue().CallLater(s_Sheet.ShowLater, 0, false);
	}

	//------------------------------------------------------------------------------------------------
	static void Hide()
	{
		s_Sheet = null;
		if (!s_Screen)
			return;

		LF_UIScreen screen = s_Screen;
		s_Screen = null;
		screen.Close();
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
		array<ref LF_UIPart> parts;
		if (s_iTab == TAB_PROGRESS)
			parts = BuildProgress(s_Data);
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
			m_Subscribed.GetOnButton().Remove(OnButton);

		m_Subscribed = screen;
		screen.GetOnButton().Insert(OnButton);
		screen.CloseOnDeath();
		screen.CloseWhenUncontrolled();
		if (s_Data)
			screen.SetListHeight(PART_LIST, LIST_HEIGHT);

		FillList(screen, s_Data);
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
	protected static array<ref LF_UIPart> Header(int tab)
	{
		array<ref LF_UIPart> parts = {};
		string title = "#LF-PlayerMenu_TitlePerson";
		if (tab == TAB_PROGRESS)
			title = "#LF-PlayerMenu_TitleProgress";

		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.TITLE, PART_TITLE, title));
		array<string> names = {PART_TAB_PERSON, PART_TAB_PROGRESS};
		// The open tab is printed between dots; the title names it too.
		string person = LF_Terms.Text("#LF-PlayerMenu_TabPerson");
		string progress = LF_Terms.Text("#LF-PlayerMenu_TabProgress");
		if (tab == TAB_PROGRESS)
			progress = LF_Terms.Text("#LF-PlayerMenu_TabOpen", progress);
		else
			person = LF_Terms.Text("#LF-PlayerMenu_TabOpen", person);

		array<string> texts = {person, progress};
		parts.Insert(LF_UIPart.CreateKeys(PART_TABS, names, texts));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.RULE, "PlayerMenuRuleTop", string.Empty));
		return parts;
	}

	//------------------------------------------------------------------------------------------------
	protected static void Footer(notnull array<ref LF_UIPart> parts)
	{
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.RULE, "PlayerMenuRuleBottom", string.Empty));
		array<string> names = {PART_GUIDE, PART_SETTINGS};
		array<string> texts = {"#LF-PlayerMenu_Guide", "#LF-PlayerMenu_Settings"};
		parts.Insert(LF_UIPart.CreateKeys(PART_FOOTER, names, texts));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.CLOSE, PART_CLOSE, "#LF-PlayerMenu_Close"));
	}

	//------------------------------------------------------------------------------------------------
	//! The PERSON page. Null data prints the waiting line in place of the figures.
	static array<ref LF_UIPart> BuildPerson(LF_PlayerMenuData data)
	{
		array<ref LF_UIPart> parts = Header(TAB_PERSON);
		if (!data)
		{
			parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, PART_IDENTITY, "#LF-PlayerMenu_Waiting"));
			Footer(parts);
			return parts;
		}

		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, PART_IDENTITY, IdentityLine(data)));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.HEADING, "PlayerMenuHeadMoney", "#LF-PlayerMenu_HeadMoney"));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.RICH, PART_MONEY, MoneyLedger(data)));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.HEADING, "PlayerMenuHeadStatus", "#LF-PlayerMenu_HeadStatus"));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.RICH, PART_STATUS, StatusLedger(data)));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.HEADING, "PlayerMenuHeadSkills", "#LF-PlayerMenu_HeadSkills"));
		parts.Insert(LF_UIPart.CreateList(PART_LIST));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, PART_DETAIL_NAME, "#LF-PlayerMenu_SkillHint"));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, PART_DETAIL_TEXT, string.Empty));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.BAR, PART_DETAIL_BAR, string.Empty));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, PART_DETAIL_NUMBERS, string.Empty));
		Footer(parts);
		return parts;
	}

	//------------------------------------------------------------------------------------------------
	//! The PROGRESS page.
	static array<ref LF_UIPart> BuildProgress(LF_PlayerMenuData data)
	{
		array<ref LF_UIPart> parts = Header(TAB_PROGRESS);
		if (!data)
		{
			parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, PART_SUMMARY, "#LF-PlayerMenu_Waiting"));
			Footer(parts);
			return parts;
		}

		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, PART_SUMMARY, SummaryLine(data)));
		parts.Insert(LF_UIPart.CreateList(PART_LIST));
		string hint = "#LF-PlayerMenu_QuestHint";
		if (data.m_aQuests.IsEmpty())
			hint = "#LF-PlayerMenu_NoQuests";

		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, PART_DETAIL_NAME, hint));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, PART_DETAIL_TEXT, string.Empty));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, PART_DETAIL_NUMBERS, string.Empty));
		if (CurrentArrival(data) >= 0)
			parts.Insert(LF_UIPart.Create(LF_EUIPartKind.BUTTON, PART_WAYPOINT, "#LF-PlayerMenu_Waypoint"));

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
				screen.AddListRow(PART_LIST, ROW_SKILL + i.ToString(), SkillName(skill.m_sJob), LF_Terms.Text("#LF-PlayerMenu_SkillLevel", skill.m_iLevel.ToString()), "#LF-PlayerMenu_Read");
			}

			if (s_iSkill >= 0 && s_iSkill < data.m_aSkills.Count())
				ShowSkill(screen, data, s_iSkill);

			return;
		}

		AddGroup(screen, data, LF_EPlayerMenuQuestGroup.ARRIVAL, "#LF-PlayerMenu_GroupArrival");
		AddGroup(screen, data, LF_EPlayerMenuQuestGroup.ERRANDS, "#LF-PlayerMenu_GroupErrands");
		if (s_iQuest >= 0 && s_iQuest < data.m_aQuests.Count())
			ShowQuest(screen, data, s_iQuest);
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
		if (name == PART_TAB_PERSON || name == PART_TAB_PROGRESS)
		{
			int tab = TAB_PERSON;
			if (name == PART_TAB_PROGRESS)
				tab = TAB_PROGRESS;

			if (tab == s_iTab)
				return;

			s_iTab = tab;
			// The press runs inside the old sheet; build the next one after it has finished.
			GetGame().GetCallqueue().CallLater(ShowLater, 0, false);
			return;
		}

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
		screen.SetLine(PART_DETAIL_NAME, SkillName(skill.m_sJob));
		screen.SetLine(PART_DETAIL_TEXT, SkillDescription(skill.m_sJob));
		screen.SetBar(PART_DETAIL_BAR, LF_PlayerMenuXp.Fraction(skill.m_iXp, skill.m_iFloor, skill.m_iNext));
		screen.SetLine(PART_DETAIL_NUMBERS, SkillNumbers(skill));
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
	static string IdentityLine(notnull LF_PlayerMenuData data)
	{
		string name = data.m_sName;
		if (name.IsEmpty())
			name = LF_Terms.Text("#LF-PlayerMenu_NoName");

		string role = LF_Terms.Text(RoleKey(data.m_eFaction));
		if (data.m_eFaction != LF_Faction.CIVILIAN)
		{
			string duty = "#LF-PlayerMenu_OffDuty";
			if (data.m_bOnDuty)
				duty = "#LF-PlayerMenu_OnDuty";

			role = LF_Terms.Text("#LF-PlayerMenu_RoleDuty", role, LF_Terms.Text(duty));
		}

		return LF_Terms.Text("#LF-PlayerMenu_Identity", name, role);
	}

	//------------------------------------------------------------------------------------------------
	//! One printed ledger line, "Cash in hand .... $20".
	static string Ledger(string labelKey, string value)
	{
		return LF_Terms.Text("#LF-PlayerMenu_Ledger", LF_Terms.Text(labelKey), value);
	}

	//------------------------------------------------------------------------------------------------
	//! The FINANCES block: cash and bank, one ledger line each (RICH markup: <br/> breaks).
	static string MoneyLedger(notnull LF_PlayerMenuData data)
	{
		string text = Ledger("#LF-PlayerMenu_Cash", LF_MoneyFormat.FormatMoney(data.m_iCash));
		text += "<br/>" + Ledger("#LF-PlayerMenu_Bank", LF_MoneyFormat.FormatMoney(data.m_iBank));
		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! The STATUS block: houses, garage, playtime, wanted, licences.
	static string StatusLedger(notnull LF_PlayerMenuData data)
	{
		string text = Ledger("#LF-PlayerMenu_Houses", data.m_iHouses.ToString());
		text += "<br/>" + Ledger("#LF-PlayerMenu_Garage", LF_Terms.Text("#LF-PlayerMenu_GarageValue", data.m_iGarageUsed.ToString(), data.m_iGarageCap.ToString()));
		text += "<br/>" + Ledger("#LF-PlayerMenu_Playtime", PlaytimeText(data.m_iPlaytimeSeconds));
		text += "<br/>" + Ledger("#LF-PlayerMenu_Wanted", WantedText(data.m_iWanted));
		text += "<br/>" + LicencesLine(data);
		return text;
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
	static string LicencesLine(notnull LF_PlayerMenuData data)
	{
		if (data.m_aLicenceKeys.IsEmpty())
			return LF_Terms.Text("#LF-PlayerMenu_LicencesNone");

		string names = string.Empty;
		foreach (string key : data.m_aLicenceKeys)
		{
			if (!names.IsEmpty())
				names += ", ";

			names += Printed(key);
		}

		return LF_Terms.Text("#LF-PlayerMenu_Licences", names);
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
	static string SummaryLine(notnull LF_PlayerMenuData data)
	{
		int done;
		int total = data.CountGroup(LF_EPlayerMenuQuestGroup.ARRIVAL, done);
		int otherDone;
		int others = data.CountGroup(LF_EPlayerMenuQuestGroup.ERRANDS, otherDone);
		return LF_Terms.Text("#LF-PlayerMenu_Summary", (done + otherDone).ToString(), (total + others).ToString());
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
