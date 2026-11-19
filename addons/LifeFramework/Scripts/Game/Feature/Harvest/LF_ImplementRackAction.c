// LF_ImplementRackAction.c: the implement rack's two prompts, "Take out reaper" and "Put back reaper" (#3295).
// The server reads the user, checks the rack reach and the character's owned list (LF_OwnedImplements), does the
// hitch or the unhitch and toasts the outcome. The prompts always show: the owned list lives on the server, so a
// client cannot tell whether the player owns one, and a player who does not is told so by the server.

enum LF_EImplementRackMode
{
	TAKE_OUT,
	PUT_BACK
}

class LF_ImplementRackAction : ScriptedUserAction
{
	//! How far the user may stand from the rack and still use it.
	static const float USE_REACH_M = 4; // lf-fixed: the rack prompt's reach (#3295)

	[Attribute("0", UIWidgets.ComboBox, "What this prompt does at the rack", "", ParamEnumArray.FromEnum(LF_EImplementRackMode))]
	protected LF_EImplementRackMode m_eMode;

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		return user != null;
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		if (m_eMode == LF_EImplementRackMode.PUT_BACK)
			outName = "#LF-Harvest_RackPutBack";
		else
			outName = "#LF-Harvest_RackTakeOut";

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer() || !pOwnerEntity || !pUserEntity)
			return;

		PlayerManager playerManager = GetGame().GetPlayerManager();
		int playerId;
		if (playerManager)
			playerId = playerManager.GetPlayerIdFromControlledEntity(pUserEntity);

		if (vector.Distance(pOwnerEntity.GetOrigin(), pUserEntity.GetOrigin()) > USE_REACH_M)
		{
			LF_Debug.Warn(LF_OwnedImplements.FEATURE, "rack action refused: the player is not at the rack");
			return;
		}

		if (!LF_RateLimit.AllowCharacter(pUserEntity, LF_ERequestKind.SPAWN, "implement rack"))
			return;

		string key = LF_Utils.GetPlayerUID(pUserEntity);
		string id = LF_OwnedImplements.ID_REAPER;
		vector at = pOwnerEntity.GetOrigin();
		if (m_eMode == LF_EImplementRackMode.PUT_BACK)
			PutBack(playerId, key, id, at);
		else
			TakeOut(playerId, key, id, at);
	}

	//------------------------------------------------------------------------------------------------
	protected void TakeOut(int playerId, string key, string id, vector at)
	{
		IEntity implement;
		LF_EImplementResult result = LF_OwnedImplements.TakeOut(key, id, at, implement);
		if (result == LF_EImplementResult.OK)
		{
			Toast(playerId, "#LF-Harvest_RackTakenOut", LF_ENotificationType.SUCCESS);
			return;
		}

		Toast(playerId, RefusalKey(result), LF_ENotificationType.WARNING);
	}

	//------------------------------------------------------------------------------------------------
	protected void PutBack(int playerId, string key, string id, vector at)
	{
		int dumpedKg;
		LF_EImplementResult result = LF_OwnedImplements.PutBack(key, id, at, dumpedKg);
		if (result != LF_EImplementResult.OK)
		{
			Toast(playerId, RefusalKey(result), LF_ENotificationType.WARNING);
			return;
		}

		if (dumpedKg > 0)
			LF_NotificationManagerComponent.NotifyPlayer(playerId, "#LF-Harvest_RackPutBack", "#LF-Harvest_RackDumped", 5.0, LF_ENotificationType.WARNING, dumpedKg.ToString());
		else
			Toast(playerId, "#LF-Harvest_RackPutBackDone", LF_ENotificationType.SUCCESS);
	}

	//------------------------------------------------------------------------------------------------
	//! The toast line for a refusal.
	static string RefusalKey(LF_EImplementResult result)
	{
		switch (result)
		{
			case LF_EImplementResult.NOT_OWNED: return "#LF-Harvest_RackNotOwned";
			case LF_EImplementResult.ALREADY_OUT: return "#LF-Harvest_RackAlreadyOut";
			case LF_EImplementResult.NOT_OUT: return "#LF-Harvest_RackNotOut";
			case LF_EImplementResult.NO_VEHICLE: return "#LF-Harvest_RackNoVehicle";
			case LF_EImplementResult.VEHICLE_FAR: return "#LF-Harvest_RackVehicleFar";
		}

		return "#LF-Harvest_RackFailed";
	}

	//------------------------------------------------------------------------------------------------
	protected void Toast(int playerId, string message, LF_ENotificationType type)
	{
		string title = "#LF-Harvest_RackTakeOut";
		if (m_eMode == LF_EImplementRackMode.PUT_BACK)
			title = "#LF-Harvest_RackPutBack";

		LF_NotificationManagerComponent.NotifyPlayer(playerId, title, message, 4.0, type);
	}
}
