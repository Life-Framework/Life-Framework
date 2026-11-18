//------------------------------------------------------------------------------------------------
//! The held-site rows of the org ledger (#2834, gang war #1022): the sites an organisation leases,
//! its base first, then each captured site, marked as such. The server builds the rows from the
//! hideout lease table (LF_Hideout.SitesOf / IsCaptured) and sends them to the ledger over its own
//! RPC; nothing the client says is trusted. A lease that changes (take, give up, lapse, capture)
//! refreshes the open ledger of every online member of the organisation it touches, through the
//! LF_OnHideoutLeaseChanged event this class listens to. A late joiner has no ledger open: the page
//! asks on open and gets the rows in the same answer as the rest.
//!
//! Row format: "<hideout id>|<0 base | 1 captured>" rows joined by ";". Names are never sent: the
//! client prints each site through LF_SiteCaptureHud.SiteLabel (the pack's LF-SiteName_<id> row).
class LF_OrgHeldSites
{
	static const string FEATURE = "Organizations";
	static const string ROW_SEPARATOR = ";";
	static const string FIELD_SEPARATOR = "|";

	protected static ref LF_OrgHeldSitesListener s_Listener;
	protected static int s_iRefreshes;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		if (s_Listener)
			LF_EventBus.Unsubscribe(LF_OnHideoutLeaseChanged, s_Listener);

		s_Listener = null;
		s_iRefreshes = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: listen to lease changes (idempotent, also after a bus reset). Called when the game mode starts.
	static bool Install()
	{
		if (!Replication.IsServer())
			return false;

		if (s_Listener)
			LF_EventBus.Unsubscribe(LF_OnHideoutLeaseChanged, s_Listener);

		s_Listener = new LF_OrgHeldSitesListener();
		return LF_EventBus.Subscribe(LF_OnHideoutLeaseChanged, s_Listener);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the rows an organisation's ledger shows: its base lease(s) first, then its captured
	//! sites, each group in id order. "" when it leases nothing.
	static string RowsFor(string orgId)
	{
		array<string> bases = {};
		array<string> captures = {};
		foreach (string hideoutId : LF_Hideout.SitesOf(orgId))
		{
			if (LF_Hideout.IsCaptured(hideoutId))
				captures.Insert(hideoutId);
			else
				bases.Insert(hideoutId);
		}

		bases.Sort();
		captures.Sort();

		string rows;
		foreach (string baseId : bases)
		{
			rows = Append(rows, baseId, false);
		}

		foreach (string capturedId : captures)
		{
			rows = Append(rows, capturedId, true);
		}

		return rows;
	}

	//------------------------------------------------------------------------------------------------
	protected static string Append(string rows, string hideoutId, bool captured)
	{
		string flag = "0";
		if (captured)
			flag = "1";

		string row = hideoutId + FIELD_SEPARATOR + flag;
		if (rows.IsEmpty())
			return row;

		return rows + ROW_SEPARATOR + row;
	}

	//------------------------------------------------------------------------------------------------
	//! Client or test: the rows split into site ids and their captured flags, in row order. A row
	//! without both fields is skipped.
	static void Split(string rows, notnull array<string> ids, notnull array<bool> captured)
	{
		ids.Clear();
		captured.Clear();
		if (rows.IsEmpty())
			return;

		array<string> parts = {};
		rows.Split(ROW_SEPARATOR, parts, true);
		foreach (string row : parts)
		{
			array<string> fields = {};
			row.Split(FIELD_SEPARATOR, fields, false);
			if (fields.Count() < 2 || fields[0].IsEmpty())
				continue;

			ids.Insert(fields[0]);
			captured.Insert(fields[1] == "1");
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The key of the mark printed beside a row.
	static string MarkKey(bool captured)
	{
		if (captured)
			return "#LF-Org_SiteCaptured";

		return "#LF-Org_SiteBase";
	}

	//------------------------------------------------------------------------------------------------
	//! The name a row prints: the pack's `LF-SiteName_<hideout id>` row, else a neutral line (the
	//! yard lease for a base, "Contested ground" for a captured site). Never a core place name.
	static string Label(string hideoutId, bool captured)
	{
		// Built from two parts: the key is per hideout id and only a pack declares it (never a core row).
		string key = "#LF" + "-SiteName_" + hideoutId;
		string text = WidgetManager.Translate(key);
		if (!text.IsEmpty() && text != key && text != key.Substring(1, key.Length() - 1))
			return text;

		if (captured)
			return WidgetManager.Translate("#LF-SiteCapture_Fallback");

		return WidgetManager.Translate("#LF-Org_Hideout");
	}

	//------------------------------------------------------------------------------------------------
	//! Server: send the held-site rows to every online member of the organisation (their ledger, when
	//! one is open, repaints; otherwise the client drops it).
	//! \return How many members were sent the rows.
	static int Refresh(string orgId)
	{
		if (orgId.IsEmpty() || !Replication.IsServer() || !GetGame() || !GetGame().GetPlayerManager())
			return 0;

		array<int> playerIds = {};
		GetGame().GetPlayerManager().GetPlayers(playerIds);
		int sent;
		foreach (int playerId : playerIds)
		{
			SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId));
			if (!character)
				continue;

			LF_Organization org = LF_OrganizationManager.GetByCharacter(LF_OrganizationPlayer.CharacterId(character));
			if (!org || org.GetId() != orgId)
				continue;

			character.LF_PushOrganizationHeldSites();
			sent++;
		}

		s_iRefreshes++;
		LF_Debug.Info(FEATURE, string.Format("held sites of %1 refreshed for %2 member(s)", orgId, sent));
		return sent;
	}

	//------------------------------------------------------------------------------------------------
	//! Lease-change events handled since the last reset (tests read it).
	static int Refreshes()
	{
		return s_iRefreshes;
	}
}

//------------------------------------------------------------------------------------------------
//! Bus subscriber: a lease changed, so the organisation it touches gets fresh held-site rows.
class LF_OrgHeldSitesListener : LF_EventSubscriber
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "OrgHeldSites";
	}

	//------------------------------------------------------------------------------------------------
	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnHideoutLeaseChanged changed = LF_OnHideoutLeaseChanged.Cast(busEvent);
		if (!changed)
			return false;

		LF_OrgHeldSites.Refresh(changed.m_sOrgId);
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_OrgHeldSites's statics (LF_StaticReset finds it by reflection).
class LF_OrgHeldSites_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_OrgHeldSites";
	}

	override void Apply()
	{
		LF_OrgHeldSites.ResetStatics();
	}
}
