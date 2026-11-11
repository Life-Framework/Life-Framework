//------------------------------------------------------------------------------------------------
//! "Apply to the police" at the station duty desk. Opens the application form on the acting
//! client; the form sends the filled sheet to the server through the player controller.
//! Hidden for a player already on the police roster.
class LF_PoliceApplyAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		int state = LF_AccountReplication.GetState(user);
		if (!LF_AccountStateBits.HasAccount(state))
		{
			SetCannotPerformReason("#LF-Duty_NoAccount");
			return false;
		}

		if (LF_AccountStateBits.IsOnRoster(state))
		{
			SetCannotPerformReason("#LF-PoliceApp_Error_OnRoster");
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		return !LF_AccountStateBits.IsOnRoster(LF_AccountReplication.GetState(user));
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		// The form is a local screen: only the acting machine opens it.
		if (!pUserEntity || pUserEntity != LF_Utils.GetLocalCharacter())
			return;

		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		LF_PoliceApplicationForm.Open(controller);
	}

	//------------------------------------------------------------------------------------------------
	override bool HasLocalEffectOnlyScript()
	{
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! "Review applications" at the same desk, offered to administrators only. Approves the
//! application waiting longest; the server re-checks the administrator role.
class LF_PoliceReviewAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		PlayerController pc = GetGame().GetPlayerController();
		if (!pc)
			return false;

		return AdminChatChannel.IsAdminPlayer(pc.GetPlayerId());
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!pUserEntity || pUserEntity != LF_Utils.GetLocalCharacter())
			return;

		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!controller)
		{
			LF_Debug.Error("Police", "application review not sent: no player controller");
			return;
		}

		controller.LF_AskPoliceReview();
	}

	//------------------------------------------------------------------------------------------------
	override bool HasLocalEffectOnlyScript()
	{
		return true;
	}
}
