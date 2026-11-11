//------------------------------------------------------------------------------------------------
//! An on-duty officer searches a back room and seizes its stock as evidence (#600). Offer checks
//! read the replicated duty bit and the room's replicated config; the raid runs on the server,
//! which re-checks duty against the real account, the reach and the shutter
//! (LF_ShopComponent.Raid). The shutter is server state: every later buy or sell, from any
//! client including a late joiner, is refused there, so no client holds a stale open sheet.
class LF_BackRoomSearchAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		LF_ShopComponent shop = LF_ShopComponent.Cast(GetOwner().FindComponent(LF_ShopComponent));
		return shop && shop.IsBackRoom() && LF_PlayerInteraction.IsOnDutyPolice(user);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		if (!LF_PlayerInteraction.IsOnDutyPolice(user))
		{
			SetCannotPerformReason("#EL-Confiscate_OnDutyOnly");
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer() || !pOwnerEntity || !pUserEntity)
			return;

		LF_ShopComponent shop = LF_ShopComponent.Cast(pOwnerEntity.FindComponent(LF_ShopComponent));
		if (!shop)
		{
			LF_Debug.Error("Shop", "back room search: entity has no LF_ShopComponent");
			return;
		}

		bool onDuty = LF_PoliceUtils.IsOnDutyOfficer(LF_PoliceUtils.GetAccountForEntity(pUserEntity));
		int seized;
		LF_EBackRoomRefusal refusal = shop.Raid(pUserEntity, onDuty, GetGame().GetWorld().GetWorldTime(), seized);
		if (refusal == LF_EBackRoomRefusal.NONE)
			LF_Utils.NotifyEntity(pUserEntity, "#LF-BackRoom_Seized", "#EL-Police_Title", 3.0);
		else if (refusal == LF_EBackRoomRefusal.ALREADY_SHUT)
			LF_Utils.NotifyEntity(pUserEntity, "#LF-BackRoom_AlreadyShut", "#EL-Police_Title", 3.0);
	}
}
