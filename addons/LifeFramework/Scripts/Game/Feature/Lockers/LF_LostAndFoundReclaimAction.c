//------------------------------------------------------------------------------------------------
//! "Reclaim your things (fee)" on the lost-and-found box (#3457). The engine runs PerformAction on the
//! server for the acting character; the server finds the box from that character's position and the owner
//! from their own record key (LF_LostAndFound.ReclaimNearest). On the server side of a listen host the
//! prompt is hidden from anyone with nothing waiting here; a dedicated client cannot know, so it shows the
//! prompt and the server refuses a stranger with a toast and moves nothing.
class LF_LostAndFoundReclaimAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		if (!LF_Systems.IsEnabled(LF_LockerBank.FEATURE))
			return false;

		if (!Replication.IsServer() || !user)
			return true;

		LF_LostAndFoundBoxComponent box = LF_Component<LF_LostAndFoundBoxComponent>.Find(GetOwner());
		if (!box)
			return true;

		return LF_LostAndFound.HasThings(LF_Utils.GetPlayerUID(user), box.GetSiteId());
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		// The fee is read on the server only, so no cash check here: the server refuses short cash with a toast.
		return user != null;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (LF_Systems.Refuses(LF_LockerBank.FEATURE, "LF_LostAndFoundReclaimAction"))
			return;

		if (!Replication.IsServer())
			return;

		if (!pUserEntity)
		{
			LF_Debug.Error(LF_LockerBank.FEATURE, "reclaim action performed without a character");
			return;
		}

		LF_ELostAndFoundResult result = LF_LostAndFound.ReclaimNearest(pUserEntity);
		LF_Utils.NotifyEntity(pUserEntity, LF_LostAndFound.ResultKey(result), "#LF-LostAndFound_Title", 4.0);
	}
}
