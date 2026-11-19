//------------------------------------------------------------------------------------------------
//! What a garbage-round site is (#3325): the depot (take the truck) or the dump (tip the load).
enum LF_EGarbageSite
{
	DEPOT = 0,
	DUMP = 1
}

//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "LifeFramework/Feature/Jobs", description: "Garbage round depot or dump: registers in LF_Places so the round finds it by place, never by name.")]
class LF_GarbageSiteComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class LF_GarbageSiteComponent : ScriptComponent
{
	[Attribute("0", UIWidgets.ComboBox, "Site kind", "", ParamEnumArray.FromEnum(LF_EGarbageSite))]
	protected LF_EGarbageSite m_eSite;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (!GetGame().InPlayMode())
			return;

		string kind = LF_GarbageRound.PLACE_DEPOT;
		if (m_eSite == LF_EGarbageSite.DUMP)
			kind = LF_GarbageRound.PLACE_DUMP;

		LF_Places.Register(kind, string.Format("garbage_%1", owner.GetID()), owner);
	}
}

//------------------------------------------------------------------------------------------------
//! Shared by the three actions: the toast for a refused step, in the player's own words.
class LF_GarbageToast
{
	//------------------------------------------------------------------------------------------------
	static void Refused(IEntity user, LF_EGarbageResult result)
	{
		string key = LF_GarbageRound.RefusalKey(result);
		if (!key.IsEmpty())
			LF_Utils.NotifyEntity(user, key, "#LF-Garbage_Title", 4.0);
	}
}

//------------------------------------------------------------------------------------------------
//! At the depot: "Take the garbage round" opens the round and hands out the company truck.
class LF_GarbageStartAction : ScriptedUserAction
{
	override bool CanBeShownScript(IEntity user)
	{
		return LF_Systems.IsEnabled(LF_CivilianJob.SYSTEM);
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;

		if (LF_Systems.Refuses(LF_CivilianJob.SYSTEM, "garbage round start"))
			return;

		LF_EGarbageResult result = LF_GarbageRound.Start(pUserEntity, LF_Utils.GetPlayerUid(pUserEntity), pOwnerEntity);
		if (result == LF_EGarbageResult.OK)
			LF_Utils.NotifyEntity(pUserEntity, "#LF-Garbage_Started", "#LF-Garbage_Title", 6.0);
		else
			LF_GarbageToast.Refused(pUserEntity, result);
	}
}

//------------------------------------------------------------------------------------------------
//! At a bin: "Empty bin" moves a bag onto the truck and deletes the bin's contents. Shown only on a round.
class LF_GarbageEmptyBinAction : ScriptedUserAction
{
	override bool CanBeShownScript(IEntity user)
	{
		if (!Replication.IsServer())
			return true;

		return LF_GarbageRound.HasRound(LF_Utils.GetPlayerUid(user));
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;

		if (LF_Systems.Refuses(LF_CivilianJob.SYSTEM, "garbage round bin"))
			return;

		LF_EGarbageResult result = LF_GarbageRound.EmptyBin(pUserEntity, LF_Utils.GetPlayerUid(pUserEntity), pOwnerEntity);
		if (result != LF_EGarbageResult.OK)
			LF_GarbageToast.Refused(pUserEntity, result);
	}
}

//------------------------------------------------------------------------------------------------
//! At the dump: "Tip the load" ends the round and pays through the job cash path.
class LF_GarbageTipAction : ScriptedUserAction
{
	override bool CanBeShownScript(IEntity user)
	{
		return LF_Systems.IsEnabled(LF_CivilianJob.SYSTEM);
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;

		if (LF_Systems.Refuses(LF_CivilianJob.SYSTEM, "garbage round tip"))
			return;

		int paid;
		LF_EGarbageResult result = LF_GarbageRound.Tip(pUserEntity, LF_Utils.GetPlayerUid(pUserEntity), pOwnerEntity, paid);
		if (result == LF_EGarbageResult.OK)
			LF_Utils.NotifyEntity(pUserEntity, "#LF-Garbage_Paid", "#LF-Garbage_Title", 6.0, paid.ToString());
		else
			LF_GarbageToast.Refused(pUserEntity, result);
	}
}
