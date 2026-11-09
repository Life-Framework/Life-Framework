//------------------------------------------------------------------------------------------------
//! Station armory. Draw gives the officer's rank kit. Promote is the higher-rank action.
//! Offer checks do not log; the server methods log refusals.
//------------------------------------------------------------------------------------------------
class LF_ArmoryDrawAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		if (!GetOwner())
			return false;

		return LF_ArmoryDrawAction.ShownTo(LF_AccountReplication.GetState(user));
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		if (!OfferDraw(user))
		{
			SetCannotPerformReason("#EL-Rank_DrawDenied");
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;

		string uid = LF_PoliceRanks.ResolveUid(pUserEntity);
		if (uid.IsEmpty())
		{
			LF_Debug.Error("Ranks", "draw action refused: player has no account id");
			return;
		}

		LF_PoliceRanks.Draw(uid, pUserEntity);
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "#EL-Rank_Draw";
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Station armory actions are shown to on-duty police only; a civilian never sees them.
	//! Reads the replicated account state, so the client and the server agree. The server
	//! still refuses a draw or promote in PerformAction.
	static bool ShownTo(int accountState)
	{
		return LF_AccountStateBits.IsOnDutyPolice(accountState);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the account rule. Client: the same rule on the replicated account state.
	protected bool OfferDraw(IEntity user)
	{
		if (Replication.IsServer())
			return LF_PoliceRanks.CanDraw(LF_PoliceRanks.ResolveUid(user));

		int state = LF_AccountReplication.GetState(user);
		return LF_AccountStateBits.IsOnDutyPolice(state) && LF_PoliceRanks.IsRankUsable(LF_AccountStateBits.GetPoliceRank(state));
	}
}

//------------------------------------------------------------------------------------------------
class LF_RankPromoteAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		if (!GetOwner())
			return false;

		return LF_ArmoryDrawAction.ShownTo(LF_AccountReplication.GetState(user));
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		if (!OfferPromote(user))
		{
			SetCannotPerformReason("#EL-Rank_PromoteDenied");
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;

		string promoterUid = LF_PoliceRanks.ResolveUid(pUserEntity);
		if (promoterUid.IsEmpty())
		{
			LF_Debug.Error("Ranks", "promote action refused: player has no account id");
			return;
		}

		string targetUid = LF_PoliceRanks.FindLowerRankNearby(pUserEntity, LF_PoliceRanks.PROMOTE_RADIUS);
		if (targetUid.IsEmpty())
		{
			LF_Debug.Error("Ranks", "promote refused: no lower-rank officer nearby");
			return;
		}

		LF_PoliceRanks.Promote(promoterUid, targetUid);
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "#EL-Rank_Promote";
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the account rule. Client: the same rule on the replicated account state.
	protected bool OfferPromote(IEntity user)
	{
		if (Replication.IsServer())
			return LF_PoliceRanks.CanPromote(LF_PoliceRanks.ResolveUid(user));

		int state = LF_AccountReplication.GetState(user);
		return LF_AccountStateBits.IsOnDutyPolice(state) && LF_AccountStateBits.GetPoliceRank(state) > LF_PoliceRanks.PATROL;
	}
}
