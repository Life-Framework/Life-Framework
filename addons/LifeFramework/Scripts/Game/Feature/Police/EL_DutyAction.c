//------------------------------------------------------------------------------------------------
//! "Clock in" / "Clock out" at the station duty desk. There is no faction screen: everyone is a
//! civilian, and a player on the police roster (EL_WhitelistManager) becomes police here.
//! The offer reads EL_AccountReplication (the server's account cache, or the character's
//! replicated copy on a client); the server re-checks the roster and swaps the inventories
//! through LF_DutyLocker. Feedback is the world first: the reach-and-grab gesture on the acting
//! client, the punch-clock click for everyone nearby, then a short toast.
class EL_DutyAction : ScriptedUserAction
{
	[Attribute("SOUND_RANGECP_STARTBUTTON", UIWidgets.EditBox, "Sound event on the desk's SCR_SoundDataComponent played when a card is punched")]
	protected string m_sPunchSound;

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		int state = EL_AccountReplication.GetState(user);
		if (!EL_AccountStateBits.HasAccount(state))
		{
			SetCannotPerformReason("#LF-Duty_NoAccount");
			return false;
		}

		if (!EL_AccountStateBits.IsOnDuty(state) && !EL_AccountStateBits.IsOnRoster(state))
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
		if (pUserEntity && pUserEntity == EL_Utils.GetLocalCharacter())
			PlayGesture(pUserEntity);

		if (!Replication.IsServer())
			return;

		EL_PlayerAccount account = GetPlayerAccount(pUserEntity);
		if (!account)
		{
			EL_Debug.Warn("Police", string.Format("duty desk refused: no account for %1", pUserEntity));
			return;
		}

		bool clockingIn = !account.IsOnDuty();
		string refusal;
		if (clockingIn)
			refusal = LF_DutyLocker.ClockIn(account, pUserEntity);
		else
			refusal = LF_DutyLocker.ClockOut(account, pUserEntity);

		if (!refusal.IsEmpty())
		{
			EL_Utils.NotifyEntity(pUserEntity, refusal, "#EL-Duty_Status", 4.0);
			return;
		}

		PlayPunch(pOwnerEntity, pUserEntity);

		string statusKey = "#LF-Duty_OffDuty";
		if (clockingIn)
			statusKey = "#LF-Duty_OnDuty";

		EL_Utils.NotifyEntity(pUserEntity, "#EL-Duty_Status_Change", "#EL-Duty_Status", 3.0, statusKey);
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
			EL_Debug.Warn("Police", "punch clock sound skipped: the user is not a character");
			return;
		}

		character.LF_BroadcastStationSound("EL_DutyAction", m_sPunchSound);
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

		int state = EL_AccountReplication.GetState(entity);
		if (EL_AccountStateBits.IsOnDuty(state))
			outName = "#LF-Duty_ClockOut";
		else
			outName = "#LF-Duty_ClockIn";
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected EL_PlayerAccount GetPlayerAccount(IEntity user)
	{
		EL_PlayerAccountManager accountManager = EL_PlayerAccountManager.GetInstance();
		if (accountManager)
		{
			string playerUid = EL_Utils.GetPlayerUid(user);
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

		IEntity prop = EL_ActionSite.FindNear(GetOrigin(), LF_STATION_SOUND_RANGE, actionClass);
		if (!prop)
		{
			EL_Debug.Warn("Police", string.Format("station sound %1 skipped: no %2 prop near the character", soundEvent, actionClass));
			return;
		}

		SCR_SoundManagerModule.CreateAndPlayAudioSource(prop, soundEvent);
		EL_Debug.Info("Police", string.Format("station sound %1 played at %2", soundEvent, EL_Utils.GetPrefabName(prop)));
	}
}
