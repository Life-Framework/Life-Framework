//------------------------------------------------------------------------------------------------
//! The war sheet at the registry desk (#2675, M7 gang war W7): a sheet of UI kit parts listing the
//! organisations on the register with a Declare button, the declarations made against the viewer's
//! organisation with Accept and the time left, and the viewer's active wars with their end time.
//! The buttons call the W3 asks (LF_OrgWarCharacter.c); the server re-checks everything, the sheet
//! only hides a button the viewer could not use.
//!
//! The server builds a page (BuildPage) and sends it in one RPC; the client rows come from the
//! page alone (Rows), so the LOGIC test reads them without widgets. Rows are keys and numbers, never
//! translated text: the screen translates them when it paints.
//!
//! Wire format: "name|orgId|members|state" rows for the register (sorted by name), and
//! "name|warId|seconds" rows for pending and active wars, all joined by ";". Organisation names
//! cannot hold "|" or ";" (LF_OrgRules.NAME_CHARS).
//------------------------------------------------------------------------------------------------
//! One row of the sheet. A name is the kit part's name; target is the organisation id (register rows)
//! or the war id (pending rows).
class LF_OrgWarSheetRow : Managed
{
	string m_sName;
	string m_sTarget;
	string m_sLabel;
	string m_sStatusKey;
	string m_sStatusArg;
	//! When above zero the status line reads this span (LF_OrgWarBoard.SpanText) as its argument.
	int m_iSeconds;
	//! Localization key of the button, or empty for a row with no button.
	string m_sAction;

	//------------------------------------------------------------------------------------------------
	static LF_OrgWarSheetRow Create(string name, string target, string label, string statusKey, string statusArg, int seconds, string action)
	{
		LF_OrgWarSheetRow row = new LF_OrgWarSheetRow();
		row.m_sName = name;
		row.m_sTarget = target;
		row.m_sLabel = label;
		row.m_sStatusKey = statusKey;
		row.m_sStatusArg = statusArg;
		row.m_iSeconds = seconds;
		row.m_sAction = action;
		return row;
	}

	//------------------------------------------------------------------------------------------------
	//! The status line as the player reads it (client).
	string StatusText()
	{
		if (m_iSeconds > 0)
			return LF_Terms.Text(m_sStatusKey, LF_OrgWarBoard.SpanText(m_iSeconds));

		return LF_Terms.Text(m_sStatusKey, m_sStatusArg);
	}
}

//------------------------------------------------------------------------------------------------
//! What the server tells one viewer. Plain values and the three row strings.
class LF_OrgWarSheetPage : Managed
{
	bool m_bHasOrg;
	//! The viewer's rank holds DECLARE_WAR.
	bool m_bCanDeclare;
	int m_iFee;
	int m_iMinMembers;
	int m_iOwnMembers;
	string m_sOrgs;
	string m_sPending;
	string m_sActive;

	//------------------------------------------------------------------------------------------------
	static LF_OrgWarSheetPage Create(bool hasOrg, bool canDeclare, int fee, int minMembers, int ownMembers, string orgs, string pending, string active)
	{
		LF_OrgWarSheetPage page = new LF_OrgWarSheetPage();
		page.m_bHasOrg = hasOrg;
		page.m_bCanDeclare = canDeclare;
		page.m_iFee = fee;
		page.m_iMinMembers = minMembers;
		page.m_iOwnMembers = ownMembers;
		page.m_sOrgs = orgs;
		page.m_sPending = pending;
		page.m_sActive = active;
		return page;
	}

	//------------------------------------------------------------------------------------------------
	//! True when this viewer may press Declare or Accept: in an organisation, holding the right, and
	//! the organisation has the member minimum. (The server checks again.)
	bool CanAct()
	{
		return m_bHasOrg && m_bCanDeclare && m_iOwnMembers >= m_iMinMembers;
	}
}

//------------------------------------------------------------------------------------------------
class LF_OrgWarSheet
{
	static const string FEATURE = "OrgWars";

	static const string PART_TITLE = "OrgWarSheetTitle";
	static const string PART_INTRO = "OrgWarIntro";
	static const string PART_ORGS_LINE = "OrgWarOrgsLine";
	static const string PART_ORGS = "OrgWarOrgs";
	static const string PART_PENDING_LINE = "OrgWarPendingLine";
	static const string PART_PENDING = "OrgWarPending";
	static const string PART_ACTIVE_LINE = "OrgWarActiveLine";
	static const string PART_ACTIVE = "OrgWarActive";
	static const string PART_CLOSE = "CloseButton";

	//! A row's button name is the prefix plus the organisation or war id.
	static const string PREFIX_DECLARE = "WarDecl_";
	static const string PREFIX_ACCEPT = "WarAcc_";
	static const string PREFIX_ACTIVE = "WarRun_";

	static const int SECTION_ORGS = 0;
	static const int SECTION_PENDING = 1;
	static const int SECTION_ACTIVE = 2;

	//! The state of one organisation as the register row shows it. Sent as a number, so append only.
	static const int STATE_OPEN = 0;
	static const int STATE_PENDING = 1;
	static const int STATE_AT_WAR = 2;
	static const int STATE_COOLDOWN = 3;	// lf-fixed: wire enum value
	static const int STATE_TOO_FEW = 4;

	//! The register rows one page carries (memory and wire bound, far above a server's organisations).
	static const int ORGS_MAX = 60;	// lf-fixed: wire bound
	//! Walk this far from the desk and the sheet closes (the ledger's range).
	static const float CLOSE_RANGE = 4.0;	// lf-fixed: the ledger's UI close range
	//! Two list rows tall (a row is about 52); the body scrolls past that.
	static const float LIST_HEIGHT = 104;	// lf-fixed: layout size, two list rows
	static const float ORGS_LIST_HEIGHT = 156;	// lf-fixed: layout size, three list rows

	protected static ref LF_OrgWarSheet s_Sheet;

	protected ref LF_UIScreen m_Screen;
	protected ref LF_OrgWarSheetPage m_Page;
	protected EntityID m_Desk;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_Sheet = null;
	}

	//------------------------------------------------------------------------------------------------
	static LF_OrgWarSheet Current()
	{
		return s_Sheet;
	}

	//------------------------------------------------------------------------------------------------
	//! The sheet as a list of kit parts. Pure: the LOGIC test reads it without widgets.
	static array<ref LF_UIPart> BuildParts()
	{
		array<ref LF_UIPart> parts = {};
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.TITLE, PART_TITLE, "#LF-OrgWarSheet_Title"));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, PART_INTRO, "").InBody());
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, PART_ORGS_LINE, "").InBody());
		parts.Insert(LF_UIPart.CreateList(PART_ORGS).InBody());
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, PART_PENDING_LINE, "").InBody());
		parts.Insert(LF_UIPart.CreateList(PART_PENDING).InBody());
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, PART_ACTIVE_LINE, "").InBody());
		parts.Insert(LF_UIPart.CreateList(PART_ACTIVE).InBody());
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.CLOSE, PART_CLOSE, "#LF-OrgWarSheet_Close"));
		return parts;
	}

	//------------------------------------------------------------------------------------------------
	//! The intro line key and its two arguments in words: what a declaration costs and how many
	//! members each side needs. The key says why the viewer cannot act when they cannot.
	//! \param[out] fee The fee as money text. \param[out] members The member minimum.
	static string IntroKey(notnull LF_OrgWarSheetPage page, out string fee, out string members)
	{
		fee = LF_MoneyFormat.FormatMoney(page.m_iFee);
		members = page.m_iMinMembers.ToString();
		if (!page.m_bHasOrg)
			return "#LF-OrgWarSheet_IntroNoOrg";

		if (!page.m_bCanDeclare)
			return "#LF-OrgWarSheet_IntroNoRight";

		if (page.m_iOwnMembers < page.m_iMinMembers)
			return "#LF-OrgWarSheet_IntroTooFew";

		return "#LF-OrgWarSheet_Intro";
	}

	//------------------------------------------------------------------------------------------------
	//! The rows of one section for a page. A register row carries Declare only when the viewer can act
	//! and the organisation is open to a declaration; a pending row carries Accept when the viewer can
	//! act; an active row never has a button.
	static array<ref LF_OrgWarSheetRow> Rows(notnull LF_OrgWarSheetPage page, int section)
	{
		array<ref LF_OrgWarSheetRow> rows = {};
		if (section == SECTION_ORGS)
		{
			foreach (string line : Split(page.m_sOrgs))
			{
				array<string> fields = {};
				line.Split("|", fields, false);
				if (fields.Count() < 4 || fields[1].IsEmpty())
					continue;

				int state = fields[3].ToInt();
				string statusKey = "#LF-OrgWarSheet_Members";
				string statusArg = fields[2];
				string action;
				if (state == STATE_PENDING)
					statusKey = "#LF-OrgWarSheet_StatePending";
				else if (state == STATE_AT_WAR)
					statusKey = "#LF-OrgWarSheet_StateAtWar";
				else if (state == STATE_COOLDOWN)
					statusKey = "#LF-OrgWarSheet_StateCooldown";
				else if (state == STATE_TOO_FEW)
				{
					statusKey = "#LF-OrgWarSheet_StateTooFew";
					statusArg = page.m_iMinMembers.ToString();
				}
				else if (page.CanAct())
					action = "#LF-OrgWarSheet_Declare";

				rows.Insert(LF_OrgWarSheetRow.Create(PREFIX_DECLARE + fields[1], fields[1], fields[0], statusKey, statusArg, 0, action));
			}

			return rows;
		}

		string raw = page.m_sPending;
		string prefix = PREFIX_ACCEPT;
		string key = "#LF-OrgWarSheet_AnswerBy";
		if (section == SECTION_ACTIVE)
		{
			raw = page.m_sActive;
			prefix = PREFIX_ACTIVE;
			key = "#LF-OrgWarSheet_EndsIn";
		}

		foreach (string warLine : Split(raw))
		{
			array<string> warFields = {};
			warLine.Split("|", warFields, false);
			if (warFields.Count() < 3 || warFields[1].IsEmpty())
				continue;

			string warAction;
			if (section == SECTION_PENDING && page.CanAct())
				warAction = "#LF-OrgWarSheet_Accept";

			rows.Insert(LF_OrgWarSheetRow.Create(prefix + warFields[1], warFields[1], warFields[0], key, "", warFields[2].ToInt(), warAction));
		}

		return rows;
	}

	//------------------------------------------------------------------------------------------------
	protected static array<string> Split(string joined)
	{
		array<string> lines = {};
		if (!joined.IsEmpty())
			joined.Split(";", lines, true);

		return lines;
	}

	//------------------------------------------------------------------------------------------------
	//! The pair's state between the viewer's organisation and another, from the war records: a war in
	//! progress, a declaration waiting, a closed war still inside its cooldown, or open. Reads the
	//! records only (call LF_OrgWars.Tick(now) first).
	static int PairState(string ownOrgId, string otherOrgId, int now, notnull array<ref LF_OrgWarRecord> wars)
	{
		int state = STATE_OPEN;
		foreach (LF_OrgWarRecord war : wars)
		{
			if (!war || !war.IsPair(ownOrgId, otherOrgId))
				continue;

			if (war.m_eState == LF_EOrgWarState.ACCEPTED)
				return STATE_AT_WAR;

			if (war.m_eState == LF_EOrgWarState.DECLARED)
				state = STATE_PENDING;
			else if (state == STATE_OPEN && now < war.m_iCooldownUntil)
				state = STATE_COOLDOWN;
		}

		return state;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the page for one character, with an explicit clock and rules (tests). Declarations on
	//! the viewer's organisation that are still inside their answer window, and its accepted wars.
	static LF_OrgWarSheetPage BuildPageAt(string characterId, int now, notnull LF_OrgWarRules rules)
	{
		LF_OrgWars.Tick(now);
		LF_Organization own = LF_OrganizationManager.GetByCharacter(characterId);
		string ownId;
		bool canDeclare;
		int ownMembers;
		if (own)
		{
			ownId = own.GetId();
			canDeclare = own.Allows(characterId, LF_EOrganizationPermission.DECLARE_WAR);
			ownMembers = own.GetMemberCount();
		}

		array<ref LF_OrgWarRecord> wars = LF_OrgWars.ExportAll();
		array<string> orgRows = {};
		array<ref LF_OrganizationRecord> records = LF_OrganizationManager.ExportAll();
		foreach (LF_OrganizationRecord record : records)
		{
			if (!record || record.m_sId == ownId)
				continue;

			int members = record.m_aMembers.Count();
			int state = PairState(ownId, record.m_sId, now, wars);
			if (state == STATE_OPEN && members < rules.m_iMinMembers)
				state = STATE_TOO_FEW;

			orgRows.Insert(string.Format("%1|%2|%3|%4", record.m_sName, record.m_sId, members, state));
		}

		orgRows.Sort();
		string orgs;
		for (int i = 0; i < orgRows.Count() && i < ORGS_MAX; i++)
		{
			if (i > 0)
				orgs += ";";

			orgs += orgRows[i];
		}

		string pending;
		string active;
		if (own)
		{
			foreach (LF_OrgWarRecord war : wars)
			{
				if (!war || !war.Involves(ownId))
					continue;

				if (war.m_eState == LF_EOrgWarState.DECLARED && war.m_sTargetOrgId == ownId)
					pending = AppendWar(pending, war.m_sDeclarerOrgId, war.m_sWarId, war.m_iDeclaredAt + war.m_iAnswerWindowSec - now);
				else if (war.m_eState == LF_EOrgWarState.ACCEPTED)
					active = AppendWar(active, OtherOrgId(war, ownId), war.m_sWarId, war.m_iEndsAt - now);
			}
		}

		return LF_OrgWarSheetPage.Create(own != null, canDeclare, rules.m_iFee, rules.m_iMinMembers, ownMembers, orgs, pending, active);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the page for one character, now, under the configured rules.
	static LF_OrgWarSheetPage BuildPage(string characterId)
	{
		return BuildPageAt(characterId, System.GetUnixTime(), LF_OrgWarConfig.Rules());
	}

	//------------------------------------------------------------------------------------------------
	protected static string OtherOrgId(notnull LF_OrgWarRecord war, string ownId)
	{
		if (war.m_sDeclarerOrgId == ownId)
			return war.m_sTargetOrgId;

		return war.m_sDeclarerOrgId;
	}

	//------------------------------------------------------------------------------------------------
	//! One "name|warId|seconds" row appended to a joined string. A span that has run out reads as 1
	//! second, so the row still shows and never as "no time".
	protected static string AppendWar(string joined, string otherOrgId, string warId, int seconds)
	{
		string name = otherOrgId;
		LF_Organization other = LF_OrganizationManager.Get(otherOrgId);
		if (other)
			name = other.GetName();

		string row = string.Format("%1|%2|%3", name, warId, LF_Utils.MaxInt(seconds, 1));
		if (joined.IsEmpty())
			return row;

		return joined + ";" + row;
	}

	//------------------------------------------------------------------------------------------------
	//! Opens the sheet at a desk. A sheet already open is brought back to this desk.
	static LF_OrgWarSheet Open(IEntity desk)
	{
		Hide();

		SCR_ChimeraCharacter character = LF_Utils.GetLocalCharacter();
		if (!character)
		{
			LF_Debug.Error(FEATURE, "war sheet refused: no local character");
			return null;
		}

		LF_OrgWarSheet sheet = new LF_OrgWarSheet();
		if (desk)
			sheet.m_Desk = desk.GetID();
		else
			sheet.m_Desk = EntityID.INVALID;

		sheet.m_Screen = LF_UIScreen.Open(BuildParts());
		if (!sheet.m_Screen)
		{
			LF_Debug.Error(FEATURE, "war sheet failed to open");
			return null;
		}

		sheet.m_Screen.SetListHeight(PART_ORGS, ORGS_LIST_HEIGHT);
		sheet.m_Screen.SetListHeight(PART_PENDING, LIST_HEIGHT);
		sheet.m_Screen.SetListHeight(PART_ACTIVE, LIST_HEIGHT);
		sheet.m_Screen.GetOnButton().Insert(sheet.OnButton);
		sheet.m_Screen.GetOnClosed().Insert(sheet.OnClosed);
		sheet.m_Screen.CloseOnDeath();
		sheet.m_Screen.CloseWhenUncontrolled();
		if (sheet.m_Desk != EntityID.INVALID)
			sheet.m_Screen.CloseBeyond(CLOSE_RANGE, sheet.m_Desk);

		s_Sheet = sheet;
		SCR_UISoundEntity.SoundEvent(SCR_SoundEvent.TURN_PAGE);
		character.LF_AskOrgWarSheet();
		LF_Debug.Info(FEATURE, "war sheet open");
		return sheet;
	}

	//------------------------------------------------------------------------------------------------
	static void Hide()
	{
		if (!s_Sheet)
			return;

		LF_OrgWarSheet sheet = s_Sheet;
		s_Sheet = null;
		sheet.Detach();
		if (sheet.m_Screen)
			sheet.m_Screen.Close();
	}

	//------------------------------------------------------------------------------------------------
	//! Server answer: paint the page.
	void ApplyPage(LF_OrgWarSheetPage page)
	{
		if (!page || !m_Screen)
			return;

		m_Page = page;
		string fee;
		string members;
		string introKey = IntroKey(page, fee, members);
		m_Screen.SetLine(PART_INTRO, LF_Terms.Text(introKey, fee, members));

		PaintList(PART_ORGS_LINE, PART_ORGS, "#LF-OrgWarSheet_OrgsLine", Rows(page, SECTION_ORGS));
		PaintList(PART_PENDING_LINE, PART_PENDING, "#LF-OrgWarSheet_PendingLine", Rows(page, SECTION_PENDING));
		PaintList(PART_ACTIVE_LINE, PART_ACTIVE, "#LF-OrgWarSheet_ActiveLine", Rows(page, SECTION_ACTIVE));
	}

	//------------------------------------------------------------------------------------------------
	protected void PaintList(string lineName, string listName, string lineKey, notnull array<ref LF_OrgWarSheetRow> rows)
	{
		m_Screen.SetLine(lineName, LF_Terms.Text(lineKey, rows.Count().ToString()));
		m_Screen.ClearList(listName);
		foreach (LF_OrgWarSheetRow row : rows)
		{
			m_Screen.AddListRow(listName, row.m_sName, row.m_sLabel, row.StatusText(), row.m_sAction);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void OnButton(string name)
	{
		SCR_ChimeraCharacter character = LF_Utils.GetLocalCharacter();
		if (!character || !m_Screen)
			return;

		// The ask, then a fresh page: the two ride one reliable channel in order, so the page the server
		// sends back already shows the result.
		if (name.StartsWith(PREFIX_DECLARE))
		{
			character.LF_AskOrgWarDeclare(name.Substring(PREFIX_DECLARE.Length(), name.Length() - PREFIX_DECLARE.Length()));
			character.LF_AskOrgWarSheet();
			return;
		}

		if (name.StartsWith(PREFIX_ACCEPT))
		{
			character.LF_AskOrgWarAnswer(name.Substring(PREFIX_ACCEPT.Length(), name.Length() - PREFIX_ACCEPT.Length()));
			character.LF_AskOrgWarSheet();
			return;
		}

		LF_Debug.Info(FEATURE, "war sheet ignored button " + name);
	}

	//------------------------------------------------------------------------------------------------
	protected void Detach()
	{
		if (!m_Screen)
			return;

		m_Screen.GetOnButton().Remove(OnButton);
		m_Screen.GetOnClosed().Remove(OnClosed);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnClosed()
	{
		Detach();
		if (s_Sheet == this)
			s_Sheet = null;

		SCR_UISoundEntity.SoundEvent(SCR_SoundEvent.SOUND_HUD_MAP_CLOSE);
		LF_Debug.Info(FEATURE, "war sheet closed");
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_OrgWarSheet's statics (LF_StaticReset finds it by reflection).
class LF_OrgWarSheet_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_OrgWarSheet";
	}

	override void Apply()
	{
		LF_OrgWarSheet.ResetStatics();
	}
}

//------------------------------------------------------------------------------------------------
//! The sheet's ask and answer on the character. The guards are the war desk's (LF_OrgWarCharacter.c,
//! which compiles first in the modded chain): systems switch, rate bucket, desk range.
modded class SCR_ChimeraCharacter
{
	//------------------------------------------------------------------------------------------------
	//! Client: ask the server for the war sheet page.
	void LF_AskOrgWarSheet()
	{
		Rpc(RpcAsk_LF_OrgWarSheet);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_OrgWarSheet()
	{
		if (!LF_PassOrgWarGuards("war sheet"))
			return;

		LF_OrgWarSheetPage page = LF_OrgWarSheet.BuildPage(LF_OrganizationPlayer.CharacterId(this));
		RplComponent rpl = RplComponent.Cast(FindComponent(RplComponent));
		// A listen host owns its own character and an Owner RPC to itself has no wire hop, so the page
		// is applied directly there (as the ledger does).
		if (rpl && rpl.IsOwner())
			RpcDo_LF_OrgWarSheet(page.m_bHasOrg, page.m_bCanDeclare, page.m_iFee, page.m_iMinMembers, page.m_iOwnMembers, page.m_sOrgs, page.m_sPending, page.m_sActive);
		else
			Rpc(RpcDo_LF_OrgWarSheet, page.m_bHasOrg, page.m_bCanDeclare, page.m_iFee, page.m_iMinMembers, page.m_iOwnMembers, page.m_sOrgs, page.m_sPending, page.m_sActive);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_LF_OrgWarSheet(bool hasOrg, bool canDeclare, int fee, int minMembers, int ownMembers, string orgs, string pending, string active)
	{
		LF_OrgWarSheet sheet = LF_OrgWarSheet.Current();
		if (!sheet)
			return;

		sheet.ApplyPage(LF_OrgWarSheetPage.Create(hasOrg, canDeclare, fee, minMembers, ownMembers, orgs, pending, active));
	}
}
