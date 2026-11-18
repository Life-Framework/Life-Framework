// LF_BarberAction.c — "Ask for a haircut" on a barber's chair (#3234). Shown only to the player
// sitting on this chair (LF_SitAction seats them); it opens the barber sheet on that player's
// machine. The haircut itself is a server request (SCR_PlayerController.LF_AskBarberCut), which
// checks the seat, the look and the cash again.
[BaseContainerProps(configRoot: true)]
class LF_BarberAction : ScriptedUserAction
{
	[Attribute("40", UIWidgets.EditBox, "Cash a haircut on this chair costs", "0 100000 1")]
	protected int m_iFee;

	//------------------------------------------------------------------------------------------------
	int GetFee()
	{
		return m_iFee;
	}

	//------------------------------------------------------------------------------------------------
	//! True when the user is really sitting on this chair.
	static bool IsSeatedOn(IEntity user, IEntity chair)
	{
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(user);
		if (!character || !chair)
			return false;

		SCR_CharacterControllerComponent controller = SCR_CharacterControllerComponent.Cast(character.GetCharacterController());
		return controller && controller.IsLoiteringOnEntity(chair);
	}

	//------------------------------------------------------------------------------------------------
	//! The barber action of a chair, or null when the entity is not a barber's chair.
	static LF_BarberAction Of(IEntity chair)
	{
		if (!chair)
			return null;

		BaseActionsManagerComponent manager = BaseActionsManagerComponent.Cast(chair.FindComponent(BaseActionsManagerComponent));
		if (!manager)
			return null;

		array<BaseUserAction> actions = {};
		manager.GetActionsList(actions);
		foreach (BaseUserAction action : actions)
		{
			LF_BarberAction barber = LF_BarberAction.Cast(action);
			if (barber)
				return barber;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		return IsSeatedOn(user, GetOwner());
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "#LF-Barber_Action";
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The sheet is local UI.
	override bool HasLocalEffectOnlyScript()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!pOwnerEntity || !pUserEntity)
		{
			LF_Debug.Error(LF_Barber.FEATURE, "barber action performed without a chair or a user");
			return;
		}

		if (!LF_CounterAccess.IsLocalActor(pUserEntity, LF_Utils.GetLocalCharacter()))
			return;

		LF_BarberSheet.Open(pOwnerEntity, pUserEntity, m_iFee);
	}
}
