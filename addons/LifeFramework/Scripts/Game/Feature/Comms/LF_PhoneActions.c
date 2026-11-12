//------------------------------------------------------------------------------------------------
//! The telephone actions (#147). Each one shows only when it applies, read from the phone's
//! replicated line (LF_PhoneLineComponent); the server decides everything again and refuses with
//! a toast (LF_PhoneCalls). The client only reaches for the receiver.
class LF_PhoneActionBase : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	//! Player id of the character using the action, 0 when it is not a player's. The same id on
	//! every machine, so it compares with the line's talker anywhere.
	protected static int PlayerIdOf(IEntity user)
	{
		if (!user || !GetGame() || !GetGame().GetPlayerManager())
			return 0;

		return GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(user);
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!pOwnerEntity || !pUserEntity)
		{
			LF_Debug.Error(LF_PhoneCalls.FEATURE, "phone action performed without a phone or a user");
			return;
		}

		if (pUserEntity == LF_Utils.GetLocalCharacter())
		{
			CharacterControllerComponent controller = CharacterControllerComponent.Cast(pUserEntity.FindComponent(CharacterControllerComponent));
			if (controller && controller.CanPlayItemGesture())
				controller.TryPlayItemGesture(EItemGesture.EItemGesturePickUp);
		}

		if (!Replication.IsServer())
			return;

		ServerPerform(pOwnerEntity, pUserEntity);
	}

	//------------------------------------------------------------------------------------------------
	protected void ServerPerform(IEntity phone, IEntity user);
}

//------------------------------------------------------------------------------------------------
//! Lift the receiver and ring a service's station telephone (on a payphone).
class LF_PhoneDialAction : LF_PhoneActionBase
{
	[Attribute("police", UIWidgets.EditBox, "Service key from Configs/Comms/LF_ServiceCalls.conf whose station telephone rings")]
	protected string m_sService;

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		int talker;
		return LF_PhoneLineComponent.StateAt(GetOwner(), talker) == LF_EPhoneLineState.IDLE;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		if (LF_MoneyUtils.GetCash(user) < LF_ServiceCallsConfig.PayphoneCost())
		{
			SetCannotPerformReason("#LF-Payphone_NoCoin");
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override protected void ServerPerform(IEntity phone, IEntity user)
	{
		LF_PhoneCalls.ServerDial(user, phone, m_sService);
	}
}

//------------------------------------------------------------------------------------------------
//! Pick up a ringing telephone.
class LF_PhoneAnswerAction : LF_PhoneActionBase
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		int talker;
		return LF_PhoneLineComponent.StateAt(GetOwner(), talker) == LF_EPhoneLineState.RINGING;
	}

	//------------------------------------------------------------------------------------------------
	override protected void ServerPerform(IEntity phone, IEntity user)
	{
		LF_PhoneCalls.ServerAnswer(user, phone);
	}
}

//------------------------------------------------------------------------------------------------
//! Put the receiver down: shown to the one talking at this telephone.
class LF_PhoneHangUpAction : LF_PhoneActionBase
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		int talker;
		LF_EPhoneLineState state = LF_PhoneLineComponent.StateAt(GetOwner(), talker);
		return state == LF_EPhoneLineState.BUSY && talker > 0 && talker == PlayerIdOf(user);
	}

	//------------------------------------------------------------------------------------------------
	override protected void ServerPerform(IEntity phone, IEntity user)
	{
		LF_PhoneCalls.ServerHangUp(user, phone);
	}
}
