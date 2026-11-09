//------------------------------------------------------------------------------------------------
//! Base class for user actions gated by a whitelist flag. Subclasses configure
//! m_eWhitelistOnlyType; without a flag set the action performs for everyone.
class LF_WhitelistAction : ScriptedUserAction
{
	[Attribute("0", UIWidgets.Flags, "Action can only be used with this whitelist", "", ParamEnumArray.FromEnum(LF_WhitelistType))]
	protected LF_WhitelistType m_eWhitelistOnlyType;

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		if (!super.CanBePerformedScript(user))
			return false;

		if (m_eWhitelistOnlyType == 0)
			return true;

		LF_PlayerWhitelistComponent whitelistComponent = LF_PlayerWhitelistComponent.Cast(user.FindComponent(LF_PlayerWhitelistComponent));
		if (!whitelistComponent || !whitelistComponent.HasWhitelist(m_eWhitelistOnlyType))
		{
			SetCannotPerformReason("#EL-Whitelist_NotAllowed");
			return false;
		}
		return true;
	}
}