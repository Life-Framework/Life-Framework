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
	//! Server: the punch clock clicks for every machine near the desk (vanilla inventory sound
	//! broadcast, the same path duty pay uses for the cash).
	protected void PlayPunch(IEntity desk, IEntity user)
	{
		if (!desk || m_sPunchSound.IsEmpty())
			return;

		SCR_InventoryStorageManagerComponent inventory = EL_Component<SCR_InventoryStorageManagerComponent>.Find(user);
		RplId deskId = EL_NetworkUtils.GetRplId(desk);
		if (!inventory || deskId == RplId.Invalid())
		{
			EL_Debug.Warn("Police", "punch clock sound skipped: no inventory or the desk has no RplComponent");
			return;
		}

		inventory.RpcAsk_PlaySound(deskId, m_sPunchSound);
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
