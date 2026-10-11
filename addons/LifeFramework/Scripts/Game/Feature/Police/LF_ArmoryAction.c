//------------------------------------------------------------------------------------------------
//! Station armory. Draw gives the officer's rank kit. Promote is the higher-rank action.
//! Offer checks do not log; the server methods log refusals.
//------------------------------------------------------------------------------------------------
class LF_ArmoryDrawAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		return GetOwner() != null;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		if (!LF_PoliceRanks.CanDraw(LF_PoliceRanks.ResolveUid(user)))
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
			EL_Debug.Error("Ranks", "draw action refused: player has no account id");
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
}

//------------------------------------------------------------------------------------------------
class LF_RankPromoteAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		return GetOwner() != null;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		if (!LF_PoliceRanks.CanPromote(LF_PoliceRanks.ResolveUid(user)))
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
			EL_Debug.Error("Ranks", "promote action refused: player has no account id");
			return;
		}

		string targetUid = LF_PoliceRanks.FindLowerRankNearby(pUserEntity, LF_PoliceRanks.PROMOTE_RADIUS);
		if (targetUid.IsEmpty())
		{
			EL_Debug.Error("Ranks", "promote refused: no lower-rank officer nearby");
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
}
