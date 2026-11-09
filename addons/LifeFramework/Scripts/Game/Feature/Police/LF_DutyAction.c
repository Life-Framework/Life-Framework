//------------------------------------------------------------------------------------------------
//! "Clock in" / "Clock out" at the station duty desk. There is no faction screen: everyone is a
//! civilian, and a player on the police roster (LF_WhitelistManager) becomes police here.
//! The offer reads LF_AccountReplication (the server's account cache, or the character's
//! replicated copy on a client); the server re-checks the roster and swaps the inventories
//! through LF_DutyLocker. Feedback is the world first: the reach-and-grab gesture on the acting
//! client, the punch-clock click for everyone nearby, then a short toast.
class LF_DutyAction : ScriptedUserAction
{
	[Attribute("SOUND_RANGECP_STARTBUTTON", UIWidgets.EditBox, "Sound event on the desk's SCR_SoundDataComponent played when a card is punched")]
	protected string m_sPunchSound;

	[Attribute(LF_Faction.POLICE.ToString(), UIWidgets.ComboBox, "Duty role this desk clocks in: POLICE at the station, MEDIC at the hospital", enums: ParamEnumArray.FromEnum(LF_Faction))]
	protected LF_Faction m_eRole;

	//------------------------------------------------------------------------------------------------
	LF_Faction GetRole()
	{
		return m_eRole;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		int state = LF_AccountReplication.GetState(user);
		if (!LF_AccountStateBits.HasAccount(state))
		{
			SetCannotPerformReason("#LF-Duty_NoAccount");
			return false;
		}

		if (LF_AccountStateBits.IsOnDuty(state) && LF_AccountStateBits.GetFaction(state) != m_eRole)
		{
			SetCannotPerformReason("#LF-Duty_WrongDesk");
			return false;
		}

		// Only the police role has a roster; the medic desk is open to any registered player.
		if (m_eRole == LF_Faction.POLICE && !LF_AccountStateBits.IsOnDuty(state) && !LF_AccountStateBits.IsOnRoster(state))
		{
			SetCannotPerformReason("#LF-Duty_NotOnRoster");
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		// The acting client reaches for the clock; the server does the work.
		if (pUserEntity && pUserEntity == LF_Utils.GetLocalCharacter())
			PlayGesture(pUserEntity);

		if (!Replication.IsServer())
			return;

		LF_PlayerAccount account = GetPlayerAccount(pUserEntity);
		if (!account)
		{
			LF_Debug.Warn("Police", string.Format("duty desk refused: no account for %1", pUserEntity));
			return;
		}

		bool clockingIn = !account.IsOnDuty();
		string refusal;
		if (clockingIn)
			refusal = LF_DutyLocker.ClockIn(account, pUserEntity, m_eRole);
		else
			refusal = LF_DutyLocker.ClockOut(account, pUserEntity, m_eRole);

		if (!refusal.IsEmpty())
		{
			LF_Utils.NotifyEntity(pUserEntity, refusal, "#EL-Duty_Status", 4.0);
			return;
		}

		PlayPunch(pOwnerEntity, pUserEntity);

		string statusKey = "#LF-Duty_OffDuty";
		if (clockingIn)
			statusKey = "#LF-Duty_OnDuty";

		LF_Utils.NotifyEntity(pUserEntity, "#EL-Duty_Status_Change", "#EL-Duty_Status", 3.0, statusKey);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the punch clock clicks for every machine near the desk. A placed static prop has no
	//! usable RplId on a dedicated server, so the broadcast rides the officer's character and each
	//! machine finds the desk beside them.
	protected void PlayPunch(IEntity desk, IEntity user)
	{
		if (!desk || m_sPunchSound.IsEmpty())
			return;

		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(user);
		if (!character)
		{
			LF_Debug.Warn("Police", "punch clock sound skipped: the user is not a character");
			return;
		}

		character.LF_BroadcastStationSound("LF_DutyAction", m_sPunchSound);
	}

	//------------------------------------------------------------------------------------------------
	protected void PlayGesture(IEntity user)
	{
		CharacterControllerComponent controller = CharacterControllerComponent.Cast(user.FindComponent(CharacterControllerComponent));
		if (controller && controller.CanPlayItemGesture())
			controller.TryPlayItemGesture(EItemGesture.EItemGesturePickUp);
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		PlayerController pc = GetGame().GetPlayerController();
		if (!pc)
			return false;
		IEntity entity = pc.GetControlledEntity();
		if (!entity)
			return false;

		int state = LF_AccountReplication.GetState(entity);
		if (LF_AccountStateBits.IsOnDuty(state))
			outName = "#LF-Duty_ClockOut";
		else
			outName = "#LF-Duty_ClockIn";
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected LF_PlayerAccount GetPlayerAccount(IEntity user)
	{
		LF_PlayerAccountManager accountManager = LF_PlayerAccountManager.GetInstance();
		if (accountManager)
		{
			string playerUid = LF_Utils.GetPlayerUid(user);
			return accountManager.GetAccount(playerUid);
		}
		return null;
	}
};

//------------------------------------------------------------------------------------------------
//! Station prop sounds that every nearby player hears, sent through the acting character (a
//! replicated entity every nearby machine streams). Each machine plays the event from the prop
//! that carries the named action beside the character.
modded class SCR_ChimeraCharacter
{
	static const float LF_STATION_SOUND_RANGE = 3;

	//------------------------------------------------------------------------------------------------
	//! Server: play soundEvent from the nearest prop carrying actionClass, on every machine.
	void LF_BroadcastStationSound(string actionClass, string soundEvent)
	{
		Rpc(RpcDo_LF_StationSound, actionClass, soundEvent);
		RpcDo_LF_StationSound(actionClass, soundEvent);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Unreliable, RplRcver.Broadcast)]
	protected void RpcDo_LF_StationSound(string actionClass, string soundEvent)
	{
		// A dedicated server renders and hears nothing.
		if (RplSession.Mode() == RplMode.Dedicated)
			return;

		IEntity prop = LF_ActionSite.FindNear(GetOrigin(), LF_STATION_SOUND_RANGE, actionClass);
		if (!prop)
		{
			LF_Debug.Warn("Police", string.Format("station sound %1 skipped: no %2 prop near the character", soundEvent, actionClass));
			return;
		}

		SCR_SoundManagerModule.CreateAndPlayAudioSource(prop, soundEvent);
		LF_Debug.Info("Police", string.Format("station sound %1 played at %2", soundEvent, LF_Utils.GetPrefabName(prop)));
	}
}
