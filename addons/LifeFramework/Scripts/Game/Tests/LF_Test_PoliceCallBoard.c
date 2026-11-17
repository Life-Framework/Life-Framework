// red-proof: `tools\cli test --no-build --red LF_Test_PoliceCallBoard` (2026-11-18): RED PROOF OK, with the take skipped the officer holds no call (`the officer holds the call (expected 'police-board-officer', got '')`). Green without the hook.

// tier: WORLD
//! police/call-board (#2479, parent #234): the police dispatch board carries a "Take a call" action. An
//! off-duty user is not offered it, an on-duty officer is; performing it on the server makes that officer
//! the holder of the oldest open call their rank may see, and a call above their rank stays open.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_PoliceCallBoard : LF_Test
{
	protected static const ResourceName BOARD_PREFAB = "{5BC90DB066B123D4}Prefabs/Comms/LF_DispatchBoard_Police.et";
	protected static const string SUSPECT_A = "police-board-suspect-a";
	protected static const string SUSPECT_B = "police-board-suspect-b";
	protected static const string OFFICER = "police-board-officer";

	override string GetName()
	{
		return "police/call-board";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_PoliceCalls.ResetStatics();

		IEntity board = LF_Utils.SpawnEntityPrefab(BOARD_PREFAB, "0 0 0");
		ctx.NotNull(board, "the police dispatch board spawns");
		if (!board)
			return;

		LF_PoliceCallBoardAction action;
		BaseActionsManagerComponent manager = BaseActionsManagerComponent.Cast(board.FindComponent(BaseActionsManagerComponent));
		ctx.NotNull(manager, "the board has an actions manager");
		if (manager)
		{
			array<BaseUserAction> actions = {};
			manager.GetActionsList(actions);
			foreach (BaseUserAction candidate : actions)
			{
				if (LF_PoliceCallBoardAction.Cast(candidate))
					action = LF_PoliceCallBoardAction.Cast(candidate);
			}
		}

		ctx.NotNull(action, "the police board carries the take-a-call action");
		if (!action)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(board);
			return;
		}

		int civilian = LF_AccountStateBits.Encode(true, LF_Faction.CIVILIAN, false, 0, 0);
		int offDuty = LF_AccountStateBits.Encode(true, LF_Faction.POLICE, false, 0, LF_PoliceRanks.SENIOR);
		int medic = LF_AccountStateBits.Encode(true, LF_Faction.MEDIC, true, 0, 0);
		int patrol = LF_AccountStateBits.Encode(true, LF_Faction.POLICE, true, 0, LF_PoliceRanks.PATROL);
		int senior = LF_AccountStateBits.Encode(true, LF_Faction.POLICE, true, 0, LF_PoliceRanks.SENIOR);

		ctx.False(action.Offers(0), "a user with no account is not offered the action");
		ctx.False(action.Offers(civilian), "a civilian is not offered the action");
		ctx.False(action.Offers(offDuty), "an off-duty officer is not offered the action");
		ctx.False(action.Offers(medic), "an on-duty medic is not offered the action");
		ctx.True(action.Offers(patrol), "an on-duty officer is offered the action");

		// Two open calls: the older one needs a senior, the newer one a patrol officer.
		LF_PoliceCall seniorCall = LF_PoliceCalls.Post(LF_PoliceCalls.KIND_FINE, SUSPECT_A, "test site", LF_PoliceRanks.SENIOR);
		LF_PoliceCall patrolCall = LF_PoliceCalls.Post(LF_PoliceCalls.KIND_FINE, SUSPECT_B, "test site", LF_PoliceRanks.PATROL);
		ctx.True(seniorCall != null && patrolCall != null, "two calls are posted");
		if (!seniorCall || !patrolCall)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(board);
			LF_PoliceCalls.ResetStatics();
			return;
		}

		ctx.True(action.ServerTake(OFFICER, offDuty) == null, "an off-duty take is refused on the server");
		ctx.True(action.ServerTake(OFFICER, civilian) == null, "a civilian's take is refused on the server");
		ctx.EqualStr("", seniorCall.m_sOfficerUid, "a refused take leaves the oldest call open");

		LF_PoliceCall first;
		if (!RedProof())
			first = action.ServerTake(OFFICER, patrol);

		// A patrol officer may not see the older senior call, so the take is the newer one.
		ctx.True(first == patrolCall, "a patrol officer takes the oldest call their rank may see");
		ctx.EqualStr(OFFICER, patrolCall.m_sOfficerUid, "the officer holds the call");
		ctx.EqualStr("", seniorCall.m_sOfficerUid, "the call above their rank stays open");
		ctx.True(action.ServerTake(OFFICER, patrol) == null, "nothing is left for a patrol officer");

		LF_PoliceCall second = action.ServerTake(OFFICER, senior);
		ctx.True(second == seniorCall, "a senior takes the call the patrol officer could not");

		// A grid site resolves to a point to mark; no text resolves to none.
		vector at;
		ctx.True(LF_PoliceCallBoardAction.SiteOrigin(LF_WantedReports.GridPlace(Vector(250, 0, 450)), at), "a grid site has a point to mark");
		ctx.Equal(250, Math.Round(at[0]), "the grid site's point is the square's centre (x)");
		ctx.Equal(450, Math.Round(at[2]), "the grid site's point is the square's centre (z)");
		ctx.False(LF_PoliceCallBoardAction.SiteOrigin("", at), "an empty site has no point");

		// A call posted before its report's place was filed reads the newest report's place when taken.
		LF_WantedReports.ResetStatics();
		LF_PoliceCall early = LF_PoliceCalls.Post(LF_PoliceCalls.KIND_FINE, SUSPECT_A + "-early", "", LF_PoliceRanks.PATROL);
		ctx.True(early != null, "a call with no site yet is posted");
		if (early)
		{
			ctx.EqualStr("", LF_PoliceCallBoardAction.SiteOf(early), "no report, no site");
			LF_WantedReports.Add(SUSPECT_A + "-early", LF_WantedReports.OFFENCE_ROBBERY, "", "", "Square", false);
			ctx.EqualStr("Square", LF_PoliceCallBoardAction.SiteOf(early), "the site is the suspect's newest report place");
		}

		LF_WantedReports.ResetStatics();

		SCR_EntityHelper.DeleteEntityAndChildren(board);
		LF_PoliceCalls.ResetStatics();
	}
}
