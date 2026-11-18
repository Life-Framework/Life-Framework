//------------------------------------------------------------------------------------------------
//! "Take a call" on the police dispatch board (#2479, slice of #234). An officer on duty takes the
//! oldest open call their rank may see (LF_PoliceCalls.OldestOpenFor) and the board puts a waypoint
//! on its site. The server validates everything on perform (duty, rank, standing at the board,
//! an open call) through LF_PoliceCalls.Take; the client's CanBeShown is only the prompt.
class LF_PoliceCallBoardAction : ScriptedUserAction
{
	static const string FEATURE = "PoliceCalls";
	static const string WAYPOINT_PREFIX = "police-call:";
	//! The report's own square size: a site filed by grid square is "#LF-Wanted_PlaceGrid XXX-ZZZ".
	static const float GRID_SQUARE = 100;	// lf-fixed: LF_WantedReports.GridPlace's square

	//------------------------------------------------------------------------------------------------
	//! The prompt: only an on-duty police officer sees it. A civilian, a medic or an off-duty officer does not.
	bool Offers(int accountState)
	{
		return LF_AccountStateBits.IsOnDutyPolice(accountState);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		if (!LF_Systems.IsEnabled("Police"))
			return false;

		return Offers(LF_AccountReplication.GetState(user));
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "#LF-Police_CallBoardTake";
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the officer takes the oldest open call their rank may see.
	//! \return The call now held by the officer, or null when none was taken (each refusal is logged).
	LF_PoliceCall ServerTake(string officerUid, int accountState)
	{
		if (!Replication.IsServer())
			return null;

		if (!Offers(accountState))
		{
			LF_Debug.Warn(FEATURE, string.Format("board take by %1 refused: not on duty", officerUid));
			return null;
		}

		LF_PoliceCall call = LF_PoliceCalls.OldestOpenFor(LF_AccountStateBits.GetPoliceRank(accountState));
		if (!call)
		{
			LF_Debug.Info(FEATURE, string.Format("board take by %1: no open call for this rank", officerUid));
			return null;
		}

		if (!LF_PoliceCalls.Take(call.m_iId, officerUid, accountState))
			return null;

		return call;
	}

	//------------------------------------------------------------------------------------------------
	//! Where a call's site text points: a grid square (its centre) or a place sign's title. A site
	//! named only by the map's location label has no point here; the call's stored position covers it (CallOrigin).
	//! \return false when the site resolves to no point.
	static bool SiteOrigin(string site, out vector origin)
	{
		if (site.IsEmpty())
			return false;

		string gridPrefix = LF_WantedReports.PLACE_GRID_KEY + " ";
		if (site.StartsWith(gridPrefix))
		{
			string square = site.Substring(gridPrefix.Length(), site.Length() - gridPrefix.Length());
			array<string> parts = {};
			square.Split("-", parts, true);
			if (parts.Count() != 2)
				return false;

			float x = (parts[0].ToInt() + 0.5) * GRID_SQUARE;
			float z = (parts[1].ToInt() + 0.5) * GRID_SQUARE;
			float y;
			if (GetGame() && GetGame().GetWorld())
				y = GetGame().GetWorld().GetSurfaceY(x, z);

			origin = Vector(x, y, z);
			return true;
		}

		array<IEntity> signs = {};
		LF_Places.FindAll(LF_SignComponent.PLACE_KIND, signs);
		foreach (IEntity entity : signs)
		{
			LF_SignComponent sign = LF_SignComponent.Cast(entity.FindComponent(LF_SignComponent));
			if (sign && sign.GetSign() && sign.GetSign().m_sTitle == site)
			{
				origin = entity.GetOrigin();
				return true;
			}
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! The call's site text. The first wanted report can post its call before the report's place is
	//! filed (LF_PoliceCalls.OnWantedChanged), so an empty site reads the suspect's newest report now.
	static string SiteOf(LF_PoliceCall call)
	{
		if (!call.m_sSite.IsEmpty())
			return call.m_sSite;

		array<ref LF_WantedReportRecord> reports = LF_WantedReports.GetFor(call.m_sSuspectUid);
		if (reports && reports.Count() > 0)
			return reports[reports.Count() - 1].m_sPlace;

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! Where a call points: the position its crime was filed at (#2578), else the site text's point.
	//! \return false when neither resolves to a point.
	static bool CallOrigin(LF_PoliceCall call, out vector origin)
	{
		if (!call)
			return false;

		if (call.m_bHasPosition)
		{
			origin = call.m_vPosition;
			return true;
		}

		return SiteOrigin(SiteOf(call), origin);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the waypoint on the officer's client at the call's position or site. \return true when one was sent.
	static bool SendWaypoint(int playerId, LF_PoliceCall call)
	{
		string site = SiteOf(call);
		vector origin;
		if (!CallOrigin(call, origin))
		{
			LF_Debug.Info(FEATURE, string.Format("call #%1 site '%2' has no point to mark", call.m_iId, site));
			return false;
		}

		LF_MapShareComponent channel = LF_MapShareComponent.OfPlayer(playerId);
		if (!channel)
			return false;

		channel.ServerJobWaypoint(WAYPOINT_PREFIX + call.m_iId.ToString(), LF_WantedBoard.ResolvePlace(site), Math.Round(origin[0]), Math.Round(origin[2]));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (LF_Systems.Refuses("Police", "LF_PoliceCallBoardAction"))
			return;

		if (!Replication.IsServer() || !pOwnerEntity || !pUserEntity)
			return;

		if (vector.Distance(pOwnerEntity.GetOrigin(), pUserEntity.GetOrigin()) > LF_PoliceConfig.BoardReadDistance())
		{
			LF_Debug.Warn(FEATURE, "board take refused: user is not at the board");
			return;
		}

		int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(pUserEntity);
		if (playerId <= 0)
		{
			LF_Debug.Error(FEATURE, "board take refused: no player behind the user");
			return;
		}

		LF_PoliceCall call = ServerTake(LF_Utils.GetPlayerUID(playerId), LF_AccountReplication.GetState(pUserEntity));
		if (!call)
		{
			LF_Utils.NotifyEntity(pUserEntity, "#LF-Police_CallNone", "#LF-Police_CallTitle", 4.0);
			return;
		}

		SendWaypoint(playerId, call);
		LF_Utils.NotifyEntity(pUserEntity, "#LF-Police_CallTaken", "#LF-Police_CallTitle", 5.0, LF_WantedBoard.ResolvePlace(SiteOf(call)));
	}
}
