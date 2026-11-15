//------------------------------------------------------------------------------------------------
//! The interaction handler calls ShowDisplay whenever the player's context changes, so a prompt
//! (for example "F Switch off") reappeared over an open kit sheet (#1738). While a sheet is open
//! the call is dropped; LF_UIScreen shows the prompt again when the last sheet closes.
modded class SCR_ActionMenuInteractionDisplay
{
	override void ShowDisplay()
	{
		if (LF_UIScreen.IsActionPromptSuppressed())
			return;

		super.ShowDisplay();
	}
}
