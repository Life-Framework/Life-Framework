//------------------------------------------------------------------------------------------------
//! Payphone entry: "Describe the suspect's clothing / vehicle / plate". One action per thing seen
//! (a phone carries one entry per preset). The server finds the caller's own witness record; the
//! client names only the field. What the caller could see comes from the server (LF_Witness).
class LF_WitnessPayphoneAction : ScriptedUserAction
{
	[Attribute("clothing", UIWidgets.EditBox, "What this entry reports: clothing, vehicle or plate")]
	protected string m_sField;

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer() || !pUserEntity)
			return;

		string uid = LF_Utils.GetPlayerUID(pUserEntity);
		LF_EWitnessField field;
		if (!ParseField(m_sField, field) || uid.IsEmpty())
		{
			LF_Debug.Error(LF_Witness.FEATURE, string.Format("payphone witness action has bad field '%1'", m_sField));
			return;
		}

		LF_EWitnessResult result = LF_Witness.Give(LF_Witness.SceneFor(uid, System.GetTickCount()), uid, field, "payphone");
		LF_Utils.NotifyEntity(pUserEntity, RefusalKey(result), "#LF-Payphone_Title", 4.0);
	}

	//------------------------------------------------------------------------------------------------
	static bool ParseField(string name, out LF_EWitnessField field)
	{
		if (name == "clothing")
			field = LF_EWitnessField.CLOTHING;
		else if (name == "vehicle")
			field = LF_EWitnessField.VEHICLE;
		else if (name == "plate")
			field = LF_EWitnessField.PLATE;
		else
			return false;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Toast key for a result; OK reads as the statement being taken down.
	static string RefusalKey(LF_EWitnessResult result)
	{
		switch (result)
		{
			case LF_EWitnessResult.OK:
				return "#LF-Witness_Noted";
			case LF_EWitnessResult.ALREADY_GIVEN:
				return "#LF-Witness_AlreadyGiven";
			case LF_EWitnessResult.NOT_SEEN:
				return "#LF-Witness_NotSeen";
		}

		return "#LF-Witness_NothingSeen";
	}
}

//------------------------------------------------------------------------------------------------
//! "Ask what you saw" on a town NPC, for on-duty police. The server checks the officer is on duty
//! and near, finds the NPC's own witness record and rolls its accuracy; an NPC that stood out of
//! range of every recent crime says it saw nothing.
class LF_WitnessAskAction : ScriptedUserAction
{
	protected static const float REACH = 3.0;

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		if (!user || !GetOwner() || vector.Distance(user.GetOrigin(), GetOwner().GetOrigin()) > REACH)
			return false;

		return LF_AccountStateBits.IsOnDutyPolice(LF_AccountReplication.GetState(user));
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer() || !pOwnerEntity || !pUserEntity)
			return;

		LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();
		PlayerManager players = GetGame().GetPlayerManager();
		if (!accounts || !players)
			return;

		int playerId = players.GetPlayerIdFromControlledEntity(pUserEntity);
		if (!LF_PoliceUtils.IsOnDutyOfficer(accounts.GetFromCache(playerId)) || vector.Distance(pUserEntity.GetOrigin(), pOwnerEntity.GetOrigin()) > REACH + 1)
		{
			LF_Debug.Warn(LF_Witness.FEATURE, string.Format("question refused for player %1: not an on-duty officer in reach", playerId));
			return;
		}

		array<float> rolls = {Math.RandomFloat01(), Math.RandomFloat01(), Math.RandomFloat01()};
		string key = LF_Witness.NpcKey(pOwnerEntity);
		LF_WitnessScene scene = LF_Witness.SceneFor(key, System.GetTickCount());
		int said = LF_Witness.Ask(scene, key, rolls, "officer");
		LF_Utils.NotifyEntity(pUserEntity, LF_Witness.AskKey(scene != null, said), "#LF-Witness_Title", 5.0);
	}
}

//------------------------------------------------------------------------------------------------
//! "Ask what you saw" in the player interaction menu (role slot "witness") for on-duty police,
//! facing a player who stood in sight of a recent crime. The witness says everything it could see.
class LF_WitnessRole : LF_PlayerRoleHandler
{
	static const string SLOT = "witness";

	//------------------------------------------------------------------------------------------------
	//! Every machine, once per world (LF_GameModeRoleplay.OnGameStart). Idempotent.
	static void Register()
	{
		if (LF_WitnessRole.Cast(LF_PlayerInteraction.GetRole(SLOT)))
			return;

		LF_PlayerInteraction.RegisterRole(SLOT, new LF_WitnessRole());
	}

	//------------------------------------------------------------------------------------------------
	override string GetActionName()
	{
		return "#LF-Witness_Ask";
	}

	//------------------------------------------------------------------------------------------------
	override bool CanOffer(IEntity user, IEntity target)
	{
		return LF_AccountStateBits.IsOnDutyPolice(LF_AccountReplication.GetState(user));
	}

	//------------------------------------------------------------------------------------------------
	override void Perform(IEntity user, IEntity target)
	{
		LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();
		if (!accounts || !LF_PoliceUtils.IsOnDutyOfficer(accounts.GetFromCache(LF_Utils.GetPlayerUID(user))))
			return;

		string key = LF_Utils.GetPlayerUID(target);
		LF_WitnessScene scene = LF_Witness.SceneFor(key, System.GetTickCount());
		int said = LF_Witness.Ask(scene, key, null, "officer");
		LF_Utils.NotifyEntity(user, LF_Witness.AskKey(scene != null, said), "#LF-Witness_Title", 5.0);
	}
}
