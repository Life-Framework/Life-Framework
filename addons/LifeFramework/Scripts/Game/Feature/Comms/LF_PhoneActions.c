//------------------------------------------------------------------------------------------------
//! The telephone actions (#147). Each one shows only when it applies, read from the phone's
//! replicated line (LF_PhoneLineComponent); the server decides everything again and refuses with
//! a toast (LF_PhoneCalls). The client only reaches for the receiver.
class LF_PhoneActionBase : ScriptedUserAction
{
	//! The Comms system is off (#1076): no prompt.
	override bool CanBeShownScript(IEntity user)
	{
		return LF_Systems.IsEnabled("Comms");
	}

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
		if (LF_Systems.Refuses("Comms", "LF_PhoneActionBase"))
			return;

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
		if (!LF_Systems.IsEnabled("Comms"))
			return false;

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
		if (!LF_Systems.IsEnabled("Comms"))
			return false;

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
		if (!LF_Systems.IsEnabled("Comms"))
			return false;

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

//------------------------------------------------------------------------------------------------
//! Leave a call-back request: shown at an idle payphone to the caller whose call there rang out
//! or found nobody on duty, while the offer lasts. Opens the message slip.
class LF_PhoneLeaveRequestAction : LF_PhoneActionBase
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		if (!LF_Systems.IsEnabled("Comms"))
			return false;

		int talker;
		if (LF_PhoneLineComponent.StateAt(GetOwner(), talker) != LF_EPhoneLineState.IDLE)
			return false;

		int offer = LF_PhoneLineComponent.OfferAt(GetOwner());
		return offer > 0 && offer == PlayerIdOf(user);
	}

	//------------------------------------------------------------------------------------------------
	override protected void ServerPerform(IEntity phone, IEntity user)
	{
		LF_PhoneCallBacks.ServerOpenSlip(user, phone);
	}
}

//------------------------------------------------------------------------------------------------
//! Read the station's message pad: the call-back requests, each with a ring-back button. Shown at
//! an idle station telephone; the server answers only the service's people on duty.
class LF_PhoneReadPadAction : LF_PhoneActionBase
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		if (!LF_Systems.IsEnabled("Comms"))
			return false;

		int talker;
		return LF_PhoneLineComponent.StateAt(GetOwner(), talker) == LF_EPhoneLineState.IDLE;
	}

	//------------------------------------------------------------------------------------------------
	override protected void ServerPerform(IEntity phone, IEntity user)
	{
		LF_PhoneCallBacks.ServerOpenPad(user, phone);
	}
}
