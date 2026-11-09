//! Opens the cash machine's terminal screen for the player who used it. The terminal is local UI:
//! cash only moves through LF_CharacterATMComponent's server RPCs, which check range and funds.
[BaseContainerProps(configRoot: true)]
class LF_ATMAction : ScriptedUserAction
{
	[Attribute("SOUND_INV_HOTKEY_SCROLL", desc: "UI sound event for a keypad key press")]
	protected string m_sKeySound;

	[Attribute("SOUND_HUD_GADGET_CONFIRM", desc: "UI sound event when the machine accepts a deposit or withdrawal")]
	protected string m_sAcceptSound;

	[Attribute("SOUND_FE_BUTTON_FAIL", desc: "UI sound event when the machine refuses")]
	protected string m_sRefuseSound;

	[Attribute("SOUND_HUD_GADGET_SELECT", desc: "UI sound event when the screen wakes")]
	protected string m_sOpenSound;

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		if (!user)
			return false;

		LF_CharacterATMComponent atmComp = LF_CharacterATMComponent.Cast(user.FindComponent(LF_CharacterATMComponent));
		return atmComp != null;
	}

	//------------------------------------------------------------------------------------------------
	//! The terminal is local UI: run PerformAction only on the acting player's machine. Without this
	//! a listen host also runs it for every other player and opens the screen on the host.
	override bool HasLocalEffectOnlyScript()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!pOwnerEntity || !pUserEntity)
		{
			LF_Debug.Error("ATM", "action performed without a machine or a user");
			return;
		}

		// Second guard behind HasLocalEffectOnlyScript: only the player at the machine sees it.
		if (!LF_CounterAccess.IsLocalActor(pUserEntity, LF_Utils.GetLocalCharacter()))
		{
			LF_Debug.Info("ATM", "terminal not opened: another player used the machine");
			return;
		}

		LF_ATMTerminal.Open(pOwnerEntity, pUserEntity, this);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		if (!user)
			return false;

		IEntity owner = GetOwner();
		if (!owner)
			return false;

		return vector.Distance(user.GetOrigin(), owner.GetOrigin()) <= LF_ATMTerminal.SHOW_DISTANCE;
	}

	//------------------------------------------------------------------------------------------------
	string GetKeySound()
	{
		return m_sKeySound;
	}

	//------------------------------------------------------------------------------------------------
	string GetAcceptSound()
	{
		return m_sAcceptSound;
	}

	//------------------------------------------------------------------------------------------------
	string GetRefuseSound()
	{
		return m_sRefuseSound;
	}

	//------------------------------------------------------------------------------------------------
	string GetOpenSound()
	{
		return m_sOpenSound;
	}
};
