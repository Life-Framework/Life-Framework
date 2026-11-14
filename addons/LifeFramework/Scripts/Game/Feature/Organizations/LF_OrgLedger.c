//------------------------------------------------------------------------------------------------
//! The registry ledger (M1-17): a 1989 ledger page on the UI kit, opened at the registry desk.
//! It is UI because founding needs a typed name and the members are a list (decision "In-world
//! first": typing and lists are correct UI); it looks like the paper on the desk.
//!
//! No organization: the founding form (name, fee, Sign and pay). A member: name, rank, fund with
//! deposit / withdraw, the processing upgrade, the hideout lease, the member list, and Leave.
//! Inviting and removing are not here: they happen face to face (LF_OrgInviteRole).
//!
//! Client-only. The page paints from the replicated copy at once, then the server's answer fills
//! the numbers. Every button sends an RPC the server re-checks (LF_OrganizationCharacter.c).
//------------------------------------------------------------------------------------------------
class LF_OrgLedgerPage : Managed
{
	bool m_bHasOrg;
	string m_sName;
	int m_iFund;
	int m_iRights;
	bool m_bProcessing;
	int m_iLease;
	//! "Name|RankKey" rows joined by ";".
	string m_sMembers;
	//! This player's rank name (a localization key for the default ranks); empty on the first paint.
	string m_sRank;

	//------------------------------------------------------------------------------------------------
	static LF_OrgLedgerPage Create(bool hasOrg, string name, int fund, int rights, bool processing, int lease, string members, string rank = "")
	{
		LF_OrgLedgerPage page = new LF_OrgLedgerPage();
		page.m_bHasOrg = hasOrg;
		page.m_sName = name;
		page.m_iFund = fund;
		page.m_iRights = rights;
		page.m_bProcessing = processing;
		page.m_iLease = lease;
		page.m_sMembers = members;
		page.m_sRank = rank;
		return page;
	}

	//------------------------------------------------------------------------------------------------
	//! The rank the server named, or until it answers the founder/member guess from the rights.
	string RankLabel()
	{
		if (!m_sRank.IsEmpty())
			return m_sRank;

		return LF_OrgRules.RankKey(m_iRights);
	}

	//------------------------------------------------------------------------------------------------
	bool Can(int right)
	{
		return (m_iRights & right) != 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Parts that differ between two pages force a rebuild; the rest is rewritten in place.
	string ShapeKey()
	{
		if (!m_bHasOrg)
			return "found";

		return "org" + m_iRights.ToString();
	}

	//------------------------------------------------------------------------------------------------
	//! \param[out] names Member names in ledger order. \param[out] ranks Their rank keys.
	void SplitMembers(notnull array<string> names, notnull array<string> ranks)
	{
		names.Clear();
		ranks.Clear();
		if (m_sMembers.IsEmpty())
			return;

		array<string> rows = {};
		m_sMembers.Split(";", rows, true);
		foreach (string row : rows)
		{
			array<string> fields = {};
			row.Split("|", fields, false);
			if (fields.Count() < 2)
				continue;

			names.Insert(fields[0]);
			ranks.Insert(fields[1]);
		}
	}
}

//------------------------------------------------------------------------------------------------
class LF_OrgLedger
{
	static const string PART_TITLE = "OrgLedgerTitle";
	static const string PART_INTRO = "OrgIntro";
	static const string PART_NAME = "OrgName";
	static const string PART_FOUND = "FoundButton";
	static const string PART_NAME_LINE = "OrgNameLine";
	static const string PART_RANK_LINE = "OrgRankLine";
	static const string PART_FUND = "OrgFundRow";
	static const string PART_AMOUNT = "OrgAmount";
	static const string PART_DEPOSIT = "DepositButton";
	static const string PART_WITHDRAW = "WithdrawButton";
	static const string PART_PROCESSING = "OrgProcessingRow";
	static const string PART_HIDEOUT = "OrgHideoutRow";
	static const string PART_MEMBERS_LINE = "OrgMembersLine";
	static const string PART_MEMBERS = "OrgMembers";
	static const string PART_INVITE_HINT = "OrgInviteHint";
	static const string PART_MEMBERSHIP = "OrgMembershipRow";
	static const string PART_NOTICE = "OrgNotice";
	static const string PART_CLOSE = "CloseButton";
	//! Walk this far from the desk and the page closes.
	static const float CLOSE_RANGE = 4.0;

	protected static ref LF_OrgLedger s_Ledger;

	protected ref LF_UIScreen m_Screen;
	protected ref LF_OrgLedgerPage m_Page;
	protected EntityID m_Desk;
	protected bool m_bRebuilding;

	//------------------------------------------------------------------------------------------------
	static LF_OrgLedger Current()
	{
		return s_Ledger;
	}

	//------------------------------------------------------------------------------------------------
	static LF_UIScreen CurrentScreen()
	{
		if (!s_Ledger)
			return null;

		return s_Ledger.m_Screen;
	}

	//------------------------------------------------------------------------------------------------
	//! The page as a list of kit parts. Pure: the LOGIC test reads it without widgets.
	static array<ref LF_UIPart> BuildParts(LF_OrgLedgerPage page)
	{
		array<ref LF_UIPart> parts = {};
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.TITLE, PART_TITLE, "#LF-Org_LedgerTitle"));
		if (!page || !page.m_bHasOrg)
		{
			parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, PART_INTRO, LF_Terms.Text("#LF-Org_FoundIntro", LF_OrgConfig.FoundingFee().ToString())));
			parts.Insert(LF_UIPart.Create(LF_EUIPartKind.FIELD, PART_NAME, "#LF-Org_NameField"));
			parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, PART_NOTICE, ""));
			parts.Insert(LF_UIPart.Create(LF_EUIPartKind.BUTTON, PART_FOUND, LF_Terms.Text("#LF-Org_FoundButton", LF_OrgConfig.FoundingFee().ToString())));
			parts.Insert(LF_UIPart.Create(LF_EUIPartKind.CLOSE, PART_CLOSE, "#LF-Org_CloseLedger"));
			return parts;
		}

		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, PART_NAME_LINE, ""));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, PART_RANK_LINE, ""));
		parts.Insert(LF_UIPart.CreateStatus(PART_FUND, "#EL-Org_Fund", "", ""));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.FIELD, PART_AMOUNT, "#EL-Org_Amount"));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.BUTTON, PART_DEPOSIT, "#EL-Org_Deposit"));
		if (page.Can(LF_OrgRules.RIGHT_WITHDRAW))
			parts.Insert(LF_UIPart.Create(LF_EUIPartKind.BUTTON, PART_WITHDRAW, "#EL-Org_Withdraw"));

		parts.Insert(LF_UIPart.CreateStatus(PART_PROCESSING, "#LF-Org_Processing", "", ""));
		parts.Insert(LF_UIPart.CreateStatus(PART_HIDEOUT, "#LF-Org_Hideout", "", ""));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, PART_MEMBERS_LINE, ""));
		parts.Insert(LF_UIPart.CreateList(PART_MEMBERS));
		if (page.Can(LF_OrgRules.RIGHT_INVITE))
			parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, PART_INVITE_HINT, "#LF-Org_InviteHint"));

		parts.Insert(LF_UIPart.CreateStatus(PART_MEMBERSHIP, "#LF-Org_Membership", "", ""));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, PART_NOTICE, ""));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.CLOSE, PART_CLOSE, "#LF-Org_CloseLedger"));
		return parts;
	}

	//------------------------------------------------------------------------------------------------
	//! Status row texts for a page, in the order fund, processing, hideout, membership.
	//! \return The action label of each row (empty hides the row's button).
	static string ProcessingAction(LF_OrgLedgerPage page)
	{
		if (page.m_bProcessing || !page.Can(LF_OrgRules.RIGHT_BUY))
			return string.Empty;

		return WidgetManager.Translate("#LF-Org_BuyFor", LF_MoneyFormat.FormatMoney(LF_OrgConfig.PriceProcessingSlot()));
	}

	//------------------------------------------------------------------------------------------------
	static string HideoutAction(LF_OrgLedgerPage page)
	{
		if (page.m_iLease != LF_EOrgLease.FREE || !page.Can(LF_OrgRules.RIGHT_BUY))
			return string.Empty;

		return "#LF-Org_TakeLease";
	}

	//------------------------------------------------------------------------------------------------
	static string MembershipAction(LF_OrgLedgerPage page)
	{
		if (page.Can(LF_OrgRules.RIGHT_FOUNDER))
			return string.Empty;

		return "#LF-Org_Leave";
	}

	//------------------------------------------------------------------------------------------------
	//! Opens the ledger at a desk. A ledger already open is brought back to this desk.
	static LF_OrgLedger Open(IEntity desk)
	{
		Hide();

		SCR_ChimeraCharacter character = LF_Utils.GetLocalCharacter();
		if (!character)
		{
			LF_Debug.Error("Organizations", "ledger refused: no local character");
			return null;
		}

		LF_OrgLedger ledger = new LF_OrgLedger();
		if (desk)
			ledger.m_Desk = desk.GetID();
		else
			ledger.m_Desk = EntityID.INVALID;

		// First paint from the replicated copy, so the right form shows before the server answers.
		LF_OrgView view = LF_OrgReplication.ViewOf(character);
		ledger.m_Page = LF_OrgLedgerPage.Create(view.HasOrg(), view.m_sOrgName, 0, view.m_iRights, false, LF_EOrgLease.FREE, "");
		if (!ledger.Build(""))
			return null;

		s_Ledger = ledger;
		SCR_UISoundEntity.SoundEvent(SCR_SoundEvent.TURN_PAGE);
		character.LF_AskOrganizationView();
		LF_Debug.Info("Organizations", "ledger open");
		return ledger;
	}

	//------------------------------------------------------------------------------------------------
	static void Hide()
	{
		if (!s_Ledger)
			return;

		LF_OrgLedger ledger = s_Ledger;
		s_Ledger = null;
		if (ledger.m_Screen)
			ledger.m_Screen.Close();
	}

	//------------------------------------------------------------------------------------------------
	protected bool Build(string notice)
	{
		m_bRebuilding = true;
		if (m_Screen)
		{
			Detach();
			m_Screen.Close();
		}

		m_bRebuilding = false;
		m_Screen = LF_UIScreen.Open(BuildParts(m_Page));
		if (!m_Screen)
		{
			LF_Debug.Error("Organizations", "ledger failed to open");
			return false;
		}

		m_Screen.GetOnButton().Insert(OnButton);
		m_Screen.GetOnClosed().Insert(OnClosed);
		m_Screen.CloseOnDeath();
		m_Screen.CloseWhenUncontrolled();
		if (m_Desk != EntityID.INVALID)
			m_Screen.CloseBeyond(CLOSE_RANGE, m_Desk);

		Paint(notice);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server answer: the page and a notice key for the ink line.
	void ApplyView(LF_OrgLedgerPage page, string notice)
	{
		if (!page)
			return;

		bool rebuild = !m_Page || m_Page.ShapeKey() != page.ShapeKey();
		bool founded = m_Page && !m_Page.m_bHasOrg && page.m_bHasOrg;
		m_Page = page;
		if (rebuild)
			Build(notice);
		else
			Paint(notice);

		if (founded)
			SCR_UISoundEntity.SoundEvent(SCR_SoundEvent.SOUND_MAP_GADGET_MARKER_DRAW_STOP);
		else if (!notice.IsEmpty())
			SCR_UISoundEntity.SoundEvent(SCR_SoundEvent.TURN_PAGE);
	}

	//------------------------------------------------------------------------------------------------
	protected void Paint(string notice)
	{
		if (!m_Screen || !m_Page)
			return;

		m_Screen.SetLine(PART_NOTICE, notice);
		if (!m_Page.m_bHasOrg)
			return;

		m_Screen.SetLine(PART_NAME_LINE, WidgetManager.Translate("#LF-Org_NameLine", m_Page.m_sName));
		m_Screen.SetLine(PART_RANK_LINE, WidgetManager.Translate("#LF-Org_RankLine", WidgetManager.Translate(m_Page.RankLabel())));
		m_Screen.SetStatusRow(PART_FUND, "#EL-Org_Fund", LF_MoneyFormat.FormatMoney(m_Page.m_iFund), "");

		string processing = "#LF-Org_NotOwned";
		if (m_Page.m_bProcessing)
			processing = "#LF-Org_Owned";
		m_Screen.SetStatusRow(PART_PROCESSING, "#LF-Org_Processing", processing, ProcessingAction(m_Page));

		string lease = "#LF-Org_LeaseFree";
		if (m_Page.m_iLease == LF_EOrgLease.OURS)
			lease = "#LF-Org_LeaseOurs";
		else if (m_Page.m_iLease == LF_EOrgLease.TAKEN)
			lease = "#LF-Org_LeaseTaken";
		m_Screen.SetStatusRow(PART_HIDEOUT, "#LF-Org_Hideout", lease, HideoutAction(m_Page));

		array<string> names = {};
		array<string> ranks = {};
		m_Page.SplitMembers(names, ranks);
		m_Screen.SetLine(PART_MEMBERS_LINE, WidgetManager.Translate("#LF-Org_MembersLine", names.Count()));
		m_Screen.ClearList(PART_MEMBERS);
		for (int i = 0; i < names.Count(); i++)
		{
			m_Screen.AddListRow(PART_MEMBERS, "OrgMember" + i.ToString(), names[i], ranks[i], "");
		}

		m_Screen.SetStatusRow(PART_MEMBERSHIP, "#LF-Org_Membership", m_Page.RankLabel(), MembershipAction(m_Page));
	}

	//------------------------------------------------------------------------------------------------
	protected void OnButton(string name)
	{
		SCR_ChimeraCharacter character = LF_Utils.GetLocalCharacter();
		if (!character || !m_Screen)
			return;

		if (name == PART_FOUND)
		{
			string typed = m_Screen.GetFieldText(PART_NAME).Trim();
			string refusal = LF_OrgRules.RefuseName(typed);
			if (!refusal.IsEmpty())
			{
				m_Screen.SetLine(PART_NOTICE, refusal);
				SCR_UISoundEntity.SoundEvent(SCR_SoundEvent.ERROR);
				return;
			}

			if (LF_MoneyUtils.GetCash(character) < LF_OrgConfig.FoundingFee())
			{
				m_Screen.SetLine(PART_NOTICE, "#LF-Org_NoFee");
				SCR_UISoundEntity.SoundEvent(SCR_SoundEvent.ERROR);
				return;
			}

			// The pencil on the page while the clerk takes the fee.
			SCR_UISoundEntity.SoundEvent(SCR_SoundEvent.SOUND_MAP_GADGET_MARKER_DRAW_START);
			character.LF_AskOrganizationCreate(typed);
			return;
		}

		if (name == PART_DEPOSIT || name == PART_WITHDRAW)
		{
			int amount = LF_PlayerInteraction.ParseAmount(m_Screen.GetFieldText(PART_AMOUNT));
			if (amount <= 0)
			{
				m_Screen.SetLine(PART_NOTICE, "#LF-Interact_BadAmount");
				SCR_UISoundEntity.SoundEvent(SCR_SoundEvent.ERROR);
				return;
			}

			if (name == PART_DEPOSIT)
				character.LF_AskOrganizationDeposit(amount);
			else
				character.LF_AskOrganizationWithdraw(amount);

			return;
		}

		if (name == PART_PROCESSING)
		{
			character.LF_AskOrganizationBuy(LF_EOrganizationUpgrade.PROCESSING_SLOT);
			return;
		}

		if (name == PART_HIDEOUT)
		{
			character.LF_AskOrganizationLease();
			return;
		}

		if (name == PART_MEMBERSHIP)
		{
			character.LF_AskOrganizationLeave();
			return;
		}

		LF_Debug.Info("Organizations", "ledger ignored button " + name);
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
		if (m_bRebuilding)
			return;

		Detach();

		if (s_Ledger == this)
			s_Ledger = null;

		SCR_UISoundEntity.SoundEvent(SCR_SoundEvent.SOUND_HUD_MAP_CLOSE);
		LF_Debug.Info("Organizations", "ledger closed");
	}
}
